/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

//TESTED_COMPONENT=src/multimedia

#include <QtTest/QtTest>
#include <QDebug>
#include <private/qmedianetworkplaylistprovider_p.h>
#include <private/qmediaplaylistnavigator_p.h>

QT_USE_NAMESPACE
class tst_QMediaPlaylistNavigator : public QObject
{
    Q_OBJECT
public slots:
    void init();
    void cleanup();

private slots:
    void construction();
    void setPlaylist();
    void linearPlayback();
    void loopPlayback();
    void currentItemOnce();
    void currentItemInLoop();
    void randomPlayback();

    void testItemAt();
    void testNextIndex();
    void testPreviousIndex();
    void testCurrentIndexChangedSignal();
    void testPlaybackModeChangedSignal();
    void testSurroundingItemsChangedSignal();
    void testActivatedSignal();
};

void tst_QMediaPlaylistNavigator::init()
{
    qRegisterMetaType<QMediaPlaylist::PlaybackMode>("QMediaPlaylist::PlaybackMode");
    qRegisterMetaType<QMediaContent>("QMediaContent");
}

void tst_QMediaPlaylistNavigator::cleanup()
{
}

void tst_QMediaPlaylistNavigator::construction()
{
    QMediaNetworkPlaylistProvider playlist;
    QCOMPARE(playlist.mediaCount(), 0);

    QMediaPlaylistNavigator navigator(&playlist);
    QVERIFY(navigator.currentItem().isNull());
    QCOMPARE(navigator.currentIndex(), -1);
}

void tst_QMediaPlaylistNavigator::setPlaylist()
{
    QMediaPlaylistNavigator navigator(0);
    QVERIFY(navigator.playlist() != 0);
    QCOMPARE(navigator.playlist()->mediaCount(), 0);
    QCOMPARE(navigator.playlist()->media(0), QMediaContent());
    QVERIFY(navigator.playlist()->isReadOnly() );

    QMediaNetworkPlaylistProvider playlist;
    QCOMPARE(playlist.mediaCount(), 0);

    navigator.setPlaylist(&playlist);
    QCOMPARE(navigator.playlist(), (QMediaPlaylistProvider*)&playlist);
    QCOMPARE(navigator.playlist()->mediaCount(), 0);
    QVERIFY(!navigator.playlist()->isReadOnly() );
}

void tst_QMediaPlaylistNavigator::linearPlayback()
{
    QMediaNetworkPlaylistProvider playlist;
    QMediaPlaylistNavigator navigator(&playlist);

    navigator.setPlaybackMode(QMediaPlaylist::Sequential);
    navigator.jump(0);
    QVERIFY(navigator.currentItem().isNull());
    QCOMPARE(navigator.currentIndex(), -1);

    QMediaContent content1(QUrl(QLatin1String("file:///1")));
    playlist.addMedia(content1);
    navigator.jump(0);
    QVERIFY(!navigator.currentItem().isNull());

    QCOMPARE(navigator.currentIndex(), 0);
    QCOMPARE(navigator.currentItem(), content1);
    QCOMPARE(navigator.nextItem(), QMediaContent());
    QCOMPARE(navigator.nextItem(2), QMediaContent());
    QCOMPARE(navigator.previousItem(), QMediaContent());
    QCOMPARE(navigator.previousItem(2), QMediaContent());

    QMediaContent content2(QUrl(QLatin1String("file:///2")));
    playlist.addMedia(content2);
    QCOMPARE(navigator.currentIndex(), 0);
    QCOMPARE(navigator.currentItem(), content1);
    QCOMPARE(navigator.nextItem(), content2);
    QCOMPARE(navigator.nextItem(2), QMediaContent());
    QCOMPARE(navigator.previousItem(), QMediaContent());
    QCOMPARE(navigator.previousItem(2), QMediaContent());

    navigator.jump(1);
    QCOMPARE(navigator.currentIndex(), 1);
    QCOMPARE(navigator.currentItem(), content2);
    QCOMPARE(navigator.nextItem(), QMediaContent());
    QCOMPARE(navigator.nextItem(2), QMediaContent());
    QCOMPARE(navigator.previousItem(), content1);
    QCOMPARE(navigator.previousItem(2), QMediaContent());

    navigator.jump(0);
    navigator.next();
    QCOMPARE(navigator.currentIndex(), 1);
    navigator.next();
    QCOMPARE(navigator.currentIndex(), -1);
    navigator.next();//jump to the first item
    QCOMPARE(navigator.currentIndex(), 0);

    navigator.previous();
    QCOMPARE(navigator.currentIndex(), -1);
    navigator.previous();//jump to the last item
    QCOMPARE(navigator.currentIndex(), 1);
}

