/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
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

#include <QtTest/QtTest>
#include <QDebug>
#include "qmediaservice.h"
#include "qmediaplaylist.h"
#include <private/qmediaplaylistcontrol_p.h>
#include <private/qmediaplaylistsourcecontrol_p.h>
#include <private/qmediaplaylistnavigator_p.h>
#include <private/qmediapluginloader_p.h>

#include "qm3uhandler.h"

//TESTED_COMPONENT=src/multimedia

#include "mockplaylistservice.h"
#include "mockmediaplaylistcontrol.h"
#include "mockmediaplaylistsourcecontrol.h"
#include "mockreadonlyplaylistprovider.h"

QT_USE_NAMESPACE

class MockReadOnlyPlaylistObject : public QMediaObject
{
    Q_OBJECT
public:
    MockReadOnlyPlaylistObject(QObject *parent = 0)
        :QMediaObject(parent, new MockPlaylistService)
    {
    }
};

class tst_QMediaPlaylist : public QObject
{
    Q_OBJECT
public slots:
    void init();
    void cleanup();
    void initTestCase();

private slots:
    void construction();
    void append();
    void insert();
    void clear();
    void removeMedia();
    void currentItem();
    void saveAndLoad();
    void loadM3uFile();
    void loadPLSFile();
    void playbackMode();
    void playbackMode_data();
    void shuffle();
    void readOnlyPlaylist();
    void setMediaObject();

    void testCurrentIndexChanged_signal();
    void testCurrentMediaChanged_signal();
    void testLoaded_signal();
    void testMediaChanged_signal();
    void testPlaybackModeChanged_signal();
    void testEnums();

    void mediaPlayListProvider();
    // TC for Abstract control classes
    void mediaPlayListControl();
    void mediaPlayListSourceControl();


private:
    QMediaContent content1;
    QMediaContent content2;
    QMediaContent content3;
};

void tst_QMediaPlaylist::init()
{
}

void tst_QMediaPlaylist::initTestCase()
{
    qRegisterMetaType<QMediaContent>();
    content1 = QMediaContent(QUrl(QLatin1String("file:///1")));
    content2 = QMediaContent(QUrl(QLatin1String("file:///2")));
    content3 = QMediaContent(QUrl(QLatin1String("file:///3")));

}

void tst_QMediaPlaylist::cleanup()
{
}

void tst_QMediaPlaylist::construction()
{
    QMediaPlaylist playlist;
    QCOMPARE(playlist.mediaCount(), 0);
    QVERIFY(playlist.isEmpty());
}

void tst_QMediaPlaylist::append()
{
    QMediaPlaylist playlist;
    QVERIFY(!playlist.isReadOnly());

    playlist.addMedia(content1);
    QCOMPARE(playlist.mediaCount(), 1);
    QCOMPARE(playlist.media(0), content1);

    QSignalSpy aboutToBeInsertedSignalSpy(&playlist, SIGNAL(mediaAboutToBeInserted(int,int)));
    QSignalSpy insertedSignalSpy(&playlist, SIGNAL(mediaInserted(int,int)));
    playlist.addMedia(content2);
    QCOMPARE(playlist.mediaCount(), 2);
    QCOMPARE(playlist.media(1), content2);

    QCOMPARE(aboutToBeInsertedSignalSpy.count(), 1);
    QCOMPARE(aboutToBeInsertedSignalSpy.first()[0].toInt(), 1);
    QCOMPARE(aboutToBeInsertedSignalSpy.first()[1].toInt(), 1);

    QCOMPARE(insertedSignalSpy.count(), 1);
    QCOMPARE(insertedSignalSpy.first()[0].toInt(), 1);
    QCOMPARE(insertedSignalSpy.first()[1].toInt(), 1);

    aboutToBeInsertedSignalSpy.clear();
    insertedSignalSpy.clear();

    QMediaContent content4(QUrl(QLatin1String("file:///4")));
    QMediaContent content5(QUrl(QLatin1String("file:///5")));
    playlist.addMedia(QList<QMediaContent>() << content3 << content4 << content5);
    QCOMPARE(playlist.mediaCount(), 5);
    QCOMPARE(playlist.media(2), content3);
    QCOMPARE(playlist.media(3), content4);
    QCOMPARE(playlist.media(4), content5);

    QCOMPARE(aboutToBeInsertedSignalSpy.count(), 1);
    QCOMPARE(aboutToBeInsertedSignalSpy[0][0].toInt(), 2);
    QCOMPARE(aboutToBeInsertedSignalSpy[0][1].toInt(), 4);

    QCOMPARE(insertedSignalSpy.count(), 1);
    QCOMPARE(insertedSignalSpy[0][0].toInt(), 2);
    QCOMPARE(insertedSignalSpy[0][1].toInt(), 4);

    aboutToBeInsertedSignalSpy.clear();
    insertedSignalSpy.clear();

    playlist.addMedia(QList<QMediaContent>());
    QCOMPARE(aboutToBeInsertedSignalSpy.count(), 0);
    QCOMPARE(insertedSignalSpy.count(), 0);
}

