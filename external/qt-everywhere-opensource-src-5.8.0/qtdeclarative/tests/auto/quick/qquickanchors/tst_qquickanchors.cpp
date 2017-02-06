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
#include <QSignalSpy>
#include <private/qquickitem_p.h>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQuick/qquickview.h>
#include <QtQuick/private/qquickrectangle_p.h>
#include <QtQuick/private/qquicktext_p.h>
#include <QtQuick/private/qquickanchors_p_p.h>
#include <QtQuick/private/qquickitem_p.h>
#include "../../shared/util.h"
#include "../shared/visualtestutil.h"

Q_DECLARE_METATYPE(QQuickAnchors::Anchor)

using namespace QQuickVisualTestUtil;

class tst_qquickanchors : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qquickanchors() {}

private slots:
    void basicAnchors();
    void basicAnchorsRTL();
    void loops();
    void illegalSets();
    void illegalSets_data();
    void reset();
    void reset_data();
    void resetConvenience();
    void nullItem();
    void nullItem_data();
    void crash1();
    void centerIn();
    void centerInRTL();
    void centerInRotation();
    void hvCenter();
    void hvCenterRTL();
    void fill();
    void fillRTL();
    void margins_data();
    void margins();
    void marginsRTL_data() { margins_data(); }
    void marginsRTL();
    void stretch();
    void baselineOffset();
};

void tst_qquickanchors::basicAnchors()
{
    QQuickView *view = new QQuickView;
    view->setSource(testFileUrl("anchors.qml"));

    qApp->processEvents();

    //sibling horizontal
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect1"))->x(), 26.0);
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect2"))->x(), 122.0);
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect3"))->x(), 74.0);
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect4"))->x(), 16.0);
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect5"))->x(), 112.0);
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect6"))->x(), 64.0);

    //parent horizontal
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect7"))->x(), 0.0);
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect8"))->x(), 240.0);
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect9"))->x(), 120.0);
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect10"))->x(), -10.0);
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect11"))->x(), 230.0);
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect12"))->x(), 110.0);

    //vertical
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect13"))->y(), 20.0);
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect14"))->y(), 155.0);

    //stretch
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect15"))->x(), 26.0);
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect15"))->width(), 96.0);
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect16"))->x(), 26.0);
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect16"))->width(), 192.0);
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect17"))->x(), -70.0);
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect17"))->width(), 192.0);

    //vertical stretch
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect18"))->y(), 20.0);
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect18"))->height(), 40.0);

    //more parent horizontal
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect19"))->x(), 115.0);
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect20"))->x(), 235.0);
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect21"))->x(), -5.0);

    //centerIn
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect22"))->x(), 69.0);
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect22"))->y(), 5.0);

     //margins
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect23"))->x(), 31.0);
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect23"))->y(), 5.0);
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect23"))->width(), 86.0);
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect23"))->height(), 10.0);

    // offsets
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect24"))->x(), 26.0);
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect25"))->y(), 60.0);
    QCOMPARE(findItem<QQuickRectangle>(view->rootObject(), QLatin1String("rect26"))->y(), 5.0);

    //baseline
    QQuickText *text1 = findItem<QQuickText>(view->rootObject(), QLatin1String("text1"));
    QQuickText *text2 = findItem<QQuickText>(view->rootObject(), QLatin1String("text2"));
    QCOMPARE(text1->y(), text2->y());

    delete view;
}

QQuickItem* childItem(QQuickItem *parentItem, const char * itemString) {
    return findItem<QQuickItem>(parentItem, QLatin1String(itemString));
}

qreal offsetMasterRTL(QQuickItem *rootItem, const char * itemString) {
    QQuickItem* masterItem = findItem<QQuickItem>(rootItem,  QLatin1String("masterRect"));
    return masterItem->width()+2*masterItem->x()-findItem<QQuickItem>(rootItem,  QLatin1String(itemString))->width();
}

qreal offsetParentRTL(QQuickItem *rootItem, const char * itemString) {
    return rootItem->width()+2*rootItem->x()-findItem<QQuickItem>(rootItem,  QLatin1String(itemString))->width();
}

void mirrorAnchors(QQuickItem *item) {
    QQuickItemPrivate *itemPrivate = QQuickItemPrivate::get(item);
    itemPrivate->setLayoutMirror(true);
}

