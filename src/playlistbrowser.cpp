/***************************************************************************
 * copyright            : (c) 2004 Pierpaolo Di Panfilo                    *
 *                        (c) 2004 Mark Kretschmann <markey@web.de>        *
 *                        (c) 2005-2007 Seb Ruiz <me@sebruiz.net>          *
 *                        (c) 2005 Gábor Lehel <illissius@gmail.com>       *
 *                        (c) 2005 Christian Muehlhaeuser <chris@chris.de> *
 *                        (c) 2006 Alexandre Oliveira <aleprj@gmail.com>   *
 *                        (c) 2006 Adam Pigg <adam@piggz.co.uk>            *
 * See COPYING file for licensing information                              *
 ***************************************************************************/

#define DEBUG_PREFIX "PlaylistBrowser"

#include "amarok.h"            //actionCollection()
#include "browserToolBar.h"
#include "querybuilder.h"      //smart playlists
#include "debug.h"
#include "htmlview.h"
#include "k3bexporter.h"
#include "mediabrowser.h"
#include "dynamicmode.h"
#include "lastfm.h"
#include "playlist.h"
#include "playlistbrowser.h"
#include "playlistbrowseritem.h"
#include "playlistselection.h"
#include "podcastbundle.h"
#include "podcastsettings.h"
#include "scancontroller.h"
#include "smartplaylisteditor.h"
#include "tagdialog.h"         //showContextMenu()
#include "threadmanager.h"
#include "statusbar.h"
#include "contextbrowser.h"
#include "xspfplaylist.h"

#include <QEvent>            //customEvent()
#include <q3header.h>           //mousePressed()
#include <QLabel>
#include <QPainter>          //paintCell()
#include <QPixmap>           //paintCell()
#include <q3textstream.h>       //loadPlaylists(), saveM3U(), savePLS()
//Added by qt3to4:
#include <QResizeEvent>
#include <QDragLeaveEvent>
#include <QLinkedList>
#include <QDragEnterEvent>
#include <QKeyEvent>
#include <QDropEvent>
#include <QDragMoveEvent>
#include <QPaintEvent>
#include <Q3PtrList>
#include <QSplitter>

#include <kaction.h>
#include <kactioncollection.h>
#include <kapplication.h>
#include <kfiledialog.h>       //openPlaylist()
#include <kio/deletejob.h>     //deleteSelectedPlaylists()
#include <kiconloader.h>       //smallIcon
#include <kinputdialog.h>
#include <klineedit.h>         //rename()
#include <klocale.h>
#include <kmessagebox.h>       //renamePlaylist(), deleteSelectedPlaylist()
#include <kmimetype.h>
#include <k3multipledrag.h>     //dragObject()
#include <kmenu.h>
#include <kpushbutton.h>
#include <kstandarddirs.h>     //KGlobal::dirs()
#include <k3urldrag.h>          //dragObject()
#include <kvbox.h>

#include <cstdio>              //rename() in renamePlaylist()



namespace Amarok {
    Q3ListViewItem*
    findItemByPath( Q3ListView *view, QString name )
    {
        const static QString escaped( "\\/" );
        const static QChar sep( '/' );

        debug() << "Searching " << name << endl;
        QStringList path = splitPath( name );

        Q3ListViewItem *prox = view->firstChild();
        Q3ListViewItem *item = 0;

        foreach( QString text, path ) {
            item = prox;
            text.replace( escaped, sep );

            for ( ; item; item = item->nextSibling() ) {
                if ( text == item->text(0) ) {
                    break;
                }
            }

            if ( !item )
                return 0;
            prox = item->firstChild();
        }
        return item;
    }

    QStringList
    splitPath( QString path ) {
        QStringList list;

        const static QChar sep( '/' );
        int bOffset = 0, sOffset = 0;

        int pos = path.indexOf( sep, bOffset );

        while ( pos != -1 ) {
            if ( pos > sOffset && pos <= (int)path.length() ) {
                if ( pos > 0 && path[pos-1] != '\\' ) {
                    list << path.mid( sOffset, pos - sOffset );
                    sOffset = pos + 1;
                }
            }
            bOffset = pos + 1;
            pos = path.indexOf( sep, bOffset );
        }

        int length = path.length() - 1;
        if ( path.mid( sOffset, length - sOffset + 1 ).length() > 0 )
            list << path.mid( sOffset, length - sOffset + 1 );

        return list;
    }
}


inline QString
fileExtension( const QString &fileName )
{
    return Amarok::extension( fileName );
}

PlaylistBrowser *PlaylistBrowser::s_instance = 0;


PlaylistBrowser::PlaylistBrowser( const char *name )
        : KVBox( 0 )
        , m_polished( false )
        , m_playlistCategory( 0 )
        , m_streamsCategory( 0 )
        , m_smartCategory( 0 )
        , m_dynamicCategory( 0 )
        , m_podcastCategory( 0 )
        , m_coolStreams( 0 )
        , m_smartDefaults( 0 )
        , m_lastfmCategory( 0 )
        , m_shoutcastCategory( 0 )
        , m_lastPlaylist( 0 )
        , m_coolStreamsOpen( false )
        , m_smartDefaultsOpen( false )
        , m_lastfmOpen( false )
        , m_ac( new KActionCollection( this ) )
        , m_podcastTimer( new QTimer( this ) )


{
    s_instance = this;

    KVBox *browserBox = new KVBox( this );
//     browserBox->setSpacing( 3 );

    //<Toolbar>
    addMenuButton  = new KActionMenu( KIcon( Amarok::icon( "add_playlist" ) ), i18n("Add"), m_ac );
    addMenuButton->setDelayed( false );

    KMenu *playlistMenu = new KMenu( browserBox );
    playlistMenu->insertItem( i18n("New..."), PLAYLIST );
    playlistMenu->insertItem( i18n("Import Existing..."), PLAYLIST_IMPORT );
    connect( playlistMenu, SIGNAL( activated(int) ), SLOT( slotAddPlaylistMenu(int) ) );

    KMenu *addMenu  = addMenuButton->popupMenu();
    addMenu->insertItem( i18n("Playlist"), playlistMenu );
    addMenu->insertItem( i18n("Smart Playlist..."), SMARTPLAYLIST );
    addMenu->insertItem( i18n("Dynamic Playlist..."), ADDDYNAMIC);
    addMenu->insertItem( i18n("Radio Stream..."), STREAM );
    addMenu->insertItem( i18n("Podcast..."), PODCAST );
    connect( addMenu, SIGNAL( activated(int) ), SLOT( slotAddMenu(int) ) );

    renameButton   = new KAction( KIcon( "edit-clear" ), i18n("Rename"), m_ac );
    connect( renameButton, SIGNAL( triggered( bool ) ), this, SLOT( renameSelectedItem() ) );
    removeButton   = new KAction( KIcon( Amarok::icon( "remove" ) ), i18n("Delete"), m_ac );
    connect( removeButton, SIGNAL( triggered( bool ) ), this, SLOT( removeSelectedItems() ) );

    m_toolbar = new Browser::ToolBar( browserBox );
    m_toolbar->setToolButtonStyle( Qt::ToolButtonIconOnly );
    m_toolbar->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Preferred );
    m_toolbar->addAction( addMenuButton );

    m_toolbar->setToolButtonStyle( Qt::ToolButtonIconOnly );      //default appearance
    m_toolbar->addSeparator();
    m_toolbar->addAction( renameButton );
    m_toolbar->addAction( removeButton );

    renameButton->setEnabled( false );
    removeButton->setEnabled( false );
    //</Toolbar>

    m_splitter = new QSplitter( Qt::Vertical, browserBox );
    m_splitter->setChildrenCollapsible( false ); // hiding the InfoPane entirely can only be confusing

    m_listview = new PlaylistBrowserView( m_splitter );

    int sort = Amarok::config( "PlaylistBrowser" ).readEntry( "Sorting", int(Qt::Ascending) );
    m_listview->setSorting( 0, sort == Qt::Ascending ? true : false );

    m_podcastTimerInterval = Amarok::config( "PlaylistBrowser" ).readEntry( "Podcast Interval", 14400000 );
    connect( m_podcastTimer, SIGNAL(timeout()), this, SLOT(scanPodcasts()) );

    // signals and slots connections
    connect( m_listview, SIGNAL( contextMenuRequested( Q3ListViewItem *, const QPoint &, int ) ),
             this,         SLOT( showContextMenu( Q3ListViewItem *, const QPoint &, int ) ) );
    connect( m_listview, SIGNAL( doubleClicked( Q3ListViewItem *, const QPoint &, int ) ),
             this,         SLOT( invokeItem( Q3ListViewItem *, const QPoint &, int ) ) );
    connect( m_listview, SIGNAL( itemRenamed( Q3ListViewItem*, const QString&, int ) ),
             this,         SLOT( renamePlaylist( Q3ListViewItem*, const QString&, int ) ) );
    connect( m_listview, SIGNAL( currentChanged( Q3ListViewItem * ) ),
             this,         SLOT( currentItemChanged( Q3ListViewItem * ) ) );
    connect( CollectionDB::instance(), SIGNAL( scanDone( bool ) ), SLOT( collectionScanDone() ) );

//     setMinimumWidth( m_toolbar->sizeHint().width() );

    m_infoPane = new InfoPane( m_splitter );

    m_podcastCategory = loadPodcasts();

    polish();

    setSpacing( 4 );
    setFocusProxy( m_listview );
}


void
PlaylistBrowser::polish()
{
    // we make startup faster by doing the slow bits for this
    // only when we are shown on screen

    DEBUG_FUNC_INFO

    Amarok::OverrideCursor cursor;

//     blockSignals( true );
//     BrowserBar::instance()->restoreWidth();
//     blockSignals( false );

    KVBox::polish();

    /// Podcasting is always initialised in the ctor because of autoscanning

    m_polished = true;

    m_playlistCategory = loadPlaylists();
    if( !CollectionDB::instance()->isEmpty() )
    {
        m_smartCategory = loadSmartPlaylists();
        loadDefaultSmartPlaylists();
    }
#define config Amarok::config( "PlaylistBrowser" )

    m_dynamicCategory = loadDynamics();
    m_randomDynamic   = new DynamicEntry( m_dynamicCategory, 0, i18n("Random Mix") );
    m_randomDynamic->setKept( false ); //don't save it
    m_randomDynamic->setCycleTracks(   config.readEntry( "Dynamic Random Remove Played", true ) );
    m_randomDynamic->setUpcomingCount( config.readEntry ( "Dynamic Random Upcoming Count", 15 ) );
    m_randomDynamic->setPreviousCount( config.readEntry ( "Dynamic Random Previous Count", 5 ) );

    m_suggestedDynamic = new DynamicEntry( m_dynamicCategory, m_randomDynamic, i18n("Suggested Songs" ) );
    m_suggestedDynamic->setKept( false ); //don't save it
    m_suggestedDynamic->setAppendType( DynamicMode::SUGGESTION );
    m_suggestedDynamic->setCycleTracks(   config.readEntry( "Dynamic Suggest Remove Played", true ) );
    m_suggestedDynamic->setUpcomingCount( config.readEntry ( "Dynamic Suggest Upcoming Count", 15 ) );
    m_suggestedDynamic->setPreviousCount( config.readEntry ( "Dynamic Suggest Previous Count", 5 ) );

#undef config

    m_streamsCategory  = loadStreams();
    loadCoolStreams();
    m_shoutcastCategory = new ShoutcastBrowser( m_streamsCategory );

    if( !AmarokConfig::scrobblerUsername().isEmpty() )
    {
        const bool subscriber = Amarok::config( "Scrobbler" ).readEntry( "Subscriber", false );
        loadLastfmStreams( subscriber );
    }

    markDynamicEntries();

    // ListView item state restoration:
    // First we check if the number of items in the listview is the same as it was on last
    // application exit. If true, we iterate over all items and restore their open/closed state.
    // Note: We ignore podcast items, because they are added dynamically added to the ListView.
    QList<int> stateList = Amarok::config( "PlaylistBrowser" ).readEntry( "Item State", QList<int>() );
    Q3ListViewItemIterator it( m_listview );
    uint count = 0;
    while ( it.current() ) {
        if( !isPodcastEpisode( it.current() ) )
            ++count;
        ++it;
    }

    if ( count == stateList.count() ) {
        uint index = 0;
        it = Q3ListViewItemIterator( m_listview );
        while ( it.current() ) {
            if( !isPodcastEpisode( it.current() ) ) {
                it.current()->setOpen( stateList[index] );
                ++index;
            }
            ++it;
        }
    }

    // Set height of InfoPane
    m_infoPane->setStoredHeight( Amarok::config( "PlaylistBrowser" ).readEntry( "InfoPane Height", 200 ) );
}


PlaylistBrowser::~PlaylistBrowser()
{
    DEBUG_BLOCK

    s_instance = 0;

    if( m_polished )
    {
        savePodcastFolderStates( m_podcastCategory );

        QStringList list;
        for( uint i=0; i < m_dynamicEntries.count(); i++ )
        {
            Q3ListViewItem *item = m_dynamicEntries.at( i );
            list.append( item->text(0) );
        }
        int sortorder = m_listview->sortOrder() == Qt::AscendingOrder ? 0 : 1;
        Amarok::config( "PlaylistBrowser" ).writeEntry( "Sorting", sortorder );
        Amarok::config( "PlaylistBrowser" ).writeEntry( "Podcast Interval", m_podcastTimerInterval );
        Amarok::config( "PlaylistBrowser" ).writeEntry( "Podcast Folder Open", m_podcastCategory->isOpen() );
        Amarok::config( "PlaylistBrowser" ).writeEntry( "InfoPane Height", m_infoPane->getHeight() );
    }
}