void tst_QMediaPlaylist::insert()
{
    QMediaPlaylist playlist;
    QVERIFY(!playlist.isReadOnly());

    playlist.addMedia(content1);
    QCOMPARE(playlist.mediaCount(), 1);
    QCOMPARE(playlist.media(0), content1);

    playlist.addMedia(content2);
    QCOMPARE(playlist.mediaCount(), 2);
    QCOMPARE(playlist.media(1), content2);

    QSignalSpy aboutToBeInsertedSignalSpy(&playlist, SIGNAL(mediaAboutToBeInserted(int,int)));
    QSignalSpy insertedSignalSpy(&playlist, SIGNAL(mediaInserted(int,int)));

    playlist.insertMedia(1, content3);
    QCOMPARE(playlist.mediaCount(), 3);
    QCOMPARE(playlist.media(0), content1);
    QCOMPARE(playlist.media(1), content3);
    QCOMPARE(playlist.media(2), content2);

    QCOMPARE(aboutToBeInsertedSignalSpy.count(), 1);
    QCOMPARE(aboutToBeInsertedSignalSpy.first()[0].toInt(), 1);
    QCOMPARE(aboutToBeInsertedSignalSpy.first()[1].toInt(), 1);

    QCOMPARE(insertedSignalSpy.count(), 1);
    QCOMPARE(insertedSignalSpy.first()[0].toInt(), 1);
    QCOMPARE(insertedSignalSpy.first()[1].toInt(), 1);

    aboutToBeInsertedSignalSpy.clear();
    insertedSignalSpy.clear();

    QMediaContent content4(QUrl(QLatin1String("file:///4")));
    QMediaContent content5(QUrl(QLatin1String("file:///5")));
    playlist.insertMedia(1, QList<QMediaContent>() << content4 << content5);

    QCOMPARE(playlist.media(0), content1);
    QCOMPARE(playlist.media(1), content4);
    QCOMPARE(playlist.media(2), content5);
    QCOMPARE(playlist.media(3), content3);
    QCOMPARE(playlist.media(4), content2);
    QCOMPARE(aboutToBeInsertedSignalSpy.count(), 1);
    QCOMPARE(aboutToBeInsertedSignalSpy[0][0].toInt(), 1);
    QCOMPARE(aboutToBeInsertedSignalSpy[0][1].toInt(), 2);

    QCOMPARE(insertedSignalSpy.count(), 1);
    QCOMPARE(insertedSignalSpy[0][0].toInt(), 1);
    QCOMPARE(insertedSignalSpy[0][1].toInt(), 2);

    aboutToBeInsertedSignalSpy.clear();
    insertedSignalSpy.clear();

    playlist.insertMedia(1, QList<QMediaContent>());
    QCOMPARE(aboutToBeInsertedSignalSpy.count(), 0);
    QCOMPARE(insertedSignalSpy.count(), 0);
}


void tst_QMediaPlaylist::currentItem()
{
    QMediaPlaylist playlist;
    playlist.addMedia(content1);
    playlist.addMedia(content2);

    QCOMPARE(playlist.currentIndex(), -1);
    QCOMPARE(playlist.currentMedia(), QMediaContent());

    QCOMPARE(playlist.nextIndex(), 0);
    QCOMPARE(playlist.nextIndex(2), 1);
    QCOMPARE(playlist.previousIndex(), 1);
    QCOMPARE(playlist.previousIndex(2), 0);

    playlist.setCurrentIndex(0);
    QCOMPARE(playlist.currentIndex(), 0);
    QCOMPARE(playlist.currentMedia(), content1);

    QCOMPARE(playlist.nextIndex(), 1);
    QCOMPARE(playlist.nextIndex(2), -1);
    QCOMPARE(playlist.previousIndex(), -1);
    QCOMPARE(playlist.previousIndex(2), -1);

    playlist.setCurrentIndex(1);
    QCOMPARE(playlist.currentIndex(), 1);
    QCOMPARE(playlist.currentMedia(), content2);

    QCOMPARE(playlist.nextIndex(), -1);
    QCOMPARE(playlist.nextIndex(2), -1);
    QCOMPARE(playlist.previousIndex(), 0);
    QCOMPARE(playlist.previousIndex(2), -1);

    playlist.setCurrentIndex(2);

    QCOMPARE(playlist.currentIndex(), -1);
    QCOMPARE(playlist.currentMedia(), QMediaContent());
}

