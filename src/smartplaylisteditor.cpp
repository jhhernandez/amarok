/// (c) Pierpaolo Di Panfilo 2004
// (c) Alexandre Pereira de Oliveira 2005
// (c) 2005 Isaiah Damron <xepo@trifault.net>
// (c) 2006 Peter C. Ndikuwera <pndiku@gmail.com>
// See COPYING file for licensing information

#define DEBUG_PREFIX "SmartPlaylistEditor"

#include "amarok.h" //foreach
#include "debug.h"
#include "collectiondb.h"
#include "metabundle.h"
#include "smartplaylisteditor.h"

#include <kcombobox.h>
#include <klineedit.h>
#include <klocale.h>
#include <knuminput.h>

#include <qcheckbox.h>
#include <qdatetime.h>
#include <qdatetimeedit.h>    //loadEditWidgets()
#include <qframe.h>
#include <qhbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qobjectlist.h>
#include <qstringlist.h>
#include <qtoolbutton.h>
#include <qvbox.h>
#include <qvgroupbox.h>


QStringList m_fields;
QStringList m_dbFields;
QStringList m_expandableFields;
QStringList m_expandableDbFields;



SmartPlaylistEditor::SmartPlaylistEditor( QString defaultName, QWidget *parent, const char *name )
    : KDialogBase( parent, name, true, i18n("Create Smart Playlist"),
      Ok|Cancel, Ok, true )
{
    init(defaultName);
    addCriteriaAny();
    addCriteriaAll();
}


SmartPlaylistEditor::SmartPlaylistEditor( QWidget *parent, QDomElement xml, const char *name)
    : KDialogBase( parent, name, true, i18n("Edit Smart Playlist"),
      Ok|Cancel, Ok, true )
{
    init( xml.attribute( "name" ) );
    // matches
    QDomNodeList matchesList =  xml.elementsByTagName( "matches" );
    bool matchedANY = false, matchedALL = false;
    
    m_matchAllCheck->setChecked( true );
    m_matchAnyCheck->setChecked( true );

    for (int i = 0, m = matchesList.count(); i<m; i++) {
        QDomElement matches = matchesList.item(i).toElement();
        QDomNodeList criteriaList =  matches.elementsByTagName( "criteria" );
        
        if ( criteriaList.count() ) {
            for (int j = 0, c=criteriaList.count() ; j<c; ++j ) {
                QDomElement criteria = criteriaList.item(j).toElement();
              
                if (matches.attribute( "glue" ) == "OR") {
                    matchedANY = true;
                    addCriteriaAny( criteria );
                }
                else {
                    matchedALL = true;
                    addCriteriaAll( criteria );
                }
            }
        }
    }

    if ( !matchedALL ) {
        addCriteriaAll();
        m_matchAllCheck->setChecked( false );
    }

    if ( !matchedANY ) {
        m_matchAnyCheck->setChecked( false );
        addCriteriaAny( );
    }

    // orderby
    QDomNodeList orderbyList =  xml.elementsByTagName( "orderby" );
    if ( orderbyList.count() ) {
        m_orderCheck->setChecked( true );
        QDomElement orderby = orderbyList.item(0).toElement(); // we only allow one orderby node

        //random is always the last one.
        int dbfield = orderby.attribute( "field" ) == "random" ? m_dbFields.count()-1 : m_dbFields.findIndex( orderby.attribute( "field" ) );

        m_orderCombo->setCurrentItem( dbfield );
        updateOrderTypes( dbfield );
        if ( orderby.attribute( "order" ) == "DESC" || orderby.attribute( "order" ) == "weighted" )
            m_orderTypeCombo->setCurrentItem( 1 );
        else
            m_orderTypeCombo->setCurrentItem( 0 );
    }
    // limit
    if  ( xml.hasAttribute( "maxresults" ) ) {
        m_limitCheck->setChecked( true );
        m_limitSpin->setValue( xml.attribute( "maxresults" ).toInt() );
    }

    // expand by
    QDomNodeList expandbyList =  xml.elementsByTagName( "expandby" );
    if ( expandbyList.count() ) {
        m_expandCheck->setChecked( true );
        QDomElement expandby = expandbyList.item(0).toElement(); // we only allow one orderby node

        int dbfield = m_expandableFields.findIndex( expandby.attribute( "field" ) );
        m_expandCombo->setCurrentItem( dbfield );
    }
}


