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

#include <qabstractvideosurface.h>
#include <qcameracontrol.h>
#include <qcameralockscontrol.h>
#include <qcameraexposurecontrol.h>
#include <qcameraflashcontrol.h>
#include <qcamerafocuscontrol.h>
#include <qcameraimagecapturecontrol.h>
#include <qimageencodercontrol.h>
#include <qcameraimageprocessingcontrol.h>
#include <qcameracapturebufferformatcontrol.h>
#include <qcameracapturedestinationcontrol.h>
#include <qmediaservice.h>
#include <qcamera.h>
#include <qcamerainfo.h>
#include <qcameraimagecapture.h>
#include <qvideorenderercontrol.h>

#include "mockcameraservice.h"

#include "mockmediaserviceprovider.h"
#include "mockvideosurface.h"
#include "mockvideorenderercontrol.h"
#include "mockvideowindowcontrol.h"

QT_USE_NAMESPACE


class tst_QCamera: public QObject
{
    Q_OBJECT

public slots:
    void initTestCase();
    void init();
    void cleanup();

private slots:
    void testSimpleCamera();
    void testSimpleCameraWhiteBalance();
    void testSimpleCameraExposure();
    void testSimpleCameraFocus();
    void testSimpleCameraCapture();
    void testSimpleCameraLock();
    void testSimpleCaptureDestination();
    void testSimpleCaptureFormat();

    void testCameraWhiteBalance();
    void testCameraExposure();
    void testCameraFocus();
    void testCameraCapture();
    void testCameraCaptureMetadata();
    void testImageSettings();
    void testViewfinderSettings();
    void testViewfinderSettingsChange();
    void testSupportedViewfinderSettings_data();
    void testSupportedViewfinderSettings();
    void testSupportedViewfinderResolutions_data();
    void testSupportedViewfinderResolutions();
    void testSupportedViewfinderFrameRateRanges_data();
    void testSupportedViewfinderFrameRateRanges();
    void testSupportedViewfinderPixelFormats_data();
    void testSupportedViewfinderPixelFormats();
    void testCameraLock();
    void testCameraLockCancel();
    void testCameraEncodingProperyChange();
    void testCaptureDestination();
    void testCaptureFormat();

    void testConstructor();
    void testCaptureMode();
    void testIsCaptureModeSupported();
    void testRequestedLocks();
    void testSupportedLocks();
    void testQCameraIsAvailable();
    void testQCameraIsNotAvailable();
    void testSearchAndLockWithLockTypes();
    void testSetCaptureMode();
    void testUnlockWithType();
    void testCaptureModeChangedSignal();
    void testLockStatusChangedWithTypesSignal();
    void testErrorSignal();
    void testError();
    void testErrorString();
    void testStatus();
    void testLockType();
    void testLockChangeReason();


    // Test cases to for QCameraFocus
    void testCameraFocusIsAvailable();
    void testFocusModes();
    void testOpticalAndDigitalZoomChanged();
    void testMaxOpticalZoomChangedSignal();
    void testMaxDigitalZoomChangedSignal();
    void testfocusZonesChangedSignal();

    // Test cases for QCameraControl class.
    void testCameraControl();
    void testCaptureModeChanged_signal();
    void testEnumsOfQCameraControl();

    // Test case for QCameraImageProcessing class
    void testContrast();
    void testDenoisingLevel();
    void testIsAvailable();
    void testSaturation();
    void testSharpeningLevel();
    void testEnumOfQCameraImageProcessing();

    void testSetVideoOutput();
    void testSetVideoOutputNoService();
    void testSetVideoOutputNoControl();
    void testSetVideoOutputDestruction();

    void testEnumDebug();

    // constructor for QCameraImageProceesing
    void testImageProcessingControl();

    // Signals test cases for QCameraExposure
    void testSignalApertureChanged();
    void testSignalExposureCompensationChanged();
    void testSignalIsoSensitivityChanged();
    void testSignalShutterSpeedChanged();
    void testSignalFlashReady();

    // test constructor
    void testExposureControlConstructor();

private:
    MockSimpleCameraService  *mockSimpleCameraService;
    MockCameraService *mockCameraService;
    MockMediaServiceProvider *provider;
};

Q_DECLARE_METATYPE(QCamera::FrameRateRange)

void tst_QCamera::initTestCase()
{
    qRegisterMetaType<QCamera::FrameRateRange>("FrameRateRange");
}

void tst_QCamera::init()
{
    provider = new MockMediaServiceProvider;
    mockSimpleCameraService = new MockSimpleCameraService;
    mockCameraService = new MockCameraService;

    provider->service = mockCameraService;
    QMediaServiceProvider::setDefaultServiceProvider(provider);
}

void tst_QCamera::cleanup()
{
    delete provider;
    delete mockCameraService;
    delete mockSimpleCameraService;
}

void tst_QCamera::testSimpleCamera()
{
    provider->service = mockSimpleCameraService;
    QCamera camera;
    QCOMPARE(camera.service(), (QMediaService*)mockSimpleCameraService);

    QCOMPARE(camera.state(), QCamera::UnloadedState);
    camera.start();
    QCOMPARE(camera.state(), QCamera::ActiveState);
    camera.stop();
    QCOMPARE(camera.state(), QCamera::LoadedState);
    camera.unload();
    QCOMPARE(camera.state(), QCamera::UnloadedState);
    camera.load();
    QCOMPARE(camera.state(), QCamera::LoadedState);
}

void tst_QCamera::testSimpleCameraWhiteBalance()
{
    provider->service = mockSimpleCameraService;
    QCamera camera;

    //only WhiteBalanceAuto is supported
    QVERIFY(!camera.imageProcessing()->isWhiteBalanceModeSupported(QCameraImageProcessing::WhiteBalanceAuto));
    QVERIFY(!camera.imageProcessing()->isWhiteBalanceModeSupported(QCameraImageProcessing::WhiteBalanceCloudy));
    QCOMPARE(camera.imageProcessing()->whiteBalanceMode(), QCameraImageProcessing::WhiteBalanceAuto);
    camera.imageProcessing()->setWhiteBalanceMode(QCameraImageProcessing::WhiteBalanceCloudy);
    QCOMPARE(camera.imageProcessing()->whiteBalanceMode(), QCameraImageProcessing::WhiteBalanceAuto);
    QCOMPARE(camera.imageProcessing()->manualWhiteBalance()+1.0, 1.0);
    camera.imageProcessing()->setManualWhiteBalance(5000);
    QCOMPARE(camera.imageProcessing()->manualWhiteBalance()+1.0, 1.0);
}

void tst_QCamera::testSimpleCameraExposure()
{
    provider->service = mockSimpleCameraService;
    QCamera camera;
    QCameraExposure *cameraExposure = camera.exposure();
    QVERIFY(cameraExposure != 0);

    QVERIFY(!cameraExposure->isExposureModeSupported(QCameraExposure::ExposureAuto));
    QCOMPARE(cameraExposure->exposureMode(), QCameraExposure::ExposureAuto);
    cameraExposure->setExposureMode(QCameraExposure::ExposureManual);//should be ignored
    QCOMPARE(cameraExposure->exposureMode(), QCameraExposure::ExposureAuto);

    QVERIFY(!cameraExposure->isFlashModeSupported(QCameraExposure::FlashOff));
    QCOMPARE(cameraExposure->flashMode(), QCameraExposure::FlashOff);
    QCOMPARE(cameraExposure->isFlashReady(), false);
    cameraExposure->setFlashMode(QCameraExposure::FlashOn);
    QCOMPARE(cameraExposure->flashMode(), QCameraExposure::FlashOff);

    QVERIFY(!cameraExposure->isMeteringModeSupported(QCameraExposure::MeteringAverage));
    QVERIFY(!cameraExposure->isMeteringModeSupported(QCameraExposure::MeteringSpot));
    QVERIFY(!cameraExposure->isMeteringModeSupported(QCameraExposure::MeteringMatrix));
    QCOMPARE(cameraExposure->meteringMode(), QCameraExposure::MeteringMatrix);
    cameraExposure->setMeteringMode(QCameraExposure::MeteringSpot);
    QCOMPARE(cameraExposure->meteringMode(), QCameraExposure::MeteringMatrix);

    QCOMPARE(cameraExposure->spotMeteringPoint(), QPointF());
    cameraExposure->setSpotMeteringPoint(QPointF(0.5f, 0.5f));
    QCOMPARE(cameraExposure->spotMeteringPoint(), QPointF());

    QCOMPARE(cameraExposure->exposureCompensation(), 0.0);
    cameraExposure->setExposureCompensation(2.0);
    QCOMPARE(cameraExposure->exposureCompensation(), 0.0);

    QCOMPARE(cameraExposure->isoSensitivity(), -1);
    QVERIFY(cameraExposure->supportedIsoSensitivities().isEmpty());
    cameraExposure->setManualIsoSensitivity(100);
    QCOMPARE(cameraExposure->isoSensitivity(), -1);
    cameraExposure->setAutoIsoSensitivity();
    QCOMPARE(cameraExposure->isoSensitivity(), -1);

    QVERIFY(cameraExposure->aperture() < 0);
    QVERIFY(cameraExposure->supportedApertures().isEmpty());
    cameraExposure->setAutoAperture();
    QVERIFY(cameraExposure->aperture() < 0);
    cameraExposure->setManualAperture(5.6);
    QVERIFY(cameraExposure->aperture() < 0);

    QVERIFY(cameraExposure->shutterSpeed() < 0);
    QVERIFY(cameraExposure->supportedShutterSpeeds().isEmpty());
    cameraExposure->setAutoShutterSpeed();
    QVERIFY(cameraExposure->shutterSpeed() < 0);
    cameraExposure->setManualShutterSpeed(1/128.0);
    QVERIFY(cameraExposure->shutterSpeed() < 0);
}

void tst_QCamera::testSimpleCameraFocus()
{
    provider->service = mockSimpleCameraService;
    QCamera camera;

    QCameraFocus *cameraFocus = camera.focus();
    QVERIFY(cameraFocus != 0);

    QVERIFY(!cameraFocus->isFocusModeSupported(QCameraFocus::AutoFocus));
    QVERIFY(!cameraFocus->isFocusModeSupported(QCameraFocus::ContinuousFocus));
    QVERIFY(!cameraFocus->isFocusModeSupported(QCameraFocus::InfinityFocus));

    QCOMPARE(cameraFocus->focusMode(), QCameraFocus::AutoFocus);
    QTest::ignoreMessage(QtWarningMsg, "Focus mode selection is not supported");
    cameraFocus->setFocusMode(QCameraFocus::ContinuousFocus);
    QCOMPARE(cameraFocus->focusMode(), QCameraFocus::AutoFocus);

    QCOMPARE(cameraFocus->maximumOpticalZoom(), 1.0);
    QCOMPARE(cameraFocus->maximumDigitalZoom(), 1.0);
    QCOMPARE(cameraFocus->opticalZoom(), 1.0);
    QCOMPARE(cameraFocus->digitalZoom(), 1.0);

    QTest::ignoreMessage(QtWarningMsg, "The camera doesn't support zooming.");
    cameraFocus->zoomTo(100.0, 100.0);
    QCOMPARE(cameraFocus->opticalZoom(), 1.0);
    QCOMPARE(cameraFocus->digitalZoom(), 1.0);


    QVERIFY(!cameraFocus->isFocusPointModeSupported(QCameraFocus::FocusPointAuto));
    QCOMPARE(cameraFocus->focusPointMode(), QCameraFocus::FocusPointAuto);

    QTest::ignoreMessage(QtWarningMsg, "Focus points mode selection is not supported");
    cameraFocus->setFocusPointMode( QCameraFocus::FocusPointCenter );
    QCOMPARE(cameraFocus->focusPointMode(), QCameraFocus::FocusPointAuto);

    QCOMPARE(cameraFocus->customFocusPoint(), QPointF(0.5, 0.5));
    QTest::ignoreMessage(QtWarningMsg, "Focus points selection is not supported");
    cameraFocus->setCustomFocusPoint(QPointF(1.0, 1.0));
    QCOMPARE(cameraFocus->customFocusPoint(), QPointF(0.5, 0.5));
}

void tst_QCamera::testSimpleCameraCapture()
{
    provider->service = mockSimpleCameraService;
    QCamera camera;
    QCameraImageCapture imageCapture(&camera);

    QVERIFY(!imageCapture.isReadyForCapture());
    QVERIFY(!imageCapture.isAvailable());

    QCOMPARE(imageCapture.error(), QCameraImageCapture::NoError);
    QVERIFY(imageCapture.errorString().isEmpty());

    QSignalSpy errorSignal(&imageCapture, SIGNAL(error(int,QCameraImageCapture::Error,QString)));
    imageCapture.capture(QString::fromLatin1("/dev/null"));
    QCOMPARE(errorSignal.size(), 1);
    QCOMPARE(imageCapture.error(), QCameraImageCapture::NotSupportedFeatureError);
    QVERIFY(!imageCapture.errorString().isEmpty());
}

void tst_QCamera::testSimpleCameraLock()
{
    provider->service = mockSimpleCameraService;
    QCamera camera;
    QCOMPARE(camera.lockStatus(), QCamera::Unlocked);
    QCOMPARE(camera.lockStatus(QCamera::LockExposure), QCamera::Unlocked);
    QCOMPARE(camera.lockStatus(QCamera::LockFocus), QCamera::Unlocked);
    QCOMPARE(camera.lockStatus(QCamera::LockWhiteBalance), QCamera::Unlocked);

    QSignalSpy lockedSignal(&camera, SIGNAL(locked()));
    QSignalSpy lockFailedSignal(&camera, SIGNAL(lockFailed()));
    QSignalSpy lockStatusChangedSignal(&camera, SIGNAL(lockStatusChanged(QCamera::LockStatus,QCamera::LockChangeReason)));

    camera.searchAndLock();
    QCOMPARE(camera.lockStatus(), QCamera::Unlocked);
    QCOMPARE(camera.lockStatus(QCamera::LockExposure), QCamera::Unlocked);
    QCOMPARE(camera.lockStatus(QCamera::LockFocus), QCamera::Unlocked);
    QCOMPARE(camera.lockStatus(QCamera::LockWhiteBalance), QCamera::Unlocked);
    QCOMPARE(lockedSignal.count(), 0);
    QCOMPARE(lockFailedSignal.count(), 0);
    QCOMPARE(lockStatusChangedSignal.count(), 0);

    lockedSignal.clear();
    lockFailedSignal.clear();
    lockStatusChangedSignal.clear();

    camera.unlock();
    QCOMPARE(camera.lockStatus(), QCamera::Unlocked);
    QCOMPARE(camera.lockStatus(QCamera::LockExposure), QCamera::Unlocked);
    QCOMPARE(camera.lockStatus(QCamera::LockFocus), QCamera::Unlocked);
    QCOMPARE(camera.lockStatus(QCamera::LockWhiteBalance), QCamera::Unlocked);

    QCOMPARE(lockedSignal.count(), 0);
    QCOMPARE(lockFailedSignal.count(), 0);
    QCOMPARE(lockStatusChangedSignal.count(), 0);
}

