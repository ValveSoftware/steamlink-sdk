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
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQuick/private/qquickpath_p.h>

#include "../../shared/util.h"

class tst_QuickPath : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_QuickPath() {}

private slots:
    void arc();
    void catmullromCurve();
    void closedCatmullromCurve();
    void svg();
    void line();
};

void tst_QuickPath::arc()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("arc.qml"));
    QQuickPath *obj = qobject_cast<QQuickPath*>(c.create());
    QVERIFY(obj != 0);

    QCOMPARE(obj->startX(), 0.);
    QCOMPARE(obj->startY(), 0.);

    QQmlListReference list(obj, "pathElements");
    QCOMPARE(list.count(), 1);

    QQuickPathArc* arc = qobject_cast<QQuickPathArc*>(list.at(0));
    QVERIFY(arc != 0);
    QCOMPARE(arc->x(), 100.);
    QCOMPARE(arc->y(), 100.);
    QCOMPARE(arc->radiusX(), 100.);
    QCOMPARE(arc->radiusY(), 100.);
    QCOMPARE(arc->useLargeArc(), false);
    QCOMPARE(arc->direction(), QQuickPathArc::Clockwise);

    QPainterPath path = obj->path();
    QVERIFY(path != QPainterPath());

    QPointF pos = obj->pointAt(0);
    QCOMPARE(pos, QPointF(0,0));
    pos = obj->pointAt(.25);
    QCOMPARE(pos.toPoint(), QPoint(39,8));  //fuzzy compare
    pos = obj->pointAt(.75);
    QCOMPARE(pos.toPoint(), QPoint(92,61)); //fuzzy compare
    pos = obj->pointAt(1);
    QCOMPARE(pos, QPointF(100,100));
}

void tst_QuickPath::catmullromCurve()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("curve.qml"));
    QQuickPath *obj = qobject_cast<QQuickPath*>(c.create());
    QVERIFY(obj != 0);

    QCOMPARE(obj->startX(), 0.);
    QCOMPARE(obj->startY(), 0.);

    QQmlListReference list(obj, "pathElements");
    QCOMPARE(list.count(), 3);

    QQuickPathCatmullRomCurve* curve = qobject_cast<QQuickPathCatmullRomCurve*>(list.at(0));
    QVERIFY(curve != 0);
    QCOMPARE(curve->x(), 100.);
    QCOMPARE(curve->y(), 50.);

    curve = qobject_cast<QQuickPathCatmullRomCurve*>(list.at(2));
    QVERIFY(curve != 0);
    QCOMPARE(curve->x(), 100.);
    QCOMPARE(curve->y(), 150.);

    QPainterPath path = obj->path();
    QVERIFY(path != QPainterPath());

    QPointF pos = obj->pointAt(0);
    QCOMPARE(pos, QPointF(0,0));
    pos = obj->pointAt(.25);
    QCOMPARE(pos.toPoint(), QPoint(63,26));  //fuzzy compare
    pos = obj->pointAt(.75);
    QCOMPARE(pos.toPoint(), QPoint(51,105)); //fuzzy compare
    pos = obj->pointAt(1);
    QCOMPARE(pos.toPoint(), QPoint(100,150));
}

void tst_QuickPath::closedCatmullromCurve()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("closedcurve.qml"));
    QQuickPath *obj = qobject_cast<QQuickPath*>(c.create());
    QVERIFY(obj != 0);

    QCOMPARE(obj->startX(), 50.);
    QCOMPARE(obj->startY(), 50.);

    QQmlListReference list(obj, "pathElements");
    QCOMPARE(list.count(), 3);

    QQuickPathCatmullRomCurve* curve = qobject_cast<QQuickPathCatmullRomCurve*>(list.at(2));
    QVERIFY(curve != 0);
    QCOMPARE(curve->x(), 50.);
    QCOMPARE(curve->y(), 50.);

    QVERIFY(obj->isClosed());

    QPainterPath path = obj->path();
    QVERIFY(path != QPainterPath());

    QPointF pos = obj->pointAt(0);
    QCOMPARE(pos, QPointF(50,50));
    pos = obj->pointAt(.1);
    QCOMPARE(pos.toPoint(), QPoint(67,56));  //fuzzy compare
    pos = obj->pointAt(.75);
    QCOMPARE(pos.toPoint(), QPoint(44,116)); //fuzzy compare
    pos = obj->pointAt(1);
    QCOMPARE(pos, QPointF(50,50));
}

void tst_QuickPath::svg()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("svg.qml"));
    QQuickPath *obj = qobject_cast<QQuickPath*>(c.create());
    QVERIFY(obj != 0);

    QCOMPARE(obj->startX(), 0.);
    QCOMPARE(obj->startY(), 0.);

    QQmlListReference list(obj, "pathElements");
    QCOMPARE(list.count(), 1);

    QQuickPathSvg* svg = qobject_cast<QQuickPathSvg*>(list.at(0));
    QVERIFY(svg != 0);
    QCOMPARE(svg->path(), QLatin1String("M200,300 Q400,50 600,300 T1000,300"));

    QPainterPath path = obj->path();
    QVERIFY(path != QPainterPath());

    QPointF pos = obj->pointAt(0);
    QCOMPARE(pos, QPointF(200,300));
    pos = obj->pointAt(.25);
    QCOMPARE(pos.toPoint(), QPoint(400,175));  //fuzzy compare
    pos = obj->pointAt(.75);
    QCOMPARE(pos.toPoint(), QPoint(800,425)); //fuzzy compare
    pos = obj->pointAt(1);
    QCOMPARE(pos, QPointF(1000,300));
}

void tst_QuickPath::line()
{
    QQmlEngine engine;
    QQmlComponent c1(&engine);
    c1.setData(
            "import QtQuick 2.0\n"
            "Path {\n"
                "startX: 0; startY: 0\n"
                "PathLine { x: 100; y: 100 }\n"
            "}", QUrl());
    QScopedPointer<QObject> o1(c1.create());
    QQuickPath *path1 = qobject_cast<QQuickPath *>(o1.data());
    QVERIFY(path1);

    QQmlComponent c2(&engine);
    c2.setData(
            "import QtQuick 2.0\n"
            "Path {\n"
                "startX: 0; startY: 0\n"
                "PathLine { x: 50; y: 50 }\n"
                "PathLine { x: 100; y: 100 }\n"
            "}", QUrl());
    QScopedPointer<QObject> o2(c2.create());
    QQuickPath *path2 = qobject_cast<QQuickPath *>(o2.data());
    QVERIFY(path2);

    for (int i = 0; i < 167; ++i) {
        qreal t = i / 167.0;

        QPointF p1 = path1->pointAt(t);
        QCOMPARE(p1.x(), p1.y());

        QPointF p2 = path2->pointAt(t);
        QCOMPARE(p1.toPoint(), p2.toPoint());
    }
}


QTEST_MAIN(tst_QuickPath)

#include "tst_qquickpath.moc"