void SmartPlaylistEditor::init(QString defaultName)
{
    makeVBoxMainWidget();

    m_fields.clear();
    m_fields << i18n("Artist") << i18n("Album") << i18n("Genre") << i18n("Title") << i18n("Length") << i18n("Track #") << i18n("Year")
             << i18n("Comment") << i18n("Play Counter") << i18n("Score") << i18n( "Rating" ) << i18n("First Play") << i18n("Last Play")
             << i18n("Modified Date") << i18n("File Path");

    m_dbFields.clear();
    m_dbFields << "artist.name" << "album.name" << "genre.name" << "tags.title" << "tags.length"
               << "tags.track" << "year.name" << "tags.comment" << "statistics.playcounter"
               << "statistics.percentage" << "statistics.rating" << "statistics.createdate"
               << "statistics.accessdate" << "tags.createdate" << "tags.url" << "tags.sampler";

    m_expandableFields.clear();
    m_expandableFields << i18n("Artist") << i18n("Album") << i18n("Genre") <<  i18n("Year");

    m_expandableDbFields.clear();
    m_expandableDbFields << "artist.name" << "album.name" << "genre.name" << "year.name";

    QHBox *hbox = new QHBox( mainWidget() );
    hbox->setSpacing( 5 );
    new QLabel( i18n("Playlist name:"), hbox );
    m_nameLineEdit = new KLineEdit( defaultName, hbox );

    QFrame *sep = new QFrame( mainWidget() );
    sep->setFrameStyle( QFrame::HLine | QFrame::Sunken );

    //match box (any)
    QHBox *matchAnyBox = new QHBox( mainWidget() );
    m_matchAnyCheck = new QCheckBox( i18n("Match Any of the following conditions" ), matchAnyBox );
    matchAnyBox->setStretchFactor( new QWidget( matchAnyBox ), 1 );

    //criteria box
    m_criteriaAnyGroupBox = new QVGroupBox( QString::null, mainWidget() );

    //match box (all)
    QHBox *matchAllBox = new QHBox( mainWidget() );
    m_matchAllCheck = new QCheckBox( i18n("Match All of the following conditions" ), matchAllBox );
    matchAllBox->setStretchFactor( new QWidget( matchAllBox ), 1 );

    //criteria box
    m_criteriaAllGroupBox = new QVGroupBox( QString::null, mainWidget() );

    //order box
    QHBox *hbox2 = new QHBox( mainWidget() );
    m_orderCheck = new QCheckBox( i18n("Order by"), hbox2 );
    QHBox *orderBox = new QHBox( hbox2 );
    orderBox->setSpacing( 5 );
    //fields combo
    m_orderCombo = new KComboBox( orderBox );
    m_orderCombo->insertStringList( m_fields );
    m_orderCombo->insertItem( i18n("Random") );
    //order type
    m_orderTypeCombo = new KComboBox( orderBox );
    updateOrderTypes(0); // populate the new m_orderTypeCombo
    hbox2->setStretchFactor( new QWidget( hbox2 ), 1 );

    //limit box
    QHBox *hbox1 = new QHBox( mainWidget() );
    m_limitCheck = new QCheckBox( i18n("Limit to"), hbox1 );
    QHBox *limitBox = new QHBox( hbox1 );
    limitBox->setSpacing( 5 );
    m_limitSpin = new KIntSpinBox( limitBox );
    m_limitSpin->setMinValue( 1 );
    m_limitSpin->setMaxValue( 1000 );
    m_limitSpin->setValue( 15 );
    new QLabel( i18n("tracks"), limitBox );
    hbox1->setStretchFactor( new QWidget( hbox1 ), 1 );

    //Expand By
    QHBox *hbox3 = new QHBox( mainWidget() );
    m_expandCheck = new QCheckBox( i18n("Expand by"), hbox3 );
    QHBox *expandBox = new QHBox( hbox3 );
    expandBox->setSpacing( 5 );
    m_expandCombo = new KComboBox( expandBox );
    m_expandCombo->insertStringList( m_expandableFields );
    hbox3->setStretchFactor( new QWidget( hbox3 ), 1 );

    //add stretch
    static_cast<QHBox *>(mainWidget())->setStretchFactor(new QWidget(mainWidget()), 1);

    connect( m_matchAnyCheck, SIGNAL( toggled(bool) ), m_criteriaAnyGroupBox, SLOT( setEnabled(bool) ) );
    connect( m_matchAllCheck, SIGNAL( toggled(bool) ), m_criteriaAllGroupBox, SLOT( setEnabled(bool) ) );
    connect( m_orderCheck, SIGNAL( toggled(bool) ), orderBox, SLOT( setEnabled(bool) ) );
    connect( m_limitCheck, SIGNAL( toggled(bool) ), limitBox, SLOT(  setEnabled(bool) ) );
    connect( m_expandCheck, SIGNAL( toggled(bool) ), expandBox, SLOT( setEnabled(bool) ) );
    connect( m_orderCombo, SIGNAL( activated(int) ), this, SLOT( updateOrderTypes(int) ) );

    m_criteriaAnyGroupBox->setEnabled( false );
    m_criteriaAllGroupBox->setEnabled( false );

    orderBox->setEnabled( false );
    limitBox->setEnabled( false );
    expandBox->setEnabled( false );

    m_nameLineEdit->setFocus();

    resize( 550, 200 );
}


