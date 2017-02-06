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

#include "qmediaresource.h"

QT_USE_NAMESPACE
class tst_QMediaResource : public QObject
{
    Q_OBJECT
private slots:
    void constructNull();
    void construct_data();
    void construct();
    void setResolution();
    void equality();
    void copy();
    void assign();

    void constructorRequest();
    void copyConstructor();
};

void tst_QMediaResource::constructNull()
{
    QMediaResource resource;

    QCOMPARE(resource.isNull(), true);
    QCOMPARE(resource.url(), QUrl());
    QCOMPARE(resource.request(), QNetworkRequest());
    QCOMPARE(resource.mimeType(), QString());
    QCOMPARE(resource.language(), QString());
    QCOMPARE(resource.audioCodec(), QString());
    QCOMPARE(resource.videoCodec(), QString());
    QCOMPARE(resource.dataSize(), qint64(0));
    QCOMPARE(resource.audioBitRate(), 0);
    QCOMPARE(resource.sampleRate(), 0);
    QCOMPARE(resource.channelCount(), 0);
    QCOMPARE(resource.videoBitRate(), 0);
    QCOMPARE(resource.resolution(), QSize());
}

void tst_QMediaResource::construct_data()
{
    QTest::addColumn<QUrl>("url");
    QTest::addColumn<QNetworkRequest>("request");
    QTest::addColumn<QString>("mimeType");
    QTest::addColumn<QString>("language");
    QTest::addColumn<QString>("audioCodec");
    QTest::addColumn<QString>("videoCodec");
    QTest::addColumn<qint64>("dataSize");
    QTest::addColumn<int>("audioBitRate");
    QTest::addColumn<int>("sampleRate");
    QTest::addColumn<int>("channelCount");
    QTest::addColumn<int>("videoBitRate");
    QTest::addColumn<QSize>("resolution");

    QTest::newRow("audio content")
            << QUrl(QString::fromLatin1("http:://test.com/test.mp3"))
            << QNetworkRequest(QUrl(QString::fromLatin1("http:://test.com/test.mp3")))
            << QString::fromLatin1("audio/mpeg")
            << QString::fromLatin1("eng")
            << QString::fromLatin1("mp3")
            << QString()
            << qint64(5465433)
            << 128000
            << 44100
            << 2
            << 0
            << QSize();
    QTest::newRow("image content")
            << QUrl(QString::fromLatin1("http:://test.com/test.jpg"))
            << QNetworkRequest(QUrl(QString::fromLatin1("http:://test.com/test.jpg")))
            << QString::fromLatin1("image/jpeg")
            << QString()
            << QString()
            << QString()
            << qint64(23600)
            << 0
            << 0
            << 0
            << 0
            << QSize(640, 480);
    QTest::newRow("video content")
            << QUrl(QString::fromLatin1("http:://test.com/test.mp4"))
            << QNetworkRequest(QUrl(QString::fromLatin1("http:://test.com/test.mp4")))
            << QString::fromLatin1("video/mp4")
            << QString()
            << QString::fromLatin1("aac")
            << QString::fromLatin1("h264")
            << qint64(36245851)
            << 96000
            << 44000
            << 5
            << 750000
            << QSize(720, 576);
    QTest::newRow("thumbnail")
            << QUrl(QString::fromLatin1("file::///thumbs/test.png"))
            << QNetworkRequest(QUrl(QString::fromLatin1("file::///thumbs/test.png")))
            << QString::fromLatin1("image/png")
            << QString()
            << QString()
            << QString()
            << qint64(2360)
            << 0
            << 0
            << 0
            << 0
            << QSize(128, 128);
}

