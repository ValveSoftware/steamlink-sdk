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
#include <QtTest/QSignalSpy>
#include <QtQml/qqmlcomponent.h>
#include <QtQml/qqmlcontext.h>
#include <QtQuick/qquickview.h>
#include <QtQuick/qquickitem.h>
#include "../../shared/util.h"
#include <QtGui/QWindow>
#include <QtCore/QDebug>
#include <QtQml/qqmlengine.h>

class SizeChangesListener : public QObject, public QVector<QSize>
{
    Q_OBJECT
public:
    explicit SizeChangesListener(QQuickItem *item);
private slots:
    void onSizeChanged();
private:
    QQuickItem *item;

};

SizeChangesListener::SizeChangesListener(QQuickItem *item) :
    item(item)
{
    connect(item, &QQuickItem::widthChanged, this, &SizeChangesListener::onSizeChanged);
    connect(item, &QQuickItem::heightChanged, this, &SizeChangesListener::onSizeChanged);
}

void SizeChangesListener::onSizeChanged()
{
    append(QSize(item->width(), item->height()));
}

class tst_QQuickView : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_QQuickView();

private slots:
    void resizemodeitem();
    void errors();
    void engine();
};


tst_QQuickView::tst_QQuickView()
{
}

void tst_QQuickView::resizemodeitem()
{
    QWindow window;
    window.setGeometry(0, 0, 400, 400);

    QQuickView *view = new QQuickView(&window);
    QVERIFY(view);
    view->setResizeMode(QQuickView::SizeRootObjectToView);
    QCOMPARE(QSize(0,0), view->initialSize());
    view->setSource(testFileUrl("resizemodeitem.qml"));
    QQuickItem* item = qobject_cast<QQuickItem*>(view->rootObject());
    QVERIFY(item);
    window.show();

    view->showNormal();

    // initial size from root object
    QCOMPARE(item->width(), 200.0);
    QCOMPARE(item->height(), 200.0);
    QCOMPARE(view->size(), QSize(200, 200));
    QCOMPARE(view->size(), view->sizeHint());
    QCOMPARE(view->size(), view->initialSize());

    // size update from view
    view->resize(QSize(80,100));

    QTRY_COMPARE(item->width(), 80.0);
    QCOMPARE(item->height(), 100.0);
    QCOMPARE(view->size(), QSize(80, 100));
    QCOMPARE(view->size(), view->sizeHint());

    view->setResizeMode(QQuickView::SizeViewToRootObject);

    // size update from view disabled
    view->resize(QSize(60,80));
    QCOMPARE(item->width(), 80.0);
    QCOMPARE(item->height(), 100.0);
    QTest::qWait(50);
    QCOMPARE(view->size(), QSize(60, 80));

    // size update from root object
    item->setWidth(250);
    item->setHeight(350);
    QCOMPARE(item->width(), 250.0);
    QCOMPARE(item->height(), 350.0);
    QTRY_COMPARE(view->size(), QSize(250, 350));
    QCOMPARE(view->size(), QSize(250, 350));
    QCOMPARE(view->size(), view->sizeHint());

    // reset window
    window.hide();
    delete view;
    view = new QQuickView(&window);
    QVERIFY(view);
    view->setResizeMode(QQuickView::SizeViewToRootObject);
    view->setSource(testFileUrl("resizemodeitem.qml"));
    item = qobject_cast<QQuickItem*>(view->rootObject());
    QVERIFY(item);
    window.show();

    view->showNormal();

    // initial size for root object
    QCOMPARE(item->width(), 200.0);
    QCOMPARE(item->height(), 200.0);
    QCOMPARE(view->size(), view->sizeHint());
    QCOMPARE(view->size(), view->initialSize());

    // size update from root object
    item->setWidth(80);
    item->setHeight(100);
    QCOMPARE(item->width(), 80.0);
    QCOMPARE(item->height(), 100.0);
    QTRY_COMPARE(view->size(), QSize(80, 100));
    QCOMPARE(view->size(), view->sizeHint());

    // size update from root object disabled
    view->setResizeMode(QQuickView::SizeRootObjectToView);
    item->setWidth(60);
    item->setHeight(80);
    QCOMPARE(view->width(), 80);
    QCOMPARE(view->height(), 100);
    QCOMPARE(QSize(item->width(), item->height()), view->sizeHint());

    // size update from view
    QCoreApplication::processEvents(); // make sure the last resize events are gone
    SizeChangesListener sizeListener(item);
    view->resize(QSize(200,300));
    QTRY_COMPARE(item->width(), 200.0);

    for (int i = 0; i < sizeListener.count(); ++i) {
        // Check that we have the correct geometry on all signals
        QCOMPARE(sizeListener.at(i), view->size());
    }

    QCOMPARE(item->height(), 300.0);
    QCOMPARE(view->size(), QSize(200, 300));
    QCOMPARE(view->size(), view->sizeHint());

    window.hide();
    delete view;

    // if we set a specific size for the view then it should keep that size
    // for SizeRootObjectToView mode.
    view = new QQuickView(&window);
    view->resize(300, 300);
    view->setResizeMode(QQuickView::SizeRootObjectToView);
    QCOMPARE(QSize(0,0), view->initialSize());
    view->setSource(testFileUrl("resizemodeitem.qml"));
    view->resize(300, 300);
    item = qobject_cast<QQuickItem*>(view->rootObject());
    QVERIFY(item);
    window.show();

    view->showNormal();
    QTest::qWait(50);

    // initial size from root object
    QCOMPARE(item->width(), 300.0);
    QCOMPARE(item->height(), 300.0);
    QCOMPARE(view->size(), QSize(300, 300));
    QCOMPARE(view->size(), view->sizeHint());
    QCOMPARE(view->initialSize(), QSize(200, 200)); // initial object size

    delete view;
}