void tst_qquickanchors::basicAnchorsRTL()
{
    QQuickView *view = new QQuickView;
    view->setSource(testFileUrl("anchors.qml"));

    qApp->processEvents();

    QQuickItem* rootItem = qobject_cast<QQuickItem*>(view->rootObject());
    foreach (QObject *child, rootItem->children()) {
        bool mirrored = QQuickItemPrivate::get(qobject_cast<QQuickItem*>(child))->anchors()->mirrored();
        QCOMPARE(mirrored, false);
    }

    foreach (QObject *child, rootItem->children())
        mirrorAnchors(qobject_cast<QQuickItem*>(child));

    foreach (QObject *child, rootItem->children()) {
        bool mirrored = QQuickItemPrivate::get(qobject_cast<QQuickItem*>(child))->anchors()->mirrored();
        QCOMPARE(mirrored, true);
    }

    //sibling horizontal
    QCOMPARE(childItem(rootItem, "rect1")->x(), offsetMasterRTL(rootItem, "rect1")-26.0);
    QCOMPARE(childItem(rootItem, "rect2")->x(), offsetMasterRTL(rootItem, "rect2")-122.0);
    QCOMPARE(childItem(rootItem, "rect3")->x(), offsetMasterRTL(rootItem, "rect3")-74.0);
    QCOMPARE(childItem(rootItem, "rect4")->x(), offsetMasterRTL(rootItem, "rect4")-16.0);
    QCOMPARE(childItem(rootItem, "rect5")->x(), offsetMasterRTL(rootItem, "rect5")-112.0);
    QCOMPARE(childItem(rootItem, "rect6")->x(), offsetMasterRTL(rootItem, "rect6")-64.0);

    //parent horizontal
    QCOMPARE(childItem(rootItem, "rect7")->x(), offsetParentRTL(rootItem, "rect7")-0.0);
    QCOMPARE(childItem(rootItem, "rect8")->x(), offsetParentRTL(rootItem, "rect8")-240.0);
    QCOMPARE(childItem(rootItem, "rect9")->x(), offsetParentRTL(rootItem, "rect9")-120.0);
    QCOMPARE(childItem(rootItem, "rect10")->x(), offsetParentRTL(rootItem, "rect10")+10.0);
    QCOMPARE(childItem(rootItem, "rect11")->x(), offsetParentRTL(rootItem, "rect11")-230.0);
    QCOMPARE(childItem(rootItem, "rect12")->x(), offsetParentRTL(rootItem, "rect12")-110.0);

    //vertical
    QCOMPARE(childItem(rootItem, "rect13")->y(), 20.0);
    QCOMPARE(childItem(rootItem, "rect14")->y(), 155.0);

    //stretch
    QCOMPARE(childItem(rootItem, "rect15")->x(), offsetMasterRTL(rootItem, "rect15")-26.0);
    QCOMPARE(childItem(rootItem, "rect15")->width(), 96.0);
    QCOMPARE(childItem(rootItem, "rect16")->x(), offsetMasterRTL(rootItem, "rect16")-26.0);
    QCOMPARE(childItem(rootItem, "rect16")->width(), 192.0);
    QCOMPARE(childItem(rootItem, "rect17")->x(), offsetMasterRTL(rootItem, "rect17")+70.0);
    QCOMPARE(childItem(rootItem, "rect17")->width(), 192.0);

    //vertical stretch
    QCOMPARE(childItem(rootItem, "rect18")->y(), 20.0);
    QCOMPARE(childItem(rootItem, "rect18")->height(), 40.0);

    //more parent horizontal
    QCOMPARE(childItem(rootItem, "rect19")->x(), offsetParentRTL(rootItem, "rect19")-115.0);
    QCOMPARE(childItem(rootItem, "rect20")->x(), offsetParentRTL(rootItem, "rect20")-235.0);
    QCOMPARE(childItem(rootItem, "rect21")->x(), offsetParentRTL(rootItem, "rect21")+5.0);

    //centerIn
    QCOMPARE(childItem(rootItem, "rect22")->x(), offsetMasterRTL(rootItem, "rect22")-69.0);
    QCOMPARE(childItem(rootItem, "rect22")->y(), 5.0);

     //margins
    QCOMPARE(childItem(rootItem, "rect23")->x(), offsetMasterRTL(rootItem, "rect23")-31.0);
    QCOMPARE(childItem(rootItem, "rect23")->y(), 5.0);
    QCOMPARE(childItem(rootItem, "rect23")->width(), 86.0);
    QCOMPARE(childItem(rootItem, "rect23")->height(), 10.0);

    // offsets
    QCOMPARE(childItem(rootItem, "rect24")->x(), offsetMasterRTL(rootItem, "rect24")-26.0);
    QCOMPARE(childItem(rootItem, "rect25")->y(), 60.0);
    QCOMPARE(childItem(rootItem, "rect26")->y(), 5.0);

    //baseline
    QQuickText *text1 = findItem<QQuickText>(rootItem, QLatin1String("text1"));
    QQuickText *text2 = findItem<QQuickText>(rootItem, QLatin1String("text2"));
    QCOMPARE(text1->y(), text2->y());

    delete view;
}