void
PlaylistBrowser::setInfo( const QString &title, const QString &info )
{
    m_infoPane->setInfo( title, info );
}

void
PlaylistBrowser::resizeEvent( QResizeEvent * )
{
    if( m_infoPane->findChild<QWidget*>( "container" )->isShown() )
        m_infoPane->setMaximumHeight( ( int )( m_splitter->height() / 1.5 ) );
}


void
PlaylistBrowser::markDynamicEntries()
{
    if( Amarok::dynamicMode() )
    {
        QStringList playlists = Amarok::dynamicMode()->items();

        for( uint i=0; i < playlists.count(); i++ )
        {
            PlaylistBrowserEntry *item = dynamic_cast<PlaylistBrowserEntry*>( Amarok::findItemByPath( m_listview, playlists[i]  ) );

            if( item )
            {
                m_dynamicEntries.append( item );
                if( item->rtti() == PlaylistEntry::RTTI )
                     static_cast<PlaylistEntry*>( item )->setDynamic( true );
                if( item->rtti() == SmartPlaylist::RTTI )
                     static_cast<SmartPlaylist*>( item )->setDynamic( true );
            }
        }
    }
}

/**
 *************************************************************************
 *  STREAMS
 *************************************************************************
 **/

QString PlaylistBrowser::streamBrowserCache() const
{
    return Amarok::saveLocation() + "streambrowser_save.xml";
}


PlaylistCategory* PlaylistBrowser::loadStreams()
{
    QFile file( streamBrowserCache() );

    QTextStream stream( &file );
    stream.setCodec( "UTF8" );

    QDomDocument d;
    QDomElement e;

    Q3ListViewItem *after = m_dynamicCategory;

    if( !file.open( QIODevice::ReadOnly ) || !d.setContent( stream.read() ) )
    { /*Couldn't open the file or it had invalid content, so let's create an empty element*/
        return new PlaylistCategory( m_listview, after, i18n("Radio Streams") );
    }
    else {
        e = d.namedItem( "category" ).toElement();
        if ( e.attribute("formatversion") =="1.1" ) {
            PlaylistCategory* p = new PlaylistCategory( m_listview, after, e );
            p->setText(0, i18n("Radio Streams") );
            return p;
        }
        else { // Old unversioned format
            PlaylistCategory* p = new PlaylistCategory( m_listview, after, i18n("Radio Streams") );
            Q3ListViewItem *last = 0;
            QDomNode n = d.namedItem( "streambrowser" ).namedItem("stream");
            for( ; !n.isNull();  n = n.nextSibling() ) {
                last = new StreamEntry( p, last, n.toElement() );
            }
            return p;
        }
    }
}

void PlaylistBrowser::loadCoolStreams()
{
    QFile file( KStandardDirs::locate( "data","amarok/data/Cool-Streams.xml" ) );
    if( !file.open( QIODevice::ReadOnly ) )
        return;

    QTextStream stream( &file );
    stream.setCodec( "UTF8" );

    QDomDocument d;

    if( !d.setContent( stream.read() ) )
    {
        error() << "Bad Cool Streams XML file" << endl;
        return;
    }

    m_coolStreams = new PlaylistCategory( m_streamsCategory, 0, i18n("Cool-Streams") );
    m_coolStreams->setOpen( m_coolStreamsOpen );
    m_coolStreams->setKept( false );
    StreamEntry *last = 0;

    QDomNode n = d.namedItem( "coolstreams" ).firstChild();

    for( ; !n.isNull(); n = n.nextSibling() )
    {
        QDomElement e = n.toElement();
        QString name = e.attribute( "name" );
        e = n.namedItem( "url" ).toElement();
        KUrl url  = KUrl::KUrl( e.text() );
        last = new StreamEntry( m_coolStreams, last, url, name );
        last->setKept( false );
    }
}

void PlaylistBrowser::addStream( Q3ListViewItem *parent )
{
    StreamEditor dialog( this, i18n( "Radio Stream" ), QString() );
    dialog.setCaption( i18n( "Add Radio Stream" ) );

    if( !parent ) parent = static_cast<Q3ListViewItem*>(m_streamsCategory);

    if( dialog.exec() == QDialog::Accepted )
    {
        new StreamEntry( parent, 0, dialog.url(), dialog.name() );
        parent->sortChildItems( 0, true );
        parent->setOpen( true );

        saveStreams();
    }
}


void PlaylistBrowser::editStreamURL( StreamEntry *item, const bool readonly )
{
    StreamEditor dialog( this, item->title(), item->url().prettyUrl(), readonly );
    dialog.setCaption( readonly ? i18n( "Radio Stream" ) : i18n( "Edit Radio Stream" ) );

    if( dialog.exec() == QDialog::Accepted )
    {
        item->setTitle( dialog.name() );
        item->setUrl( dialog.url() );
        item->setText(0, dialog.name() );
    }
}

void PlaylistBrowser::saveStreams()
{
    QFile file( streamBrowserCache() );

    QDomDocument doc;
    QDomElement streamB = m_streamsCategory->xml();
    streamB.setAttribute( "product", "Amarok" );
    streamB.setAttribute( "version", APP_VERSION );
    streamB.setAttribute( "formatversion", "1.1" );
    QDomNode streamsNode = doc.importNode( streamB, true );
    doc.appendChild( streamsNode );

    QString temp( doc.toString() );

    // Only open the file after all data is ready. If it crashes, data is not lost!
    if ( !file.open( QIODevice::WriteOnly ) ) return;

    QTextStream stream( &file );
    stream.setCodec( "UTF8" );
    stream << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    stream << temp;
}

/**
 *************************************************************************
 *  LAST.FM
 *************************************************************************
 **/

void PlaylistBrowser::loadLastfmStreams( const bool subscriber /*false*/ )
{
    QFile file( Amarok::saveLocation() + "lastfmbrowser_save.xml" );

    QTextStream stream( &file );
    stream.setCodec( "UTF8" );

    QDomDocument d;
    QDomElement e;

    Q3ListViewItem *after = m_streamsCategory;

    if( !file.open( QIODevice::ReadOnly ) || !d.setContent( stream.read() ) )
    { /*Couldn't open the file or it had invalid content, so let's create an empty element*/
        m_lastfmCategory = new PlaylistCategory( m_listview, after , i18n("Last.fm Radio") );
    }
    else {
        e = d.namedItem( "category" ).toElement();
        m_lastfmCategory = new PlaylistCategory( m_listview, after, e );
        m_lastfmCategory->setText( 0, i18n("Last.fm Radio") );
    }

    /// Load the default items

    QStringList globaltags;
    globaltags << "Alternative" << "Ambient" << "Chill Out" << "Classical" << "Dance"
            << "Electronica" << "Favorites" << "Heavy Metal" << "Hip Hop" << "Indie Rock"
            << "Industrial" << "Japanese" << "Pop" << "Psytrance" << "Rap" << "Rock"
            << "Soundtrack" << "Techno" << "Trance";

    PlaylistCategory *tagsFolder = new PlaylistCategory( m_lastfmCategory, 0, i18n("Global Tags") );
    tagsFolder->setKept( false );
    LastFmEntry *last = 0;

    foreach( QString str, globaltags )
    {
        const KUrl url( "lastfm://globaltags/" + str );
        last = new LastFmEntry( tagsFolder, last, url, str );
        last->setKept( false );
    }

    QString user = AmarokConfig::scrobblerUsername();
    KUrl url( QString("lastfm://user/%1/neighbours").arg( user ) );
    last = new LastFmEntry( m_lastfmCategory, tagsFolder, url, i18n( "Neighbor Radio" ) );
    last->setKept( false );

    if( subscriber )
    {
        url = KUrl( QString("lastfm://user/%1/personal").arg( user ) );
        last = new LastFmEntry( m_lastfmCategory, last, url, i18n( "Personal Radio" ) );
        last->setKept( false );

        url = KUrl( QString("lastfm://user/%1/loved").arg( user ) );
        last = new LastFmEntry( m_lastfmCategory, last, url, i18n( "Loved Radio" ) );
        last->setKept( false );
    }
}

void PlaylistBrowser::addLastFmRadio( Q3ListViewItem *parent )
{
    StreamEditor dialog( this, i18n( "Last.fm Radio" ), QString() );
    dialog.setCaption( i18n( "Add Last.fm Radio" ) );

    if( !parent ) parent = static_cast<Q3ListViewItem*>(m_lastfmCategory);

    if( dialog.exec() == QDialog::Accepted )
    {
        new LastFmEntry( parent, 0, dialog.url(), dialog.name() );
        parent->sortChildItems( 0, true );
        parent->setOpen( true );
        saveLastFm();
    }
}

void PlaylistBrowser::addLastFmCustomRadio( Q3ListViewItem *parent )
{
    QString token = LastFm::Controller::createCustomStation();
    if( token.isEmpty() ) return;
    token.replace( "/", "%252" );

    const QString text = "lastfm://artistnames/" + token;
    const KUrl url( text );

    QString name = LastFm::Controller::stationDescription( text );
    name.replace( "%252", "/" );
    new LastFmEntry( parent, 0, url, name );
    saveLastFm();
}

void PlaylistBrowser::saveLastFm()
{
    if ( !m_lastfmCategory )
        return;

    QFile file( Amarok::saveLocation() + "lastfmbrowser_save.xml" );

    QDomDocument doc;
    QDomElement lastfmB = m_lastfmCategory->xml();
    lastfmB.setAttribute( "product", "Amarok" );
    lastfmB.setAttribute( "version", APP_VERSION );
    lastfmB.setAttribute( "formatversion", "1.1" );
    QDomNode lastfmNode = doc.importNode( lastfmB, true );
    doc.appendChild( lastfmNode );

    QString temp( doc.toString() );

    // Only open the file after all data is ready. If it crashes, data is not lost!
    if ( !file.open( QIODevice::WriteOnly ) ) return;

    QTextStream stream( &file );
    stream.setCodec( "UTF8" );
    stream << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    stream << temp;
}


/**
 *************************************************************************
 *  SMART-PLAYLISTS
 *************************************************************************
 **/

QString PlaylistBrowser::smartplaylistBrowserCache() const
{
    return Amarok::saveLocation() + "smartplaylistbrowser_save.xml";
}

void PlaylistBrowser::addSmartPlaylist( Q3ListViewItem *parent ) //SLOT
{
    if( CollectionDB::instance()->isEmpty() || !m_smartCategory )
        return;

    if( !parent ) parent = static_cast<Q3ListViewItem*>(m_smartCategory);


    SmartPlaylistEditor dialog( i18n("Untitled"), this );
    if( dialog.exec() == QDialog::Accepted ) {

        PlaylistCategory *category = dynamic_cast<PlaylistCategory*>(parent);
        if( !category )
            return; //this should never happen, but let's make sure amarok doesn't crash
        for( Q3ListViewItem *item = category->firstChild(); item; item = item->nextSibling() ) {
            SmartPlaylist *sp = dynamic_cast<SmartPlaylist*>(item);
            if ( sp && sp->title() == dialog.name() ) {
                if( KMessageBox::warningContinueCancel(
                    PlaylistWindow::self(),
                    i18n( "A Smart Playlist named \"%1\" already exists. Do you want to overwrite it?", dialog.name() ),
                    i18n( "Overwrite Playlist?" ), KGuiItem( i18n( "Overwrite" ) ) ) ==
KMessageBox::Continue )
                {
                    delete item;
                    break;
                }
                else
                    return;
            }
        }
        new SmartPlaylist( parent, 0, dialog.result() );
        parent->sortChildItems( 0, true );
        parent->setOpen( true );

        saveSmartPlaylists();
    }
}

PlaylistCategory* PlaylistBrowser::loadSmartPlaylists()
{

    QFile file( smartplaylistBrowserCache() );
    QTextStream stream( &file );
    stream.setCodec( "UTF8" );
    Q3ListViewItem *after = m_playlistCategory;

    QDomDocument d;
    QDomElement e;

    if( !file.open( QIODevice::ReadOnly ) || !d.setContent( stream.read() ) )
    { /*Couldn't open the file or it had invalid content, so let's create an empty element*/
        return new PlaylistCategory(m_listview, after, i18n("Smart Playlists") );
    }
    else {
        e = d.namedItem( "category" ).toElement();
        QString version = e.attribute("formatversion");
        float fversion = e.attribute("formatversion").toFloat();
        if ( version == "1.8" )
        {
            PlaylistCategory* p = new PlaylistCategory(m_listview, after, e );
            p->setText( 0, i18n("Smart Playlists") );
            return p;
        }
        else if ( fversion > 1.0f  )
        {
            PlaylistCategory* p = new PlaylistCategory(m_listview, after, e );
            p->setText( 0, i18n("Smart Playlists") );
            debug() << "loading old format smart playlists, converted to new format" << endl;
            updateSmartPlaylists( p );
            saveSmartPlaylists( p );
            return p;
        }
        else { // Old unversioned format
            PlaylistCategory* p = new PlaylistCategory(m_listview, after , i18n("Smart Playlists") );
            Q3ListViewItem *last = 0;
            QDomNode n = d.namedItem( "smartplaylists" ).namedItem("smartplaylist");
            for( ; !n.isNull();  n = n.nextSibling() ) {
                last = new SmartPlaylist( p, last, n.toElement() );
            }
            return p;
        }
    }
}

void PlaylistBrowser::updateSmartPlaylists( Q3ListViewItem *p )
{
    if( !p )
        return;

    for( Q3ListViewItem *it =  p->firstChild();
            it;
            it = it->nextSibling() )
    {
        SmartPlaylist *spl = dynamic_cast<SmartPlaylist *>( it );
        if( spl )
        {
            QDomElement xml = spl->xml();
            QDomElement query = xml.namedItem( "sqlquery" ).toElement();
            QDomElement expandBy = xml.namedItem( "expandby" ).toElement();
            updateSmartPlaylistElement( query );
            updateSmartPlaylistElement( expandBy );
            spl->setXml( xml );
        }
        else
            updateSmartPlaylists( it );
    }
}

