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
#include <QtTest/QtTest>
#include <QtQuick/private/qquickpixmapcache_p.h>
#include <QtQml/qqmlengine.h>
#include <QtQuick/qquickimageprovider.h>
#include <QNetworkReply>
#include "../../shared/util.h"
#include "testhttpserver.h"
#include <QtNetwork/QNetworkConfigurationManager>

#ifndef QT_NO_CONCURRENT
#include <qtconcurrentrun.h>
#include <qfuture.h>
#endif

#define PIXMAP_DATA_LEAK_TEST 0

class tst_qquickpixmapcache : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qquickpixmapcache() {}

private slots:
    void initTestCase();
    void single();
    void single_data();
    void parallel();
    void parallel_data();
    void massive();
    void cancelcrash();
    void shrinkcache();
#ifndef QT_NO_CONCURRENT
    void networkCrash();
#endif
    void lockingCrash();
    void uncached();
#if PIXMAP_DATA_LEAK_TEST
    void dataLeak();
#endif
private:
    QQmlEngine engine;
    TestHTTPServer server;
};

static int slotters=0;

class Slotter : public QObject
{
    Q_OBJECT
public:
    Slotter()
    {
        gotslot = false;
        slotters++;
    }
    bool gotslot;

public slots:
    void got()
    {
        gotslot = true;
        --slotters;
        if (slotters==0)
            QTestEventLoop::instance().exitLoop();
    }
};

#ifndef QT_NO_LOCALFILE_OPTIMIZED_QML
static const bool localfile_optimized = true;
#else
static const bool localfile_optimized = false;
#endif

void tst_qquickpixmapcache::initTestCase()
{
    QQmlDataTest::initTestCase();

    QVERIFY2(server.listen(), qPrintable(server.errorString()));

#ifndef QT_NO_BEARERMANAGEMENT
    // This avoids a race condition/deadlock bug in network config
    // manager when it is accessed by the HTTP server thread before
    // anything else. Bug report can be found at:
    // QTBUG-26355
    QNetworkConfigurationManager cm;
    cm.updateConfigurations();
#endif

    server.serveDirectory(testFile("http"));
}

void tst_qquickpixmapcache::single_data()
{
    // Note, since QQuickPixmapCache is shared, tests affect each other!
    // so use different files fore all test functions.

    QTest::addColumn<QUrl>("target");
    QTest::addColumn<bool>("incache");
    QTest::addColumn<bool>("exists");
    QTest::addColumn<bool>("neterror");

    // File URLs are optimized
    QTest::newRow("local") << testFileUrl("exists.png") << localfile_optimized << true << false;
    QTest::newRow("local") << testFileUrl("notexists.png") << localfile_optimized << false << false;
    QTest::newRow("remote") << server.url("/exists.png") << false << true << false;
    QTest::newRow("remote") << server.url("/notexists.png") << false << false << true;
}

void tst_qquickpixmapcache::single()
{
    QFETCH(QUrl, target);
    QFETCH(bool, incache);
    QFETCH(bool, exists);
    QFETCH(bool, neterror);

    QString expectedError;
    if (neterror) {
        expectedError = "Error transferring " + target.toString() + " - server replied: Not found";
    } else if (!exists) {
        expectedError = "Cannot open: " + target.toString();
    }

    QQuickPixmap pixmap;
    QVERIFY(pixmap.width() <= 0); // Check Qt assumption

    pixmap.load(&engine, target);

    if (incache) {
        QCOMPARE(pixmap.error(), expectedError);
        if (exists) {
            QCOMPARE(pixmap.status(), QQuickPixmap::Ready);
            QVERIFY(pixmap.width() > 0);
        } else {
            QCOMPARE(pixmap.status(), QQuickPixmap::Error);
            QVERIFY(pixmap.width() <= 0);
        }
    } else {
        QVERIFY(pixmap.width() <= 0);

        Slotter getter;
        pixmap.connectFinished(&getter, SLOT(got()));
        QTestEventLoop::instance().enterLoop(10);
        QVERIFY(!QTestEventLoop::instance().timeout());
        QVERIFY(getter.gotslot);
        if (exists) {
            QCOMPARE(pixmap.status(), QQuickPixmap::Ready);
            QVERIFY(pixmap.width() > 0);
        } else {
            QCOMPARE(pixmap.status(), QQuickPixmap::Error);
            QVERIFY(pixmap.width() <= 0);
        }
        QCOMPARE(pixmap.error(), expectedError);
    }
}

void tst_qquickpixmapcache::parallel_data()
{
    // Note, since QQuickPixmapCache is shared, tests affect each other!
    // so use different files fore all test functions.

    QTest::addColumn<QUrl>("target1");
    QTest::addColumn<QUrl>("target2");
    QTest::addColumn<int>("incache");
    QTest::addColumn<int>("cancel"); // which one to cancel

    QTest::newRow("local")
            << testFileUrl("exists1.png")
            << testFileUrl("exists2.png")
            << (localfile_optimized ? 2 : 0)
            << -1;

    QTest::newRow("remote")
            << server.url("/exists2.png")
            << server.url("/exists3.png")
            << 0
            << -1;

    QTest::newRow("remoteagain")
            << server.url("/exists2.png")
            << server.url("/exists3.png")
            << 2
            << -1;

    QTest::newRow("remotecopy")
            << server.url("/exists4.png")
            << server.url("/exists4.png")
            << 0
            << -1;

    QTest::newRow("remotecopycancel")
            << server.url("/exists5.png")
            << server.url("/exists5.png")
            << 0
            << 0;
}

