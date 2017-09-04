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

//TESTED_COMPONENT=src/multimedia

#include <QtTest/QtTest>
#include <QDebug>
#include <QtMultimedia/qmediametadata.h>
#include <qmediaobject.h>
#include <qmediacontrol.h>
#include <qmediaservice.h>
#include <qmediarecordercontrol.h>
#include <qmediarecorder.h>
#include <qmetadatawritercontrol.h>
#include <qaudioinputselectorcontrol.h>
#include <qaudioencodersettingscontrol.h>
#include <qmediacontainercontrol.h>
#include <qvideoencodersettingscontrol.h>
#include <qaudioformat.h>

#include "mockmediarecorderservice.h"
#include "mockmediaobject.h"

QT_USE_NAMESPACE

class tst_QMediaRecorder: public QObject
{
    Q_OBJECT

public slots:
    void initTestCase();
    void cleanupTestCase();

private slots:
    void testNullService();
    void testNullControls();
    void testDeleteMediaObject();
    void testError();
    void testSink();
    void testRecord();
    void testMute();
    void testVolume();
    void testAudioDeviceControl();
    void testAudioEncodeControl();
    void testMediaFormatsControl();
    void testVideoEncodeControl();
    void testEncodingSettings();
    void testAudioSettings();
    void testVideoSettings();
    void testSettingsApplied();

    void nullMetaDataControl();
    void isMetaDataAvailable();
    void isWritable();
    void metaDataChanged();
    void metaData_data();
    void metaData();
    void setMetaData_data();
    void setMetaData();

    void testAudioSettingsCopyConstructor();
    void testAudioSettingsOperatorNotEqual();
    void testAudioSettingsOperatorEqual();
    void testAudioSettingsOperatorAssign();
    void testAudioSettingsDestructor();

    void testAvailabilityStatus();
    void testIsAvailable();
    void testMediaObject();
    void testEnum();

    void testVideoSettingsQuality();
    void testVideoSettingsEncodingMode();
    void testVideoSettingsCopyConstructor();
    void testVideoSettingsOperatorAssignment();
    void testVideoSettingsOperatorNotEqual();
    void testVideoSettingsOperatorComparison();
    void testVideoSettingsDestructor();

private:
    QAudioEncoderSettingsControl* encode;
    QAudioInputSelectorControl* audio;
    MockMediaObject *object;
    MockMediaRecorderService*service;
    MockMediaRecorderControl *mock;
    QMediaRecorder *capture;
    QVideoEncoderSettingsControl* videoEncode;
};

void tst_QMediaRecorder::initTestCase()
{
    qRegisterMetaType<QMediaRecorder::State>("QMediaRecorder::State");
    qRegisterMetaType<QMediaRecorder::Error>("QMediaRecorder::Error");

    mock = new MockMediaRecorderControl(this);
    service = new MockMediaRecorderService(this, mock);
    object = new MockMediaObject(this, service);
    capture = new QMediaRecorder(object);

    audio = qobject_cast<QAudioInputSelectorControl*>(service->requestControl(QAudioInputSelectorControl_iid));
    encode = qobject_cast<QAudioEncoderSettingsControl*>(service->requestControl(QAudioEncoderSettingsControl_iid));
    videoEncode = qobject_cast<QVideoEncoderSettingsControl*>(service->requestControl(QVideoEncoderSettingsControl_iid));
}

void tst_QMediaRecorder::cleanupTestCase()
{
    delete capture;
    delete object;
    delete service;
    delete mock;
}

void tst_QMediaRecorder::testNullService()
{
    const QString id(QLatin1String("application/x-format"));

    MockMediaObject object(0, 0);
    QMediaRecorder recorder(&object);

    QCOMPARE(recorder.outputLocation(), QUrl());
    QCOMPARE(recorder.state(), QMediaRecorder::StoppedState);
    QCOMPARE(recorder.error(), QMediaRecorder::NoError);
    QCOMPARE(recorder.duration(), qint64(0));
    QCOMPARE(recorder.supportedContainers(), QStringList());
    QCOMPARE(recorder.containerDescription(id), QString());
    QCOMPARE(recorder.supportedAudioCodecs(), QStringList());
    QCOMPARE(recorder.audioCodecDescription(id), QString());
    QCOMPARE(recorder.supportedAudioSampleRates(), QList<int>());
    QCOMPARE(recorder.supportedVideoCodecs(), QStringList());
    QCOMPARE(recorder.videoCodecDescription(id), QString());
    bool continuous = true;
    QCOMPARE(recorder.supportedResolutions(QVideoEncoderSettings(), &continuous), QList<QSize>());
    QCOMPARE(continuous, false);
    continuous = true;
    QCOMPARE(recorder.supportedFrameRates(QVideoEncoderSettings(), &continuous), QList<qreal>());
    QCOMPARE(continuous, false);
    QCOMPARE(recorder.audioSettings(), QAudioEncoderSettings());
    QCOMPARE(recorder.videoSettings(), QVideoEncoderSettings());
    QCOMPARE(recorder.containerFormat(), QString());
    QVERIFY(!recorder.isMuted());
    recorder.setMuted(true);
    QVERIFY(!recorder.isMuted());
}

void tst_QMediaRecorder::testNullControls()
{
    const QString id(QLatin1String("application/x-format"));

    MockMediaRecorderService service(0, 0);
    service.hasControls = false;
    MockMediaObject object(0, &service);
    QMediaRecorder recorder(&object);

    QCOMPARE(recorder.outputLocation(), QUrl());
    QCOMPARE(recorder.state(), QMediaRecorder::StoppedState);
    QCOMPARE(recorder.error(), QMediaRecorder::NoError);
    QCOMPARE(recorder.duration(), qint64(0));
    QCOMPARE(recorder.supportedContainers(), QStringList());
    QCOMPARE(recorder.containerDescription(id), QString());
    QCOMPARE(recorder.supportedAudioCodecs(), QStringList());
    QCOMPARE(recorder.audioCodecDescription(id), QString());
    QCOMPARE(recorder.supportedAudioSampleRates(), QList<int>());
    QCOMPARE(recorder.supportedVideoCodecs(), QStringList());
    QCOMPARE(recorder.videoCodecDescription(id), QString());
    bool continuous = true;
    QCOMPARE(recorder.supportedResolutions(QVideoEncoderSettings(), &continuous), QList<QSize>());
    QCOMPARE(continuous, false);
    continuous = true;
    QCOMPARE(recorder.supportedFrameRates(QVideoEncoderSettings(), &continuous), QList<qreal>());
    QCOMPARE(continuous, false);
    QCOMPARE(recorder.audioSettings(), QAudioEncoderSettings());
    QCOMPARE(recorder.videoSettings(), QVideoEncoderSettings());
    QCOMPARE(recorder.containerFormat(), QString());

    recorder.setOutputLocation(QUrl("file://test/save/file.mp4"));
    QCOMPARE(recorder.outputLocation(), QUrl());
    QCOMPARE(recorder.actualLocation(), QUrl());

    QAudioEncoderSettings audio;
    audio.setCodec(id);
    audio.setQuality(QMultimedia::LowQuality);

    QVideoEncoderSettings video;
    video.setCodec(id);
    video.setResolution(640, 480);

    recorder.setEncodingSettings(audio, video, id);

    QCOMPARE(recorder.audioSettings(), QAudioEncoderSettings());
    QCOMPARE(recorder.videoSettings(), QVideoEncoderSettings());
    QCOMPARE(recorder.containerFormat(), QString());

    QSignalSpy spy(&recorder, SIGNAL(stateChanged(QMediaRecorder::State)));

    recorder.record();
    QCOMPARE(recorder.state(), QMediaRecorder::StoppedState);
    QCOMPARE(recorder.error(), QMediaRecorder::NoError);
    QCOMPARE(spy.count(), 0);

    recorder.pause();
    QCOMPARE(recorder.state(), QMediaRecorder::StoppedState);
    QCOMPARE(recorder.error(), QMediaRecorder::NoError);
    QCOMPARE(spy.count(), 0);

    recorder.stop();
    QCOMPARE(recorder.state(), QMediaRecorder::StoppedState);
    QCOMPARE(recorder.error(), QMediaRecorder::NoError);
    QCOMPARE(spy.count(), 0);
}