void tst_QMediaResource::construct()
{
    QFETCH(QUrl, url);
    QFETCH(QNetworkRequest, request);
    QFETCH(QString, mimeType);
    QFETCH(QString, language);
    QFETCH(QString, audioCodec);
    QFETCH(QString, videoCodec);
    QFETCH(qint64, dataSize);
    QFETCH(int, audioBitRate);
    QFETCH(int, sampleRate);
    QFETCH(int, channelCount);
    QFETCH(int, videoBitRate);
    QFETCH(QSize, resolution);

    {
        QMediaResource resource(url);

        QCOMPARE(resource.isNull(), false);
        QCOMPARE(resource.url(), url);
        QCOMPARE(resource.mimeType(), QString());
        QCOMPARE(resource.language(), QString());
        QCOMPARE(resource.audioCodec(), QString());
        QCOMPARE(resource.videoCodec(), QString());
        QCOMPARE(resource.dataSize(), qint64(0));
        QCOMPARE(resource.audioBitRate(), 0);
        QCOMPARE(resource.sampleRate(), 0);
        QCOMPARE(resource.channelCount(), 0);
        QCOMPARE(resource.videoBitRate(), 0);
        QCOMPARE(resource.resolution(), QSize());
    }
    {
        QMediaResource resource(url, mimeType);

        QCOMPARE(resource.isNull(), false);
        QCOMPARE(resource.url(), url);
        QCOMPARE(resource.request(), request);
        QCOMPARE(resource.mimeType(), mimeType);
        QCOMPARE(resource.language(), QString());
        QCOMPARE(resource.audioCodec(), QString());
        QCOMPARE(resource.videoCodec(), QString());
        QCOMPARE(resource.dataSize(), qint64(0));
        QCOMPARE(resource.audioBitRate(), 0);
        QCOMPARE(resource.sampleRate(), 0);
        QCOMPARE(resource.channelCount(), 0);
        QCOMPARE(resource.videoBitRate(), 0);
        QCOMPARE(resource.resolution(), QSize());

        resource.setLanguage(language);
        resource.setAudioCodec(audioCodec);
        resource.setVideoCodec(videoCodec);
        resource.setDataSize(dataSize);
        resource.setAudioBitRate(audioBitRate);
        resource.setSampleRate(sampleRate);
        resource.setChannelCount(channelCount);
        resource.setVideoBitRate(videoBitRate);
        resource.setResolution(resolution);

        QCOMPARE(resource.language(), language);
        QCOMPARE(resource.audioCodec(), audioCodec);
        QCOMPARE(resource.videoCodec(), videoCodec);
        QCOMPARE(resource.dataSize(), dataSize);
        QCOMPARE(resource.audioBitRate(), audioBitRate);
        QCOMPARE(resource.sampleRate(), sampleRate);
        QCOMPARE(resource.channelCount(), channelCount);
        QCOMPARE(resource.videoBitRate(), videoBitRate);
        QCOMPARE(resource.resolution(), resolution);
    }
    {
        QMediaResource resource(request, mimeType);

        QCOMPARE(resource.isNull(), false);
        QCOMPARE(resource.url(), url);
        QCOMPARE(resource.request(), request);
        QCOMPARE(resource.mimeType(), mimeType);
        QCOMPARE(resource.language(), QString());
        QCOMPARE(resource.audioCodec(), QString());
        QCOMPARE(resource.videoCodec(), QString());
        QCOMPARE(resource.dataSize(), qint64(0));
        QCOMPARE(resource.audioBitRate(), 0);
        QCOMPARE(resource.sampleRate(), 0);
        QCOMPARE(resource.channelCount(), 0);
        QCOMPARE(resource.videoBitRate(), 0);
        QCOMPARE(resource.resolution(), QSize());

        resource.setLanguage(language);
        resource.setAudioCodec(audioCodec);
        resource.setVideoCodec(videoCodec);
        resource.setDataSize(dataSize);
        resource.setAudioBitRate(audioBitRate);
        resource.setSampleRate(sampleRate);
        resource.setChannelCount(channelCount);
        resource.setVideoBitRate(videoBitRate);
        resource.setResolution(resolution);

        QCOMPARE(resource.language(), language);
        QCOMPARE(resource.audioCodec(), audioCodec);
        QCOMPARE(resource.videoCodec(), videoCodec);
        QCOMPARE(resource.dataSize(), dataSize);
        QCOMPARE(resource.audioBitRate(), audioBitRate);
        QCOMPARE(resource.sampleRate(), sampleRate);
        QCOMPARE(resource.channelCount(), channelCount);
        QCOMPARE(resource.videoBitRate(), videoBitRate);
        QCOMPARE(resource.resolution(), resolution);
    }
}

