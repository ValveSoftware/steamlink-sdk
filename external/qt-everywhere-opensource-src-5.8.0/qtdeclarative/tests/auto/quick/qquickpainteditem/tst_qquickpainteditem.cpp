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
#include <QtTest/QSignalSpy>
#include <QtQuick/qquickpainteditem.h>
#include <QtQuick/qquickview.h>

#include <private/qquickitem_p.h>
#ifndef QT_NO_OPENGL
#include <private/qsgdefaultpainternode_p.h>
#else
#include <private/qsgsoftwarepainternode_p.h>
#endif
class tst_QQuickPaintedItem: public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void update();
    void opaquePainting();
    void antialiasing();
    void mipmap();
    void performanceHints();
    void contentsSize();
    void contentScale();
    void contentsBoundingRect();
    void fillColor();
    void renderTarget();

private:
    QQuickWindow window;
};

class TestPaintedItem : public QQuickPaintedItem
{
    Q_OBJECT
public:
    TestPaintedItem(QQuickItem *parent = 0)
        : QQuickPaintedItem(parent)
        , paintNode(0)
        , paintRequests(0)
    {
    }

    void paint(QPainter *painter)
    {
        ++paintRequests;
        clipRect = painter->clipBoundingRect();
    }
#ifndef QT_NO_OPENGL
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data)
    {
        paintNode = static_cast<QSGDefaultPainterNode *>(QQuickPaintedItem::updatePaintNode(oldNode, data));
        return paintNode;
    }

    QSGDefaultPainterNode *paintNode;
#else
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data)
    {
        paintNode = static_cast<QSGSoftwarePainterNode *>(QQuickPaintedItem::updatePaintNode(oldNode, data));
        return paintNode;
    }

    QSGSoftwarePainterNode *paintNode;
#endif
    int paintRequests;
    QRectF clipRect;
};

static bool hasDirtyContentFlag(QQuickItem *item) {
    return QQuickItemPrivate::get(item)->dirtyAttributes & QQuickItemPrivate::Content; }
static void clearDirtyContentFlag(QQuickItem *item) {
    QQuickItemPrivate::get(item)->dirtyAttributes &= ~QQuickItemPrivate::Content; }

void tst_QQuickPaintedItem::initTestCase()
{
    window.resize(320, 240);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
}

void tst_QQuickPaintedItem::update()
{
    TestPaintedItem item;
    item.setParentItem(window.contentItem());

    QCOMPARE(hasDirtyContentFlag(&item), false);
    item.update();
    QCOMPARE(hasDirtyContentFlag(&item), true);

    QTRY_COMPARE(hasDirtyContentFlag(&item), false);
    QCOMPARE(item.paintRequests, 0);    // Size empty

    item.setSize(QSizeF(320, 240));

    item.update();
    QCOMPARE(hasDirtyContentFlag(&item), true);

    QTRY_COMPARE(hasDirtyContentFlag(&item), false);
    QCOMPARE(item.paintRequests, 1);
    QCOMPARE(item.clipRect, QRectF(0, 0, 0, 0));

    item.update(QRect(30, 25, 12, 11));
    QCOMPARE(hasDirtyContentFlag(&item), true);

    QTRY_COMPARE(hasDirtyContentFlag(&item), false);
    QCOMPARE(item.paintRequests, 2);
    QCOMPARE(item.clipRect, QRectF(30, 25, 12, 11));

    item.update(QRect(30, 25, 12, 11));
    item.update(QRect(112, 56, 20, 20));

    QCOMPARE(hasDirtyContentFlag(&item), true);

    QTRY_COMPARE(hasDirtyContentFlag(&item), false);
    QCOMPARE(item.paintRequests, 3);
    QCOMPARE(item.clipRect, QRectF(30, 25, 102, 51));
}

void tst_QQuickPaintedItem::opaquePainting()
{
    TestPaintedItem item;
    item.setSize(QSizeF(320, 240));
    item.setParentItem(window.contentItem());

    QCOMPARE(item.opaquePainting(), false);

    item.setOpaquePainting(false);
    QCOMPARE(item.opaquePainting(), false);
    QCOMPARE(hasDirtyContentFlag(&item), false);

    item.update();
    QTRY_COMPARE(hasDirtyContentFlag(&item), false);
    QVERIFY(item.paintNode);
    QCOMPARE(item.paintNode->opaquePainting(), false);

    item.setOpaquePainting(true);
    QCOMPARE(item.opaquePainting(), true);
    QCOMPARE(hasDirtyContentFlag(&item), true);

    QTRY_COMPARE(hasDirtyContentFlag(&item), false);
    QVERIFY(item.paintNode);
    QCOMPARE(item.paintNode->opaquePainting(), true);

    item.setOpaquePainting(true);
    QCOMPARE(item.opaquePainting(), true);
    QCOMPARE(hasDirtyContentFlag(&item), false);

    item.setOpaquePainting(false);
    QCOMPARE(item.opaquePainting(), false);
    QCOMPARE(hasDirtyContentFlag(&item), true);

    QTRY_COMPARE(hasDirtyContentFlag(&item), false);
    QVERIFY(item.paintNode);
    QCOMPARE(item.paintNode->opaquePainting(), false);
}

