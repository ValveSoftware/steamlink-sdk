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
#include <QtTest/QtTest>
#include <QtDeclarative/qdeclarativeengine.h>
#include <QtDeclarative/qdeclarativeimageprovider.h>
#include <private/qdeclarativeimage_p.h>
#include <QImageReader>
#include <QWaitCondition>

Q_DECLARE_METATYPE(QDeclarativeImageProvider*);

class tst_qdeclarativeimageprovider : public QObject
{
    Q_OBJECT
public:
    tst_qdeclarativeimageprovider()
    {
    }

private slots:
    void requestImage_sync_data();
    void requestImage_sync();
    void requestImage_async_data();
    void requestImage_async();

    void requestPixmap_sync_data();
    void requestPixmap_sync();
    void requestPixmap_async();

    void removeProvider_data();
    void removeProvider();

    void threadTest();

private:
    QString newImageFileName() const;
    void fillRequestTestsData(const QString &id);
    void runTest(bool async, QDeclarativeImageProvider *provider);
};


class TestQImageProvider : public QDeclarativeImageProvider
{
public:
    TestQImageProvider(bool *deleteWatch = 0)
        : QDeclarativeImageProvider(Image), deleteWatch(deleteWatch)
    {
    }

    ~TestQImageProvider()
    {
        if (deleteWatch)
            *deleteWatch = true;
    }

    QImage requestImage(const QString &id, QSize *size, const QSize& requestedSize)
    {
        lastImageId = id;

        if (id == QLatin1String("no-such-file.png"))
            return QImage();

        int width = 100;
        int height = 100;
        QImage image(width, height, QImage::Format_RGB32);
        if (size)
            *size = QSize(width, height);
        if (requestedSize.isValid())
            image = image.scaled(requestedSize);
        return image;
    }

    bool *deleteWatch;
    QString lastImageId;
};
Q_DECLARE_METATYPE(TestQImageProvider*);


class TestQPixmapProvider : public QDeclarativeImageProvider
{
public:
    TestQPixmapProvider(bool *deleteWatch = 0)
        : QDeclarativeImageProvider(Pixmap), deleteWatch(deleteWatch)
    {
    }

    ~TestQPixmapProvider()
    {
        if (deleteWatch)
            *deleteWatch = true;
    }

    QPixmap requestPixmap(const QString &id, QSize *size, const QSize& requestedSize)
    {
        lastImageId = id;

        if (id == QLatin1String("no-such-file.png"))
            return QPixmap();

        int width = 100;
        int height = 100;
        QPixmap image(width, height);
        if (size)
            *size = QSize(width, height);
        if (requestedSize.isValid())
            image = image.scaled(requestedSize);
        return image;
    }

    bool *deleteWatch;
    QString lastImageId;
};
Q_DECLARE_METATYPE(TestQPixmapProvider*);


QString tst_qdeclarativeimageprovider::newImageFileName() const
{
    // need to generate new filenames each time or else images are loaded
    // from cache and we won't get loading status changes when testing
    // async loading
    static int count = 0;
    return QString("image://test/image-%1.png").arg(count++);
}

void tst_qdeclarativeimageprovider::fillRequestTestsData(const QString &id)
{
    QTest::addColumn<QString>("source");
    QTest::addColumn<QString>("imageId");
    QTest::addColumn<QString>("properties");
    QTest::addColumn<QSize>("size");
    QTest::addColumn<QString>("error");

    QString fileName = newImageFileName();
    QTest::newRow(QTest::toString(id + " simple test"))
            << "image://test/" + fileName << fileName << "" << QSize(100,100) << "";

    fileName = newImageFileName();
    QTest::newRow(QTest::toString(id + " simple test with capitalization"))//As it's a URL, should make no difference
            << "image://Test/" + fileName << fileName << "" << QSize(100,100) << "";

    fileName = newImageFileName();
    QTest::newRow(QTest::toString(id + " url with no id"))
        << "image://test/" + fileName << "" + fileName << "" << QSize(100,100) << "";

    fileName = newImageFileName();
    QTest::newRow(QTest::toString(id + " url with path"))
        << "image://test/test/path" + fileName << "test/path" + fileName << "" << QSize(100,100) << "";

    fileName = newImageFileName();
    QTest::newRow(QTest::toString(id + " url with fragment"))
        << "image://test/faq.html?#question13" + fileName << "faq.html?#question13" + fileName << "" << QSize(100,100) << "";

    fileName = newImageFileName();
    QTest::newRow(QTest::toString(id + " url with query"))
        << "image://test/cgi-bin/drawgraph.cgi?type=pie&color=green" + fileName << "cgi-bin/drawgraph.cgi?type=pie&color=green" + fileName
        << "" << QSize(100,100) << "";

    fileName = newImageFileName();
    QTest::newRow(QTest::toString(id + " scaled image"))
            << "image://test/" + fileName << fileName << "sourceSize: \"80x30\"" << QSize(80,30) << "";

    QTest::newRow(QTest::toString(id + " missing"))
        << "image://test/no-such-file.png" << "no-such-file.png" << "" << QSize(100,100)
        << "<Unknown File>:2:1: QML Image: Failed to get image from provider: image://test/no-such-file.png";

    QTest::newRow(QTest::toString(id + " unknown provider"))
        << "image://bogus/exists.png" << "" << "" << QSize()
        << "<Unknown File>:2:1: QML Image: Failed to get image from provider: image://bogus/exists.png";
}