void tst_QMediaRecorder::testDeleteMediaObject()
{
    MockMediaRecorderControl *mock = new MockMediaRecorderControl(this);
    MockMediaRecorderService *service = new MockMediaRecorderService(this, mock);
    MockMediaObject *object = new MockMediaObject(this, service);
    QMediaRecorder *capture = new QMediaRecorder(object);

    QVERIFY(capture->mediaObject() == object);
    QVERIFY(capture->isAvailable());

    delete object;
    delete service;
    delete mock;

    QVERIFY(capture->mediaObject() == 0);
    QVERIFY(!capture->isAvailable());

    delete capture;
}

void tst_QMediaRecorder::testError()
{
    const QString errorString(QLatin1String("format error"));

    QSignalSpy spy(capture, SIGNAL(error(QMediaRecorder::Error)));

    QCOMPARE(capture->error(), QMediaRecorder::NoError);
    QCOMPARE(capture->errorString(), QString());

    mock->error(QMediaRecorder::FormatError, errorString);
    QCOMPARE(capture->error(), QMediaRecorder::FormatError);
    QCOMPARE(capture->errorString(), errorString);
    QCOMPARE(spy.count(), 1);

    QCOMPARE(spy.last()[0].value<QMediaRecorder::Error>(), QMediaRecorder::FormatError);
}

void tst_QMediaRecorder::testSink()
{
    capture->setOutputLocation(QUrl("test.tmp"));
    QUrl s = capture->outputLocation();
    QCOMPARE(s.toString(), QString("test.tmp"));
    QCOMPARE(capture->actualLocation(), QUrl());

    //the actual location is available after record
    capture->record();
    QCOMPARE(capture->actualLocation().toString(), QString("test.tmp"));
    capture->stop();
    QCOMPARE(capture->actualLocation().toString(), QString("test.tmp"));

    //setOutputLocation resets the actual location
    capture->setOutputLocation(QUrl());
    QCOMPARE(capture->actualLocation(), QUrl());

    capture->record();
    QCOMPARE(capture->actualLocation(), QUrl::fromLocalFile("default_name.mp4"));
    capture->stop();
    QCOMPARE(capture->actualLocation(), QUrl::fromLocalFile("default_name.mp4"));
}

void tst_QMediaRecorder::testRecord()
{
    QSignalSpy stateSignal(capture,SIGNAL(stateChanged(QMediaRecorder::State)));
    QSignalSpy statusSignal(capture,SIGNAL(statusChanged(QMediaRecorder::Status)));
    QSignalSpy progressSignal(capture, SIGNAL(durationChanged(qint64)));
    capture->record();
    QCOMPARE(capture->state(), QMediaRecorder::RecordingState);
    QCOMPARE(capture->error(), QMediaRecorder::NoError);
    QCOMPARE(capture->errorString(), QString());

    QCOMPARE(stateSignal.count(), 1);
    QCOMPARE(stateSignal.last()[0].value<QMediaRecorder::State>(), QMediaRecorder::RecordingState);

    QTestEventLoop::instance().enterLoop(1);

    QCOMPARE(capture->status(), QMediaRecorder::RecordingStatus);
    QVERIFY(!statusSignal.isEmpty());
    QCOMPARE(statusSignal.last()[0].value<QMediaRecorder::Status>(), QMediaRecorder::RecordingStatus);
    statusSignal.clear();

    QVERIFY(progressSignal.count() > 0);

    capture->pause();

    QCOMPARE(capture->state(), QMediaRecorder::PausedState);

    QCOMPARE(stateSignal.count(), 2);

    QTestEventLoop::instance().enterLoop(1);
    QCOMPARE(capture->status(), QMediaRecorder::PausedStatus);
    QVERIFY(!statusSignal.isEmpty());
    QCOMPARE(statusSignal.last()[0].value<QMediaRecorder::Status>(), QMediaRecorder::PausedStatus);
    statusSignal.clear();

    capture->stop();

    QCOMPARE(capture->state(), QMediaRecorder::StoppedState);
    QCOMPARE(stateSignal.count(), 3);

    QTestEventLoop::instance().enterLoop(1);
    QCOMPARE(capture->status(), QMediaRecorder::LoadedStatus);
    QVERIFY(!statusSignal.isEmpty());
    QCOMPARE(statusSignal.last()[0].value<QMediaRecorder::Status>(), QMediaRecorder::LoadedStatus);
    statusSignal.clear();

    mock->stop();
    QCOMPARE(stateSignal.count(), 3);
}

void tst_QMediaRecorder::testMute()
{
    QSignalSpy mutedChanged(capture, SIGNAL(mutedChanged(bool)));
    QVERIFY(!capture->isMuted());
    capture->setMuted(true);

    QCOMPARE(mutedChanged.size(), 1);
    QCOMPARE(mutedChanged[0][0].toBool(), true);
    QVERIFY(capture->isMuted());

    capture->setMuted(false);

    QCOMPARE(mutedChanged.size(), 2);
    QCOMPARE(mutedChanged[1][0].toBool(), false);
    QVERIFY(!capture->isMuted());

    capture->setMuted(false);
    QCOMPARE(mutedChanged.size(), 2);
}

void tst_QMediaRecorder::testVolume()
{
    QSignalSpy volumeChanged(capture, SIGNAL(volumeChanged(qreal)));
    QCOMPARE(capture->volume(), 1.0);
    capture->setVolume(2.0);

    QCOMPARE(volumeChanged.size(), 1);
    QCOMPARE(volumeChanged[0][0].toReal(), 2.0);
    QCOMPARE(capture->volume(), 2.0);

    capture->setVolume(1.0);

    QCOMPARE(volumeChanged.size(), 2);
    QCOMPARE(volumeChanged[1][0].toReal(), 1.0);
    QCOMPARE(capture->volume(), 1.0);

    capture->setVolume(1.0);
    QCOMPARE(volumeChanged.size(), 2);
}

void tst_QMediaRecorder::testAudioDeviceControl()
{
    QSignalSpy readSignal(audio,SIGNAL(activeInputChanged(QString)));
    QVERIFY(audio->availableInputs().size() == 3);
    QVERIFY(audio->defaultInput().compare("device1") == 0);
    audio->setActiveInput("device2");
    QTestEventLoop::instance().enterLoop(1);
    QVERIFY(audio->activeInput().compare("device2") == 0);
    QVERIFY(readSignal.count() == 1);
    QVERIFY(audio->inputDescription("device2").compare("dev2 comment") == 0);
}

void tst_QMediaRecorder::testAudioEncodeControl()
{
    QStringList codecs = capture->supportedAudioCodecs();
    QVERIFY(codecs.count() == 2);
    QVERIFY(capture->audioCodecDescription("audio/pcm") == "Pulse Code Modulation");
    QList<int> rates;
    rates << 8000 << 11025 << 22050 << 44100;
    QCOMPARE(capture->supportedAudioSampleRates(), rates);
}

