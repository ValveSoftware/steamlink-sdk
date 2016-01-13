/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include <qtest.h>
#include <QtTest/QSignalSpy>
#include <QtDeclarative/qdeclarativecomponent.h>
#include <QtDeclarative/qdeclarativecontext.h>
#include <QtDeclarative/qdeclarativeview.h>
#include <QtDeclarative/qdeclarativeitem.h>
#include <QtWidgets/qgraphicswidget.h>

class tst_QDeclarativeView : public QObject

{
    Q_OBJECT
public:
    tst_QDeclarativeView();

private slots:
    void scene();
    void resizemodedeclarativeitem();
    void resizemodegraphicswidget();
    void errors();

private:
    template<typename T>
    T *findItem(QGraphicsObject *parent, const QString &objectName);
};


tst_QDeclarativeView::tst_QDeclarativeView()
{
}

void tst_QDeclarativeView::scene()
{
    // QTBUG-14771
    QGraphicsScene scene;
    scene.setItemIndexMethod(QGraphicsScene::NoIndex);
    scene.setStickyFocus(true);

    QDeclarativeView *view = new QDeclarativeView();
    QVERIFY(view);
    QVERIFY(view->scene());
    view->setScene(&scene);
    QCOMPARE(view->scene(), &scene);

    view->setSource(QUrl::fromLocalFile(SRCDIR "/data/resizemodedeclarativeitem.qml"));
    QDeclarativeItem* declarativeItem = qobject_cast<QDeclarativeItem*>(view->rootObject());
    QVERIFY(declarativeItem);
    QVERIFY(scene.items().count() > 0);
    QCOMPARE(scene.items().at(0), declarativeItem);
}

void tst_QDeclarativeView::resizemodedeclarativeitem()
{
    QWidget window;
    QDeclarativeView *canvas = new QDeclarativeView(&window);
    QVERIFY(canvas);
    QSignalSpy sceneResizedSpy(canvas, SIGNAL(sceneResized(QSize)));
    canvas->setResizeMode(QDeclarativeView::SizeRootObjectToView);
    QCOMPARE(QSize(0,0), canvas->initialSize());
    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/resizemodedeclarativeitem.qml"));
    QDeclarativeItem* declarativeItem = qobject_cast<QDeclarativeItem*>(canvas->rootObject());
    QVERIFY(declarativeItem);
    window.show();

    // initial size from root object
    QCOMPARE(declarativeItem->width(), 200.0);
    QCOMPARE(declarativeItem->height(), 200.0);
    QCOMPARE(canvas->size(), QSize(200, 200));
    QCOMPARE(canvas->size(), canvas->sizeHint());
    QCOMPARE(canvas->size(), canvas->initialSize());
    QCOMPARE(sceneResizedSpy.count(), 1);

    // size update from view
    canvas->resize(QSize(80,100));
    QCOMPARE(declarativeItem->width(), 80.0);
    QCOMPARE(declarativeItem->height(), 100.0);
    QCOMPARE(canvas->size(), QSize(80, 100));
    QCOMPARE(canvas->size(), canvas->sizeHint());
    QCOMPARE(sceneResizedSpy.count(), 2);

    canvas->setResizeMode(QDeclarativeView::SizeViewToRootObject);

    // size update from view disabled
    canvas->resize(QSize(60,80));
    QCOMPARE(declarativeItem->width(), 80.0);
    QCOMPARE(declarativeItem->height(), 100.0);
    QCOMPARE(canvas->size(), QSize(60, 80));
    QCOMPARE(sceneResizedSpy.count(), 3);

    // size update from root object
    declarativeItem->setWidth(250);
    declarativeItem->setHeight(350);
    QCOMPARE(declarativeItem->width(), 250.0);
    QCOMPARE(declarativeItem->height(), 350.0);
    QTRY_COMPARE(canvas->size(), QSize(250, 350));
    QCOMPARE(canvas->size(), QSize(250, 350));
    QCOMPARE(canvas->size(), canvas->sizeHint());
    QCOMPARE(sceneResizedSpy.count(), 4);

    // reset canvas
    window.hide();
    delete canvas;
    canvas = new QDeclarativeView(&window);
    QVERIFY(canvas);
    QSignalSpy sceneResizedSpy2(canvas, SIGNAL(sceneResized(QSize)));
    canvas->setResizeMode(QDeclarativeView::SizeViewToRootObject);
    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/resizemodedeclarativeitem.qml"));
    declarativeItem = qobject_cast<QDeclarativeItem*>(canvas->rootObject());
    QVERIFY(declarativeItem);
    window.show();

    // initial size for root object
    QCOMPARE(declarativeItem->width(), 200.0);
    QCOMPARE(declarativeItem->height(), 200.0);
    QCOMPARE(canvas->size(), canvas->sizeHint());
    QCOMPARE(canvas->size(), canvas->initialSize());
    QCOMPARE(sceneResizedSpy2.count(), 1);

    // size update from root object
    declarativeItem->setWidth(80);
    declarativeItem->setHeight(100);
    QCOMPARE(declarativeItem->width(), 80.0);
    QCOMPARE(declarativeItem->height(), 100.0);
    QTRY_COMPARE(canvas->size(), QSize(80, 100));
    QCOMPARE(canvas->size(), QSize(80, 100));
    QCOMPARE(canvas->size(), canvas->sizeHint());
    QCOMPARE(sceneResizedSpy2.count(), 2);

    // size update from root object disabled
    canvas->setResizeMode(QDeclarativeView::SizeRootObjectToView);
    declarativeItem->setWidth(60);
    declarativeItem->setHeight(80);
    QCOMPARE(canvas->width(), 80);
    QCOMPARE(canvas->height(), 100);
    QCOMPARE(QSize(declarativeItem->width(), declarativeItem->height()), canvas->sizeHint());
    QCOMPARE(sceneResizedSpy2.count(), 2);

    // size update from view
    canvas->resize(QSize(200,300));
    QCOMPARE(declarativeItem->width(), 200.0);
    QCOMPARE(declarativeItem->height(), 300.0);
    QCOMPARE(canvas->size(), QSize(200, 300));
    QCOMPARE(canvas->size(), canvas->sizeHint());
    QCOMPARE(sceneResizedSpy2.count(), 3);

    delete canvas;
}