void PlaylistBrowser::updateSmartPlaylistElement( QDomElement& query )
{
    QRegExp limitSearch( "LIMIT.*(\\d+)\\s*,\\s*(\\d+)" );
    QRegExp selectFromSearch( "SELECT[^'\"]*FROM" );
    for(QDomNode child = query.firstChild();
            !child.isNull();
            child = child.nextSibling() )
    {
        if( child.isText() )
        {
            //HACK this should be refactored to just regenerate the SQL from the <criteria>'s
            QDomText text = child.toText();
            QString sql = text.data();
            if ( selectFromSearch.search( sql ) != -1 )
                sql.replace( selectFromSearch, "SELECT (*ListOfFields*) FROM" );
            if ( limitSearch.search( sql ) != -1 )
                sql.replace( limitSearch,
                    QString( "LIMIT %1 OFFSET %2").arg( limitSearch.capturedTexts()[2].toInt() ).arg( limitSearch.capturedTexts()[1].toInt() ) );

            text.setData( sql );
            break;
        }
    }
}

void PlaylistBrowser::loadDefaultSmartPlaylists()
{
    DEBUG_BLOCK

    const QStringList genres  = CollectionDB::instance()->query( "SELECT DISTINCT name FROM genre;" );
    const QStringList artists = CollectionDB::instance()->artistList();
    SmartPlaylist *item;
    QueryBuilder qb;
    SmartPlaylist *last = 0;
    m_smartDefaults = new PlaylistCategory( m_smartCategory, 0, i18n("Collection") );
    m_smartDefaults->setOpen( m_smartDefaultsOpen );
    m_smartDefaults->setKept( false );
    /********** All Collection **************/
    qb.initSQLDrag();
    qb.sortBy( QueryBuilder::tabArtist, QueryBuilder::valName );
    qb.sortBy( QueryBuilder::tabAlbum, QueryBuilder::valName );
    qb.sortBy( QueryBuilder::tabSong, QueryBuilder::valTrack );

    item = new SmartPlaylist( m_smartDefaults, 0, i18n( "All Collection" ), qb.query() );
    item->setPixmap( 0, SmallIcon( Amarok::icon( "collection" ) ) );
    item->setKept( false );
    /********** Favorite Tracks **************/
    qb.initSQLDrag();
    qb.sortByFavorite();
    qb.setLimit( 0, 15 );
    item = new SmartPlaylist( m_smartDefaults, item, i18n( "Favorite Tracks" ), qb.query() );
    item->setKept( false );
    last = 0;

    qb.initSQLDrag();
    qb.sortByFavorite();
    qb.setLimit( 0, 15 );
    foreach( QString str, artists ) {
        QueryBuilder qbTemp( qb );
        qbTemp.addMatch( QueryBuilder::tabArtist, str );

        last = new SmartPlaylist( item, last, i18n( "By %1", str ), qbTemp.query() );
        last->setKept( false );
    }

    /********** Most Played **************/
    qb.initSQLDrag();
    qb.sortBy( QueryBuilder::tabStats, QueryBuilder::valPlayCounter, true );
    qb.setLimit( 0, 15 );

    item = new SmartPlaylist( m_smartDefaults, item, i18n( "Most Played" ), qb.query() );
    item->setKept( false );
    last = 0;

    qb.initSQLDrag();
    qb.sortBy( QueryBuilder::tabStats, QueryBuilder::valPlayCounter, true );
    qb.setLimit( 0, 15 );
    foreach( QString str, artists ) {
        QueryBuilder qbTemp( qb );
        qbTemp.addMatch( QueryBuilder::tabArtist, str );

        last = new SmartPlaylist( item, last, i18n( "By %1", str ), qbTemp.query() );
        last->setKept( false );
    }

    /********** Newest Tracks **************/
    qb.initSQLDrag();
    qb.sortBy( QueryBuilder::tabSong, QueryBuilder::valCreateDate, true );
    qb.setLimit( 0, 15 );

    item = new SmartPlaylist( m_smartDefaults, item, i18n( "Newest Tracks" ), qb.query() );
    item->setKept( false );
    last = 0;

    qb.initSQLDrag();
    qb.sortBy( QueryBuilder::tabSong, QueryBuilder::valCreateDate, true );
    qb.setLimit( 0, 15 );
    foreach( QString str, artists ) {
        QueryBuilder qbTemp( qb );
        qbTemp.addMatch( QueryBuilder::tabArtist, str );

        last = new SmartPlaylist( item, last, i18n( "By %1", str ), qbTemp.query( true ));
        last->setKept( false );
    }

    /********** Last Played **************/
    qb.initSQLDrag();
    qb.sortBy( QueryBuilder::tabStats, QueryBuilder::valAccessDate, true );
    qb.setLimit( 0, 15 );

    item = new SmartPlaylist( m_smartDefaults, item, i18n( "Last Played" ), qb.query( true ) );
    item->setKept( false );

    /********** Never Played **************/
    qb.initSQLDrag();
    qb.addNumericFilter( QueryBuilder::tabStats, QueryBuilder::valPlayCounter, "0" );
    qb.sortBy( QueryBuilder::tabArtist, QueryBuilder::valName );
    qb.sortBy( QueryBuilder::tabAlbum, QueryBuilder::valName );
    qb.sortBy( QueryBuilder::tabSong, QueryBuilder::valTrack );

    item = new SmartPlaylist( m_smartDefaults, item, i18n( "Never Played" ), qb.query( true ) );
    item->setKept( false );

    /********** Ever Played **************/
    qb.initSQLDrag();
    qb.excludeFilter( QueryBuilder::tabStats, QueryBuilder::valPlayCounter, "1", QueryBuilder::modeLess );
    qb.sortBy( QueryBuilder::tabArtist, QueryBuilder::valName );
    qb.sortBy( QueryBuilder::tabAlbum, QueryBuilder::valName );
    qb.sortBy( QueryBuilder::tabSong, QueryBuilder::valTrack );
    qb.sortBy( QueryBuilder::tabStats, QueryBuilder::valScore );

    item = new SmartPlaylist( m_smartDefaults, item, i18n( "Ever Played" ), qb.query( true ) );
    item->setKept( false );

    /********** Genres **************/
    item = new SmartPlaylist( m_smartDefaults, item, i18n( "Genres" ), QString() );
    item->setKept( false );
    last = 0;

    qb.initSQLDrag();
    qb.sortBy( QueryBuilder::tabArtist, QueryBuilder::valName );
    qb.sortBy( QueryBuilder::tabAlbum, QueryBuilder::valName );
    qb.sortBy( QueryBuilder::tabSong, QueryBuilder::valTrack );
    foreach( QString str, genres ) {
        QueryBuilder qbTemp( qb );
        qbTemp.addMatch( QueryBuilder::tabGenre, str );

        last = new SmartPlaylist( item, last, i18n( "%1", str ), qbTemp.query( true ) );
        last->setKept( false );
    }

    /********** 50 Random Tracks **************/
    qb.initSQLDrag();
    qb.setOptions( QueryBuilder::optRandomize );
    qb.setLimit( 0, 50 );
    item = new SmartPlaylist( m_smartDefaults, item, i18n( "50 Random Tracks" ), qb.query( true ) );
    item->setKept( false );
}

void PlaylistBrowser::editSmartPlaylist( SmartPlaylist* item )
{
    SmartPlaylistEditor dialog( this, item->xml() );

    if( dialog.exec() == QDialog::Accepted )
    {
        item->setXml ( dialog.result()  );
        item->setText( 0, dialog.name() );

        if( item->isDynamic() ) // rebuild the cache if the smart playlist has changed
            Playlist::instance()->rebuildDynamicModeCache();
    }
}

void PlaylistBrowser::saveSmartPlaylists( PlaylistCategory *smartCategory )
{
    QFile file( smartplaylistBrowserCache() );

    if( !smartCategory )
        smartCategory = m_smartCategory;

    // If the user hadn't set a collection, we didn't create the Smart Playlist Item
    if( !smartCategory ) return;

    QDomDocument doc;
    QDomElement smartB = smartCategory->xml();
    smartB.setAttribute( "product", "Amarok" );
    smartB.setAttribute( "version", APP_VERSION );
    smartB.setAttribute( "formatversion", "1.8" );
    QDomNode smartplaylistsNode = doc.importNode( smartB, true );
    doc.appendChild( smartplaylistsNode );

    QString temp( doc.toString() );

    // Only open the file after all data is ready. If it crashes, data is not lost!
    if ( !file.open( QIODevice::WriteOnly ) ) return;

    QTextStream smart( &file );
    smart.setEncoding( QTextStream::UnicodeUTF8 );
    smart << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    smart << temp;
}

/**
 *************************************************************************
 *  PARTIES
 *************************************************************************
 **/

QString PlaylistBrowser::dynamicBrowserCache() const
{
    return Amarok::saveLocation() + "dynamicbrowser_save.xml";
}

PlaylistCategory* PlaylistBrowser::loadDynamics()
{
    QFile file( dynamicBrowserCache() );

    QTextStream stream( &file );
    stream.setCodec( "UTF8" );

    QDomDocument d;
    QDomElement e;

    PlaylistCategory *after = m_smartCategory;
    if( CollectionDB::instance()->isEmpty() || !m_smartCategory ) // incase of no collection
        after = m_playlistCategory;

    if( !file.open( QIODevice::ReadOnly ) || !d.setContent( stream.read() ) )
    { /*Couldn't open the file or it had invalid content, so let's create some defaults*/
        PlaylistCategory *p = new PlaylistCategory( m_listview, after, i18n("Dynamic Playlists") );
        return p;
    }
    else {
        e = d.namedItem( "category" ).toElement();
        QString version = e.attribute("formatversion");
        if ( version == "1.2" ) {
            PlaylistCategory* p = new PlaylistCategory( m_listview, after, e );
            p->setText( 0, i18n("Dynamic Playlists") );
            return p;
        }
        else if ( version == "1.1" ) {
            // In 1.1, playlists would be referred only by its name.
            // TODO: We can *try* to convert by using findItem
            PlaylistCategory* p = new PlaylistCategory( m_listview, after, e );
            p->setText( 0, i18n("Dynamic Playlists") );
            fixDynamicPlaylistPath( p );
            return p;
        }
        else { // Old unversioned format
            PlaylistCategory* p = new PlaylistCategory( m_listview, after, i18n("Dynamic Playlists") );
            Q3ListViewItem *last = 0;
            QDomNode n = d.namedItem( "dynamicbrowser" ).namedItem("dynamic");
            for( ; !n.isNull();  n = n.nextSibling() ) {
                last = new DynamicEntry( p, last, n.toElement() );
            }
            return p;
        }
    }
}


void
PlaylistBrowser::fixDynamicPlaylistPath( Q3ListViewItem *item )
{
    DynamicEntry *entry = dynamic_cast<DynamicEntry*>( item );
    if ( entry ) {
        QStringList names = entry->items();
        QStringList paths;
        foreach( QString str, names ) {
            QString path = guessPathFromPlaylistName( str );
            if ( !path.isNull() )
                paths+=path;
        }
        entry->setItems( paths );
    }
    PlaylistCategory *cat = dynamic_cast<PlaylistCategory*>( item );
    if ( cat ) {
        Q3ListViewItem *it = cat->firstChild();
        for( ; it; it = it->nextSibling() ) {
            fixDynamicPlaylistPath( it );
        }
    }
}

QString
PlaylistBrowser::guessPathFromPlaylistName( QString name )
{
    Q3ListViewItem *item = m_listview->findItem( name, 0, Q3ListView::ExactMatch );
    PlaylistBrowserEntry *entry = dynamic_cast<PlaylistBrowserEntry*>( item );
    if ( entry )
        return entry->name();
    return QString();
}

void PlaylistBrowser::saveDynamics()
{
    Amarok::config( "PlaylistBrowser" ).writeEntry( "Dynamic Random Remove Played",  m_randomDynamic->cycleTracks() );
    Amarok::config( "PlaylistBrowser" ).writeEntry( "Dynamic Random Upcoming Count", m_randomDynamic->upcomingCount() );
    Amarok::config( "PlaylistBrowser" ).writeEntry( "Dynamic Random Previous Count", m_randomDynamic->previousCount() );

    Amarok::config( "PlaylistBrowser" ).writeEntry( "Dynamic Suggest Remove Played",  m_suggestedDynamic->cycleTracks() );
    Amarok::config( "PlaylistBrowser" ).writeEntry( "Dynamic Suggest Upcoming Count", m_suggestedDynamic->upcomingCount() );
    Amarok::config( "PlaylistBrowser" ).writeEntry( "Dynamic Suggest Previous Count", m_suggestedDynamic->previousCount() );

    QFile file( dynamicBrowserCache() );
    QTextStream stream( &file );

    QDomDocument doc;
    QDomElement dynamicB = m_dynamicCategory->xml();
    dynamicB.setAttribute( "product", "Amarok" );
    dynamicB.setAttribute( "version", APP_VERSION );
    dynamicB.setAttribute( "formatversion", "1.2" );
    QDomNode dynamicsNode = doc.importNode( dynamicB, true );
    doc.appendChild( dynamicsNode );

    QString temp( doc.toString() );

    // Only open the file after all data is ready. If it crashes, data is not lost!
    if ( !file.open( QIODevice::WriteOnly ) ) return;

    stream.setCodec( "UTF8" );
    stream << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    stream << temp;
}