void tst_QMediaPlaylist::clear()
{
    QMediaPlaylist playlist;
    playlist.addMedia(content1);
    playlist.addMedia(content2);

    playlist.clear();
    QVERIFY(playlist.isEmpty());
    QCOMPARE(playlist.mediaCount(), 0);
}

void tst_QMediaPlaylist::removeMedia()
{
    QMediaPlaylist playlist;
    playlist.addMedia(content1);
    playlist.addMedia(content2);
    playlist.addMedia(content3);

    QSignalSpy aboutToBeRemovedSignalSpy(&playlist, SIGNAL(mediaAboutToBeRemoved(int,int)));
    QSignalSpy removedSignalSpy(&playlist, SIGNAL(mediaRemoved(int,int)));
    playlist.removeMedia(1);
    QCOMPARE(playlist.mediaCount(), 2);
    QCOMPARE(playlist.media(1), content3);

    QCOMPARE(aboutToBeRemovedSignalSpy.count(), 1);
    QCOMPARE(aboutToBeRemovedSignalSpy.first()[0].toInt(), 1);
    QCOMPARE(aboutToBeRemovedSignalSpy.first()[1].toInt(), 1);

    QCOMPARE(removedSignalSpy.count(), 1);
    QCOMPARE(removedSignalSpy.first()[0].toInt(), 1);
    QCOMPARE(removedSignalSpy.first()[1].toInt(), 1);

    aboutToBeRemovedSignalSpy.clear();
    removedSignalSpy.clear();

    playlist.removeMedia(0,1);
    QVERIFY(playlist.isEmpty());

    QCOMPARE(aboutToBeRemovedSignalSpy.count(), 1);
    QCOMPARE(aboutToBeRemovedSignalSpy.first()[0].toInt(), 0);
    QCOMPARE(aboutToBeRemovedSignalSpy.first()[1].toInt(), 1);

    QCOMPARE(removedSignalSpy.count(), 1);
    QCOMPARE(removedSignalSpy.first()[0].toInt(), 0);
    QCOMPARE(removedSignalSpy.first()[1].toInt(), 1);


    playlist.addMedia(content1);
    playlist.addMedia(content2);
    playlist.addMedia(content3);

    playlist.removeMedia(0,1);
    QCOMPARE(playlist.mediaCount(), 1);
    QCOMPARE(playlist.media(0), content3);
}

void tst_QMediaPlaylist::saveAndLoad()
{
    QMediaPlaylist playlist;
    playlist.addMedia(content1);
    playlist.addMedia(content2);
    playlist.addMedia(content3);

    QCOMPARE(playlist.error(), QMediaPlaylist::NoError);
    QVERIFY(playlist.errorString().isEmpty());

    QBuffer buffer;
    buffer.open(QBuffer::ReadWrite);

    bool res = playlist.save(&buffer, "unsupported_format");
    QVERIFY(!res);
    QVERIFY(playlist.error() == QMediaPlaylist::FormatNotSupportedError);
    QVERIFY(!playlist.errorString().isEmpty());

    QSignalSpy loadedSignal(&playlist, SIGNAL(loaded()));
    QSignalSpy errorSignal(&playlist, SIGNAL(loadFailed()));
    playlist.load(&buffer, "unsupported_format");
    QTRY_VERIFY(loadedSignal.isEmpty());
    QCOMPARE(errorSignal.size(), 1);
    QVERIFY(playlist.error() != QMediaPlaylist::NoError);
    QVERIFY(!playlist.errorString().isEmpty());

    res = playlist.save(QUrl::fromLocalFile(QLatin1String("tmp.unsupported_format")), "unsupported_format");
    QVERIFY(!res);
    QVERIFY(playlist.error() != QMediaPlaylist::NoError);
    QVERIFY(!playlist.errorString().isEmpty());

    loadedSignal.clear();
    errorSignal.clear();
    playlist.load(QUrl::fromLocalFile(QLatin1String("tmp.unsupported_format")), "unsupported_format");
    QTRY_VERIFY(loadedSignal.isEmpty());
    QCOMPARE(errorSignal.size(), 1);
    QVERIFY(playlist.error() == QMediaPlaylist::FormatNotSupportedError);
    QVERIFY(!playlist.errorString().isEmpty());

    res = playlist.save(&buffer, "m3u");

    QVERIFY(res);
    QVERIFY(buffer.pos() > 0);
    buffer.seek(0);

    QMediaPlaylist playlist2;
    QSignalSpy loadedSignal2(&playlist2, SIGNAL(loaded()));
    QSignalSpy errorSignal2(&playlist2, SIGNAL(loadFailed()));
    playlist2.load(&buffer, "m3u");
    QCOMPARE(loadedSignal2.size(), 1);
    QTRY_VERIFY(errorSignal2.isEmpty());
    QCOMPARE(playlist.error(), QMediaPlaylist::NoError);

    QCOMPARE(playlist.mediaCount(), playlist2.mediaCount());
    QCOMPARE(playlist.media(0), playlist2.media(0));
    QCOMPARE(playlist.media(1), playlist2.media(1));
    QCOMPARE(playlist.media(3), playlist2.media(3));
    res = playlist.save(QUrl::fromLocalFile(QLatin1String("tmp.m3u")), "m3u");
    QVERIFY(res);

    loadedSignal2.clear();
    errorSignal2.clear();
    playlist2.clear();
    QVERIFY(playlist2.isEmpty());
    playlist2.load(QUrl::fromLocalFile(QLatin1String("tmp.m3u")), "m3u");
    QCOMPARE(loadedSignal2.size(), 1);
    QTRY_VERIFY(errorSignal2.isEmpty());
    QCOMPARE(playlist.error(), QMediaPlaylist::NoError);

    QCOMPARE(playlist.mediaCount(), playlist2.mediaCount());
    QCOMPARE(playlist.media(0), playlist2.media(0));
    QCOMPARE(playlist.media(1), playlist2.media(1));
    QCOMPARE(playlist.media(3), playlist2.media(3));
}