// mostly testing that we don't crash
void tst_qquickanchors::loops()
{
    {
        QUrl source(testFileUrl("loop1.qml"));

        QString expect = source.toString() + ":6:5: QML Text: Possible anchor loop detected on horizontal anchor.";
        QTest::ignoreMessage(QtWarningMsg, expect.toLatin1());
        QTest::ignoreMessage(QtWarningMsg, expect.toLatin1());

        QQuickView *view = new QQuickView;
        view->setSource(source);
        qApp->processEvents();

        delete view;
    }

    {
        QUrl source(testFileUrl("loop2.qml"));

        QString expect = source.toString() + ":8:3: QML Image: Possible anchor loop detected on horizontal anchor.";
        QTest::ignoreMessage(QtWarningMsg, expect.toLatin1());

        QQuickView *view = new QQuickView;
        view->setSource(source);
        qApp->processEvents();

        delete view;
    }
}

void tst_qquickanchors::illegalSets()
{
    QFETCH(QString, qml);
    QFETCH(QString, warning);

    QTest::ignoreMessage(QtWarningMsg, warning.toLatin1());

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(QByteArray("import QtQuick 2.0\n" + qml.toUtf8()), QUrl::fromLocalFile(""));
    if (!component.isReady())
        qWarning() << "Test errors:" << component.errors();
    QVERIFY(component.isReady());
    QObject *o = component.create();
    delete o;
}

void tst_qquickanchors::illegalSets_data()
{
    QTest::addColumn<QString>("qml");
    QTest::addColumn<QString>("warning");

    QTest::newRow("H - too many anchors")
        << "Rectangle { id: rect; Rectangle { anchors.left: rect.left; anchors.right: rect.right; anchors.horizontalCenter: rect.horizontalCenter } }"
        << "<Unknown File>:2:23: QML Rectangle: Cannot specify left, right, and horizontalCenter anchors at the same time.";

    foreach (const QString &side, QStringList() << "left" << "right") {
        QTest::newRow("H - anchor to V")
            << QString("Rectangle { Rectangle { anchors.%1: parent.top } }").arg(side)
            << "<Unknown File>:2:13: QML Rectangle: Cannot anchor a horizontal edge to a vertical edge.";

        QTest::newRow("H - anchor to non parent/sibling")
            << QString("Rectangle { Item { Rectangle { id: rect } } Rectangle { anchors.%1: rect.%1 } }").arg(side)
            << "<Unknown File>:2:45: QML Rectangle: Cannot anchor to an item that isn't a parent or sibling.";

        QTest::newRow("H - anchor to self")
            << QString("Rectangle { id: rect; anchors.%1: rect.%1 }").arg(side)
            << "<Unknown File>:2:1: QML Rectangle: Cannot anchor item to self.";
    }


    QTest::newRow("V - too many anchors")
        << "Rectangle { id: rect; Rectangle { anchors.top: rect.top; anchors.bottom: rect.bottom; anchors.verticalCenter: rect.verticalCenter } }"
        << "<Unknown File>:2:23: QML Rectangle: Cannot specify top, bottom, and verticalCenter anchors at the same time.";

    QTest::newRow("V - too many anchors with baseline")
        << "Rectangle { Text { id: text1; text: \"Hello\" } Text { anchors.baseline: text1.baseline; anchors.top: text1.top; } }"
        << "<Unknown File>:2:47: QML Text: Baseline anchor cannot be used in conjunction with top, bottom, or verticalCenter anchors.";

    foreach (const QString &side, QStringList() << "top" << "bottom" << "baseline") {

        QTest::newRow("V - anchor to H")
            << QString("Rectangle { Rectangle { anchors.%1: parent.left } }").arg(side)
            << "<Unknown File>:2:13: QML Rectangle: Cannot anchor a vertical edge to a horizontal edge.";

        QTest::newRow("V - anchor to non parent/sibling")
            << QString("Rectangle { Item { Rectangle { id: rect } } Rectangle { anchors.%1: rect.%1 } }").arg(side)
            << "<Unknown File>:2:45: QML Rectangle: Cannot anchor to an item that isn't a parent or sibling.";

        QTest::newRow("V - anchor to self")
            << QString("Rectangle { id: rect; anchors.%1: rect.%1 }").arg(side)
            << "<Unknown File>:2:1: QML Rectangle: Cannot anchor item to self.";
    }


    QTest::newRow("centerIn - anchor to non parent/sibling")
        << "Rectangle { Item { Rectangle { id: rect } } Rectangle { anchors.centerIn: rect} }"
        << "<Unknown File>:2:45: QML Rectangle: Cannot anchor to an item that isn't a parent or sibling.";


    QTest::newRow("fill - anchor to non parent/sibling")
        << "Rectangle { Item { Rectangle { id: rect } } Rectangle { anchors.fill: rect} }"
        << "<Unknown File>:2:45: QML Rectangle: Cannot anchor to an item that isn't a parent or sibling.";
}