void tst_QMediaRecorder::testMediaFormatsControl()
{
    QCOMPARE(capture->supportedContainers(), QStringList() << "wav" << "mp3" << "mov");

    QCOMPARE(capture->containerDescription("wav"), QString("WAV format"));
    QCOMPARE(capture->containerDescription("mp3"), QString("MP3 format"));
    QCOMPARE(capture->containerDescription("ogg"), QString());
}

void tst_QMediaRecorder::testVideoEncodeControl()
{
    bool continuous = false;
    QList<QSize> sizes = capture->supportedResolutions(QVideoEncoderSettings(), &continuous);
    QCOMPARE(sizes.count(), 2);
    QCOMPARE(continuous, true);

    QList<qreal> rates = capture->supportedFrameRates(QVideoEncoderSettings(), &continuous);
    QCOMPARE(rates.count(), 3);
    QCOMPARE(continuous, false);

    QStringList vCodecs = capture->supportedVideoCodecs();
    QVERIFY(vCodecs.count() == 2);
    QCOMPARE(capture->videoCodecDescription("video/3gpp"), QString("video/3gpp"));
}

void tst_QMediaRecorder::testEncodingSettings()
{
    QAudioEncoderSettings audioSettings = capture->audioSettings();
    QCOMPARE(audioSettings.codec(), QString("audio/pcm"));
    QCOMPARE(audioSettings.bitRate(), 128*1024);
    QCOMPARE(audioSettings.sampleRate(), 8000);
    QCOMPARE(audioSettings.quality(), QMultimedia::NormalQuality);
    QCOMPARE(audioSettings.channelCount(), -1);

    QCOMPARE(audioSettings.encodingMode(), QMultimedia::ConstantQualityEncoding);

    QVideoEncoderSettings videoSettings = capture->videoSettings();
    QCOMPARE(videoSettings.codec(), QString());
    QCOMPARE(videoSettings.bitRate(), -1);
    QCOMPARE(videoSettings.resolution(), QSize());
    QCOMPARE(videoSettings.frameRate(), 0.0);
    QCOMPARE(videoSettings.quality(), QMultimedia::NormalQuality);
    QCOMPARE(videoSettings.encodingMode(), QMultimedia::ConstantQualityEncoding);

    QString format = capture->containerFormat();
    QCOMPARE(format, QString());

    audioSettings.setCodec("audio/mpeg");
    audioSettings.setSampleRate(44100);
    audioSettings.setBitRate(256*1024);
    audioSettings.setQuality(QMultimedia::HighQuality);
    audioSettings.setEncodingMode(QMultimedia::AverageBitRateEncoding);

    videoSettings.setCodec("video/3gpp");
    videoSettings.setBitRate(800);
    videoSettings.setFrameRate(24*1024);
    videoSettings.setResolution(QSize(800,600));
    videoSettings.setQuality(QMultimedia::HighQuality);
    audioSettings.setEncodingMode(QMultimedia::TwoPassEncoding);

    format = QString("mov");

    capture->setAudioSettings(audioSettings);
    capture->setVideoSettings(videoSettings);
    capture->setContainerFormat(format);

    QCOMPARE(capture->audioSettings(), audioSettings);
    QCOMPARE(capture->videoSettings(), videoSettings);
    QCOMPARE(capture->containerFormat(), format);
}

void tst_QMediaRecorder::testAudioSettings()
{
    QAudioEncoderSettings settings;
    QVERIFY(settings.isNull());
    QVERIFY(settings == QAudioEncoderSettings());

    QCOMPARE(settings.codec(), QString());
    settings.setCodec(QLatin1String("codecName"));
    QCOMPARE(settings.codec(), QLatin1String("codecName"));
    QVERIFY(!settings.isNull());
    QVERIFY(settings != QAudioEncoderSettings());

    settings = QAudioEncoderSettings();
    QCOMPARE(settings.bitRate(), -1);
    settings.setBitRate(128000);
    QCOMPARE(settings.bitRate(), 128000);
    QVERIFY(!settings.isNull());

    settings = QAudioEncoderSettings();
    QCOMPARE(settings.quality(), QMultimedia::NormalQuality);
    settings.setQuality(QMultimedia::HighQuality);
    QCOMPARE(settings.quality(), QMultimedia::HighQuality);
    QVERIFY(!settings.isNull());

    settings = QAudioEncoderSettings();
    QCOMPARE(settings.sampleRate(), -1);
    settings.setSampleRate(44100);
    QCOMPARE(settings.sampleRate(), 44100);
    QVERIFY(!settings.isNull());

    settings = QAudioEncoderSettings();
    QCOMPARE(settings.channelCount(), -1);
    settings.setChannelCount(2);
    QCOMPARE(settings.channelCount(), 2);
    QVERIFY(!settings.isNull());

    settings = QAudioEncoderSettings();
    settings.setEncodingOption(QLatin1Literal("encoderOption"), QVariant(1));
    QCOMPARE(settings.encodingOption(QLatin1Literal("encoderOption")), QVariant(1));
    QVariantMap options;
    options.insert(QLatin1Literal("encoderOption"), QVariant(1));
    QCOMPARE(settings.encodingOptions(), options);
    options.insert(QLatin1Literal("encoderOption2"), QVariant(2));
    options.remove(QLatin1Literal("encoderOption"));
    settings.setEncodingOptions(options);
    QCOMPARE(settings.encodingOption(QLatin1Literal("encoderOption")), QVariant());
    QCOMPARE(settings.encodingOption(QLatin1Literal("encoderOption2")), QVariant(2));
    QVERIFY(!settings.isNull());
    QVERIFY(settings != QAudioEncoderSettings());

    settings = QAudioEncoderSettings();
    QVERIFY(settings.isNull());
    QCOMPARE(settings.codec(), QString());
    QCOMPARE(settings.bitRate(), -1);
    QCOMPARE(settings.quality(), QMultimedia::NormalQuality);
    QCOMPARE(settings.sampleRate(), -1);
    QVERIFY(settings.encodingOptions().isEmpty());

    {
        QAudioEncoderSettings settings1;
        QAudioEncoderSettings settings2;
        QCOMPARE(settings2, settings1);

        settings2 = settings1;
        QCOMPARE(settings2, settings1);
        QVERIFY(settings2.isNull());

        settings1.setQuality(QMultimedia::HighQuality);

        QVERIFY(settings2.isNull());
        QVERIFY(!settings1.isNull());
        QVERIFY(settings1 != settings2);
    }

    {
        QAudioEncoderSettings settings1;
        QAudioEncoderSettings settings2(settings1);
        QCOMPARE(settings2, settings1);

        settings2 = settings1;
        QCOMPARE(settings2, settings1);
        QVERIFY(settings2.isNull());

        settings1.setQuality(QMultimedia::HighQuality);

        QVERIFY(settings2.isNull());
        QVERIFY(!settings1.isNull());
        QVERIFY(settings1 != settings2);
    }

    QAudioEncoderSettings settings1;
    settings1.setBitRate(1);
    QAudioEncoderSettings settings2;
    settings2.setBitRate(1);
    QVERIFY(settings1 == settings2);
    settings2.setBitRate(2);
    QVERIFY(settings1 != settings2);

    settings1 = QAudioEncoderSettings();
    settings1.setChannelCount(1);
    settings2 = QAudioEncoderSettings();
    settings2.setChannelCount(1);
    QVERIFY(settings1 == settings2);
    settings2.setChannelCount(2);
    QVERIFY(settings1 != settings2);

    settings1 = QAudioEncoderSettings();
    settings1.setCodec("codec1");
    settings2 = QAudioEncoderSettings();
    settings2.setCodec("codec1");
    QVERIFY(settings1 == settings2);
    settings2.setCodec("codec2");
    QVERIFY(settings1 != settings2);

    settings1 = QAudioEncoderSettings();
    settings1.setEncodingMode(QMultimedia::ConstantBitRateEncoding);
    settings2 = QAudioEncoderSettings();
    settings2.setEncodingMode(QMultimedia::ConstantBitRateEncoding);
    QVERIFY(settings1 == settings2);
    settings2.setEncodingMode(QMultimedia::TwoPassEncoding);
    QVERIFY(settings1 != settings2);

    settings1 = QAudioEncoderSettings();
    settings1.setQuality(QMultimedia::NormalQuality);
    settings2 = QAudioEncoderSettings();
    settings2.setQuality(QMultimedia::NormalQuality);
    QVERIFY(settings1 == settings2);
    settings2.setQuality(QMultimedia::LowQuality);
    QVERIFY(settings1 != settings2);

    settings1 = QAudioEncoderSettings();
    settings1.setSampleRate(1);
    settings2 = QAudioEncoderSettings();
    settings2.setSampleRate(1);
    QVERIFY(settings1 == settings2);
    settings2.setSampleRate(2);
    QVERIFY(settings1 != settings2);

    settings1 = QAudioEncoderSettings();
    settings1.setEncodingOption(QLatin1Literal("encoderOption"), QVariant(1));
    settings2 = QAudioEncoderSettings();
    settings2.setEncodingOption(QLatin1Literal("encoderOption"), QVariant(1));
    QVERIFY(settings1 == settings2);
    settings2.setEncodingOption(QLatin1Literal("encoderOption"), QVariant(2));
    QVERIFY(settings1 != settings2);
}