void tst_QMediaPlaylist::loadM3uFile()
{
    QMediaPlaylist playlist;

    // Try to load playlist that does not exist in the testdata folder
    QSignalSpy loadSpy(&playlist, SIGNAL(loaded()));
    QSignalSpy loadFailedSpy(&playlist, SIGNAL(loadFailed()));
    QString testFileName = QFINDTESTDATA("testdata");
    playlist.load(QUrl::fromLocalFile(testFileName + "/missing_file.m3u"));
    QTRY_VERIFY(loadSpy.isEmpty());
    QVERIFY(!loadFailedSpy.isEmpty());
    QVERIFY(playlist.error() != QMediaPlaylist::NoError);

    loadSpy.clear();
    loadFailedSpy.clear();
    testFileName = QFINDTESTDATA("testdata/test.m3u");
    playlist.load(QUrl::fromLocalFile(testFileName));
    QTRY_VERIFY(!loadSpy.isEmpty());
    QVERIFY(loadFailedSpy.isEmpty());
    QCOMPARE(playlist.error(), QMediaPlaylist::NoError);
    QCOMPARE(playlist.mediaCount(), 7);

    QCOMPARE(playlist.media(0).canonicalUrl(), QUrl(QLatin1String("http://test.host/path")));
    QCOMPARE(playlist.media(1).canonicalUrl(), QUrl(QLatin1String("http://test.host/path")));
    testFileName = QFINDTESTDATA("testdata/testfile");
    QCOMPARE(playlist.media(2).canonicalUrl(),
             QUrl::fromLocalFile(testFileName));
    testFileName = QFINDTESTDATA("testdata");
    QCOMPARE(playlist.media(3).canonicalUrl(),
             QUrl::fromLocalFile(testFileName + "/testdir/testfile"));
    QCOMPARE(playlist.media(4).canonicalUrl(), QUrl(QLatin1String("file:///testdir/testfile")));
    QCOMPARE(playlist.media(5).canonicalUrl(), QUrl(QLatin1String("file://path/name#suffix")));
    //ensure #2 suffix is not stripped from path
    testFileName = QFINDTESTDATA("testdata/testfile2#suffix");
    QCOMPARE(playlist.media(6).canonicalUrl(), QUrl::fromLocalFile(testFileName));

    // check ability to load from QNetworkRequest
    loadSpy.clear();
    loadFailedSpy.clear();
    playlist.load(QNetworkRequest(QUrl::fromLocalFile(QFINDTESTDATA("testdata/test.m3u"))));
    QTRY_VERIFY(!loadSpy.isEmpty());
    QVERIFY(loadFailedSpy.isEmpty());
}