void PlaylistBrowser::loadDynamicItems()
{
    // Make sure all items are unmarked
    for( uint i=0; i < m_dynamicEntries.count(); i++ )
    {
        Q3ListViewItem *it = m_dynamicEntries.at( i );

        if( it )
            static_cast<PlaylistBrowserEntry*>(it)->setDynamic( false );
    }
    m_dynamicEntries.clear();  // Don't use remove(), since we do i++, which would cause skip overs!!!

    // Mark appropriate items as used
    markDynamicEntries();
}

/**
 *************************************************************************
 *  PODCASTS
 *************************************************************************
 **/

QString PlaylistBrowser::podcastBrowserCache() const
{
    //returns the playlists stats cache file
    return Amarok::saveLocation() + "podcastbrowser_save.xml";
}

PlaylistCategory* PlaylistBrowser::loadPodcasts()
{
    DEBUG_BLOCK

    QFile file( podcastBrowserCache() );
    QTextStream stream( &file );
    stream.setCodec( "UTF8" );

    QDomDocument d;
    QDomElement e;

    Q3ListViewItem *after = 0;

    if( !file.open( QIODevice::ReadOnly ) || !d.setContent( stream.read() ) )
    { /*Couldn't open the file or it had invalid content, so let's create an empty element*/
        PlaylistCategory *p = new PlaylistCategory( m_listview, after, i18n("Podcasts") );
        p->setId( 0 );
        loadPodcastsFromDatabase( p );
        return p;
    }
    else {
        e = d.namedItem( "category" ).toElement();

        if ( e.attribute("formatversion") == "1.1" ) {
            debug() << "Podcasts are being moved to the database..." << endl;
            m_podcastItemsToScan.clear();

            PlaylistCategory *p = new PlaylistCategory( m_listview, after, e );
            p->setId( 0 );
            //delete the file, it is deprecated
            KIO::del( KUrl( podcastBrowserCache() ) );

            if( !m_podcastItemsToScan.isEmpty() )
                m_podcastTimer->start( m_podcastTimerInterval );

            return p;
        }
    }
    PlaylistCategory *p = new PlaylistCategory( m_listview, after, i18n("Podcasts") );
    p->setId( 0 );
    return p;
}

void PlaylistBrowser::loadPodcastsFromDatabase( PlaylistCategory *p )
{
DEBUG_BLOCK
    if( !p )   p = m_podcastCategory;
    m_podcastItemsToScan.clear();

    while( p->firstChild() )
        delete p->firstChild();

    QMap<int,PlaylistCategory*> folderMap = loadPodcastFolders( p );

    Q3ValueList<PodcastChannelBundle> channels;

    channels = CollectionDB::instance()->getPodcastChannels();

    PodcastChannel *channel = 0;

    oldForeachType( Q3ValueList<PodcastChannelBundle>, channels )
    {
        PlaylistCategory *parent = p;
        const int parentId = (*it).parentId();
        if( parentId > 0 && folderMap.find( parentId ) != folderMap.end() )
            parent = folderMap[parentId];

        channel  = new PodcastChannel( parent, channel, *it );

        bool hasNew = CollectionDB::instance()->query( QString("SELECT COUNT(parent) FROM podcastepisodes WHERE ( parent='%1' AND isNew=%2 ) LIMIT 1" )
                        .arg( (*it).url().url(), CollectionDB::instance()->boolT() ) )
                        .first().toInt() > 0;

        channel->setNew( hasNew );

        if( channel->autoscan() )
            m_podcastItemsToScan.append( channel );
    }

    if( !m_podcastItemsToScan.isEmpty() )
        m_podcastTimer->start( m_podcastTimerInterval );
}

QMap<int,PlaylistCategory*>
PlaylistBrowser::loadPodcastFolders( PlaylistCategory *p )
{
DEBUG_BLOCK
    QString sql = "SELECT * FROM podcastfolders ORDER BY parent ASC;";
    QStringList values = CollectionDB::instance()->query( sql );

    // store the folder and IDs so finding a parent is fast
    QMap<int,PlaylistCategory*> folderMap;
    PlaylistCategory *folder = 0;
    oldForeach( values )
    {
        const int     id       =     (*it).toInt();
        const QString t        =    *++it;
        const int     parentId =   (*++it).toInt();
        const bool    isOpen   = ( (*++it) == CollectionDB::instance()->boolT() ? true : false );

        PlaylistCategory *parent = p;
        if( parentId > 0 && folderMap.find( parentId ) != folderMap.end() )
            parent = folderMap[parentId];

        folder = new PlaylistCategory( parent, folder, t, id );
        folder->setOpen( isOpen );

        folderMap[id] = folder;
    }
    // check if the base folder exists
    p->setOpen( Amarok::config( "PlaylistBrowser" ).readEntry( "Podcast Folder Open", true ) );

    return folderMap;
}

void PlaylistBrowser::savePodcastFolderStates( PlaylistCategory *folder )
{
    if( !folder ) return;

    PlaylistCategory *child = static_cast<PlaylistCategory*>(folder->firstChild());
    while( child )
    {
        if( isCategory( child ) )
            savePodcastFolderStates( child );
        else
            break;

        child = static_cast<PlaylistCategory*>(child->nextSibling());
    }
    if( folder != m_podcastCategory )
    {
        if( folder->id() < 0 ) // probably due to a 1.3->1.4 migration
        {                      // we add the folder to the db, set the id and then update all the children
            int parentId = static_cast<PlaylistCategory*>(folder->parent())->id();
            int newId = CollectionDB::instance()->addPodcastFolder( folder->text(0), parentId, folder->isOpen() );
            folder->setId( newId );
            PodcastChannel *chan = static_cast<PodcastChannel*>(folder->firstChild());
            while( chan )
            {
                if( isPodcastChannel( chan ) )
                    // will update the database so child has correct parentId.
                    chan->setParent( folder );
                chan = static_cast<PodcastChannel*>(chan->nextSibling());
            }
        }
        else
        {
            CollectionDB::instance()->updatePodcastFolder( folder->id(), folder->text(0),
                              static_cast<PlaylistCategory*>(folder->parent())->id(), folder->isOpen() );
        }
    }
}

void PlaylistBrowser::scanPodcasts()
{
    //don't want to restart timer unnecessarily.  addPodcast will start it if it is necessary
    if( m_podcastItemsToScan.isEmpty() ) return;

    for( uint i=0; i < m_podcastItemsToScan.count(); i++ )
    {
        Q3ListViewItem  *item = m_podcastItemsToScan.at( i );
        PodcastChannel *pc   = static_cast<PodcastChannel*>(item);
        pc->rescan();
    }
    //restart timer
    m_podcastTimer->start( m_podcastTimerInterval );
}

void PlaylistBrowser::refreshPodcasts( Q3ListViewItem *parent )
{
    for( Q3ListViewItem *child = parent->firstChild();
            child;
            child = child->nextSibling() )
    {
        if( isPodcastChannel( child ) )
            static_cast<PodcastChannel*>( child )->rescan();
        else if( isCategory( child ) )
            refreshPodcasts( child );
    }
}

void PlaylistBrowser::addPodcast( Q3ListViewItem *parent )
{
    bool ok;
    const QString name = KInputDialog::getText(i18n("Add Podcast"), i18n("Enter Podcast URL:"), QString(), &ok, this);

    if( ok && !name.isEmpty() )
    {
        addPodcast( KUrl( name ), parent );
    }
}

void PlaylistBrowser::configurePodcasts( Q3ListViewItem *parent )
{
    Q3PtrList<PodcastChannel> podcastChannelList;
    for( Q3ListViewItem *child = parent->firstChild();
         child;
         child = child->nextSibling() )
    {
        if( isPodcastChannel( child ) )
        {
            podcastChannelList.append( static_cast<PodcastChannel*>( child ) );
        }
    }
    if( !podcastChannelList.isEmpty() )
        configurePodcasts( podcastChannelList, i18nc( "Podcasts contained in %1",
                           "All in %1", parent->text( 0 ) ) );
}

void PlaylistBrowser::configureSelectedPodcasts()
{
    Q3PtrList<PodcastChannel> selected;
    Q3ListViewItemIterator it( m_listview, Q3ListViewItemIterator::Selected);
    for( ; it.current(); ++it )
    {
        if( isPodcastChannel( (*it) ) )
            selected.append( static_cast<PodcastChannel*>(*it) );
    }
    if (selected.isEmpty() )
        return; //shouldn't happen

    if( selected.count() == 1 )
        selected.getFirst()->configure();
    else
        configurePodcasts( selected, i18np("1 Podcast", "%1 Podcasts", selected.count() ) );

    if( m_podcastItemsToScan.isEmpty() )
        m_podcastTimer->stop();

    else if( m_podcastItemsToScan.count() == 1 )
        m_podcastTimer->start( m_podcastTimerInterval );
                    // else timer is already running
}

void PlaylistBrowser::configurePodcasts( Q3PtrList<PodcastChannel> &podcastChannelList,
                                         const QString &caption )
{

    if( podcastChannelList.isEmpty() )
    {
        debug() << "BUG: podcastChannelList is empty" << endl;
        return;
    }
    Q3PtrList<PodcastSettings> podcastSettingsList;
    oldForeachType( Q3PtrList<PodcastChannel>, podcastChannelList)
    {
        podcastSettingsList.append( (*it)->getSettings() );
    }
    PodcastSettingsDialog *dialog = new PodcastSettingsDialog( podcastSettingsList, caption );
    if( dialog->configure() )
    {
        PodcastChannel *channel = podcastChannelList.first();
        oldForeachType( Q3PtrList<PodcastSettings>, podcastSettingsList )
        {
            if ( (*it)->title() ==  channel->title() )
            {
                channel->setSettings( *it );
            }
            else
                debug() << " BUG in playlistbrowser.cpp:configurePodcasts( )" << endl;

            channel = podcastChannelList.next();
        }
    }
}

PodcastChannel *
PlaylistBrowser::findPodcastChannel( const KUrl &feed, Q3ListViewItem *parent ) const
{
    if( !parent ) parent = static_cast<Q3ListViewItem*>(m_podcastCategory);

    for( Q3ListViewItem *it = parent->firstChild();
            it;
            it = it->nextSibling() )
    {
        if( isPodcastChannel( it ) )
        {
            PodcastChannel *channel = static_cast<PodcastChannel *>( it );
            if( channel->url().prettyUrl() == feed.prettyUrl() )
            {
                return channel;
            }
        }
        else if( isCategory( it ) )
        {
            PodcastChannel *channel = findPodcastChannel( feed, it );
            if( channel )
                return channel;
        }
    }

    return 0;
}

PodcastEpisode *
PlaylistBrowser::findPodcastEpisode( const KUrl &episode, const KUrl &feed ) const
{
    PodcastChannel *channel = findPodcastChannel( feed );
    if( !channel )
        return 0;

    if( !channel->isPolished() )
        channel->load();

    Q3ListViewItem *child = channel->firstChild();
    while( child )
    {
        #define child static_cast<PodcastEpisode*>(child)
        if( child->url() == episode )
            return child;
        #undef  child
        child = child->nextSibling();
    }

    return 0;
}

void PlaylistBrowser::addPodcast( const KUrl& origUrl, Q3ListViewItem *parent )
{
    if( !parent ) parent = static_cast<Q3ListViewItem*>(m_podcastCategory);

    KUrl url( origUrl );
    if( url.protocol() == "itpc" || url.protocol() == "pcast" )
        url.setProtocol( "http" );

    PodcastChannel *channel = findPodcastChannel( url );
    if( channel )
    {
        Amarok::StatusBar::instance()->longMessage(
                i18n( "Already subscribed to feed %1 as %2", url.prettyUrl(),
                     channel->title() ),
                KDE::StatusBar::Sorry );
        return;
    }

    PodcastChannel *pc = new PodcastChannel( parent, 0, url );

    if( m_podcastItemsToScan.isEmpty() )
    {
        m_podcastItemsToScan.append( pc );
        m_podcastTimer->start( m_podcastTimerInterval );
    }
    else
    {
        m_podcastItemsToScan.append( pc );
    }

    parent->sortChildItems( 0, true );
    parent->setOpen( true );
}

void PlaylistBrowser::changePodcastInterval()
{
    double time = static_cast<double>(m_podcastTimerInterval / ( 60 * 60 * 1000 ));
    bool ok;
    double interval = KInputDialog::getDouble( i18n("Download Interval"),
                                            i18n("Scan interval (hours):"), time,
                                            0.5, 100.0, .5, 1, // min, max, step, base
                                            &ok, this);
    int milliseconds = static_cast<int>(interval*60.0*60.0*1000.0);
    if( ok )
    {
        if( milliseconds != m_podcastTimerInterval )
        {
            m_podcastTimerInterval = milliseconds;
            m_podcastTimer->start( m_podcastTimerInterval );
        }
    }
}

bool PlaylistBrowser::deleteSelectedPodcastItems( const bool removeItem, const bool silent )
{
    KUrl::List urls;
    Q3ListViewItemIterator it( m_podcastCategory, Q3ListViewItemIterator::Selected );
    Q3PtrList<PodcastEpisode> erasedItems;

    for( ; it.current(); ++it )
    {
        if( isPodcastEpisode( *it ) )
        {
            #define item static_cast<PodcastEpisode*>(*it)
            if( item->isOnDisk() ) {
                urls.append( item->localUrl() );
                erasedItems.append( item );
            }
            #undef  item
        }
    }

    if( urls.isEmpty() ) return false;
    int button;
    if( !silent )
        button = KMessageBox::warningContinueCancel( this,
                    i18np( "<p>You have selected 1 podcast episode to be <b>irreversibly</b> deleted. ",
                          "<p>You have selected %1 podcast episodes to be <b>irreversibly</b> deleted. ",
                           urls.count() ), QString(), KStandardGuiItem::del() );
    if( silent || button != KMessageBox::Continue )
        return false;

    KIO::Job *job = KIO::del( urls );

    PodcastEpisode *item;
    for ( item = erasedItems.first(); item; item = erasedItems.next() )
    {
        if( removeItem )
        {
            CollectionDB::instance()->removePodcastEpisode( item->dBId() );
            delete item;
        }
        else
            connect( job, SIGNAL( result( KIO::Job* ) ), item, SLOT( isOnDisk() ) );;
    }
    return true;
}