void tst_QMediaRecorder::testVideoSettings()
{
    QVideoEncoderSettings settings;
    QVERIFY(settings.isNull());
    QVERIFY(settings == QVideoEncoderSettings());

    QCOMPARE(settings.codec(), QString());
    settings.setCodec(QLatin1String("codecName"));
    QCOMPARE(settings.codec(), QLatin1String("codecName"));
    QVERIFY(!settings.isNull());
    QVERIFY(settings != QVideoEncoderSettings());

    settings = QVideoEncoderSettings();
    QCOMPARE(settings.bitRate(), -1);
    settings.setBitRate(128000);
    QCOMPARE(settings.bitRate(), 128000);
    QVERIFY(!settings.isNull());

    settings = QVideoEncoderSettings();
    QCOMPARE(settings.quality(), QMultimedia::NormalQuality);
    settings.setQuality(QMultimedia::HighQuality);
    QCOMPARE(settings.quality(), QMultimedia::HighQuality);
    QVERIFY(!settings.isNull());

    settings = QVideoEncoderSettings();
    QCOMPARE(settings.frameRate(), qreal());
    settings.setFrameRate(30000.0/10001);
    QVERIFY(qFuzzyCompare(settings.frameRate(), qreal(30000.0/10001)));
    settings.setFrameRate(24.0);
    QVERIFY(qFuzzyCompare(settings.frameRate(), qreal(24.0)));
    QVERIFY(!settings.isNull());

    settings = QVideoEncoderSettings();
    QCOMPARE(settings.resolution(), QSize());
    settings.setResolution(QSize(320,240));
    QCOMPARE(settings.resolution(), QSize(320,240));
    settings.setResolution(800,600);
    QCOMPARE(settings.resolution(), QSize(800,600));
    QVERIFY(!settings.isNull());

    settings = QVideoEncoderSettings();
    settings.setEncodingOption(QLatin1Literal("encoderOption"), QVariant(1));
    QCOMPARE(settings.encodingOption(QLatin1Literal("encoderOption")), QVariant(1));
    QVariantMap options;
    options.insert(QLatin1Literal("encoderOption"), QVariant(1));
    QCOMPARE(settings.encodingOptions(), options);
    options.insert(QLatin1Literal("encoderOption2"), QVariant(2));
    options.remove(QLatin1Literal("encoderOption"));
    settings.setEncodingOptions(options);
    QCOMPARE(settings.encodingOption(QLatin1Literal("encoderOption")), QVariant());
    QCOMPARE(settings.encodingOption(QLatin1Literal("encoderOption2")), QVariant(2));
    QVERIFY(!settings.isNull());
    QVERIFY(settings != QVideoEncoderSettings());

    settings = QVideoEncoderSettings();
    QVERIFY(settings.isNull());
    QCOMPARE(settings.codec(), QString());
    QCOMPARE(settings.bitRate(), -1);
    QCOMPARE(settings.quality(), QMultimedia::NormalQuality);
    QCOMPARE(settings.frameRate(), qreal());
    QCOMPARE(settings.resolution(), QSize());
    QVERIFY(settings.encodingOptions().isEmpty());

    {
        QVideoEncoderSettings settings1;
        QVideoEncoderSettings settings2;
        QCOMPARE(settings2, settings1);

        settings2 = settings1;
        QCOMPARE(settings2, settings1);
        QVERIFY(settings2.isNull());

        settings1.setQuality(QMultimedia::HighQuality);

        QVERIFY(settings2.isNull());
        QVERIFY(!settings1.isNull());
        QVERIFY(settings1 != settings2);
    }

    {
        QVideoEncoderSettings settings1;
        QVideoEncoderSettings settings2(settings1);
        QCOMPARE(settings2, settings1);

        settings2 = settings1;
        QCOMPARE(settings2, settings1);
        QVERIFY(settings2.isNull());

        settings1.setQuality(QMultimedia::HighQuality);

        QVERIFY(settings2.isNull());
        QVERIFY(!settings1.isNull());
        QVERIFY(settings1 != settings2);
    }

    QVideoEncoderSettings settings1;
    settings1.setBitRate(1);
    QVideoEncoderSettings settings2;
    settings2.setBitRate(1);
    QVERIFY(settings1 == settings2);
    settings2.setBitRate(2);
    QVERIFY(settings1 != settings2);

    settings1 = QVideoEncoderSettings();
    settings1.setResolution(800,600);
    settings2 = QVideoEncoderSettings();
    settings2.setResolution(QSize(800,600));
    QVERIFY(settings1 == settings2);
    settings2.setResolution(QSize(400,300));
    QVERIFY(settings1 != settings2);

    settings1 = QVideoEncoderSettings();
    settings1.setCodec("codec1");
    settings2 = QVideoEncoderSettings();
    settings2.setCodec("codec1");
    QVERIFY(settings1 == settings2);
    settings2.setCodec("codec2");
    QVERIFY(settings1 != settings2);

    settings1 = QVideoEncoderSettings();
    settings1.setEncodingMode(QMultimedia::ConstantBitRateEncoding);
    settings2 = QVideoEncoderSettings();
    settings2.setEncodingMode(QMultimedia::ConstantBitRateEncoding);
    QVERIFY(settings1 == settings2);
    settings2.setEncodingMode(QMultimedia::TwoPassEncoding);
    QVERIFY(settings1 != settings2);

    settings1 = QVideoEncoderSettings();
    settings1.setQuality(QMultimedia::NormalQuality);
    settings2 = QVideoEncoderSettings();
    settings2.setQuality(QMultimedia::NormalQuality);
    QVERIFY(settings1 == settings2);
    settings2.setQuality(QMultimedia::LowQuality);
    QVERIFY(settings1 != settings2);

    settings1 = QVideoEncoderSettings();
    settings1.setFrameRate(1);
    settings2 = QVideoEncoderSettings();
    settings2.setFrameRate(1);
    QVERIFY(settings1 == settings2);
    settings2.setFrameRate(2);
    QVERIFY(settings1 != settings2);

    settings1 = QVideoEncoderSettings();
    settings1.setEncodingOption(QLatin1Literal("encoderOption"), QVariant(1));
    settings2 = QVideoEncoderSettings();
    settings2.setEncodingOption(QLatin1Literal("encoderOption"), QVariant(1));
    QVERIFY(settings1 == settings2);
    settings2.setEncodingOption(QLatin1Literal("encoderOption"), QVariant(2));
    QVERIFY(settings1 != settings2);
}