void tst_QCamera::testSimpleCaptureDestination()
{
    provider->service = mockSimpleCameraService;
    QCamera camera;
    QCameraImageCapture imageCapture(&camera);

    QVERIFY(imageCapture.isCaptureDestinationSupported(QCameraImageCapture::CaptureToFile));
    QVERIFY(!imageCapture.isCaptureDestinationSupported(QCameraImageCapture::CaptureToBuffer));
    QVERIFY(!imageCapture.isCaptureDestinationSupported(
                QCameraImageCapture::CaptureToBuffer | QCameraImageCapture::CaptureToFile));

    QCOMPARE(imageCapture.captureDestination(), QCameraImageCapture::CaptureToFile);
    imageCapture.setCaptureDestination(QCameraImageCapture::CaptureToBuffer);
    QCOMPARE(imageCapture.captureDestination(), QCameraImageCapture::CaptureToFile);
}

void tst_QCamera::testSimpleCaptureFormat()
{
    provider->service = mockSimpleCameraService;
    QCamera camera;
    QCameraImageCapture imageCapture(&camera);

    QCOMPARE(imageCapture.bufferFormat(), QVideoFrame::Format_Invalid);
    QVERIFY(imageCapture.supportedBufferFormats().isEmpty());

    imageCapture.setBufferFormat(QVideoFrame::Format_AdobeDng);
    QCOMPARE(imageCapture.bufferFormat(), QVideoFrame::Format_Invalid);
}

void tst_QCamera::testCaptureDestination()
{
    QCamera camera;
    QCameraImageCapture imageCapture(&camera);

    QVERIFY(imageCapture.isCaptureDestinationSupported(QCameraImageCapture::CaptureToFile));
    QVERIFY(imageCapture.isCaptureDestinationSupported(QCameraImageCapture::CaptureToBuffer));
    QVERIFY(!imageCapture.isCaptureDestinationSupported(
                QCameraImageCapture::CaptureToBuffer | QCameraImageCapture::CaptureToFile));

    QSignalSpy destinationChangedSignal(&imageCapture, SIGNAL(captureDestinationChanged(QCameraImageCapture::CaptureDestinations)));

    QCOMPARE(imageCapture.captureDestination(), QCameraImageCapture::CaptureToFile);
    imageCapture.setCaptureDestination(QCameraImageCapture::CaptureToBuffer);
    QCOMPARE(imageCapture.captureDestination(), QCameraImageCapture::CaptureToBuffer);
    QCOMPARE(destinationChangedSignal.size(), 1);
    QCOMPARE(destinationChangedSignal.first().first().value<QCameraImageCapture::CaptureDestinations>(),
             QCameraImageCapture::CaptureToBuffer);

    //not supported combination
    imageCapture.setCaptureDestination(QCameraImageCapture::CaptureToBuffer | QCameraImageCapture::CaptureToFile);
    QCOMPARE(imageCapture.captureDestination(), QCameraImageCapture::CaptureToBuffer);
    QCOMPARE(destinationChangedSignal.size(), 1);
}

void tst_QCamera::testCaptureFormat()
{
    QCamera camera;
    QCameraImageCapture imageCapture(&camera);

    QSignalSpy formatChangedSignal(&imageCapture, SIGNAL(bufferFormatChanged(QVideoFrame::PixelFormat)));

    QCOMPARE(imageCapture.bufferFormat(), QVideoFrame::Format_Jpeg);
    QCOMPARE(imageCapture.supportedBufferFormats().size(), 3);

    imageCapture.setBufferFormat(QVideoFrame::Format_AdobeDng);
    QCOMPARE(imageCapture.bufferFormat(), QVideoFrame::Format_AdobeDng);

    QCOMPARE(formatChangedSignal.size(), 1);
    QCOMPARE(formatChangedSignal.first().first().value<QVideoFrame::PixelFormat>(),
             QVideoFrame::Format_AdobeDng);

    imageCapture.setBufferFormat(QVideoFrame::Format_Y16);
    QCOMPARE(imageCapture.bufferFormat(), QVideoFrame::Format_AdobeDng);

    QCOMPARE(formatChangedSignal.size(), 1);
}


void tst_QCamera::testCameraCapture()
{
    QCamera camera;
    QCameraImageCapture imageCapture(&camera);

    QVERIFY(!imageCapture.isReadyForCapture());

    QSignalSpy capturedSignal(&imageCapture, SIGNAL(imageCaptured(int,QImage)));
    QSignalSpy errorSignal(&imageCapture, SIGNAL(error(int,QCameraImageCapture::Error,QString)));

    imageCapture.capture(QString::fromLatin1("/dev/null"));
    QCOMPARE(capturedSignal.size(), 0);
    QCOMPARE(errorSignal.size(), 1);
    QCOMPARE(imageCapture.error(), QCameraImageCapture::NotReadyError);

    errorSignal.clear();

    camera.start();
    QVERIFY(imageCapture.isReadyForCapture());
    QCOMPARE(errorSignal.size(), 0);

    imageCapture.capture(QString::fromLatin1("/dev/null"));

    QTRY_COMPARE(capturedSignal.size(), 1);
    QCOMPARE(errorSignal.size(), 0);
    QCOMPARE(imageCapture.error(), QCameraImageCapture::NoError);
}

void tst_QCamera::testCameraCaptureMetadata()
{
    QCamera camera;
    QCameraImageCapture imageCapture(&camera);

    QSignalSpy metadataSignal(&imageCapture, SIGNAL(imageMetadataAvailable(int,QString,QVariant)));
    QSignalSpy savedSignal(&imageCapture, SIGNAL(imageSaved(int,QString)));

    camera.start();
    int id = imageCapture.capture(QString::fromLatin1("/dev/null"));

    QTRY_COMPARE(savedSignal.size(), 1);

    QCOMPARE(metadataSignal.size(), 3);

    QVariantList metadata = metadataSignal[0];
    QCOMPARE(metadata[0].toInt(), id);
    QCOMPARE(metadata[1].toString(), QMediaMetaData::FocalLengthIn35mmFilm);
    QCOMPARE(metadata[2].value<QVariant>().toInt(), 50);

    metadata = metadataSignal[1];
    QCOMPARE(metadata[0].toInt(), id);
    QCOMPARE(metadata[1].toString(), QMediaMetaData::DateTimeOriginal);
    QDateTime captureTime = metadata[2].value<QVariant>().value<QDateTime>();
    const qint64 dt = captureTime.secsTo(QDateTime::currentDateTime());
    QVERIFY2(qAbs(dt) < 5, QByteArray::number(dt).constData()); // it should not take more than 5 seconds for signal to arrive here

    metadata = metadataSignal[2];
    QCOMPARE(metadata[0].toInt(), id);
    QCOMPARE(metadata[1].toString(), QLatin1String("Answer to the Ultimate Question of Life, the Universe, and Everything"));
    QCOMPARE(metadata[2].value<QVariant>().toInt(), 42);
}


void tst_QCamera::testCameraWhiteBalance()
{
    QSet<QCameraImageProcessing::WhiteBalanceMode> whiteBalanceModes;
    whiteBalanceModes << QCameraImageProcessing::WhiteBalanceAuto;
    whiteBalanceModes << QCameraImageProcessing::WhiteBalanceFlash;
    whiteBalanceModes << QCameraImageProcessing::WhiteBalanceTungsten;

    MockCameraService service;
    service.mockImageProcessingControl->setWhiteBalanceMode(QCameraImageProcessing::WhiteBalanceFlash);
    service.mockImageProcessingControl->setSupportedWhiteBalanceModes(whiteBalanceModes);
    service.mockImageProcessingControl->setParameter(
                QCameraImageProcessingControl::ColorTemperature,
                QVariant(34));

    provider->service = &service;

    QCamera camera;
    QCameraImageProcessing *cameraImageProcessing = camera.imageProcessing();

    QCOMPARE(cameraImageProcessing->whiteBalanceMode(), QCameraImageProcessing::WhiteBalanceFlash);
    QVERIFY(camera.imageProcessing()->isWhiteBalanceModeSupported(QCameraImageProcessing::WhiteBalanceAuto));
    QVERIFY(camera.imageProcessing()->isWhiteBalanceModeSupported(QCameraImageProcessing::WhiteBalanceFlash));
    QVERIFY(camera.imageProcessing()->isWhiteBalanceModeSupported(QCameraImageProcessing::WhiteBalanceTungsten));
    QVERIFY(!camera.imageProcessing()->isWhiteBalanceModeSupported(QCameraImageProcessing::WhiteBalanceCloudy));

    cameraImageProcessing->setWhiteBalanceMode(QCameraImageProcessing::WhiteBalanceTungsten);
    QCOMPARE(cameraImageProcessing->whiteBalanceMode(), QCameraImageProcessing::WhiteBalanceTungsten);

    cameraImageProcessing->setWhiteBalanceMode(QCameraImageProcessing::WhiteBalanceManual);
    QCOMPARE(cameraImageProcessing->whiteBalanceMode(), QCameraImageProcessing::WhiteBalanceManual);
    QCOMPARE(cameraImageProcessing->manualWhiteBalance(), 34.0);

    cameraImageProcessing->setManualWhiteBalance(432.0);
    QCOMPARE(cameraImageProcessing->manualWhiteBalance(), 432.0);
}

