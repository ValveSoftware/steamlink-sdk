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
#include <QtDeclarative/qdeclarativeengine.h>
#include <QtDeclarative/qdeclarativeview.h>
#include <QtDeclarative/qdeclarativeitem.h>
#include <QtDeclarative/qdeclarativecontext.h>
#include <QtWidgets/qmenubar.h>
#include <QSignalSpy>
#include "qmlruntime.h"
#include "deviceorientation.h"

#if defined(Q_OS_MAC) || defined(Q_WS_MAEMO_5) || defined(Q_WS_S60)
#  define MENUBAR_HEIGHT(mw) 0
#else
#  define MENUBAR_HEIGHT(mw) (mw->menuBar()->height())
#endif

class tst_QDeclarativeViewer : public QObject

{
    Q_OBJECT
public:
    tst_QDeclarativeViewer();

private slots:
    void runtimeContextProperty();
    void loading();
    void fileBrowser();
    void resizing();
    void paths();
    void slowMode();

private:
    QDeclarativeEngine engine;
};

tst_QDeclarativeViewer::tst_QDeclarativeViewer()
{
}

#define TEST_INITIAL_SIZES(viewer) { \
    QDeclarativeItem* rootItem = qobject_cast<QDeclarativeItem*>(viewer->view()->rootObject()); \
    QVERIFY(rootItem); \
\
    QCOMPARE(rootItem->width(), 200.0); \
    QCOMPARE(rootItem->height(), 300.0); \
    QTRY_COMPARE(viewer->view()->size(), QSize(200, 300)); \
    QCOMPARE(viewer->view()->sceneRect().size(), QSizeF(200, 300)); \
    QCOMPARE(viewer->size(), QSize(200, 300 + MENUBAR_HEIGHT(viewer))); \
    QCOMPARE(viewer->size(), viewer->sizeHint()); \
}

void tst_QDeclarativeViewer::runtimeContextProperty()
{
    QDeclarativeViewer *viewer = new QDeclarativeViewer();
    QVERIFY(viewer);
    viewer->open(SRCDIR "/data/orientation.qml");
    QVERIFY(viewer->view());
    QVERIFY(viewer->menuBar());
    QDeclarativeItem* rootItem = qobject_cast<QDeclarativeItem*>(viewer->view()->rootObject());
    QVERIFY(rootItem);
    QObject *runtimeObject = qvariant_cast<QObject*>(viewer->view()->engine()->rootContext()->contextProperty("runtime"));
    QVERIFY(runtimeObject);

    // test isActiveWindow property
    QVERIFY(!runtimeObject->property("isActiveWindow").toBool());

    viewer->show();
    QApplication::setActiveWindow(viewer);
    //viewer->requestActivateWindow();
    QVERIFY(QTest::qWaitForWindowActive(viewer));
    QCOMPARE(QApplication::activeWindow(), static_cast<QWidget *>(viewer));
    //QTRY_VERIFY(viewer == qGuiApp->focusWindow());

    QVERIFY(runtimeObject->property("isActiveWindow").toBool());

    TEST_INITIAL_SIZES(viewer);

    // test orientation property
    QCOMPARE(runtimeObject->property("orientation").toInt(), int(DeviceOrientation::Portrait));

    viewer->rotateOrientation();
    qApp->processEvents();

    QCOMPARE(runtimeObject->property("orientation").toInt(), int(DeviceOrientation::Landscape));
    QCOMPARE(rootItem->width(), 300.0);

    QCOMPARE(rootItem->width(), 300.0);
    QCOMPARE(rootItem->height(), 200.0);
    QTRY_COMPARE(viewer->view()->size(), QSize(300, 200));
    QCOMPARE(viewer->view()->sceneRect().size(), QSizeF(300, 200));
    QCOMPARE(viewer->size(), QSize(300, 200 + MENUBAR_HEIGHT(viewer)));
    QCOMPARE(viewer->size(), viewer->sizeHint());

    viewer->rotateOrientation();
    qApp->processEvents();

    QCOMPARE(runtimeObject->property("orientation").toInt(), int(DeviceOrientation::PortraitInverted));

    QCOMPARE(rootItem->width(), 200.0);
    QCOMPARE(rootItem->height(), 300.0);
    QTRY_COMPARE(viewer->view()->size(), QSize(200, 300));
    QCOMPARE(viewer->view()->sceneRect().size(), QSizeF(200, 300));
    QCOMPARE(viewer->size(), QSize(200, 300 + MENUBAR_HEIGHT(viewer)));
    QCOMPARE(viewer->size(), viewer->sizeHint());

    viewer->rotateOrientation();
    qApp->processEvents();

    QCOMPARE(runtimeObject->property("orientation").toInt(), int(DeviceOrientation::LandscapeInverted));

    viewer->rotateOrientation();
    qApp->processEvents();

    QCOMPARE(runtimeObject->property("orientation").toInt(), int(DeviceOrientation::Portrait));

    viewer->hide();
    QVERIFY(!runtimeObject->property("isActiveWindow").toBool());

    delete viewer;
}