void tst_QMediaRecorder::testSettingsApplied()
{
    MockMediaRecorderControl recorderControl(0);
    MockMediaRecorderService service(0, &recorderControl);
    MockMediaObject object(0, &service);

    //if the media recorder is not configured after construction
    //the settings are applied in the next event loop
    QMediaRecorder recorder(&object);
    QCOMPARE(recorderControl.m_settingAppliedCount, 0);
    QTRY_COMPARE(recorderControl.m_settingAppliedCount, 1);

    QVideoEncoderSettings videoSettings;
    videoSettings.setResolution(640,480);
    recorder.setVideoSettings(videoSettings);

    QAudioEncoderSettings audioSettings;
    audioSettings.setQuality(QMultimedia::HighQuality);
    recorder.setAudioSettings(audioSettings);

    recorder.setContainerFormat("mkv");

    QCOMPARE(recorderControl.m_settingAppliedCount, 1);
    QTRY_COMPARE(recorderControl.m_settingAppliedCount, 2);

    //encoder settings are applied before recording if changed
    audioSettings.setQuality(QMultimedia::VeryHighQuality);
    recorder.setAudioSettings(audioSettings);

    QCOMPARE(recorderControl.m_settingAppliedCount, 2);
    recorder.record();
    QCOMPARE(recorderControl.m_settingAppliedCount, 3);

    recorder.stop();

    //applySettings is not called if setting has not changes
    recorder.record();
    QCOMPARE(recorderControl.m_settingAppliedCount, 3);
}

void tst_QMediaRecorder::nullMetaDataControl()
{
    const QString titleKey(QLatin1String("Title"));
    const QString title(QLatin1String("Host of Seraphim"));

    MockMediaRecorderControl recorderControl(0);
    MockMediaRecorderService service(0, &recorderControl);
    service.hasControls = false;
    MockMediaObject object(0, &service);

    QMediaRecorder recorder(&object);

    QSignalSpy spy(&recorder, SIGNAL(metaDataChanged()));

    QCOMPARE(recorder.isMetaDataAvailable(), false);
    QCOMPARE(recorder.isMetaDataWritable(), false);

    recorder.setMetaData(QMediaMetaData::Title, title);

    QCOMPARE(recorder.metaData(QMediaMetaData::Title).toString(), QString());
    QCOMPARE(recorder.availableMetaData(), QStringList());
    QCOMPARE(spy.count(), 0);
}

void tst_QMediaRecorder::isMetaDataAvailable()
{
    MockMediaRecorderControl recorderControl(0);
    MockMediaRecorderService service(0, &recorderControl);
    service.mockMetaDataControl->setMetaDataAvailable(false);
    MockMediaObject object(0, &service);

    QMediaRecorder recorder(&object);
    QCOMPARE(recorder.isMetaDataAvailable(), false);

    QSignalSpy spy(&recorder, SIGNAL(metaDataAvailableChanged(bool)));
    service.mockMetaDataControl->setMetaDataAvailable(true);

    QCOMPARE(recorder.isMetaDataAvailable(), true);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), true);

    service.mockMetaDataControl->setMetaDataAvailable(false);

    QCOMPARE(recorder.isMetaDataAvailable(), false);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).toBool(), false);
}

void tst_QMediaRecorder::isWritable()
{
    MockMediaRecorderControl recorderControl(0);
    MockMediaRecorderService service(0, &recorderControl);
    service.mockMetaDataControl->setWritable(false);

    MockMediaObject object(0, &service);

    QMediaRecorder recorder(&object);

    QSignalSpy spy(&recorder, SIGNAL(metaDataWritableChanged(bool)));

    QCOMPARE(recorder.isMetaDataWritable(), false);

    service.mockMetaDataControl->setWritable(true);

    QCOMPARE(recorder.isMetaDataWritable(), true);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), true);

    service.mockMetaDataControl->setWritable(false);

    QCOMPARE(recorder.isMetaDataWritable(), false);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).toBool(), false);
}

void tst_QMediaRecorder::metaDataChanged()
{
    MockMediaRecorderControl recorderControl(0);
    MockMediaRecorderService service(0, &recorderControl);
    MockMediaObject object(0, &service);

    QMediaRecorder recorder(&object);

    QSignalSpy changedSpy(&recorder, SIGNAL(metaDataChanged()));
    QSignalSpy changedWithValueSpy(&recorder, SIGNAL(metaDataChanged(QString,QVariant)));

    service.mockMetaDataControl->setMetaData("key", "Value");
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(changedWithValueSpy.count(), 1);
    QCOMPARE(changedWithValueSpy.last()[0], QVariant("key"));
    QCOMPARE(changedWithValueSpy.last()[1].value<QVariant>(), QVariant("Value"));

    service.mockMetaDataControl->setMetaData("key", "Value");
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(changedWithValueSpy.count(), 1);

    service.mockMetaDataControl->setMetaData("key2", "Value");
    QCOMPARE(changedSpy.count(), 2);
    QCOMPARE(changedWithValueSpy.count(), 2);
    QCOMPARE(changedWithValueSpy.last()[0], QVariant("key2"));
    QCOMPARE(changedWithValueSpy.last()[1].value<QVariant>(), QVariant("Value"));
}

void tst_QMediaRecorder::metaData_data()
{
    QTest::addColumn<QString>("artist");
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("genre");
    QTest::addColumn<QString>("custom");

    QTest::newRow("")
            << QString::fromLatin1("Dead Can Dance")
            << QString::fromLatin1("Host of Seraphim")
            << QString::fromLatin1("Awesome")
            << QString::fromLatin1("Something else");
}

void tst_QMediaRecorder::metaData()
{
    QFETCH(QString, artist);
    QFETCH(QString, title);
    QFETCH(QString, genre);
    QFETCH(QString, custom);

    MockMediaRecorderControl recorderControl(0);
    MockMediaRecorderService service(0, &recorderControl);
    service.mockMetaDataControl->populateMetaData();

    MockMediaObject object(0, &service);

    QMediaRecorder recorder(&object);
    QVERIFY(object.availableMetaData().isEmpty());

    service.mockMetaDataControl->m_data.insert(QMediaMetaData::AlbumArtist, artist);
    service.mockMetaDataControl->m_data.insert(QMediaMetaData::Title, title);
    service.mockMetaDataControl->m_data.insert(QMediaMetaData::Genre, genre);
    service.mockMetaDataControl->m_data.insert(QLatin1String("CustomEntry"), custom );

    QCOMPARE(recorder.metaData(QMediaMetaData::AlbumArtist).toString(), artist);
    QCOMPARE(recorder.metaData(QMediaMetaData::Title).toString(), title);

    QStringList metaDataKeys = recorder.availableMetaData();
    QCOMPARE(metaDataKeys.size(), 4);
    QVERIFY(metaDataKeys.contains(QMediaMetaData::AlbumArtist));
    QVERIFY(metaDataKeys.contains(QMediaMetaData::Title));
    QVERIFY(metaDataKeys.contains(QMediaMetaData::Genre));
    QVERIFY(metaDataKeys.contains(QLatin1String("CustomEntry")));
}

void tst_QMediaRecorder::setMetaData_data()
{
    QTest::addColumn<QString>("title");

    QTest::newRow("")
            << QString::fromLatin1("In the Kingdom of the Blind the One eyed are Kings");
}