void tst_qquickpixmapcache::parallel()
{
    QFETCH(QUrl, target1);
    QFETCH(QUrl, target2);
    QFETCH(int, incache);
    QFETCH(int, cancel);

    QList<QUrl> targets;
    targets << target1 << target2;

    QList<QQuickPixmap *> pixmaps;
    QList<bool> pending;
    QList<Slotter*> getters;

    for (int i=0; i<targets.count(); ++i) {
        QUrl target = targets.at(i);
        QQuickPixmap *pixmap = new QQuickPixmap;

        pixmap->load(&engine, target);

        QVERIFY(pixmap->status() != QQuickPixmap::Error);
        pixmaps.append(pixmap);
        if (pixmap->isReady()) {
            QVERIFY(pixmap->width() >  0);
            getters.append(0);
            pending.append(false);
        } else {
            QVERIFY(pixmap->width() <= 0);
            getters.append(new Slotter);
            pixmap->connectFinished(getters[i], SLOT(got()));
            pending.append(true);
        }
    }

    if (incache + slotters != targets.count())
        QFAIL(QString::fromLatin1("pixmap counts don't add up: %1 incache, %2 slotters, %3 total")
              .arg(incache).arg(slotters).arg(targets.count()).toLatin1().constData());

    if (cancel >= 0) {
        pixmaps.at(cancel)->clear(getters[cancel]);
        slotters--;
    }

    if (slotters) {
        QTestEventLoop::instance().enterLoop(10);
        QVERIFY(!QTestEventLoop::instance().timeout());
    }

    for (int i=0; i<targets.count(); ++i) {
        QQuickPixmap *pixmap = pixmaps[i];

        if (i == cancel) {
            QVERIFY(!getters[i]->gotslot);
        } else {
            if (pending[i])
                QVERIFY(getters[i]->gotslot);

            if (!pixmap->isReady()) {
                QFAIL(QString::fromLatin1("pixmap %1 not ready, status %2: %3")
                      .arg(pixmap->url().toString()).arg(pixmap->status())
                      .arg(pixmap->error()).toLatin1().constData());

            }
            QVERIFY(pixmap->width() > 0);
            delete getters[i];
        }
    }

    qDeleteAll(pixmaps);
}

void tst_qquickpixmapcache::massive()
{
    QQmlEngine engine;
    QUrl url = testFileUrl("massive.png");

    // Confirm that massive images remain in the cache while they are
    // in use by the application.
    {
    qint64 cachekey = 0;
    QQuickPixmap p(&engine, url);
    QVERIFY(p.isReady());
    QVERIFY(p.image().size() == QSize(10000, 1000));
    cachekey = p.image().cacheKey();

    QQuickPixmap p2(&engine, url);
    QVERIFY(p2.isReady());
    QVERIFY(p2.image().size() == QSize(10000, 1000));

    QCOMPARE(p2.image().cacheKey(), cachekey);
    }

    // Confirm that massive images are removed from the cache when
    // they become unused
    {
    qint64 cachekey = 0;
    {
        QQuickPixmap p(&engine, url);
        QVERIFY(p.isReady());
        QVERIFY(p.image().size() == QSize(10000, 1000));
        cachekey = p.image().cacheKey();
    }

    QQuickPixmap p2(&engine, url);
    QVERIFY(p2.isReady());
    QVERIFY(p2.image().size() == QSize(10000, 1000));

    QVERIFY(p2.image().cacheKey() != cachekey);
    }
}

// QTBUG-12729
void tst_qquickpixmapcache::cancelcrash()
{
    QUrl url = server.url("/cancelcrash_notexist.png");
    for (int ii = 0; ii < 1000; ++ii) {
        QQuickPixmap pix(&engine, url);
    }
}

class MyPixmapProvider : public QQuickImageProvider
{
public:
    MyPixmapProvider()
    : QQuickImageProvider(Pixmap) {}

    virtual QPixmap requestPixmap(const QString &d, QSize *, const QSize &) {
        Q_UNUSED(d)
        QPixmap pix(800, 600);
        pix.fill(fillColor);
        return pix;
    }

    static QRgb fillColor;
};

QRgb MyPixmapProvider::fillColor = qRgb(255, 0, 0);

// QTBUG-13345
void tst_qquickpixmapcache::shrinkcache()
{
    QQmlEngine engine;
    engine.addImageProvider(QLatin1String("mypixmaps"), new MyPixmapProvider);

    for (int ii = 0; ii < 4000; ++ii) {
        QUrl url("image://mypixmaps/" + QString::number(ii));
        QQuickPixmap p(&engine, url);
    }
}

