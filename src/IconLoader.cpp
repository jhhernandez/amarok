/***************************************************************************
 *   Copyright (C) 2006 by Mark Kretschmann <markey@web.de>                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.          *
 ***************************************************************************/

#include "amarok.h"
#include "amarokconfig.h"

#include <qmap.h>
#include <qapplication.h>


QString
Amarok::icon( const QString& name ) //declared in amarok.h
{
    bool isRTL = QApplication::isRightToLeft();

    // We map our Amarok icon theme names to system icons, instead of using the same
    // naming scheme. This has two advantages:
    // 1. Our icons can have simpler and more meaningful names
    // 2. We can map several of our icons to one system icon, if necessary
    static QMap<QString, QString> iconMap;

    if( iconMap.empty() ) {
        iconMap["add_lyrics"]           = "list-add";
        iconMap["add_playlist"]         = "list-add";
        iconMap["add_collection"]       = "list-add";
        iconMap["album"]                = "media-optical-audio";
        iconMap["artist"]               = "view-media-artist";
        iconMap["audioscrobbler"]       = "audioscrobbler";
        iconMap["love"]                 = "love";
        iconMap["back"]                 = "media-skip-backward";
        iconMap["burn"]                 = "tools-media-optical-burn";
        iconMap["change_language"]      = "preferences-desktop-locale";
        iconMap["clock"]                = "view-history";
        iconMap["collection"]           = "collection";
        iconMap["configure"]            = "configure";
        iconMap["covermanager"]         = "covermanager";
        iconMap["device"]               = "multimedia-player";
        iconMap["download"]             = "get-hot-new-stuff";
        iconMap["dynamic"]              = "dynamic";
        iconMap["edit_properties"]      = "document-properties";
        iconMap["editcopy"]             = "edit-copy";
        iconMap["equalizer"]            = "view-media-equalizer";
        iconMap["external"]             = "system-run";
        iconMap["fastforward"]          = "media-seek-forward";
        iconMap["favourite_genres"]     = "system-file-manager";
        iconMap["files"]                = "folder";
        iconMap["files2"]               = "folder-red";
        iconMap["info"]                 = "help-about";
        iconMap["lyrics"]               = "view-media-lyrics";
        iconMap["magnatune"]            = "services";
        iconMap["mostplayed"]           = "favorites";
        iconMap["music"]                = "x-media-podcast";
        iconMap["next"]                 = "media-skip-forward";
        iconMap["pause"]                = "media-playback-pause";
        iconMap["play"]                 = "media-playback-start";
        iconMap["playlist"]             = "view-media-playlist";
        iconMap["playlist_clear"]       = "edit-clear-list";
        iconMap["playlist_refresh"]     = "view-refresh";
        iconMap["queue"]                = "go-bottom";
        iconMap["queue_track"]          = isRTL ? "go-previous" : "go-next"; // todo jpetso: move-left/right?
        iconMap["dequeue_track"]        = isRTL ? "go-next" : "go-previous"; // todo jpetso: move-left/right?
        iconMap["random"]               = "media-playlist-shuffle";
        iconMap["random_album"]         = "media-optical-audio-shuffle"; // please request a separate icon and call it "media-album-shuffle". the current name at least falls back to the audio CD icon.
        iconMap["random_no"]            = "go-next";
        iconMap["random_track"]         = "media-playlist-shuffle";
        iconMap["redo"]                 = "edit-redo";
        iconMap["refresh"]              = "view-refresh";
        iconMap["remove"]               = "edit-delete";
        iconMap["remove_from_playlist"] = "list-remove";
        iconMap["rename"]               = "edit-rename";
        iconMap["repeat_album"]         = "media-optical-audio-repeat"; // please request a separate icon and call it "media-album-repeat". the current name at least falls back to the audio CD icon.
        iconMap["repeat_no"]            = "go-down";
        iconMap["repeat_playlist"]      = "media-playlist-repeat";
        iconMap["repeat_track"]         = "audio-x-generic-repeat"; // please request a separate icon and call it "media-track-repeat". the current name at least falls back to the audio file icon.
        iconMap["rescan"]               = "view-refresh";
        iconMap["rewind"]               = "media-seek-backward";
        iconMap["save"]                 = "document-save";
        iconMap["scripts"]              = "media-scripts";
        iconMap["search"]               = "edit-find";
        iconMap["settings_engine"]      = "amarok";
        iconMap["settings_general"]     = "preferences-other";
        iconMap["settings_indicator"]   = "preferences-desktop-display";
        iconMap["settings_playback"]    = "preferences-desktop-sound";
        iconMap["settings_view"]        = "preferences-desktop-theme";
        iconMap["statistics"]           = "view-statistics";
        iconMap["stop"]                 = "media-playback-stop";
        iconMap["podcast"]              = "x-media-podcast";
        iconMap["track"]                = "audio-x-generic";
        iconMap["undo"]                 = "edit-undo";
        iconMap["visualizations"]       = "view-media-visualization";
        iconMap["zoom"]                 = "zoom-in";
    }

    static QMap<QString, QString> amarokMap;
    if( amarokMap.empty() ) {
        amarokMap["queue_track"]          = "fastforward";
        amarokMap["dequeue_track"]        = "rewind";
    }

    if( iconMap.contains( name ) )
    {
        if( AmarokConfig::useCustomIconTheme() )
        {
            if( amarokMap.contains( name ) )
                return QString( "amarok_" ) + amarokMap[name];
            return QString( "amarok_" ) + name;
        }
        else
            return iconMap[name];
    }

    return name;
}