void tst_QMediaResource::setResolution()
{
    QMediaResource resource(
            QUrl(QString::fromLatin1("file::///thumbs/test.png")),
            QString::fromLatin1("image/png"));

    QCOMPARE(resource.resolution(), QSize());

    resource.setResolution(QSize(120, 80));
    QCOMPARE(resource.resolution(), QSize(120, 80));

    resource.setResolution(QSize(-1, 23));
    QCOMPARE(resource.resolution(), QSize(-1, 23));

    resource.setResolution(QSize(-43, 34));
    QCOMPARE(resource.resolution(), QSize(-43, 34));

    resource.setResolution(QSize(64, -1));
    QCOMPARE(resource.resolution(), QSize(64, -1));

    resource.setResolution(QSize(64, -83));
    QCOMPARE(resource.resolution(), QSize(64, -83));

    resource.setResolution(QSize(-12, -83));
    QCOMPARE(resource.resolution(), QSize(-12, -83));

    resource.setResolution(QSize());
    QCOMPARE(resource.resolution(), QSize(-1, -1));

    resource.setResolution(120, 80);
    QCOMPARE(resource.resolution(), QSize(120, 80));

    resource.setResolution(-1, 23);
    QCOMPARE(resource.resolution(), QSize(-1, 23));

    resource.setResolution(-43, 34);
    QCOMPARE(resource.resolution(), QSize(-43, 34));

    resource.setResolution(64, -1);
    QCOMPARE(resource.resolution(), QSize(64, -1));

    resource.setResolution(64, -83);
    QCOMPARE(resource.resolution(), QSize(64, -83));

    resource.setResolution(-12, -83);
    QCOMPARE(resource.resolution(), QSize(-12, -83));

    resource.setResolution(-1, -1);
    QCOMPARE(resource.resolution(), QSize());
}