void tst_QDeclarativeViewer::loading()
{
    QDeclarativeViewer *viewer = new QDeclarativeViewer();
    QVERIFY(viewer);
    viewer->setSizeToView(true);
    viewer->open(SRCDIR "/data/orientation.qml");
    QVERIFY(viewer->view());
    QVERIFY(viewer->menuBar());
    QDeclarativeItem* rootItem = qobject_cast<QDeclarativeItem*>(viewer->view()->rootObject());
    QVERIFY(rootItem);
    viewer->show();

    QApplication::setActiveWindow(viewer);
    QVERIFY(QTest::qWaitForWindowActive(viewer));
    QCOMPARE(QApplication::activeWindow(), static_cast<QWidget *>(viewer));

    TEST_INITIAL_SIZES(viewer);

    viewer->resize(QSize(250, 350));
    qApp->processEvents();

    // window resized
    QTRY_COMPARE(rootItem->width(), 250.0);
    QTRY_COMPARE(rootItem->height(), 350.0 - MENUBAR_HEIGHT(viewer));
    QCOMPARE(viewer->view()->size(), QSize(250, 350 - MENUBAR_HEIGHT(viewer)));
    QCOMPARE(viewer->view()->initialSize(), QSize(200, 300));
    QCOMPARE(viewer->view()->sceneRect().size(), QSizeF(250, 350 - MENUBAR_HEIGHT(viewer)));
    QCOMPARE(viewer->size(), QSize(250, 350));
    QCOMPARE(viewer->size(), viewer->sizeHint());

    QSignalSpy statusSpy(viewer->view(), SIGNAL(statusChanged(QDeclarativeView::Status)));
    viewer->reload();
    QTRY_VERIFY(statusSpy.count() == 1);
    rootItem = qobject_cast<QDeclarativeItem*>(viewer->view()->rootObject());
    QVERIFY(rootItem);

    // reload cause the window to return back to initial size
    QTRY_COMPARE(rootItem->width(), 200.0);
    QTRY_COMPARE(rootItem->height(), 300.0);
    QCOMPARE(viewer->view()->size(), QSize(200, 300));
    QCOMPARE(viewer->view()->initialSize(), QSize(200, 300));
    QCOMPARE(viewer->view()->sceneRect().size(), QSizeF(200, 300));
    QCOMPARE(viewer->size(), QSize(200, 300 + MENUBAR_HEIGHT(viewer)));
    QCOMPARE(viewer->size(), viewer->sizeHint());

    viewer->resize(QSize(250, 350));
    qApp->processEvents();

    // window resized again
    QTRY_COMPARE(rootItem->width(), 250.0);
    QTRY_COMPARE(rootItem->height(), 350.0 - MENUBAR_HEIGHT(viewer));
    QCOMPARE(viewer->view()->size(), QSize(250, 350 - MENUBAR_HEIGHT(viewer)));
    QCOMPARE(viewer->view()->initialSize(), QSize(200, 300));
    QCOMPARE(viewer->view()->sceneRect().size(), QSizeF(250, 350 - MENUBAR_HEIGHT(viewer)));
    QCOMPARE(viewer->size(), QSize(250, 350));
    QCOMPARE(viewer->size(), viewer->sizeHint());

    viewer->open(SRCDIR "/data/orientation.qml");
    rootItem = qobject_cast<QDeclarativeItem*>(viewer->view()->rootObject());
    QVERIFY(rootItem);

    // open also causes the window to return back to initial size
    QTRY_COMPARE(rootItem->width(), 200.0);
    QTRY_COMPARE(rootItem->height(), 300.0);
    QCOMPARE(viewer->view()->size(), QSize(200, 300));
    QCOMPARE(viewer->view()->initialSize(), QSize(200, 300));
    QCOMPARE(viewer->view()->sceneRect().size(), QSizeF(200, 300));
    QCOMPARE(viewer->size(), QSize(200, 300 + MENUBAR_HEIGHT(viewer)));
    QCOMPARE(viewer->size(), viewer->sizeHint());

    delete viewer;
}

static QStringList warnings;

static void checkWarnings(QtMsgType, const QMessageLogContext &, const QString &warning)
{
    warnings.push_back(warning);
}