void tst_QMediaPlaylistNavigator::loopPlayback()
{
    QMediaNetworkPlaylistProvider playlist;
    QMediaPlaylistNavigator navigator(&playlist);

    navigator.setPlaybackMode(QMediaPlaylist::Loop);
    navigator.jump(0);
    QVERIFY(navigator.currentItem().isNull());
    QCOMPARE(navigator.currentIndex(), -1);

    QMediaContent content1(QUrl(QLatin1String("file:///1")));
    playlist.addMedia(content1);
    navigator.jump(0);
    QVERIFY(!navigator.currentItem().isNull());

    QCOMPARE(navigator.currentIndex(), 0);
    QCOMPARE(navigator.currentItem(), content1);
    QCOMPARE(navigator.nextItem(), content1);
    QCOMPARE(navigator.nextItem(2), content1);
    QCOMPARE(navigator.previousItem(), content1);
    QCOMPARE(navigator.previousItem(2), content1);

    QMediaContent content2(QUrl(QLatin1String("file:///2")));
    playlist.addMedia(content2);
    QCOMPARE(navigator.currentIndex(), 0);
    QCOMPARE(navigator.currentItem(), content1);
    QCOMPARE(navigator.nextItem(), content2);
    QCOMPARE(navigator.nextItem(2), content1); //loop over end of the list
    QCOMPARE(navigator.previousItem(), content2);
    QCOMPARE(navigator.previousItem(2), content1);

    navigator.jump(1);
    QCOMPARE(navigator.currentIndex(), 1);
    QCOMPARE(navigator.currentItem(), content2);
    QCOMPARE(navigator.nextItem(), content1);
    QCOMPARE(navigator.nextItem(2), content2);
    QCOMPARE(navigator.previousItem(), content1);
    QCOMPARE(navigator.previousItem(2), content2);

    navigator.jump(0);
    navigator.next();
    QCOMPARE(navigator.currentIndex(), 1);
    navigator.next();
    QCOMPARE(navigator.currentIndex(), 0);
    navigator.previous();
    QCOMPARE(navigator.currentIndex(), 1);
    navigator.previous();
    QCOMPARE(navigator.currentIndex(), 0);
}

void tst_QMediaPlaylistNavigator::currentItemOnce()
{
    QMediaNetworkPlaylistProvider playlist;
    QMediaPlaylistNavigator navigator(&playlist);

    navigator.setPlaybackMode(QMediaPlaylist::CurrentItemOnce);

    QCOMPARE(navigator.playbackMode(), QMediaPlaylist::CurrentItemOnce);
    QCOMPARE(navigator.currentIndex(), -1);

    playlist.addMedia(QMediaContent(QUrl(QLatin1String("file:///1"))));
    playlist.addMedia(QMediaContent(QUrl(QLatin1String("file:///2"))));
    playlist.addMedia(QMediaContent(QUrl(QLatin1String("file:///3"))));

    QCOMPARE(navigator.currentIndex(), -1);
    navigator.next();
    QCOMPARE(navigator.currentIndex(), -1);

    navigator.jump(1);
    QCOMPARE(navigator.currentIndex(), 1);
    navigator.next();
    QCOMPARE(navigator.currentIndex(), -1);
    navigator.next();
    QCOMPARE(navigator.currentIndex(), -1);
    navigator.previous();
    QCOMPARE(navigator.currentIndex(), -1);
    navigator.jump(1);
    navigator.previous();
    QCOMPARE(navigator.currentIndex(), -1);
}

