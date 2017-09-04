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

#include <qapplication.h>
#include <qdebug.h>
#include <qsvgrenderer.h>
#include <qsvggenerator.h>
#include <QPainter>
#include <QPen>
#include <QPicture>
#include <QXmlStreamReader>

#ifndef SRCDIR
#define SRCDIR
#endif

class tst_QSvgRenderer : public QObject
{
Q_OBJECT

public:
    tst_QSvgRenderer();
    virtual ~tst_QSvgRenderer();

private slots:
    void getSetCheck();
    void inexistentUrl();
    void emptyUrl();
    void testStrokeWidth();
    void testMapViewBoxToTarget();
    void testRenderElement();
    void constructorQXmlStreamReader() const;
    void loadQXmlStreamReader() const;
    void nestedQXmlStreamReader() const;
    void stylePropagation() const;
    void matrixForElement() const;
    void boundsOnElement() const;
    void gradientStops() const;
    void gradientRefs();
    void fillRule();
    void opacity();
    void paths();
    void displayMode();
    void strokeInherit();
    void testFillInheritance();
    void testStopOffsetOpacity();
    void testUseElement();
    void smallFont();

#ifndef QT_NO_COMPRESS
    void testGzLoading();
    void testGzHelper_data();
    void testGzHelper();
#endif

private:
    static const char *const src;
};

const char *const tst_QSvgRenderer::src = "<svg><g><rect x='250' y='250' width='500' height='500'/>"
                                          "<rect id='foo' x='400' y='400' width='100' height='100'/></g></svg>";

tst_QSvgRenderer::tst_QSvgRenderer()
{
}

tst_QSvgRenderer::~tst_QSvgRenderer()
{
}

// Testing get/set functions
void tst_QSvgRenderer::getSetCheck()
{
    QSvgRenderer obj1;
    // int QSvgRenderer::framesPerSecond()
    // void QSvgRenderer::setFramesPerSecond(int)
    obj1.setFramesPerSecond(20);
    QCOMPARE(20, obj1.framesPerSecond());
    obj1.setFramesPerSecond(0);
    QCOMPARE(0, obj1.framesPerSecond());
    obj1.setFramesPerSecond(INT_MIN);
    QCOMPARE(0, obj1.framesPerSecond()); // Can't have a negative framerate
    obj1.setFramesPerSecond(INT_MAX);
    QCOMPARE(INT_MAX, obj1.framesPerSecond());
}

void tst_QSvgRenderer::inexistentUrl()
{
    const char *src = "<svg><g><path d=\"\" style=\"stroke:url(#inexistent)\"/></g></svg>";

    QByteArray data(src);
    QSvgRenderer renderer(data);

    QVERIFY(renderer.isValid());
}

void tst_QSvgRenderer::emptyUrl()
{
    const char *src = "<svg><text fill=\"url()\" /></svg>";

    QByteArray data(src);
    QSvgRenderer renderer(data);

    QVERIFY(renderer.isValid());
}

void tst_QSvgRenderer::testStrokeWidth()
{
    qreal squareSize = 30.0;
    qreal strokeWidth = 1.0;
    qreal topLeft = 100.0;

    QSvgGenerator generator;

    QBuffer buffer;
    QByteArray byteArray;
    buffer.setBuffer(&byteArray);
    generator.setOutputDevice(&buffer);

    QPainter painter(&generator);
    painter.setBrush(Qt::blue);

    // Draw a rect with stroke
    painter.setPen(QPen(Qt::black, strokeWidth));
    painter.drawRect(topLeft, topLeft, squareSize, squareSize);

    // Draw a rect without stroke
    painter.setPen(Qt::NoPen);
    painter.drawRect(topLeft, topLeft, squareSize, squareSize);
    painter.end();

    // Insert ID tags into the document
    byteArray.insert(byteArray.indexOf("stroke=\"#000000\""), "id=\"SquareStroke\" ");
    byteArray.insert(byteArray.indexOf("stroke=\"none\""), "id=\"SquareNoStroke\" ");

    QSvgRenderer renderer(byteArray);

    QRectF noStrokeRect = renderer.boundsOnElement("SquareNoStroke");
    QCOMPARE(noStrokeRect.width(), squareSize);
    QCOMPARE(noStrokeRect.height(), squareSize);
    QCOMPARE(noStrokeRect.x(), topLeft);
    QCOMPARE(noStrokeRect.y(), topLeft);

    QRectF strokeRect = renderer.boundsOnElement("SquareStroke");
    QCOMPARE(strokeRect.width(), squareSize + strokeWidth);
    QCOMPARE(strokeRect.height(), squareSize + strokeWidth);
    QCOMPARE(strokeRect.x(), topLeft - (strokeWidth / 2));
    QCOMPARE(strokeRect.y(), topLeft - (strokeWidth / 2));
}

void tst_QSvgRenderer::testMapViewBoxToTarget()
{
    const char *src = "<svg><g><rect x=\"250\" y=\"250\" width=\"500\" height=\"500\" /></g></svg>";
    QByteArray data(src);

    { // No viewport, viewBox, targetRect, or deviceRect -> boundingRect
        QPicture picture;
        QPainter painter(&picture);
        QSvgRenderer rend(data);
        rend.render(&painter);
        painter.end();
        QCOMPARE(picture.boundingRect(), QRect(0, 0, 500, 500));
    }

    { // No viewport, viewBox, targetRect -> deviceRect
        QPicture picture;
        picture.setBoundingRect(QRect(100, 100, 200, 200));
        QPainter painter(&picture);
        QSvgRenderer rend(data);
        rend.render(&painter);
        painter.end();
        QCOMPARE(picture.boundingRect(), QRect(100, 100, 200, 200));
    }

    { // No viewport, viewBox -> targetRect
        QPicture picture;
        QPainter painter(&picture);
        QSvgRenderer rend(data);
        rend.render(&painter, QRectF(50, 50, 250, 250));
        painter.end();
        QCOMPARE(picture.boundingRect(), QRect(50, 50, 250, 250));

    }

    data.replace("<svg>", "<svg viewBox=\"0 0 1000 1000\">");

    { // No viewport, no targetRect -> viewBox
        QPicture picture;
        QPainter painter(&picture);
        QSvgRenderer rend(data);
        rend.render(&painter);
        painter.end();
        QCOMPARE(picture.boundingRect(), QRect(250, 250, 500, 500));
    }

    data.replace("<svg", "<svg width=\"500\" height=\"500\"");

    { // Viewport
        QPicture picture;
        QPainter painter(&picture);
        QSvgRenderer rend(data);
        rend.render(&painter);
        painter.end();
        QCOMPARE(picture.boundingRect(), QRect(125, 125, 250, 250));
    }

}

void tst_QSvgRenderer::testRenderElement()
{
    QByteArray data(src);

    { // No viewport, viewBox, targetRect, or deviceRect -> boundingRect
        QPicture picture;
        QPainter painter(&picture);
        QSvgRenderer rend(data);
        rend.render(&painter, QLatin1String("foo"));
        painter.end();
        QCOMPARE(picture.boundingRect(), QRect(0, 0, 100, 100));
    }

    { // No viewport, viewBox, targetRect -> deviceRect
        QPicture picture;
        picture.setBoundingRect(QRect(100, 100, 200, 200));
        QPainter painter(&picture);
        QSvgRenderer rend(data);
        rend.render(&painter, QLatin1String("foo"));
        painter.end();
        QCOMPARE(picture.boundingRect(), QRect(100, 100, 200, 200));
    }

    { // No viewport, viewBox -> targetRect
        QPicture picture;
        QPainter painter(&picture);
        QSvgRenderer rend(data);
        rend.render(&painter, QLatin1String("foo"), QRectF(50, 50, 250, 250));
        painter.end();
        QCOMPARE(picture.boundingRect(), QRect(50, 50, 250, 250));

    }

    data.replace("<svg>", "<svg viewBox=\"0 0 1000 1000\">");

    { // No viewport, no targetRect -> view box size
        QPicture picture;
        QPainter painter(&picture);
        QSvgRenderer rend(data);
        rend.render(&painter, QLatin1String("foo"));
        painter.end();
        QCOMPARE(picture.boundingRect(), QRect(0, 0, 100, 100));
    }

    data.replace("<svg", "<svg width=\"500\" height=\"500\"");

    { // Viewport
        QPicture picture;
        QPainter painter(&picture);
        QSvgRenderer rend(data);
        rend.render(&painter, QLatin1String("foo"));
        painter.end();
        QCOMPARE(picture.boundingRect(), QRect(0, 0, 100, 100));
    }

}

