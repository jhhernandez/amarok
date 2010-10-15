/****************************************************************************************
 * Copyright (c) 2008 Daniel Caleb Jones <danielcjones@gmail.com>                       *
 * Copyright (c) 2009 Mark Kretschmann <kretschmann@kde.org>                            *
 * Copyright (c) 2010 Ralf Engels <ralf-engels@gmx.de>                                  *
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

#ifndef AMAROK_METAQUERY_H
#define AMAROK_METAQUERY_H

#include <QWidget>
#include <QPointer>
#include "core/meta/Meta.h"
#include "core/meta/support/MetaConstants.h"

class QFrame;
class QGridLayout;
class QHBoxLayout;
class QVBoxLayout;
class QLabel;
class QToolButton;
class KComboBox;
class KIntSpinBox;
class KToolBar;
class KVBox;

namespace Collections
{
    class QueryMaker;
}

/**
 *  A class that allows to select a time distance.
 */
class TimeDistanceWidget : public QWidget
{
    Q_OBJECT

public:
    TimeDistanceWidget( QWidget *parent = 0 );
    qint64 timeDistance() const;
    void setTimeDistance( qint64 value );
    void connectChanged( QObject *receiver, const char *slot );

protected:
    KIntSpinBox *m_timeEdit;
    KComboBox *m_unitSelection;

private slots:
    void slotUpdateComboBoxLabels( int value );
};

class MetaQueryWidget : public QWidget
{
    Q_OBJECT

    public:
        /** Creates a MetaQueryWidget which can be used to select one meta query filter.
         *  @param onlyNumeric If set to true the widget will only display numeric fields.
         *  @param noBetween If set to true the condition Between will not be used.
         */
        explicit MetaQueryWidget( QWidget* parent = 0, bool onlyNumeric = false, bool noCondition = false );
        ~MetaQueryWidget();

        enum FilterCondition
        {
            Equals = 0,
            GreaterThan = 1,
            LessThan = 2,
            Between = 3,
            OlderThan = 4,
            Contains = 5 // this is the string comparison
        };

        struct Filter
        {
            Filter()
                : field(Meta::valArtist)
                  , numValue(0)
                  , numValue2(0)
                  , condition(Contains)
            {}

            /** Returns a textual representation of the field.
             */
            QString fieldToString();

            /** Returns a textual representation of the filter.
             *  Used for the edit filter dialog (or for debugging)
             */
            QString toString( bool invert = false );

            qint64   field;
            QString  value;
            qint64   numValue;
            qint64   numValue2;
            FilterCondition condition;
        };

        /** Returns the current filter value.
         */
        Filter filter() const;

        /** Returns true if the given field is a numeric field */
        static bool isNumeric( qint64 field );

        /** Returns true if the given field is a date field */
        static bool isDate( qint64 field );

    public slots:
        void setFilter(const MetaQueryWidget::Filter &value);

    signals:
        void changed(const MetaQueryWidget::Filter &value);

    private slots:
        void fieldChanged( int );
        void compareChanged( int );
        void valueChanged( const QString& );
        void numValueChanged( int );
        void numValue2Changed( int );
        void numValueChanged( qint64 );
        void numValue2Changed( qint64 );
        void numValueChanged( const QTime& );
        void numValue2Changed( const QTime& );
        void numValueDateChanged();
        void numValue2DateChanged();
        void numValueTimeDistanceChanged();
        void numValueFormatChanged( int );

        void populateComboBox( QString collectionId, QStringList );
        void comboBoxPopulated();

    private:
        void makeFieldSelection();

        /** Adds the value selection widgets to the layout.
         *  Adds m_compareSelection, m_valueSelection1, m_valueSelection2 to the layout.
         */
        void setValueSelection();

        void makeCompareSelection();
        void makeValueSelection();

        void makeGenericComboSelection( bool editable, Collections::QueryMaker* populateQuery );
        void makeMetaComboSelection( qint64 field );

        void makeFormatComboSelection();
        void makeGenericNumberSelection( int min, int max, int def );
        void makePlaycountSelection();
        void makeRatingSelection();
        void makeLengthSelection();
        void makeDateTimeSelection();
        void makeFilenameSelection();

        bool m_onlyNumeric;
        bool m_noCondition;

        bool m_settingFilter; // if set to true we are just setting the filter

        QVBoxLayout* m_layoutMain;
        QHBoxLayout* m_layoutValue;
        QVBoxLayout* m_layoutValueLabels;
        QVBoxLayout* m_layoutValueValues;

        KComboBox*   m_fieldSelection;
        QLabel*      m_andLabel;
        KComboBox*   m_compareSelection;
        QWidget*     m_valueSelection1;
        QWidget*     m_valueSelection2;

        Filter m_filter;

        QMap< QObject*, QPointer<KComboBox> > m_runningQueries;
};

#endif