void tst_QMediaRecorder::setMetaData()
{
    QFETCH(QString, title);

    MockMediaRecorderControl recorderControl(0);
    MockMediaRecorderService service(0, &recorderControl);
    service.mockMetaDataControl->populateMetaData();

    MockMediaObject object(0, &service);

    QMediaRecorder recorder(&object);

    recorder.setMetaData(QMediaMetaData::Title, title);
    QCOMPARE(recorder.metaData(QMediaMetaData::Title).toString(), title);
    QCOMPARE(service.mockMetaDataControl->m_data.value(QMediaMetaData::Title).toString(), title);
}

void tst_QMediaRecorder::testAudioSettingsCopyConstructor()
{
    /* create an object for AudioEncodersettings */
    QAudioEncoderSettings audiosettings;
    QVERIFY(audiosettings.isNull());

    /* setting the desired properties for the AudioEncoder */
    audiosettings.setBitRate(128*1000);
    audiosettings.setChannelCount(4);
    audiosettings.setCodec("audio/pcm");
    audiosettings.setEncodingMode(QMultimedia::ConstantBitRateEncoding);
    audiosettings.setQuality(QMultimedia::LowQuality);
    audiosettings.setSampleRate(44100);

    /* Copy constructor */
    QAudioEncoderSettings other(audiosettings);
    QVERIFY(!(other.isNull()));

    /* Verifying whether data is copied properly or not */
    QVERIFY(other.bitRate() == audiosettings.bitRate());
    QVERIFY(other.sampleRate() == audiosettings.sampleRate());
    QVERIFY(other.channelCount() == audiosettings.channelCount());
    QCOMPARE(other.codec(), audiosettings.codec());
    QVERIFY(other.encodingMode() == audiosettings.encodingMode());
    QVERIFY(other.quality() == audiosettings.quality());
}

void tst_QMediaRecorder::testAudioSettingsOperatorNotEqual()
{
    /* create an object for AudioEncodersettings */
    QAudioEncoderSettings audiosettings1;
    QVERIFY(audiosettings1.isNull());

    QAudioEncoderSettings audiosettings2;
    QVERIFY(audiosettings2.isNull());

    /* setting the desired properties to for the AudioEncoder */
    audiosettings1.setBitRate(128*1000);
    audiosettings1.setChannelCount(4);
    audiosettings1.setCodec("audio/pcm");
    audiosettings1.setEncodingMode(QMultimedia::ConstantBitRateEncoding);
    audiosettings1.setQuality(QMultimedia::LowQuality);
    audiosettings1.setSampleRate(44100);

    /* setting the desired properties for the AudioEncoder */
    audiosettings2.setBitRate(128*1000);
    audiosettings2.setChannelCount(4);
    audiosettings2.setCodec("audio/pcm");
    audiosettings2.setEncodingMode(QMultimedia::ConstantBitRateEncoding);
    audiosettings2.setQuality(QMultimedia::LowQuality);
    audiosettings2.setSampleRate(44100);

    /* verify the both are equal or not */
    QVERIFY(!(audiosettings1 != audiosettings2));

    /* Modify the settings value for one object */
    audiosettings2.setBitRate(64*1000);
    audiosettings2.setEncodingMode(QMultimedia::ConstantQualityEncoding);

    /* verify the not equal opertor */
    QVERIFY(audiosettings1 != audiosettings2);

    QVERIFY(audiosettings2.bitRate() != audiosettings1.bitRate());
    QVERIFY(audiosettings2.encodingMode() != audiosettings1.encodingMode());
}

void tst_QMediaRecorder::testAudioSettingsOperatorEqual()
{
    /* create an object for AudioEncodersettings */
    QAudioEncoderSettings audiosettings1;
    QVERIFY(audiosettings1.isNull());

    /* setting the desired properties to for the AudioEncoder */
    audiosettings1.setBitRate(128*1000);
    audiosettings1.setChannelCount(4);
    audiosettings1.setCodec("audio/pcm");
    audiosettings1.setEncodingMode(QMultimedia::ConstantBitRateEncoding);
    audiosettings1.setQuality(QMultimedia::LowQuality);
    audiosettings1.setSampleRate(44100);

    QAudioEncoderSettings audiosettings2;
    QVERIFY(audiosettings2.isNull());

    /* setting the desired properties for the AudioEncoder */
    audiosettings2.setBitRate(128*1000);
    audiosettings2.setChannelCount(4);
    audiosettings2.setCodec("audio/pcm");
    audiosettings2.setEncodingMode(QMultimedia::ConstantBitRateEncoding);
    audiosettings2.setQuality(QMultimedia::LowQuality);
    audiosettings2.setSampleRate(44100);

    /* verify both the values are same or not */
    QVERIFY(audiosettings1 == audiosettings2);
    audiosettings2.setChannelCount(2);
    QVERIFY(audiosettings1 != audiosettings2);
}

void tst_QMediaRecorder::testAudioSettingsOperatorAssign()
{

    /* create an object for AudioEncodersettings */
    QAudioEncoderSettings audiosettings1;
    QVERIFY(audiosettings1.isNull());

    /* setting the desired properties for the AudioEncoder */
    audiosettings1.setBitRate(128*1000);
    audiosettings1.setChannelCount(4);
    audiosettings1.setCodec("audio/pcm");
    audiosettings1.setEncodingMode(QMultimedia::ConstantBitRateEncoding);
    audiosettings1.setQuality(QMultimedia::LowQuality);
    audiosettings1.setSampleRate(44100);

    QAudioEncoderSettings audiosettings2;
    audiosettings2 = audiosettings1;
    /* Verifying whether data is copied properly or not */
    QVERIFY(audiosettings2.bitRate() == audiosettings1.bitRate());
    QVERIFY(audiosettings2.sampleRate() == audiosettings1.sampleRate());
    QVERIFY(audiosettings2.channelCount() == audiosettings1.channelCount());
    QCOMPARE(audiosettings2.codec(), audiosettings1.codec());
    QVERIFY(audiosettings2.encodingMode() == audiosettings1.encodingMode());
    QVERIFY(audiosettings2.quality() == audiosettings1.quality());
}

void tst_QMediaRecorder::testAudioSettingsDestructor()
{
    /* Creating null object for the audioencodersettings */
    QAudioEncoderSettings * audiosettings = new QAudioEncoderSettings;

   /* Verifying the object is null or not */
    QVERIFY(audiosettings->isNull());
    /* delete the allocated memory */
    delete audiosettings;
}