void tst_qdeclarativeimageprovider::runTest(bool async, QDeclarativeImageProvider *provider)
{
    QFETCH(QString, source);
    QFETCH(QString, imageId);
    QFETCH(QString, properties);
    QFETCH(QSize, size);
    QFETCH(QString, error);

    if (!error.isEmpty())
        QTest::ignoreMessage(QtWarningMsg, error.toUtf8());

    QDeclarativeEngine engine;

    engine.addImageProvider("test", provider);
    QVERIFY(engine.imageProvider("test") != 0);

    QString componentStr = "import QtQuick 1.0\nImage { source: \"" + source + "\"; "
            + (async ? "asynchronous: true; " : "")
            + properties + " }";
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeImage *obj = qobject_cast<QDeclarativeImage*>(component.create());
    QVERIFY(obj != 0);

    if (async)
        QTRY_VERIFY(obj->status() == QDeclarativeImage::Loading);

    QCOMPARE(obj->source(), QUrl(source));

    if (error.isEmpty()) {
        if (async)
            QTRY_VERIFY(obj->status() == QDeclarativeImage::Ready);
        else
            QVERIFY(obj->status() == QDeclarativeImage::Ready);
        if (QByteArray(QTest::currentDataTag()).startsWith("qimage"))
            QCOMPARE(static_cast<TestQImageProvider*>(provider)->lastImageId, imageId);
        else
            QCOMPARE(static_cast<TestQPixmapProvider*>(provider)->lastImageId, imageId);

        QCOMPARE(obj->width(), qreal(size.width()));
        QCOMPARE(obj->height(), qreal(size.height()));
        QCOMPARE(obj->pixmap().width(), size.width());
        QCOMPARE(obj->pixmap().height(), size.height());
        QCOMPARE(obj->fillMode(), QDeclarativeImage::Stretch);
        QCOMPARE(obj->progress(), 1.0);
    } else {
        if (async)
            QTRY_VERIFY(obj->status() == QDeclarativeImage::Error);
        else
            QVERIFY(obj->status() == QDeclarativeImage::Error);
    }

    delete obj;
}

void tst_qdeclarativeimageprovider::requestImage_sync_data()
{
    fillRequestTestsData("qimage|sync");
}

void tst_qdeclarativeimageprovider::requestImage_sync()
{
    bool deleteWatch = false;
    runTest(false, new TestQImageProvider(&deleteWatch));
    QVERIFY(deleteWatch);
}

void tst_qdeclarativeimageprovider::requestImage_async_data()
{
    fillRequestTestsData("qimage|async");
}

void tst_qdeclarativeimageprovider::requestImage_async()
{
    bool deleteWatch = false;
    runTest(true, new TestQImageProvider(&deleteWatch));
    QVERIFY(deleteWatch);
}

void tst_qdeclarativeimageprovider::requestPixmap_sync_data()
{
    fillRequestTestsData("qpixmap");
}

void tst_qdeclarativeimageprovider::requestPixmap_sync()
{
    bool deleteWatch = false;
    runTest(false, new TestQPixmapProvider(&deleteWatch));
    QVERIFY(deleteWatch);
}