void tst_QSvgRenderer::constructorQXmlStreamReader() const
{
    const QByteArray data(src);

    QXmlStreamReader reader(data);

    QPicture picture;
    QPainter painter(&picture);
    QSvgRenderer rend(&reader);
    rend.render(&painter, QLatin1String("foo"));
    painter.end();
    QCOMPARE(picture.boundingRect(), QRect(0, 0, 100, 100));
}

void tst_QSvgRenderer::loadQXmlStreamReader() const
{
    const QByteArray data(src);

    QXmlStreamReader reader(data);
    QPicture picture;
    QPainter painter(&picture);
    QSvgRenderer rend;
    rend.load(&reader);
    rend.render(&painter, QLatin1String("foo"));
    painter.end();
    QCOMPARE(picture.boundingRect(), QRect(0, 0, 100, 100));
}


void tst_QSvgRenderer::nestedQXmlStreamReader() const
{
    const QByteArray data(QByteArray("<bar>") + QByteArray(src) + QByteArray("</bar>"));

    QXmlStreamReader reader(data);

    QCOMPARE(reader.readNext(), QXmlStreamReader::StartDocument);
    QCOMPARE(reader.readNext(), QXmlStreamReader::StartElement);
    QCOMPARE(reader.name().toString(), QLatin1String("bar"));

    QPicture picture;
    QPainter painter(&picture);
    QSvgRenderer renderer(&reader);
    renderer.render(&painter, QLatin1String("foo"));
    painter.end();
    QCOMPARE(picture.boundingRect(), QRect(0, 0, 100, 100));

    QCOMPARE(reader.readNext(), QXmlStreamReader::EndElement);
    QCOMPARE(reader.name().toString(), QLatin1String("bar"));
    QCOMPARE(reader.readNext(), QXmlStreamReader::EndDocument);

    QVERIFY(reader.atEnd());
    QVERIFY(!reader.hasError());
}

void tst_QSvgRenderer::stylePropagation() const
{
    QByteArray data("<svg>"
                      "<g id='foo' style='fill:#ffff00;'>"
                        "<g id='bar' style='fill:#ff00ff;'>"
                          "<g id='baz' style='fill:#00ffff;'>"
                            "<rect id='alpha' x='0' y='0' width='100' height='100'/>"
                          "</g>"
                          "<rect id='beta' x='100' y='0' width='100' height='100'/>"
                        "</g>"
                        "<rect id='gamma' x='0' y='100' width='100' height='100'/>"
                      "</g>"
                      "<rect id='delta' x='100' y='100' width='100' height='100'/>"
                    "</svg>"); // alpha=cyan, beta=magenta, gamma=yellow, delta=black

    QImage image1(200, 200, QImage::Format_RGB32);
    QImage image2(200, 200, QImage::Format_RGB32);
    QImage image3(200, 200, QImage::Format_RGB32);
    QPainter painter;
    QSvgRenderer renderer(data);
    QLatin1String parts[4] = {QLatin1String("alpha"), QLatin1String("beta"), QLatin1String("gamma"), QLatin1String("delta")};

    QVERIFY(painter.begin(&image1));
    for (int i = 0; i < 4; ++i)
        renderer.render(&painter, parts[i], QRectF(renderer.boundsOnElement(parts[i])));
    painter.end();

    QVERIFY(painter.begin(&image2));
    renderer.render(&painter, renderer.viewBoxF());
    painter.end();

    QVERIFY(painter.begin(&image3));
    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(Qt::cyan));
    painter.drawRect(0, 0, 100, 100);
    painter.setBrush(QBrush(Qt::magenta));
    painter.drawRect(100, 0, 100, 100);
    painter.setBrush(QBrush(Qt::yellow));
    painter.drawRect(0, 100, 100, 100);
    painter.setBrush(QBrush(Qt::black));
    painter.drawRect(100, 100, 100, 100);
    painter.end();

    QCOMPARE(image1, image2);
    QCOMPARE(image1, image3);
}

static qreal transformNorm(const QTransform &m)
{
    return qSqrt(m.m11() * m.m11()
        + m.m12() * m.m12()
        + m.m13() * m.m13()
        + m.m21() * m.m21()
        + m.m22() * m.m22()
        + m.m23() * m.m23()
        + m.m31() * m.m31()
        + m.m32() * m.m32()
        + m.m33() * m.m33());
}

static bool diffIsSmallEnough(double diff, double norm)
{
    return diff <= 1e-12 * norm;
}

static inline bool diffIsSmallEnough(float diff, float norm)
{
    return diff <= 1e-5 * norm;
}

static void compareTransforms(const QTransform &m1, const QTransform &m2)
{
    qreal norm1 = transformNorm(m1);
    qreal norm2 = transformNorm(m2);
    qreal diffNorm = transformNorm(QTransform(m1.m11() - m2.m11(),
                                              m1.m12() - m2.m12(),
                                              m1.m13() - m2.m13(),
                                              m1.m21() - m2.m21(),
                                              m1.m22() - m2.m22(),
                                              m1.m23() - m2.m23(),
                                              m1.m31() - m2.m31(),
                                              m1.m32() - m2.m32(),
                                              m1.m33() - m2.m33()));
    QVERIFY(diffIsSmallEnough(diffNorm, qMin(norm1, norm2)));
}

void tst_QSvgRenderer::matrixForElement() const
{
    QByteArray data("<svg>"
                      "<g id='ichi' transform='translate(-3,1)'>"
                        "<g id='ni' transform='rotate(45)'>"
                          "<g id='san' transform='scale(4,2)'>"
                            "<g id='yon' transform='matrix(1,2,3,4,5,6)'>"
                              "<rect id='firkant' x='-1' y='-1' width='2' height='2'/>"
                            "</g>"
                          "</g>"
                        "</g>"
                      "</g>"
                    "</svg>");

    QImage image(13, 37, QImage::Format_RGB32);
    QPainter painter(&image);
    QSvgRenderer renderer(data);

    compareTransforms(QTransform(painter.worldMatrix()), QTransform(renderer.matrixForElement(QLatin1String("ichi"))));
    painter.translate(-3, 1);
    compareTransforms(QTransform(painter.worldMatrix()), QTransform(renderer.matrixForElement(QLatin1String("ni"))));
    painter.rotate(45);
    compareTransforms(QTransform(painter.worldMatrix()), QTransform(renderer.matrixForElement(QLatin1String("san"))));
    painter.scale(4, 2);
    compareTransforms(QTransform(painter.worldMatrix()), QTransform(renderer.matrixForElement(QLatin1String("yon"))));
    painter.setWorldMatrix(QMatrix(1, 2, 3, 4, 5, 6), true);
    compareTransforms(QTransform(painter.worldMatrix()), QTransform(renderer.matrixForElement(QLatin1String("firkant"))));
}

void tst_QSvgRenderer::boundsOnElement() const
{
    QByteArray data("<svg>"
                      "<g id=\"prim\" transform=\"translate(-3,1)\">"
                        "<g id=\"sjokade\" stroke=\"none\" stroke-width=\"10\">"
                          "<rect id=\"kaviar\" transform=\"rotate(45)\" x=\"-10\" y=\"-10\" width=\"20\" height=\"20\"/>"
                        "</g>"
                        "<g id=\"nugatti\" stroke=\"black\" stroke-width=\"2\">"
                          "<use x=\"0\" y=\"0\" transform=\"rotate(45)\" xlink:href=\"#kaviar\"/>"
                        "</g>"
                        "<g id=\"nutella\" stroke=\"none\" stroke-width=\"10\">"
                          "<path id=\"baconost\" transform=\"rotate(45)\" d=\"M-10 -10 L10 -10 L10 10 L-10 10 Z\"/>"
                        "</g>"
                        "<g id=\"hapaa\" transform=\"translate(-2,2)\" stroke=\"black\" stroke-width=\"2\">"
                          "<use x=\"0\" y=\"0\" transform=\"rotate(45)\" xlink:href=\"#baconost\"/>"
                        "</g>"
                      "</g>"
                    "</svg>");
    
    qreal sqrt2 = qSqrt(2);
    QSvgRenderer renderer(data);
    QCOMPARE(renderer.boundsOnElement(QLatin1String("sjokade")), QRectF(-10 * sqrt2, -10 * sqrt2, 20 * sqrt2, 20 * sqrt2));
    QCOMPARE(renderer.boundsOnElement(QLatin1String("kaviar")), QRectF(-10 * sqrt2, -10 * sqrt2, 20 * sqrt2, 20 * sqrt2));
    QCOMPARE(renderer.boundsOnElement(QLatin1String("nugatti")), QRectF(-11, -11, 22, 22));
    QCOMPARE(renderer.boundsOnElement(QLatin1String("nutella")), QRectF(-10 * sqrt2, -10 * sqrt2, 20 * sqrt2, 20 * sqrt2));
    QCOMPARE(renderer.boundsOnElement(QLatin1String("baconost")), QRectF(-10 * sqrt2, -10 * sqrt2, 20 * sqrt2, 20 * sqrt2));
    QCOMPARE(renderer.boundsOnElement(QLatin1String("hapaa")), QRectF(-13, -9, 22, 22));
    QCOMPARE(renderer.boundsOnElement(QLatin1String("prim")), QRectF(-10 * sqrt2 - 3, -10 * sqrt2 + 1, 20 * sqrt2, 20 * sqrt2));
}