void tst_qquickanchors::reset()
{
    QFETCH(QString, side);
    QFETCH(QQuickAnchors::Anchor, anchor);

    QQuickItem *baseItem = new QQuickItem;

    QQuickAnchorLine anchorLine;
    anchorLine.item = baseItem;
    anchorLine.anchorLine = anchor;

    QQuickItem *item = new QQuickItem;
    QQuickItemPrivate *itemPrivate = QQuickItemPrivate::get(item);

    const QMetaObject *meta = itemPrivate->anchors()->metaObject();
    QMetaProperty p = meta->property(meta->indexOfProperty(side.toUtf8().constData()));

    QVERIFY(p.write(itemPrivate->anchors(), qVariantFromValue(anchorLine)));
    QCOMPARE(itemPrivate->anchors()->usedAnchors().testFlag(anchor), true);

    QVERIFY(p.reset(itemPrivate->anchors()));
    QCOMPARE(itemPrivate->anchors()->usedAnchors().testFlag(anchor), false);

    delete item;
    delete baseItem;
}

void tst_qquickanchors::reset_data()
{
    QTest::addColumn<QString>("side");
    QTest::addColumn<QQuickAnchors::Anchor>("anchor");

    QTest::newRow("left") << "left" << QQuickAnchors::LeftAnchor;
    QTest::newRow("top") << "top" << QQuickAnchors::TopAnchor;
    QTest::newRow("right") << "right" << QQuickAnchors::RightAnchor;
    QTest::newRow("bottom") << "bottom" << QQuickAnchors::BottomAnchor;

    QTest::newRow("hcenter") << "horizontalCenter" << QQuickAnchors::HCenterAnchor;
    QTest::newRow("vcenter") << "verticalCenter" << QQuickAnchors::VCenterAnchor;
    QTest::newRow("baseline") << "baseline" << QQuickAnchors::BaselineAnchor;
}

void tst_qquickanchors::resetConvenience()
{
    QQuickItem *baseItem = new QQuickItem;
    QQuickItem *item = new QQuickItem;
    QQuickItemPrivate *itemPrivate = QQuickItemPrivate::get(item);

    //fill
    itemPrivate->anchors()->setFill(baseItem);
    QCOMPARE(itemPrivate->anchors()->fill(), baseItem);
    itemPrivate->anchors()->resetFill();
    QVERIFY(!itemPrivate->anchors()->fill());

    //centerIn
    itemPrivate->anchors()->setCenterIn(baseItem);
    QCOMPARE(itemPrivate->anchors()->centerIn(), baseItem);
    itemPrivate->anchors()->resetCenterIn();
    QVERIFY(!itemPrivate->anchors()->centerIn());

    delete item;
    delete baseItem;
}

void tst_qquickanchors::nullItem()
{
    QFETCH(QString, side);

    QQuickAnchorLine anchor;
    QQuickItem *item = new QQuickItem;
    QQuickItemPrivate *itemPrivate = QQuickItemPrivate::get(item);

    const QMetaObject *meta = itemPrivate->anchors()->metaObject();
    QMetaProperty p = meta->property(meta->indexOfProperty(side.toUtf8().constData()));

    QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML Item: Cannot anchor to a null item.");
    QVERIFY(p.write(itemPrivate->anchors(), qVariantFromValue(anchor)));

    delete item;
}