void tst_QCamera::testCameraExposure()
{
    QCamera camera;

    QCameraExposure *cameraExposure = camera.exposure();
    QVERIFY(cameraExposure != 0);

    QVERIFY(cameraExposure->isExposureModeSupported(QCameraExposure::ExposureAuto));
    QCOMPARE(cameraExposure->exposureMode(), QCameraExposure::ExposureAuto);

    // Test Cases For QCameraExposure
    QVERIFY(cameraExposure->isExposureModeSupported(QCameraExposure::ExposureManual));
    cameraExposure->setExposureMode(QCameraExposure::ExposureManual);
    QCOMPARE(cameraExposure->exposureMode(), QCameraExposure::ExposureManual);

    QVERIFY(cameraExposure->isExposureModeSupported(QCameraExposure::ExposureNight));
    cameraExposure->setExposureMode(QCameraExposure::ExposureNight);
    QCOMPARE(cameraExposure->exposureMode(), QCameraExposure::ExposureNight);

    QVERIFY(cameraExposure->isExposureModeSupported(QCameraExposure::ExposureBacklight));
    cameraExposure->setExposureMode(QCameraExposure::ExposureBacklight);
    QCOMPARE(cameraExposure->exposureMode(), QCameraExposure::ExposureBacklight);


    QVERIFY(cameraExposure->isExposureModeSupported(QCameraExposure::ExposureSpotlight ));
    cameraExposure->setExposureMode(QCameraExposure::ExposureSpotlight);
    QCOMPARE(cameraExposure->exposureMode(), QCameraExposure::ExposureSpotlight);

    QVERIFY(cameraExposure->isExposureModeSupported(QCameraExposure::ExposureSports ));
    cameraExposure->setExposureMode(QCameraExposure::ExposureSports);
    QCOMPARE(cameraExposure->exposureMode(), QCameraExposure::ExposureSports);

    QVERIFY(cameraExposure->isExposureModeSupported(QCameraExposure::ExposureSnow ));
    cameraExposure->setExposureMode(QCameraExposure::ExposureSnow);
    QCOMPARE(cameraExposure->exposureMode(), QCameraExposure::ExposureSnow);

    QVERIFY(cameraExposure->isExposureModeSupported(QCameraExposure::ExposureBeach ));
    cameraExposure->setExposureMode(QCameraExposure::ExposureBeach);
    QCOMPARE(cameraExposure->exposureMode(), QCameraExposure::ExposureBeach);

    QVERIFY(cameraExposure->isExposureModeSupported(QCameraExposure::ExposureLargeAperture ));
    cameraExposure->setExposureMode(QCameraExposure::ExposureLargeAperture);
    QCOMPARE(cameraExposure->exposureMode(), QCameraExposure::ExposureLargeAperture);

    QVERIFY(cameraExposure->isExposureModeSupported(QCameraExposure::ExposureSmallAperture ));
    cameraExposure->setExposureMode(QCameraExposure::ExposureSmallAperture);
    QCOMPARE(cameraExposure->exposureMode(), QCameraExposure::ExposureSmallAperture);

    QVERIFY(cameraExposure->isExposureModeSupported(QCameraExposure::ExposurePortrait ));
    cameraExposure->setExposureMode(QCameraExposure::ExposurePortrait);
    QCOMPARE(cameraExposure->exposureMode(), QCameraExposure::ExposurePortrait);

    QVERIFY(cameraExposure->isExposureModeSupported(QCameraExposure::ExposureModeVendor ));
    cameraExposure->setExposureMode(QCameraExposure::ExposureModeVendor);
    QCOMPARE(cameraExposure->exposureMode(), QCameraExposure::ExposureModeVendor);


    cameraExposure->setFlashMode(QCameraExposure::FlashAuto);
    QCOMPARE(cameraExposure->flashMode(), QCameraExposure::FlashAuto);
    QCOMPARE(cameraExposure->isFlashReady(), true);
    cameraExposure->setFlashMode(QCameraExposure::FlashRedEyeReduction);
    QCOMPARE(cameraExposure->flashMode(), QCameraExposure::FlashRedEyeReduction);
    cameraExposure->setFlashMode(QCameraExposure::FlashOn);
    QCOMPARE(cameraExposure->flashMode(), QCameraExposure::FlashOn);
    cameraExposure->setFlashMode(QCameraExposure::FlashFill);
    QCOMPARE(cameraExposure->flashMode(), QCameraExposure::FlashFill);
    cameraExposure->setFlashMode(QCameraExposure::FlashTorch);
    QCOMPARE(cameraExposure->flashMode(), QCameraExposure::FlashTorch);
    cameraExposure->setFlashMode(QCameraExposure::FlashSlowSyncFrontCurtain);
    QCOMPARE(cameraExposure->flashMode(), QCameraExposure::FlashSlowSyncFrontCurtain);

    QVERIFY(!cameraExposure->isMeteringModeSupported(QCameraExposure::MeteringAverage));
    QVERIFY(cameraExposure->isMeteringModeSupported(QCameraExposure::MeteringSpot));
    QVERIFY(cameraExposure->isMeteringModeSupported(QCameraExposure::MeteringMatrix));

    cameraExposure->setMeteringMode(QCameraExposure::MeteringMatrix);
    QCOMPARE(cameraExposure->meteringMode(), QCameraExposure::MeteringMatrix);

    //MeteringAverage is not supported, metering mode should not be changed
    cameraExposure->setMeteringMode(QCameraExposure::MeteringAverage);
    QCOMPARE(cameraExposure->meteringMode(), QCameraExposure::MeteringMatrix);

    cameraExposure->setMeteringMode(QCameraExposure::MeteringSpot);
    QCOMPARE(cameraExposure->meteringMode(), QCameraExposure::MeteringSpot);

    cameraExposure->setSpotMeteringPoint(QPointF(0.5f, 0.25f));
    QCOMPARE(cameraExposure->spotMeteringPoint(), QPointF(0.5f, 0.25f));
    cameraExposure->setSpotMeteringPoint(QPointF(0.25f, 56.3f));
    QCOMPARE(cameraExposure->spotMeteringPoint(), QPointF(0.5f, 0.25f));
    cameraExposure->setSpotMeteringPoint(QPointF(0, 0));
    QCOMPARE(cameraExposure->spotMeteringPoint(), QPointF(0, 0));
    cameraExposure->setSpotMeteringPoint(QPointF(1, 1));
    QCOMPARE(cameraExposure->spotMeteringPoint(), QPointF(1, 1));

    QCOMPARE(cameraExposure->exposureCompensation(), 0.0);
    cameraExposure->setExposureCompensation(2.0);
    QCOMPARE(cameraExposure->exposureCompensation(), 2.0);

    int minIso = cameraExposure->supportedIsoSensitivities().first();
    int maxIso = cameraExposure->supportedIsoSensitivities().last();
    QVERIFY(cameraExposure->isoSensitivity() > 0);
    QCOMPARE(cameraExposure->requestedIsoSensitivity(), -1);
    QVERIFY(minIso > 0);
    QVERIFY(maxIso > 0);
    cameraExposure->setManualIsoSensitivity(minIso);
    QCOMPARE(cameraExposure->isoSensitivity(), minIso);
    cameraExposure->setManualIsoSensitivity(maxIso*10);
    QCOMPARE(cameraExposure->isoSensitivity(), maxIso);
    QCOMPARE(cameraExposure->requestedIsoSensitivity(), maxIso*10);

    cameraExposure->setManualIsoSensitivity(-10);
    QCOMPARE(cameraExposure->isoSensitivity(), minIso);
    QCOMPARE(cameraExposure->requestedIsoSensitivity(), -10);
    cameraExposure->setAutoIsoSensitivity();
    QCOMPARE(cameraExposure->isoSensitivity(), 100);
    QCOMPARE(cameraExposure->requestedIsoSensitivity(), -1);

    QCOMPARE(cameraExposure->requestedAperture(), -1.0);
    qreal minAperture = cameraExposure->supportedApertures().first();
    qreal maxAperture = cameraExposure->supportedApertures().last();
    QVERIFY(minAperture > 0);
    QVERIFY(maxAperture > 0);
    QVERIFY(cameraExposure->aperture() >= minAperture);
    QVERIFY(cameraExposure->aperture() <= maxAperture);

    cameraExposure->setAutoAperture();
    QVERIFY(cameraExposure->aperture() >= minAperture);
    QVERIFY(cameraExposure->aperture() <= maxAperture);
    QCOMPARE(cameraExposure->requestedAperture(), -1.0);

    cameraExposure->setManualAperture(0);
    QCOMPARE(cameraExposure->aperture(), minAperture);
    QCOMPARE(cameraExposure->requestedAperture()+1.0, 1.0);

    cameraExposure->setManualAperture(10000);
    QCOMPARE(cameraExposure->aperture(), maxAperture);
    QCOMPARE(cameraExposure->requestedAperture(), 10000.0);

    cameraExposure->setAutoAperture();
    QCOMPARE(cameraExposure->requestedAperture(), -1.0);

    QCOMPARE(cameraExposure->requestedShutterSpeed(), -1.0);
    qreal minShutterSpeed = cameraExposure->supportedShutterSpeeds().first();
    qreal maxShutterSpeed = cameraExposure->supportedShutterSpeeds().last();
    QVERIFY(minShutterSpeed > 0);
    QVERIFY(maxShutterSpeed > 0);
    QVERIFY(cameraExposure->shutterSpeed() >= minShutterSpeed);
    QVERIFY(cameraExposure->shutterSpeed() <= maxShutterSpeed);

    cameraExposure->setAutoShutterSpeed();
    QVERIFY(cameraExposure->shutterSpeed() >= minShutterSpeed);
    QVERIFY(cameraExposure->shutterSpeed() <= maxShutterSpeed);
    QCOMPARE(cameraExposure->requestedShutterSpeed(), -1.0);

    cameraExposure->setManualShutterSpeed(0);
    QCOMPARE(cameraExposure->shutterSpeed(), minShutterSpeed);
    QCOMPARE(cameraExposure->requestedShutterSpeed()+1.0, 1.0);

    cameraExposure->setManualShutterSpeed(10000);
    QCOMPARE(cameraExposure->shutterSpeed(), maxShutterSpeed);
    QCOMPARE(cameraExposure->requestedShutterSpeed(), 10000.0);

    cameraExposure->setAutoShutterSpeed();
    QCOMPARE(cameraExposure->requestedShutterSpeed(), -1.0);
}

void tst_QCamera::testCameraFocus()
{
    QCamera camera;

    QCameraFocus *cameraFocus = camera.focus();
    QVERIFY(cameraFocus != 0);

    QVERIFY(cameraFocus->isFocusModeSupported(QCameraFocus::AutoFocus));
    QVERIFY(cameraFocus->isFocusModeSupported(QCameraFocus::ContinuousFocus));
    QVERIFY(!cameraFocus->isFocusModeSupported(QCameraFocus::InfinityFocus));

    QCOMPARE(cameraFocus->focusMode(), QCameraFocus::AutoFocus);
    cameraFocus->setFocusMode(QCameraFocus::ManualFocus);
    QCOMPARE(cameraFocus->focusMode(), QCameraFocus::AutoFocus);
    cameraFocus->setFocusMode(QCameraFocus::ContinuousFocus);
    QCOMPARE(cameraFocus->focusMode(), QCameraFocus::ContinuousFocus);

    QVERIFY(cameraFocus->maximumOpticalZoom() >= 1.0);
    QVERIFY(cameraFocus->maximumDigitalZoom() >= 1.0);
    QCOMPARE(cameraFocus->opticalZoom(), 1.0);
    QCOMPARE(cameraFocus->digitalZoom(), 1.0);
    cameraFocus->zoomTo(0.5, 1.0);
    QCOMPARE(cameraFocus->opticalZoom(), 1.0);
    QCOMPARE(cameraFocus->digitalZoom(), 1.0);
    cameraFocus->zoomTo(2.0, 0.5);
    QCOMPARE(cameraFocus->opticalZoom(), 2.0);
    QCOMPARE(cameraFocus->digitalZoom(), 1.0);
    cameraFocus->zoomTo(2.0, 2.5);
    QCOMPARE(cameraFocus->opticalZoom(), 2.0);
    QCOMPARE(cameraFocus->digitalZoom(), 2.5);
    cameraFocus->zoomTo(2000000.0, 1000000.0);
    QVERIFY(qFuzzyCompare(cameraFocus->opticalZoom(), cameraFocus->maximumOpticalZoom()));
    QVERIFY(qFuzzyCompare(cameraFocus->digitalZoom(), cameraFocus->maximumDigitalZoom()));

    QVERIFY(cameraFocus->isFocusPointModeSupported(QCameraFocus::FocusPointAuto));
    QVERIFY(cameraFocus->isFocusPointModeSupported(QCameraFocus::FocusPointCenter));
    QVERIFY(cameraFocus->isFocusPointModeSupported(QCameraFocus::FocusPointCustom));
    QCOMPARE(cameraFocus->focusPointMode(), QCameraFocus::FocusPointAuto);

    cameraFocus->setFocusPointMode( QCameraFocus::FocusPointCenter );
    QCOMPARE(cameraFocus->focusPointMode(), QCameraFocus::FocusPointCenter);

    cameraFocus->setFocusPointMode( QCameraFocus::FocusPointFaceDetection );
    QCOMPARE(cameraFocus->focusPointMode(), QCameraFocus::FocusPointCenter);

    QCOMPARE(cameraFocus->customFocusPoint(), QPointF(0.5, 0.5));
    cameraFocus->setCustomFocusPoint(QPointF(1.0, 1.0));
    QCOMPARE(cameraFocus->customFocusPoint(), QPointF(1.0, 1.0));
}

void tst_QCamera::testImageSettings()
{
    QImageEncoderSettings settings;
    QVERIFY(settings.isNull());
    QVERIFY(settings == QImageEncoderSettings());

    QCOMPARE(settings.codec(), QString());
    settings.setCodec(QLatin1String("codecName"));
    QCOMPARE(settings.codec(), QLatin1String("codecName"));
    QVERIFY(!settings.isNull());
    QVERIFY(settings != QImageEncoderSettings());

    settings = QImageEncoderSettings();
    QCOMPARE(settings.quality(), QMultimedia::NormalQuality);
    settings.setQuality(QMultimedia::HighQuality);
    QCOMPARE(settings.quality(), QMultimedia::HighQuality);
    QVERIFY(!settings.isNull());

    settings = QImageEncoderSettings();
    QCOMPARE(settings.resolution(), QSize());
    settings.setResolution(QSize(320,240));
    QCOMPARE(settings.resolution(), QSize(320,240));
    settings.setResolution(800,600);
    QCOMPARE(settings.resolution(), QSize(800,600));
    QVERIFY(!settings.isNull());

    settings = QImageEncoderSettings();
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
    QVERIFY(settings != QImageEncoderSettings());

    settings = QImageEncoderSettings();
    QVERIFY(settings.isNull());
    QCOMPARE(settings.codec(), QString());
    QCOMPARE(settings.quality(), QMultimedia::NormalQuality);
    QCOMPARE(settings.resolution(), QSize());
    QVERIFY(settings.encodingOptions().isEmpty());

    {
        QImageEncoderSettings settings1;
        QImageEncoderSettings settings2;
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
        QImageEncoderSettings settings1;
        QImageEncoderSettings settings2(settings1);
        QCOMPARE(settings2, settings1);

        settings2 = settings1;
        QCOMPARE(settings2, settings1);
        QVERIFY(settings2.isNull());

        settings1.setQuality(QMultimedia::HighQuality);

        QVERIFY(settings2.isNull());
        QVERIFY(!settings1.isNull());
        QVERIFY(settings1 != settings2);
    }

    QImageEncoderSettings settings1;
    QImageEncoderSettings settings2;

    settings1 = QImageEncoderSettings();
    settings1.setResolution(800,600);
    settings2 = QImageEncoderSettings();
    settings2.setResolution(QSize(800,600));
    QVERIFY(settings1 == settings2);
    settings2.setResolution(QSize(400,300));
    QVERIFY(settings1 != settings2);

    settings1 = QImageEncoderSettings();
    settings1.setCodec("codec1");
    settings2 = QImageEncoderSettings();
    settings2.setCodec("codec1");
    QVERIFY(settings1 == settings2);
    settings2.setCodec("codec2");
    QVERIFY(settings1 != settings2);

    settings1 = QImageEncoderSettings();
    settings1.setQuality(QMultimedia::NormalQuality);
    settings2 = QImageEncoderSettings();
    settings2.setQuality(QMultimedia::NormalQuality);
    QVERIFY(settings1 == settings2);
    settings2.setQuality(QMultimedia::LowQuality);
    QVERIFY(settings1 != settings2);

    settings1 = QImageEncoderSettings();
    settings1.setEncodingOption(QLatin1Literal("encoderOption"), QVariant(1));
    settings2 = QImageEncoderSettings();
    settings2.setEncodingOption(QLatin1Literal("encoderOption"), QVariant(1));
    QVERIFY(settings1 == settings2);
    settings2.setEncodingOption(QLatin1Literal("encoderOption"), QVariant(2));
    QVERIFY(settings1 != settings2);
}