/* availability() API test. */
void tst_QMediaRecorder::testAvailabilityStatus()
{
    {
        MockMediaRecorderService service(0, 0);
        MockMediaObject object(0, &service);
        QMediaRecorder recorder(&object);
        QCOMPARE(recorder.availability(), QMultimedia::ServiceMissing);
        QCOMPARE(recorder.isAvailable(), false);
    }
    {
        MockMediaRecorderControl recorderControl(0);
        MockMediaRecorderService service1(0, &recorderControl);
        service1.mockMetaDataControl->populateMetaData();
        MockMediaObject object1(0, &service1);
        QMediaRecorder recorder1(&object1);
        QCOMPARE(recorder1.availability(), QMultimedia::Available);
        QCOMPARE(recorder1.isAvailable(), true);
    }
    {
        MockMediaRecorderControl recorderControl(0);
        MockMediaRecorderService service1(0, &recorderControl, 0);
        service1.mockMetaDataControl->populateMetaData();
        MockMediaObject object1(0, &service1);
        QMediaRecorder recorder1(&object1);

        QCOMPARE(recorder1.availability(), QMultimedia::Available);
        QCOMPARE(recorder1.isAvailable(), true);
    }
    {
        MockMediaRecorderControl recorderControl(0);
        MockAvailabilityControl availability(QMultimedia::Available);
        MockMediaRecorderService service1(0, &recorderControl, &availability);
        service1.mockMetaDataControl->populateMetaData();
        MockMediaObject object1(0, &service1);
        QMediaRecorder recorder1(&object1);

        QSignalSpy spy(&object1, SIGNAL(availabilityChanged(QMultimedia::AvailabilityStatus)));

        QCOMPARE(recorder1.availability(), QMultimedia::Available);
        QCOMPARE(recorder1.isAvailable(), true);

        availability.setAvailability(QMultimedia::Busy);
        QCOMPARE(recorder1.availability(), QMultimedia::Busy);
        QCOMPARE(recorder1.isAvailable(), false);
        QCOMPARE(spy.count(), 1);

        availability.setAvailability(QMultimedia::Available);
        QCOMPARE(recorder1.availability(), QMultimedia::Available);
        QCOMPARE(recorder1.isAvailable(), true);
        QCOMPARE(spy.count(), 2);
    }
}

/* isAvailable() API test. */
void tst_QMediaRecorder::testIsAvailable()
{
    MockMediaRecorderService service(0, 0);
    MockMediaObject object(0, &service);
    QMediaRecorder recorder(&object);
    QCOMPARE(recorder.isAvailable(), false);

    MockMediaRecorderControl recorderControl(0);
    MockMediaRecorderService service1(0, &recorderControl);
    service1.mockMetaDataControl->populateMetaData();
    MockMediaObject object1(0, &service1);
    QMediaRecorder recorder1(&object1);
    QCOMPARE(recorder1.isAvailable(), true);
}

/* mediaObject() API test. */
void tst_QMediaRecorder::testMediaObject()
{
    MockMediaRecorderService service(0, 0);
    service.hasControls = false;
    MockMediaObject object(0, &service);
    QMediaRecorder recorder(&object);

    QMediaObject *medobj = recorder.mediaObject();
    QVERIFY(medobj == NULL);

    QMediaObject *medobj1 = capture->mediaObject();
    QVERIFY(medobj1 != NULL);
}

/* enum QMediaRecorder::ResourceError property test. */
void tst_QMediaRecorder::testEnum()
{
    const QString errorString(QLatin1String("resource error"));

    QSignalSpy spy(capture, SIGNAL(error(QMediaRecorder::Error)));

    QCOMPARE(capture->error(), QMediaRecorder::NoError);
    QCOMPARE(capture->errorString(), QString());

    mock->error(QMediaRecorder::ResourceError, errorString);
    QCOMPARE(capture->error(), QMediaRecorder::ResourceError);
    QCOMPARE(capture->errorString(), errorString);
    QCOMPARE(spy.count(), 1);

    QCOMPARE(spy.last()[0].value<QMediaRecorder::Error>(), QMediaRecorder::ResourceError);
}

/* Test the QVideoEncoderSettings quality API*/
void tst_QMediaRecorder::testVideoSettingsQuality()
{
    /* Create the instance*/
    QVideoEncoderSettings settings;
    QVERIFY(settings.isNull());
    QVERIFY(settings == QVideoEncoderSettings());

    /* Verify the default value is intialised correctly*/
    QCOMPARE(settings.quality(), QMultimedia::NormalQuality);

    /* Set all types of Quality parameter and Verify if it is set correctly*/
    settings.setQuality(QMultimedia::HighQuality);
    QCOMPARE(settings.quality(), QMultimedia::HighQuality);
    QVERIFY(!settings.isNull());

    settings.setQuality(QMultimedia::VeryLowQuality);
    QCOMPARE(settings.quality(), QMultimedia::VeryLowQuality);

    settings.setQuality(QMultimedia::LowQuality);
    QCOMPARE(settings.quality(), QMultimedia::LowQuality);

    settings.setQuality(QMultimedia::VeryHighQuality);
    QCOMPARE(settings.quality(), QMultimedia::VeryHighQuality);
}

/* Test  QVideoEncoderSettings encodingMode */
void tst_QMediaRecorder::testVideoSettingsEncodingMode()
{
    /* Create the instance*/
    QVideoEncoderSettings settings;
    QVERIFY(settings.isNull());
    QVERIFY(settings == QVideoEncoderSettings());

    /* Verify the default values are initialised correctly*/
    QCOMPARE(settings.encodingMode(), QMultimedia::ConstantQualityEncoding);
    QVERIFY(settings.isNull());

    /* Set each type of encoding mode and Verify if it is set correctly*/
    settings.setEncodingMode(QMultimedia::ConstantBitRateEncoding);
    QCOMPARE(settings.encodingMode(),QMultimedia::ConstantBitRateEncoding);
    QVERIFY(!settings.isNull());

    settings.setEncodingMode(QMultimedia::AverageBitRateEncoding);
    QCOMPARE(settings.encodingMode(), QMultimedia::AverageBitRateEncoding);

    settings.setEncodingMode(QMultimedia::TwoPassEncoding);
    QCOMPARE(settings.encodingMode(), QMultimedia::TwoPassEncoding);
}

/* Test QVideoEncoderSettings copy constructor */
void tst_QMediaRecorder::testVideoSettingsCopyConstructor()
{
    /* Create the instance and initialise it*/
    QVideoEncoderSettings settings1;
    settings1.setCodec(QLatin1String("codecName"));
    settings1.setBitRate(128000);
    settings1.setQuality(QMultimedia::HighQuality);
    settings1.setEncodingMode(QMultimedia::ConstantBitRateEncoding);
    settings1.setFrameRate(30000.0/10001);
    settings1.setResolution(QSize(320,240));

    /* Create another instance with instance1 as argument*/
    QVideoEncoderSettings settings2(settings1);

    /* Verify if all the parameters are copied correctly*/
    QCOMPARE(settings2 != settings1, false);
    QCOMPARE(settings2.codec(), QLatin1String("codecName"));
    QCOMPARE(settings2.bitRate(), 128000);
    QCOMPARE(settings2.encodingMode(), QMultimedia::ConstantBitRateEncoding);
    QVERIFY(qFuzzyCompare(settings2.frameRate(), qreal(30000.0/10001)));
    QCOMPARE(settings2.resolution(), QSize(320,240));
    QCOMPARE(settings2.quality(), QMultimedia::HighQuality);

    /* Verify both the instances are equal*/
    QCOMPARE(settings2, settings1);
    QVERIFY(!settings2.isNull());
}

/* Test QVideoEncoderSettings Overloaded Operator assignment*/
void tst_QMediaRecorder::testVideoSettingsOperatorAssignment()
{
    /* Create two instances.*/
    QVideoEncoderSettings settings1;
    QVideoEncoderSettings settings2;
    QCOMPARE(settings2, settings1);
    QVERIFY(settings2.isNull());

    /* Initialize all the parameters */
    settings1.setCodec(QLatin1String("codecName"));
    settings1.setBitRate(128000);
    settings1.setEncodingMode(QMultimedia::ConstantBitRateEncoding);
    settings1.setFrameRate(30000.0/10001);
    settings1.setResolution(QSize(320,240));
    settings1.setQuality(QMultimedia::HighQuality);
    /* Assign one object to other*/
    settings2 = settings1;

    /* Verify all the parameters are copied correctly*/
    QCOMPARE(settings2, settings1);
    QCOMPARE(settings2.codec(), QLatin1String("codecName"));
    QCOMPARE(settings2.bitRate(), 128000);
    QCOMPARE(settings2.encodingMode(), QMultimedia::ConstantBitRateEncoding);
    QVERIFY(qFuzzyCompare(settings2.frameRate(), qreal(30000.0/10001)));
    QCOMPARE(settings2.resolution(), QSize(320,240));
    QCOMPARE(settings2.quality(), QMultimedia::HighQuality);
    QCOMPARE(settings2, settings1);
    QVERIFY(!settings2.isNull());
}