void tst_QMediaResource::equality()
{
    QMediaResource resource1(
            QUrl(QString::fromLatin1("http://test.com/test.mp4")),
            QString::fromLatin1("video/mp4"));
    QMediaResource resource2(
            QUrl(QString::fromLatin1("http://test.com/test.mp4")),
            QString::fromLatin1("video/mp4"));
    QMediaResource resource3(
            QUrl(QString::fromLatin1("file:///thumbs/test.jpg")));
    QMediaResource resource4(
            QUrl(QString::fromLatin1("file:///thumbs/test.jpg")));
    QMediaResource resource5(
            QUrl(QString::fromLatin1("http://test.com/test.mp3")),
            QString::fromLatin1("audio/mpeg"));

    QNetworkRequest request(QUrl("http://test.com/test.mp3"));
    QString requestMimeType("audio/mp3");

    QMediaResource requestResource1(request, requestMimeType);
    QMediaResource requestResource2(request, requestMimeType);

    QCOMPARE(requestResource1 == requestResource2, true);
    QCOMPARE(requestResource1 != requestResource2, false);
    QCOMPARE(requestResource1 != resource5, true);

    QCOMPARE(resource1 == resource2, true);
    QCOMPARE(resource1 != resource2, false);

    QCOMPARE(resource3 == resource4, true);
    QCOMPARE(resource3 != resource4, false);

    QCOMPARE(resource1 == resource3, false);
    QCOMPARE(resource1 != resource3, true);

    QCOMPARE(resource1 == resource5, false);
    QCOMPARE(resource1 != resource5, true);

    resource1.setAudioCodec(QString::fromLatin1("mp3"));
    resource2.setAudioCodec(QString::fromLatin1("aac"));

    // Not equal differing audio codecs.
    QCOMPARE(resource1 == resource2, false);
    QCOMPARE(resource1 != resource2, true);

    resource1.setAudioCodec(QString::fromLatin1("aac"));

    // Equal.
    QCOMPARE(resource1 == resource2, true);
    QCOMPARE(resource1 != resource2, false);

    resource1.setVideoCodec(QString());

    // Equal.
    QCOMPARE(resource1 == resource2, true);
    QCOMPARE(resource1 != resource2, false);

    resource1.setVideoCodec(QString::fromLatin1("h264"));

    // Not equal differing video codecs.
    QCOMPARE(resource1 == resource2, false);
    QCOMPARE(resource1 != resource2, true);

    resource2.setVideoCodec(QString::fromLatin1("h264"));

    // Equal.
    QCOMPARE(resource1 == resource2, true);
    QCOMPARE(resource1 != resource2, false);

    resource2.setDataSize(0);

    // Equal.
    QCOMPARE(resource1 == resource2, true);
    QCOMPARE(resource1 != resource2, false);

    resource1.setDataSize(546423);

    // Not equal differing video codecs.
    QCOMPARE(resource1 == resource2, false);
    QCOMPARE(resource1 != resource2, true);

    resource2.setDataSize(546423);

    // Equal.
    QCOMPARE(resource1 == resource2, true);
    QCOMPARE(resource1 != resource2, false);

    resource1.setAudioBitRate(96000);
    resource1.setSampleRate(48000);
    resource2.setSampleRate(44100);
    resource1.setChannelCount(0);
    resource1.setVideoBitRate(900000);
    resource2.setLanguage(QString::fromLatin1("eng"));

    // Not equal, audio bit rate, sample rate, video bit rate, and
    // language.
    QCOMPARE(resource1 == resource2, false);
    QCOMPARE(resource1 != resource2, true);

    resource2.setAudioBitRate(96000);
    resource1.setSampleRate(44100);

    // Not equal, differing video bit rate, and language.
    QCOMPARE(resource1 == resource2, false);
    QCOMPARE(resource1 != resource2, true);

    resource2.setVideoBitRate(900000);
    resource1.setLanguage(QString::fromLatin1("eng"));

    // Equal
    QCOMPARE(resource1 == resource2, true);
    QCOMPARE(resource1 != resource2, false);

    resource1.setResolution(QSize());

    // Equal
    QCOMPARE(resource1 == resource2, true);
    QCOMPARE(resource1 != resource2, false);

    resource2.setResolution(-1, -1);

    // Equal
    QCOMPARE(resource1 == resource2, true);
    QCOMPARE(resource1 != resource2, false);

    resource1.setResolution(QSize(-640, -480));

    // Not equal, differing resolution.
    QCOMPARE(resource1 == resource2, false);
    QCOMPARE(resource1 != resource2, true);
    resource1.setResolution(QSize(640, 480));
    resource2.setResolution(QSize(800, 600));

    // Not equal, differing resolution.
    QCOMPARE(resource1 == resource2, false);
    QCOMPARE(resource1 != resource2, true);

    resource1.setResolution(800, 600);

    // Equal
    QCOMPARE(resource1 == resource2, true);
    QCOMPARE(resource1 != resource2, false);

    /* equality tests for constructor of QMediaresource(QNetworkrequest,mimeType)*/
    QNetworkRequest request2(QUrl(QString::fromLatin1("http://test.com/test.mp4")));
    QUrl url(QString::fromLatin1("http://test.com/test.mp4"));
    QString mimeType(QLatin1String("video/mp4"));

    QMediaResource resource6(request2,mimeType);
    QMediaResource resource7(request2,mimeType);


    QVERIFY(resource6.request()==request2);
    QVERIFY(resource6.mimeType()==mimeType);


    QVERIFY(resource7.request()==request2);
    QVERIFY(resource7.mimeType()==mimeType);

    QVERIFY(resource6.request()==resource7.request());
    QVERIFY(resource6.mimeType()==resource7.mimeType());

    QVERIFY(resource6==resource7);

    /*for copy constructor*/
    QMediaResource resource8(resource7);

    QVERIFY(resource8.request()==request2);
    QVERIFY(resource8.mimeType()==mimeType);


    QVERIFY(resource7.request()==request2);
    QVERIFY(resource7.mimeType()==mimeType);

    QVERIFY(resource8.request()==resource7.request());
    QVERIFY(resource8.mimeType()==resource7.mimeType());


    QVERIFY(resource8==resource7);

    /*for assign constructor*/

    QMediaResource resource9(request2,mimeType);

    QMediaResource resource10=resource9;

    QVERIFY(resource10.request()==request2);
    QVERIFY(resource10.mimeType()==mimeType);


    QVERIFY(resource9.request()==request2);
    QVERIFY(resource9.mimeType()==mimeType);

    QVERIFY(resource8.request()==resource7.request());
    QVERIFY(resource8.mimeType()==resource7.mimeType());

    QVERIFY(resource8==resource7);
}