void tst_QMediaPlaylistNavigator::currentItemInLoop()
{
    QMediaNetworkPlaylistProvider playlist;
    QMediaPlaylistNavigator navigator(&playlist);

    navigator.setPlaybackMode(QMediaPlaylist::CurrentItemInLoop);

    QCOMPARE(navigator.playbackMode(), QMediaPlaylist::CurrentItemInLoop);
    QCOMPARE(navigator.currentIndex(), -1);

    playlist.addMedia(QMediaContent(QUrl(QLatin1String("file:///1"))));
    playlist.addMedia(QMediaContent(QUrl(QLatin1String("file:///2"))));
    playlist.addMedia(QMediaContent(QUrl(QLatin1String("file:///3"))));

    QCOMPARE(navigator.currentIndex(), -1);
    navigator.next();
    QCOMPARE(navigator.currentIndex(), -1);
    navigator.jump(1);
    navigator.next();
    QCOMPARE(navigator.currentIndex(), 1);
    navigator.next();
    QCOMPARE(navigator.currentIndex(), 1);
    navigator.previous();
    QCOMPARE(navigator.currentIndex(), 1);
    navigator.previous();
    QCOMPARE(navigator.currentIndex(), 1);
}

void tst_QMediaPlaylistNavigator::randomPlayback()
{
    QMediaNetworkPlaylistProvider playlist;
    QMediaPlaylistNavigator navigator(&playlist);

    navigator.setPlaybackMode(QMediaPlaylist::Random);

    QCOMPARE(navigator.playbackMode(), QMediaPlaylist::Random);
    QCOMPARE(navigator.currentIndex(), -1);

    playlist.addMedia(QMediaContent(QUrl(QLatin1String("file:///1"))));
    playlist.addMedia(QMediaContent(QUrl(QLatin1String("file:///2"))));
    playlist.addMedia(QMediaContent(QUrl(QLatin1String("file:///3"))));

    playlist.shuffle();

    QCOMPARE(navigator.currentIndex(), -1);
    navigator.next();
    int pos1 = navigator.currentIndex();
    navigator.next();
    int pos2 = navigator.currentIndex();
    navigator.next();
    int pos3 = navigator.currentIndex();

    QVERIFY(pos1 != -1);
    QVERIFY(pos2 != -1);
    QVERIFY(pos3 != -1);

    navigator.previous();
    QCOMPARE(navigator.currentIndex(), pos2);
    navigator.next();
    QCOMPARE(navigator.currentIndex(), pos3);
    navigator.next();
    int pos4 = navigator.currentIndex();
    navigator.previous();
    QCOMPARE(navigator.currentIndex(), pos3);
    navigator.previous();
    QCOMPARE(navigator.currentIndex(), pos2);
    navigator.previous();
    QCOMPARE(navigator.currentIndex(), pos1);
    navigator.previous();
    int pos0 = navigator.currentIndex();
    QVERIFY(pos0 != -1);
    navigator.next();
    navigator.next();
    navigator.next();
    navigator.next();
    QCOMPARE(navigator.currentIndex(), pos4);

}

void tst_QMediaPlaylistNavigator::testItemAt()
{
    QMediaNetworkPlaylistProvider playlist;
    QMediaPlaylistNavigator navigator(&playlist);
    navigator.setPlaybackMode(QMediaPlaylist::Random);
    QCOMPARE(navigator.playbackMode(), QMediaPlaylist::Random);
    QCOMPARE(navigator.currentIndex(), -1);

    //Adding the media to the playlist
    QMediaContent content = QMediaContent(QUrl(QLatin1String("file:///1")));
    playlist.addMedia(content);

    //Currently it is not pointing to any index , Returns Null mediacontent
    QCOMPARE(navigator.currentIndex(), -1);
    QCOMPARE(navigator.itemAt(navigator.currentIndex()),QMediaContent());
    navigator.next();

    //Points to the added media
    int pos1 = navigator.currentIndex();
    QCOMPARE(content,navigator.itemAt(pos1));
}

