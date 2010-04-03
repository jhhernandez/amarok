/****************************************************************************************
 * Copyright (c) 2007 Ian Monroe <ian@monroe.nu>                                        *
 *                                                                                      *
 * This program is free software; you can redistribute it and/or modify it under        *
 * the terms of the GNU General Public License as published by the Free Software        *
 * Foundation; either version 2 of the License, or (at your option) version 3 or        *
 * any later version accepted by the membership of KDE e.V. (or its successor approved  *
 * by the membership of KDE e.V.), which shall act as a proxy defined in Section 14 of  *
 * version 3 of the license.                                                            *
 *                                                                                      *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY      *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A      *
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.             *
 *                                                                                      *
 * You should have received a copy of the GNU General Public License along with         *
 * this program.  If not, see <http://www.gnu.org/licenses/>.                           *
 ****************************************************************************************/

#include "core/support/Amarok.h"
#include "core-impl/playlists/types/file/PlaylistFileSupport.h"
#include "core/support/Debug.h"
#include "core-impl/playlists/types/file/xspf/XSPFPlaylist.h"
#include "core-impl/playlists/types/file/pls/PLSPlaylist.h"
#include "core-impl/playlists/types/file/m3u/M3UPlaylist.h"
#include "statusbar/StatusBar.h"


#include <KLocale>
#include <KTemporaryFile>
#include <KUrl>

#include <QFile>
#include <QFileInfo>

namespace Playlists {

PlaylistFormat
getFormat( const KUrl &path )
{
    const QString ext = Amarok::extension( path.fileName() );

    if( ext == "m3u" || ext == "m3u8" ) return M3U; //m3u8 is M3U in UTF8
    if( ext == "pls" ) return PLS;
    if( ext == "ram" ) return RAM;
    if( ext == "smil") return SMIL;
    if( ext == "asx" || ext == "wax" ) return ASX;
    if( ext == "xml" ) return XML;
    if( ext == "xspf" ) return XSPF;

    return Unknown;
}

bool
isPlaylist( const KUrl &path )
{
    return ( getFormat( path ) != Unknown );
}

PlaylistFilePtr
loadPlaylistFile( const KUrl &url )
{
    //DEBUG_BLOCK

    QFile file;
    KUrl fileToLoad;

    if( !url.isValid() )
    {
        error() << "url is not valid!";
        return PlaylistFilePtr();
    }

    if( url.isLocalFile() )
    {
        if( !QFileInfo( url.toLocalFile() ).exists() )
        {
            error() << QString("Could not load local playlist file %1!").arg( url.toLocalFile() );
            return PlaylistFilePtr();
        }
    }

    if( url.isLocalFile() )
    {
        //debug() << "local file";

        file.setFileName( url.toLocalFile() );

        if( !file.open( QIODevice::ReadOnly | QIODevice::Text ) )
        {
            debug() << "could not read file " << url.path();

            if( The::statusBar() )
                The::statusBar()->longMessage( i18n( "Cannot read playlist (%1).", url.url() ) );

            return Playlists::PlaylistFilePtr( 0 );
        }
        fileToLoad = url;
    }
    else
    {
        //debug() << "remote file: " << url;
        //FIXME: for now, just do a blocking download... Someone please come up with a better way...

        KTemporaryFile tempFile;

        tempFile.setSuffix(  '.' + Amarok::extension( url.url() ) );
        tempFile.setAutoRemove( false );  //file will be removed in JamendoXmlParser
        if( !tempFile.open() )
        {
            if( The::statusBar() )
                The::statusBar()->longMessage( i18n( "Could not create a temporary file to download playlist.") );

            return Playlists::PlaylistFilePtr( 0 ); //error
        }


        QString tempFileName = tempFile.fileName();
        #ifdef Q_WS_WIN
        // KIO::file_copy faild to overwrite an open file
        // using KTemporary.close() is not enough here
        tempFile.remove();
        #endif
        KIO::FileCopyJob * job = KIO::file_copy( url , KUrl( tempFileName ), 0774 , KIO::Overwrite | KIO::HideProgressInfo );

        if( The::statusBar() )
            The::statusBar()->newProgressOperation( job, i18n( "Downloading remote playlist" ) );

        if( !job->exec() ) //Job deletes itself after execution
        {
            error() << "error";
            return Playlists::PlaylistFilePtr( 0 );
        }
        else
        {
            file.setFileName( tempFileName );
            if( !file.open( QFile::ReadOnly ) )
            {
                debug() << "error opening file: " << tempFileName;
                return Playlists::PlaylistFilePtr( 0 );
            }
            fileToLoad = KUrl::fromPath( file.fileName() );
        }
    }

    PlaylistFormat format = getFormat( fileToLoad );
    PlaylistFile *playlist = 0;
    switch( format )
    {
        case PLS:
            playlist = new PLSPlaylist( fileToLoad );
            break;
        case M3U:
            playlist = new M3UPlaylist( fileToLoad );
            break;
        case XSPF:
            playlist = new XSPFPlaylist( fileToLoad );
            break;
        default:
            debug() << "Could not load playlist file " << fileToLoad;
            break;
    }

    return PlaylistFilePtr( playlist );
}

bool
exportPlaylistFile( const Meta::TrackList &list, const KUrl &path )
{
    PlaylistFormat format = getFormat( path );
    bool result = false;
    switch( format )
    {
        case PLS:
            result = PLSPlaylist( list ).save( path.path(), true );
            break;
        case M3U:
            result = M3UPlaylist( list ).save( path.path(), true );
            break;
        case XSPF:
            result = XSPFPlaylist( list ).save( path.path(), true );
            break;
        default:
            debug() << "Could not export playlist file " << path;
            break;
    }
    return result;
}

bool
canExpand( Meta::TrackPtr track )
{
    if( !track )
        return false;

    return Playlists::getFormat( track->uidUrl() ) != Playlists::NotPlaylist;
}

PlaylistPtr
expand( Meta::TrackPtr track )
{
   //this should really be made asyncrhonous
   return Playlists::PlaylistPtr::dynamicCast( loadPlaylistFile( track->uidUrl() ) );
}

KUrl
newPlaylistFilePath( const QString &fileExtension )
{
    int trailingNumber = 1;
    KLocalizedString fileName = ki18n("Playlist_%1");
    KUrl url( Amarok::saveLocation( "playlists" ) );
    url.addPath( fileName.subs( trailingNumber ).toString() );

    while( QFileInfo( url.path() ).exists() )
        url.setFileName( fileName.subs( ++trailingNumber ).toString() );

    return KUrl( QString( "%1.%2" ).arg( url.path() ).arg( fileExtension ) );
}

}