void tst_qdeclarativeimageprovider::requestPixmap_async()
{
    QDeclarativeEngine engine;
    QDeclarativeImageProvider *provider = new TestQPixmapProvider();

    engine.addImageProvider("test", provider);
    QVERIFY(engine.imageProvider("test") != 0);

    // pixmaps are loaded synchronously regardless of 'asynchronous' value
    QString componentStr = "import QtQuick 1.0\nImage { asynchronous: true; source: \"image://test/pixmap-async-test.png\" }";
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeImage *obj = qobject_cast<QDeclarativeImage*>(component.create());
    QVERIFY(obj != 0);

    delete obj;
}

void tst_qdeclarativeimageprovider::removeProvider_data()
{
    QTest::addColumn<QDeclarativeImageProvider*>("provider");

    QTest::newRow("qimage") << static_cast<QDeclarativeImageProvider*>(new TestQImageProvider);
    QTest::newRow("qpixmap") << static_cast<QDeclarativeImageProvider*>(new TestQPixmapProvider);
}

void tst_qdeclarativeimageprovider::removeProvider()
{
    QFETCH(QDeclarativeImageProvider*, provider);

    QDeclarativeEngine engine;

    engine.addImageProvider("test", provider);
    QVERIFY(engine.imageProvider("test") != 0);

    // add provider, confirm it works
    QString componentStr = "import QtQuick 1.0\nImage { source: \"" + newImageFileName() + "\" }";
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeImage *obj = qobject_cast<QDeclarativeImage*>(component.create());
    QVERIFY(obj != 0);

    QCOMPARE(obj->status(), QDeclarativeImage::Ready);

    // remove the provider and confirm
    QString fileName = newImageFileName();
    QString error("<Unknown File>:2:1: QML Image: Failed to get image from provider: " + fileName);
    QTest::ignoreMessage(QtWarningMsg, error.toUtf8());

    engine.removeImageProvider("test");

    obj->setSource(QUrl(fileName));
    QCOMPARE(obj->status(), QDeclarativeImage::Error);

    delete obj;
}

class TestThreadProvider : public QDeclarativeImageProvider
{
    public:
        TestThreadProvider() : QDeclarativeImageProvider(Image), ok(false) {}

        ~TestThreadProvider() {}

        QImage requestImage(const QString &id, QSize *size, const QSize& requestedSize)
        {
            mutex.lock();
            if (!ok)
                cond.wait(&mutex);
            mutex.unlock();
            QVector<int> v;
            for (int i = 0; i < 10000; i++)
                v.prepend(i); //do some computation
            QImage image(50,50, QImage::Format_RGB32);
            image.fill(QColor(id).rgb());
            if (size)
                *size = image.size();
            if (requestedSize.isValid())
                image = image.scaled(requestedSize);
            return image;
        }

        QWaitCondition cond;
        QMutex mutex;
        bool ok;
};


void tst_qdeclarativeimageprovider::threadTest()
{
    QDeclarativeEngine engine;

    TestThreadProvider *provider = new TestThreadProvider;

    engine.addImageProvider("test_thread", provider);
    QVERIFY(engine.imageProvider("test_thread") != 0);

    QString componentStr = "import QtQuick 1.0\nItem { \n"
            "Image { source: \"image://test_thread/blue\";  asynchronous: true; }\n"
            "Image { source: \"image://test_thread/red\";  asynchronous: true; }\n"
            "Image { source: \"image://test_thread/green\";  asynchronous: true; }\n"
            "Image { source: \"image://test_thread/yellow\";  asynchronous: true; }\n"
            " }";
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QObject *obj = component.create();
    //MUST not deadlock
    QVERIFY(obj != 0);
    QList<QDeclarativeImage *> images = obj->findChildren<QDeclarativeImage *>();
    QCOMPARE(images.count(), 4);
    QTest::qWait(100);
    foreach(QDeclarativeImage *img, images) {
        QCOMPARE(img->status(), QDeclarativeImage::Loading);
    }
    provider->ok = true;
    provider->cond.wakeAll();
    QTest::qWait(250);
    foreach(QDeclarativeImage *img, images) {
        QTRY_VERIFY(img->status() == QDeclarativeImage::Ready);
    }
}


QTEST_MAIN(tst_qdeclarativeimageprovider)

#include "tst_qdeclarativeimageprovider.moc"
