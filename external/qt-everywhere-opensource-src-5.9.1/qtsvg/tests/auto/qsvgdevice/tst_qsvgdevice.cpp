/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/


#include <QtTest/QtTest>

#if 0
#include <private/qpaintengine_svg_p.h>
#endif

#include <qapplication.h>
#include <qpainter.h>
#include <qbuffer.h>
#include <qimage.h>
#include <qpicture.h>
#include <qdrawutil.h>
#include <qpaintdevice.h>

class tst_QSvgDevice : public QObject
{
    Q_OBJECT
public:
    tst_QSvgDevice();

private slots:
    void play_data();
    void play();
    void boundingRect();

private:
    void playPaint( QPainter *p, const QString &type );
};

tst_QSvgDevice::tst_QSvgDevice()
{
}

void tst_QSvgDevice::play_data()
{
    QTest::addColumn<QString>("tag name");
    // we only use the tag name
    QTest::newRow( "lines" );
    QTest::newRow( "font" );
    QTest::newRow( "polyline" );
    QTest::newRow( "translate" );
    QTest::newRow( "scaleRect" );
    QTest::newRow( "ellipseOdd" );
    QTest::newRow( "ellipseEven" );
    QTest::newRow( "ellipseRandom" );
    QTest::newRow( "scaleText" );
    QTest::newRow( "scaleTextWithFont" );
    QTest::newRow( "scaleTextSaveRestore" );
    QTest::newRow( "scaleLineWithPen" );
    QTest::newRow( "task-17637" );
    QTest::newRow( "dashed-lines" );
    QTest::newRow( "dot-lines" );
    QTest::newRow( "dashed-dot-lines" );
    QTest::newRow( "dashed-dot-dot-lines" );
    QTest::newRow( "scaleDashed-lines" );
    QTest::newRow( "thick-dashed-lines" );
    QTest::newRow( "negative-rect" );
    QTest::newRow( "lightText" );
    QTest::newRow( "boldText" );
    QTest::newRow( "demiBoldText" );
    QTest::newRow( "blackText" );
    QTest::newRow( "task-20239" );
    QTest::newRow( "clipRect" );
    QTest::newRow( "multipleClipRects" );
    QTest::newRow("qsimplerichtext");
}

#if 0

class SVGDummyDevice : public QPaintDevice
{
public:
    SVGDummyDevice()
	: QPaintDevice(QInternal::ExternalDevice), eng(0) { }
    ~SVGDummyDevice() {
	delete eng;
    }
    QPaintEngine *engine() const {
	if (!eng)
	    const_cast<SVGDummyDevice *>(this)->eng = new QSVGPaintEngine;
	return eng;
    }

private:
    QPaintEngine *eng;
};

void tst_QSvgDevice::play()
{
    // current tag name
    QString type = data()->dataTag();

    // reference pixmap
    QPixmap ref( 100, 100 );
    ref.fill( Qt::white );
    QPainter pref( &ref );
    playPaint( &pref, type );
    pref.end();

    // draw the same into the SVG device
    SVGDummyDevice dev;
    QPainter pdev( &dev );
    playPaint( &pdev, type );
    pdev.end();


    //dev.setBoundingRect( QRect( 0, 0, 100, 100 ) );
    //dev.save( type + "-res.svg" ); // ### sets bounding rect to 0 !

    // replay on a result pixmap and compare
    QPixmap res( 100, 100 );
    res.fill( Qt::white );
    QPainter pres( &res );
    static_cast<QSVGPaintEngine *>(dev.engine())->play( &pres );

#if 0
    // for visual inspection
    ref.save( type + "-ref.xpm", "XPM" );
    res.save( type + "-res.xpm", "XPM" );
#endif

    QVERIFY( res.convertToImage() == ref.convertToImage() );
}