void tst_QCamera::testViewfinderSettings()
{
    QCameraViewfinderSettings settings;
    QVERIFY(settings.isNull());
    QVERIFY(settings == QCameraViewfinderSettings());

    QCOMPARE(settings.resolution(), QSize());
    settings.setResolution(QSize(640, 480));
    QCOMPARE(settings.resolution(), QSize(640, 480));
    settings.setResolution(1280, 720);
    QCOMPARE(settings.resolution(), QSize(1280, 720));
    QVERIFY(!settings.isNull());
    QVERIFY(settings != QCameraViewfinderSettings());

    settings = QCameraViewfinderSettings();
    QVERIFY(qFuzzyIsNull(settings.minimumFrameRate()));
    settings.setMinimumFrameRate(10.0);
    QVERIFY(qFuzzyCompare(settings.minimumFrameRate(), 10.0));
    QVERIFY(qFuzzyIsNull(settings.maximumFrameRate()));
    settings.setMaximumFrameRate(20.0);
    QVERIFY(qFuzzyCompare(settings.maximumFrameRate(), 20.0));
    QVERIFY(!settings.isNull());

    settings = QCameraViewfinderSettings();
    QCOMPARE(settings.pixelFormat(), QVideoFrame::Format_Invalid);
    settings.setPixelFormat(QVideoFrame::Format_RGB32);
    QCOMPARE(settings.pixelFormat(), QVideoFrame::Format_RGB32);
    QVERIFY(!settings.isNull());

    settings = QCameraViewfinderSettings();
    QCOMPARE(settings.pixelAspectRatio(), QSize());
    settings.setPixelAspectRatio(QSize(2, 1));
    QCOMPARE(settings.pixelAspectRatio(), QSize(2, 1));
    settings.setPixelAspectRatio(3, 2);
    QCOMPARE(settings.pixelAspectRatio(), QSize(3, 2));
    QVERIFY(!settings.isNull());

    settings = QCameraViewfinderSettings();

    {
        QCameraViewfinderSettings settings1;
        QCameraViewfinderSettings settings2;
        QCOMPARE(settings2, settings1);

        settings2 = settings1;
        QCOMPARE(settings2, settings1);
        QVERIFY(settings2.isNull());

        settings1.setResolution(800, 600);

        QVERIFY(settings2.isNull());
        QVERIFY(!settings1.isNull());
        QVERIFY(settings1 != settings2);
    }

    {
        QCameraViewfinderSettings settings1;
        QCameraViewfinderSettings settings2(settings1);
        QCOMPARE(settings2, settings1);

        settings2 = settings1;
        QCOMPARE(settings2, settings1);
        QVERIFY(settings2.isNull());

        settings1.setResolution(800, 600);

        QVERIFY(settings2.isNull());
        QVERIFY(!settings1.isNull());
        QVERIFY(settings1 != settings2);
    }

    QCameraViewfinderSettings settings1;
    QCameraViewfinderSettings settings2;

    settings1 = QCameraViewfinderSettings();
    settings1.setResolution(800,600);
    settings2 = QCameraViewfinderSettings();
    settings2.setResolution(QSize(800,600));
    QVERIFY(settings1 == settings2);
    settings2.setResolution(QSize(400,300));
    QVERIFY(settings1 != settings2);

    settings1 = QCameraViewfinderSettings();
    settings1.setMinimumFrameRate(10.0);
    settings2 = QCameraViewfinderSettings();
    settings2.setMinimumFrameRate(10.0);
    QVERIFY(settings1 == settings2);
    settings2.setMinimumFrameRate(15.0);
    QVERIFY(settings1 != settings2);

    settings1 = QCameraViewfinderSettings();
    settings1.setMaximumFrameRate(30.0);
    settings2 = QCameraViewfinderSettings();
    settings2.setMaximumFrameRate(30.0);
    QVERIFY(settings1 == settings2);
    settings2.setMaximumFrameRate(15.0);
    QVERIFY(settings1 != settings2);

    settings1 = QCameraViewfinderSettings();
    settings1.setPixelFormat(QVideoFrame::Format_YV12);
    settings2 = QCameraViewfinderSettings();
    settings2.setPixelFormat(QVideoFrame::Format_YV12);
    QVERIFY(settings1 == settings2);
    settings2.setPixelFormat(QVideoFrame::Format_NV21);
    QVERIFY(settings1 != settings2);

    settings1 = QCameraViewfinderSettings();
    settings1.setPixelAspectRatio(2,1);
    settings2 = QCameraViewfinderSettings();
    settings2.setPixelAspectRatio(QSize(2,1));
    QVERIFY(settings1 == settings2);
    settings2.setPixelAspectRatio(QSize(1,2));
    QVERIFY(settings1 != settings2);
}

void tst_QCamera::testViewfinderSettingsChange()
{
    QCamera camera;

    QSignalSpy stateChangedSignal(&camera, SIGNAL(stateChanged(QCamera::State)));
    QSignalSpy statusChangedSignal(&camera, SIGNAL(statusChanged(QCamera::Status)));

    camera.start();
    QCOMPARE(camera.state(), QCamera::ActiveState);
    QCOMPARE(camera.status(), QCamera::ActiveStatus);

    QCOMPARE(stateChangedSignal.count(), 1);
    QCOMPARE(statusChangedSignal.count(), 1);
    stateChangedSignal.clear();
    statusChangedSignal.clear();

    //the settings change should trigger camera stop/start
    camera.setViewfinderSettings(QCameraViewfinderSettings());
    QCOMPARE(camera.state(), QCamera::ActiveState);
    QCOMPARE(camera.status(), QCamera::LoadedStatus);

    QCOMPARE(stateChangedSignal.count(), 0);
    QCOMPARE(statusChangedSignal.count(), 1);
    stateChangedSignal.clear();
    statusChangedSignal.clear();

    QCOMPARE(camera.state(), QCamera::ActiveState);
    QTRY_COMPARE(camera.status(), QCamera::ActiveStatus);

    QCOMPARE(stateChangedSignal.count(), 0);
    QCOMPARE(statusChangedSignal.count(), 1);
    stateChangedSignal.clear();
    statusChangedSignal.clear();

    //the settings change should trigger camera stop/start only once
    camera.setViewfinderSettings(QCameraViewfinderSettings());
    camera.setViewfinderSettings(QCameraViewfinderSettings());

    QCOMPARE(camera.state(), QCamera::ActiveState);
    QCOMPARE(camera.status(), QCamera::LoadedStatus);

    QCOMPARE(stateChangedSignal.count(), 0);
    QCOMPARE(statusChangedSignal.count(), 1);
    stateChangedSignal.clear();
    statusChangedSignal.clear();

    QCOMPARE(camera.state(), QCamera::ActiveState);
    QTRY_COMPARE(camera.status(), QCamera::ActiveStatus);

    QCOMPARE(stateChangedSignal.count(), 0);
    QCOMPARE(statusChangedSignal.count(), 1);
}

class ViewfinderSettingsBuilder
{
public:
    ViewfinderSettingsBuilder &setResolution(int width, int height) {
        m_settings.setResolution(width, height);
        return *this;
    }

    ViewfinderSettingsBuilder &setMinimumFrameRate(qreal r) {
        m_settings.setMinimumFrameRate(r);
        return *this;
    }

    ViewfinderSettingsBuilder &setMaximumFrameRate(qreal r) {
        m_settings.setMaximumFrameRate(r);
        return *this;
    }

    ViewfinderSettingsBuilder &setPixelFormat(QVideoFrame::PixelFormat f) {
        m_settings.setPixelFormat(f);
        return *this;
    }

    ViewfinderSettingsBuilder &setPixelAspectRatio(int h, int v) {
        m_settings.setPixelAspectRatio(h, v);
        return *this;
    }

    QCameraViewfinderSettings build() {
        QCameraViewfinderSettings s = m_settings;
        m_settings = QCameraViewfinderSettings();
        return s;
    }

private:
    QCameraViewfinderSettings m_settings;
};

void tst_QCamera::testSupportedViewfinderSettings_data()
{
    // see mockcameraviewfindersettingscontrol.h for expected values

    ViewfinderSettingsBuilder builder;

    QTest::addColumn<QCameraViewfinderSettings>("settings");
    QTest::addColumn< QList<int> >("expectedSupportedSettings");

    QTest::newRow("all supported settings") << QCameraViewfinderSettings()
                                            << (QList<int>() << 0 << 1 << 2 << 3 << 4 << 5);

    QTest::newRow("invalid resolution") << builder.setResolution(452472, 44453).build()
                                        << QList<int>();

    QTest::newRow("resolution (1)") << builder.setResolution(640, 480).build()
                                    << (QList<int>() << 0);

    QTest::newRow("resolution (2)") << builder.setResolution(1280, 720).build()
                                    << (QList<int>() << 1 << 3 << 4);

    QTest::newRow("invalid minimum frame rate") << builder.setMinimumFrameRate(2).build()
                                                << QList<int>();

    QTest::newRow("minimum frame rate (1)") << builder.setMinimumFrameRate(5).build()
                                            << (QList<int>() << 2);

    QTest::newRow("minimum frame rate (2)") << builder.setMinimumFrameRate(10).build()
                                            << (QList<int>() << 1 << 3);

    QTest::newRow("minimum frame rate (3)") << builder.setMinimumFrameRate(30).build()
                                            << (QList<int>() << 0 << 4 << 5);

    QTest::newRow("invalid maximum frame rate") << builder.setMaximumFrameRate(2).build()
                                                << QList<int>();

    QTest::newRow("maximum frame rate (1)") << builder.setMaximumFrameRate(10).build()
                                            << (QList<int>() << 1 << 2 << 3);

    QTest::newRow("maximum frame rate (2)") << builder.setMaximumFrameRate(30).build()
                                            << (QList<int>() << 0 << 4 << 5);

    QTest::newRow("invalid pixel format") << builder.setPixelFormat(QVideoFrame::Format_CameraRaw).build()
                                          << QList<int>();

    QTest::newRow("pixel format (1)") << builder.setPixelFormat(QVideoFrame::Format_BGR32).build()
                                      << (QList<int>() << 2);

    QTest::newRow("pixel format (2)") << builder.setPixelFormat(QVideoFrame::Format_YV12).build()
                                      << (QList<int>() << 3 << 4);

    QTest::newRow("pixel format (3)") << builder.setPixelFormat(QVideoFrame::Format_NV12).build()
                                      << (QList<int>() << 0 << 1 << 5);

    QTest::newRow("invalid pixel aspect ratio") << builder.setPixelAspectRatio(5, 3).build()
                                                << QList<int>();

    QTest::newRow("pixel aspect ratio (1)") << builder.setPixelAspectRatio(2, 1).build()
                                            << (QList<int>() << 2);

    QTest::newRow("pixel aspect ratio (2)") << builder.setPixelAspectRatio(1, 1).build()
                                            << (QList<int>() << 0 << 1 << 3 << 4 << 5);

    QTest::newRow("invalid multi settings") << builder.setResolution(640, 480)
                                                      .setMinimumFrameRate(10)
                                                      .setMaximumFrameRate(10)
                                                      .build()
                                            << QList<int>();

    QTest::newRow("multi settings (1)") << builder.setResolution(640, 480)
                                                  .setMinimumFrameRate(30)
                                                  .setMaximumFrameRate(30)
                                                  .build()
                                        << (QList<int>() << 0);

    QTest::newRow("multi settings (2)") << builder.setResolution(1280, 720)
                                                  .setMinimumFrameRate(10)
                                                  .setMaximumFrameRate(10)
                                                  .build()
                                        << (QList<int>() << 1 << 3);

    QTest::newRow("multi settings (3)") << builder.setPixelFormat(QVideoFrame::Format_NV12)
                                                  .setMinimumFrameRate(30)
                                                  .build()
                                        << (QList<int>() << 0 << 5);

    QTest::newRow("multi settings (4)") << builder.setPixelAspectRatio(1, 1)
                                                  .setMaximumFrameRate(10)
                                                  .build()
                                        << (QList<int>() << 1 << 3);
}

void tst_QCamera::testSupportedViewfinderSettings()
{
    QFETCH(QCameraViewfinderSettings, settings);
    QFETCH(QList<int>, expectedSupportedSettings);

    QList<QCameraViewfinderSettings> actualSupportedSettings = QCamera().supportedViewfinderSettings(settings);
    QCOMPARE(actualSupportedSettings.size(), expectedSupportedSettings.size());
    for (int i = 0; i < expectedSupportedSettings.size(); ++i) {
        QCameraViewfinderSettings expectedSettings = mockCameraService->mockViewfinderSettingsControl->supportedSettings.at(expectedSupportedSettings.at(i));
        QCOMPARE(actualSupportedSettings.at(i), expectedSettings);
    }
}

void tst_QCamera::testSupportedViewfinderResolutions_data()
{
    // see mockcameraviewfindersettingscontrol.h for expected values

    typedef QList<QSize> SizeList;
    ViewfinderSettingsBuilder builder;

    QTest::addColumn<QCameraViewfinderSettings>("settings");
    QTest::addColumn<SizeList>("expectedResolutions");

    QTest::newRow("empty settings") << QCameraViewfinderSettings()
                                    << (SizeList() << QSize(320, 240)
                                                   << QSize(640, 480)
                                                   << QSize(1280, 720)
                                                   << QSize(1920, 1080));

    QTest::newRow("invalid minimum frame rate") << builder.setMinimumFrameRate(2).build()
                                                << SizeList();

    QTest::newRow("minimum frame rate (1)") << builder.setMinimumFrameRate(5).build()
                                            << (SizeList() << QSize(1920, 1080));

    QTest::newRow("minimum frame rate (2)") << builder.setMinimumFrameRate(10).build()
                                            << (SizeList() << QSize(1280, 720));

    QTest::newRow("minimum frame rate (3)") << builder.setMinimumFrameRate(30).build()
                                            << (SizeList() << QSize(320, 240)
                                                           << QSize(640, 480)
                                                           << QSize(1280, 720));

    QTest::newRow("invalid maximum frame rate") << builder.setMaximumFrameRate(2).build()
                                                << SizeList();

    QTest::newRow("maximum frame rate") << builder.setMaximumFrameRate(10).build()
                                        << (SizeList() << QSize(1280, 720)
                                                       << QSize(1920, 1080));

    QTest::newRow("invalid pixel format") << builder.setPixelFormat(QVideoFrame::Format_CameraRaw).build()
                                          << SizeList();

    QTest::newRow("pixel format (1)") << builder.setPixelFormat(QVideoFrame::Format_BGR32).build()
                                      << (SizeList() << QSize(1920, 1080));

    QTest::newRow("pixel format (2)") << builder.setPixelFormat(QVideoFrame::Format_YV12).build()
                                      << (SizeList() << QSize(1280, 720));

    QTest::newRow("pixel format (3)") << builder.setPixelFormat(QVideoFrame::Format_NV12).build()
                                      << (SizeList() << QSize(320, 240)
                                                     << QSize(640, 480)
                                                     << QSize(1280, 720));

    QTest::newRow("invalid pixel aspect ratio") << builder.setPixelAspectRatio(7, 2).build()
                                                << SizeList();

    QTest::newRow("pixel aspect ratio (1") << builder.setPixelAspectRatio(2, 1).build()
                                           << (SizeList() << QSize(1920, 1080));

    QTest::newRow("pixel aspect ratio (2") << builder.setPixelAspectRatio(1, 1).build()
                                           << (SizeList() << QSize(320, 240)
                                                          << QSize(640, 480)
                                                          << QSize(1280, 720));

    QTest::newRow("invalid multi settings (1)") << builder.setMinimumFrameRate(2)
                                                          .setMaximumFrameRate(3)
                                                          .build()
                                                << SizeList();

    QTest::newRow("invalid multi settings (2)") << builder.setMinimumFrameRate(5)
                                                          .setMaximumFrameRate(11)
                                                          .build()
                                                << SizeList();

    QTest::newRow("multi settings (1)") << builder.setMinimumFrameRate(10)
                                                  .setMaximumFrameRate(10)
                                                  .build()
                                        << (SizeList() << QSize(1280, 720));

    QTest::newRow("multi settings (2)") << builder.setMinimumFrameRate(30)
                                                  .setMaximumFrameRate(30)
                                                  .build()
                                        << (SizeList() << QSize(320, 240)
                                                       << QSize(640, 480)
                                                       << QSize(1280, 720));

    QTest::newRow("multi settings (3)") << builder.setPixelFormat(QVideoFrame::Format_NV12)
                                                  .setMinimumFrameRate(30)
                                                  .build()
                                        << (SizeList() << QSize(320, 240)
                                                       << QSize(640, 480));

    QTest::newRow("multi settings (4)") << builder.setPixelAspectRatio(1, 1)
                                                  .setMaximumFrameRate(10)
                                                  .build()
                                        << (SizeList() << QSize(1280, 720));
}