void tst_QMediaPlaylist::loadPLSFile()
{
    QMediaPlaylist playlist;

    // Try to load playlist that does not exist in the testdata folder
    QSignalSpy loadSpy(&playlist, SIGNAL(loaded()));
    QSignalSpy loadFailedSpy(&playlist, SIGNAL(loadFailed()));
    QString testFileName = QFINDTESTDATA("testdata");
    playlist.load(QUrl::fromLocalFile(testFileName + "/missing_file.pls"));
    QTRY_VERIFY(loadSpy.isEmpty());
    QVERIFY(!loadFailedSpy.isEmpty());
    QVERIFY(playlist.error() != QMediaPlaylist::NoError);

    // Try to load empty playlist
    loadSpy.clear();
    loadFailedSpy.clear();
    testFileName = QFINDTESTDATA("testdata/empty.pls");
    playlist.load(QUrl::fromLocalFile(testFileName));
    QTRY_VERIFY(!loadSpy.isEmpty());
    QVERIFY(loadFailedSpy.isEmpty());
    QCOMPARE(playlist.error(), QMediaPlaylist::NoError);
    QCOMPARE(playlist.mediaCount(), 0);

    // Try to load regular playlist
    loadSpy.clear();
    loadFailedSpy.clear();
    testFileName = QFINDTESTDATA("testdata/test.pls");
    playlist.load(QUrl::fromLocalFile(testFileName));
    QTRY_VERIFY(!loadSpy.isEmpty());
    QVERIFY(loadFailedSpy.isEmpty());
    QCOMPARE(playlist.error(), QMediaPlaylist::NoError);
    QCOMPARE(playlist.mediaCount(), 7);

    QCOMPARE(playlist.media(0).canonicalUrl(), QUrl(QLatin1String("http://test.host/path")));
    QCOMPARE(playlist.media(1).canonicalUrl(), QUrl(QLatin1String("http://test.host/path")));
    testFileName = QFINDTESTDATA("testdata/testfile");
    QCOMPARE(playlist.media(2).canonicalUrl(),
             QUrl::fromLocalFile(testFileName));
    testFileName = QFINDTESTDATA("testdata");
    QCOMPARE(playlist.media(3).canonicalUrl(),
             QUrl::fromLocalFile(testFileName + "/testdir/testfile"));
    QCOMPARE(playlist.media(4).canonicalUrl(), QUrl(QLatin1String("file:///testdir/testfile")));
    QCOMPARE(playlist.media(5).canonicalUrl(), QUrl(QLatin1String("file://path/name#suffix")));
    //ensure #2 suffix is not stripped from path
    testFileName = QFINDTESTDATA("testdata/testfile2#suffix");
    QCOMPARE(playlist.media(6).canonicalUrl(), QUrl::fromLocalFile(testFileName));

    // Try to load a totem-pl generated playlist
    // (Format doesn't respect the spec)
    loadSpy.clear();
    loadFailedSpy.clear();
    playlist.clear();
    testFileName = QFINDTESTDATA("testdata/totem-pl-example.pls");
    playlist.load(QUrl::fromLocalFile(testFileName));
    QTRY_VERIFY(!loadSpy.isEmpty());
    QVERIFY(loadFailedSpy.isEmpty());
    QCOMPARE(playlist.error(), QMediaPlaylist::NoError);
    QCOMPARE(playlist.mediaCount(), 1);
    QCOMPARE(playlist.media(0).canonicalUrl(), QUrl(QLatin1String("http://test.host/path")));


    // check ability to load from QNetworkRequest
    loadSpy.clear();
    loadFailedSpy.clear();
    playlist.load(QNetworkRequest(QUrl::fromLocalFile(QFINDTESTDATA("testdata/test.pls"))));
    QTRY_VERIFY(!loadSpy.isEmpty());
    QVERIFY(loadFailedSpy.isEmpty());
}

void tst_QMediaPlaylist::playbackMode_data()
{
    QTest::addColumn<QMediaPlaylist::PlaybackMode>("playbackMode");
    QTest::addColumn<int>("expectedPrevious");
    QTest::addColumn<int>("pos");
    QTest::addColumn<int>("expectedNext");

    QTest::newRow("Sequential, 0") << QMediaPlaylist::Sequential << -1 << 0 << 1;
    QTest::newRow("Sequential, 1") << QMediaPlaylist::Sequential << 0 << 1 << 2;
    QTest::newRow("Sequential, 2") << QMediaPlaylist::Sequential << 1 << 2 << -1;

    QTest::newRow("Loop, 0") << QMediaPlaylist::Loop << 2 << 0 << 1;
    QTest::newRow("Loop, 1") << QMediaPlaylist::Loop << 0 << 1 << 2;
    QTest::newRow("Lopp, 2") << QMediaPlaylist::Loop << 1 << 2 << 0;

    QTest::newRow("ItemOnce, 1") << QMediaPlaylist::CurrentItemOnce << -1 << 1 << -1;
    QTest::newRow("ItemInLoop, 1") << QMediaPlaylist::CurrentItemInLoop << 1 << 1 << 1;

    // Bit difficult to test random this way
}