void tst_QSvgRenderer::gradientStops() const
{
    {
        QByteArray data("<svg>"
                          "<defs>"
                            "<linearGradient id=\"gradient\">"
                            "</linearGradient>"
                          "</defs>"
                          "<rect fill=\"url(#gradient)\" height=\"64\" width=\"64\" x=\"0\" y=\"0\"/>"
                        "</svg>");
        QSvgRenderer renderer(data);

        QImage image(64, 64, QImage::Format_ARGB32_Premultiplied), refImage(64, 64, QImage::Format_ARGB32_Premultiplied);
        image.fill(0x87654321);
        refImage.fill(0x87654321);

        QPainter painter(&image);
        renderer.render(&painter);
        QCOMPARE(image, refImage);
    }

    {
        QByteArray data("<svg>"
                          "<defs>"
                            "<linearGradient id=\"gradient\">"
                              "<stop offset=\"1\" stop-color=\"cyan\"/>"
                            "</linearGradient>"
                          "</defs>"
                          "<rect fill=\"url(#gradient)\" height=\"64\" width=\"64\" x=\"0\" y=\"0\"/>"
                        "</svg>");
        QSvgRenderer renderer(data);

        QImage image(64, 64, QImage::Format_ARGB32_Premultiplied), refImage(64, 64, QImage::Format_ARGB32_Premultiplied);
        refImage.fill(0xff00ffff);

        QPainter painter(&image);
        renderer.render(&painter);
        QCOMPARE(image, refImage);
    }

    {
        QByteArray data("<svg>"
                          "<defs>"
                            "<linearGradient id=\"gradient\">"
                              "<stop offset=\"0\" stop-color=\"red\"/>"
                              "<stop offset=\"0\" stop-color=\"cyan\"/>"
                              "<stop offset=\"0.5\" stop-color=\"cyan\"/>"
                              "<stop offset=\"0.5\" stop-color=\"magenta\"/>"
                              "<stop offset=\"0.5\" stop-color=\"yellow\"/>"
                              "<stop offset=\"1\" stop-color=\"yellow\"/>"
                              "<stop offset=\"1\" stop-color=\"blue\"/>"
                            "</linearGradient>"
                          "</defs>"
                          "<rect fill=\"url(#gradient)\" height=\"64\" width=\"64\" x=\"0\" y=\"0\"/>"
                        "</svg>");
        QSvgRenderer renderer(data);

        QImage image(64, 64, QImage::Format_ARGB32_Premultiplied), refImage(64, 64, QImage::Format_ARGB32_Premultiplied);

        QPainter painter;
        painter.begin(&refImage);
        painter.fillRect(QRectF(0, 0, 32, 64), Qt::cyan);
        painter.fillRect(QRectF(32, 0, 32, 64), Qt::yellow);
        painter.end();

        painter.begin(&image);
        renderer.render(&painter);
        painter.end();

        QCOMPARE(image, refImage);
    }
}

void tst_QSvgRenderer::gradientRefs()
{
    const char *svgs[] = {
        "<svg>"
            "<defs>"
                "<linearGradient id=\"gradient\">"
                    "<stop offset=\"0\" stop-color=\"red\" stop-opacity=\"0\"/>"
                    "<stop offset=\"1\" stop-color=\"blue\"/>"
                "</linearGradient>"
            "</defs>"
            "<rect fill=\"url(#gradient)\" height=\"8\" width=\"256\" x=\"0\" y=\"0\"/>"
        "</svg>",
        "<svg>"
            "<defs>"
                "<linearGradient id=\"gradient\" xlink:href=\"#gradient0\">"
                "</linearGradient>"
                "<linearGradient id=\"gradient0\">"
                    "<stop offset=\"0\" stop-color=\"red\" stop-opacity=\"0\"/>"
                    "<stop offset=\"1\" stop-color=\"blue\"/>"
                "</linearGradient>"
            "</defs>"
            "<rect fill=\"url(#gradient)\" height=\"8\" width=\"256\" x=\"0\" y=\"0\"/>"
        "</svg>",
        "<svg>"
            "<defs>"
                "<linearGradient id=\"gradient0\">"
                    "<stop offset=\"0\" stop-color=\"red\" stop-opacity=\"0\"/>"
                    "<stop offset=\"1\" stop-color=\"blue\"/>"
                "</linearGradient>"
                "<linearGradient id=\"gradient\" xlink:href=\"#gradient0\">"
                "</linearGradient>"
            "</defs>"
            "<rect fill=\"url(#gradient)\" height=\"8\" width=\"256\" x=\"0\" y=\"0\"/>"
        "</svg>",
        "<svg>"
            "<rect fill=\"url(#gradient)\" height=\"8\" width=\"256\" x=\"0\" y=\"0\"/>"
            "<defs>"
                "<linearGradient id=\"gradient0\">"
                    "<stop offset=\"0\" stop-color=\"red\" stop-opacity=\"0\"/>"
                    "<stop offset=\"1\" stop-color=\"blue\"/>"
                "</linearGradient>"
                "<linearGradient id=\"gradient\" xlink:href=\"#gradient0\">"
                "</linearGradient>"
            "</defs>"
        "</svg>",
        "<svg>"
            "<defs>"
                "<linearGradient xlink:href=\"#0\" id=\"0\">"
                    "<stop offset=\"0\" stop-color=\"red\" stop-opacity=\"0\"/>"
                    "<stop offset=\"1\" stop-color=\"blue\"/>"
                "</linearGradient>"
            "</defs>"
            "<rect fill=\"url(#0)\" height=\"8\" width=\"256\" x=\"0\" y=\"0\"/>"
        "</svg>"
    };
    for (size_t i = 0 ; i < sizeof(svgs) / sizeof(svgs[0]) ; ++i)
    {
        QByteArray data = svgs[i];
        QSvgRenderer renderer(data);

        QImage image(256, 8, QImage::Format_ARGB32_Premultiplied);
        image.fill(0);

        QPainter painter(&image);
        renderer.render(&painter);

        const QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(3));
        QRgb left = line[0]; // transparent black
        QRgb mid = line[127]; // semi transparent magenta
        QRgb right = line[255]; // opaque blue

        QVERIFY((qAlpha(left) < 3) && (qRed(left) < 3) && (qGreen(left) == 0) && (qBlue(left) < 3));
        QVERIFY((qAbs(qAlpha(mid) - 127) < 3) && (qAbs(qRed(mid) - 63) < 4) && (qGreen(mid) == 0) && (qAbs(qBlue(mid) - 63) < 4));
        QVERIFY((qAlpha(right) > 253) && (qRed(right) < 3) && (qGreen(right) == 0) && (qBlue(right) > 251));
    }
}


#ifndef QT_NO_COMPRESS
void tst_QSvgRenderer::testGzLoading()
{
    QSvgRenderer renderer(QLatin1String(SRCDIR "heart.svgz"));
    QVERIFY(renderer.isValid());

    QSvgRenderer resourceRenderer(QLatin1String(":/heart.svgz"));
    QVERIFY(resourceRenderer.isValid());

    QFile largeFileGz(SRCDIR "large.svgz");
    largeFileGz.open(QIODevice::ReadOnly);
    QByteArray data = largeFileGz.readAll();
    QSvgRenderer autoDetectGzData(data);
    QVERIFY(autoDetectGzData.isValid());
}

#ifdef QT_BUILD_INTERNAL
QT_BEGIN_NAMESPACE
QByteArray qt_inflateGZipDataFrom(QIODevice *device);
QT_END_NAMESPACE
#endif