void tst_qquickanchors::nullItem_data()
{
    QTest::addColumn<QString>("side");

    QTest::newRow("left") << "left";
    QTest::newRow("top") << "top";
    QTest::newRow("right") << "right";
    QTest::newRow("bottom") << "bottom";

    QTest::newRow("hcenter") << "horizontalCenter";
    QTest::newRow("vcenter") << "verticalCenter";
    QTest::newRow("baseline") << "baseline";
}

//QTBUG-5428
void tst_qquickanchors::crash1()
{
    QUrl source(testFileUrl("crash1.qml"));

    QQuickView *view = new QQuickView(source);
    qApp->processEvents();

    delete view;
}

void tst_qquickanchors::fill()
{
    QQuickView *view = new QQuickView(testFileUrl("fill.qml"));

    qApp->processEvents();
    QQuickRectangle* rect = findItem<QQuickRectangle>(view->rootObject(), QLatin1String("filler"));
    QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);
    QCOMPARE(rectPrivate->anchors()->leftMargin(), 10.0);
    QCOMPARE(rectPrivate->anchors()->topMargin(), 30.0);
    QCOMPARE(rectPrivate->anchors()->rightMargin(), 20.0);
    QCOMPARE(rectPrivate->anchors()->bottomMargin(), 40.0);
    QCOMPARE(rect->x(), 0.0 + 10.0);
    QCOMPARE(rect->y(), 0.0 + 30.0);
    QCOMPARE(rect->width(), 200.0 - 10.0 - 20.0);
    QCOMPARE(rect->height(), 200.0 - 30.0 - 40.0);
    //Alter Offsets (tests QTBUG-6631)
    rectPrivate->anchors()->setLeftMargin(20.0);
    rectPrivate->anchors()->setRightMargin(0.0);
    rectPrivate->anchors()->setBottomMargin(0.0);
    rectPrivate->anchors()->setTopMargin(10.0);
    QCOMPARE(rectPrivate->anchors()->leftMargin(), 20.0);
    QCOMPARE(rectPrivate->anchors()->topMargin(), 10.0);
    QCOMPARE(rectPrivate->anchors()->rightMargin(), 0.0);
    QCOMPARE(rectPrivate->anchors()->bottomMargin(), 0.0);
    QCOMPARE(rect->x(), 0.0 + 20.0);
    QCOMPARE(rect->y(), 0.0 + 10.0);
    QCOMPARE(rect->width(), 200.0 - 20.0);
    QCOMPARE(rect->height(), 200.0 - 10.0);

    delete view;
}

void tst_qquickanchors::fillRTL()
{
    QQuickView *view = new QQuickView(testFileUrl("fill.qml"));

    qApp->processEvents();
    QQuickRectangle* rect = findItem<QQuickRectangle>(view->rootObject(), QLatin1String("filler"));
    QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);
    mirrorAnchors(rect);

    QCOMPARE(rect->x(), 0.0 + 20.0);
    QCOMPARE(rect->y(), 0.0 + 30.0);
    QCOMPARE(rect->width(), 200.0 - 10.0 - 20.0);
    QCOMPARE(rect->height(), 200.0 - 30.0 - 40.0);
    //Alter Offsets (tests QTBUG-6631)
    rectPrivate->anchors()->setLeftMargin(20.0);
    rectPrivate->anchors()->setRightMargin(0.0);
    rectPrivate->anchors()->setBottomMargin(0.0);
    rectPrivate->anchors()->setTopMargin(10.0);
    QCOMPARE(rect->x(), 0.0 + 0.0);
    QCOMPARE(rect->y(), 0.0 + 10.0);
    QCOMPARE(rect->width(), 200.0 - 20.0);
    QCOMPARE(rect->height(), 200.0 - 10.0);

    delete view;
}

void tst_qquickanchors::centerIn()
{
    QQuickView *view = new QQuickView(testFileUrl("centerin.qml"));

    qApp->processEvents();
    QQuickRectangle* rect = findItem<QQuickRectangle>(view->rootObject(), QLatin1String("centered"));
    QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);

    QCOMPARE(rectPrivate->anchors()->horizontalCenterOffset(), 10.0);
    QCOMPARE(rectPrivate->anchors()->verticalCenterOffset(), 30.0);
    QCOMPARE(rect->x(), 75.0 + 10);
    QCOMPARE(rect->y(), 75.0 + 30);
    //Alter Offsets (tests QTBUG-6631)
    rectPrivate->anchors()->setHorizontalCenterOffset(-20.0);
    rectPrivate->anchors()->setVerticalCenterOffset(-10.0);
    QCOMPARE(rectPrivate->anchors()->horizontalCenterOffset(), -20.0);
    QCOMPARE(rectPrivate->anchors()->verticalCenterOffset(), -10.0);
    QCOMPARE(rect->x(), 75.0 - 20.0);
    QCOMPARE(rect->y(), 75.0 - 10.0);

    // By default center aligned to pixel
    QQuickRectangle* rect2 = findItem<QQuickRectangle>(view->rootObject(), QLatin1String("centered2"));
    QCOMPARE(rect2->x(), 94.0);
    QCOMPARE(rect2->y(), 94.0);

    //QTBUG-21730 (use actual center to prevent animation jitter)
    QQuickRectangle* rect3 = findItem<QQuickRectangle>(view->rootObject(), QLatin1String("centered3"));
    QCOMPARE(rect3->x(), 94.5);
    QCOMPARE(rect3->y(), 94.5);

    delete view;
}