void SmartPlaylistEditor::addCriteriaAny()
{
    CriteriaEditor *criteria= new CriteriaEditor( this, m_criteriaAnyGroupBox, criteriaAny );
    m_criteriaEditorAnyList.append( criteria );
    m_criteriaEditorAnyList.first()->enableRemove( m_criteriaEditorAnyList.count() > 1 );
}

void SmartPlaylistEditor::addCriteriaAll()
{
    CriteriaEditor *criteria= new CriteriaEditor( this, m_criteriaAllGroupBox, criteriaAll );
    m_criteriaEditorAllList.append( criteria );
    m_criteriaEditorAllList.first()->enableRemove( m_criteriaEditorAllList.count() > 1 );
}

void SmartPlaylistEditor::addCriteriaAny( QDomElement &xml )
{
    CriteriaEditor *criteria = new CriteriaEditor( this, m_criteriaAnyGroupBox, criteriaAny, xml );
    m_criteriaEditorAnyList.append( criteria );
    m_criteriaEditorAnyList.first()->enableRemove( m_criteriaEditorAnyList.count() > 1 );
}

void SmartPlaylistEditor::addCriteriaAll( QDomElement &xml )
{
    CriteriaEditor *criteria = new CriteriaEditor( this, m_criteriaAllGroupBox, criteriaAll, xml );
    m_criteriaEditorAllList.append( criteria );
    m_criteriaEditorAllList.first()->enableRemove( m_criteriaEditorAllList.count() > 1 );
}

void SmartPlaylistEditor::removeCriteriaAny( CriteriaEditor *criteria )
{
    m_criteriaEditorAnyList.remove( criteria );
    criteria->deleteLater();
    resize( size().width(), sizeHint().height() );
    
    if( m_criteriaEditorAnyList.count() == 1 )
	m_criteriaEditorAnyList.first()->enableRemove( false );
}

void SmartPlaylistEditor::removeCriteriaAll( CriteriaEditor *criteria )
{
    m_criteriaEditorAllList.remove( criteria );
    criteria->deleteLater();
    resize( size().width(), sizeHint().height() );
    
    if( m_criteriaEditorAllList.count() == 1 )
	m_criteriaEditorAllList.first()->enableRemove( false );
}

void SmartPlaylistEditor::updateOrderTypes( int index )
{
    int currentOrderType = m_orderTypeCombo->currentItem();
    if( index == m_orderCombo->count()-1 ) {  // random order selected
        m_orderTypeCombo->clear();
        m_orderTypeCombo->insertItem( i18n("Completely Random") );
        m_orderTypeCombo->insertItem( i18n("Score Weighted") );
    }
    else {  // ordinary order column selected
        m_orderTypeCombo->clear();
        m_orderTypeCombo->insertItem( i18n("Ascending") );
        m_orderTypeCombo->insertItem( i18n("Descending") );
    }
    if( currentOrderType < m_orderTypeCombo->count() )
        m_orderTypeCombo->setCurrentItem( currentOrderType );
    m_orderTypeCombo->setFont(m_orderTypeCombo->font());  // invalidate size hint
    m_orderTypeCombo->updateGeometry();
}

QDomElement SmartPlaylistEditor::result() {
    QDomDocument doc;
    QDomNode node = doc.namedItem( "smartplaylists" );
    QDomElement nodeE;
    nodeE = node.toElement();

    QDomElement smartplaylist = doc.createElement( "smartplaylist" );

    smartplaylist.setAttribute( "name", name() );

    // Limit
    if ( m_limitCheck->isChecked() )
        smartplaylist.setAttribute( "maxresults", m_limitSpin->value() );

    nodeE.appendChild( smartplaylist );
    // Matches
    if( m_matchAnyCheck->isChecked() ) {
        QDomElement matches = doc.createElement("matches");
        smartplaylist.appendChild( matches );
        // Iterate through all criteria list
        CriteriaEditor *criteriaeditor = m_criteriaEditorAnyList.first();
        for( int i=0; criteriaeditor; criteriaeditor = m_criteriaEditorAnyList.next(), ++i ) {
            matches.appendChild( doc.importNode( criteriaeditor->getDomSearchCriteria( doc ), true ) );
        }
        matches.setAttribute( "glue",  "OR" );
        smartplaylist.appendChild( matches );
    }

    if( m_matchAllCheck->isChecked() ) {
        QDomElement matches = doc.createElement("matches");
        smartplaylist.appendChild( matches );
        // Iterate through all criteria list
        CriteriaEditor *criteriaeditor = m_criteriaEditorAllList.first();
        for( int i=0; criteriaeditor; criteriaeditor = m_criteriaEditorAllList.next(), ++i ) {
            matches.appendChild( doc.importNode( criteriaeditor->getDomSearchCriteria( doc ), true ) );
        }
        matches.setAttribute( "glue",  "AND" );
        smartplaylist.appendChild( matches );
    }
    
    // Order By
    if( m_orderCheck->isChecked() ) {
        QDomElement orderby = doc.createElement("orderby");
        if (m_orderCombo->currentItem() != m_orderCombo->count()-1) {
            orderby.setAttribute( "field", m_dbFields[ m_orderCombo->currentItem() ] );
            orderby.setAttribute( "order", m_orderTypeCombo->currentItem() == 1 ? "DESC" : "ASC" );
        } else {
            orderby.setAttribute( "field", "random" );
            orderby.setAttribute( "order", m_orderTypeCombo->currentItem() == 1 ? "weighted" : "random" );
        }

        smartplaylist.appendChild( orderby );
    }
    QDomElement Sql = doc.createElement("sqlquery");
    buildQuery();
    Sql.appendChild( doc.createTextNode( m_query ) );
    smartplaylist.appendChild( Sql );

    if( m_expandCheck->isChecked() ) {
        QDomElement expandBy = doc.createElement("expandby");
        expandBy.setAttribute( "field", m_expandableFields[ m_expandCombo->currentItem() ] );
        QDomText t = doc.createTextNode( m_expandQuery );
        expandBy.appendChild( t );
        smartplaylist.appendChild( expandBy );
    }
    return (smartplaylist);
}


