/***************************************************************************
 *   Copyright (c) 2008  Nikolaj Hald Nielsen <nhnFreespirit@gmail.com>    *
 *             (c) 2009  Teo Mrnjavac <teo.mrnjavac@gmail.com>             *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
 ***************************************************************************/
 
#include "LayoutConfigAction.h"

#include "Debug.h"
#include "LayoutManager.h"
#include "dialogs/PlaylistLayoutEditDialog.h"
#include "widgets/EditDeleteDelegate.h"
#include "widgets/EditDeleteComboBoxView.h"
#include "MainWindow.h"

#include <QLabel>
#include <QComboBox>

namespace Playlist
{

LayoutConfigAction::LayoutConfigAction( QWidget * parent )
    : KAction( parent )
    , m_playlistEditDialog( 0 )
{
    KIcon actionIcon( "configure" );    //TEMPORARY ICON
    setIcon( actionIcon );
    m_layoutMenu = new KMenu( parent );
    setMenu( m_layoutMenu );
    m_configAction = new KAction( m_layoutMenu );
    m_layoutMenu->addSeparator();
    
    m_layoutMenu->addAction( m_configAction );
    m_layoutActions = new QActionGroup( m_layoutMenu );
    m_layoutActions->setExclusive( TRUE );

    QStringList layoutsList( LayoutManager::instance()->layouts() );
    foreach( QString iterator, layoutsList )
    {
        m_layoutActions->addAction( iterator )->setCheckable( TRUE );
    }
    m_layoutMenu->addActions( m_layoutActions->actions() );
    int index = LayoutManager::instance()->layouts().indexOf( LayoutManager::instance()->activeLayoutName() );
    m_layoutActions->actions()[ index ]->setChecked( TRUE );

    connect( m_layoutActions, SIGNAL( triggered( QAction * ) ), this, SLOT( setActiveLayout( QAction * ) ) );

    connect( LayoutManager::instance(), SIGNAL( layoutListChanged() ), this, SLOT( layoutListChanged() ) );
    connect( LayoutManager::instance(), SIGNAL( activeLayoutChanged() ), this, SLOT( onActiveLayoutChanged() ) );

    const KIcon configIcon( "configure" );
    m_configAction->setIcon( configIcon );
    m_configAction->setText( i18n( "Configure playlist layouts..." ) );

    connect( m_configAction, SIGNAL( triggered() ), this, SLOT( configureLayouts() ) );
}


LayoutConfigAction::~LayoutConfigAction()
{
}

void LayoutConfigAction::setActiveLayout( QAction *layoutAction )
{
    debug() << "About to set layout " << layoutAction->text();
    QString layoutName( layoutAction->text() );
    layoutName = layoutName.remove( QChar( '&' ) );        //need to remove the & from the string, used for the shortcut key underscore
    LayoutManager::instance()->setActiveLayout( layoutName );
}

void LayoutConfigAction::configureLayouts()
{
    if ( !m_playlistEditDialog )
        m_playlistEditDialog = new PlaylistLayoutEditDialog( The::mainWindow() );
    m_playlistEditDialog->show();
}

void Playlist::LayoutConfigAction::layoutListChanged()
{
    m_layoutActions->actions().clear();
    QStringList layoutsList( LayoutManager::instance()->layouts() );
    foreach( QString iterator, layoutsList )
    {
        m_layoutActions->addAction( iterator );
    }
}

void LayoutConfigAction::onActiveLayoutChanged()
{
    DEBUG_BLOCK
    QString layoutName( LayoutManager::instance()->activeLayoutName() );
    layoutName = layoutName.remove( QChar( '&' ) );        //need to remove the & from the string, used for the shortcut key underscore
    int index = LayoutManager::instance()->layouts().indexOf( layoutName );
    debug() << "Index in the LayoutManager of currently active layout, called " << LayoutManager::instance()->activeLayoutName() << ", is: " << index;
    if( m_layoutActions->actions()[ index ] != m_layoutActions->checkedAction() )
        m_layoutActions->actions()[ index ]->setChecked( TRUE );
//     QString layout = LayoutManager::instance()->activeLayoutName();
//     if( layout != m_layoutActions->checkedAction()->text() )
//        m_layoutActions->actions()[ m_layoutActions->actions().indexOf( layout ) ]->setChecked( TRUE ); //WTF doesn't work
}


}

#include "LayoutConfigAction.moc"