#ifndef QT_NO_CONCURRENT

void createNetworkServer(TestHTTPServer *server)
{
   QEventLoop eventLoop;
   server->serveDirectory(QQmlDataTest::instance()->testFile("http"));
   QTimer::singleShot(100, &eventLoop, SLOT(quit()));
   eventLoop.exec();
}

#ifndef QT_NO_CONCURRENT
// QT-3957
void tst_qquickpixmapcache::networkCrash()
{
   TestHTTPServer server;
   QVERIFY2(server.listen(), qPrintable(server.errorString()));
    QFuture<void> future = QtConcurrent::run(createNetworkServer, &server);
    QQmlEngine engine;
    for (int ii = 0; ii < 100 ; ++ii) {
        QQuickPixmap* pixmap = new QQuickPixmap;
        pixmap->load(&engine,  server.url("/exists.png"));
        QTest::qSleep(1);
        pixmap->clear();
        delete pixmap;
    }
    future.cancel();
}
#endif

#endif

// QTBUG-22125
void tst_qquickpixmapcache::lockingCrash()
{
    TestHTTPServer server;
    QVERIFY2(server.listen(), qPrintable(server.errorString()));
    server.serveDirectory(testFile("http"), TestHTTPServer::Delay);

    {
        QQuickPixmap* p = new QQuickPixmap;
        {
            QQmlEngine e;
            p->load(&e,  server.url("/exists6.png"));
        }
        p->clear();
        QVERIFY(p->isNull());
        delete p;
        server.sendDelayedItem();
    }
}

void tst_qquickpixmapcache::uncached()
{
    QQmlEngine engine;
    engine.addImageProvider(QLatin1String("mypixmaps"), new MyPixmapProvider);

    QUrl url("image://mypixmaps/mypix");
    {
        QQuickPixmap p;
        p.load(&engine, url, 0);
        QImage img = p.image();
        QCOMPARE(img.pixel(0,0), qRgb(255, 0, 0));
    }

    // uncached, so we will get a different colored image
    MyPixmapProvider::fillColor = qRgb(0, 255, 0);
    {
        QQuickPixmap p;
        p.load(&engine, url, 0);
        QImage img = p.image();
        QCOMPARE(img.pixel(0,0), qRgb(0, 255, 0));
    }

    // Load the image with cache enabled
    MyPixmapProvider::fillColor = qRgb(0, 0, 255);
    {
        QQuickPixmap p;
        p.load(&engine, url, QQuickPixmap::Cache);
        QImage img = p.image();
        QCOMPARE(img.pixel(0,0), qRgb(0, 0, 255));
    }

    // We should not get the cached version if we request uncached
    MyPixmapProvider::fillColor = qRgb(255, 0, 255);
    {
        QQuickPixmap p;
        p.load(&engine, url, 0);
        QImage img = p.image();
        QCOMPARE(img.pixel(0,0), qRgb(255, 0, 255));
    }

    // If we again load the image with cache enabled, we should get the previously cached version
    MyPixmapProvider::fillColor = qRgb(0, 255, 255);
    {
        QQuickPixmap p;
        p.load(&engine, url, QQuickPixmap::Cache);
        QImage img = p.image();
        QCOMPARE(img.pixel(0,0), qRgb(0, 0, 255));
    }
}


#if PIXMAP_DATA_LEAK_TEST
// This test should not be enabled by default as it
// produces spurious output in the expected case.
#include <QtQuick/QQuickView>
class DataLeakView : public QQuickView
{
    Q_OBJECT

public:
    explicit DataLeakView() : QQuickView()
    {
        setSource(testFileUrl("dataLeak.qml"));
    }

    void showFor2Seconds()
    {
        showFullScreen();
        QTimer::singleShot(2000, this, SIGNAL(ready()));
    }

signals:
    void ready();
};

// QTBUG-22742
Q_GLOBAL_STATIC(QQuickPixmap, dataLeakPixmap)
void tst_qquickpixmapcache::dataLeak()
{
    // Should not leak cached QQuickPixmapData.
    // Unfortunately, since the QQuickPixmapStore
    // is a global static, and it releases the cache
    // entries on dtor (application exit), we must use
    // valgrind to determine whether it leaks or not.
    QQuickPixmap *p1 = new QQuickPixmap;
    QQuickPixmap *p2 = new QQuickPixmap;
    {
        QScopedPointer<DataLeakView> test(new DataLeakView);
        test->showFor2Seconds();
        dataLeakPixmap()->load(test->engine(), testFileUrl("exists.png"));
        p1->load(test->engine(), testFileUrl("exists.png"));
        p2->load(test->engine(), testFileUrl("exists2.png"));
        QTest::qWait(2005); // 2 seconds + a few more millis.
    }

    // When the (global static) dataLeakPixmap is deleted, it
    // shouldn't attempt to dereference a QQuickPixmapData
    // which has been deleted by the QQuickPixmapStore
    // destructor.
}
#endif
#undef PIXMAP_DATA_LEAK_TEST

QTEST_MAIN(tst_qquickpixmapcache)

#include "tst_qquickpixmapcache.moc"