void tst_QDeclarativeViewer::fileBrowser()
{
    QtMessageHandler previousMsgHandler = qInstallMessageHandler(checkWarnings);
    QDeclarativeViewer *viewer = new QDeclarativeViewer();
    QVERIFY(viewer);
    viewer->setUseNativeFileBrowser(false);
    viewer->openFile();
    viewer->show();
    QCoreApplication::processEvents();
    qInstallMessageHandler(previousMsgHandler);

    // QTBUG-15720
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join(QLatin1Char('\n'))));

    QApplication::setActiveWindow(viewer);
    QVERIFY(QTest::qWaitForWindowActive(viewer));
    QCOMPARE(QApplication::activeWindow(), static_cast<QWidget *>(viewer));

    // Browser.qml successfully loaded
    QDeclarativeItem* browserItem = qobject_cast<QDeclarativeItem*>(viewer->view()->rootObject());
    QVERIFY(viewer->view());
    QVERIFY(viewer->menuBar());
    QVERIFY(browserItem);

    // load something
    viewer->open(SRCDIR "/data/orientation.qml");
    QVERIFY(viewer->view());
    QVERIFY(viewer->menuBar());
    QDeclarativeItem* rootItem = qobject_cast<QDeclarativeItem*>(viewer->view()->rootObject());
    QVERIFY(rootItem);
    QVERIFY(browserItem != rootItem);

    // go back to Browser.qml
    viewer->openFile();
    browserItem = qobject_cast<QDeclarativeItem*>(viewer->view()->rootObject());
    QVERIFY(viewer->view());
    QVERIFY(viewer->menuBar());
    QVERIFY(browserItem);

    delete viewer;
}

void tst_QDeclarativeViewer::resizing()
{
    QDeclarativeViewer *viewer = new QDeclarativeViewer();
    QVERIFY(viewer);
    viewer->open(SRCDIR "/data/orientation.qml");
    QVERIFY(viewer->view());
    QVERIFY(viewer->menuBar());
    QDeclarativeItem* rootItem = qobject_cast<QDeclarativeItem*>(viewer->view()->rootObject());
    QVERIFY(rootItem);
    viewer->show();

    QApplication::setActiveWindow(viewer);
    QVERIFY(QTest::qWaitForWindowActive(viewer));
    QCOMPARE(QApplication::activeWindow(), static_cast<QWidget *>(viewer));

    TEST_INITIAL_SIZES(viewer);

    viewer->setSizeToView(false);

    // size view to root object
    rootItem->setWidth(150);
    rootItem->setHeight(200);
    qApp->processEvents();

    QCOMPARE(rootItem->width(), 150.0);
    QCOMPARE(rootItem->height(), 200.0);
    QTRY_COMPARE(viewer->view()->size(), QSize(150, 200));
    QCOMPARE(viewer->view()->initialSize(), QSize(200, 300));
    QCOMPARE(viewer->view()->sceneRect().size(), QSizeF(150, 200));
    QCOMPARE(viewer->size(), QSize(150, 200 + MENUBAR_HEIGHT(viewer)));

    // do not size root object to view
    viewer->resize(QSize(180,250));
    QCOMPARE(rootItem->width(), 150.0);
    QCOMPARE(rootItem->height(), 200.0);

    viewer->setSizeToView(true);

    // size root object to view
    viewer->resize(QSize(250,350));
    qApp->processEvents();

    QTRY_COMPARE(rootItem->width(), 250.0);
    QTRY_COMPARE(rootItem->height(), 350.0 - MENUBAR_HEIGHT(viewer));
    QTRY_COMPARE(viewer->view()->size(), QSize(250, 350 - MENUBAR_HEIGHT(viewer)));
    QCOMPARE(viewer->view()->initialSize(), QSize(200, 300));
    QCOMPARE(viewer->view()->sceneRect().size(), QSizeF(250, 350 - MENUBAR_HEIGHT(viewer)));
    QCOMPARE(viewer->size(), QSize(250, 350));

    // do not size view to root object
    rootItem->setWidth(150);
    rootItem->setHeight(200);
    QTRY_COMPARE(viewer->size(), QSize(250, 350));

    delete viewer;
}

void tst_QDeclarativeViewer::paths()
{
    QDeclarativeViewer *viewer = new QDeclarativeViewer();
    QVERIFY(viewer);

    viewer->addLibraryPath("miscImportPath");
    viewer->view()->engine()->importPathList().contains("miscImportPath");

    viewer->addPluginPath("miscPluginPath");
    viewer->view()->engine()->pluginPathList().contains("miscPluginPath");

    delete viewer;
}

void tst_QDeclarativeViewer::slowMode()
{
    QDeclarativeViewer *viewer = new QDeclarativeViewer();
    QVERIFY(viewer);

    viewer->setSlowMode(true);
    viewer->setSlowMode(false);

    delete viewer;
}

QTEST_MAIN(tst_QDeclarativeViewer)

#include "tst_qdeclarativeviewer.moc"