void tst_qquickanchors::centerInRTL()
{
    QQuickView *view = new QQuickView(testFileUrl("centerin.qml"));

    qApp->processEvents();
    QQuickRectangle* rect = findItem<QQuickRectangle>(view->rootObject(), QLatin1String("centered"));
    QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);
    mirrorAnchors(rect);

    QCOMPARE(rect->x(), 75.0 - 10);
    QCOMPARE(rect->y(), 75.0 + 30);
    //Alter Offsets (tests QTBUG-6631)
    rectPrivate->anchors()->setHorizontalCenterOffset(-20.0);
    rectPrivate->anchors()->setVerticalCenterOffset(-10.0);
    QCOMPARE(rect->x(), 75.0 + 20.0);
    QCOMPARE(rect->y(), 75.0 - 10.0);

    delete view;
}

//QTBUG-12441
void tst_qquickanchors::centerInRotation()
{
    QQuickView *view = new QQuickView(testFileUrl("centerinRotation.qml"));

    qApp->processEvents();
    QQuickRectangle* outer = findItem<QQuickRectangle>(view->rootObject(), QLatin1String("outer"));
    QQuickRectangle* inner = findItem<QQuickRectangle>(view->rootObject(), QLatin1String("inner"));

    QCOMPARE(outer->x(), qreal(49.5));
    QCOMPARE(outer->y(), qreal(49.5));
    QCOMPARE(inner->x(), qreal(25.5));
    QCOMPARE(inner->y(), qreal(25.5));

    delete view;
}

void tst_qquickanchors::hvCenter()
{
    QQuickView *view = new QQuickView(testFileUrl("hvCenter.qml"));

    qApp->processEvents();
    QQuickRectangle* rect = findItem<QQuickRectangle>(view->rootObject(), QLatin1String("centered"));
    QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);

    // test QTBUG-10999
    QCOMPARE(rect->x(), 10.0);
    QCOMPARE(rect->y(), 19.0);

    rectPrivate->anchors()->setHorizontalCenterOffset(-5.0);
    rectPrivate->anchors()->setVerticalCenterOffset(5.0);
    QCOMPARE(rect->x(), 10.0 - 5.0);
    QCOMPARE(rect->y(), 19.0 + 5.0);

    delete view;
}

void tst_qquickanchors::hvCenterRTL()
{
    QQuickView *view = new QQuickView(testFileUrl("hvCenter.qml"));

    qApp->processEvents();
    QQuickRectangle* rect = findItem<QQuickRectangle>(view->rootObject(), QLatin1String("centered"));
    QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);
    mirrorAnchors(rect);

    // test QTBUG-10999
    QCOMPARE(rect->x(), 10.0);
    QCOMPARE(rect->y(), 19.0);

    rectPrivate->anchors()->setHorizontalCenterOffset(-5.0);
    rectPrivate->anchors()->setVerticalCenterOffset(5.0);
    QCOMPARE(rect->x(), 10.0 + 5.0);
    QCOMPARE(rect->y(), 19.0 + 5.0);

    delete view;
}

void tst_qquickanchors::margins_data()
{
    QTest::addColumn<QUrl>("source");

    QTest::newRow("fill") << testFileUrl("margins.qml");
    QTest::newRow("individual") << testFileUrl("individualMargins.qml");
}