void tst_QCamera::testSupportedViewfinderResolutions()
{
    QFETCH(QCameraViewfinderSettings, settings);
    QFETCH(QList<QSize>, expectedResolutions);

    QList<QSize> actualResolutions = QCamera().supportedViewfinderResolutions(settings);
    QCOMPARE(actualResolutions.size(), expectedResolutions.size());
    for (int i = 0; i < actualResolutions.size(); ++i)
        QCOMPARE(actualResolutions.at(i), expectedResolutions.at(i));
}

void tst_QCamera::testSupportedViewfinderFrameRateRanges_data()
{
    // see mockcameraviewfindersettingscontrol.h for expected values

    typedef QList<QCamera::FrameRateRange> RangeList;
    ViewfinderSettingsBuilder builder;

    QTest::addColumn<QCameraViewfinderSettings>("settings");
    QTest::addColumn<RangeList>("expectedFrameRateRanges");

    QTest::newRow("empty settings") << QCameraViewfinderSettings()
                                    << (RangeList() << QCamera::FrameRateRange(5, 10)
                                                    << QCamera::FrameRateRange(10, 10)
                                                    << QCamera::FrameRateRange(30, 30));

    QTest::newRow("invalid resolution") << builder.setResolution(452472, 444534).build()
                                        << RangeList();

    QTest::newRow("resolution (1)") << builder.setResolution(320, 240).build()
                                    << (RangeList() << QCamera::FrameRateRange(30, 30));

    QTest::newRow("resolution (2)") << builder.setResolution(1280, 720).build()
                                    << (RangeList() << QCamera::FrameRateRange(10, 10)
                                                    << QCamera::FrameRateRange(30, 30));

    QTest::newRow("resolution (3)") << builder.setResolution(1920, 1080).build()
                                    << (RangeList() << QCamera::FrameRateRange(5, 10));

    QTest::newRow("invalid minimum frame rate") << builder.setMinimumFrameRate(2).build()
                                                << RangeList();

    QTest::newRow("minimum frame rate (1)") << builder.setMinimumFrameRate(5).build()
                                            << (RangeList() << QCamera::FrameRateRange(5, 10));

    QTest::newRow("minimum frame rate (2)") << builder.setMinimumFrameRate(10).build()
                                            << (RangeList() << QCamera::FrameRateRange(10, 10));

    QTest::newRow("invalid maximum frame rate") << builder.setMaximumFrameRate(2).build()
                                                << RangeList();

    QTest::newRow("maximum frame rate (1)") << builder.setMaximumFrameRate(10).build()
                                            << (RangeList() << QCamera::FrameRateRange(5, 10)
                                                            << QCamera::FrameRateRange(10, 10));

    QTest::newRow("maximum frame rate (2)") << builder.setMaximumFrameRate(30).build()
                                            << (RangeList() << QCamera::FrameRateRange(30, 30));

    QTest::newRow("invalid pixel format") << builder.setPixelFormat(QVideoFrame::Format_IMC1).build()
                                          << RangeList();

    QTest::newRow("pixel format (1)") << builder.setPixelFormat(QVideoFrame::Format_BGR32).build()
                                            << (RangeList() << QCamera::FrameRateRange(5, 10));

    QTest::newRow("pixel format (2)") << builder.setPixelFormat(QVideoFrame::Format_NV12).build()
                                      << (RangeList() << QCamera::FrameRateRange(10, 10)
                                                      << QCamera::FrameRateRange(30, 30));

    QTest::newRow("invalid pixel aspect ratio") << builder.setPixelAspectRatio(2, 3).build()
                                                << RangeList();

    QTest::newRow("pixel aspect ratio (1)") << builder.setPixelAspectRatio(2, 1).build()
                                            << (RangeList() << QCamera::FrameRateRange(5, 10));

    QTest::newRow("pixel aspect ratio (2)") << builder.setPixelAspectRatio(1, 1).build()
                                            << (RangeList() << QCamera::FrameRateRange(10, 10)
                                                            << QCamera::FrameRateRange(30, 30));
}

void tst_QCamera::testSupportedViewfinderFrameRateRanges()
{
    QFETCH(QCameraViewfinderSettings, settings);
    QFETCH(QList<QCamera::FrameRateRange>, expectedFrameRateRanges);

    QList<QCamera::FrameRateRange> actualFrameRateRanges = QCamera().supportedViewfinderFrameRateRanges(settings);
    QCOMPARE(actualFrameRateRanges.size(), expectedFrameRateRanges.size());
    for (int i = 0; i < actualFrameRateRanges.size(); ++i)
        QCOMPARE(actualFrameRateRanges.at(i), expectedFrameRateRanges.at(i));
}

void tst_QCamera::testSupportedViewfinderPixelFormats_data()
{
    // see mockcameraviewfindersettingscontrol.h for expected values

    typedef QList<QVideoFrame::PixelFormat> FormatList;
    ViewfinderSettingsBuilder builder;

    QTest::addColumn<QCameraViewfinderSettings>("settings");
    QTest::addColumn<FormatList>("expectedPixelFormats");

    QTest::newRow("empty settings") << QCameraViewfinderSettings()
                                    << (FormatList() << QVideoFrame::Format_NV12
                                                     << QVideoFrame::Format_BGR32
                                                     << QVideoFrame::Format_YV12);

    QTest::newRow("invalid resolution") << builder.setResolution(452472, 444534).build()
                                        << FormatList();

    QTest::newRow("resolution (1)") << builder.setResolution(640, 480).build()
                                    << (FormatList() << QVideoFrame::Format_NV12);

    QTest::newRow("resolution (2)") << builder.setResolution(1280, 720).build()
                                    << (FormatList() << QVideoFrame::Format_NV12
                                                     << QVideoFrame::Format_YV12);

    QTest::newRow("invalid minimum frame rate") << builder.setMinimumFrameRate(2).build()
                                                << FormatList();

    QTest::newRow("minimum frame rate (1)") << builder.setMinimumFrameRate(5).build()
                                            << (FormatList() << QVideoFrame::Format_BGR32);

    QTest::newRow("minimum frame rate (2)") << builder.setMinimumFrameRate(10).build()
                                            << (FormatList() << QVideoFrame::Format_NV12
                                                             << QVideoFrame::Format_YV12);

    QTest::newRow("invalid maximum frame rate") << builder.setMaximumFrameRate(2).build()
                                                << FormatList();

    QTest::newRow("maximum frame rate (1)") << builder.setMaximumFrameRate(10).build()
                                            << (FormatList() << QVideoFrame::Format_NV12
                                                             << QVideoFrame::Format_BGR32
                                                             << QVideoFrame::Format_YV12);

    QTest::newRow("maximum frame rate (2)") << builder.setMinimumFrameRate(30).build()
                                            << (FormatList() << QVideoFrame::Format_NV12
                                                             << QVideoFrame::Format_YV12);

    QTest::newRow("invalid pixel aspect ratio") << builder.setPixelAspectRatio(2, 3).build()
                                                << FormatList();

    QTest::newRow("pixel aspect ratio (1)") << builder.setPixelAspectRatio(2, 1).build()
                                            << (FormatList() << QVideoFrame::Format_BGR32);

    QTest::newRow("pixel aspect ratio (2)") << builder.setPixelAspectRatio(1, 1).build()
                                            << (FormatList() << QVideoFrame::Format_NV12
                                                             << QVideoFrame::Format_YV12);
}

void tst_QCamera::testSupportedViewfinderPixelFormats()
{
    QFETCH(QCameraViewfinderSettings, settings);
    QFETCH(QList<QVideoFrame::PixelFormat>, expectedPixelFormats);

    QList<QVideoFrame::PixelFormat> actualPixelFormats = QCamera().supportedViewfinderPixelFormats(settings);
    QCOMPARE(actualPixelFormats.size(), expectedPixelFormats.size());
    for (int i = 0; i < actualPixelFormats.size(); ++i)
        QCOMPARE(actualPixelFormats.at(i), expectedPixelFormats.at(i));
}

void tst_QCamera::testCameraLock()
{
    QCamera camera;

    camera.focus()->setFocusMode(QCameraFocus::AutoFocus);

    QCOMPARE(camera.lockStatus(), QCamera::Unlocked);

    QSignalSpy lockedSignal(&camera, SIGNAL(locked()));
    QSignalSpy lockFailedSignal(&camera, SIGNAL(lockFailed()));
    QSignalSpy lockStatusChangedSignal(&camera, SIGNAL(lockStatusChanged(QCamera::LockStatus,QCamera::LockChangeReason)));

    camera.searchAndLock();
    QCOMPARE(camera.lockStatus(), QCamera::Searching);
    QCOMPARE(lockedSignal.count(), 0);
    QCOMPARE(lockFailedSignal.count(), 0);
    QCOMPARE(lockStatusChangedSignal.count(), 1);

    lockedSignal.clear();
    lockFailedSignal.clear();
    lockStatusChangedSignal.clear();

    QTRY_COMPARE(camera.lockStatus(), QCamera::Locked);
    QCOMPARE(lockedSignal.count(), 1);
    QCOMPARE(lockFailedSignal.count(), 0);
    QCOMPARE(lockStatusChangedSignal.count(), 1);

    lockedSignal.clear();
    lockFailedSignal.clear();
    lockStatusChangedSignal.clear();

    camera.searchAndLock();
    QCOMPARE(camera.lockStatus(), QCamera::Searching);
    QCOMPARE(lockedSignal.count(), 0);
    QCOMPARE(lockFailedSignal.count(), 0);
    QCOMPARE(lockStatusChangedSignal.count(), 1);

    lockedSignal.clear();
    lockFailedSignal.clear();
    lockStatusChangedSignal.clear();

    camera.unlock();
    QCOMPARE(camera.lockStatus(), QCamera::Unlocked);
    QCOMPARE(lockedSignal.count(), 0);
    QCOMPARE(lockFailedSignal.count(), 0);
    QCOMPARE(lockStatusChangedSignal.count(), 1);
}

void tst_QCamera::testCameraLockCancel()
{
    MockCameraService service;
    provider->service = &service;
    QCamera camera(0, provider);

    camera.focus()->setFocusMode(QCameraFocus::AutoFocus);

    QCOMPARE(camera.lockStatus(), QCamera::Unlocked);

    QSignalSpy lockedSignal(&camera, SIGNAL(locked()));
    QSignalSpy lockFailedSignal(&camera, SIGNAL(lockFailed()));
    QSignalSpy lockStatusChangedSignal(&camera, SIGNAL(lockStatusChanged(QCamera::LockStatus,QCamera::LockChangeReason)));
    camera.searchAndLock();
    QCOMPARE(camera.lockStatus(), QCamera::Searching);
    QCOMPARE(lockedSignal.count(), 0);
    QCOMPARE(lockFailedSignal.count(), 0);
    QCOMPARE(lockStatusChangedSignal.count(), 1);

    lockedSignal.clear();
    lockFailedSignal.clear();
    lockStatusChangedSignal.clear();

    camera.unlock();
    QCOMPARE(camera.lockStatus(), QCamera::Unlocked);
    QCOMPARE(lockedSignal.count(), 0);
    QCOMPARE(lockFailedSignal.count(), 0);
    QCOMPARE(lockStatusChangedSignal.count(), 1);
}

void tst_QCamera::testCameraEncodingProperyChange()
{
    QCamera camera;
    QCameraImageCapture imageCapture(&camera);

    QSignalSpy stateChangedSignal(&camera, SIGNAL(stateChanged(QCamera::State)));
    QSignalSpy statusChangedSignal(&camera, SIGNAL(statusChanged(QCamera::Status)));

    camera.start();
    QCOMPARE(camera.state(), QCamera::ActiveState);
    QCOMPARE(camera.status(), QCamera::ActiveStatus);

    QCOMPARE(stateChangedSignal.count(), 1);
    QCOMPARE(statusChangedSignal.count(), 1);
    stateChangedSignal.clear();
    statusChangedSignal.clear();


    camera.setCaptureMode(QCamera::CaptureVideo);
    QCOMPARE(camera.state(), QCamera::ActiveState);
    QCOMPARE(camera.status(), QCamera::LoadedStatus);

    QCOMPARE(stateChangedSignal.count(), 0);
    QCOMPARE(statusChangedSignal.count(), 1);
    stateChangedSignal.clear();
    statusChangedSignal.clear();

    QCOMPARE(camera.state(), QCamera::ActiveState);
    QTRY_COMPARE(camera.status(), QCamera::ActiveStatus);

    QCOMPARE(stateChangedSignal.count(), 0);
    QCOMPARE(statusChangedSignal.count(), 1);
    stateChangedSignal.clear();
    statusChangedSignal.clear();

    //backens should not be stopped since the capture mode is Video
    imageCapture.setEncodingSettings(QImageEncoderSettings());
    QCOMPARE(stateChangedSignal.count(), 0);
    QCOMPARE(statusChangedSignal.count(), 0);

    camera.setCaptureMode(QCamera::CaptureStillImage);

    QCOMPARE(camera.state(), QCamera::ActiveState);
    QTRY_COMPARE(camera.status(), QCamera::ActiveStatus);

    stateChangedSignal.clear();
    statusChangedSignal.clear();

    //the settings change should trigger camera stop/start
    imageCapture.setEncodingSettings(QImageEncoderSettings());
    QCOMPARE(camera.state(), QCamera::ActiveState);
    QCOMPARE(camera.status(), QCamera::LoadedStatus);

    QCOMPARE(stateChangedSignal.count(), 0);
    QCOMPARE(statusChangedSignal.count(), 1);
    stateChangedSignal.clear();
    statusChangedSignal.clear();

    QCOMPARE(camera.state(), QCamera::ActiveState);
    QTRY_COMPARE(camera.status(), QCamera::ActiveStatus);

    QCOMPARE(stateChangedSignal.count(), 0);
    QCOMPARE(statusChangedSignal.count(), 1);
    stateChangedSignal.clear();
    statusChangedSignal.clear();

    //the settings change should trigger camera stop/start only once
    camera.setCaptureMode(QCamera::CaptureVideo);
    camera.setCaptureMode(QCamera::CaptureStillImage);
    imageCapture.setEncodingSettings(QImageEncoderSettings());
    imageCapture.setEncodingSettings(QImageEncoderSettings());

    QCOMPARE(camera.state(), QCamera::ActiveState);
    QCOMPARE(camera.status(), QCamera::LoadedStatus);

    QCOMPARE(stateChangedSignal.count(), 0);
    QCOMPARE(statusChangedSignal.count(), 1);
    stateChangedSignal.clear();
    statusChangedSignal.clear();

    QCOMPARE(camera.state(), QCamera::ActiveState);
    QTRY_COMPARE(camera.status(), QCamera::ActiveStatus);

    QCOMPARE(stateChangedSignal.count(), 0);
    QCOMPARE(statusChangedSignal.count(), 1);
}