void SmartPlaylistEditor::buildQuery()
{
    DEBUG_BLOCK

    QString joins = "tags LEFT JOIN year ON year.id=tags.year LEFT JOIN genre ON genre.id=tags.genre"
                    " LEFT JOIN artist ON artist.id=tags.artist LEFT JOIN album ON album.id=tags.album";
    QString whereStr;
    QString criteriaListStr;
    QString orderStr;
    QString limitStr;
    
    //where expression
    if( m_matchAnyCheck->isChecked() || m_matchAllCheck->isChecked() ) {
        int i = 0;
        
        if( m_matchAnyCheck->isChecked() ) {
            criteriaListStr += "( (";
            
            CriteriaEditor *criteria = m_criteriaEditorAnyList.first();
            for( i=0; criteria; criteria = m_criteriaEditorAnyList.next(), i++ ) {
                
                QString str = criteria->getSearchCriteria();
                if( str.contains( "statistics." ) && !joins.contains( "statistics" ) )
                    joins += " LEFT JOIN statistics ON statistics.url=tags.url";
                
                if( i ) //multiple conditions
                    str.prepend( " OR (");
                
                criteriaListStr += str+")";
            }
            
            criteriaListStr += " )"; // we want our ORs all in one bracket. :-)
        }
        
        if( m_matchAllCheck->isChecked() ) {
            if ( i ) // conditions exist from above
                criteriaListStr += " AND ";
            
            criteriaListStr += "( (";
            
            CriteriaEditor *criteria2 = m_criteriaEditorAllList.first();
            for( i=0; criteria2; criteria2 = m_criteriaEditorAllList.next(), i++ ) {
                
                QString str = criteria2->getSearchCriteria();
                if( str.contains( "statistics." ) && !joins.contains( "statistics" ) )
                    joins += " LEFT JOIN statistics ON statistics.url=tags.url";
                
                if( i ) //multiple conditions
                    str.prepend( " AND (");
                
                criteriaListStr += str+")";
            }
            criteriaListStr += " )";
        }
        
        whereStr = " WHERE " + criteriaListStr;
    }
    
    //order by expression
    if( m_orderCheck->isChecked() ) {
        if( m_orderCombo->currentItem() != m_orderCombo->count()-1 ) {
            QString field = m_dbFields[ m_orderCombo->currentItem() ];
            if( field.contains( "statistics." ) && !joins.contains( "statistics" ) )
                joins += " LEFT JOIN statistics ON statistics.url=tags.url";

            QString orderType = m_orderTypeCombo->currentItem() == 1 ? " DESC" : " ASC";
            orderStr = " ORDER BY " +  field + orderType;
        }
        else if( m_orderTypeCombo->currentItem() == 0 ) { // completely random
            orderStr = " ORDER BY " + CollectionDB::instance()->randomFunc();
        }
        else {
            /*
            This is the score weighted random order.
            The RAND() function returns random values equally distributed between 0.0 (inclusive) and 1.0 (exclusive).
            The obvious way to get this order is to put every track <score> times into a list, sort the list by RAND()
            (i.e. shuffle it) and discard every occurence of every track but the very first of each.
            By putting every track into the list only once but applying a transfer function
            T_s(x) := 1-(1-x)^(1/s) where s is the score, to RAND() before sorting the list, exactly the same
            distribution of tracks can be achieved (for a proof write to Stefan Siegel <kde@sdas.de>)
            In the query below a simplified function is used: The score is incremented by one to prevent division by
            zero, RAND() is used instead of 1-RAND() because it doesn't matter if it becomes zero (the exponent is
            always non-zero), and finally POWER(...) is used instead of 1-POWER(...) because it only changes the order type.
            */
           orderStr = " ORDER BY POWER(" + CollectionDB::instance()->randomFunc() + ",1.0/(statistics.percentage+1)) DESC";
            if( !joins.contains( "statistics" ) ) {
                joins += " LEFT JOIN statistics ON statistics.url=tags.url";
            }
        }
    }

    if( m_limitCheck->isChecked() ) 
        limitStr = " LIMIT " + QString::number( m_limitSpin->value() )+" OFFSET 0 ";
    

    // take care to adapt SmartPlaylist::NumReturnValues accordingly
    // album / artist / genre / title / year / comment / track / bitrate / discnumber / length / samplerate / path / compilation
    m_query = "SELECT album.name, artist.name, genre.name, tags.title, year.name, tags.comment, tags.track, "
                    "tags.bitrate, tags.discnumber, tags.length, tags.samplerate, tags.filesize, "
                    // here, just before tags.url, is the place to add new return values
                    "tags.url, tags.sampler"
                    " FROM " + joins + whereStr + orderStr + limitStr + ";";

    if( m_expandCheck->isChecked() ) { //We use "(*ExpandString*)" as a marker, if a artist/track/album has this bizarre name, it won't work.
        QString field = m_expandableDbFields[ m_expandCombo->currentItem() ];
        QString table = field.left( field.find('.') );
        if( !joins.contains( table ) ) {
            joins += " LEFT JOIN statistics ON statistics.url=tags.url";
        }
        if ( !criteriaListStr.isEmpty() )
            whereStr = QString(" WHERE (%1) AND %2 = '(*ExpandString*)'").arg(criteriaListStr).arg(field);
        else
            whereStr = QString("WHERE %1 = '(*ExpandString*)'").arg(field);
        m_expandQuery = "SELECT album.name, artist.name, genre.name, tags.title, year.name, "
                            "tags.comment, tags.track, tags.bitrate, tags.discnumber, tags.length,"
                            "tags.samplerate, tags.filesize, tags.url, tags.sampler"
                            " FROM " + joins + whereStr + orderStr + limitStr + ";";
    }
}