void tst_QDeclarativeView::resizemodegraphicswidget()
{
    QWidget window;
    QDeclarativeView *canvas = new QDeclarativeView(&window);
    QVERIFY(canvas);
    QSignalSpy sceneResizedSpy(canvas, SIGNAL(sceneResized(QSize)));
    canvas->setResizeMode(QDeclarativeView::SizeRootObjectToView);
    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/resizemodegraphicswidget.qml"));
    QGraphicsWidget* graphicsWidget = qobject_cast<QGraphicsWidget*>(canvas->rootObject());
    QVERIFY(graphicsWidget);
    window.show();

    // initial size from root object
    QCOMPARE(graphicsWidget->size(), QSizeF(200.0, 200.0));
    QCOMPARE(canvas->size(), QSize(200, 200));
    QCOMPARE(canvas->size(), QSize(200, 200));
    QCOMPARE(canvas->size(), canvas->sizeHint());
    QCOMPARE(canvas->size(), canvas->initialSize());
    QCOMPARE(sceneResizedSpy.count(), 1);

    // size update from view
    canvas->resize(QSize(80,100));
    QCOMPARE(graphicsWidget->size(), QSizeF(80.0,100.0));
    QCOMPARE(canvas->size(), QSize(80,100));
    QCOMPARE(canvas->size(), canvas->sizeHint());
    QCOMPARE(sceneResizedSpy.count(), 2);

    // size update from view disabled
    canvas->setResizeMode(QDeclarativeView::SizeViewToRootObject);
    canvas->resize(QSize(60,80));
    QCOMPARE(graphicsWidget->size(), QSizeF(80.0,100.0));
    QCOMPARE(canvas->size(), QSize(60, 80));
    QCOMPARE(sceneResizedSpy.count(), 3);

    // size update from root object
    graphicsWidget->resize(QSizeF(250.0, 350.0));
    QCOMPARE(graphicsWidget->size(), QSizeF(250.0,350.0));
    QCOMPARE(canvas->size(), QSize(250, 350));
    QCOMPARE(canvas->size(), canvas->sizeHint());
    QCOMPARE(sceneResizedSpy.count(), 4);

    // reset canvas
    window.hide();
    delete canvas;
    canvas = new QDeclarativeView(&window);
    QVERIFY(canvas);
    QSignalSpy sceneResizedSpy2(canvas, SIGNAL(sceneResized(QSize)));
    canvas->setResizeMode(QDeclarativeView::SizeViewToRootObject);
    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/resizemodegraphicswidget.qml"));
    graphicsWidget = qobject_cast<QGraphicsWidget*>(canvas->rootObject());
    QVERIFY(graphicsWidget);
    window.show();

    // initial size from root object
    QCOMPARE(graphicsWidget->size(), QSizeF(200.0, 200.0));
    QCOMPARE(canvas->size(), QSize(200, 200));
    QCOMPARE(canvas->size(), canvas->sizeHint());
    QCOMPARE(canvas->size(), canvas->initialSize());
    QCOMPARE(sceneResizedSpy2.count(), 1);

    // size update from root object
    graphicsWidget->resize(QSizeF(80, 100));
    QCOMPARE(graphicsWidget->size(), QSizeF(80.0, 100.0));
    QCOMPARE(canvas->size(), QSize(80, 100));
    QCOMPARE(canvas->size(), canvas->sizeHint());
    QCOMPARE(sceneResizedSpy2.count(), 2);

    // size update from root object disabled
    canvas->setResizeMode(QDeclarativeView::SizeRootObjectToView);
    graphicsWidget->resize(QSizeF(60,80));
    QCOMPARE(canvas->size(), QSize(80,100));
    QCOMPARE(QSize(graphicsWidget->size().width(), graphicsWidget->size().height()), canvas->sizeHint());
    QCOMPARE(sceneResizedSpy2.count(), 2);

    // size update from view
    canvas->resize(QSize(200,300));
    QCOMPARE(graphicsWidget->size(), QSizeF(200.0, 300.0));
    QCOMPARE(canvas->size(), QSize(200, 300));
    QCOMPARE(canvas->size(), canvas->sizeHint());
    QCOMPARE(sceneResizedSpy2.count(), 3);

    window.show();
    delete canvas;
}