void tst_QQuickPaintedItem::antialiasing()
{
    TestPaintedItem item;
    item.setSize(QSizeF(320, 240));
    item.setParentItem(window.contentItem());

    QCOMPARE(item.antialiasing(), false);

    item.setAntialiasing(false);
    QCOMPARE(item.antialiasing(), false);
    QCOMPARE(hasDirtyContentFlag(&item), false);

    item.update();
    QTRY_COMPARE(hasDirtyContentFlag(&item), false);
    QVERIFY(item.paintNode);
    QCOMPARE(item.paintNode->smoothPainting(), false);

    item.setAntialiasing(true);
    QCOMPARE(item.antialiasing(), true);
    QCOMPARE(hasDirtyContentFlag(&item), true);

    QTRY_COMPARE(hasDirtyContentFlag(&item), false);
    QVERIFY(item.paintNode);
    QCOMPARE(item.paintNode->smoothPainting(), true);

    item.setAntialiasing(true);
    QCOMPARE(item.antialiasing(), true);
    QCOMPARE(hasDirtyContentFlag(&item), false);

    item.setAntialiasing(false);
    QCOMPARE(item.antialiasing(), false);
    QCOMPARE(hasDirtyContentFlag(&item), true);

    QTRY_COMPARE(hasDirtyContentFlag(&item), false);
    QVERIFY(item.paintNode);
    QCOMPARE(item.paintNode->smoothPainting(), false);
}

void tst_QQuickPaintedItem::mipmap()
{
    TestPaintedItem item;
    item.setSize(QSizeF(320, 240));
    item.setParentItem(window.contentItem());

    QCOMPARE(item.mipmap(), false);

    item.setMipmap(false);
    QCOMPARE(item.mipmap(), false);
    QCOMPARE(hasDirtyContentFlag(&item), false);

    item.update();
    QTRY_COMPARE(hasDirtyContentFlag(&item), false);
    QVERIFY(item.paintNode);
    QCOMPARE(item.paintNode->mipmapping(), false);

    item.setMipmap(true);
    QCOMPARE(item.mipmap(), true);
    QCOMPARE(hasDirtyContentFlag(&item), true);

    QTRY_COMPARE(hasDirtyContentFlag(&item), false);
    QVERIFY(item.paintNode);
    QCOMPARE(item.paintNode->mipmapping(), true);

    item.setMipmap(true);
    QCOMPARE(item.mipmap(), true);
    QCOMPARE(hasDirtyContentFlag(&item), false);

    item.setMipmap(false);
    QCOMPARE(item.mipmap(), false);
    QCOMPARE(hasDirtyContentFlag(&item), true);

    QTRY_COMPARE(hasDirtyContentFlag(&item), false);
    QVERIFY(item.paintNode);
    QCOMPARE(item.paintNode->mipmapping(), false);
}

