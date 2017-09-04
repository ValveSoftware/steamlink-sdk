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

#include <QtTest/QtTest>
#include <QtNetwork/qnetworkrequest.h>

#include <qmediacontent.h>
#include <qmediaplaylist.h>

//TESTED_COMPONENT=src/multimedia

QT_USE_NAMESPACE
class tst_QMediaContent : public QObject
{
    Q_OBJECT

private slots:
    void testNull();
    void testUrlCtor();
    void testRequestCtor();
    void testResourceCtor();
    void testResourceListCtor();
    void testCopy();
    void testAssignment();
    void testEquality();
    void testResources();
    void testPlaylist();
};

void tst_QMediaContent::testNull()
{
    QMediaContent media;

    QCOMPARE(media.isNull(), true);
    QCOMPARE(media.canonicalUrl(), QUrl());
    QCOMPARE(media.canonicalResource(), QMediaResource());
    QCOMPARE(media.resources(), QMediaResourceList());
}

void tst_QMediaContent::testUrlCtor()
{
    QMediaContent media(QUrl("http://example.com/movie.mov"));

    QCOMPARE(media.canonicalUrl(), QUrl("http://example.com/movie.mov"));
    QCOMPARE(media.canonicalResource().url(), QUrl("http://example.com/movie.mov"));
}

void tst_QMediaContent::testRequestCtor()
{
    QNetworkRequest request(QUrl("http://example.com/movie.mov"));
    request.setAttribute(QNetworkRequest::User, QVariant(1234));

    QMediaContent media(request);

    QCOMPARE(media.canonicalUrl(), QUrl("http://example.com/movie.mov"));
    QCOMPARE(media.canonicalRequest(),request);
    QCOMPARE(media.canonicalResource().request(), request);
    QCOMPARE(media.canonicalResource().url(), QUrl("http://example.com/movie.mov"));
}

void tst_QMediaContent::testResourceCtor()
{
    QMediaContent media(QMediaResource(QUrl("http://example.com/movie.mov")));

    QCOMPARE(media.canonicalResource(), QMediaResource(QUrl("http://example.com/movie.mov")));
}

void tst_QMediaContent::testResourceListCtor()
{
    QMediaResourceList  resourceList;
    resourceList << QMediaResource(QUrl("http://example.com/movie.mov"));

    QMediaContent media(resourceList);

    QCOMPARE(media.canonicalUrl(), QUrl("http://example.com/movie.mov"));
    QCOMPARE(media.canonicalResource().url(), QUrl("http://example.com/movie.mov"));
}

void tst_QMediaContent::testCopy()
{
    QMediaContent media1(QMediaResource(QUrl("http://example.com/movie.mov")));
    QMediaContent media2(media1);

    QVERIFY(media1 == media2);
}

void tst_QMediaContent::testAssignment()
{
    QMediaContent media1(QMediaResource(QUrl("http://example.com/movie.mov")));
    QMediaContent media2;
    QMediaContent media3;

    media2 = media1;
    QVERIFY(media2 == media1);

    media2 = media3;
    QVERIFY(media2 == media3);
}

void tst_QMediaContent::testEquality()
{
    QMediaContent media1;
    QMediaContent media2;
    QMediaContent media3(QMediaResource(QUrl("http://example.com/movie.mov")));
    QMediaContent media4(QMediaResource(QUrl("http://example.com/movie.mov")));
    QMediaContent media5(QMediaResource(QUrl("file:///some/where/over/the/rainbow.mp3")));

    // null == null
    QCOMPARE(media1 == media2, true);
    QCOMPARE(media1 != media2, false);

    // null != something
    QCOMPARE(media1 == media3, false);
    QCOMPARE(media1 != media3, true);

    // equiv
    QCOMPARE(media3 == media4, true);
    QCOMPARE(media3 != media4, false);

    // not equiv
    QCOMPARE(media4 == media5, false);
    QCOMPARE(media4 != media5, true);
}

void tst_QMediaContent::testResources()
{
    QMediaResourceList  resourceList;

    resourceList << QMediaResource(QUrl("http://example.com/movie-main.mov"));
    resourceList << QMediaResource(QUrl("http://example.com/movie-big.mov"));
    QMediaContent    media(resourceList);

    QMediaResourceList  res = media.resources();
    QCOMPARE(res.size(), 2);
    QCOMPARE(res[0], QMediaResource(QUrl("http://example.com/movie-main.mov")));
    QCOMPARE(res[1], QMediaResource(QUrl("http://example.com/movie-big.mov")));
}

void tst_QMediaContent::testPlaylist()
{
    QMediaContent media(QUrl("http://example.com/movie.mov"));
    QVERIFY(media.canonicalUrl().isValid());
    QVERIFY(!media.playlist());

    {
        QPointer<QMediaPlaylist> playlist(new QMediaPlaylist);
        media = QMediaContent(playlist.data(), QUrl("http://example.com/sample.m3u"), true);
        QVERIFY(media.canonicalUrl().isValid());
        QCOMPARE(media.playlist(), playlist.data());
        media = QMediaContent();
        // Make sure playlist is destroyed by QMediaContent
        QTRY_VERIFY(!playlist);
    }

    {
        QMediaPlaylist *playlist = new QMediaPlaylist;
        media = QMediaContent(playlist, QUrl("http://example.com/sample.m3u"), true);
        // Delete playlist outside QMediaContent
        delete playlist;
        QVERIFY(!media.playlist());
        media = QMediaContent();
    }

    {
        QPointer<QMediaPlaylist> playlist(new QMediaPlaylist);
        media = QMediaContent(playlist.data(), QUrl(), false);
        QVERIFY(!media.canonicalUrl().isValid());
        QCOMPARE(media.playlist(), playlist.data());
        media = QMediaContent();
        QVERIFY(playlist);
        delete playlist.data();
    }
}

QTEST_MAIN(tst_QMediaContent)

#include "tst_qmediacontent.moc"