void tst_QMediaPlaylist::playbackMode()
{
    QFETCH(QMediaPlaylist::PlaybackMode, playbackMode);
    QFETCH(int, expectedPrevious);
    QFETCH(int, pos);
    QFETCH(int, expectedNext);

    QMediaPlaylist playlist;
    playlist.addMedia(content1);
    playlist.addMedia(content2);
    playlist.addMedia(content3);

    QCOMPARE(playlist.playbackMode(), QMediaPlaylist::Sequential);
    QCOMPARE(playlist.currentIndex(), -1);

    playlist.setPlaybackMode(playbackMode);
    QCOMPARE(playlist.playbackMode(), playbackMode);

    playlist.setCurrentIndex(pos);
    QCOMPARE(playlist.currentIndex(), pos);
    QCOMPARE(playlist.nextIndex(), expectedNext);
    QCOMPARE(playlist.previousIndex(), expectedPrevious);

    playlist.next();
    QCOMPARE(playlist.currentIndex(), expectedNext);

    playlist.setCurrentIndex(pos);
    playlist.previous();
    QCOMPARE(playlist.currentIndex(), expectedPrevious);
}

void tst_QMediaPlaylist::shuffle()
{
    QMediaPlaylist playlist;
    QList<QMediaContent> contentList;

    for (int i=0; i<100; i++) {
        QMediaContent content(QUrl::fromLocalFile(QString::number(i)));
        contentList.append(content);
        playlist.addMedia(content);
    }

    playlist.shuffle();

    QList<QMediaContent> shuffledContentList;
    for (int i=0; i<playlist.mediaCount(); i++)
        shuffledContentList.append(playlist.media(i));

    QVERIFY(contentList != shuffledContentList);

}

void tst_QMediaPlaylist::readOnlyPlaylist()
{
    MockReadOnlyPlaylistObject mediaObject;
    QMediaPlaylist playlist;
    mediaObject.bind(&playlist);

    QVERIFY(playlist.isReadOnly());
    QVERIFY(!playlist.isEmpty());
    QCOMPARE(playlist.mediaCount(), 3);

    QCOMPARE(playlist.media(0), content1);
    QCOMPARE(playlist.media(1), content2);
    QCOMPARE(playlist.media(2), content3);
    QCOMPARE(playlist.media(3), QMediaContent());

    //it's a read only playlist, so all the modification should fail
    QVERIFY(!playlist.addMedia(content1));
    QCOMPARE(playlist.mediaCount(), 3);
    QVERIFY(!playlist.addMedia(QList<QMediaContent>() << content1 << content2));
    QCOMPARE(playlist.mediaCount(), 3);
    QVERIFY(!playlist.insertMedia(1, content1));
    QCOMPARE(playlist.mediaCount(), 3);
    QVERIFY(!playlist.insertMedia(1, QList<QMediaContent>() << content1 << content2));
    QCOMPARE(playlist.mediaCount(), 3);
    QVERIFY(!playlist.removeMedia(1));
    QCOMPARE(playlist.mediaCount(), 3);
    QVERIFY(!playlist.removeMedia(0,2));
    QCOMPARE(playlist.mediaCount(), 3);
    QVERIFY(!playlist.clear());
    QCOMPARE(playlist.mediaCount(), 3);

    //but it is still allowed to append/insert an empty list
    QVERIFY(playlist.addMedia(QList<QMediaContent>()));
    QVERIFY(playlist.insertMedia(1, QList<QMediaContent>()));

    playlist.shuffle();
    //it's still the same
    QCOMPARE(playlist.media(0), content1);
    QCOMPARE(playlist.media(1), content2);
    QCOMPARE(playlist.media(2), content3);
    QCOMPARE(playlist.media(3), QMediaContent());


    //load to read only playlist should fail,
    //unless underlaying provider supports it
    QBuffer buffer;
    buffer.open(QBuffer::ReadWrite);
    buffer.write(QByteArray("file:///1\nfile:///2"));
    buffer.seek(0);

    QSignalSpy errorSignal(&playlist, SIGNAL(loadFailed()));
    playlist.load(&buffer, "m3u");
    QCOMPARE(errorSignal.size(), 1);
    QCOMPARE(playlist.error(), QMediaPlaylist::AccessDeniedError);
    QVERIFY(!playlist.errorString().isEmpty());
    QCOMPARE(playlist.mediaCount(), 3);

    errorSignal.clear();
    playlist.load(QUrl::fromLocalFile(QLatin1String("tmp.m3u")), "m3u");

    QCOMPARE(errorSignal.size(), 1);
    QCOMPARE(playlist.error(), QMediaPlaylist::AccessDeniedError);
    QVERIFY(!playlist.errorString().isEmpty());
    QCOMPARE(playlist.mediaCount(), 3);
}