void tst_QQuickPaintedItem::performanceHints()
{
    TestPaintedItem item;
    item.setSize(QSizeF(320, 240));
    item.setParentItem(window.contentItem());

    QCOMPARE(item.performanceHints(), QQuickPaintedItem::PerformanceHints());

    item.setPerformanceHints(QQuickPaintedItem::PerformanceHints());
    QCOMPARE(item.performanceHints(), QQuickPaintedItem::PerformanceHints());
    QCOMPARE(hasDirtyContentFlag(&item), false);

    item.update();
    QTRY_COMPARE(hasDirtyContentFlag(&item), false);
    QVERIFY(item.paintNode);
    QCOMPARE(item.paintNode->fastFBOResizing(), false);

    item.setPerformanceHints(QQuickPaintedItem::PerformanceHints(QQuickPaintedItem::FastFBOResizing));
    QCOMPARE(item.performanceHints(), QQuickPaintedItem::PerformanceHints(QQuickPaintedItem::FastFBOResizing));
    QCOMPARE(hasDirtyContentFlag(&item), true);

    QTRY_COMPARE(hasDirtyContentFlag(&item), false);
    QVERIFY(item.paintNode);
    QCOMPARE(item.paintNode->fastFBOResizing(), true);

    item.setPerformanceHint(QQuickPaintedItem::FastFBOResizing, true);
    QCOMPARE(item.performanceHints(), QQuickPaintedItem::PerformanceHints(QQuickPaintedItem::FastFBOResizing));
    QCOMPARE(hasDirtyContentFlag(&item), false);

    item.setPerformanceHint(QQuickPaintedItem::FastFBOResizing, false);
    QCOMPARE(item.performanceHints(), QQuickPaintedItem::PerformanceHints());
    QCOMPARE(hasDirtyContentFlag(&item), true);

    QTRY_COMPARE(hasDirtyContentFlag(&item), false);
    QVERIFY(item.paintNode);
    QCOMPARE(item.paintNode->fastFBOResizing(), false);
}

void tst_QQuickPaintedItem::contentsSize()
{
    TestPaintedItem item;

    QSignalSpy spy(&item, SIGNAL(contentsSizeChanged()));

    QCOMPARE(item.contentsSize(), QSize());

    item.setContentsSize(QSize());
    QCOMPARE(item.contentsSize(), QSize());
    QCOMPARE(hasDirtyContentFlag(&item), false);
    QCOMPARE(spy.count(), 0);

    item.setContentsSize(QSize(320, 240));
    QCOMPARE(item.contentsSize(), QSize(320, 240));
    QCOMPARE(hasDirtyContentFlag(&item), true);
    QCOMPARE(spy.count(), 1);

    clearDirtyContentFlag(&item);

    item.setContentsSize(QSize(320, 240));
    QCOMPARE(item.contentsSize(), QSize(320, 240));
    QCOMPARE(hasDirtyContentFlag(&item), false);
    QCOMPARE(spy.count(), 1);

    item.resetContentsSize();
    QCOMPARE(item.contentsSize(), QSize());
    QCOMPARE(hasDirtyContentFlag(&item), true);
    QCOMPARE(spy.count(), 2);
}

void tst_QQuickPaintedItem::contentScale()
{
    TestPaintedItem item;
    item.setSize(QSizeF(320, 240));
    item.setParentItem(window.contentItem());

    QSignalSpy spy(&item, SIGNAL(contentsScaleChanged()));

    QCOMPARE(item.contentsScale(), 1.);

    item.setContentsScale(1.);
    QCOMPARE(item.contentsScale(), 1.);
    QCOMPARE(hasDirtyContentFlag(&item), false);
    QCOMPARE(spy.count(), 0);

    item.update();
    QTRY_COMPARE(hasDirtyContentFlag(&item), false);
    QVERIFY(item.paintNode);
    QCOMPARE(item.paintNode->contentsScale(), 1.0);

    item.setContentsScale(0.4);
    QCOMPARE(item.contentsScale(), 0.4);
    QCOMPARE(hasDirtyContentFlag(&item), true);
    QCOMPARE(spy.count(), 1);

    QTRY_COMPARE(hasDirtyContentFlag(&item), false);
    QVERIFY(item.paintNode);
    QCOMPARE(item.paintNode->contentsScale(), 0.4);

    item.setContentsScale(0.4);
    QCOMPARE(item.contentsScale(), 0.4);
    QCOMPARE(hasDirtyContentFlag(&item), false);
    QCOMPARE(spy.count(), 1);

    item.setContentsScale(2.5);
    QCOMPARE(item.contentsScale(), 2.5);
    QCOMPARE(hasDirtyContentFlag(&item), true);
    QCOMPARE(spy.count(), 2);

    QTRY_COMPARE(hasDirtyContentFlag(&item), false);
    QVERIFY(item.paintNode);
    QCOMPARE(item.paintNode->contentsScale(), 2.5);
}