void tst_QCamera::testSetVideoOutput()
{
    MockVideoSurface surface;
    QCamera camera;

    camera.setViewfinder(reinterpret_cast<QVideoWidget *>(0));

    camera.setViewfinder(reinterpret_cast<QGraphicsVideoItem *>(0));

    QCOMPARE(mockCameraService->rendererRef, 0);

    camera.setViewfinder(&surface);
    QVERIFY(mockCameraService->rendererControl->surface() == &surface);
    QCOMPARE(mockCameraService->rendererRef, 1);

    camera.setViewfinder(reinterpret_cast<QAbstractVideoSurface *>(0));
    QVERIFY(mockCameraService->rendererControl->surface() == 0);

    //rendererControl is released
    QCOMPARE(mockCameraService->rendererRef, 0);

    camera.setViewfinder(&surface);
    QVERIFY(mockCameraService->rendererControl->surface() == &surface);
    QCOMPARE(mockCameraService->rendererRef, 1);

    camera.setViewfinder(reinterpret_cast<QVideoWidget *>(0));
    QVERIFY(mockCameraService->rendererControl->surface() == 0);
    //rendererControl is released
    QCOMPARE(mockCameraService->rendererRef, 0);

    camera.setViewfinder(&surface);
    QVERIFY(mockCameraService->rendererControl->surface() == &surface);
    QCOMPARE(mockCameraService->rendererRef, 1);
}


void tst_QCamera::testSetVideoOutputNoService()
{
    MockVideoSurface surface;

    provider->service = 0;
    QCamera camera;

    camera.setViewfinder(&surface);
    // Nothing we can verify here other than it doesn't assert.
}

void tst_QCamera::testSetVideoOutputNoControl()
{
    MockVideoSurface surface;

    MockCameraService service;
    service.rendererRef = 1;
    provider->service = &service;

    QCamera camera;

    camera.setViewfinder(&surface);
    QVERIFY(service.rendererControl->surface() == 0);
}

void tst_QCamera::testSetVideoOutputDestruction()
{
    MockVideoSurface surface;
    MockCameraService service;
    provider->service = &service;

    {
        QCamera camera;
        camera.setViewfinder(&surface);
        QVERIFY(service.rendererControl->surface() == &surface);
        QCOMPARE(service.rendererRef, 1);
    }
    QVERIFY(service.rendererControl->surface() == 0);
    QCOMPARE(service.rendererRef, 0);
}

void tst_QCamera::testEnumDebug()
{
    QTest::ignoreMessage(QtDebugMsg, "QCamera::ActiveState");
    qDebug() << QCamera::ActiveState;
    QTest::ignoreMessage(QtDebugMsg, "QCamera::ActiveStatus");
    qDebug() << QCamera::ActiveStatus;
    QTest::ignoreMessage(QtDebugMsg, "QCamera::CaptureVideo");
    qDebug() << QCamera::CaptureVideo;
    QTest::ignoreMessage(QtDebugMsg, "QCamera::CameraError");
    qDebug() << QCamera::CameraError;
    QTest::ignoreMessage(QtDebugMsg, "QCamera::Unlocked");
    qDebug() << QCamera::Unlocked;
    QTest::ignoreMessage(QtDebugMsg, "QCamera::LockAcquired");
    qDebug() << QCamera::LockAcquired;
    QTest::ignoreMessage(QtDebugMsg, "QCamera::NoLock");
    qDebug() << QCamera::NoLock;
    QTest::ignoreMessage(QtDebugMsg, "QCamera::LockExposure");
    qDebug() << QCamera::LockExposure;
    QTest::ignoreMessage(QtDebugMsg, "QCamera::FrontFace ");
    qDebug() << QCamera::FrontFace;
}

void tst_QCamera::testCameraControl()
{
    MockCameraControl *m_cameraControl=new MockCameraControl(this);
    QVERIFY(m_cameraControl != NULL);
}

void tst_QCamera::testConstructor()
{
    // Service doesn't implement QVideoDeviceSelectorControl
    provider->service = mockSimpleCameraService;

    {
        QCamera camera;
        QCOMPARE(camera.availability(), QMultimedia::Available);
        QCOMPARE(camera.error(), QCamera::NoError);
    }

    {
        // Requesting a camera at a specific position from a service which doesn't implement
        // the QVideoDeviceSelectorControl should result in loading the default camera
        QCamera camera(QCamera::FrontFace);
        QCOMPARE(camera.availability(), QMultimedia::Available);
        QCOMPARE(camera.error(), QCamera::NoError);
    }

    // Service implements QVideoDeviceSelectorControl
    provider->service = mockCameraService;

    {
        QCamera camera;
        QCOMPARE(camera.availability(), QMultimedia::Available);
        QCOMPARE(camera.error(), QCamera::NoError);
        QCOMPARE(mockCameraService->mockVideoDeviceSelectorControl->selectedDevice(), 1); // default is 1
    }

    {
        QCamera camera(QCameraInfo::defaultCamera());
        QCOMPARE(camera.availability(), QMultimedia::Available);
        QCOMPARE(camera.error(), QCamera::NoError);
        QCOMPARE(mockCameraService->mockVideoDeviceSelectorControl->selectedDevice(), 1);
        QCOMPARE(QCameraInfo(camera), QCameraInfo::defaultCamera());
    }

    {
        QCameraInfo cameraInfo = QCameraInfo::availableCameras().at(0);
        QCamera camera(cameraInfo);
        QCOMPARE(camera.availability(), QMultimedia::Available);
        QCOMPARE(camera.error(), QCamera::NoError);
        QCOMPARE(mockCameraService->mockVideoDeviceSelectorControl->selectedDevice(), 0);
        QCOMPARE(QCameraInfo(camera), cameraInfo);
    }

    {
        // Requesting a camera at a position which is not available should result in
        // loading the default camera
        QCamera camera(QCamera::FrontFace);
        QCOMPARE(camera.availability(), QMultimedia::Available);
        QCOMPARE(camera.error(), QCamera::NoError);
        QCOMPARE(mockCameraService->mockVideoDeviceSelectorControl->selectedDevice(), 1);
    }

    {
        QCamera camera(QCamera::BackFace);
        QCOMPARE(camera.availability(), QMultimedia::Available);
        QCOMPARE(camera.error(), QCamera::NoError);
        QCOMPARE(mockCameraService->mockVideoDeviceSelectorControl->selectedDevice(), 0);
    }

    {
        // Should load the default camera when UnspecifiedPosition is requested
        QCamera camera(QCamera::UnspecifiedPosition);
        QCOMPARE(camera.availability(), QMultimedia::Available);
        QCOMPARE(camera.error(), QCamera::NoError);
        QCOMPARE(mockCameraService->mockVideoDeviceSelectorControl->selectedDevice(), 1);
    }
}

/* captureModeChanged Signal test case. */
void tst_QCamera::testCaptureModeChanged_signal()
{
    MockCameraControl *m_cameraControl= new MockCameraControl(this);
    QSignalSpy spy(m_cameraControl, SIGNAL(captureModeChanged(QCamera::CaptureModes)));
    QVERIFY(spy.size() == 0);

    m_cameraControl->setCaptureMode(QCamera::CaptureVideo);
    QVERIFY(spy.size() == 1);

    m_cameraControl->setCaptureMode(QCamera::CaptureStillImage);
    QVERIFY(spy.size() == 2);
}

/* Test case for captureMode */
void tst_QCamera::testCaptureMode()
{
    QCamera camera;
    QVERIFY(camera.captureMode() == QCamera::CaptureStillImage);

    camera.setCaptureMode(QCamera::CaptureVideo);
    QVERIFY(camera.captureMode() == QCamera::CaptureVideo);
}

/* Test case for isCaptureModeSupported */
void tst_QCamera::testIsCaptureModeSupported()
{
    QCamera camera;
    QVERIFY(camera.isCaptureModeSupported(QCamera::CaptureStillImage) == true);
    QVERIFY(camera.isCaptureModeSupported(QCamera::CaptureVideo) == true);
}

/* Test case for requestedLocks. LockType is stored in OR combination so all
   types of combinations are verified here.*/
void tst_QCamera::testRequestedLocks()
{
    QCamera camera;

    QCOMPARE(camera.requestedLocks(),QCamera::NoLock);

    camera.searchAndLock(QCamera::LockExposure);
    QCOMPARE(camera.requestedLocks(),QCamera::LockExposure);

    camera.unlock();
    camera.searchAndLock(QCamera::LockFocus);
    QCOMPARE(camera.requestedLocks(),QCamera::LockFocus );

    camera.unlock();
    camera.searchAndLock(QCamera::LockWhiteBalance);
    QCOMPARE(camera.requestedLocks(),QCamera::NoLock);

    camera.unlock();
    camera.searchAndLock(QCamera::LockExposure |QCamera::LockFocus );
    QCOMPARE(camera.requestedLocks(),QCamera::LockExposure |QCamera::LockFocus );
    camera.searchAndLock(QCamera::LockWhiteBalance);
    QCOMPARE(camera.requestedLocks(),QCamera::LockExposure |QCamera::LockFocus);
    camera.unlock(QCamera::LockExposure);
    QCOMPARE(camera.requestedLocks(),QCamera::LockFocus);
    camera.unlock(QCamera::LockFocus);
    camera.searchAndLock(QCamera::LockExposure |QCamera::LockWhiteBalance );
    QCOMPARE(camera.requestedLocks(),QCamera::LockExposure);
}

/* Test case for supportedLocks() */
void tst_QCamera::testSupportedLocks()
{
    QCamera camera;

    QCOMPARE(camera.supportedLocks(),QCamera::LockExposure | QCamera::LockFocus);
}

/* Test case for isAvailable */
void tst_QCamera::testQCameraIsAvailable()
{
    QCamera camera;
    QVERIFY(camera.isAvailable());
    QVERIFY(camera.availability() == QMultimedia::Available);
}

void tst_QCamera::testQCameraIsNotAvailable()
{
    provider->service = 0;
    QCamera camera("random");

    QCOMPARE(camera.error(), QCamera::ServiceMissingError);
    QVERIFY(!camera.isAvailable());
    QCOMPARE(camera.availability(), QMultimedia::ServiceMissing);
}

/* Test case for searchAndLock ( QCamera::LockTypes locks ) */
void tst_QCamera::testSearchAndLockWithLockTypes()
{
    QCamera camera;

    QCOMPARE(camera.lockStatus(), QCamera::Unlocked);

    /* Spy the signals */
    QSignalSpy lockedSignal(&camera, SIGNAL(locked()));
    QSignalSpy lockFailedSignal(&camera, SIGNAL(lockFailed()));
    QSignalSpy lockStatusChangedSignal(&camera, SIGNAL(lockStatusChanged(QCamera::LockStatus,QCamera::LockChangeReason)));
    QSignalSpy lockStatusChangedSignalWithType(&camera, SIGNAL(lockStatusChanged(QCamera::LockType,QCamera::LockStatus,QCamera::LockChangeReason)));

    /* search and lock the camera with QCamera::LockExposure and verify if the signal is emitted correctly */
    camera.searchAndLock(QCamera::LockExposure);
    QCOMPARE(camera.lockStatus(), QCamera::Locked);
    QCOMPARE(lockedSignal.count(), 1);
    QCOMPARE(lockFailedSignal.count(), 0);
    QCOMPARE(lockStatusChangedSignal.count(), 1);
    QCOMPARE(lockStatusChangedSignalWithType.count(), 1);
}

/* Test case for setCaptureMode() */
void tst_QCamera::testSetCaptureMode()
{
    QCamera camera;

    /* Set the capture mode and verify if it set correctly */
    camera.setCaptureMode(QCamera::CaptureVideo);
    QVERIFY(camera.captureMode() == QCamera::CaptureVideo);

    camera.setCaptureMode(QCamera::CaptureStillImage);
    QVERIFY(camera.captureMode() == QCamera::CaptureStillImage);
}