void tst_QSvgRenderer::testGzHelper_data()
{
    QTest::addColumn<QByteArray>("in");
    QTest::addColumn<QByteArray>("out");

    QTest::newRow("empty") << QByteArray() << QByteArray();
    QTest::newRow("small") << QByteArray::fromHex(QByteArray("1f8b08005819934800034b"
            "cbcfe70200a865327e04000000")) << QByteArray("foo\n");

    QFile largeFileGz("large.svgz");
    largeFileGz.open(QIODevice::ReadOnly);
    QFile largeFile("large.svg");
    largeFile.open(QIODevice::ReadOnly);
    QTest::newRow("large") << largeFileGz.readAll() << largeFile.readAll();

    QTest::newRow("zeroes") << QByteArray::fromHex(QByteArray("1f8b0800131f9348000333"
            "301805a360148c54c00500d266708601040000")) << QByteArray(1024, '0').append('\n');

    QTest::newRow("twoMembers") << QByteArray::fromHex(QByteArray("1f8b08001c2a934800034b"
            "cbcfe70200a865327e040000001f8b08001c2a934800034b4a2ce20200e9b3a20404000000"))
        << QByteArray("foo\nbar\n");

    // We should still get data of the first member if subsequent members are corrupt
    QTest::newRow("corruptedSecondMember") << QByteArray::fromHex(QByteArray("1f8b08001c2a934800034b"
            "cbcfe70200a865327e040000001f8c08001c2a934800034b4a2ce20200e9b3a20404000000"))
        << QByteArray("foo\n");

}

void tst_QSvgRenderer::testGzHelper()
{
#ifdef QT_BUILD_INTERNAL
    QFETCH(QByteArray, in);
    QFETCH(QByteArray, out);

    QBuffer buffer(&in);
    buffer.open(QIODevice::ReadOnly);
    QVERIFY(buffer.isReadable());
    QByteArray result = qt_inflateGZipDataFrom(&buffer);
    QCOMPARE(result, out);
#endif
}
#endif

void tst_QSvgRenderer::fillRule()
{
    static const char *svgs[] = {
        // Paths
        // Default fill-rule (nonzero)
        "<svg>"
        "   <rect x=\"0\" y=\"0\" height=\"30\" width=\"30\" fill=\"blue\" />"
        "   <path d=\"M10 0 L30 0 L30 30 L0 30 L0 10 L20 10 L20 20 L10 20 Z\" fill=\"red\" />"
        "</svg>",
        // nonzero
        "<svg>"
        "   <rect x=\"0\" y=\"0\" height=\"30\" width=\"30\" fill=\"blue\" />"
        "   <path d=\"M10 0 L30 0 L30 30 L0 30 L0 10 L20 10 L20 20 L10 20 Z\" fill=\"red\" fill-rule=\"nonzero\" />"
        "</svg>",
        // evenodd
        "<svg>"
        "   <rect x=\"0\" y=\"0\" height=\"30\" width=\"30\" fill=\"blue\" />"
        "   <path d=\"M10 0 L30 0 L30 30 L0 30 L0 10 L20 10 L20 20 L10 20 Z\" fill=\"red\" fill-rule=\"evenodd\" />"
        "</svg>",

        // Polygons
        // Default fill-rule (nonzero)
        "<svg>"
        "   <rect x=\"0\" y=\"0\" height=\"30\" width=\"30\" fill=\"blue\" />"
        "   <polygon points=\"10 0 30 0 30 30 0 30 0 10 20 10 20 20 10 20\" fill=\"red\" />"
        "</svg>",
        // nonzero
        "<svg>"
        "   <rect x=\"0\" y=\"0\" height=\"30\" width=\"30\" fill=\"blue\" />"
        "   <polygon points=\"10 0 30 0 30 30 0 30 0 10 20 10 20 20 10 20\" fill=\"red\" fill-rule=\"nonzero\" />"
        "</svg>",
        // evenodd
        "<svg>"
        "   <rect x=\"0\" y=\"0\" height=\"30\" width=\"30\" fill=\"blue\" />"
        "   <polygon points=\"10 0 30 0 30 30 0 30 0 10 20 10 20 20 10 20\" fill=\"red\" fill-rule=\"evenodd\" />"
        "</svg>"
    };

    const int COUNT = sizeof(svgs) / sizeof(svgs[0]);
    QImage refImageNonZero(30, 30, QImage::Format_ARGB32_Premultiplied);
    QImage refImageEvenOdd(30, 30, QImage::Format_ARGB32_Premultiplied);
    refImageNonZero.fill(0xffff0000);
    refImageEvenOdd.fill(0xffff0000);
    QPainter p;
    p.begin(&refImageNonZero);
    p.fillRect(QRectF(0, 0, 10, 10), Qt::blue);
    p.end();
    p.begin(&refImageEvenOdd);
    p.fillRect(QRectF(0, 0, 10, 10), Qt::blue);
    p.fillRect(QRectF(10, 10, 10, 10), Qt::blue);
    p.end();

    for (int i = 0; i < COUNT; ++i) {
        QByteArray data(svgs[i]);
        QSvgRenderer renderer(data);
        QImage image(30, 30, QImage::Format_ARGB32_Premultiplied);
        image.fill(0);
        p.begin(&image);
        renderer.render(&p);
        p.end();
        QCOMPARE(image, i % 3 == 2 ? refImageEvenOdd : refImageNonZero);
    }
}

static void opacity_drawSvgAndVerify(const QByteArray &data)
{
    QSvgRenderer renderer(data);
    QVERIFY(renderer.isValid());
    QImage image(10, 10, QImage::Format_ARGB32_Premultiplied);
    image.fill(0xffff00ff);
    QPainter painter(&image);
    renderer.render(&painter);
    painter.end();
    QCOMPARE(image.pixel(5, 5), 0xff7f7f7f);
}

void tst_QSvgRenderer::opacity()
{
    static const char *opacities[] = {"-1.4641", "0", "0.5", "1", "1.337"};
    static const char *firstColors[] = {"#7f7f7f", "#7f7f7f", "#402051", "blue", "#123456"};
    static const char *secondColors[] = {"red", "#bad", "#bedead", "#7f7f7f", "#7f7f7f"};

    // Fill-opacity
    for (int i = 0; i < 5; ++i) {
        QByteArray data("<svg><rect x=\"0\" y=\"0\" height=\"10\" width=\"10\" fill=\"");
        data.append(firstColors[i]);
        data.append("\"/><rect x=\"0\" y=\"0\" height=\"10\" width=\"10\" fill=\"");
        data.append(secondColors[i]);
        data.append("\" fill-opacity=\"");
        data.append(opacities[i]);
        data.append("\"/></svg>");
        opacity_drawSvgAndVerify(data);
    }
    // Stroke-opacity
    for (int i = 0; i < 5; ++i) {
        QByteArray data("<svg viewBox=\"0 0 10 10\"><polyline points=\"0 5 10 5\" fill=\"none\" stroke=\"");
        data.append(firstColors[i]);
        data.append("\" stroke-width=\"10\"/><line x1=\"5\" y1=\"0\" x2=\"5\" y2=\"10\" fill=\"none\" stroke=\"");
        data.append(secondColors[i]);
        data.append("\" stroke-width=\"10\" stroke-opacity=\"");
        data.append(opacities[i]);
        data.append("\"/></svg>");
        opacity_drawSvgAndVerify(data);
    }
    // As gradients:
    // Fill-opacity
    for (int i = 0; i < 5; ++i) {
        QByteArray data("<svg><defs><linearGradient id=\"gradient\"><stop offset=\"0\" stop-color=\"");
        data.append(secondColors[i]);
        data.append("\"/><stop offset=\"1\" stop-color=\"");
        data.append(secondColors[i]);
        data.append("\"/></linearGradient></defs><rect x=\"0\" y=\"0\" height=\"10\" width=\"10\" fill=\"");
        data.append(firstColors[i]);
        data.append("\"/><rect x=\"0\" y=\"0\" height=\"10\" width=\"10\" fill=\"url(#gradient)\" fill-opacity=\"");
        data.append(opacities[i]);
        data.append("\"/></svg>");
        opacity_drawSvgAndVerify(data);
    }
    // Stroke-opacity
    for (int i = 0; i < 5; ++i) {
        QByteArray data("<svg viewBox=\"0 0 10 10\"><defs><linearGradient id=\"grad\"><stop offset=\"0\" stop-color=\"");
        data.append(secondColors[i]);
        data.append("\"/><stop offset=\"1\" stop-color=\"");
        data.append(secondColors[i]);
        data.append("\"/></linearGradient></defs><polyline points=\"0 5 10 5\" fill=\"none\" stroke=\"");
        data.append(firstColors[i]);
        data.append("\" stroke-width=\"10\" /><line x1=\"5\" y1=\"0\" x2=\"5\" y2=\"10\" fill=\"none\" stroke=\"url(#grad)\" stroke-width=\"10\" stroke-opacity=\"");
        data.append(opacities[i]);
        data.append("\" /></svg>");
        opacity_drawSvgAndVerify(data);
    }
}