void tst_QMediaPlaylistNavigator::testNextIndex()
{
    QMediaNetworkPlaylistProvider playlist;
    QMediaPlaylistNavigator navigator(&playlist);
    navigator.setPlaybackMode(QMediaPlaylist::Random);
    QCOMPARE(navigator.playbackMode(), QMediaPlaylist::Random);
    QCOMPARE(navigator.currentIndex(), -1);

    //Adding the media to the playlist
    playlist.addMedia(QMediaContent(QUrl(QLatin1String("file:///1"))));
    playlist.addMedia(QMediaContent(QUrl(QLatin1String("file:///2"))));
    playlist.addMedia(QMediaContent(QUrl(QLatin1String("file:///3"))));

    playlist.shuffle();

    //Currently it is not pointing to any index
    QCOMPARE(navigator.currentIndex(), -1);
    navigator.next();
    int pos1 = navigator.currentIndex();
    //Pointing to the next index
    navigator.next();
    int pos2 = navigator.currentIndex();
    navigator.next();
    int pos3 = navigator.currentIndex();

    //Pointing to the previous index
    navigator.previous();
    QCOMPARE(navigator.nextIndex(1), pos3);
    navigator.previous();
    QCOMPARE(navigator.nextIndex(1), pos2);
    QCOMPARE(navigator.nextIndex(2), pos3);
    navigator.previous();
    QCOMPARE(navigator.nextIndex(1), pos1);
}

void tst_QMediaPlaylistNavigator::testPreviousIndex()
{
    QMediaNetworkPlaylistProvider playlist;
    QMediaPlaylistNavigator navigator(&playlist);
    navigator.setPlaybackMode(QMediaPlaylist::Random);
    QCOMPARE(navigator.playbackMode(), QMediaPlaylist::Random);
    QCOMPARE(navigator.currentIndex(), -1);

    //Adding the media to the playlist
    playlist.addMedia(QMediaContent(QUrl(QLatin1String("file:///1"))));
    playlist.addMedia(QMediaContent(QUrl(QLatin1String("file:///2"))));
    playlist.addMedia(QMediaContent(QUrl(QLatin1String("file:///3"))));
    playlist.shuffle();

    //Currently it is not pointing to any index
    QCOMPARE(navigator.currentIndex(), -1);

    //pointing to next index
    navigator.next();
    int pos1 = navigator.currentIndex();
    navigator.next();
    int pos2 = navigator.currentIndex();
    navigator.next();
    int pos3 = navigator.currentIndex();
    QCOMPARE(navigator.previousIndex(1), pos2);
    QCOMPARE(navigator.previousIndex(2), pos1);
    navigator.next();
    QCOMPARE(navigator.previousIndex(1), pos3);
}

void tst_QMediaPlaylistNavigator::testCurrentIndexChangedSignal()
{
    QMediaNetworkPlaylistProvider playlist;
    QMediaPlaylistNavigator navigator(&playlist);
    QCOMPARE(navigator.playbackMode(), QMediaPlaylist::Sequential);
    QCOMPARE(navigator.currentIndex(), -1);

    //Creating a QSignalSpy object for currentIndexChanged() signal
    QSignalSpy spy(&navigator,SIGNAL(currentIndexChanged(int)));
    QVERIFY(spy.count() == 0);

    //Adding the media to the playlist
    playlist.addMedia(QMediaContent(QUrl(QLatin1String("file:///1"))));
    playlist.addMedia(QMediaContent(QUrl(QLatin1String("file:///2"))));
    playlist.addMedia(QMediaContent(QUrl(QLatin1String("file:///3"))));

    //Currently it is not pointing to any index
    QCOMPARE(navigator.currentIndex(), -1);
    navigator.next();
    QVERIFY(spy.count() == 1);
    int pos1 = navigator.currentIndex();
    //Pointing to the next index
    navigator.next();
    QVERIFY(navigator.previousIndex(1) == pos1);
    QVERIFY(spy.count() == 2);
}