/* Test case for unlock (QCamera::LockTypes) */
void tst_QCamera::testUnlockWithType()
{
    QCamera camera;

    QCOMPARE(camera.lockStatus(), QCamera::Unlocked);

    /* Spy the signal */
    QSignalSpy lockedSignal(&camera, SIGNAL(locked()));
    QSignalSpy lockFailedSignal(&camera, SIGNAL(lockFailed()));
    QSignalSpy lockStatusChangedSignal(&camera, SIGNAL(lockStatusChanged(QCamera::LockStatus,QCamera::LockChangeReason)));
    QSignalSpy lockStatusChangedSignalWithType(&camera, SIGNAL(lockStatusChanged(QCamera::LockType,QCamera::LockStatus,QCamera::LockChangeReason)));

    /* lock the camera with QCamera::LockExposure and Verify if the signal is emitted correctly */
    camera.searchAndLock(QCamera::LockExposure);
    QCOMPARE(camera.lockStatus(), QCamera::Locked);
    QCOMPARE(lockedSignal.count(), 1);
    QCOMPARE(lockFailedSignal.count(), 0);
    QCOMPARE(lockStatusChangedSignal.count(), 1);
    QCOMPARE(lockStatusChangedSignalWithType.count(), 1);

    /* Clear the signal */
    lockedSignal.clear();
    lockFailedSignal.clear();
    lockStatusChangedSignal.clear();
    lockStatusChangedSignalWithType.clear();

    /* Unlock the camera and verify if the signal is emitted correctly */
    camera.unlock(QCamera::LockExposure);
    QCOMPARE(camera.lockStatus(), QCamera::Unlocked);
    QCOMPARE(lockedSignal.count(), 0);
    QCOMPARE(lockFailedSignal.count(), 0);
    QCOMPARE(lockStatusChangedSignal.count(), 1);
    QCOMPARE(lockStatusChangedSignalWithType.count(), 1);
    QCamera::LockType lockType = qvariant_cast<QCamera::LockType >(lockStatusChangedSignalWithType.at(0).at(0));
    QCamera::LockStatus lockStatus = qvariant_cast<QCamera::LockStatus >(lockStatusChangedSignalWithType.at(0).at(1));
    QVERIFY(lockType == QCamera::LockExposure);
    QVERIFY(lockStatus == QCamera::Unlocked);

    lockedSignal.clear();
    lockFailedSignal.clear();
    lockStatusChangedSignal.clear();
    lockStatusChangedSignalWithType.clear();

    /* Lock the camera with QCamera::LockFocus */
    camera.searchAndLock(QCamera::LockFocus);
    lockedSignal.clear();
    lockFailedSignal.clear();
    lockStatusChangedSignal.clear();
    lockStatusChangedSignalWithType.clear();

    /* Unlock the camera and Verify if the signal is emitted correctly */
    camera.unlock(QCamera::LockFocus);
    QCOMPARE(camera.lockStatus(), QCamera::Unlocked);
    QCOMPARE(lockedSignal.count(), 0);
    QCOMPARE(lockFailedSignal.count(), 0);
    QCOMPARE(lockStatusChangedSignal.count(), 1);
    QCOMPARE(lockStatusChangedSignalWithType.count(), 1);
    lockType = qvariant_cast<QCamera::LockType >(lockStatusChangedSignalWithType.at(0).at(0));
    lockStatus = qvariant_cast<QCamera::LockStatus >(lockStatusChangedSignalWithType.at(0).at(1));
    QVERIFY(lockType == QCamera::LockFocus);
    QVERIFY(lockStatus == QCamera::Unlocked);
}

/* Test case for signal captureModeChanged(QCamera::CaptureModes) */
void tst_QCamera::testCaptureModeChangedSignal()
{
    QCamera camera;
    QVERIFY(camera.captureMode() == QCamera::CaptureStillImage);

    qRegisterMetaType<QCamera::CaptureModes>("QCamera::CaptureModes");

    /* Spy the signal */
    QSignalSpy lockCaptureModeChangedSignal(&camera, SIGNAL(captureModeChanged(QCamera::CaptureModes)));

    /* set the capture mode and Verify if the signal is emitted */
    camera.setCaptureMode(QCamera::CaptureVideo);
    QVERIFY(camera.captureMode() == QCamera::CaptureVideo);
    QCOMPARE(lockCaptureModeChangedSignal.count(), 1);
    QCamera::CaptureModes lockCaptureMode = qvariant_cast<QCamera::CaptureModes >(lockCaptureModeChangedSignal.at(0).at(0));
    QVERIFY(lockCaptureMode == QCamera::CaptureVideo);
}

/* Test case for signal lockStatusChanged(QCamera::LockType,QCamera::LockStatus, QCamera::LockChangeReason) */
void tst_QCamera::testLockStatusChangedWithTypesSignal()
{
    QCamera camera;

    QCOMPARE(camera.lockStatus(), QCamera::Unlocked);

    /* Spy the signal lockStatusChanged(QCamera::LockType,QCamera::LockStatus, QCamera::LockChangeReason) */
    QSignalSpy lockStatusChangedSignalWithType(&camera, SIGNAL(lockStatusChanged(QCamera::LockType,QCamera::LockStatus,QCamera::LockChangeReason)));

    /* Lock the camera with type QCamera::LockExposure */
    camera.searchAndLock(QCamera::LockExposure);

    /* Verify if the signal is emitted and lock status is set correclty */
    QCOMPARE(camera.lockStatus(), QCamera::Locked);
    QCOMPARE(lockStatusChangedSignalWithType.count(), 1);
    QCamera::LockType lockType = qvariant_cast<QCamera::LockType >(lockStatusChangedSignalWithType.at(0).at(0));
    QCamera::LockStatus lockStatus = qvariant_cast<QCamera::LockStatus >(lockStatusChangedSignalWithType.at(0).at(1));
    QVERIFY(lockType == QCamera::LockExposure);
    QVERIFY(lockStatus == QCamera::Locked);

    lockStatusChangedSignalWithType.clear();

    /* Unlock the camera */
    camera.unlock();

    /* Verify if the signal is emitted and lock status is set correclty */
    QCOMPARE(camera.lockStatus(), QCamera::Unlocked);
    QCOMPARE(lockStatusChangedSignalWithType.count(), 1);
    lockType = qvariant_cast<QCamera::LockType >(lockStatusChangedSignalWithType.at(0).at(0));
    lockStatus = qvariant_cast<QCamera::LockStatus >(lockStatusChangedSignalWithType.at(0).at(1));
    QVERIFY(lockType == QCamera::LockExposure);
    QVERIFY(lockStatus == QCamera::Unlocked);
}

/* Test case for verifying if error signal generated correctly */
void tst_QCamera::testErrorSignal()
{
    MockCameraService service;
    provider->service = &service;

    QCamera camera;

    QSignalSpy spyError(&camera, SIGNAL(error(QCamera::Error)));

    /* Set the QCameraControl error and verify if the signal is emitted correctly in QCamera */
    service.mockControl->setError(QCamera::CameraError,QString("Camera Error"));

    QVERIFY(spyError.count() == 1);
    QCamera::Error err = qvariant_cast<QCamera::Error >(spyError.at(0).at(0));
    QVERIFY(err == QCamera::CameraError);

    spyError.clear();

    /* Set the QCameraControl error and verify if the signal is emitted correctly in QCamera */
    service.mockControl->setError(QCamera::InvalidRequestError,QString("InvalidRequestError Error"));
    QVERIFY(spyError.count() == 1);
    err = qvariant_cast<QCamera::Error >(spyError.at(0).at(0));
    QVERIFY(err == QCamera::InvalidRequestError);

    spyError.clear();

    /* Set the QCameraControl error and verify if the signal is emitted correctly in QCamera */
    service.mockControl->setError(QCamera::NotSupportedFeatureError,QString("NotSupportedFeatureError Error"));
    QVERIFY(spyError.count() == 1);
    err = qvariant_cast<QCamera::Error >(spyError.at(0).at(0));
    QVERIFY(err == QCamera::NotSupportedFeatureError);

}

/* Test case for verifying the QCamera error */
void tst_QCamera::testError()
{
    MockCameraService service;
    provider->service = &service;

    QCamera camera;

    /* Set the QCameraControl error and verify if it is set correctly in QCamera */
    service.mockControl->setError(QCamera::CameraError,QString("Camera Error"));
    QVERIFY(camera.error() == QCamera::CameraError);

    /* Set the QCameraControl error and verify if it is set correctly in QCamera */
    service.mockControl->setError(QCamera::InvalidRequestError,QString("InvalidRequestError Error"));
    QVERIFY(camera.error() == QCamera::InvalidRequestError);

    /* Set the QCameraControl error and verify if it is set correctly in QCamera */
    service.mockControl->setError(QCamera::NotSupportedFeatureError,QString("NotSupportedFeatureError Error"));
    QVERIFY(camera.error() == QCamera::NotSupportedFeatureError);

}

/* Test the error strings for QCamera class */
void tst_QCamera::testErrorString()
{
    MockCameraService service;
    provider->service = &service;

    QCamera camera;

    /* Set the QCameraControl error and verify if it is set correctly in QCamera */
    service.mockControl->setError(QCamera::CameraError,QString("Camera Error"));
    QVERIFY(camera.errorString() == QString("Camera Error"));

    /* Set the QCameraControl error and verify if it is set correctly in QCamera */
    service.mockControl->setError(QCamera::InvalidRequestError,QString("InvalidRequestError Error"));
    QVERIFY(camera.errorString() == QString("InvalidRequestError Error"));

    /* Set the QCameraControl error and verify if it is set correctly in QCamera */
    service.mockControl->setError(QCamera::NotSupportedFeatureError,QString("NotSupportedFeatureError Error"));
    QVERIFY(camera.errorString() == QString("NotSupportedFeatureError Error"));
}

/* Test case for verifying Status of QCamera. */
void tst_QCamera::testStatus()
{
    MockCameraService service;
    provider->service = &service;

    QCamera camera;

    /* Set the QCameraControl status and verify if it is set correctly in QCamera */
    service.mockControl->setStatus(QCamera::StartingStatus);
    QVERIFY(camera.status() == QCamera::StartingStatus);

    /* Set the QCameraControl status and verify if it is set correctly in QCamera */
    service.mockControl->setStatus(QCamera::StandbyStatus);
    QVERIFY(camera.status() == QCamera::StandbyStatus);

    /* Set the QCameraControl status and verify if it is set correctly in QCamera */
    service.mockControl->setStatus(QCamera::LoadingStatus);
    QVERIFY(camera.status() == QCamera::LoadingStatus);

    /* Set the QCameraControl status and verify if it is set correctly in QCamera */
    service.mockControl->setStatus(QCamera::UnavailableStatus);
    QVERIFY(camera.status() == QCamera::UnavailableStatus);
}

/* Test case for verifying default locktype QCamera::NoLock */
void tst_QCamera::testLockType()
{
    QCamera camera;

    QCOMPARE(camera.requestedLocks(),QCamera::NoLock);
}

/* Test case for QCamera::LockChangeReason with QCamera::LockAcquired */
void tst_QCamera::testLockChangeReason()
{
    MockCameraService service;
    provider->service = &service;

    QCamera camera;

    QSignalSpy lockStatusChangedSignalWithType(&camera, SIGNAL(lockStatusChanged(QCamera::LockType,QCamera::LockStatus,QCamera::LockChangeReason)));

    /* Set the lockChangeReason */
    service.mockLocksControl->setLockChangeReason(QCamera::LockAcquired);

    /* Verify if lockChangeReson is eqaul toQCamera::LockAcquired */
    QCOMPARE(lockStatusChangedSignalWithType.count(), 1);
    QCamera::LockChangeReason LockChangeReason = qvariant_cast<QCamera::LockChangeReason >(lockStatusChangedSignalWithType.at(0).at(2));
    QVERIFY(LockChangeReason == QCamera::LockAcquired);

}

/* All the enums test case for QCameraControl class*/
void tst_QCamera::testEnumsOfQCameraControl()
{
    MockCameraControl *m_cameraControl = new MockCameraControl(this);
    bool result;

    // In still mode, can't change much
    QVERIFY(m_cameraControl->captureMode() == QCamera::CaptureStillImage);
    result = m_cameraControl->canChangeProperty(MockCameraControl::CaptureMode, QCamera::ActiveStatus);
    QVERIFY(!result);
    result = m_cameraControl->canChangeProperty(MockCameraControl::ImageEncodingSettings, QCamera::ActiveStatus);
    QVERIFY(!result);
    result = m_cameraControl->canChangeProperty(MockCameraControl::VideoEncodingSettings, QCamera::ActiveStatus);
    QVERIFY(result);
    result = m_cameraControl->canChangeProperty(MockCameraControl::Viewfinder, QCamera::ActiveStatus);
    QVERIFY(!result);

    // In video mode can change image encoding settings
    m_cameraControl->setCaptureMode(QCamera::CaptureVideo);
    result = m_cameraControl->canChangeProperty(MockCameraControl::ImageEncodingSettings, QCamera::ActiveStatus);
    QVERIFY(result);
    result = m_cameraControl->canChangeProperty(MockCameraControl::VideoEncodingSettings, QCamera::ActiveStatus);
    QVERIFY(result);
    result = m_cameraControl->canChangeProperty(MockCameraControl::Viewfinder, QCamera::ActiveStatus);
    QVERIFY(!result);

    // Flip the allow everything bit
    m_cameraControl->m_propertyChangesSupported = true;
    result = m_cameraControl->canChangeProperty(MockCameraControl::CaptureMode, QCamera::ActiveStatus);
    QVERIFY(result);
    result = m_cameraControl->canChangeProperty(MockCameraControl::ImageEncodingSettings, QCamera::ActiveStatus);
    QVERIFY(result);
    result = m_cameraControl->canChangeProperty(MockCameraControl::VideoEncodingSettings, QCamera::ActiveStatus);
    QVERIFY(result);
    result = m_cameraControl->canChangeProperty(MockCameraControl::Viewfinder, QCamera::ActiveStatus);
    QVERIFY(result);
}

// Test case for QCameraImageProcessing class
void tst_QCamera::testContrast()
{
    QCamera camera;
    QCameraImageProcessing *cameraImageProcessing = camera.imageProcessing();
    QVERIFY(cameraImageProcessing->contrast() ==0);

    cameraImageProcessing->setContrast(0.123);
    QCOMPARE(cameraImageProcessing->contrast(), 0.123);

    cameraImageProcessing->setContrast(4.56);
    QCOMPARE(cameraImageProcessing->contrast(), 4.56);
}

void tst_QCamera::testDenoisingLevel()
{
    QCamera camera;
    QCameraImageProcessing *cameraImageProcessing = camera.imageProcessing();

    QCOMPARE(cameraImageProcessing->denoisingLevel()+1 , 1.0);

    cameraImageProcessing->setDenoisingLevel(-0.3);
    QCOMPARE(cameraImageProcessing->denoisingLevel() , -0.3);

    cameraImageProcessing->setDenoisingLevel(0.3);
    QCOMPARE(cameraImageProcessing->denoisingLevel() , 0.3);
}

