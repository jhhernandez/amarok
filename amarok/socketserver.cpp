// Maintainer: Max Howell <max.howell@methylblue.com>, (C) 2004
// Copyright: See COPYING file that comes with this distribution


#include "config.h" //XMMS_CONFIG_DIR

#include "enginebase.h"       //to get the scope
#include "enginecontroller.h" //to get the engine
#include "fht.h"              //processing the scope
#include "socketserver.h"

#include <kdebug.h>
#include <klocale.h>
#include <kprocess.h> //Vis::Selector
#include <kstandarddirs.h>
#include <qsocketnotifier.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>

#include <dirent.h> //FIXME use QDir
#include <sys/stat.h>


QGuardedPtr<Vis::Selector> Vis::Selector::m_instance;


//TODO allow stop/start and pause signals to be sent to registered visualisations
//TODO see if we need two socket servers
//TODO allow transmission of visual data back to us here and allow that to be embedded in stuff
//TODO decide whether to use 16 bit integers or 32 bit floats as data sent to analyzers
//     remember that there may be 1024 operations on these data points up to 50 times every second!
//TODO consider moving fht.* here
//TODO allow visualisations to determine their own data sizes


Vis::SocketServer::SocketServer( QObject *parent )
  : QServerSocket( parent )
{
    m_sockfd = ::socket( AF_UNIX, SOCK_STREAM, 0 );

    if ( m_sockfd == -1 )
    {
        kdWarning() << k_funcinfo << " socket() error\n";
        return;
    }

    sockaddr_un local;
    local.sun_family = AF_UNIX;
    QCString path = ::locateLocal( "socket", QString( "amarok.visualization_socket" ) ).local8Bit();
    ::strcpy( &local.sun_path[0], path );
    ::unlink( path );

    if ( ::bind( m_sockfd, (struct sockaddr*) &local, sizeof( local ) ) == -1 )
    {
        kdWarning() << k_funcinfo << " bind() error\n";
        ::close ( m_sockfd );
        m_sockfd = -1;
        return;
    }
    if ( ::listen( m_sockfd, 1 ) == -1 )
    {
        kdWarning() << k_funcinfo << " listen() error\n";
        ::close ( m_sockfd );
        m_sockfd = -1;
        return;
    }

    this->setSocket( m_sockfd );
}


/////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC interface
/////////////////////////////////////////////////////////////////////////////////////////

void
Vis::SocketServer::newConnection( int sockfd )
{
    kdDebug() << "[Vis::Server] Connection requested: " << sockfd << endl;

    QSocketNotifier *sn = new QSocketNotifier( sockfd, QSocketNotifier::Read, this );

    connect( sn, SIGNAL(activated( int )), SLOT(request( int )) );
}


/////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE interface
/////////////////////////////////////////////////////////////////////////////////////////

void
Vis::SocketServer::request( int sockfd )
{
    std::vector<float> *scope = EngineController::engine()->scope();

    char buf[32]; //TODO docs should state requests can only be 32bytes at most
    int nbytes = recv( sockfd, buf, sizeof(buf) - 1, 0 );

    if( nbytes > 0 )
    {
        buf[nbytes] = '\000';
        QString result( buf );

        if( result == "PCM" )
        {
            if( scope->empty() ) kdDebug() << "empty scope!\n";
            if( scope->size() < 512 ) kdDebug() << "scope too small!\n";

            float data[512]; for( uint x = 0; x < 512; ++x ) data[x] = (*scope)[x];

            ::send( sockfd, data, 512*sizeof(float), 0 );

            delete scope;
        }
        else if( result == "FFT" )
        {
            FHT fht( 9 ); //data set size 512

            {
                static float max = -100;
                static float min = 100;

                bool b = false;

                for( uint x = 0; x < scope->size(); ++x )
                {
                    float val = (*scope)[x];
                    if( val > max ) { max = val; b = true; }
                    if( val < min ) { min = val; b = true; }
                }

                if( b ) kdDebug() << "max: " << max << ", min: " << min << endl;
            }

            float *front = static_cast<float*>( &scope->front() );

            fht.spectrum( front );
            fht.scale( front, 1.0 / 64 );

            //only half the samples from the fft are useful

            ::send( sockfd, scope, 256*sizeof(float), 0 );

            delete scope;
        }
        else if( result.startsWith( "REG", false ) )
        {

        }

    } else {

        kdDebug() << "[Vis::Server] recv() error, closing socket" << endl;
        ::close( sockfd );
    }
}


/////////////////////////////////////////////////////////////////////////////////////////
// CLASS Vis::Selector
/////////////////////////////////////////////////////////////////////////////////////////

Vis::Selector::Selector()
  : KListView()
{
    //TODO we will have to update the status of the visualisation window using the socket
    //     it should know which processes are requesting data from it
    //FIXME problem, you can have more than one of each vis running!
    //      solution (for now) data starve multiple registrants

    //TODO for now we keep the widget around as this will keep the checkboxes set as the user expects
    //     it isn't a perfect system, but it will suffice
    //setWFlags( Qt::WDestructiveClose ); //FIXME these are the defaults no?

    setFullWidth( true );
    setShowSortIndicator( true );
    setSorting( 0 );
    setCaption( i18n( "Visualizations - amaroK" ) );
    addColumn( i18n( "Name" ) );
    resize( 250, 250 );
    
    
    QString dirname = XMMS_PLUGIN_PATH;
    dirname.append( "/" );
    QString filepath;
    DIR *dir;
    struct dirent *ent;
    struct stat statbuf;

    dir = opendir( dirname.local8Bit() );//TODO use QDir, it's just better to do that

    while( (ent = readdir( dir )) )
    {
        QString filename = QString::fromLocal8Bit( ent->d_name );
        
        filepath = dirname + filename;
        
        if( filename.endsWith( ".so" ) &&
            !stat( filepath.local8Bit(), &statbuf ) &&
            S_ISREG( statbuf.st_mode ) )
        {
            new Selector::Item( this, filename );
        }
    }
    
    closedir( dir );
}

void
Vis::Selector::processExited( KProcess *proc )
{
    for( Item *item = (Item*)firstChild(); item; item = (Item*)item->nextSibling() )
    {
        if( item->m_proc == proc ) item->setOn( false ); //will delete m_proc via stateChange( bool )
    }
    
    kdDebug() << "done\n";
}

Vis::Selector::Item::~Item()
{
    //if( m_proc ) m_proc->kill(); //NOTE makes no difference to kill speed
    delete m_proc; //kills the process too
}

void
Vis::Selector::Item::stateChange( bool ) //SLOT
{
    //TODO was !m_ignoreState sillyness here, why!?

    switch( state() ) {
    case On:
        m_proc = new KProcess();
        *m_proc << KStandardDirs::findExe( "amarok_xmmswrapper" ) << text( 0 );

        connect( m_proc, SIGNAL(processExited( KProcess* )), (Selector*)listView(), SLOT(processExited( KProcess* )) );

        kdDebug() << "[Vis::Selector] Starting XMMS visualization..\n";

        if( m_proc->start() ) break;

        //ELSE FALL_THROUGH

        kdWarning() << "[Vis::Selector] Could not start amarok_xmmswrapper!\n";

    case Off:
        kdDebug() << "[Vis::Selector] Stopping XMMS visualization\n";        
            
        //m_proc->kill(); no point, will be done by delete, and crashes amaroK in some cases
        delete m_proc;
        m_proc = 0;
        
        break;
    
    default:
        break;
    }
}


#include "socketserver.moc"