/////////////////////////////////////////////////////////////////////////////
//    CLASS CriteriaEditor
////////////////////////////////////////////////////////////////////////////

CriteriaEditor::CriteriaEditor( SmartPlaylistEditor *editor, QWidget *parent, int criteriaType, QDomElement criteria )
    : QHBox( parent )
    , m_playlistEditor( editor )
    , m_currentValueType( -1 )
{
    setSpacing( 5 );

    m_fieldCombo = new KComboBox( this );
    m_fieldCombo->insertStringList( m_fields );

    m_criteriaCombo = new KComboBox( this );

    m_editBox = new QHBox( this );
    m_editBox->setSpacing( 5 );
    setStretchFactor( m_editBox, 1 );

    m_addButton = new QToolButton( this );
    m_addButton->setUsesTextLabel( true );
    m_addButton->setTextLabel("+");
    m_removeButton = new QToolButton( this );
    m_removeButton->setUsesTextLabel( true );
    m_removeButton->setTextLabel("-");

    connect( m_fieldCombo,    SIGNAL( activated(int) ), SLOT( slotFieldSelected(int) ) );
    connect( m_criteriaCombo, SIGNAL( activated(int) ), SLOT( loadEditWidgets() ) );
    if (criteriaType == SmartPlaylistEditor::criteriaAny) {
	connect( m_addButton, SIGNAL( clicked() ), editor, SLOT( addCriteriaAny() ) );
	connect( m_removeButton, SIGNAL( clicked() ), SLOT( slotRemoveCriteriaAny() ) );
    }
    else {
	connect( m_addButton, SIGNAL( clicked() ), editor, SLOT( addCriteriaAll() ) );
	connect( m_removeButton, SIGNAL( clicked() ), SLOT( slotRemoveCriteriaAll() ) );
    }

    if ( !criteria.isNull() ) {
        int field = m_dbFields.findIndex( criteria.attribute( "field" ) );
        QString condition = criteria.attribute("condition");


        QStringList values; //List of the values (only one item, unless condition is "is between")
        QDomNodeList domvalueList = criteria.elementsByTagName( "value" );
        for (int j = 0, c=domvalueList.count() ; j<c; ++j ) {
                values << domvalueList.item(j).toElement().text();
        }

        //Set the selected field

        m_fieldCombo->setCurrentItem( field );
        slotFieldSelected( field );
        int valueType = getValueType( field );
        //Load the right set of criterias for this type, in the dialog
        loadCriteriaList( valueType, condition );

        loadEditWidgets();

        switch( valueType ) {
            case String: //fall through
            case AutoCompletionString:
            {
                m_lineEdit->setText( values.first() );
                break;
            }
            case Year:    //fall through
            case Number:
            {
                m_intSpinBox1->setValue( values.first().toInt() );
                if( condition == i18n("is between") )
                    m_intSpinBox2->setValue( values.last().toInt() );
                break;
            }
            case Rating:
            {
                m_comboBox->setCurrentItem( ratingToIndex( values.first().toInt() ) );
                if( condition == i18n("is between") )
                    m_comboBox2->setCurrentItem( ratingToIndex( values.last().toInt() ) );
                break;
            }
            case Date:
            {
                if( condition == i18n("is in the last") || condition == i18n("is not in the last") ) {
                    m_intSpinBox1->setValue( values.first().toInt() );
                    QString period = criteria.attribute("period");
                    if (period=="days")
                        m_dateCombo->setCurrentItem(0);
                    else if (period=="months")
                        m_dateCombo->setCurrentItem(1);
                    else
                        m_dateCombo->setCurrentItem(2);
                }
                else {
                    QDateTime dt;
                    dt.setTime_t( values.first().toUInt() );
                    m_dateEdit1->setDate( dt.date() );
                    if( condition == i18n("is between") ) {
                        dt.setTime_t( values.last().toUInt() );
                        m_dateEdit2->setDate( dt.date() );
                    }
                }
                break;
            }
            default: ;
        };
    }
    else
        slotFieldSelected( 0 );
    show();
}