bool PlaylistBrowser::deletePodcasts( Q3PtrList<PodcastChannel> items )
{
    if( items.isEmpty() ) return false;

    KUrl::List urls;
    oldForeachType( Q3PtrList<PodcastChannel>, items )
    {
        for( Q3ListViewItem *ch = (*it)->firstChild(); ch; ch = ch->nextSibling() )
        {
            #define ch static_cast<PodcastEpisode*>(ch)
            if( ch->isOnDisk() )
            {
                //delete downloaded media
                urls.append( ch->localUrl() );
            }
            #undef  ch
            /// we don't need to delete from the database, because removing the channel from the database
            /// automatically removes the children as well.
            m_podcastItemsToScan.remove( static_cast<PodcastChannel*>(*it) );
        }
        CollectionDB::instance()->removePodcastChannel( static_cast<PodcastChannel*>(*it)->url() );

    }
    // TODO We need to check which files have been deleted successfully
    if ( urls.count() )
        KIO::del( urls );
    return true;
}

void PlaylistBrowser::downloadSelectedPodcasts()
{
    Q3ListViewItemIterator it( m_listview, Q3ListViewItemIterator::Selected );

    for( ; it.current(); ++it )
    {
        if( isPodcastEpisode( *it ) )
        {
            #define item static_cast<PodcastEpisode*>(*it)
            if( !item->isOnDisk() )
                m_podcastDownloadQueue.append( item );
            #undef  item
        }
    }
    downloadPodcastQueue();
}

void PlaylistBrowser::downloadPodcastQueue() //SLOT
{
    if( m_podcastDownloadQueue.isEmpty() ) return;

    PodcastEpisode *first = m_podcastDownloadQueue.first();
    first->downloadMedia();
    m_podcastDownloadQueue.removeFirst();

    connect( first, SIGNAL( downloadFinished() ), this, SLOT( downloadPodcastQueue() ) );
    connect( first, SIGNAL( downloadAborted() ),  this, SLOT( abortPodcastQueue()  ) );
}

void PlaylistBrowser::abortPodcastQueue() //SLOT
{
    m_podcastDownloadQueue.clear();
}

void PlaylistBrowser::registerPodcastSettings( const QString &title, const PodcastSettings *settings )
{
    m_podcastSettings.insert( title, settings );
}

/**
 *************************************************************************
 *  PLAYLISTS
 *************************************************************************
 **/

QString PlaylistBrowser::playlistBrowserCache() const
{
    //returns the playlists stats cache file
    return Amarok::saveLocation() + "playlistbrowser_save.xml";
}

PlaylistCategory* PlaylistBrowser::loadPlaylists()
{
    QFile file( playlistBrowserCache() );

    QTextStream stream( &file );
    stream.setCodec( "UTF8" );

    QDomDocument d;
    QDomElement e;

    if( !file.open( QIODevice::ReadOnly ) || !d.setContent( stream.read() ) )
    { /*Couldn't open the file or it had invalid content, so let's create an empty element*/
        return new PlaylistCategory(m_listview, 0 , i18n("Playlists") );
    }
    else {
        e = d.namedItem( "category" ).toElement();
        if ( e.attribute("formatversion") =="1.1" )
        {
            PlaylistCategory* p = new PlaylistCategory( m_listview, 0 , e );
            p->setText( 0, i18n("Playlists") );
            return p;
        }
        else { // Old unversioned format
            PlaylistCategory* p = new PlaylistCategory( m_listview, 0 , i18n("Playlists") );
            Q3ListViewItem *last = 0;
            QDomNode n = d.namedItem( "playlistbrowser" ).namedItem("playlist");

            for ( ; !n.isNull();  n = n.nextSibling() )
                last = new PlaylistEntry( p, last, n.toElement() );

            return p;
        }
    }
}

Q3ListViewItem *
PlaylistBrowser::findItemInTree( const QString &searchstring, int c ) const
{
    QStringList list = QStringList::split( "/", searchstring, true );

    // select the 1st level
    QStringList::Iterator it = list.begin();
    Q3ListViewItem *pli = findItem (*it, c);
    if ( !pli ) return pli;

    for ( ++it ; it != list.end(); ++it )
    {

        Q3ListViewItemIterator it2( pli );
        for( ++it2 ; it2.current(); ++it2 )
        {
            if ( *it == (*it2)->text(0) )
            {
                pli = *it2;
                break;
            }
            // test, to not go over into the next category
            if ( isCategory( *it2 ) && (pli->nextSibling() == *it2) )
                return 0;
        }
        if ( ! it2.current() )
            return 0;

    }
    return pli;
}

DynamicMode *PlaylistBrowser::findDynamicModeByTitle( const QString &title )
{
    if( !m_polished )
       polish();

    for ( Q3ListViewItem *item = m_dynamicCategory->firstChild(); item; item = item->nextSibling() )
    {
        DynamicEntry *entry = dynamic_cast<DynamicEntry *>( item );
        if ( entry && entry->title() == title )
            return entry;
    }

    return 0;
}

PlaylistEntry *
PlaylistBrowser::findPlaylistEntry( const QString &url, Q3ListViewItem *parent ) const
{
    if( !parent ) parent = static_cast<Q3ListViewItem*>(m_playlistCategory);

    for( Q3ListViewItem *it = parent->firstChild();
            it;
            it = it->nextSibling() )
    {
        if( isPlaylist( it ) )
        {
            PlaylistEntry *pl = static_cast<PlaylistEntry*>( it );
            debug() << pl->url().path() << " == " << url << endl;
            if( pl->url().path() == url )
            {
                debug() << "ok!" << endl;
                return pl;
            }
        }
        else if( isCategory( it ) )
        {
            PlaylistEntry *pl = findPlaylistEntry( url, it );
            if( pl )
                return pl;
        }
    }

    return 0;
}

int PlaylistBrowser::loadPlaylist( const QString &playlist, bool /*force*/ )
{
    // roland
    DEBUG_BLOCK

    Q3ListViewItem *pli = findItemInTree( playlist, 0 );
    if ( ! pli ) return -1;

    slotDoubleClicked( pli );
    return 0;
    // roland
}

void PlaylistBrowser::addPlaylist( const QString &path, Q3ListViewItem *parent, bool force, bool imported )
{
    // this function adds a playlist to the playlist browser

    if( !m_polished )
       polish();

    QFile file( path );
    if( !file.exists() ) return;

    PlaylistEntry *playlist = findPlaylistEntry( path );

    if( playlist && force )
        playlist->load(); //reload the playlist

    if( imported ) {
        Q3ListViewItem *playlistImports = 0;
        //First try and find the imported folder
        for ( Q3ListViewItem *it = m_playlistCategory->firstChild(); it; it = it->nextSibling() )
        {
            if ( dynamic_cast<PlaylistCategory*>( it ) && static_cast<PlaylistCategory*>( it )->isFolder() &&
                 it->text( 0 ) == i18n( "Imported" ) )
            {
                playlistImports = it;
                break;
            }
        }
        if ( !playlistImports )   //We didn't find the Imported folder, so create it.
            playlistImports = new PlaylistCategory( m_playlistCategory, 0, i18n("Imported") );
        parent = playlistImports;
    }
    else if( !parent ) parent = static_cast<Q3ListViewItem*>(m_playlistCategory);

    if( !playlist ) {
        if( !m_playlistCategory || !m_playlistCategory->childCount() ) {  //first child
            removeButton->setEnabled( true );
            renameButton->setEnabled( true );
        }

        KUrl auxKURL;
        auxKURL.setPath(path);
        m_lastPlaylist = playlist = new PlaylistEntry( parent, 0, auxKURL );
    }

    parent->setOpen( true );
    parent->sortChildItems( 0, true );
    m_listview->clearSelection();
    playlist->setSelected( true );
}

bool PlaylistBrowser::savePlaylist( const QString &path, const Q3ValueList<KUrl> &in_urls,
                                    const Q3ValueList<QString> &titles, const Q3ValueList<int> &lengths,
                                    bool relative )
{
    if( path.isEmpty() )
        return false;

    QFile file( path );

    if( !file.open( QIODevice::WriteOnly ) )
    {
        KMessageBox::sorry( PlaylistWindow::self(), i18n( "Cannot write playlist (%1).").arg(path) );
        return false;
    }

    QTextStream stream( &file );
    stream << "#EXTM3U\n";

    KUrl::List urls;
    for( int i = 0, n = in_urls.count(); i < n; ++i )
    {
        const KUrl &url = in_urls[i];
        if( url.isLocalFile() && QFileInfo( url.path() ).isDir() )
            urls += recurse( url );
        else
            urls += url;
    }

    for( int i = 0, n = urls.count(); i < n; ++i )
    {
        const KUrl &url = urls[i];

        if( !titles.isEmpty() && !lengths.isEmpty() )
        {
            stream << "#EXTINF:";
            stream << QString::number( lengths[i] );
            stream << ',';
            stream << titles[i];
            stream << '\n';
        }
        if (url.protocol() == "file" ) {
            if ( relative ) {
                const QFileInfo fi(file);
                stream << KUrl::relativePath(fi.path(), url.path());
            } else
                stream << url.path();
        } else {
            stream << url.url();
        }
        stream << "\n";
    }

    file.close(); // Flushes the file, before we read it
    PlaylistBrowser::instance()->addPlaylist( path, 0, true );

    return true;
}

void PlaylistBrowser::openPlaylist( Q3ListViewItem *parent ) //SLOT
{
    // open a file selector to add playlists to the playlist browser
    QStringList files;
    files = KFileDialog::getOpenFileNames( KUrl(), "*.m3u *.pls *.xspf|" + i18n("Playlist Files"), this, i18n("Import Playlists") );

    const QStringList::ConstIterator end  = files.constEnd();
    for( QStringList::ConstIterator it = files.constBegin(); it != end; ++it )
        addPlaylist( *it, parent );

    savePlaylists();
}

void PlaylistBrowser::savePlaylists()
{
    QFile file( playlistBrowserCache() );

    QDomDocument doc;
    QDomElement playlistsB = m_playlistCategory->xml();
    playlistsB.setAttribute( "product", "Amarok" );
    playlistsB.setAttribute( "version", APP_VERSION );
    playlistsB.setAttribute( "formatversion", "1.1" );
    QDomNode playlistsNode = doc.importNode( playlistsB, true );
    doc.appendChild( playlistsNode );

    QString temp( doc.toString() );

    // Only open the file after all data is ready. If it crashes, data is not lost!
    if ( !file.open( QIODevice::WriteOnly ) ) return;

    QTextStream stream( &file );
    stream.setCodec( "UTF8" );
    stream << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    stream << temp;
}

bool PlaylistBrowser::deletePlaylists( Q3PtrList<PlaylistEntry> items )
{
    KUrl::List urls;
    oldForeachType( Q3PtrList<PlaylistEntry>, items )
    {
        urls.append( (*it)->url() );
    }
    if( !urls.isEmpty() )
        return deletePlaylists( urls );

    return false;
}

bool PlaylistBrowser::deletePlaylists( KUrl::List items )
{
    if ( items.isEmpty() ) return false;

    // TODO We need to check which files have been deleted successfully
    // Avoid deleting dirs. See bug #122480
    for ( KUrl::List::iterator it = items.begin(), end = items.end(); it != end; ++it ) {
        if ( QFileInfo( (*it).path() ).isDir() ) {
            it = items.erase( it );
            continue;
        }
    }
    KIO::del( items );
    return true;
}

void PlaylistBrowser::savePlaylist( PlaylistEntry *item )
{
    bool append = false;

    if( item->trackList().count() == 0 ) //the playlist hasn't been loaded so we append the dropped tracks
        append = true;

    //save the modified playlist in m3u or pls format
    const QString ext = fileExtension( item->url().path() );
    if( ext.toLower() == "m3u" )
        saveM3U( item, append );
    else if ( ext.toLower() == "xspf" )
        saveXSPF( item, append );
    else
        savePLS( item, append );
}

/**
 *************************************************************************
 *  General Methods
 *************************************************************************
 **/

PlaylistBrowserEntry *
PlaylistBrowser::findItem( QString &t, int c ) const
{
    return static_cast<PlaylistBrowserEntry *>( m_listview->findItem( t, c, Q3ListView::ExactMatch ) );
}

bool PlaylistBrowser::createPlaylist( Q3ListViewItem *parent, bool current, QString title )
{
    if( title.isEmpty() ) title = i18n("Untitled");

    const QString path = PlaylistDialog::getSaveFileName( title );
    if( path.isEmpty() )
        return false;

    if( !parent )
        parent = static_cast<Q3ListViewItem *>( m_playlistCategory );

    if( current )
    {
        if ( !Playlist::instance()->saveM3U( path ) ) {
            return false;
        }
    }
    else
    {
        //Remove any items in Listview that have the same path as this one
        //  Should only happen when overwriting a playlist
        Q3ListViewItem *item = parent->firstChild();
        while( item )
        {
            if( static_cast<PlaylistEntry*>( item )->url() == path )
            {
                Q3ListViewItem *todelete = item;
                item = item->nextSibling();
                delete todelete;
            }
            else
                item = item->nextSibling();
        }

        //Remove existing playlist if it exists
        if ( QFileInfo( path ).exists() )
            QFileInfo( path ).dir().remove( path );

        m_lastPlaylist = new PlaylistEntry( parent, 0, path );
        parent->sortChildItems( 0, true );
    }

    savePlaylists();

    return true;
}