void tst_QCamera::testIsAvailable()
{
    QCamera camera;
    QCameraImageProcessing *cameraImageProcessing = camera.imageProcessing();
    QVERIFY(cameraImageProcessing->isAvailable() == true);
}

void tst_QCamera::testSaturation()
{
    QCamera camera;
    QCameraImageProcessing *cameraImageProcessing = camera.imageProcessing();
    QCOMPARE(cameraImageProcessing->saturation()+1.0, 1.0);

    cameraImageProcessing->setSaturation(0.5);
    QCOMPARE(cameraImageProcessing->saturation(), 0.5);

    cameraImageProcessing->setSaturation(-0.5);
    QCOMPARE(cameraImageProcessing->saturation(), -0.5);
}

void tst_QCamera::testSharpeningLevel()
{
    QCamera camera;

    QCameraImageProcessing *cameraImageProcessing = camera.imageProcessing();

    QCOMPARE(cameraImageProcessing->sharpeningLevel()+1 , 1.0);

    cameraImageProcessing->setSharpeningLevel(-0.3);
    QCOMPARE(cameraImageProcessing->sharpeningLevel() , -0.3);

    cameraImageProcessing->setSharpeningLevel(0.3);
    QCOMPARE(cameraImageProcessing->sharpeningLevel() , 0.3);
}

void tst_QCamera::testEnumOfQCameraImageProcessing()
{
    QSet<QCameraImageProcessing::WhiteBalanceMode> whiteBalanceModes;
    whiteBalanceModes << QCameraImageProcessing::WhiteBalanceManual;
    whiteBalanceModes << QCameraImageProcessing::WhiteBalanceAuto;
    whiteBalanceModes << QCameraImageProcessing::WhiteBalanceSunlight;
    whiteBalanceModes << QCameraImageProcessing::WhiteBalanceCloudy;
    whiteBalanceModes << QCameraImageProcessing::WhiteBalanceShade;
    whiteBalanceModes << QCameraImageProcessing::WhiteBalanceTungsten;
    whiteBalanceModes << QCameraImageProcessing::WhiteBalanceFluorescent;
    whiteBalanceModes << QCameraImageProcessing::WhiteBalanceFlash;
    whiteBalanceModes << QCameraImageProcessing::WhiteBalanceSunset;
    whiteBalanceModes << QCameraImageProcessing::WhiteBalanceVendor;

    MockCameraService service;
    service.mockImageProcessingControl->setSupportedWhiteBalanceModes(whiteBalanceModes);

    service.mockImageProcessingControl->setWhiteBalanceMode(QCameraImageProcessing::WhiteBalanceManual);
    QVERIFY(service.mockImageProcessingControl->isWhiteBalanceModeSupported(QCameraImageProcessing::WhiteBalanceManual));
    QVERIFY(service.mockImageProcessingControl->whiteBalanceMode() == QCameraImageProcessing::WhiteBalanceManual);

    service.mockImageProcessingControl->setWhiteBalanceMode(QCameraImageProcessing::WhiteBalanceAuto);
    QVERIFY(service.mockImageProcessingControl->isWhiteBalanceModeSupported(QCameraImageProcessing::WhiteBalanceAuto));
    QVERIFY(service.mockImageProcessingControl->whiteBalanceMode() == QCameraImageProcessing::WhiteBalanceAuto);

    service.mockImageProcessingControl->setWhiteBalanceMode(QCameraImageProcessing::WhiteBalanceSunlight);
    QVERIFY(service.mockImageProcessingControl->isWhiteBalanceModeSupported(QCameraImageProcessing::WhiteBalanceSunlight));
    QVERIFY(service.mockImageProcessingControl->whiteBalanceMode() == QCameraImageProcessing::WhiteBalanceSunlight);

    service.mockImageProcessingControl->setWhiteBalanceMode(QCameraImageProcessing::WhiteBalanceCloudy);
    QVERIFY(service.mockImageProcessingControl->isWhiteBalanceModeSupported(QCameraImageProcessing::WhiteBalanceCloudy));
    QVERIFY(service.mockImageProcessingControl->whiteBalanceMode() == QCameraImageProcessing::WhiteBalanceCloudy);

    service.mockImageProcessingControl->setWhiteBalanceMode(QCameraImageProcessing::WhiteBalanceShade);
    QVERIFY(service.mockImageProcessingControl->isWhiteBalanceModeSupported(QCameraImageProcessing::WhiteBalanceShade));
    QVERIFY(service.mockImageProcessingControl->whiteBalanceMode() == QCameraImageProcessing::WhiteBalanceShade);

    service.mockImageProcessingControl->setWhiteBalanceMode(QCameraImageProcessing::WhiteBalanceTungsten);
    QVERIFY(service.mockImageProcessingControl->isWhiteBalanceModeSupported(QCameraImageProcessing::WhiteBalanceTungsten));
    QVERIFY(service.mockImageProcessingControl->whiteBalanceMode() == QCameraImageProcessing::WhiteBalanceTungsten);

    service.mockImageProcessingControl->setWhiteBalanceMode(QCameraImageProcessing::WhiteBalanceFluorescent);
    QVERIFY(service.mockImageProcessingControl->isWhiteBalanceModeSupported(QCameraImageProcessing::WhiteBalanceFluorescent));
    QVERIFY(service.mockImageProcessingControl->whiteBalanceMode() == QCameraImageProcessing::WhiteBalanceFluorescent);

    service.mockImageProcessingControl->setWhiteBalanceMode(QCameraImageProcessing::WhiteBalanceFlash);
    QVERIFY(service.mockImageProcessingControl->isWhiteBalanceModeSupported(QCameraImageProcessing::WhiteBalanceFlash));
    QVERIFY(service.mockImageProcessingControl->whiteBalanceMode() == QCameraImageProcessing::WhiteBalanceFlash);

    service.mockImageProcessingControl->setWhiteBalanceMode(QCameraImageProcessing::WhiteBalanceSunset);
    QVERIFY(service.mockImageProcessingControl->isWhiteBalanceModeSupported(QCameraImageProcessing::WhiteBalanceSunset));
    QVERIFY(service.mockImageProcessingControl->whiteBalanceMode() == QCameraImageProcessing::WhiteBalanceSunset);

    service.mockImageProcessingControl->setWhiteBalanceMode(QCameraImageProcessing::WhiteBalanceVendor);
    QVERIFY(service.mockImageProcessingControl->isWhiteBalanceModeSupported(QCameraImageProcessing::WhiteBalanceVendor));
    QVERIFY(service.mockImageProcessingControl->whiteBalanceMode() == QCameraImageProcessing::WhiteBalanceVendor);
}

//Added test cases for QCameraFocus
void tst_QCamera::testCameraFocusIsAvailable()
{
    QCamera camera;

    QCameraFocus *cameraFocus = camera.focus();
    QVERIFY(cameraFocus != 0);
    QVERIFY(cameraFocus->isAvailable());
}

//Added this code to cover QCameraFocus::HyperfocalFocus and QCameraFocus::MacroFocus
//As the HyperfocalFocus and MacroFocus are not supported we can not set the focus mode to these Focus Modes
void tst_QCamera::testFocusModes()
{
    QCamera camera;

    QCameraFocus *cameraFocus = camera.focus();
    QVERIFY(cameraFocus != 0);
    QVERIFY(!cameraFocus->isFocusModeSupported(QCameraFocus::HyperfocalFocus));
    QVERIFY(!cameraFocus->isFocusModeSupported(QCameraFocus::MacroFocus));
    QCOMPARE(cameraFocus->focusMode(), QCameraFocus::AutoFocus);
    cameraFocus->setFocusMode(QCameraFocus::HyperfocalFocus);
    QVERIFY(cameraFocus->focusMode()!= QCameraFocus::HyperfocalFocus);
    QCOMPARE(cameraFocus->focusMode(), QCameraFocus::AutoFocus);
    cameraFocus->setFocusMode(QCameraFocus::MacroFocus);
    QVERIFY(cameraFocus->focusMode()!= QCameraFocus::MacroFocus);
    QCOMPARE(cameraFocus->focusMode(), QCameraFocus::AutoFocus);
}

void tst_QCamera::testOpticalAndDigitalZoomChanged()
{
    QCamera camera;

    QCameraFocus *cameraFocus = camera.focus();
    QVERIFY(cameraFocus != 0);
    QSignalSpy spy1(cameraFocus,SIGNAL(digitalZoomChanged(qreal)));
    QSignalSpy spy2(cameraFocus,SIGNAL(opticalZoomChanged(qreal)));
    QVERIFY(spy1.count() == 0);
    QVERIFY(spy2.count() == 0);
    cameraFocus->zoomTo(2.0,3.0);
    QVERIFY(spy1.count() == 1);
    QVERIFY(spy2.count() == 1);
}

void tst_QCamera::testMaxDigitalZoomChangedSignal()
{
    QCamera camera;

    QCameraFocus *cameraFocus = camera.focus();
    QVERIFY(cameraFocus != 0);
    QSignalSpy spy(cameraFocus,SIGNAL(maximumDigitalZoomChanged(qreal)));
    QVERIFY(spy.count() == 0);
    cameraFocus->zoomTo(5.0,6.0);
    QVERIFY(spy.count() == 1);
}

void tst_QCamera::testMaxOpticalZoomChangedSignal()
{
    QCamera camera;

    QCameraFocus *cameraFocus = camera.focus();
    QVERIFY(cameraFocus != 0);
    QSignalSpy spy(cameraFocus,SIGNAL(maximumOpticalZoomChanged(qreal)));
    QVERIFY(spy.count() == 0);
    cameraFocus->zoomTo(5.0,6.0);
    QVERIFY(spy.count() == 1);
}

void tst_QCamera::testfocusZonesChangedSignal()
{
    QCamera camera;

    QCameraFocus *cameraFocus = camera.focus();
    QVERIFY(cameraFocus != 0);

    QSignalSpy spy(cameraFocus,SIGNAL(focusZonesChanged()));
    cameraFocus->setCustomFocusPoint(QPointF(0.1, 0.1));
    QVERIFY(spy.count() == 1);
}

// test constructor for abstract class of ImageProcessingControl
void tst_QCamera :: testImageProcessingControl()
{
    QObject parent;
    MockImageProcessingControl processCtrl(&parent);
}

void tst_QCamera::testSignalApertureChanged()
{
    QCamera camera;

    QCameraExposure *cameraExposure = camera.exposure(); //create camera expose instance
    QVERIFY(cameraExposure != 0);

    QSignalSpy spyApertureChanged(cameraExposure , SIGNAL(apertureChanged(qreal)));
    QSignalSpy spyApertureRangeChanged(cameraExposure , SIGNAL(apertureRangeChanged()));


    QVERIFY(spyApertureChanged.count() ==0);
    cameraExposure->setManualAperture(10.0);//set the ManualAperture to 10.0

    QTest::qWait(100);
    QVERIFY(spyApertureChanged.count() ==1);
    QVERIFY(spyApertureRangeChanged.count() ==1);
}

void tst_QCamera::testSignalExposureCompensationChanged()
{
    QCamera camera;

    QCameraExposure *cameraExposure = camera.exposure(); //create camera expose instance
    QVERIFY(cameraExposure != 0);

    QSignalSpy spyExposureCompensationChanged(cameraExposure , SIGNAL(exposureCompensationChanged(qreal)));

    QVERIFY(spyExposureCompensationChanged.count() ==0);

    QVERIFY(cameraExposure->exposureCompensation() != 800);
    cameraExposure->setExposureCompensation(2.0);

    QTest::qWait(100);

    QVERIFY(cameraExposure->exposureCompensation() == 2.0);

    QCOMPARE(spyExposureCompensationChanged.count(),1);

    // Setting the same should not result in a signal
    cameraExposure->setExposureCompensation(2.0);
    QTest::qWait(100);

    QVERIFY(cameraExposure->exposureCompensation() == 2.0);
    QCOMPARE(spyExposureCompensationChanged.count(),1);
}

void tst_QCamera::testSignalIsoSensitivityChanged()
{
    QCamera camera;

    QCameraExposure *cameraExposure = camera.exposure(); //create camera expose instance
    QVERIFY(cameraExposure != 0);

    QSignalSpy spyisoSensitivityChanged(cameraExposure , SIGNAL(isoSensitivityChanged(int)));

    QVERIFY(spyisoSensitivityChanged.count() ==0);

    cameraExposure->setManualIsoSensitivity(800); //set the manualiso sentivity to 800
    QTest::qWait(100);
    QVERIFY(spyisoSensitivityChanged.count() ==1);

}
void tst_QCamera::testSignalShutterSpeedChanged()
{
    QCamera camera;

    QCameraExposure *cameraExposure = camera.exposure(); //create camera expose instance
    QVERIFY(cameraExposure != 0);

    QSignalSpy spySignalShutterSpeedChanged(cameraExposure , SIGNAL(shutterSpeedChanged(qreal)));
    QSignalSpy spySignalShutterSpeedRangeChanged(cameraExposure , SIGNAL(shutterSpeedRangeChanged()));

    QVERIFY(spySignalShutterSpeedChanged.count() ==0);

    cameraExposure->setManualShutterSpeed(2.0);//set the ManualShutterSpeed to 2.0
    QTest::qWait(100);

    QVERIFY(spySignalShutterSpeedChanged.count() ==1);
    QVERIFY(spySignalShutterSpeedRangeChanged.count() ==1);
}

void tst_QCamera::testSignalFlashReady()
{
    QCamera camera;

    QCameraExposure *cameraExposure = camera.exposure(); //create camera expose instance
    QVERIFY(cameraExposure != 0);


    QSignalSpy spyflashReady(cameraExposure,SIGNAL(flashReady(bool)));

    QVERIFY(spyflashReady.count() ==0);

    QVERIFY(cameraExposure->flashMode() ==QCameraExposure::FlashAuto);

    cameraExposure->setFlashMode(QCameraExposure::FlashOff);//set theFlashMode to QCameraExposure::FlashOff

    QVERIFY(cameraExposure->flashMode() ==QCameraExposure::FlashOff);

    QVERIFY(spyflashReady.count() ==1);
}

// test constructor
void tst_QCamera::testExposureControlConstructor()
{
    // To check changes in abstract classes's pure virtual functions
    MockCameraExposureControl obj;
}

QTEST_MAIN(tst_QCamera)

#include "tst_qcamera.moc"