void tst_QSvgRenderer::paths()
{
    static const char *svgs[] = {
        // Absolute coordinates, explicit commands.
        "<svg>"
        "   <rect x=\"0\" y=\"0\" height=\"50\" width=\"50\" fill=\"blue\" />"
        "   <path d=\"M50 0 V50 H0 Q0 25 25 25 T50 0 C25 0 50 50 25 50 S25 0 0 0 Z\" fill=\"red\" fill-rule=\"evenodd\"/>"
        "</svg>",
        // Absolute coordinates, implicit commands.
        "<svg>"
        "   <rect x=\"0\" y=\"0\" height=\"50\" width=\"50\" fill=\"blue\" />"
        "   <path d=\"M50 0 50 50 0 50 Q0 25 25 25 Q50 25 50 0 C25 0 50 50 25 50 C0 50 25 0 0 0 Z\" fill=\"red\" fill-rule=\"evenodd\" />"
        "</svg>",
        // Relative coordinates, explicit commands.
        "<svg>"
        "   <rect x=\"0\" y=\"0\" height=\"50\" width=\"50\" fill=\"blue\" />"
        "   <path d=\"m50 0 v50 h-50 q0 -25 25 -25 t25 -25 c-25 0 0 50 -25 50 s0 -50 -25 -50 z\" fill=\"red\" fill-rule=\"evenodd\" />"
        "</svg>",
        // Relative coordinates, implicit commands.
        "<svg>"
        "   <rect x=\"0\" y=\"0\" height=\"50\" width=\"50\" fill=\"blue\" />"
        "   <path d=\"m50 0 0 50 -50 0 q0 -25 25 -25 25 0 25 -25 c-25 0 0 50 -25 50 -25 0 0 -50 -25 -50 z\" fill=\"red\" fill-rule=\"evenodd\" />"
        "</svg>",
        // Absolute coordinates, explicit commands, minimal whitespace.
        "<svg>"
        "   <rect x=\"0\" y=\"0\" height=\"50\" width=\"50\" fill=\"blue\" />"
        "   <path d=\"m50 0v50h-50q0-25 25-25t25-25c-25 0 0 50-25 50s0-50-25-50z\" fill=\"red\" fill-rule=\"evenodd\" />"
        "</svg>",
        // Absolute coordinates, explicit commands, extra whitespace.
        "<svg>"
        "   <rect x=\"0\" y=\"0\" height=\"50\" width=\"50\" fill=\"blue\" />"
        "   <path d=\" M  50  0  V  50  H  0  Q 0  25   25 25 T  50 0 C 25   0 50  50 25 50 S  25 0 0  0 Z  \" fill=\"red\" fill-rule=\"evenodd\" />"
        "</svg>"
    };

    const int COUNT = sizeof(svgs) / sizeof(svgs[0]);
    QImage images[COUNT];
    QPainter p;

    for (int i = 0; i < COUNT; ++i) {
        QByteArray data(svgs[i]);
        QSvgRenderer renderer(data);
        QVERIFY(renderer.isValid());
        images[i] = QImage(50, 50, QImage::Format_ARGB32_Premultiplied);
        images[i].fill(0);
        p.begin(&images[i]);
        renderer.render(&p);
        p.end();
        if (i != 0) {
            QCOMPARE(images[i], images[0]);
        }
    }
}

void tst_QSvgRenderer::displayMode()
{
    static const char *svgs[] = {
        // All visible.
        "<svg>"
        "   <g>"
        "       <rect x=\"0\" y=\"0\" height=\"10\" width=\"10\" fill=\"red\" />"
        "       <rect x=\"0\" y=\"0\" height=\"10\" width=\"10\" fill=\"blue\" />"
        "   </g>"
        "</svg>",
        // Don't display svg element.
        "<svg display=\"none\">"
        "   <g>"
        "       <rect x=\"0\" y=\"0\" height=\"10\" width=\"10\" fill=\"red\" />"
        "       <rect x=\"0\" y=\"0\" height=\"10\" width=\"10\" fill=\"blue\" />"
        "   </g>"
        "</svg>",
        // Don't display g element.
        "<svg>"
        "   <g display=\"none\">"
        "       <rect x=\"0\" y=\"0\" height=\"10\" width=\"10\" fill=\"red\" />"
        "       <rect x=\"0\" y=\"0\" height=\"10\" width=\"10\" fill=\"blue\" />"
        "   </g>"
        "</svg>",
        // Don't display first rect element.
        "<svg>"
        "   <g>"
        "       <rect x=\"0\" y=\"0\" height=\"10\" width=\"10\" fill=\"red\" display=\"none\" />"
        "       <rect x=\"0\" y=\"0\" height=\"10\" width=\"10\" fill=\"blue\" />"
        "   </g>"
        "</svg>",
        // Don't display second rect element.
        "<svg>"
        "   <g>"
        "       <rect x=\"0\" y=\"0\" height=\"10\" width=\"10\" fill=\"red\" />"
        "       <rect x=\"0\" y=\"0\" height=\"10\" width=\"10\" fill=\"blue\" display=\"none\" />"
        "   </g>"
        "</svg>",
        // Don't display svg element, but set display mode to "inline" for other elements.
        "<svg display=\"none\">"
        "   <g display=\"inline\">"
        "       <rect x=\"0\" y=\"0\" height=\"10\" width=\"10\" fill=\"red\" display=\"inline\" />"
        "       <rect x=\"0\" y=\"0\" height=\"10\" width=\"10\" fill=\"blue\" display=\"inline\" />"
        "   </g>"
        "</svg>"
    };

    QRgb expectedColors[] = {0xff0000ff, 0xff00ff00, 0xff00ff00, 0xff0000ff, 0xffff0000, 0xff00ff00};

    const int COUNT = sizeof(svgs) / sizeof(svgs[0]);
    QPainter p;

    for (int i = 0; i < COUNT; ++i) {
        QByteArray data(svgs[i]);
        QSvgRenderer renderer(data);
        QVERIFY(renderer.isValid());
        QImage image(10, 10, QImage::Format_ARGB32_Premultiplied);
        image.fill(0xff00ff00);
        p.begin(&image);
        renderer.render(&p);
        p.end();
        QCOMPARE(image.pixel(5, 5), expectedColors[i]);
    }
}

