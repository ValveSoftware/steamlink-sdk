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

#include <qtest.h>

#include <QtQuick/qquickitem.h>
#include <QtQuick/qquickview.h>
#include <QtQuick/qsgnode.h>
#include <QtQuick/qsggeometry.h>
#include <QtQuick/qsgflatcolormaterial.h>
#include <QtGui/qscreen.h>
#include <QtGui/qopenglcontext.h>

#include "../../shared/util.h"

class tst_drawingmodes : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_drawingmodes();

    bool hasPixelAround(const QImage &fb, int centerX, int centerY);
    QImage runTest(const QString &fileName)
    {
        QQuickView view(&outerWindow);
        view.setResizeMode(QQuickView::SizeViewToRootObject);
        view.setSource(testFileUrl(fileName));
        view.setVisible(true);
        QTest::qWaitForWindowExposed(&view);
        return view.grabWindow();
    }

    //It is important for platforms that only are able to show fullscreen windows
    //to have a container for the window that is painted on.
    QQuickWindow outerWindow;
    const QRgb black;
    const QRgb red;

private slots:
    void points();
    void lines();
    void lineStrip();
    void lineLoop();
    void triangles();
    void triangleStrip();
    void triangleFan();
};

class DrawingModeItem : public QQuickItem
{
    Q_OBJECT
public:
    static GLenum drawingMode;

    DrawingModeItem() : first(QSGGeometry::defaultAttributes_Point2D(), 5),
        second(QSGGeometry::defaultAttributes_Point2D(), 5)
    {
        setFlag(ItemHasContents, true);
        material.setColor(Qt::red);
    }

protected:
    QSGGeometry first;
    QSGGeometry second;
    QSGFlatColorMaterial material;

    virtual QSGNode *updatePaintNode(QSGNode *node, UpdatePaintNodeData *)
    {
        if (!node) {
            QRect bounds(0, 0, 200, 200);
            first.setDrawingMode(drawingMode);
            second.setDrawingMode(drawingMode);

            QSGGeometry::Point2D *v = first.vertexDataAsPoint2D();
            v[0].set(bounds.width() * 2 / 8, bounds.height() / 2);
            v[1].set(bounds.width() / 8, bounds.height() / 4);
            v[2].set(bounds.width() * 3 / 8, bounds.height() / 4);
            v[3].set(bounds.width() * 3 / 8, bounds.height() * 3 / 4);
            v[4].set(bounds.width() / 8, bounds.height() * 3 / 4);

            v = second.vertexDataAsPoint2D();
            v[0].set(bounds.width() * 6 / 8, bounds.height() / 2);
            v[1].set(bounds.width() * 5 / 8, bounds.height() / 4);
            v[2].set(bounds.width() * 7 / 8, bounds.height() / 4);
            v[3].set(bounds.width() * 7 / 8, bounds.height() * 3 / 4);
            v[4].set(bounds.width() * 5 / 8, bounds.height() * 3 / 4);

            node = new QSGNode;
            QSGGeometryNode *child = new QSGGeometryNode;
            child->setGeometry(&first);
            child->setMaterial(&material);
            node->appendChildNode(child);
            child = new QSGGeometryNode;
            child->setGeometry(&second);
            child->setMaterial(&material);
            node->appendChildNode(child);
        }
        return node;
    }
};

GLenum DrawingModeItem::drawingMode;

bool tst_drawingmodes::hasPixelAround(const QImage &fb, int centerX, int centerY) {
    for (int x = centerX - 2; x <= centerX + 2; ++x) {
        for (int y = centerY - 2; y <= centerY + 2; ++y) {
            if (fb.pixel(x, y) == red)
                return true;
        }
    }
    return false;
}

tst_drawingmodes::tst_drawingmodes() : black(qRgb(0, 0, 0)), red(qRgb(0xff, 0, 0))
{
    qmlRegisterType<DrawingModeItem>("Test", 1, 0, "DrawingModeItem");
    outerWindow.showNormal();
    outerWindow.setGeometry(0,0,400,400);
}

void tst_drawingmodes::points()
{
    DrawingModeItem::drawingMode = GL_POINTS;
    if (QGuiApplication::primaryScreen()->depth() < 24)
        QSKIP("This test does not work at display depths < 24");

#ifdef Q_OS_WIN
    if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES)
        QSKIP("ANGLE cannot draw GL_POINTS.");