CriteriaEditor::~CriteriaEditor()
{
}

QDomElement CriteriaEditor::getDomSearchCriteria( QDomDocument &doc )
{
    QDomElement criteria = doc.createElement( "criteria" );
    QString field = m_dbFields[ m_fieldCombo->currentItem() ];
    QString condition = m_criteriaCombo->currentText();

    criteria.setAttribute( "condition", condition );
    criteria.setAttribute( "field", field );

    QStringList values;
    // Get the proper value(s)
    switch( getValueType( m_fieldCombo->currentItem() ) ) {
         case String: // fall through
         case AutoCompletionString:
            values << m_lineEdit->text();
            break;
         case Year: // fall through
         case Number:
         {
            values << QString::number( m_intSpinBox1->value() );
            if( condition == i18n("is between")  )
                values << QString::number( m_intSpinBox2->value() );
            break;
         }
         case Rating:
         {
            values << QString::number( indexToRating( m_comboBox->currentItem() ) );
            if( condition == i18n("is between")  )
                    values << QString::number( indexToRating( m_comboBox2->currentItem() ) );
            break;
         }
         case Date:
         {
            if( condition == i18n("is in the last") || condition == i18n("is not in the last") ) {
                values << QString::number( m_intSpinBox1->value() );
                // 0 = days; 1=months; 2=years
                criteria.setAttribute( "period", !m_dateCombo->currentItem() ? "days" : (m_dateCombo->currentItem() == 1 ? "months" : "years") );
            }
            else {
                values << QString::number( QDateTime( m_dateEdit1->date() ).toTime_t() );
                if( condition == i18n("is between")  ) {
                    values << QString::number( QDateTime( m_dateEdit2->date() ).toTime_t() );
               }
            }
            break;
         }
         default: ;
    }
    foreach( values ) {
        QDomElement value = doc.createElement( "value" );
        QDomText t = doc.createTextNode( *it );
        value.appendChild( t );
        criteria.appendChild( value );
    }
    return (criteria);
}


