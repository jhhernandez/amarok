/***************************************************************************
 * copyright            : (C) 2007 Leo Franchi <lfranchi@gmail.com>        *
 **************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef LYRICS_APPLET_H
#define LYRICS_APPLET_H

#include "context/Applet.h"
#include "context/DataEngine.h"
#include "context/Svg.h"

class QGraphicsSimpleTextItem;
class QGraphicsTextItem;

class LyricsApplet : public Context::Applet
{
    Q_OBJECT
public:
    LyricsApplet( QObject* parent, const QVariantList& args );

    void paintInterface( QPainter *painter, const QStyleOptionGraphicsItem* option, const QRect& contentsRect );

    // reimplemented to tell Plasma our size
    QSizeF contentSizeHint() const;
    // reimplemented to help Plasma::Layout deal with us
//    QSizeF sizeHint() const { return boundingRect().size(); }

    void constraintsUpdated();

   // void setRect( const QRectF& rect );
    // for use with the Context layout
   // void setGeometry( const QRectF& rect ) { setRect( rect ); }

public slots:
    void updated( const QString& name, const Plasma::DataEngine::Data& data );

signals:
    void changed();

private:
    void calculateHeight();
    void resize( qreal newWidth );

    Context::Svg* m_header;
    qreal m_headerAspectRatio;
    QSizeF m_size;

    // labels
    QGraphicsSimpleTextItem* m_lyricsLabel;
    QGraphicsSimpleTextItem* m_titleLabel;
    QGraphicsSimpleTextItem* m_artistLabel;
    QGraphicsSimpleTextItem* m_siteLabel;
    // holds main body
    QGraphicsTextItem* m_lyrics;
    // titles
    QGraphicsSimpleTextItem* m_title;
    QGraphicsSimpleTextItem* m_artist;
    QGraphicsSimpleTextItem* m_site;
};

K_EXPORT_AMAROK_APPLET( lyrics, LyricsApplet )

#endif