void tst_QQuickView::errors()
{
    {
        QQuickView view;
        QVERIFY(view.errors().isEmpty()); // don't crash
    }
    {
        QQuickView view;
        QQmlTestMessageHandler messageHandler;
        view.setSource(testFileUrl("error1.qml"));
        QCOMPARE(view.status(), QQuickView::Error);
        QCOMPARE(view.errors().count(), 1);
    }

    {
        QQuickView view;
        QQmlTestMessageHandler messageHandler;
        view.setSource(testFileUrl("error2.qml"));
        QCOMPARE(view.status(), QQuickView::Error);
        QCOMPARE(view.errors().count(), 1);
        view.show();
    }
}

void tst_QQuickView::engine()
{
    QQmlEngine *engine = new QQmlEngine;
    QVERIFY(!engine->incubationController());

    QQuickView *view = new QQuickView(engine, 0);
    QVERIFY(view);
    QCOMPARE(engine->incubationController(), view->incubationController());

    QQuickView *view2 = new QQuickView(engine, 0);
    QVERIFY(view);
    QCOMPARE(engine->incubationController(), view->incubationController());
    delete view;
    QVERIFY(!engine->incubationController());

    engine->setIncubationController(view2->incubationController());
    QCOMPARE(engine->incubationController(), view2->incubationController());
    delete view2;
    QVERIFY(!engine->incubationController());

    QQuickView *view3 = new QQuickView;
    QQuickView *view4 = new QQuickView(view3->engine(), 0);

    QVERIFY(view3->engine());
    QVERIFY(view4->engine());
    QCOMPARE(view3->engine(), view4->engine());
    delete view3;
    QVERIFY(!view4->engine());
    QTest::ignoreMessage(QtWarningMsg, "QQuickView: invalid qml engine.");
    view4->setSource(QUrl());

    QCOMPARE(view4->status(), QQuickView::Error);
    QVERIFY(!view4->errors().isEmpty());
    QCOMPARE(view4->errors().back().description(), QLatin1String("QQuickView: invalid qml engine."));
    delete view4;
}

QTEST_MAIN(tst_QQuickView)

#include "tst_qquickview.moc"