void tst_QSvgRenderer::strokeInherit()
{
    static const char *svgs[] = {
        // Reference.
        "<svg viewBox=\"0 0 200 30\">"
        "   <g stroke=\"blue\" stroke-width=\"20\" stroke-linecap=\"butt\""
        "       stroke-linejoin=\"miter\" stroke-miterlimit=\"1\" stroke-dasharray=\"20,10\""
        "       stroke-dashoffset=\"10\" stroke-opacity=\"0.5\">"
        "       <polyline fill=\"none\" points=\"10 10 100 10 100 20 190 20\"/>"
        "   </g>"
        "   <g stroke=\"green\" stroke-width=\"0\" stroke-dasharray=\"3,3,1\" stroke-dashoffset=\"4.5\">"
        "       <polyline fill=\"none\" points=\"10 25 80 25\"/>"
        "   </g>"
        "</svg>",
        // stroke
        "<svg viewBox=\"0 0 200 30\">"
        "   <g stroke=\"none\" stroke-width=\"20\" stroke-linecap=\"butt\""
        "       stroke-linejoin=\"miter\" stroke-miterlimit=\"1\" stroke-dasharray=\"20,10\""
        "       stroke-dashoffset=\"10\" stroke-opacity=\"0.5\">"
        "       <polyline fill=\"none\" points=\"10 10 100 10 100 20 190 20\" stroke=\"blue\"/>"
        "   </g>"
        "   <g stroke=\"yellow\" stroke-width=\"0\" stroke-dasharray=\"3,3,1\" stroke-dashoffset=\"4.5\">"
        "       <polyline fill=\"none\" points=\"10 25 80 25\" stroke=\"green\"/>"
        "   </g>"
        "</svg>",
        // stroke-width
        "<svg viewBox=\"0 0 200 30\">"
        "   <g stroke=\"blue\" stroke-width=\"0\" stroke-linecap=\"butt\""
        "       stroke-linejoin=\"miter\" stroke-miterlimit=\"1\" stroke-dasharray=\"20,10\""
        "       stroke-dashoffset=\"10\" stroke-opacity=\"0.5\">"
        "       <polyline fill=\"none\" points=\"10 10 100 10 100 20 190 20\" stroke-width=\"20\"/>"
        "   </g>"
        "   <g stroke=\"green\" stroke-width=\"10\" stroke-dasharray=\"3,3,1\" stroke-dashoffset=\"4.5\">"
        "       <polyline fill=\"none\" points=\"10 25 80 25\" stroke-width=\"0\"/>"
        "   </g>"
        "</svg>",
        // stroke-linecap
        "<svg viewBox=\"0 0 200 30\">"
        "   <g stroke=\"blue\" stroke-width=\"20\" stroke-linecap=\"round\""
        "       stroke-linejoin=\"miter\" stroke-miterlimit=\"1\" stroke-dasharray=\"20,10\""
        "       stroke-dashoffset=\"10\" stroke-opacity=\"0.5\">"
        "       <polyline fill=\"none\" points=\"10 10 100 10 100 20 190 20\" stroke-linecap=\"butt\"/>"
        "   </g>"
        "   <g stroke=\"green\" stroke-width=\"0\" stroke-dasharray=\"3,3,1\" stroke-dashoffset=\"4.5\">"
        "       <polyline fill=\"none\" points=\"10 25 80 25\"/>"
        "   </g>"
        "</svg>",
        // stroke-linejoin
        "<svg viewBox=\"0 0 200 30\">"
        "   <g stroke=\"blue\" stroke-width=\"20\" stroke-linecap=\"butt\""
        "       stroke-linejoin=\"round\" stroke-miterlimit=\"1\" stroke-dasharray=\"20,10\""
        "       stroke-dashoffset=\"10\" stroke-opacity=\"0.5\">"
        "       <polyline fill=\"none\" points=\"10 10 100 10 100 20 190 20\" stroke-linejoin=\"miter\"/>"
        "   </g>"
        "   <g stroke=\"green\" stroke-width=\"0\" stroke-dasharray=\"3,3,1\" stroke-dashoffset=\"4.5\">"
        "       <polyline fill=\"none\" points=\"10 25 80 25\"/>"
        "   </g>"
        "</svg>",
        // stroke-miterlimit
        "<svg viewBox=\"0 0 200 30\">"
        "   <g stroke=\"blue\" stroke-width=\"20\" stroke-linecap=\"butt\""
        "       stroke-linejoin=\"miter\" stroke-miterlimit=\"2\" stroke-dasharray=\"20,10\""
        "       stroke-dashoffset=\"10\" stroke-opacity=\"0.5\">"
        "       <polyline fill=\"none\" points=\"10 10 100 10 100 20 190 20\" stroke-miterlimit=\"1\"/>"
        "   </g>"
        "   <g stroke=\"green\" stroke-width=\"0\" stroke-dasharray=\"3,3,1\" stroke-dashoffset=\"4.5\">"
        "       <polyline fill=\"none\" points=\"10 25 80 25\"/>"
        "   </g>"
        "</svg>",
        // stroke-dasharray
        "<svg viewBox=\"0 0 200 30\">"
        "   <g stroke=\"blue\" stroke-width=\"20\" stroke-linecap=\"butt\""
        "       stroke-linejoin=\"miter\" stroke-miterlimit=\"1\" stroke-dasharray=\"1,1,1,1,1,1,3,1,3,1,3,1,1,1,1,1,1,3\""
        "       stroke-dashoffset=\"10\" stroke-opacity=\"0.5\">"
        "       <polyline fill=\"none\" points=\"10 10 100 10 100 20 190 20\" stroke-dasharray=\"20,10\"/>"
        "   </g>"
        "   <g stroke=\"green\" stroke-width=\"0\" stroke-dasharray=\"none\" stroke-dashoffset=\"4.5\">"
        "       <polyline fill=\"none\" points=\"10 25 80 25\" stroke-dasharray=\"3,3,1\"/>"
        "   </g>"
        "</svg>",
        // stroke-dashoffset
        "<svg viewBox=\"0 0 200 30\">"
        "   <g stroke=\"blue\" stroke-width=\"20\" stroke-linecap=\"butt\""
        "       stroke-linejoin=\"miter\" stroke-miterlimit=\"1\" stroke-dasharray=\"20,10\""
        "       stroke-dashoffset=\"0\" stroke-opacity=\"0.5\">"
        "       <polyline fill=\"none\" points=\"10 10 100 10 100 20 190 20\" stroke-dashoffset=\"10\"/>"
        "   </g>"
        "   <g stroke=\"green\" stroke-width=\"0\" stroke-dasharray=\"3,3,1\" stroke-dashoffset=\"0\">"
        "       <polyline fill=\"none\" points=\"10 25 80 25\" stroke-dashoffset=\"4.5\"/>"
        "   </g>"
        "</svg>",
        // stroke-opacity
        "<svg viewBox=\"0 0 200 30\">"
        "   <g stroke=\"blue\" stroke-width=\"20\" stroke-linecap=\"butt\""
        "       stroke-linejoin=\"miter\" stroke-miterlimit=\"1\" stroke-dasharray=\"20,10\""
        "       stroke-dashoffset=\"10\" stroke-opacity=\"0\">"
        "       <polyline fill=\"none\" points=\"10 10 100 10 100 20 190 20\" stroke-opacity=\"0.5\"/>"
        "   </g>"
        "   <g stroke=\"green\" stroke-width=\"0\" stroke-dasharray=\"3,3,1\" stroke-dashoffset=\"4.5\">"
        "       <polyline fill=\"none\" points=\"10 25 80 25\"/>"
        "   </g>"
        "</svg>"
    };

    const int COUNT = sizeof(svgs) / sizeof(svgs[0]);
    QImage images[COUNT];
    QPainter p;

    for (int i = 0; i < COUNT; ++i) {
        QByteArray data(svgs[i]);
        QSvgRenderer renderer(data);
        QVERIFY(renderer.isValid());
        images[i] = QImage(200, 30, QImage::Format_ARGB32_Premultiplied);
        images[i].fill(-1);
        p.begin(&images[i]);
        renderer.render(&p);
        p.end();
        if (i != 0) {
            QCOMPARE(images[0], images[i]);
        }
    }
}