void tst_qquickanchors::margins()
{
    QFETCH(QUrl, source);
    QQuickView *view = new QQuickView(source);

    qApp->processEvents();
    QQuickRectangle* rect = findItem<QQuickRectangle>(view->rootObject(), QLatin1String("filler"));
    QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);
    QCOMPARE(rectPrivate->anchors()->margins(), 10.0);
    QCOMPARE(rectPrivate->anchors()->topMargin(), 6.0);
    QCOMPARE(rectPrivate->anchors()->leftMargin(), 5.0);
    QCOMPARE(rectPrivate->anchors()->bottomMargin(), 10.0);
    QCOMPARE(rectPrivate->anchors()->rightMargin(), 10.0);
    QCOMPARE(rect->x(), 5.0);
    QCOMPARE(rect->y(), 6.0);
    QCOMPARE(rect->width(), 200.0 - 5.0 - 10.0);
    QCOMPARE(rect->height(), 200.0 - 6.0 - 10.0);

    rectPrivate->anchors()->setTopMargin(0.0);
    rectPrivate->anchors()->setMargins(20.0);

    QCOMPARE(rectPrivate->anchors()->margins(), 20.0);
    QCOMPARE(rectPrivate->anchors()->topMargin(), 0.0);
    QCOMPARE(rectPrivate->anchors()->leftMargin(), 5.0);
    QCOMPARE(rectPrivate->anchors()->bottomMargin(), 20.0);
    QCOMPARE(rectPrivate->anchors()->rightMargin(), 20.0);
    QCOMPARE(rect->x(), 5.0);
    QCOMPARE(rect->y(), 0.0);
    QCOMPARE(rect->width(), 200.0 - 5.0 - 20.0);
    QCOMPARE(rect->height(), 200.0 - 0.0 - 20.0);

    rectPrivate->anchors()->setRightMargin(0.0);
    rectPrivate->anchors()->setBottomMargin(0.0);
    QCOMPARE(rectPrivate->anchors()->margins(), 20.0);
    QCOMPARE(rectPrivate->anchors()->topMargin(), 0.0);
    QCOMPARE(rectPrivate->anchors()->leftMargin(), 5.0);
    QCOMPARE(rectPrivate->anchors()->bottomMargin(), 0.0);
    QCOMPARE(rectPrivate->anchors()->rightMargin(), 0.0);
    QCOMPARE(rect->x(), 5.0);
    QCOMPARE(rect->y(), 0.0);
    QCOMPARE(rect->width(), 200.0 - 5.0 - 0.0);
    QCOMPARE(rect->height(), 200.0 - 0.0 - 0.0);

    // Test setting margins doesn't have any effect on individual margins with explicit values.
    rectPrivate->anchors()->setMargins(50.0);
    QCOMPARE(rectPrivate->anchors()->margins(), 50.0);
    QCOMPARE(rectPrivate->anchors()->leftMargin(), 5.0);
    QCOMPARE(rectPrivate->anchors()->topMargin(), 0.0);
    QCOMPARE(rectPrivate->anchors()->rightMargin(), 0.0);
    QCOMPARE(rectPrivate->anchors()->bottomMargin(), 0.0);
    QCOMPARE(rect->x(), 0.0 + 5.0);
    QCOMPARE(rect->y(), 0.0 + 0.0);
    QCOMPARE(rect->width(), 200.0 - 5.0 - 0.0);
    QCOMPARE(rect->height(), 200.0 - 0.0 - 0.0);

    // Test that individual margins that are reset have the same value as margins.
    rectPrivate->anchors()->resetLeftMargin();
    rectPrivate->anchors()->resetBottomMargin();
    QCOMPARE(rectPrivate->anchors()->leftMargin(), 50.0);
    QCOMPARE(rectPrivate->anchors()->topMargin(), 0.0);
    QCOMPARE(rectPrivate->anchors()->rightMargin(), 0.0);
    QCOMPARE(rectPrivate->anchors()->bottomMargin(), 50.0);
    QCOMPARE(rect->x(), 0.0 + 50.0);
    QCOMPARE(rect->y(), 0.0 + 0.0);
    QCOMPARE(rect->width(), 200.0 - 50.0 - 0.0);
    QCOMPARE(rect->height(), 200.0 - 0.0 - 50.0);

    rectPrivate->anchors()->setMargins(30.0);
    QCOMPARE(rectPrivate->anchors()->margins(), 30.0);
    QCOMPARE(rectPrivate->anchors()->leftMargin(), 30.0);
    QCOMPARE(rectPrivate->anchors()->topMargin(), 0.0);
    QCOMPARE(rectPrivate->anchors()->rightMargin(), 0.0);
    QCOMPARE(rectPrivate->anchors()->bottomMargin(), 30.0);
    QCOMPARE(rect->x(), 0.0 + 30.0);
    QCOMPARE(rect->y(), 0.0 + 0.0);
    QCOMPARE(rect->width(), 200.0 - 30.0 - 0.0);
    QCOMPARE(rect->height(), 200.0 - 0.0 - 30.0);

    rectPrivate->anchors()->resetTopMargin();
    rectPrivate->anchors()->resetRightMargin();
    QCOMPARE(rectPrivate->anchors()->leftMargin(), 30.0);
    QCOMPARE(rectPrivate->anchors()->topMargin(), 30.0);
    QCOMPARE(rectPrivate->anchors()->rightMargin(), 30.0);
    QCOMPARE(rectPrivate->anchors()->bottomMargin(), 30.0);
    QCOMPARE(rect->x(), 0.0 + 30.0);
    QCOMPARE(rect->y(), 0.0 + 30.0);
    QCOMPARE(rect->width(), 200.0 - 30.0 - 30.0);
    QCOMPARE(rect->height(), 200.0 - 30.0 - 30.0);

    rectPrivate->anchors()->setMargins(25.0);
    QCOMPARE(rectPrivate->anchors()->margins(), 25.0);
    QCOMPARE(rectPrivate->anchors()->leftMargin(), 25.0);
    QCOMPARE(rectPrivate->anchors()->topMargin(), 25.0);
    QCOMPARE(rectPrivate->anchors()->rightMargin(), 25.0);
    QCOMPARE(rectPrivate->anchors()->bottomMargin(), 25.0);
    QCOMPARE(rect->x(), 0.0 + 25.0);
    QCOMPARE(rect->y(), 0.0 + 25.0);
    QCOMPARE(rect->width(), 200.0 - 25.0 - 25.0);
    QCOMPARE(rect->height(), 200.0 - 25.0 - 25.0);

    delete view;
}