static void silentErrorsMsgHandler(QtMsgType, const QMessageLogContext &, const QString &)
{
}

void tst_QDeclarativeView::errors()
{
    QDeclarativeView *canvas = new QDeclarativeView;
    QVERIFY(canvas);
    QtMessageHandler old = qInstallMessageHandler(silentErrorsMsgHandler);
    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/error1.qml"));
    qInstallMessageHandler(old);
    QVERIFY(canvas->status() == QDeclarativeView::Error);
    QVERIFY(canvas->errors().count() == 1);
    delete canvas;
}

template<typename T>
T *tst_QDeclarativeView::findItem(QGraphicsObject *parent, const QString &objectName)
{
    if (!parent)
        return 0;

    const QMetaObject &mo = T::staticMetaObject;
    //qDebug() << parent->QGraphicsObject::children().count() << "children";
    for (int i = 0; i < parent->childItems().count(); ++i) {
        QDeclarativeItem *item = qobject_cast<QDeclarativeItem*>(parent->childItems().at(i));
        if(!item)
            continue;
        //qDebug() << "try" << item;
        if (mo.cast(item) && (objectName.isEmpty() || item->objectName() == objectName))
            return static_cast<T*>(item);
        item = findItem<T>(item, objectName);
        if (item)
            return static_cast<T*>(item);
    }

    return 0;
}

QTEST_MAIN(tst_QDeclarativeView)

#include "tst_qdeclarativeview.moc"