void tst_QSvgRenderer::testFillInheritance()
{
    static const char *svgs[] = {
        //reference
        "<svg viewBox = \"0 0 200 200\">"
        "    <polygon points=\"20,20 50,120 100,10 40,80 50,80\" fill= \"red\" stroke = \"blue\" fill-opacity = \"0.5\" fill-rule = \"evenodd\"/>"
        "</svg>",
        "<svg viewBox = \"0 0 200 200\">"
        "    <polygon points=\"20,20 50,120 100,10 40,80 50,80\" fill= \"red\" stroke = \"blue\" fill-opacity = \"0.5\" fill-rule = \"evenodd\"/>"
        "    <rect x = \"40\" y = \"40\" width = \"70\" height =\"20\" fill = \"green\" fill-opacity = \"0\"/>"
        "</svg>",
        "<svg viewBox = \"0 0 200 200\">"
        "   <g fill = \"red\" fill-opacity = \"0.5\" fill-rule = \"evenodd\">"
        "       <polygon points=\"20,20 50,120 100,10 40,80 50,80\" stroke = \"blue\"/>"
        "   </g>"
        "    <rect x = \"40\" y = \"40\" width = \"70\" height =\"20\" fill = \"green\" fill-opacity = \"0\"/>"
        "</svg>",
        "<svg viewBox = \"0 0 200 200\">"
        "   <g  fill = \"green\" fill-rule = \"nonzero\">"
        "       <polygon points=\"20,20 50,120 100,10 40,80 50,80\" stroke = \"blue\" fill = \"red\" fill-opacity = \"0.5\" fill-rule = \"evenodd\"/>"
        "   </g>"
        "   <g fill-opacity = \"0.8\" fill = \"red\">"
        "       <rect x = \"40\" y = \"40\" width = \"70\" height =\"20\" fill = \"green\" fill-opacity = \"0\"/>"
        "   </g>"
        "</svg>",
        "<svg viewBox = \"0 0 200 200\">"
        "   <g  fill = \"red\" >"
        "      <g fill-opacity = \"0.5\">"
        "         <g fill-rule = \"evenodd\">"
        "            <g>"
        "                <polygon points=\"20,20 50,120 100,10 40,80 50,80\" stroke = \"blue\"/>"
        "            </g>"
        "         </g>"
        "      </g>"
        "   </g>"
        "   <g fill-opacity = \"0.8\" >"
        "       <rect x = \"40\" y = \"40\" width = \"70\" height =\"20\" fill = \"none\"/>"
        "   </g>"
        "</svg>",
        "<svg viewBox = \"0 0 200 200\">"
        "   <g fill = \"none\" fill-opacity = \"0\">"
        "       <polygon points=\"20,20 50,120 100,10 40,80 50,80\" stroke = \"blue\" fill = \"red\" fill-opacity = \"0.5\" fill-rule = \"evenodd\"/>"
        "   </g>"
        "   <g fill-opacity = \"0\" >"
        "       <rect x = \"40\" y = \"40\" width = \"70\" height =\"20\" fill = \"green\"/>"
        "   </g>"
        "</svg>"
    };

    const int COUNT = sizeof(svgs) / sizeof(svgs[0]);
    QImage images[COUNT];
    QPainter p;

    for (int i = 0; i < COUNT; ++i) {
        QByteArray data(svgs[i]);
        QSvgRenderer renderer(data);
        QVERIFY(renderer.isValid());
        images[i] = QImage(200, 200, QImage::Format_ARGB32_Premultiplied);
        images[i].fill(-1);
        p.begin(&images[i]);
        renderer.render(&p);
        p.end();
        if (i != 0) {
            QCOMPARE(images[0], images[i]);
        }
    }
}
void tst_QSvgRenderer::testStopOffsetOpacity()
{
    static const char *svgs[] = {
        //reference
        "<svg  viewBox=\"0 0 64 64\">"
         "<radialGradient id=\"MyGradient1\" gradientUnits=\"userSpaceOnUse\" cx=\"50\" cy=\"50\" r=\"30\" fx=\"20\" fy=\"20\">"
          "<stop offset=\"0.0\" style=\"stop-color:red\"  stop-opacity=\"0.3\"/>"
          "<stop offset=\"0.5\" style=\"stop-color:green\"  stop-opacity=\"1\"/>"
          "<stop offset=\"1\" style=\"stop-color:yellow\"  stop-opacity=\"1\"/>"
         "</radialGradient>"
         "<radialGradient id=\"MyGradient2\" gradientUnits=\"userSpaceOnUse\" cx=\"50\" cy=\"70\" r=\"70\" fx=\"20\" fy=\"20\">"
          "<stop offset=\"0.0\" style=\"stop-color:blue\"  stop-opacity=\"0.3\"/>"
          "<stop offset=\"0.5\" style=\"stop-color:violet\"  stop-opacity=\"1\"/>"
          "<stop offset=\"1\" style=\"stop-color:orange\"  stop-opacity=\"1\"/>"
         "</radialGradient>"
         "<rect  x=\"5\" y=\"5\" width=\"55\" height=\"55\" fill=\"url(#MyGradient1)\" stroke=\"black\" />"
         "<rect  x=\"20\" y=\"20\" width=\"35\" height=\"35\" fill=\"url(#MyGradient2)\"/>"
        "</svg>",
        //Stop Offset
        "<svg  viewBox=\"0 0 64 64\">"
         "<radialGradient id=\"MyGradient1\" gradientUnits=\"userSpaceOnUse\" cx=\"50\" cy=\"50\" r=\"30\" fx=\"20\" fy=\"20\">"
          "<stop offset=\"abc\" style=\"stop-color:red\"  stop-opacity=\"0.3\"/>"
          "<stop offset=\"0.5\" style=\"stop-color:green\"  stop-opacity=\"1\"/>"
          "<stop offset=\"1\" style=\"stop-color:yellow\"  stop-opacity=\"1\"/>"
         "</radialGradient>"
         "<radialGradient id=\"MyGradient2\" gradientUnits=\"userSpaceOnUse\" cx=\"50\" cy=\"70\" r=\"70\" fx=\"20\" fy=\"20\">"
          "<stop offset=\"-3.bc\" style=\"stop-color:blue\"  stop-opacity=\"0.3\"/>"
          "<stop offset=\"0.5\" style=\"stop-color:violet\"  stop-opacity=\"1\"/>"
          "<stop offset=\"1\" style=\"stop-color:orange\"  stop-opacity=\"1\"/>"
         "</radialGradient>"
         "<rect  x=\"5\" y=\"5\" width=\"55\" height=\"55\" fill=\"url(#MyGradient1)\" stroke=\"black\" />"
         "<rect  x=\"20\" y=\"20\" width=\"35\" height=\"35\" fill=\"url(#MyGradient2)\"/>"
        "</svg>",
        //Stop Opacity
        "<svg  viewBox=\"0 0 64 64\">"
         "<radialGradient id=\"MyGradient1\" gradientUnits=\"userSpaceOnUse\" cx=\"50\" cy=\"50\" r=\"30\" fx=\"20\" fy=\"20\">"
          "<stop offset=\"0.0\" style=\"stop-color:red\"  stop-opacity=\"0.3\"/>"
          "<stop offset=\"0.5\" style=\"stop-color:green\"  stop-opacity=\"x.45\"/>"
          "<stop offset=\"1\" style=\"stop-color:yellow\"  stop-opacity=\"-3.abc\"/>"
         "</radialGradient>"
         "<radialGradient id=\"MyGradient2\" gradientUnits=\"userSpaceOnUse\" cx=\"50\" cy=\"70\" r=\"70\" fx=\"20\" fy=\"20\">"
          "<stop offset=\"0.0\" style=\"stop-color:blue\"  stop-opacity=\"0.3\"/>"
          "<stop offset=\"0.5\" style=\"stop-color:violet\"  stop-opacity=\"-0.xy\"/>"
          "<stop offset=\"1\" style=\"stop-color:orange\"  stop-opacity=\"z.5\"/>"
         "</radialGradient>"
         "<rect  x=\"5\" y=\"5\" width=\"55\" height=\"55\" fill=\"url(#MyGradient1)\" stroke=\"black\" />"
         "<rect  x=\"20\" y=\"20\" width=\"35\" height=\"35\" fill=\"url(#MyGradient2)\"/>"
        "</svg>",
        //Stop offset and Stop opacity
        "<svg  viewBox=\"0 0 64 64\">"
         "<radialGradient id=\"MyGradient1\" gradientUnits=\"userSpaceOnUse\" cx=\"50\" cy=\"50\" r=\"30\" fx=\"20\" fy=\"20\">"
          "<stop offset=\"abc\" style=\"stop-color:red\"  stop-opacity=\"0.3\"/>"
          "<stop offset=\"0.5\" style=\"stop-color:green\"  stop-opacity=\"x.45\"/>"
          "<stop offset=\"1\" style=\"stop-color:yellow\"  stop-opacity=\"-3.abc\"/>"
         "</radialGradient>"
         "<radialGradient id=\"MyGradient2\" gradientUnits=\"userSpaceOnUse\" cx=\"50\" cy=\"70\" r=\"70\" fx=\"20\" fy=\"20\">"
          "<stop offset=\"-3.bc\" style=\"stop-color:blue\"  stop-opacity=\"0.3\"/>"
          "<stop offset=\"0.5\" style=\"stop-color:violet\"  stop-opacity=\"-0.xy\"/>"
          "<stop offset=\"1\" style=\"stop-color:orange\"  stop-opacity=\"z.5\"/>"
         "</radialGradient>"
         "<rect  x=\"5\" y=\"5\" width=\"55\" height=\"55\" fill=\"url(#MyGradient1)\" stroke=\"black\" />"
         "<rect  x=\"20\" y=\"20\" width=\"35\" height=\"35\" fill=\"url(#MyGradient2)\"/>"
        "</svg>"
    };

    QImage images[4];
    QPainter p;

    for (int i = 0; i < 4; ++i) {
        QByteArray data(svgs[i]);
        QSvgRenderer renderer(data);
        QVERIFY(renderer.isValid());
        images[i] = QImage(64, 64, QImage::Format_ARGB32_Premultiplied);
        images[i].fill(-1);
        p.begin(&images[i]);
        renderer.render(&p);
        p.end();
    }
    QCOMPARE(images[0], images[1]);
    QCOMPARE(images[0], images[2]);
    QCOMPARE(images[0], images[3]);
}