void PlaylistBrowser::addSelectedToPlaylist( int options )
{
    if ( options == -1 )
        options = Playlist::Unique | Playlist::Append;

    KUrl::List list;

    Q3ListViewItemIterator it( m_listview, Q3ListViewItemIterator::Selected );
    for( ; it.current(); ++it )
    {
        #define item (*it)
        if ( isPlaylist( item ) )
            list << static_cast<PlaylistEntry*>(item)->url();

        else if( isLastFm( item ) )
            list << static_cast<LastFmEntry*>(item)->url();

        else if ( isStream( item ) )
            list << static_cast<StreamEntry*>(item)->url();

        else if ( isPodcastChannel( item ) )
        {
            #define channel static_cast<PodcastChannel*>(item)
            if( !channel->isPolished() )
                 channel->load();
            #undef  channel
            KUrl::List _list;
            Q3ListViewItem *child = item->firstChild();
            while( child )
            {
                #define child static_cast<PodcastEpisode *>(child)
                child->isOnDisk() ?
                    _list.prepend( child->localUrl() ):
                    _list.prepend( child->url()      );
                #undef child
                child = child->nextSibling();
            }
            list += _list ;
        }

        else if ( isPodcastEpisode( item ) )
        {
            #define pod static_cast<PodcastEpisode*>(item)
            if( pod->isOnDisk() )
                list << pod->localUrl();
            else
                list << pod->url();
            #undef  pod
        }

        else if ( isPlaylistTrackItem( item ) )
            list << static_cast<PlaylistTrackItem*>(item)->url();
        #undef item
    }

    if( !list.isEmpty() )
        Playlist::instance()->insertMedia( list, options );
}

void
PlaylistBrowser::invokeItem( Q3ListViewItem* i, const QPoint& point, int column ) //SLOT
{
    if( column == -1 )
        return;

    PlaylistBrowserView *view = getListView();

    QPoint p = mapFromGlobal( point );
    if ( p.x() > view->header()->sectionPos( view->header()->mapToIndex( 0 ) ) + view->treeStepSize() * ( i->depth() + ( view->rootIsDecorated() ? 1 : 0) ) + view->itemMargin()
            || p.x() < view->header()->sectionPos( view->header()->mapToIndex( 0 ) ) )
        slotDoubleClicked( i );
}

void PlaylistBrowser::slotDoubleClicked( Q3ListViewItem *item ) //SLOT
{
    if( !item ) return;
    PlaylistBrowserEntry *entry = dynamic_cast<PlaylistBrowserEntry*>(item);
    if ( entry )
        entry->slotDoubleClicked();
}

void PlaylistBrowser::collectionScanDone()
{
    if( !m_polished || CollectionDB::instance()->isEmpty() )
    {
        return;
    }
    else if( !m_smartCategory )
    {
        m_smartCategory = loadSmartPlaylists();
        loadDefaultSmartPlaylists();
        m_smartCategory->setOpen( true );
    }
}

void PlaylistBrowser::removeSelectedItems() //SLOT
{
    // this function remove selected playlists and tracks

    int playlistCount = 0;
    int trackCount    = 0;
    int streamCount   = 0;
    int smartyCount   = 0;
    int dynamicCount  = 0;
    int podcastCount  = 0;
    int folderCount   = 0;
    int lastfmCount   = 0;

    Q3PtrList<PlaylistEntry>    playlistsToDelete;
    Q3PtrList<PodcastChannel>   podcastsToDelete;

    Q3PtrList<PlaylistCategory> playlistFoldersToDelete;
    Q3PtrList<PlaylistCategory> podcastFoldersToDelete;

    //remove currentItem, no matter if selected or not
    m_listview->setSelected( m_listview->currentItem(), true );

    Q3PtrList<Q3ListViewItem> selected;
    Q3ListViewItemIterator it( m_listview, Q3ListViewItemIterator::Selected );
    for( ; it.current(); ++it )
    {
        if( !static_cast<PlaylistBrowserEntry*>(*it)->isKept() )
            continue;

        if( isCategory( *it ) && !static_cast<PlaylistCategory*>(*it)->isFolder() ) //its a base category
            continue;

        // if the playlist containing this item is already selected the current item will be skipped
        // it will be deleted from the parent
        Q3ListViewItem *parent = it.current()->parent();

        if( parent && parent->isSelected() ) //parent will remove children
            continue;

	if (parent) {
            while( parent->parent() && static_cast<PlaylistBrowserEntry*>(parent)->isKept() )
                parent = parent->parent();
	}

        if( parent && !static_cast<PlaylistBrowserEntry*>(parent)->isKept() )
            continue;

        switch( (*it)->rtti() )
        {
            case PlaylistEntry::RTTI:
                playlistsToDelete.append( static_cast<PlaylistEntry*>(*it) );
                playlistCount++;
                continue; // don't add the folder to selected, else it will be deleted twice

            case PlaylistTrackItem::RTTI:
                trackCount++;
                break;

            case LastFmEntry::RTTI:
                lastfmCount++;
                break;

            case StreamEntry::RTTI:
                streamCount++;
                break;

            case DynamicEntry::RTTI:
                dynamicCount++;
                break;

            case SmartPlaylist::RTTI:
                smartyCount++;
                break;

            case PodcastChannel::RTTI:
                podcastCount++;
                podcastsToDelete.append( static_cast<PodcastChannel*>(*it) );
            case PodcastEpisode::RTTI: //episodes can't be removed
                continue; // don't add the folder to selected, else it will be deleted twice

            case PlaylistCategory::RTTI:
                folderCount++;
                if( parent == m_playlistCategory )
                {
                    for( Q3ListViewItem *ch = (*it)->firstChild(); ch; ch = ch->nextSibling() )
                    {
                        if( isCategory( ch ) )
                        {
                            folderCount++;
                            playlistFoldersToDelete.append( static_cast<PlaylistCategory*>(ch) );
                        }
                        else
                        {
                            playlistCount++;
                            playlistsToDelete.append( static_cast<PlaylistEntry*>(ch) );
                        }
                    }
                    playlistFoldersToDelete.append( static_cast<PlaylistCategory*>(*it) );
                    continue; // don't add the folder to selected, else it will be deleted twice
                }
                else if( parent == m_podcastCategory )
                {
                    for( Q3ListViewItem *ch = (*it)->firstChild(); ch; ch = ch->nextSibling() )
                    {
                        if( isCategory( ch ) )
                        {
                            folderCount++;
                            podcastFoldersToDelete.append( static_cast<PlaylistCategory*>(ch) );
                        }
                        else
                        {
                            podcastCount++;
                            podcastsToDelete.append( static_cast<PodcastChannel*>(ch) );
                        }
                    }
                    podcastFoldersToDelete.append( static_cast<PlaylistCategory*>(*it) );
                    continue; // don't add the folder to selected, else it will be deleted twice
                }

            default:
                break;
        }

        selected.append( it.current() );
    }

    int totalCount = playlistCount + smartyCount  + dynamicCount +
                     streamCount   + podcastCount + folderCount  + lastfmCount;

    if( selected.isEmpty() && !totalCount ) return;

    QString message = i18n( "<p>You have selected:<ul>" );

    if( playlistCount ) message += "<li>" + i18np( "1 playlist", "%1 playlists", playlistCount )  + "</li>";

    if( smartyCount   ) message += "<li>" + i18np( "1 smart playlist", "%1 smart playlists", smartyCount ) + "</li>";

    if( dynamicCount  ) message += "<li>" + i18np( "1 dynamic playlist", "%1 dynamic playlists", dynamicCount ) + "</li>";

    if( streamCount   ) message += "<li>" + i18np( "1 stream", "%1 streams", streamCount ) + "</li>";

    if( podcastCount  ) message += "<li>" + i18np( "1 podcast", "%1 podcasts", podcastCount ) + "</li>";

    if( folderCount   ) message += "<li>" + i18np( "1 folder", "%1 folders", folderCount ) + "</li>";

    if( lastfmCount   ) message += "<li>" + i18np( "1 last.fm stream", "%1 last.fm streams", lastfmCount ) + "</li>";

    message += i18n( "</ul><br>to be <b>irreversibly</b> deleted.</p>" );

    if( podcastCount )
        message += i18n( "<br><p>All downloaded podcast episodes will also be deleted.</p>" );

    if( totalCount > 0 )
    {
        int button = KMessageBox::warningContinueCancel( this, message, QString(), KStandardGuiItem::del() );
        if( button != KMessageBox::Continue )
            return;
    }

    oldForeachType( Q3PtrList<Q3ListViewItem>, selected )
    {
        if ( isPlaylistTrackItem( *it ) )
        {
            static_cast<PlaylistEntry*>( (*it)->parent() )->removeTrack( (*it) );
            continue;
        }
        if ( isDynamic( *it ) )
            static_cast<DynamicEntry*>( *it )->deleting();
        delete (*it);
    }

    // used for deleting playlists first, then folders.
    if( playlistCount )
    {
        if( deletePlaylists( playlistsToDelete ) )
        {
            oldForeachType( Q3PtrList<PlaylistEntry>, playlistsToDelete )
            {
                m_dynamicEntries.remove(*it);
                delete (*it);
            }
        }
    }

    if( podcastCount )
    {
        if( deletePodcasts( podcastsToDelete ) )
            oldForeachType( Q3PtrList<PodcastChannel>, podcastsToDelete )
                delete (*it);
    }

    oldForeachType( Q3PtrList<PlaylistCategory>, playlistFoldersToDelete )
        delete (*it);

    oldForeachType( Q3PtrList<PlaylistCategory>, podcastFoldersToDelete )
        removePodcastFolder( *it );

    if( playlistCount || trackCount )
        savePlaylists();

    if( streamCount )        saveStreams();
    if( smartyCount ) saveSmartPlaylists();
    if( dynamicCount )      saveDynamics();
    if( lastfmCount )         saveLastFm();
}

// remove podcast folders. we need to do this recursively to ensure all children are removed from the db
void PlaylistBrowser::removePodcastFolder( PlaylistCategory *item )
{
    if( !item ) return;
    if( !item->childCount() )
    {
        CollectionDB::instance()->removePodcastFolder( item->id() );
        delete item;
        return;
    }

    Q3ListViewItem *child = item->firstChild();
    while( child )
    {
        Q3ListViewItem *nextChild = 0;
        if( isPodcastChannel( child ) )
        {
        #define child static_cast<PodcastChannel*>(child)
            nextChild = child->nextSibling();
            CollectionDB::instance()->removePodcastChannel( child->url() );
            m_podcastItemsToScan.remove( child );
        #undef  child
        }
        else if( isCategory( child ) )
        {
            nextChild = child->nextSibling();
            removePodcastFolder( static_cast<PlaylistCategory*>(child) );
        }

        child = nextChild;
    }
    CollectionDB::instance()->removePodcastFolder( item->id() );
    delete item;
}

void PlaylistBrowser::renameSelectedItem() //SLOT
{
    Q3ListViewItem *item = m_listview->currentItem();
    if( !item ) return;

    if( item == m_randomDynamic || item == m_suggestedDynamic )
        return;

    PlaylistBrowserEntry *entry = dynamic_cast<PlaylistBrowserEntry*>( item );
    if ( entry )
        entry->slotRenameItem();
}


void PlaylistBrowser::renamePlaylist( Q3ListViewItem* item, const QString& newName, int ) //SLOT
{
    PlaylistBrowserEntry *entry = dynamic_cast<PlaylistBrowserEntry*>( item );
    if ( entry )
        entry->slotPostRenameItem( newName );
}


void PlaylistBrowser::saveM3U( PlaylistEntry *item, bool append )
{
    QFile file( item->url().path() );

    if( append ? file.open( QIODevice::WriteOnly | QIODevice::Append ) : file.open( QIODevice::WriteOnly ) )
    {
        QTextStream stream( &file );
        if( !append )
            stream << "#EXTM3U\n";
        Q3PtrList<TrackItemInfo> trackList = append ? item->droppedTracks() : item->trackList();
        for( TrackItemInfo *info = trackList.first(); info; info = trackList.next() )
        {
            stream << "#EXTINF:";
            stream << info->length();
            stream << ',';
            stream << info->title();
            stream << '\n';
            stream << (info->url().protocol() == "file" ? info->url().path() : info->url().url());
            stream << "\n";
        }

        file.close();
    }
}

void PlaylistBrowser::saveXSPF( PlaylistEntry *item, bool append )
{
    XSPFPlaylist playlist;

    playlist.setCreator( "Amarok" );
    playlist.setTitle( item->text(0) );

    XSPFtrackList list;

    Q3PtrList<TrackItemInfo> trackList = append ? item->droppedTracks() : item->trackList();
    for( TrackItemInfo *info = trackList.first(); info; info = trackList.next() )
    {
        XSPFtrack track;
        MetaBundle b( info->url() );
        track.creator  = b.artist();
        track.title    = b.title();
        track.location = b.url().url();
        list.append( track );
    }

    playlist.setTrackList( list, append );

    QFile file( item->url().path() );
    file.open( QIODevice::WriteOnly );

    QTextStream stream ( &file );

    playlist.save( stream, 2 );

    file.close();
}