#endif

    QImage fb = runTest("DrawingModes.qml");

    QVERIFY(hasPixelAround(fb, 50, 100));
    QVERIFY(hasPixelAround(fb, 25, 50));
    QVERIFY(hasPixelAround(fb, 75, 50));
    QVERIFY(hasPixelAround(fb, 75, 150));
    QVERIFY(hasPixelAround(fb, 25, 150));

    QVERIFY(hasPixelAround(fb, 150, 100));
    QVERIFY(hasPixelAround(fb, 125, 50));
    QVERIFY(hasPixelAround(fb, 175, 50));
    QVERIFY(hasPixelAround(fb, 175, 150));
    QVERIFY(hasPixelAround(fb, 125, 150));

    QVERIFY(!hasPixelAround(fb, 135, 70));
    QVERIFY(!hasPixelAround(fb, 175, 100));
    QVERIFY(!hasPixelAround(fb, 110, 140));
    QVERIFY(!hasPixelAround(fb, 50, 50));
    QVERIFY(!hasPixelAround(fb, 50, 150));
    QVERIFY(!hasPixelAround(fb, 25, 100));
    QVERIFY(!hasPixelAround(fb, 75, 100));
    QVERIFY(!hasPixelAround(fb, 125, 100));
    QVERIFY(!hasPixelAround(fb, 150, 50));
    QVERIFY(!hasPixelAround(fb, 150, 150));
    QVERIFY(!hasPixelAround(fb, 135, 130));
    QVERIFY(!hasPixelAround(fb, 35, 130));
}

void tst_drawingmodes::lines()
{
    DrawingModeItem::drawingMode = GL_LINES;
    if (QGuiApplication::primaryScreen()->depth() < 24)
        QSKIP("This test does not work at display depths < 24");
    QImage fb = runTest("DrawingModes.qml");

    QCOMPARE(fb.width(), 200);
    QCOMPARE(fb.height(), 200);

    QVERIFY(hasPixelAround(fb, 135, 70));
    QVERIFY(hasPixelAround(fb, 175, 100));
    QVERIFY(!hasPixelAround(fb, 110, 140));
    QVERIFY(!hasPixelAround(fb, 50, 50));
    QVERIFY(!hasPixelAround(fb, 50, 150));

    QVERIFY(hasPixelAround(fb, 35, 70));
    QVERIFY(hasPixelAround(fb, 75, 100));
    QVERIFY(!hasPixelAround(fb, 25, 100));
    QVERIFY(!hasPixelAround(fb, 125, 100));
    QVERIFY(!hasPixelAround(fb, 150, 50));
    QVERIFY(!hasPixelAround(fb, 150, 150));
    QVERIFY(!hasPixelAround(fb, 135, 130));
    QVERIFY(!hasPixelAround(fb, 35, 130));
}

void tst_drawingmodes::lineStrip()
{
    DrawingModeItem::drawingMode = GL_LINE_STRIP;
    if (QGuiApplication::primaryScreen()->depth() < 24)
        QSKIP("This test does not work at display depths < 24");
    QImage fb = runTest("DrawingModes.qml");

    QCOMPARE(fb.width(), 200);
    QCOMPARE(fb.height(), 200);

    QVERIFY(hasPixelAround(fb, 135, 70));
    QVERIFY(hasPixelAround(fb, 150, 50));
    QVERIFY(hasPixelAround(fb, 175, 100));
    QVERIFY(hasPixelAround(fb, 150, 150));

    QVERIFY(hasPixelAround(fb, 35, 70));
    QVERIFY(hasPixelAround(fb, 50, 50));
    QVERIFY(hasPixelAround(fb, 75, 100));
    QVERIFY(hasPixelAround(fb, 50, 150));

    QVERIFY(!hasPixelAround(fb, 110, 140)); // bad line not there => line strip unbatched

    QVERIFY(!hasPixelAround(fb, 25, 100));
    QVERIFY(!hasPixelAround(fb, 125, 100));
    QVERIFY(!hasPixelAround(fb, 135, 130));
    QVERIFY(!hasPixelAround(fb, 35, 130));
}