// helper function for play()
void tst_QSvgDevice::playPaint( QPainter *p, const QString &type )
{
    if ( type == "lines" ) {
	// line with pen width 0
	p->setPen( QPen( Qt::black, 0, Qt::SolidLine ) );
	p->drawLine( 10, 0, 20, 3 );

	// line with pen width 1
	p->setPen( QPen( Qt::black, 1, Qt::SolidLine ) );
	p->drawLine( 2, 0, 10, 3 );

	// rect without outline (qt-bugs/arc-17/35556)
	p->setPen( Qt::NoPen );
	p->setBrush( Qt::red );
	p->drawRect( 5, 10, 20, 30 );
    } else if ( type == "text" ) {
	QFont f = p->font();
	f.setItalic( TRUE );
	f.setBold( TRUE );
	p->setFont( f );
	p->drawText( 5, 55, "Text" );
    } else if ( type == "polyline" ) {
	// we'll draw 4 triangular polylines. Only two will show up
	// as the QPainter::drawPolyline() doesn't respect the brush
	// just the pen setting
	QPolygon pa( 3 );
	pa.setPoint( 0, 0, 0 );
	pa.setPoint( 1, 10, 0 );
	pa.setPoint( 2, 0, 10 );

	// frame around the following 4 polylines
	p->setBrush( Qt::NoBrush );
	p->setPen( Qt::SolidLine );
	p->drawRect( 46, 3, 19, 60 );

	// polyline with blue brush, no pen
	p->setPen( Qt::NoPen );
	p->setBrush( Qt::blue );
	p->translate( 50, 5 );
	p->drawPolyline( pa );

	// polyline without brush, no pen
	p->setBrush( Qt::NoBrush );
	p->translate( 0, 15 );
	p->drawPolyline( pa );

	// polyline with green brush, solid pen
	p->setBrush( Qt::green );
	p->setPen( Qt::SolidLine );
	p->translate( 0, 15 );
	p->drawPolyline( pa );

	// polyline without brush, solid pen
	p->setBrush( Qt::NoBrush );
	p->setPen( Qt::SolidLine );
	p->translate( 0, 15 );
	p->drawPolyline( pa );
    } else if ( type == "translate" ) {
	p->translate(-10,-10);
	p->save();
	p->setBrush( Qt::blue );
	p->drawRect( 20, 30, 50, 20 );
	p->restore();
	p->setBrush( Qt::green );
	p->drawRect( 70, 50, 10, 10 );
    } else if ( type == "scaleRect" ) {
	p->scale( 1, 2 );
	p->setBrush( Qt::blue );
	p->drawRect( 20, 20, 60, 60 );
    } else if ( type == "ellipseEven" ) {
	p->setBrush( Qt::blue );
	p->drawEllipse( 20, 20, 60, 60 );
    } else if ( type == "ellipseOdd" ) {
	p->setBrush( Qt::blue );
	p->drawEllipse( 20, 20, 59, 59 );
    } else if ( type == "ellipseRandom" ) {
	p->setBrush( Qt::blue );
	p->drawEllipse( 20, 34, 89, 123 );
    } else if ( type == "scaleText" ) {
	p->scale(0.25,0.25);
	p->drawText(200,200,"Hello!");
    } else if ( type == "scaleTextWithFont" ) {
	p->setFont(QFont("Helvetica",12));
	p->scale(0.25,0.25);
	p->drawText(200,200,"Hello!");
#ifdef Q_WS_WIN
    // Only test on Windows for now, visually it looks fine, but
    // it's failing for some reason on Linux
    } else if ( type == "scaleTextSaveRestore" ) {
        p->scale(1,-1);
	p->save();
	p->setFont(QFont("Helvetica",12));
	p->scale(0.5,0.5);
	p->drawText(0,0,"Hello!");
	p->restore();
#endif
    } else if ( type == "scaleLineWithPen" ) {
        p->scale(0.1,0.1);
	p->setPen(QPen(Qt::red,100));
	p->drawLine(3,3,500,500);
    } else if ( type == "task-17637" ) {
	p->translate(0,200);
	p->scale(0.01,-0.01);
	p->setBrush(Qt::blue);
	p->drawEllipse(2000,2000,6000,6000);
	p->setPen(QPen(Qt::red,100));
	p->setFont(QFont("Helvetica",12));
	p->save();
	p->scale(1,-1);
	p->drawText(4000,4000,"Hello!");
	p->restore();
	p->drawLine(3000,3000,5000,5000);
    } else if ( type == "dashed-lines" ) {
	p->setPen(QPen(Qt::red, 1, Qt::DashLine));
	p->drawLine(10,10,50,50);
    } else if ( type == "dot-lines" ) {
	p->setPen(QPen(Qt::red, 1, Qt::DotLine));
	p->drawLine(10,10,50,50);
    } else if ( type == "dashed-dot-lines" ) {
	p->setPen(QPen(Qt::red, 1, Qt::DashDotLine));
	p->drawLine(10,10,50,50);
    } else if ( type == "dashed-dot-dot-lines" ) {
	p->setPen(QPen(Qt::red, 1, Qt::DashDotDotLine));
	p->drawLine(10,10,50,50);
    } else if ( type == "scaleDashed-lines" ) {
	p->scale(0.1,0.1);
	p->setPen(QPen(Qt::red, 10, Qt::DashLine));
	p->drawLine(10,10,500,500);
    } else if ( type == "thick-dashed-lines" ) {
	p->setPen(QPen(Qt::red, 10, Qt::DashLine));
	p->drawLine(10,10,50,50);
    } else if ( type == "negative-rect" ) {
	p->drawRect(70, 0,-30,30);
    } else if ( type == "lightText" ) {
	QFont f("Helvetica",12);
	f.setWeight( QFont::Light );
	p->setFont(f);
	p->drawText(0,0,"Hello!");
    } else if ( type == "boldText" ) {
	QFont f("Helvetica",12);
	f.setWeight( QFont::Bold );
	p->setFont(f);
	p->drawText(0,0,"Hello!");
    } else if ( type == "demiBoldText" ) {
	QFont f("Helvetica",12);
	f.setWeight( QFont::DemiBold );
	p->setFont(f);
	p->drawText(0,0,"Hello!");
    } else if ( type == "blackText" ) {
	QFont f("Helvetica",12);
	f.setWeight( QFont::Black );
	p->setFont(f);
	p->drawText(0,0,"Hello!");
    } else if ( type == "task-20239" ) {
        p->translate(0,200);
	p->scale(0.02,-0.02);
	p->scale(1,-1);
	for(int y = 1000; y <=10000; y += 2000) {
	    QBrush br(Qt::green);
	    QPalette palette(Qt::green,Qt::blue);
	    qDrawShadePanel(p,4000,5000,4000,2000,palette.active(),false,200,&br);
	    qDrawShadeRect(p,2000,2000,4000,2000,palette.active(),true,100,0,&br);
	}
    } else if (type=="clipRect") {
	p->setClipRect(15,25,10,10);
	p->drawEllipse(10,20,50,70);
    } else if (type=="multipleClipRects") {
	p->setClipRect(15,25,10,10);
	p->drawEllipse(10,20,50,70);
	p->setClipRect(100,200,20,20);
	p->drawEllipse(110,220,50,70);
    } else if (type == "qsimplerichtext") {
	p->setPen(QColor(0,0,0));
        p->setBrush(Qt::NoBrush);
        QRect rect1(10, 10, 100, 50);
        QRect rect2(200, 20, 80, 50);
        p->drawRect(10, 10, 100, 50);
        p->drawRect(200, 20, 80, 50);

        QSimpleRichText text1("Text 1", QApplication::font());
        QSimpleRichText text2("Text 2", QApplication::font());

        QColorGroup cg(Qt::black, Qt::red, Qt::gray, Qt::gray, Qt::gray, Qt::black, Qt::blue, Qt::black, Qt::white);
        text1.draw(p, rect1.x(), rect1.y(), rect1, cg);
        text2.draw(p, rect2.x(), rect2.y(), rect2, cg);
    }
}

void tst_QSvgDevice::boundingRect()
{
    // ### the bounding rect is only calculated/parsed
    // ### when replaying. Therefore we go through QPicture.
    QRect r( 10, 20, 30, 40 );
    // create document
    QPicture pic;
    QPainter p( &pic );
    p.drawRect( r );
    p.end();
    pic.save( "tmp.svg", "svg" );
    // load it
    QPicture pic2;
    pic2.load( "tmp.svg", "svg" );
    QCOMPARE( pic2.boundingRect(), r );
}

#else

void tst_QSvgDevice::play()
{
    QSKIP("This test needs some redoing, this is just a temp measure until I work it out");
}

void tst_QSvgDevice::boundingRect()
{
    QSKIP("This test needs some redoing, this is just a temp measure until I work it out");
}

#endif

QTEST_MAIN(tst_QSvgDevice)
#include "tst_qsvgdevice.moc"