void PlaylistBrowser::savePLS( PlaylistEntry *item, bool append )
{
    QFile file( item->url().path() );

    if( append ? file.open( QIODevice::WriteOnly | QIODevice::Append ) : file.open( QIODevice::WriteOnly ) )
    {
        QTextStream stream( &file );
        Q3PtrList<TrackItemInfo> trackList = append ? item->droppedTracks() : item->trackList();
        stream << "NumberOfEntries=" << trackList.count() << endl;
        int c=1;
        for( TrackItemInfo *info = trackList.first(); info; info = trackList.next(), ++c )
        {
            stream << "File" << c << "=";
            stream << (info->url().protocol() == "file" ? info->url().path() : info->url().url());
            stream << "\nTitle" << c << "=";
            stream << info->title();
            stream << "\nLength" << c << "=";
            stream << info->length();
            stream << "\n";
        }

        stream << "Version=2\n";
        file.close();
    }
}

#include <kdirlister.h>
#include <QEventLoop>
#include "playlistloader.h"
//this function (C) Copyright 2003-4 Max Howell, (C) Copyright 2004 Mark Kretschmann
KUrl::List PlaylistBrowser::recurse( const KUrl &url )
{
    typedef QMap<QString, KUrl> FileMap;

    KDirLister lister( false );
    lister.setAutoUpdate( false );
    lister.setAutoErrorHandlingEnabled( false, 0 );
    lister.openUrl( url );

    while( !lister.isFinished() )
        kapp->processEvents( QEventLoop::ExcludeUserInput );

    KFileItemList items = lister.items(); //returns QPtrList, so we MUST only do it once!
    KUrl::List urls;
    FileMap files;
    oldForeachType( KFileItemList, items ) {
        if( (*it)->isFile() ) { files[(*it)->name()] = (*it)->url(); continue; }
        if( (*it)->isDir() ) urls += recurse( (*it)->url() );
    }

    oldForeachType( FileMap, files )
        // users often have playlist files that reflect directories
        // higher up, or stuff in this directory. Don't add them as
        // it produces double entries
        if( !PlaylistFile::isPlaylistFile( (*it).fileName() ) )
            urls += *it;

    return urls;
}


void PlaylistBrowser::currentItemChanged( Q3ListViewItem *item )    //SLOT
{
    // rename remove and delete buttons are disabled if there are no playlists
    // rename and delete buttons are disabled for track items

    bool enable_remove = false;
    bool enable_rename = false;

    if ( !item )
        goto enable_buttons;

    if ( isCategory( item ) )
    {
        if( static_cast<PlaylistCategory*>(item)->isFolder() &&
            static_cast<PlaylistCategory*>(item)->isKept() )
            enable_remove = enable_rename = true;
    }
    else if ( isPodcastChannel( item ) )
    {
        enable_remove = true;
        enable_rename = false;
    }
    else if ( !isPodcastEpisode( item ) )
        enable_remove = enable_rename = static_cast<PlaylistCategory*>(item)->isKept();

    static_cast<PlaylistBrowserEntry*>(item)->updateInfo();

    enable_buttons:

    removeButton->setEnabled( enable_remove );
    renameButton->setEnabled( enable_rename );
}


void PlaylistBrowser::customEvent( QEvent *e )
{
    // If a playlist is found in collection folders it will be automatically added to the playlist browser
    // The ScanController sends a PlaylistFoundEvent when a playlist is found.

    ScanController::PlaylistFoundEvent* p = static_cast<ScanController::PlaylistFoundEvent*>( e );
    addPlaylist( p->path(), 0, false, true );
}


void PlaylistBrowser::slotAddMenu( int id ) //SLOT
{
    switch( id )
    {
        case STREAM:
            addStream();
            break;

        case SMARTPLAYLIST:
            addSmartPlaylist();
            break;

        case PODCAST:
            addPodcast();
            break;

        case ADDDYNAMIC:
            ConfigDynamic::dynamicDialog(this);
            break;
    }
}


void PlaylistBrowser::slotAddPlaylistMenu( int id ) //SLOT
{
    switch( id )
    {
        case PLAYLIST:
            createPlaylist( 0/*base cat*/, false/*make empty*/ );
            break;

        case PLAYLIST_IMPORT:
            openPlaylist();
            break;
    }
}


/**
 ************************
 *  Context Menu Entries
 ************************
 **/

void PlaylistBrowser::showContextMenu( Q3ListViewItem *item, const QPoint &p, int )  //SLOT
{
    if( !item ) return;

    PlaylistBrowserEntry *entry = dynamic_cast<PlaylistBrowserEntry*>( item );
    if ( entry )
        entry->showContextMenu( p );
}

/////////////////////////////////////////////////////////////////////////////
//    CLASS PlaylistBrowserView
////////////////////////////////////////////////////////////////////////////

PlaylistBrowserView::PlaylistBrowserView( QWidget *parent, const char *name )
    : K3ListView( parent )
    , m_marker( 0 )
{
    setObjectName( name );
    addColumn( i18n("Playlists") );

    setSelectionMode( Q3ListView::Extended );
    setResizeMode( Q3ListView::AllColumns );
    setShowSortIndicator( true );
    setRootIsDecorated( true );

    setDropVisualizer( true );    //the visualizer (a line marker) is drawn when dragging over tracks
    setDropHighlighter( true );   //and the highligther (a focus rect) is drawn when dragging over playlists
    setDropVisualizerWidth( 3 );
    setAcceptDrops( true );

    setTreeStepSize( 20 );

    connect( this, SIGNAL( mouseButtonPressed ( int, Q3ListViewItem *, const QPoint &, int ) ),
             this,   SLOT( mousePressed( int, Q3ListViewItem *, const QPoint &, int ) ) );

    //TODO moving tracks
    //connect( this, SIGNAL( moved(QListViewItem *, QListViewItem *, QListViewItem * )),
    //        this, SLOT( itemMoved(QListViewItem *, QListViewItem *, QListViewItem * )));
}

PlaylistBrowserView::~PlaylistBrowserView() { }

void PlaylistBrowserView::contentsDragEnterEvent( QDragEnterEvent *e )
{
    e->accept( e->source() == viewport() || K3URLDrag::canDecode( e ) );
}

void PlaylistBrowserView::contentsDragMoveEvent( QDragMoveEvent* e )
{
    //Get the closest item _before_ the cursor
    const QPoint p = contentsToViewport( e->pos() );
    Q3ListViewItem *item = itemAt( p );
    if( !item ) {
        eraseMarker();
        return;
    }

    //only for track items (for playlist items we draw the highlighter)
    if( isPlaylistTrackItem( item ) )
        item = item->itemAbove();

    if( item != m_marker )
    {
        eraseMarker();
        m_marker = item;
        viewportPaintEvent( 0 );
    }
}

void PlaylistBrowserView::contentsDragLeaveEvent( QDragLeaveEvent* )
{
     eraseMarker();
}


void PlaylistBrowserView::contentsDropEvent( QDropEvent *e )
{
    Q3ListViewItem *parent = 0;
    Q3ListViewItem *after;

    const QPoint p = contentsToViewport( e->pos() );
    Q3ListViewItem *item = itemAt( p );
    if( !item ) {
        eraseMarker();
        return;
    }

    if( !isPlaylist( item ) )
        findDrop( e->pos(), parent, after );

    eraseMarker();

    if( e->source() == this )
    {
        moveSelectedItems( item ); // D&D sucks, do it ourselves
    }
    else {
        KUrl::List decodedList;
        Q3ValueList<MetaBundle> bundles;
        if( K3URLDrag::decode( e, decodedList ) )
        {
            KUrl::List::ConstIterator it = decodedList.begin();
            MetaBundle first( *it );
            const QString album  = first.album();
            const QString artist = first.artist();

            int suggestion = !album.trimmed().isEmpty() ? 1 : !artist.trimmed().isEmpty() ? 2 : 3;

            for ( ; it != decodedList.end(); ++it )
            {
                if( isCategory(item) )
                { // check if it is podcast category
                    Q3ListViewItem *cat = item;
                    while( isCategory(cat) && cat!=PlaylistBrowser::instance()->podcastCategory() )
                        cat = cat->parent();

                    if( cat == PlaylistBrowser::instance()->podcastCategory() )
                        PlaylistBrowser::instance()->addPodcast(*it, item);
                    continue;
                }

                QString filename = (*it).fileName();

                if( filename.endsWith("m3u") || filename.endsWith("pls") )
                    PlaylistBrowser::instance()->addPlaylist( (*it).path() );
                else if( ContextBrowser::hasContextProtocol( *it ) )
                {
                    KUrl::List urls = ContextBrowser::expandURL( *it );
                    for( KUrl::List::iterator i = urls.begin();
                            i != urls.end();
                            i++ )
                    {
                        MetaBundle mb(*i);
                        bundles.append( mb );
                    }
                }
                else //TODO: check canDecode ?
                {
                    MetaBundle mb(*it);
                    bundles.append( mb );
                    if( suggestion == 1 && mb.album()->lower().trimmed() != album.toLower().trimmed() )
                        suggestion = 2;
                    if( suggestion == 2 && mb.artist()->lower().trimmed() != artist.toLower().trimmed() )
                        suggestion = 3;
                }
            }

            if( bundles.isEmpty() ) return;

            if( parent && isPlaylist( parent ) ) {
                //insert the dropped tracks
                PlaylistEntry *playlist = static_cast<PlaylistEntry *>( parent );
                playlist->insertTracks( after, bundles );
            }
            else //dropped on a playlist item
            {
                Q3ListViewItem *parent = item;
                bool isPlaylistFolder = false;

                while( parent )
                {
                    if( parent == PlaylistBrowser::instance()->m_playlistCategory )
                    {
                        isPlaylistFolder = true;
                        break;
                    }
                    parent = parent->parent();
                }

                if( isPlaylist( item ) ) {
                    PlaylistEntry *playlist = static_cast<PlaylistEntry *>( item );
                    //append the dropped tracks
                    playlist->insertTracks( 0, bundles );
                }
                else if( isCategory( item ) && isPlaylistFolder )
                {
                    PlaylistBrowser *pb = PlaylistBrowser::instance();
                    QString title = suggestion == 1 ? album
                                                  : suggestion == 2 ? artist
                                                  : QString();
                    if ( pb->createPlaylist( item, false, title ) )
                        pb->m_lastPlaylist->insertTracks( 0, bundles );
                }
            }
        }
        else
            e->ignore();
    }

}

void PlaylistBrowserView::eraseMarker() //SLOT
{
    if( m_marker )
    {
        QRect spot;
        if( isPlaylist( m_marker ) )
            spot = drawItemHighlighter( 0, m_marker );
        else
            spot = drawDropVisualizer( 0, 0, m_marker );

        m_marker = 0;
        viewport()->repaint( spot, false );
    }
}

void PlaylistBrowserView::viewportPaintEvent( QPaintEvent *e )
{
    if( e ) K3ListView::viewportPaintEvent( e ); //we call with 0 in contentsDropEvent()

    if( m_marker )
    {
        QPainter painter( viewport() );
        if( isPlaylist( m_marker ) )    //when dragging on a playlist we draw a focus rect
            drawItemHighlighter( &painter, m_marker );
        else //when dragging on a track we draw a line marker
            painter.fillRect( drawDropVisualizer( 0, 0, m_marker ),
                                   QBrush( colorGroup().highlight(), Qt::Dense4Pattern ) );
    }
}

void PlaylistBrowserView::mousePressed( int button, Q3ListViewItem *item, const QPoint &pnt, int )    //SLOT
{
    // this function expande/collapse the playlist if the +/- symbol has been pressed
    // and show the save menu if the save icon has been pressed

    if( !item || button != Qt::LeftButton ) return;

    if( isPlaylist( item ) )
    {
        QPoint p = mapFromGlobal( pnt );
        p.setY( p.y() - header()->height() );

        QRect itemrect = itemRect( item );

        QRect expandRect = QRect( 4, itemrect.y() + (item->height()/2) - 5, 15, 15 );
        if( expandRect.contains( p ) ) {    //expand symbol clicked
            setOpen( item, !item->isOpen() );
            return;
        }
    }
}

void PlaylistBrowserView::moveSelectedItems( Q3ListViewItem *newParent )
{
    if( !newParent || isDynamic( newParent ) || isPodcastChannel( newParent ) ||
         isSmartPlaylist( newParent ) || isPodcastEpisode( newParent ) )
        return;

    #define newParent static_cast<PlaylistBrowserEntry*>(newParent)
    if( !newParent->isKept() )
        return;
    #undef  newParent

    Q3PtrList<Q3ListViewItem> selected;
    Q3ListViewItemIterator it( this, Q3ListViewItemIterator::Selected );
    for( ; it.current(); ++it )
    {
        if( !(*it)->parent() ) //must be a base category we are draggin'
            continue;

        selected.append( *it );
    }

    Q3ListViewItem *after=0;
    for( Q3ListViewItem *item = selected.first(); item; item = selected.next() )
    {
        Q3ListViewItem *itemParent = item->parent();
        if( isPlaylistTrackItem( item ) )
        {
            if( isPlaylistTrackItem( newParent ) )
            {
                if( !after && newParent != newParent->parent()->firstChild() )
                    after = newParent->itemAbove();

                newParent = static_cast<PlaylistEntry*>(newParent->parent());
            }
            else if( !isPlaylist( newParent ) )
                continue;


            #define newParent static_cast<PlaylistEntry*>(newParent)
            newParent->insertTracks( after, KUrl::List( static_cast<PlaylistTrackItem*>(item)->url() ));
            #undef  newParent
            #define itemParent static_cast<PlaylistEntry*>(itemParent)
            itemParent->removeTrack( static_cast<PlaylistTrackItem*>(item) );
            #undef  itemParent
            continue;
        }
        else if( !isCategory( newParent ) )
            continue;

        Q3ListViewItem *base = newParent;
        while( base->parent() )
            base = base->parent();

        if( base == PlaylistBrowser::instance()->m_playlistCategory && isPlaylist( item )   ||
            base == PlaylistBrowser::instance()->m_streamsCategory && isStream( item )      ||
            base == PlaylistBrowser::instance()->m_smartCategory && isSmartPlaylist( item ) ||
            base == PlaylistBrowser::instance()->m_dynamicCategory && isDynamic( item )      )
        {
            itemParent->takeItem( item );
            newParent->insertItem( item );
            newParent->sortChildItems( 0, true );
        }
        else if( base == PlaylistBrowser::instance()->m_podcastCategory && isPodcastChannel( item ) )
        {
        #define item static_cast<PodcastChannel*>(item)
            item->setParent( static_cast<PlaylistCategory*>(newParent) );
        #undef  item
        }
    }
}