void tst_QSvgRenderer::testUseElement()
{
    static const char *svgs[] = {
        //Use refering to non group node (1)
        "<svg viewBox = \"0 0 200 200\">"
        " <polygon points=\"20,20 50,120 100,10 40,80 50,80\"/>"
        " <polygon points=\"20,80 50,180 100,70 40,140 50,140\" fill= \"red\" stroke = \"blue\" fill-opacity = \"0.7\" fill-rule = \"evenodd\" stroke-width = \"3\"/>"
        "</svg>",
        "<svg viewBox = \"0 0 200 200\">"
        " <polygon id = \"usedPolyline\" points=\"20,20 50,120 100,10 40,80 50,80\"/>"
        " <use y = \"60\" xlink:href = \"#usedPolyline\" fill= \"red\" stroke = \"blue\" fill-opacity = \"0.7\" fill-rule = \"evenodd\" stroke-width = \"3\"/>"
        "</svg>",
        "<svg viewBox = \"0 0 200 200\">"
        " <polygon id = \"usedPolyline\" points=\"20,20 50,120 100,10 40,80 50,80\"/>"
        " <g fill = \" red\" fill-opacity =\"0.2\">"
        "<use y = \"60\" xlink:href = \"#usedPolyline\" stroke = \"blue\" fill-opacity = \"0.7\" fill-rule = \"evenodd\" stroke-width = \"3\"/>"
        "</g>"
        "</svg>",
        "<svg viewBox = \"0 0 200 200\">"
        " <polygon id = \"usedPolyline\" points=\"20,20 50,120 100,10 40,80 50,80\"/>"
        " <g stroke-width = \"3\" stroke = \"yellow\">"
        "  <use y = \"60\" xlink:href = \"#usedPolyline\" fill = \" red\" stroke = \"blue\" fill-opacity = \"0.7\" fill-rule = \"evenodd\"/>"
        " </g>"
        "</svg>",
        //Use refering to non group node (2)
        "<svg viewBox = \"0 0 200 200\">"
        " <polygon points=\"20,20 50,120 100,10 40,80 50,80\" fill = \"green\" fill-rule = \"nonzero\" stroke = \"purple\" stroke-width = \"4\" stroke-dasharray = \"1,1,3,1\" stroke-offset = \"3\" stroke-miterlimit = \"6\" stroke-linecap = \"butt\" stroke-linejoin = \"round\"/>"
        " <polygon points=\"20,80 50,180 100,70 40,140 50,140\" fill= \"red\" stroke = \"blue\" fill-opacity = \"0.7\" fill-rule = \"evenodd\" stroke-width = \"3\" stroke-dasharray = \"1,1,1,1\" stroke-offset = \"5\" stroke-miterlimit = \"3\" stroke-linecap = \"butt\" stroke-linejoin = \"square\"/>"
        "</svg>",
        "<svg viewBox = \"0 0 200 200\">"
        " <g fill = \"green\" fill-rule = \"nonzero\" stroke = \"purple\" stroke-width = \"4\" stroke-dasharray = \"1,1,3,1\" stroke-offset = \"3\" stroke-miterlimit = \"6\" stroke-linecap = \"butt\" stroke-linejoin = \"round\">"
        "  <polygon id = \"usedPolyline\" points=\"20,20 50,120 100,10 40,80 50,80\" />"
        " </g>"
        " <g stroke = \"blue\" stroke-width = \"3\" stroke-dasharray = \"1,1,1,1\" stroke-offset = \"5\" stroke-miterlimit = \"3\" stroke-linecap = \"butt\" stroke-linejoin = \"square\">"
        "  <use y = \"60\" xlink:href = \"#usedPolyline\"  fill-opacity = \"0.7\" fill= \"red\" stroke = \"blue\" fill-rule = \"evenodd\"/>"
        " </g>"
        "</svg>",
        "<svg viewBox = \"0 0 200 200\">"
        " <g fill = \"green\" fill-rule = \"nonzero\" stroke = \"purple\" stroke-width = \"4\" stroke-dasharray = \"1,1,3,1\" stroke-offset = \"3\" stroke-miterlimit = \"6\" stroke-linecap = \"butt\" stroke-linejoin = \"round\">"
        "  <polygon id = \"usedPolyline\" points=\"20,20 50,120 100,10 40,80 50,80\" />"
        " </g>"
        " <g stroke-width = \"3\" stroke-dasharray = \"1,1,1,1\" stroke-offset = \"5\" stroke-miterlimit = \"3\" stroke-linecap = \"butt\" stroke-linejoin = \"square\" >"
        "  <use y = \"60\" xlink:href = \"#usedPolyline\" fill= \"red\" stroke = \"blue\" fill-opacity = \"0.7\" fill-rule = \"evenodd\" />"
        " </g>"
        "</svg>",
        //Use refering to group node
        "<svg viewBox = \"0 0 200 200\">"
        " <g>"
        "  <circle cx=\"0\" cy=\"0\" r=\"100\" fill = \"red\" fill-opacity = \"0.6\"/>"
        "  <rect x = \"10\" y = \"10\" width = \"30\" height = \"30\" fill = \"red\" fill-opacity = \"0.5\"/>"
        "  <circle fill=\"#a6ce39\" cx=\"0\" cy=\"0\" r=\"33\" fill-opacity = \"0.5\"/>"
        " </g>"
        "</svg>",
        "<svg viewBox = \"0 0 200 200\">"
        " <defs>"
        "  <g id=\"usedG\">"
        "   <circle cx=\"0\" cy=\"0\" r=\"100\" fill-opacity = \"0.6\"/>"
        "   <rect x = \"10\" y = \"10\" width = \"30\" height = \"30\"/>"
        "   <circle fill=\"#a6ce39\" cx=\"0\" cy=\"0\" r=\"33\" />"
        "  </g>"
        " </defs>"
        " <use xlink:href =\"#usedG\" fill = \"red\" fill-opacity =\"0.5\"/>"
        "</svg>",
        "<svg viewBox = \"0 0 200 200\">"
        " <defs>"
        "  <g fill = \"blue\" fill-opacity = \"0.3\">"
        "   <g id=\"usedG\">"
        "    <circle cx=\"0\" cy=\"0\" r=\"100\" fill-opacity = \"0.6\"/>"
        "    <rect x = \"10\" y = \"10\" width = \"30\" height = \"30\"/>"
        "    <circle fill=\"#a6ce39\" cx=\"0\" cy=\"0\" r=\"33\" />"
        "   </g>"
        "  </g>"
        " </defs>"
        " <g fill = \"red\" fill-opacity =\"0.5\">"
        "  <use xlink:href =\"#usedG\" />"
        " </g>"
        "</svg>"
    };

    const int COUNT = sizeof(svgs) / sizeof(svgs[0]);
    QImage images[COUNT];
    QPainter p;

    for (int i = 0; i < COUNT; ++i) {
        QByteArray data(svgs[i]);
        QSvgRenderer renderer(data);
        images[i] = QImage(200, 200, QImage::Format_ARGB32_Premultiplied);
        images[i].fill(-1);
        p.begin(&images[i]);
        renderer.render(&p);
        p.end();

        if (i < 4 && i != 0) {
            QCOMPARE(images[0], images[i]);
        } else if (i > 4 && i < 7) {
            if (sizeof(qreal) != sizeof(float))
            {
                // These images use blending functions which due to numerical
                // issues on Windows CE and likes differ in very few pixels.
                // For this reason an exact comparison will fail.
                QCOMPARE(images[4], images[i]);
            }
        } else if (i > 7) {
            QCOMPARE(images[8], images[i]);
        }
    }
}

void tst_QSvgRenderer::smallFont()
{
    static const char *svgs[] = { "<svg width=\"50px\" height=\"50px\"><text x=\"10\" y=\"10\" font-size=\"0\">Hello world</text></svg>",
                                  "<svg width=\"50px\" height=\"50px\"><text x=\"10\" y=\"10\" font-size=\"0.5\">Hello world</text></svg>"
    };
    const int COUNT = sizeof(svgs) / sizeof(svgs[0]);
    QImage images[COUNT];
    QPainter p;

    for (int i = 0; i < COUNT; ++i) {
        QByteArray data(svgs[i]);
        if (i == 0)
            QTest::ignoreMessage(QtWarningMsg, "QFont::setPointSizeF: Point size <= 0 (0.000000), must be greater than 0");
        QSvgRenderer renderer(data);
        images[i] = QImage(50, 50, QImage::Format_ARGB32_Premultiplied);
        images[i].fill(-1);
        p.begin(&images[i]);
        renderer.render(&p);
        p.end();
    }
    QVERIFY(images[0] != images[1]);
}

QTEST_MAIN(tst_QSvgRenderer)
#include "tst_qsvgrenderer.moc"