void tst_QMediaResource::copy()
{
    const QUrl url(QString::fromLatin1("http://test.com/test.mp4"));
    const QString mimeType(QLatin1String("video/mp4"));
    const QString amrCodec(QLatin1String("amr"));
    const QString mp3Codec(QLatin1String("mp3"));
    const QString aacCodec(QLatin1String("aac"));
    const QString h264Codec(QLatin1String("h264"));

    QMediaResource original(url, mimeType);
    original.setAudioCodec(amrCodec);

    QMediaResource copy(original);

    QCOMPARE(copy.url(), url);
    QCOMPARE(copy.mimeType(), mimeType);
    QCOMPARE(copy.audioCodec(), amrCodec);

    QCOMPARE(original == copy, true);
    QCOMPARE(original != copy, false);

    original.setAudioCodec(mp3Codec);

    QCOMPARE(copy.audioCodec(), amrCodec);
    QCOMPARE(original == copy, false);
    QCOMPARE(original != copy, true);

    copy.setAudioCodec(aacCodec);
    copy.setVideoCodec(h264Codec);

    QCOMPARE(copy.url(), url);
    QCOMPARE(copy.mimeType(), mimeType);

    QCOMPARE(original.audioCodec(), mp3Codec);
}

void tst_QMediaResource::assign()
{
    const QUrl url(QString::fromLatin1("http://test.com/test.mp4"));
    const QString mimeType(QLatin1String("video/mp4"));
    const QString amrCodec(QLatin1String("amr"));
    const QString mp3Codec(QLatin1String("mp3"));
    const QString aacCodec(QLatin1String("aac"));
    const QString h264Codec(QLatin1String("h264"));

    QNetworkRequest request(QUrl(QString::fromLatin1("http://test.com/test.mp4")));
    const qint64 dataSize(23600);
    int audioBitRate = 1, sampleRate = 2, channelCount = 3, videoBitRate = 4;
    QSize resolution(QSize(640, 480));
    QString language("eng");

    QMediaResource copy(QUrl(QString::fromLatin1("file:///thumbs/test.jpg")));

    QMediaResource original(url, mimeType);
    original.setAudioCodec(amrCodec);

    copy = original;

    QCOMPARE(copy.url(), url);
    QCOMPARE(copy.mimeType(), mimeType);
    QCOMPARE(copy.audioCodec(), amrCodec);

    QCOMPARE(original == copy, true);
    QCOMPARE(original != copy, false);

    original.setAudioCodec(mp3Codec);

    QCOMPARE(copy.audioCodec(), amrCodec);
    QCOMPARE(original == copy, false);
    QCOMPARE(original != copy, true);

    copy.setAudioCodec(aacCodec);
    copy.setVideoCodec(h264Codec);

    QCOMPARE(copy.url(), url);
    QCOMPARE(copy.mimeType(), mimeType);

    QCOMPARE(original.audioCodec(), mp3Codec);

    /* for constructor of QMediaresource(QNetworkrequest,mimeType)*/

    QMediaResource copy1(QNetworkRequest(QUrl(QString::fromLatin1("file:///thumbs/test.jpg"))));

    QMediaResource original1(request, mimeType);

    original1.setAudioCodec(amrCodec);
    original1.setLanguage(QString("eng"));
    original1.setVideoCodec(h264Codec);
    original1.setDataSize(dataSize);
    original1.setAudioBitRate(audioBitRate);
    original1.setSampleRate(sampleRate);
    original1.setChannelCount(channelCount);
    original1.setVideoBitRate(videoBitRate);
    original1.setResolution(resolution);

    copy1 = original1;

    QCOMPARE(original1 == copy1, true);
}