void PlaylistBrowserView::rename( Q3ListViewItem *item, int c )
{
    K3ListView::rename( item, c );

    QRect rect( itemRect( item ) );
    int fieldX = rect.x() + treeStepSize() + 2;
    int fieldW = rect.width() - treeStepSize() - 2;

    KLineEdit *renameEdit = renameLineEdit();
    renameEdit->setGeometry( fieldX, rect.y(), fieldW, rect.height() );
    renameEdit->show();
}

void PlaylistBrowserView::keyPressEvent( QKeyEvent *e )
{
    switch( e->key() ) {
         case Qt::Key_Space:    //load
            PlaylistBrowser::instance()->slotDoubleClicked( currentItem() );
            break;

        case Qt::ShiftModifier+Qt::Key_Delete:    //delete
        case Qt::Key_Delete:          //remove
            PlaylistBrowser::instance()->removeSelectedItems();
            break;

        case Qt::Key_F2:    //rename
            PlaylistBrowser::instance()->renameSelectedItem();
            break;

        default:
            K3ListView::keyPressEvent( e );
            break;
    }
}


void PlaylistBrowserView::startDrag()
{
    DEBUG_BLOCK

    KUrl::List urls;
    KUrl::List itemList; // this is for CollectionDB::createDragPixmap()
    KUrl::List podList;  // used to add podcast episodes of the same channel in reverse order (usability)
    PodcastEpisode *lastPodcastEpisode = 0; // keep track of the last podcastepisode we visited.
    K3MultipleDrag *drag = new K3MultipleDrag( this );

    Q3ListViewItemIterator it( this, Q3ListViewItemIterator::Selected );
    QString pixText;
    uint count = 0;

    for( ; it.current(); ++it )
    {
        if( !isPodcastEpisode( *it ) && !podList.isEmpty() )
        {   // we left the podcast channel, so append those items we iterated over
            urls += podList;
            podList.clear();
        }

        if( isPlaylist( *it ) )
        {
            urls     += static_cast<PlaylistEntry*>(*it)->url();
            itemList += static_cast<PlaylistEntry*>(*it)->url();
            pixText = (*it)->text(0);
        }

        else if( isStream( *it ) )
        {
            urls     += static_cast<StreamEntry*>(*it)->url();
            itemList += KUrl( "stream://" );
            pixText = (*it)->text(0);
        }

        else if( isLastFm( *it ) )
        {
            urls     += static_cast<LastFmEntry*>(*it)->url();
            itemList += static_cast<LastFmEntry*>(*it)->url();
            pixText = (*it)->text(0);
        }

        else if( isPodcastEpisode( *it ) )
        {
            if( (*it)->parent()->isSelected() ) continue;
            if( !podList.isEmpty() && lastPodcastEpisode && lastPodcastEpisode->Q3ListViewItem::parent() != (*it)->parent() )
            {   // we moved onto a new podcast channel
                urls += podList;
                podList.clear();
            }
            #define item static_cast<PodcastEpisode *>(*it)
            if( item->isOnDisk() )
            {
                podList.prepend( item->localUrl() );
                itemList += item->url();
            }
            else
            {
                podList.prepend( item->url() );
                itemList += item->url();
            }
            lastPodcastEpisode = item;
            pixText = (*it)->text(0);
            #undef item
        }
        else if( isPodcastChannel( *it ) )
        {
            #define item static_cast<PodcastChannel *>(*it)
            if( !item->isPolished() )
                 item->load();

            Q3ListViewItem *child = item->firstChild();
            KUrl::List tmp;
            // we add the podcasts in reverse, its much nicer to add them chronologically :)
            while( child )
            {
                PodcastEpisode *pe = static_cast<PodcastEpisode*>( child );
                if( pe->isOnDisk() )
                    tmp.prepend( pe->localUrl() );
                else
                    tmp.prepend( pe->url() );
                child = child->nextSibling();
            }
            urls += tmp;
            itemList += KUrl( item->url().url() );
            pixText = (*it)->text(0);
            #undef item
        }

        else if( isSmartPlaylist( *it ) )
        {
            SmartPlaylist *item = static_cast<SmartPlaylist*>( *it );

            if( !item->query().isEmpty() )
            {
                Q3TextDrag *textdrag = new Q3TextDrag( item->text(0) + '\n' + item->query(), 0 );
                textdrag->setSubtype( "amarok-sql" );
                drag->addDragObject( textdrag );
            }
            itemList += KUrl( QString("smartplaylist://%1").arg( item->text(0) ) );
            pixText = (*it)->text(0);
        }

        else if( isDynamic( *it ) )
        {
            DynamicEntry *item = static_cast<DynamicEntry*>( *it );

            // Serialize pointer to string
            const QString str = QString::number( reinterpret_cast<qulonglong>( item ) );

            Q3TextDrag *textdrag = new Q3TextDrag( str, 0 );
            textdrag->setSubtype( "dynamic" );
            drag->addDragObject( textdrag );
            itemList += KUrl( QString("dynamic://%1").arg( item->text(0) ) );
            pixText = (*it)->text(0);
        }

        else if( isPlaylistTrackItem( *it ) )
        {
            if( (*it)->parent()->isSelected() ) continue;
            urls     += static_cast<PlaylistTrackItem*>(*it)->url();
            itemList += static_cast<PlaylistTrackItem*>(*it)->url();
        }
        count++;
    }

    if( !podList.isEmpty() )
        urls += podList;

    if( count > 1 ) pixText.clear();

    drag->addDragObject( new K3URLDrag( urls, viewport() ) );
    drag->setPixmap( CollectionDB::createDragPixmap( itemList, pixText ),
                     QPoint( CollectionDB::DRAGPIXMAP_OFFSET_X, CollectionDB::DRAGPIXMAP_OFFSET_Y ) );
    drag->dragCopy();
}

/////////////////////////////////////////////////////////////////////////////
//    CLASS PlaylistDialog
////////////////////////////////////////////////////////////////////////////

QString PlaylistDialog::getSaveFileName( const QString &suggestion, bool proposeOverwriting ) //static
{
    PlaylistDialog dialog;
    if( !suggestion.isEmpty() )
    {
        QString path = Amarok::saveLocation("playlists/") + "%1" + ".m3u";
        if( QFileInfo( path.arg( suggestion ) ).exists() && !proposeOverwriting )
        {
            int n = 2;
            while( QFileInfo( path.arg( i18n( "%1 (%2)", suggestion, QString::number( n ) ) ) ).exists() )
                n++;
            dialog.edit->setText( i18n( "%1 (%2)", suggestion, QString::number( n ) ) );
        }
        else
          dialog.edit->setText( suggestion );
    }
    if( dialog.exec() == Accepted )
        return dialog.result;
    return QString();
}

PlaylistDialog::PlaylistDialog()
    : KDialog( PlaylistWindow::self() )
    , customChosen( false )
{
    setCaption( i18n( "Save Playlist" ) );
    setModal( true );
    setButtons( Ok | Cancel | User1 );
    setDefaultButton( Ok );
    showButtonSeparator( false );
    setButtonGuiItem( User1, KGuiItem( i18n( "Save to location..." ) ) ); //KIcon( Amarok::icon( "files" ) )


    KVBox *vbox = new KVBox( this );
    setMainWidget( vbox );

    QLabel *label = new QLabel( i18n( "&Enter a name for the playlist:" ), vbox );
    edit = new KLineEdit( vbox );
    edit->setFocus();
    label->setBuddy( edit );
    enableButtonOk( false );
    connect( edit, SIGNAL( textChanged( const QString & ) ),
             this, SLOT( slotTextChanged( const QString& ) ) );
    connect( this, SIGNAL( user1Clicked() ), SLOT( slotCustomPath() ) );
    connect(this,SIGNAL(okClicked()),this,SLOT(slotOk()));
}

void PlaylistDialog::slotOk()
{
    // TODO Remove this hack for 1.2. It's needed because playlists was a file once.
    QString folder = Amarok::saveLocation( "playlists" );
    QFileInfo info( folder );
    if ( !info.isDir() ) QFile::remove( folder );

    if( !customChosen && !edit->text().isEmpty() )
        result = Amarok::saveLocation( "playlists/" ) + edit->text() + ".m3u";

    if( !QFileInfo( result ).exists() ||
        KMessageBox::warningContinueCancel(
            PlaylistWindow::self(),
            i18n( "A playlist named \"%1\" already exists. Do you want to overwrite it?", edit->text() ),
            i18n( "Overwrite Playlist?" ), KGuiItem( i18n( "Overwrite" ) ) ) == KMessageBox::Continue )
    {
        //KDialog::slotOk();
        slotButtonClicked( Ok );
    }
}

void PlaylistDialog::slotTextChanged( const QString &s )
{
    enableButtonOk( !s.isEmpty() );
}

void PlaylistDialog::slotCustomPath()
{
   result = KFileDialog::getSaveFileName( KUrl( "kfiledialog:///saveplaylists" ), "*.m3u" );
   if( !result.isNull() )
   {
      edit->setText( result );
      edit->setReadOnly( true );
      enableButtonOk( true );
      customChosen = true;
   }
}


InfoPane::InfoPane( QWidget *parent )
        : KVBox( parent ),
          m_enable( false ),
          m_storedHeight( 100 )
{
    KVBox *container = new KVBox( this );
    container->setObjectName( "container" );
    container->hide();

    {
        KHBox  *box = new KHBox( container );
        //box->setMargin( 3 );
        box->setBackgroundMode( Qt::PaletteBase );

        m_infoBrowser = new HTMLView( box, "extended_info" );

        container->setFrameStyle( QFrame::StyledPanel );
        container->setMargin( 3 );
        container->setBackgroundMode( Qt::PaletteBase );
    }

    m_pushButton = new KPushButton( KGuiItem( i18n("&Show Extended Info"), "info" ), this );
    m_pushButton->setToggleButton( true );
    m_pushButton->setEnabled( m_enable );
    connect( m_pushButton, SIGNAL(toggled( bool )), SLOT(toggle( bool )) );

    //Set the height to fixed. The button shouldn't be resized.
    setFixedHeight( m_pushButton->sizeHint().height() );
}

InfoPane::~InfoPane()
{
    // Ensure the KHTMLPart dies before its KHTMLView dies,
    // because KHTMLPart's dtoring relies on its KHTMLView still being alive
    // (see bug 130494).
    delete m_infoBrowser;
}

int
InfoPane::getHeight()
{
    if( findChild<QWidget*>( "container" )->isShown() )
    {
        //If the InfoPane is shown, return true height.
        return static_cast<QSplitter*>( parentWidget() )->sizes().last();
    }

    return m_storedHeight;
}

void
InfoPane::setStoredHeight( const int newHeight ) {
    m_storedHeight = newHeight;
}

void
InfoPane::toggle( bool toggled )
{
    QSplitter *splitter = static_cast<QSplitter*>( parentWidget() );

    if ( !toggled )
    {
        //Save the height for later
        setStoredHeight( splitter->sizes().last() );

        //Set the height to fixed. The button shouldn't be resized.
        setFixedHeight( m_pushButton->sizeHint().height() );

        //Now the info pane is not shown, we can disable the button if necessary
        m_pushButton->setEnabled( m_enable );
    }
    else {
        setMaximumHeight( ( int )( parentWidget()->height() / 1.5 ) );

        //Restore the height of the InfoPane (change the splitter properties)
        //Done every time since the pane forgets its height if you try to resize it while the info is hidden.
        Q3ValueList<int> sizes = splitter->sizes();
        const int sizeOffset = getHeight() - sizes.last();
        sizes.first() -= sizeOffset;
        sizes.last() += sizeOffset;
        splitter->setSizes( sizes );

        setMinimumHeight( 150 );
    }

    findChild<QWidget*>( "container" )->setShown( toggled );
}

void
InfoPane::setInfo( const QString &title, const QString &info )
{
    //If the info pane is not shown, we can enable or disable the button depending on
    //whether there is content to show. Otherwise, just remember what we wanted to do
    //so we can do it later, when the user does hide the pane.
    m_enable = !( info.isEmpty() && title.isEmpty() );
    if ( !findChild<QWidget*>("container")->isShown() )
        m_pushButton->setEnabled( m_enable );

    if( m_pushButton->isOn() )
        toggle( !(info.isEmpty() && title.isEmpty()) );

    m_infoBrowser->set(
        m_enable ?
        QString( "<div id='extended_box' class='box'>"
                  "<div id='extended_box-header-title' class='box-header'>"
                  "<span id='extended_box-header-title' class='box-header-title'>"
                  " %1 "
                  "</span>"
                  "</div>"
                  "<table id='extended_box-table' class='box-body' width='100%' cellpadding='0' cellspacing='0'>"
                  "<tr>"
                  "<td id='extended_box-information-td'>"
                  "  %2 "
                  "</td>"
                  "</tr>"
                  "</table>"
                  "</div>" ).arg( title, info ) :
        QString() );
}

#include "playlistbrowser.moc"