void tst_drawingmodes::lineLoop()
{
    DrawingModeItem::drawingMode = GL_LINE_LOOP;
    if (QGuiApplication::primaryScreen()->depth() < 24)
        QSKIP("This test does not work at display depths < 24");
    QImage fb = runTest("DrawingModes.qml");

    QCOMPARE(fb.width(), 200);
    QCOMPARE(fb.height(), 200);

    QVERIFY(hasPixelAround(fb, 135, 70));
    QVERIFY(hasPixelAround(fb, 135, 130));
    QVERIFY(hasPixelAround(fb, 150, 50));
    QVERIFY(hasPixelAround(fb, 175, 100));
    QVERIFY(hasPixelAround(fb, 150, 150));

    QVERIFY(hasPixelAround(fb, 35, 70));
    QVERIFY(hasPixelAround(fb, 35, 130));
    QVERIFY(hasPixelAround(fb, 50, 50));
    QVERIFY(hasPixelAround(fb, 75, 100));
    QVERIFY(hasPixelAround(fb, 50, 150));

    QVERIFY(!hasPixelAround(fb, 110, 140)); // bad line not there => line loop unbatched

    QVERIFY(!hasPixelAround(fb, 25, 100));
    QVERIFY(!hasPixelAround(fb, 125, 100));
}

void tst_drawingmodes::triangles()
{
    DrawingModeItem::drawingMode = GL_TRIANGLES;
    if (QGuiApplication::primaryScreen()->depth() < 24)
        QSKIP("This test does not work at display depths < 24");
    QImage fb = runTest("DrawingModes.qml");

    QCOMPARE(fb.width(), 200);
    QCOMPARE(fb.height(), 200);

    QVERIFY(hasPixelAround(fb, 150, 75));
    QVERIFY(!hasPixelAround(fb, 162, 100));
    QVERIFY(!hasPixelAround(fb, 150, 125));
    QVERIFY(!hasPixelAround(fb, 137, 100));

    QVERIFY(!hasPixelAround(fb, 100, 125));

    QVERIFY(hasPixelAround(fb, 50, 75));
    QVERIFY(!hasPixelAround(fb, 62, 100));
    QVERIFY(!hasPixelAround(fb, 50, 125));
    QVERIFY(!hasPixelAround(fb, 37, 100));
}


void tst_drawingmodes::triangleStrip()
{
    DrawingModeItem::drawingMode = GL_TRIANGLE_STRIP;
    if (QGuiApplication::primaryScreen()->depth() < 24)
        QSKIP("This test does not work at display depths < 24");
    QImage fb = runTest("DrawingModes.qml");

    QCOMPARE(fb.width(), 200);
    QCOMPARE(fb.height(), 200);

    QVERIFY(hasPixelAround(fb, 150, 75));
    QVERIFY(hasPixelAround(fb, 162, 100));
    QVERIFY(hasPixelAround(fb, 150, 125));
    QVERIFY(!hasPixelAround(fb, 137, 100));

    QVERIFY(!hasPixelAround(fb, 100, 125)); // batching avoids extra triangle by duplicating vertices.

    QVERIFY(hasPixelAround(fb, 50, 75));
    QVERIFY(hasPixelAround(fb, 62, 100));
    QVERIFY(hasPixelAround(fb, 50, 125));
    QVERIFY(!hasPixelAround(fb, 37, 100));
}

void tst_drawingmodes::triangleFan()
{
    DrawingModeItem::drawingMode = GL_TRIANGLE_FAN;
    if (QGuiApplication::primaryScreen()->depth() < 24)
        QSKIP("This test does not work at display depths < 24");
    QImage fb = runTest("DrawingModes.qml");

    QCOMPARE(fb.width(), 200);
    QCOMPARE(fb.height(), 200);

    QVERIFY(hasPixelAround(fb, 150, 75));
    QVERIFY(hasPixelAround(fb, 162, 100));
    QVERIFY(hasPixelAround(fb, 150, 125));
    QVERIFY(!hasPixelAround(fb, 137, 100));

    QVERIFY(!hasPixelAround(fb, 100, 125)); // no extra triangle; triangle fan is not batched

    QVERIFY(hasPixelAround(fb, 50, 75));
    QVERIFY(hasPixelAround(fb, 62, 100));
    QVERIFY(hasPixelAround(fb, 50, 125));
    QVERIFY(!hasPixelAround(fb, 37, 100));
}


QTEST_MAIN(tst_drawingmodes)

#include "tst_drawingmodes.moc"