void tst_QMediaPlaylistNavigator::testPlaybackModeChangedSignal()
{
    QMediaNetworkPlaylistProvider playlist;
    QMediaPlaylistNavigator navigator(&playlist);
    navigator.setPlaybackMode(QMediaPlaylist::Random);
    QCOMPARE(navigator.playbackMode(), QMediaPlaylist::Random);
    QCOMPARE(navigator.currentIndex(), -1);

    //Creating a QSignalSpy object for currentIndexChanged() signal
    QSignalSpy spy(&navigator,SIGNAL(playbackModeChanged(QMediaPlaylist::PlaybackMode)));
    QVERIFY(spy.count() == 0);

    //Adding the media to the playlist
    playlist.addMedia(QMediaContent(QUrl(QLatin1String("file:///1"))));

    //set the play back mode to sequential
    navigator.setPlaybackMode(QMediaPlaylist::Sequential);
    QCOMPARE(navigator.playbackMode(), QMediaPlaylist::Sequential);
    QVERIFY(spy.count() == 1);

    //set the play back mode to loop
    navigator.setPlaybackMode(QMediaPlaylist::Loop);
    QCOMPARE(navigator.playbackMode(), QMediaPlaylist::Loop);
    QVERIFY(spy.count() == 2);
}

void tst_QMediaPlaylistNavigator::testSurroundingItemsChangedSignal()
{
    QMediaNetworkPlaylistProvider playlist;
    QMediaPlaylistNavigator navigator(&playlist);
    navigator.setPlaybackMode(QMediaPlaylist::Random);
    QCOMPARE(navigator.playbackMode(), QMediaPlaylist::Random);
    QCOMPARE(navigator.currentIndex(), -1);

    //Creating a QSignalSpy object for surroundingItemsChanged()signal
    QSignalSpy spy(&navigator,SIGNAL(surroundingItemsChanged()));
    QVERIFY(spy.count() == 0);

    //Adding the media to the playlist
    playlist.addMedia(QMediaContent(QUrl(QLatin1String("file:///1"))));
    QVERIFY(spy.count() == 1);

    //set the play back mode to sequential
    navigator.setPlaybackMode(QMediaPlaylist::Sequential);
    QCOMPARE(navigator.playbackMode(), QMediaPlaylist::Sequential);
    QVERIFY(spy.count() == 2);

    //Point to the next index
    navigator.next();
    QVERIFY(spy.count() == 3);
}

void tst_QMediaPlaylistNavigator::testActivatedSignal()
{
    QMediaNetworkPlaylistProvider playlist;
    QMediaPlaylistNavigator navigator(&playlist);
    navigator.setPlaybackMode(QMediaPlaylist::Random);
    QCOMPARE(navigator.playbackMode(), QMediaPlaylist::Random);
    QCOMPARE(navigator.currentIndex(), -1);

    //Creating a QSignalSpy object for surroundingItemsChanged()signal
    QSignalSpy spy(&navigator,SIGNAL(activated(QMediaContent)));
    QVERIFY(spy.count() == 0);

    //Adding the media to the playlist
    playlist.addMedia(QMediaContent(QUrl(QLatin1String("file:///1"))));
    playlist.addMedia(QMediaContent(QUrl(QLatin1String("file:///2"))));
    playlist.shuffle();

    //Point to the next index
    navigator.next();
    QVERIFY(spy.count() == 1);

    //Jump to 0th item
    navigator.jump(0);
    QVERIFY(spy.count() == 2);

    //move to previous item
    navigator.previous();
    QVERIFY(spy.count() == 3);
}

QTEST_MAIN(tst_QMediaPlaylistNavigator)
#include "tst_qmediaplaylistnavigator.moc"