QString CriteriaEditor::getSearchCriteria()
{
    QString searchCriteria;
    QString field = m_dbFields[ m_fieldCombo->currentItem() ];
    QString criteria = m_criteriaCombo->currentText();

    if( field.isEmpty() )
        return QString::null;

    if ( ( field=="statistics.playcounter" || field=="statistics.rating" || field=="statistics.percentage" || field=="statistics.accessdate" || field=="statistics.createdate") )
        searchCriteria += "COALESCE(" + field + ",0)";
    else
        searchCriteria += field;

    QString value;
    switch( getValueType( m_fieldCombo->currentItem() ) ) {
        case String:
        case AutoCompletionString:
            value = m_lineEdit->text();
            break;
        case Year:    //fall through
        case Number:
            value = QString::number( m_intSpinBox1->value() );
            if( criteria == i18n("is between")  )
                value += " AND " + QString::number( m_intSpinBox2->value() );
            break;
        case Rating:
        {
            value = QString::number( indexToRating( m_comboBox->currentItem() ) );
            if( criteria == i18n("is between")  )
                value += " AND " + QString::number( indexToRating( m_comboBox2->currentItem() ) );
            break;
        }
        case Date:
        {
            if( criteria == i18n("is in the last") || criteria == i18n("is not in the last") ) {
                int n = m_intSpinBox1->value();
                int time;
                if( m_dateCombo->currentItem() == 0 ) //days
                    time=86400*n;
                else if( m_dateCombo->currentItem() == 1 ) //months
                    time=86400*30*n;
                else time=86400*365*n; //years
                value += "(*CurrentTimeT*)" + QString(" - %1 AND ").arg(time) + "(*CurrentTimeT*)";
            }
            else {
                QDateTime datetime1( m_dateEdit1->date() );
                value += QString::number( datetime1.toTime_t() );
                if( criteria == i18n("is between")  ) {
                    QDateTime datetime2( m_dateEdit2->date() );
                    value += " AND " + QString::number( datetime2.toTime_t() );
                }
                else
                    value += " AND " + QString::number( datetime1.addDays( 1 ).toTime_t() );
            }
            break;
        }
        default: ;
    };


    if( criteria == i18n("contains") )
        searchCriteria += CollectionDB::likeCondition( value, true, true );
    else if( criteria == i18n("does not contain") )
        searchCriteria += " NOT " + CollectionDB::likeCondition( value, true, true );
    else if( criteria == i18n("is") ) {
        if( m_currentValueType == Date )
            searchCriteria += " BETWEEN ";
        else
            searchCriteria += " = ";
        if( m_currentValueType == String || m_currentValueType == AutoCompletionString )
            value.prepend("'").append("'");
        searchCriteria += value;
    }
    else if( criteria == i18n("is not") ) {
        if( m_currentValueType == Date )
            searchCriteria += " NOT BETWEEN ";
        else
            searchCriteria += " <> ";
        if( m_currentValueType == String || m_currentValueType == AutoCompletionString )
            value.prepend("'").append("'");
        searchCriteria += value;
    }
    else if( criteria == i18n("starts with") )
        searchCriteria += CollectionDB::likeCondition( value, false, true );
    else if( criteria == i18n("ends with") )
        searchCriteria += CollectionDB::likeCondition( value, true, false );
    else if( criteria == i18n("is greater than") || criteria == i18n("is after") )
        searchCriteria += " > " + value;
    else if( criteria == i18n("is smaller than") || criteria == i18n("is before" ) )
        searchCriteria += " < " + value;
    else if( criteria == i18n("is between") || criteria == i18n("is in the last") )
        searchCriteria += " BETWEEN " + value;
    else if( criteria == i18n("is not in the last") )
        searchCriteria += " NOT BETWEEN " + value;

    return searchCriteria;
}


void CriteriaEditor::setSearchCriteria( const QString & )
{
    //TODO
}


void CriteriaEditor::enableRemove( bool enable )
{
    m_removeButton->setEnabled( enable );
}


void CriteriaEditor::slotRemoveCriteriaAny()
{
    m_playlistEditor->removeCriteriaAny( this );
}

void CriteriaEditor::slotRemoveCriteriaAll()
{
    m_playlistEditor->removeCriteriaAll( this );
}

void CriteriaEditor::slotAddCriteriaAny()
{
    m_playlistEditor->addCriteriaAny();
}

void CriteriaEditor::slotAddCriteriaAll()
{
    m_playlistEditor->addCriteriaAll();
}

void CriteriaEditor::slotFieldSelected( int field )
{
    int valueType = getValueType( field );
    loadCriteriaList( valueType );
    loadEditWidgets();
    m_currentValueType = valueType;

    //enable auto-completion for artist, album and genre
    if( valueType == AutoCompletionString ) { //Artist, Album, Genre
        QStringList items;
        m_comboBox->clear();
        m_comboBox->completionObject()->clear();

        int currentField = m_fieldCombo->currentItem();
        if( currentField == 0 ) //artist
           items = CollectionDB::instance()->artistList();
        else if( currentField == 1 ) //album
           items = CollectionDB::instance()->albumList();
        else  //genre
           items = CollectionDB::instance()->genreList();

        m_comboBox->insertStringList( items );
        m_comboBox->completionObject()->insertItems( items );
        m_comboBox->completionObject()->setIgnoreCase( true );
        m_comboBox->setCurrentText( "" );
        m_comboBox->setFocus();
    }
}