void tst_QMediaPlaylist::setMediaObject()
{
    MockReadOnlyPlaylistObject mediaObject;

    QMediaPlaylist playlist;
    QVERIFY(playlist.mediaObject() == 0);
    QVERIFY(!playlist.isReadOnly());

    mediaObject.bind(&playlist);
    QCOMPARE(playlist.mediaObject(), qobject_cast<QMediaObject*>(&mediaObject));
    QCOMPARE(playlist.mediaCount(), 3);
    QVERIFY(playlist.isReadOnly());

    mediaObject.unbind(&playlist);
    QVERIFY(playlist.mediaObject() == 0);
    QCOMPARE(playlist.mediaCount(), 0);
    QVERIFY(!playlist.isReadOnly());

    mediaObject.bind(&playlist);
    QCOMPARE(playlist.mediaObject(), qobject_cast<QMediaObject*>(&mediaObject));
    QCOMPARE(playlist.mediaCount(), 3);
    QVERIFY(playlist.isReadOnly());
}

void tst_QMediaPlaylist::testCurrentIndexChanged_signal()
{
    //create an instance of QMediaPlaylist class.
    QMediaPlaylist playlist;
    playlist.addMedia(content1); //set the media to playlist
    playlist.addMedia(content2); //set the media to playlist

    QSignalSpy spy(&playlist, SIGNAL(currentIndexChanged(int)));
    QVERIFY(spy.size()== 0);
    QCOMPARE(playlist.currentIndex(), -1);

    //set the current index for playlist.
    playlist.setCurrentIndex(0);
    QVERIFY(spy.size()== 1); //verify the signal emission.
    QCOMPARE(playlist.currentIndex(), 0); //verify the current index of playlist

    //set the current index for playlist.
    playlist.setCurrentIndex(1);
    QVERIFY(spy.size()== 2); //verify the signal emission.
    QCOMPARE(playlist.currentIndex(), 1); //verify the current index of playlist
}

void tst_QMediaPlaylist::testCurrentMediaChanged_signal()
{
    //create an instance of QMediaPlaylist class.
    QMediaPlaylist playlist;
    playlist.addMedia(content1); //set the media to playlist
    playlist.addMedia(content2); //set the media to playlist

    QSignalSpy spy(&playlist, SIGNAL(currentMediaChanged(QMediaContent)));
    QVERIFY(spy.size()== 0);
    QCOMPARE(playlist.currentIndex(), -1);
    QCOMPARE(playlist.currentMedia(), QMediaContent());

    //set the current index for playlist.
    playlist.setCurrentIndex(0);
    QVERIFY(spy.size()== 1); //verify the signal emission.
    QCOMPARE(playlist.currentIndex(), 0); //verify the current index of playlist
    QCOMPARE(playlist.currentMedia(), content1); //verify the current media of playlist

    //set the current index for playlist.
    playlist.setCurrentIndex(1);
    QVERIFY(spy.size()== 2);  //verify the signal emission.
    QCOMPARE(playlist.currentIndex(), 1); //verify the current index of playlist
    QCOMPARE(playlist.currentMedia(), content2); //verify the current media of playlist
}

void tst_QMediaPlaylist::testLoaded_signal()
{
    //create an instance of QMediaPlaylist class.
    QMediaPlaylist playlist;
    playlist.addMedia(content1); //set the media to playlist
    playlist.addMedia(content2); //set the media to playlist
    playlist.addMedia(content3); //set the media to playlist

    QSignalSpy spy(&playlist, SIGNAL(loaded()));
    QVERIFY(spy.size()== 0);

    QBuffer buffer;
    buffer.open(QBuffer::ReadWrite);

    //load the playlist
    playlist.load(&buffer,"m3u");
    QVERIFY(spy.size()== 1); //verify the signal emission.
}