void tst_QQuickPaintedItem::contentsBoundingRect()
{
    TestPaintedItem item;
    item.setSize(QSizeF(320, 240));
    item.setParentItem(window.contentItem());

    QCOMPARE(item.contentsBoundingRect(), QRectF(0, 0, 320, 240));

    item.setContentsSize(QSize(500, 500));
    QCOMPARE(item.contentsBoundingRect(), QRectF(0, 0, 500, 500));

    QTRY_COMPARE(hasDirtyContentFlag(&item), false);
    QVERIFY(item.paintNode);
    QCOMPARE(item.paintNode->size(), QSize(500, 500));

    item.setContentsScale(0.5);
    QCOMPARE(item.contentsBoundingRect(), QRectF(0, 0, 320, 250));

    QTRY_COMPARE(hasDirtyContentFlag(&item), false);
    QVERIFY(item.paintNode);
    QCOMPARE(item.paintNode->size(), QSize(320, 250));

    item.setContentsSize(QSize(150, 150));
    QCOMPARE(item.contentsBoundingRect(), QRectF(0, 0, 320, 240));

    QTRY_COMPARE(hasDirtyContentFlag(&item), false);
    QVERIFY(item.paintNode);
    QCOMPARE(item.paintNode->size(), QSize(320, 240));

    item.setContentsScale(2.0);
    QCOMPARE(item.contentsBoundingRect(), QRectF(0, 0, 320, 300));

    QTRY_COMPARE(hasDirtyContentFlag(&item), false);
    QVERIFY(item.paintNode);
    QCOMPARE(item.paintNode->size(), QSize(320, 300));

}

void tst_QQuickPaintedItem::fillColor()
{
    TestPaintedItem item;
    item.setSize(QSizeF(320, 240));
    item.setParentItem(window.contentItem());

    QSignalSpy spy(&item, SIGNAL(fillColorChanged()));

    QCOMPARE(item.fillColor(), QColor(Qt::transparent));

    item.setFillColor(QColor(Qt::transparent));
    QCOMPARE(item.fillColor(), QColor(Qt::transparent));
    QCOMPARE(hasDirtyContentFlag(&item), false);
    QCOMPARE(spy.count(), 0);

    item.update();
    QTRY_COMPARE(hasDirtyContentFlag(&item), false);
    QVERIFY(item.paintNode);
    QCOMPARE(item.paintNode->fillColor(), QColor(Qt::transparent));

    item.setFillColor(QColor(Qt::green));
    QCOMPARE(item.fillColor(), QColor(Qt::green));
    QCOMPARE(hasDirtyContentFlag(&item), true);
    QCOMPARE(spy.count(), 1);

    QTRY_COMPARE(hasDirtyContentFlag(&item), false);
    QVERIFY(item.paintNode);
    QCOMPARE(item.paintNode->fillColor(), QColor(Qt::green));

    item.setFillColor(QColor(Qt::green));
    QCOMPARE(item.fillColor(), QColor(Qt::green));
    QCOMPARE(hasDirtyContentFlag(&item), false);
    QCOMPARE(spy.count(), 1);

    item.setFillColor(QColor(Qt::blue));
    QCOMPARE(item.fillColor(), QColor(Qt::blue));
    QCOMPARE(hasDirtyContentFlag(&item), true);
    QCOMPARE(spy.count(), 2);

    QTRY_COMPARE(hasDirtyContentFlag(&item), false);
    QVERIFY(item.paintNode);
    QCOMPARE(item.paintNode->fillColor(), QColor(Qt::blue));

}

void tst_QQuickPaintedItem::renderTarget()
{
    TestPaintedItem item;

    QSignalSpy spy(&item, SIGNAL(renderTargetChanged()));

    QCOMPARE(item.renderTarget(), QQuickPaintedItem::Image);

    item.setRenderTarget(QQuickPaintedItem::Image);
    QCOMPARE(item.renderTarget(), QQuickPaintedItem::Image);
    QCOMPARE(hasDirtyContentFlag(&item), false);
    QCOMPARE(spy.count(), 0);

    item.setRenderTarget(QQuickPaintedItem::FramebufferObject);
    QCOMPARE(item.renderTarget(), QQuickPaintedItem::FramebufferObject);
    QCOMPARE(hasDirtyContentFlag(&item), true);
    QCOMPARE(spy.count(), 1);

    clearDirtyContentFlag(&item);

    item.setRenderTarget(QQuickPaintedItem::FramebufferObject);
    QCOMPARE(item.renderTarget(), QQuickPaintedItem::FramebufferObject);
    QCOMPARE(hasDirtyContentFlag(&item), false);
    QCOMPARE(spy.count(), 1);

    item.setRenderTarget(QQuickPaintedItem::InvertedYFramebufferObject);
    QCOMPARE(item.renderTarget(), QQuickPaintedItem::InvertedYFramebufferObject);
    QCOMPARE(hasDirtyContentFlag(&item), true);
    QCOMPARE(spy.count(), 2);
}

QTEST_MAIN(tst_QQuickPaintedItem)

#include "tst_qquickpainteditem.moc"