// Constructor for request without passing mimetype.
void tst_QMediaResource::constructorRequest()
{
    //Initialise the request and url.
    QNetworkRequest request(QUrl(QString::fromLatin1("http:://test.com/test.mp3")));
    QUrl url(QString::fromLatin1("http:://test.com/test.mp3"));

    // Create the instance with request as parameter.
    QMediaResource resource(request);

    // Verify all the parameters of objects.
    QCOMPARE(resource.isNull(), false);
    QCOMPARE(resource.url(), url);
    QCOMPARE(resource.request(), request);
    QCOMPARE(resource.mimeType(), QString());
    QCOMPARE(resource.language(), QString());
    QCOMPARE(resource.audioCodec(), QString());
    QCOMPARE(resource.videoCodec(), QString());
    QCOMPARE(resource.dataSize(), qint64(0));
    QCOMPARE(resource.audioBitRate(), 0);
    QCOMPARE(resource.sampleRate(), 0);
    QCOMPARE(resource.channelCount(), 0);
    QCOMPARE(resource.videoBitRate(), 0);
    QCOMPARE(resource.resolution(), QSize());
}

// Copy constructor with all the parameter and copy constructor for constructor with request and mimetype as parameter.
void tst_QMediaResource::copyConstructor()
{
    // Initialise all the parameters.
    const QUrl url(QString::fromLatin1("http://test.com/test.mp4"));
    const QString mimeType(QLatin1String("video/mp4"));
    const QString amrCodec(QLatin1String("amr"));
    const QString h264Codec(QLatin1String("h264"));

    const qint64 dataSize(23600);
    int audioBitRate = 1, sampleRate = 2, channelCount = 3, videoBitRate = 4;
    QSize resolution(QSize(640, 480));
    QString language("eng");

    // Create the instance with url and mimetype.
    QMediaResource original(url, mimeType);

    // Set all the parameters.
    original.setAudioCodec(amrCodec);
    original.setLanguage(QString("eng"));
    original.setVideoCodec(h264Codec);
    original.setDataSize(dataSize);
    original.setAudioBitRate(audioBitRate);
    original.setSampleRate(sampleRate);
    original.setChannelCount(channelCount);
    original.setVideoBitRate(videoBitRate);
    original.setResolution(resolution);

    // Copy the instance to new object.
    QMediaResource copy(original);

    // Verify all the parameters of the copied object.
    QCOMPARE(copy.url(), url);
    QCOMPARE(copy.mimeType(), mimeType);
    QCOMPARE(copy.audioCodec(), amrCodec);
    QCOMPARE(copy.language(), language );
    QCOMPARE(copy.videoCodec(), h264Codec);
    QCOMPARE(copy.dataSize(), dataSize);
    QCOMPARE(copy.audioBitRate(), audioBitRate);
    QCOMPARE(copy.sampleRate(), sampleRate);
    QCOMPARE(copy.channelCount(), channelCount);
    QCOMPARE(copy.videoBitRate(), videoBitRate);
    QCOMPARE(copy.resolution(), resolution);

    // Compare both the objects are equal.
    QCOMPARE(original == copy, true);
    QCOMPARE(original != copy, false);

    // Initialise the request parameter.
    QNetworkRequest request1(QUrl(QString::fromLatin1("http://test.com/test.mp4")));

    // Constructor with rerquest and mimetype.
    QMediaResource original1(request1, mimeType);

    // Copy the object and verify if both are eqaul or not.
    QMediaResource copy1(original1);
    QCOMPARE(copy1.url(), url);
    QCOMPARE(copy1.mimeType(), mimeType);
    QCOMPARE(copy1.request(), request1);
    QCOMPARE(original1 == copy1, true);
}

QTEST_MAIN(tst_QMediaResource)

#include "tst_qmediaresource.moc"