/* Test QVideoEncoderSettings Overloaded OperatorNotEqual*/
void tst_QMediaRecorder::testVideoSettingsOperatorNotEqual()
{
    /* Create the instance and set the bit rate and Verify objects with OperatorNotEqual*/
    QVideoEncoderSettings settings1;
    settings1.setBitRate(1);
    QVideoEncoderSettings settings2;
    settings2.setBitRate(1);
    /* OperatorNotEqual returns false when both objects are equal*/
    QCOMPARE(settings1 != settings2, false);
    settings2.setBitRate(2);
    /* OperatorNotEqual returns true when both objects are not equal*/
    QVERIFY(settings1 != settings2);

    /* Verify Resolution with not equal operator*/
    settings1 = QVideoEncoderSettings();
    settings1.setResolution(800,600);
    settings2 = QVideoEncoderSettings();
    settings2.setResolution(QSize(800,600));
    /* OperatorNotEqual returns false when both objects are equal*/
    QCOMPARE(settings1 != settings2, false);
    settings2.setResolution(QSize(400,300));
    /* OperatorNotEqual returns true when both objects are not equal*/
    QVERIFY(settings1 != settings2);

    /* Verify Codec with not equal operator*/
    settings1 = QVideoEncoderSettings();
    settings1.setCodec("codec1");
    settings2 = QVideoEncoderSettings();
    settings2.setCodec("codec1");
    /* OperatorNotEqual returns false when both objects are equal*/
    QCOMPARE(settings1 != settings2, false);
    settings2.setCodec("codec2");
    /* OperatorNotEqual returns true when both objects are not equal*/
    QVERIFY(settings1 != settings2);

    /* Verify EncodingMode with not equal operator*/
    settings1 = QVideoEncoderSettings();
    settings1.setEncodingMode(QMultimedia::ConstantBitRateEncoding);
    settings2 = QVideoEncoderSettings();
    settings2.setEncodingMode(QMultimedia::ConstantBitRateEncoding);
    /* OperatorNotEqual returns false when both objects are equal*/
    QCOMPARE(settings1 != settings2, false);
    settings2.setEncodingMode(QMultimedia::TwoPassEncoding);
    /* OperatorNotEqual returns true when both objects are not equal*/
    QVERIFY(settings1 != settings2);

    /* Verify Quality with not equal operator*/
    settings1 = QVideoEncoderSettings();
    settings1.setQuality(QMultimedia::NormalQuality);
    settings2 = QVideoEncoderSettings();
    settings2.setQuality(QMultimedia::NormalQuality);
    /* OperatorNotEqual returns false when both objects are equal*/
    QCOMPARE(settings1 != settings2, false);
    settings2.setQuality(QMultimedia::LowQuality);
    /* OperatorNotEqual returns true when both objects are not equal*/
    QVERIFY(settings1 != settings2);

    /* Verify FrameRate with not equal operator*/
    settings1 = QVideoEncoderSettings();
    settings1.setFrameRate(1);
    settings2 = QVideoEncoderSettings();
    settings2.setFrameRate(1);
    /* OperatorNotEqual returns false when both objects are equal*/
    QCOMPARE(settings1 != settings2, false);
    settings2.setFrameRate(2);
    /* OperatorNotEqual returns true when both objects are not equal*/
    QVERIFY(settings1 != settings2);
}

/* Test QVideoEncoderSettings Overloaded comparison operator*/
void tst_QMediaRecorder::testVideoSettingsOperatorComparison()
{
    /* Create the instance and set the bit rate and Verify objects with comparison operator*/
    QVideoEncoderSettings settings1;
    settings1.setBitRate(1);
    QVideoEncoderSettings settings2;
    settings2.setBitRate(1);

    /* Comparison operator returns true when both objects are equal*/
    QVERIFY(settings1 == settings2);
    settings2.setBitRate(2);
    /* Comparison operator returns false when both objects are not equal*/
    QCOMPARE(settings1 == settings2, false);

    /* Verify resolution with comparison operator*/
    settings1 = QVideoEncoderSettings();
    settings1.setResolution(800,600);
    settings2 = QVideoEncoderSettings();
    settings2.setResolution(QSize(800,600));
    /* Comparison operator returns true when both objects are equal*/
    QVERIFY(settings1 == settings2);
    settings2.setResolution(QSize(400,300));
    /* Comparison operator returns false when both objects are not equal*/
    QCOMPARE(settings1 == settings2, false);

    /* Verify Codec with comparison operator*/
    settings1 = QVideoEncoderSettings();
    settings1.setCodec("codec1");
    settings2 = QVideoEncoderSettings();
    settings2.setCodec("codec1");
    /* Comparison operator returns true when both objects are equal*/
    QVERIFY(settings1 == settings2);
    settings2.setCodec("codec2");
    /* Comparison operator returns false when both objects are not equal*/
    QCOMPARE(settings1 == settings2, false);

    /* Verify EncodingMode with comparison operator*/
    settings1 = QVideoEncoderSettings();
    settings1.setEncodingMode(QMultimedia::ConstantBitRateEncoding);
    settings2 = QVideoEncoderSettings();
    settings2.setEncodingMode(QMultimedia::ConstantBitRateEncoding);
    /* Comparison operator returns true when both objects are equal*/
    QVERIFY(settings1 == settings2);
    settings2.setEncodingMode(QMultimedia::TwoPassEncoding);
    /* Comparison operator returns false when both objects are not equal*/
    QCOMPARE(settings1 == settings2, false);

    /* Verify Quality with comparison operator*/
    settings1 = QVideoEncoderSettings();
    settings1.setQuality(QMultimedia::NormalQuality);
    settings2 = QVideoEncoderSettings();
    settings2.setQuality(QMultimedia::NormalQuality);
    /* Comparison operator returns true when both objects are equal*/
    QVERIFY(settings1 == settings2);
    settings2.setQuality(QMultimedia::LowQuality);
    /* Comparison operator returns false when both objects are not equal*/
    QCOMPARE(settings1 == settings2, false);

    /* Verify FrameRate with comparison operator*/
    settings1 = QVideoEncoderSettings();
    settings1.setFrameRate(1);
    settings2 = QVideoEncoderSettings();
    settings2.setFrameRate(1);
    /* Comparison operator returns true when both objects are equal*/
    QVERIFY(settings1 == settings2);
    settings2.setFrameRate(2);
    /* Comparison operator returns false when both objects are not equal*/
    QCOMPARE(settings1 == settings2, false);
}

/* Test the destuctor of the QVideoEncoderSettings*/
void tst_QMediaRecorder::testVideoSettingsDestructor()
{
    /* Create the instance on heap and verify if object deleted correctly*/
    QVideoEncoderSettings *settings1 = new QVideoEncoderSettings();
    QVERIFY(settings1 != NULL);
    QVERIFY(settings1->isNull());
    delete settings1;

    /* Create the instance on heap and initialise it and verify if object deleted correctly.*/
    QVideoEncoderSettings *settings2 = new QVideoEncoderSettings();
    QVERIFY(settings2 != NULL);
    settings2->setCodec(QString("codec"));
    QVERIFY(!settings2->isNull());
    delete settings2;
}

QTEST_GUILESS_MAIN(tst_QMediaRecorder)
#include "tst_qmediarecorder.moc"