void tst_qquickanchors::marginsRTL()
{
    QFETCH(QUrl, source);
    QQuickView *view = new QQuickView(source);

    QQuickRectangle* rect = findItem<QQuickRectangle>(view->rootObject(), QLatin1String("filler"));
    QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);
    mirrorAnchors(rect);

    QCOMPARE(rect->x(), 10.0);
    QCOMPARE(rect->y(), 6.0);
    QCOMPARE(rect->width(), 200.0 - 5.0 - 10.0);
    QCOMPARE(rect->height(), 200.0 - 6.0 - 10.0);

    rectPrivate->anchors()->setTopMargin(0.0);
    rectPrivate->anchors()->setMargins(20.0);

    QCOMPARE(rect->x(), 20.0);
    QCOMPARE(rect->y(), 0.0);
    QCOMPARE(rect->width(), 200.0 - 5.0 - 20.0);
    QCOMPARE(rect->height(), 200.0 - 0.0 - 20.0);

    delete view;
}

void tst_qquickanchors::stretch()
{
    QQuickView *view = new QQuickView(testFileUrl("stretch.qml"));

    qApp->processEvents();
    QQuickRectangle* rect = findItem<QQuickRectangle>(view->rootObject(), QLatin1String("stretcher"));
    QCOMPARE(rect->x(), 160.0);
    QCOMPARE(rect->y(), 130.0);
    QCOMPARE(rect->width(), 40.0);
    QCOMPARE(rect->height(), 100.0);

    QQuickRectangle* rect2 = findItem<QQuickRectangle>(view->rootObject(), QLatin1String("stretcher2"));
    QCOMPARE(rect2->y(), 130.0);
    QCOMPARE(rect2->height(), 100.0);

    delete view;
}

void tst_qquickanchors::baselineOffset()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("baselineOffset.qml"));
    QScopedPointer<QObject> object(component.create());

    QQuickItem *item = qobject_cast<QQuickItem  *>(object.data());
    QVERIFY(item);

    QQuickItem *anchoredItem = findItem<QQuickItem>(item, QLatin1String("baselineAnchored"));

    QCOMPARE(anchoredItem->baselineOffset(), 0.0);
    QCOMPARE(anchoredItem->y(), 100.0);

    anchoredItem->setBaselineOffset(5);
    QCOMPARE(anchoredItem->baselineOffset(), 5.0);
    QCOMPARE(anchoredItem->y(), 95.0);

    anchoredItem->setBaselineOffset(10);
    QCOMPARE(anchoredItem->baselineOffset(), 10.0);
    QCOMPARE(anchoredItem->y(), 90.0);
}

QTEST_MAIN(tst_qquickanchors)

#include "tst_qquickanchors.moc"