void tst_QMediaPlaylist::testMediaChanged_signal()
{
    //create an instance of QMediaPlaylist class.
    QMediaPlaylist playlist;

    QSignalSpy spy(&playlist, SIGNAL(mediaChanged(int,int)));

    // Add media to playlist
    playlist.addMedia(content1); //set the media to playlist
    playlist.addMedia(content2); //set the media to playlist
    playlist.addMedia(content3); //set the media to playlist

    // Adds/inserts do not cause change signals
    QVERIFY(spy.size() == 0);

    // Now change the list
    playlist.shuffle();

    QVERIFY(spy.size() == 1);
    spy.clear();

    //create media.
    QMediaContent content4(QUrl(QLatin1String("file:///4")));
    QMediaContent content5(QUrl(QLatin1String("file:///5")));

    //insert media to playlist
    playlist.insertMedia(1, content4);
    playlist.insertMedia(2, content5);
    // Adds/inserts do not cause change signals
    QVERIFY(spy.size() == 0);

    // And again
    playlist.shuffle();

    QVERIFY(spy.size() == 1);
}

void tst_QMediaPlaylist::testPlaybackModeChanged_signal()
{
    //create an instance of QMediaPlaylist class.
    QMediaPlaylist playlist;
    playlist.addMedia(content1); //set the media to playlist
    playlist.addMedia(content2); //set the media to playlist
    playlist.addMedia(content3); //set the media to playlist

    QSignalSpy spy(&playlist, SIGNAL(playbackModeChanged(QMediaPlaylist::PlaybackMode)));
    QVERIFY(playlist.playbackMode()== QMediaPlaylist::Sequential);
    QVERIFY(spy.size() == 0);

    // Set playback mode to the playlist
    playlist.setPlaybackMode(QMediaPlaylist::CurrentItemOnce);
    QVERIFY(playlist.playbackMode()== QMediaPlaylist::CurrentItemOnce);
    QVERIFY(spy.size() == 1);

    // Set playback mode to the playlist
    playlist.setPlaybackMode(QMediaPlaylist::CurrentItemInLoop);
    QVERIFY(playlist.playbackMode()== QMediaPlaylist::CurrentItemInLoop);
    QVERIFY(spy.size() == 2);

    // Set playback mode to the playlist
    playlist.setPlaybackMode(QMediaPlaylist::Sequential);
    QVERIFY(playlist.playbackMode()== QMediaPlaylist::Sequential);
    QVERIFY(spy.size() == 3);

    // Set playback mode to the playlist
    playlist.setPlaybackMode(QMediaPlaylist::Loop);
    QVERIFY(playlist.playbackMode()== QMediaPlaylist::Loop);
    QVERIFY(spy.size() == 4);

    // Set playback mode to the playlist
    playlist.setPlaybackMode(QMediaPlaylist::Random);
    QVERIFY(playlist.playbackMode()== QMediaPlaylist::Random);
    QVERIFY(spy.size() == 5);
}

void tst_QMediaPlaylist::testEnums()
{
    //create an instance of QMediaPlaylist class.
    QMediaPlaylist playlist;
    playlist.addMedia(content1); //set the media to playlist
    playlist.addMedia(content2); //set the media to playlist
    playlist.addMedia(content3); //set the media to playlist
    QCOMPARE(playlist.error(), QMediaPlaylist::NoError);

    QBuffer buffer;
    buffer.open(QBuffer::ReadWrite);

    // checking for QMediaPlaylist::FormatNotSupportedError enum
    QVERIFY(!playlist.save(&buffer, "unsupported_format"));
    QVERIFY(playlist.error() == QMediaPlaylist::FormatNotSupportedError);

    playlist.load(&buffer,"unsupported_format");
    QVERIFY(playlist.error() == QMediaPlaylist::FormatNotSupportedError);
}

// MaemoAPI-1849:test QMediaPlayListControl constructor
void tst_QMediaPlaylist::mediaPlayListControl()
{
    // To check changes in abstract classe's pure virtual functions
    QObject parent;
    MockMediaPlaylistControl plylistctrl(&parent);
}

// MaemoAPI-1850:test QMediaPlayListSourceControl constructor
void tst_QMediaPlaylist::mediaPlayListSourceControl()
{
    // To check changes in abstract classe's pure virtual functions
    QObject parent;
    MockPlaylistSourceControl plylistsrcctrl(&parent);
}

// MaemoAPI-1852:test constructor
void tst_QMediaPlaylist::mediaPlayListProvider()
{
    // srcs of QMediaPlaylistProvider is incomplete
    QObject parent;
    MockReadOnlyPlaylistProvider provider(&parent);
}

QTEST_MAIN(tst_QMediaPlaylist)
#include "tst_qmediaplaylist.moc"