void CriteriaEditor::loadEditWidgets()
{
    int valueType = getValueType( m_fieldCombo->currentItem() );

    if( m_currentValueType == valueType && !(
        m_criteriaCombo->currentText() == i18n( "is between" ) ||
        m_criteriaCombo->currentText() == i18n( "is in the last" ) ||
        m_criteriaCombo->currentText() == i18n( "is not in the last" ) ||
        m_lastCriteria == i18n( "is between" ) ||
        m_lastCriteria == i18n( "is in the last" ) ||
        m_lastCriteria == i18n( "is not in the last" ) ) )
        return;

    /* Store lastCriteria. This information is used above to decide whether it's necessary to change the Widgets */
    m_lastCriteria = m_criteriaCombo->currentText();

    QObjectList* list = m_editBox->queryList( "QWidget" );
    for( QObject *obj = list->first(); obj; obj = list->next()  )
        static_cast<QWidget*>(obj)->deleteLater();

    delete list;

    switch( valueType ) {

        case String:
        {
            m_lineEdit = new KLineEdit( m_editBox );
            m_lineEdit->setFocus();
            m_lineEdit->show();
            break;
        }

        case AutoCompletionString:    //artist, album, genre
        {
            m_comboBox = new KComboBox( true, m_editBox );
            m_lineEdit = static_cast<KLineEdit*>( m_comboBox->lineEdit() );
            m_lineEdit->setFocus();
            m_comboBox->setMinimumSize( QSize( 240, 20 ) );
            m_comboBox->show();
            break;
        }

        case Year:    //fall through
        case Number:
        {
            bool yearField = m_fieldCombo->currentText() == i18n("Year");

            m_intSpinBox1 = new KIntSpinBox( m_editBox );
            int maxValue = 1000;
            if( yearField ) {
                maxValue = QDate::currentDate().year();
                m_intSpinBox1->setValue( maxValue );
            }
            m_intSpinBox1->setMaxValue( maxValue );
            m_intSpinBox1->setFocus();
            m_intSpinBox1->show();

            if( m_criteriaCombo->currentText() == i18n("is between") ) {
                m_rangeLabel = new QLabel( i18n("and"), m_editBox );
                m_rangeLabel->setAlignment( AlignCenter );
                m_rangeLabel->show();
                m_intSpinBox2 = new KIntSpinBox( m_editBox );
                if( yearField ) {
                    maxValue = QDate::currentDate().year();
                    m_intSpinBox2->setValue( maxValue );
                }
                m_intSpinBox2->setMaxValue( maxValue );
                m_intSpinBox2->show();
            }
            break;
        }

        case Rating:
        {
            const QStringList list = MetaBundle::ratingList();
            m_comboBox = new KComboBox( false, m_editBox );
            m_comboBox->insertStringList( list );
            m_comboBox->show();

            if( m_criteriaCombo->currentText() == i18n("is between") ) {
                m_rangeLabel = new QLabel( i18n("and"), m_editBox );
                m_rangeLabel->setAlignment( AlignCenter );
                m_rangeLabel->show();
                m_comboBox2 = new KComboBox( false, m_editBox );
                m_comboBox2->insertStringList( list );
                m_comboBox2->show();
            }
            break;
        }

        case Date:
        {
            if( m_criteriaCombo->currentText() == i18n("is in the last") ||
                m_criteriaCombo->currentText() == i18n("is not in the last") ) {
                m_intSpinBox1 = new KIntSpinBox( m_editBox );
                m_intSpinBox1->setMinValue( 1 );
                m_intSpinBox1->show();
                m_dateCombo = new KComboBox( m_editBox );
                m_dateCombo->insertItem( i18n("Days") );
                m_dateCombo->insertItem( i18n("Months") );
                m_dateCombo->insertItem( i18n("Years") );
                m_dateCombo->show();
            }
            else {
                m_dateEdit1 = new QDateEdit( QDate::currentDate(), m_editBox);
                m_dateEdit1->setFocus();
                m_dateEdit1->show();
                if( m_criteriaCombo->currentText() == i18n("is between") ) {
                    m_rangeLabel = new QLabel( i18n("and"), m_editBox );
                    m_rangeLabel->setAlignment( AlignCenter );
                    m_rangeLabel->show();
                    m_dateEdit2 = new QDateEdit( QDate::currentDate(), m_editBox);
                    m_dateEdit2->show();
                }
            }

            break;
        }

        default: ;
    };

}


void CriteriaEditor::loadCriteriaList( int valueType, QString condition )
{
    if( m_currentValueType == valueType && condition == QString::null )
        return;

    QStringList items;

    switch( valueType ) {
        case String:
        case AutoCompletionString:
            items << i18n( "contains" ) << i18n( "does not contain" ) << i18n( "is" ) << i18n( "is not" )
                  << i18n( "starts with" ) << i18n( "ends with" );
            break;

        case Rating:
        case Number:
            items << i18n( "is" ) << i18n( "is not" ) << i18n( "is greater than" ) << i18n( "is smaller than" )
                  << i18n( "is between" );
            break;

        case Year: //fall through
        case Date:
            items << i18n( "is" ) << i18n( "is not" ) << i18n( "is before" ) << i18n( "is after" )
                  << i18n( "is in the last" ) << i18n( "is not in the last" ) << i18n( "is between" );
            break;
        default: ;
    };

    m_criteriaCombo->clear();
    m_criteriaCombo->insertStringList( items );

    if ( !condition.isEmpty() ) {
        int index = items.findIndex( condition );
        if (index!=-1)
            m_criteriaCombo->setCurrentItem( index );
    }
}


int CriteriaEditor::getValueType( int index )
{
    int valueType;

    switch( index ) {
        case 0:
        case 1:
        case 2:
            valueType = AutoCompletionString;
            break;
        case 3:
        case 7:
        case 14:
            valueType = String;
            break;
        case 4:
        case 5:
        case 8:
        case 9:
            valueType = Number;
            break;
        case 10:
            valueType = Rating;
            break;
        case 6:
            valueType = Year;
            break;
        default: valueType = Date;
    };

    return valueType;
}


#include "smartplaylisteditor.moc"
