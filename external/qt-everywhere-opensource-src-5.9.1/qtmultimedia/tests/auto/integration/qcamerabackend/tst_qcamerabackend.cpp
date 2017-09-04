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
#include <QtGui/QImageReader>
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
#include <private/qmediaserviceprovider_p.h>

QT_USE_NAMESPACE

/*
 This is the backend conformance test.

 Since it relies on platform media framework and sound hardware
 it may be less stable.
*/

class tst_QCameraBackend: public QObject
{
    Q_OBJECT

public slots:
    void initTestCase();
    void cleanupTestCase();

private slots:
    void testAvailableDevices();
    void testDeviceDescription();
    void testCameraInfo();
    void testCtorWithDevice();
    void testCtorWithCameraInfo();
    void testCtorWithPosition();

    void testCameraStates();
    void testCaptureMode();
    void testCameraCapture();
    void testCaptureToBuffer();
    void testCameraCaptureMetadata();
    void testExposureCompensation();
    void testExposureMode();

    void testVideoRecording_data();
    void testVideoRecording();
private:
};

void tst_QCameraBackend::initTestCase()
{
    QCamera camera;
    if (!camera.isAvailable())
        QSKIP("Camera is not available");
}

void tst_QCameraBackend::cleanupTestCase()
{
}

void tst_QCameraBackend::testAvailableDevices()
{
    int deviceCount = QMediaServiceProvider::defaultServiceProvider()->devices(QByteArray(Q_MEDIASERVICE_CAMERA)).count();
    QCOMPARE(QCamera::availableDevices().count(), deviceCount);
}

void tst_QCameraBackend::testDeviceDescription()
{
    int deviceCount = QMediaServiceProvider::defaultServiceProvider()->devices(QByteArray(Q_MEDIASERVICE_CAMERA)).count();

    if (deviceCount == 0)
        QVERIFY(QCamera::deviceDescription(QByteArray("random")).isNull());
    else {
        foreach (const QByteArray &device, QCamera::availableDevices())
            QVERIFY(QCamera::deviceDescription(device).length() > 0);
    }
}

void tst_QCameraBackend::testCameraInfo()
{
    int deviceCount = QMediaServiceProvider::defaultServiceProvider()->devices(QByteArray(Q_MEDIASERVICE_CAMERA)).count();
    QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
    QCOMPARE(cameras.count(), deviceCount);
    if (cameras.isEmpty()) {
        QVERIFY(QCameraInfo::defaultCamera().isNull());
        QSKIP("Camera selection is not supported");
    }

    foreach (const QCameraInfo &info, cameras) {
        QVERIFY(!info.deviceName().isEmpty());
        QVERIFY(!info.description().isEmpty());
        QVERIFY(info.orientation() % 90 == 0);
    }
}

void tst_QCameraBackend::testCtorWithDevice()
{
    if (QCamera::availableDevices().isEmpty())
        QSKIP("Camera selection not supported");

    QCamera *camera = new QCamera(QCamera::availableDevices().first());
    QCOMPARE(camera->error(), QCamera::NoError);
    delete camera;

    //loading non existing camera should fail
    camera = new QCamera(QUuid::createUuid().toByteArray());
    QCOMPARE(camera->error(), QCamera::ServiceMissingError);

    delete camera;
}

void tst_QCameraBackend::testCtorWithCameraInfo()
{
    if (QCameraInfo::availableCameras().isEmpty())
        QSKIP("Camera selection not supported");

    {
        QCameraInfo info = QCameraInfo::defaultCamera();
        QCamera camera(info);
        QCOMPARE(camera.error(), QCamera::NoError);
        QCOMPARE(QCameraInfo(camera), info);
    }
    {
        QCameraInfo info = QCameraInfo::availableCameras().first();
        QCamera camera(info);
        QCOMPARE(camera.error(), QCamera::NoError);
        QCOMPARE(QCameraInfo(camera), info);
    }
    {
        // loading an invalid CameraInfo should fail
        QCamera *camera = new QCamera(QCameraInfo());
        QCOMPARE(camera->error(), QCamera::ServiceMissingError);
        QVERIFY(QCameraInfo(*camera).isNull());
        delete camera;
    }
    {
        // loading non existing camera should fail
        QCamera camera(QCameraInfo(QUuid::createUuid().toByteArray()));
        QCOMPARE(camera.error(), QCamera::ServiceMissingError);
        QVERIFY(QCameraInfo(camera).isNull());
    }
}

void tst_QCameraBackend::testCtorWithPosition()
{
    {
        QCamera camera(QCamera::UnspecifiedPosition);
        QCOMPARE(camera.error(), QCamera::NoError);
    }
    {
        QCamera camera(QCamera::FrontFace);
        // even if no camera is available at this position, it should not fail
        // and load the default camera
        QCOMPARE(camera.error(), QCamera::NoError);
    }
    {
        QCamera camera(QCamera::BackFace);
        // even if no camera is available at this position, it should not fail
        // and load the default camera
        QCOMPARE(camera.error(), QCamera::NoError);
    }
}

void tst_QCameraBackend::testCameraStates()
{
    QCamera camera;
    QCameraImageCapture imageCapture(&camera);

    QSignalSpy errorSignal(&camera, SIGNAL(error(QCamera::Error)));
    QSignalSpy stateChangedSignal(&camera, SIGNAL(stateChanged(QCamera::State)));
    QSignalSpy statusChangedSignal(&camera, SIGNAL(statusChanged(QCamera::Status)));

    QCOMPARE(camera.state(), QCamera::UnloadedState);
    QCOMPARE(camera.status(), QCamera::UnloadedStatus);

    camera.load();
    QCOMPARE(camera.state(), QCamera::LoadedState);
    QCOMPARE(stateChangedSignal.count(), 1);
    QCOMPARE(stateChangedSignal.last().first().value<QCamera::State>(), QCamera::LoadedState);
    QVERIFY(stateChangedSignal.count() > 0);

    QTRY_COMPARE(camera.status(), QCamera::LoadedStatus);
    QCOMPARE(statusChangedSignal.last().first().value<QCamera::Status>(), QCamera::LoadedStatus);

    camera.unload();
    QCOMPARE(camera.state(), QCamera::UnloadedState);
    QCOMPARE(stateChangedSignal.last().first().value<QCamera::State>(), QCamera::UnloadedState);
    QTRY_COMPARE(camera.status(), QCamera::UnloadedStatus);
    QCOMPARE(statusChangedSignal.last().first().value<QCamera::Status>(), QCamera::UnloadedStatus);

    camera.start();
    QCOMPARE(camera.state(), QCamera::ActiveState);
    QCOMPARE(stateChangedSignal.last().first().value<QCamera::State>(), QCamera::ActiveState);
    QTRY_COMPARE(camera.status(), QCamera::ActiveStatus);
    QCOMPARE(statusChangedSignal.last().first().value<QCamera::Status>(), QCamera::ActiveStatus);

    camera.stop();
    QCOMPARE(camera.state(), QCamera::LoadedState);
    QCOMPARE(stateChangedSignal.last().first().value<QCamera::State>(), QCamera::LoadedState);
    QTRY_COMPARE(camera.status(), QCamera::LoadedStatus);
    QCOMPARE(statusChangedSignal.last().first().value<QCamera::Status>(), QCamera::LoadedStatus);

    camera.unload();
    QCOMPARE(camera.state(), QCamera::UnloadedState);
    QCOMPARE(stateChangedSignal.last().first().value<QCamera::State>(), QCamera::UnloadedState);
    QTRY_COMPARE(camera.status(), QCamera::UnloadedStatus);
    QCOMPARE(statusChangedSignal.last().first().value<QCamera::Status>(), QCamera::UnloadedStatus);

    QCOMPARE(camera.errorString(), QString());
    QCOMPARE(errorSignal.count(), 0);
}

void tst_QCameraBackend::testCaptureMode()
{
    QCamera camera;

    QSignalSpy errorSignal(&camera, SIGNAL(error(QCamera::Error)));
    QSignalSpy stateChangedSignal(&camera, SIGNAL(stateChanged(QCamera::State)));
    QSignalSpy captureModeSignal(&camera, SIGNAL(captureModeChanged(QCamera::CaptureModes)));

    QCOMPARE(camera.captureMode(), QCamera::CaptureStillImage);

    if (!camera.isCaptureModeSupported(QCamera::CaptureVideo)) {
        camera.setCaptureMode(QCamera::CaptureVideo);
        QCOMPARE(camera.captureMode(), QCamera::CaptureStillImage);
        QSKIP("Video capture not supported");
    }

    camera.setCaptureMode(QCamera::CaptureVideo);
    QCOMPARE(camera.captureMode(), QCamera::CaptureVideo);
    QTRY_COMPARE(captureModeSignal.size(), 1);
    QCOMPARE(captureModeSignal.last().first().value<QCamera::CaptureModes>(), QCamera::CaptureVideo);
    captureModeSignal.clear();

    camera.load();
    QTRY_COMPARE(camera.status(), QCamera::LoadedStatus);
    //capture mode should still be video
    QCOMPARE(camera.captureMode(), QCamera::CaptureVideo);

    //it should be possible to switch capture mode in Loaded state
    camera.setCaptureMode(QCamera::CaptureStillImage);
    QTRY_COMPARE(captureModeSignal.size(), 1);
    QCOMPARE(captureModeSignal.last().first().value<QCamera::CaptureModes>(), QCamera::CaptureStillImage);
    captureModeSignal.clear();

    camera.setCaptureMode(QCamera::CaptureVideo);
    QTRY_COMPARE(captureModeSignal.size(), 1);
    QCOMPARE(captureModeSignal.last().first().value<QCamera::CaptureModes>(), QCamera::CaptureVideo);
    captureModeSignal.clear();

    camera.start();
    QTRY_COMPARE(camera.status(), QCamera::ActiveStatus);
    //capture mode should still be video
    QCOMPARE(camera.captureMode(), QCamera::CaptureVideo);

    stateChangedSignal.clear();
    //it should be possible to switch capture mode in Active state
    camera.setCaptureMode(QCamera::CaptureStillImage);
    //camera may leave Active status, but should return to Active
    QTest::qWait(10); //camera may leave Active status async
    QTRY_COMPARE(camera.status(), QCamera::ActiveStatus);
    QCOMPARE(camera.captureMode(), QCamera::CaptureStillImage);
    QVERIFY2(stateChangedSignal.isEmpty(), "camera should not change the state during capture mode changes");

    QCOMPARE(captureModeSignal.size(), 1);
    QCOMPARE(captureModeSignal.last().first().value<QCamera::CaptureModes>(), QCamera::CaptureStillImage);
    captureModeSignal.clear();

    camera.setCaptureMode(QCamera::CaptureVideo);
    //camera may leave Active status, but should return to Active
    QTest::qWait(10); //camera may leave Active status async
    QTRY_COMPARE(camera.status(), QCamera::ActiveStatus);
    QCOMPARE(camera.captureMode(), QCamera::CaptureVideo);

    QVERIFY2(stateChangedSignal.isEmpty(), "camera should not change the state during capture mode changes");

    QCOMPARE(captureModeSignal.size(), 1);
    QCOMPARE(captureModeSignal.last().first().value<QCamera::CaptureModes>(), QCamera::CaptureVideo);
    captureModeSignal.clear();

    camera.stop();
    QCOMPARE(camera.captureMode(), QCamera::CaptureVideo);
    camera.unload();
    QCOMPARE(camera.captureMode(), QCamera::CaptureVideo);

    QVERIFY2(errorSignal.isEmpty(), QString("Camera error: %1").arg(camera.errorString()).toLocal8Bit());
}

void tst_QCameraBackend::testCameraCapture()
{
    QCamera camera;
    QCameraImageCapture imageCapture(&camera);
    //prevents camera to flash during the test
    camera.exposure()->setFlashMode(QCameraExposure::FlashOff);

    QVERIFY(!imageCapture.isReadyForCapture());

    QSignalSpy capturedSignal(&imageCapture, SIGNAL(imageCaptured(int,QImage)));
    QSignalSpy savedSignal(&imageCapture, SIGNAL(imageSaved(int,QString)));
    QSignalSpy errorSignal(&imageCapture, SIGNAL(error(int,QCameraImageCapture::Error,QString)));

    imageCapture.capture();
    QTRY_COMPARE(errorSignal.size(), 1);
    QCOMPARE(imageCapture.error(), QCameraImageCapture::NotReadyError);
    QCOMPARE(capturedSignal.size(), 0);
    errorSignal.clear();

    camera.start();

    QTRY_VERIFY(imageCapture.isReadyForCapture());
    QCOMPARE(camera.status(), QCamera::ActiveStatus);
    QCOMPARE(errorSignal.size(), 0);

    int id = imageCapture.capture();

    QTRY_VERIFY(!savedSignal.isEmpty());

    QCOMPARE(capturedSignal.size(), 1);
    QCOMPARE(capturedSignal.last().first().toInt(), id);
    QCOMPARE(errorSignal.size(), 0);
    QCOMPARE(imageCapture.error(), QCameraImageCapture::NoError);

    QCOMPARE(savedSignal.last().first().toInt(), id);
    QString location = savedSignal.last().last().toString();
    QVERIFY(!location.isEmpty());
    QVERIFY(QFileInfo(location).exists());
    QImageReader reader(location);
    reader.setScaledSize(QSize(320,240));
    QVERIFY(!reader.read().isNull());

    QFile(location).remove();
}


void tst_QCameraBackend::testCaptureToBuffer()
{
    QCamera camera;
    QCameraImageCapture imageCapture(&camera);
    camera.exposure()->setFlashMode(QCameraExposure::FlashOff);

    camera.load();

    if (!imageCapture.isCaptureDestinationSupported(QCameraImageCapture::CaptureToBuffer))
        QSKIP("Buffer capture not supported");

    QTRY_COMPARE(camera.status(), QCamera::LoadedStatus);

    QCOMPARE(imageCapture.bufferFormat(), QVideoFrame::Format_Jpeg);

    QVERIFY(imageCapture.isCaptureDestinationSupported(QCameraImageCapture::CaptureToFile));
    QVERIFY(imageCapture.isCaptureDestinationSupported(QCameraImageCapture::CaptureToBuffer));
    QVERIFY(imageCapture.isCaptureDestinationSupported(
                QCameraImageCapture::CaptureToBuffer | QCameraImageCapture::CaptureToFile));

    QSignalSpy destinationChangedSignal(&imageCapture, SIGNAL(captureDestinationChanged(QCameraImageCapture::CaptureDestinations)));

    QCOMPARE(imageCapture.captureDestination(), QCameraImageCapture::CaptureToFile);
    imageCapture.setCaptureDestination(QCameraImageCapture::CaptureToBuffer);
    QCOMPARE(imageCapture.captureDestination(), QCameraImageCapture::CaptureToBuffer);
    QCOMPARE(destinationChangedSignal.size(), 1);
    QCOMPARE(destinationChangedSignal.first().first().value<QCameraImageCapture::CaptureDestinations>(),
             QCameraImageCapture::CaptureToBuffer);

    QSignalSpy capturedSignal(&imageCapture, SIGNAL(imageCaptured(int,QImage)));
    QSignalSpy imageAvailableSignal(&imageCapture, SIGNAL(imageAvailable(int,QVideoFrame)));
    QSignalSpy savedSignal(&imageCapture, SIGNAL(imageSaved(int,QString)));
    QSignalSpy errorSignal(&imageCapture, SIGNAL(error(int,QCameraImageCapture::Error,QString)));

    camera.start();
    QTRY_VERIFY(imageCapture.isReadyForCapture());

    int id = imageCapture.capture();
    QTRY_VERIFY(!imageAvailableSignal.isEmpty());

    QVERIFY(errorSignal.isEmpty());
    QVERIFY(!capturedSignal.isEmpty());
    QVERIFY(!imageAvailableSignal.isEmpty());

    QTest::qWait(2000);
    QVERIFY(savedSignal.isEmpty());

    QCOMPARE(capturedSignal.first().first().toInt(), id);
    QCOMPARE(imageAvailableSignal.first().first().toInt(), id);

    QVideoFrame frame = imageAvailableSignal.first().last().value<QVideoFrame>();
    QVERIFY(frame.isValid());
    QCOMPARE(frame.pixelFormat(), QVideoFrame::Format_Jpeg);
    QVERIFY(!frame.size().isEmpty());
    QVERIFY(frame.map(QAbstractVideoBuffer::ReadOnly));
    QByteArray data((const char *)frame.bits(), frame.mappedBytes());
    frame.unmap();
    frame = QVideoFrame();

    QVERIFY(!data.isEmpty());
    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer, "JPG");
    reader.setScaledSize(QSize(640,480));
    QImage img(reader.read());
    QVERIFY(!img.isNull());

    capturedSignal.clear();
    imageAvailableSignal.clear();
    savedSignal.clear();

    if (imageCapture.supportedBufferFormats().contains(QVideoFrame::Format_UYVY)) {
        imageCapture.setBufferFormat(QVideoFrame::Format_UYVY);
        QCOMPARE(imageCapture.bufferFormat(), QVideoFrame::Format_UYVY);

        id = imageCapture.capture();
        QTRY_VERIFY(!imageAvailableSignal.isEmpty());

        QVERIFY(errorSignal.isEmpty());
        QVERIFY(!capturedSignal.isEmpty());
        QVERIFY(!imageAvailableSignal.isEmpty());
        QVERIFY(savedSignal.isEmpty());

        QTest::qWait(2000);
        QVERIFY(savedSignal.isEmpty());

        frame = imageAvailableSignal.first().last().value<QVideoFrame>();
        QVERIFY(frame.isValid());

        qDebug() << frame.pixelFormat();
        QCOMPARE(frame.pixelFormat(), QVideoFrame::Format_UYVY);
        QVERIFY(!frame.size().isEmpty());
        frame = QVideoFrame();

        capturedSignal.clear();
        imageAvailableSignal.clear();
        savedSignal.clear();

        imageCapture.setBufferFormat(QVideoFrame::Format_Jpeg);
        QCOMPARE(imageCapture.bufferFormat(), QVideoFrame::Format_Jpeg);
    }

    QTRY_VERIFY(imageCapture.isReadyForCapture());

    //Try to capture to both buffer and file
    if (imageCapture.isCaptureDestinationSupported(QCameraImageCapture::CaptureToBuffer | QCameraImageCapture::CaptureToFile)) {
        imageCapture.setCaptureDestination(QCameraImageCapture::CaptureToBuffer | QCameraImageCapture::CaptureToFile);

        int oldId = id;
        id = imageCapture.capture();
        QVERIFY(id != oldId);
        QTRY_VERIFY(!savedSignal.isEmpty());

        QVERIFY(errorSignal.isEmpty());
        QVERIFY(!capturedSignal.isEmpty());
        QVERIFY(!imageAvailableSignal.isEmpty());
        QVERIFY(!savedSignal.isEmpty());

        QCOMPARE(capturedSignal.first().first().toInt(), id);
        QCOMPARE(imageAvailableSignal.first().first().toInt(), id);

        frame = imageAvailableSignal.first().last().value<QVideoFrame>();
        QVERIFY(frame.isValid());
        QCOMPARE(frame.pixelFormat(), QVideoFrame::Format_Jpeg);
        QVERIFY(!frame.size().isEmpty());

        QString fileName = savedSignal.first().last().toString();
        QVERIFY(QFileInfo(fileName).exists());
    }
}

void tst_QCameraBackend::testCameraCaptureMetadata()
{
    QSKIP("Capture metadata is supported only on harmattan");

    QCamera camera;
    QCameraImageCapture imageCapture(&camera);
    camera.exposure()->setFlashMode(QCameraExposure::FlashOff);

    QSignalSpy metadataSignal(&imageCapture, SIGNAL(imageMetadataAvailable(int,QString,QVariant)));
    QSignalSpy savedSignal(&imageCapture, SIGNAL(imageSaved(int,QString)));

    camera.start();

    QTRY_VERIFY(imageCapture.isReadyForCapture());

    int id = imageCapture.capture(QString::fromLatin1("/dev/null"));
    QTRY_VERIFY(!savedSignal.isEmpty());
    QVERIFY(!metadataSignal.isEmpty());
    QCOMPARE(metadataSignal.first().first().toInt(), id);
}

void tst_QCameraBackend::testExposureCompensation()
{
    QSKIP("Capture exposure parameters are supported only on mobile platforms");

    QCamera camera;
    QCameraExposure *exposure = camera.exposure();

    QSignalSpy exposureCompensationSignal(exposure, SIGNAL(exposureCompensationChanged(qreal)));

    //it should be possible to set exposure parameters in Unloaded state
    QCOMPARE(exposure->exposureCompensation()+1.0, 1.0);
    exposure->setExposureCompensation(1.0);
    QCOMPARE(exposure->exposureCompensation(), 1.0);
    QTRY_COMPARE(exposureCompensationSignal.count(), 1);
    QCOMPARE(exposureCompensationSignal.last().first().toReal(), 1.0);

    //exposureCompensationChanged should not be emitted when value is not changed
    exposure->setExposureCompensation(1.0);
    QTest::qWait(50);
    QCOMPARE(exposureCompensationSignal.count(), 1);

    //exposure compensation should be preserved during load/start
    camera.load();
    QTRY_COMPARE(camera.status(), QCamera::LoadedStatus);

    QCOMPARE(exposure->exposureCompensation(), 1.0);

    exposureCompensationSignal.clear();
    exposure->setExposureCompensation(-1.0);
    QCOMPARE(exposure->exposureCompensation(), -1.0);
    QTRY_COMPARE(exposureCompensationSignal.count(), 1);
    QCOMPARE(exposureCompensationSignal.last().first().toReal(), -1.0);

    camera.start();
    QTRY_COMPARE(camera.status(), QCamera::ActiveStatus);

    QCOMPARE(exposure->exposureCompensation(), -1.0);

    exposureCompensationSignal.clear();
    exposure->setExposureCompensation(1.0);
    QCOMPARE(exposure->exposureCompensation(), 1.0);
    QTRY_COMPARE(exposureCompensationSignal.count(), 1);
    QCOMPARE(exposureCompensationSignal.last().first().toReal(), 1.0);
}

void tst_QCameraBackend::testExposureMode()
{
    QSKIP("Capture exposure parameters are supported only on mobile platforms");

    QCamera camera;
    QCameraExposure *exposure = camera.exposure();

    QCOMPARE(exposure->exposureMode(), QCameraExposure::ExposureAuto);

    // Night
    exposure->setExposureMode(QCameraExposure::ExposureNight);
    QCOMPARE(exposure->exposureMode(), QCameraExposure::ExposureNight);
    camera.start();
    QTRY_COMPARE(camera.status(), QCamera::ActiveStatus);
    QCOMPARE(exposure->exposureMode(), QCameraExposure::ExposureNight);

    camera.unload();
    QTRY_COMPARE(camera.status(), QCamera::UnloadedStatus);

    // Auto
    exposure->setExposureMode(QCameraExposure::ExposureAuto);
    QCOMPARE(exposure->exposureMode(), QCameraExposure::ExposureAuto);
    camera.start();
    QTRY_COMPARE(camera.status(), QCamera::ActiveStatus);
    QCOMPARE(exposure->exposureMode(), QCameraExposure::ExposureAuto);
}

void tst_QCameraBackend::testVideoRecording_data()
{
    QTest::addColumn<QByteArray>("device");

    QList<QByteArray> devices = QCamera::availableDevices();

    foreach (const QByteArray &device, devices) {
        QTest::newRow(QCamera::deviceDescription(device).toUtf8())
                << device;
    }

    if (devices.isEmpty())
        QTest::newRow("Default device") << QByteArray();
}

void tst_QCameraBackend::testVideoRecording()
{
    QFETCH(QByteArray, device);

    QScopedPointer<QCamera> camera(device.isEmpty() ? new QCamera : new QCamera(device));

    QMediaRecorder recorder(camera.data());

    QSignalSpy errorSignal(camera.data(), SIGNAL(error(QCamera::Error)));
    QSignalSpy recorderErrorSignal(&recorder, SIGNAL(error(QMediaRecorder::Error)));
    QSignalSpy recorderStatusSignal(&recorder, SIGNAL(statusChanged(QMediaRecorder::Status)));

    if (!camera->isCaptureModeSupported(QCamera::CaptureVideo)) {
        QSKIP("Video capture not supported");
    }

    camera->setCaptureMode(QCamera::CaptureVideo);

    QVideoEncoderSettings videoSettings;
    videoSettings.setResolution(320, 240);
    recorder.setVideoSettings(videoSettings);

    QCOMPARE(recorder.status(), QMediaRecorder::UnloadedStatus);

    camera->start();
    QVERIFY(recorder.status() == QMediaRecorder::LoadingStatus ||
            recorder.status() == QMediaRecorder::LoadedStatus);
    QCOMPARE(recorderStatusSignal.last().first().value<QMediaRecorder::Status>(), recorder.status());
    QTRY_COMPARE(camera->status(), QCamera::ActiveStatus);
    QTRY_COMPARE(recorder.status(), QMediaRecorder::LoadedStatus);
    QCOMPARE(recorderStatusSignal.last().first().value<QMediaRecorder::Status>(), recorder.status());

    //record 5 seconds clip
    recorder.record();
    QTRY_COMPARE(recorder.status(), QMediaRecorder::RecordingStatus);
    QCOMPARE(recorderStatusSignal.last().first().value<QMediaRecorder::Status>(), recorder.status());
    QTest::qWait(5000);
    recorder.stop();
    QCOMPARE(recorder.status(), QMediaRecorder::FinalizingStatus);
    QCOMPARE(recorderStatusSignal.last().first().value<QMediaRecorder::Status>(), recorder.status());
    QTRY_COMPARE(recorder.status(), QMediaRecorder::LoadedStatus);
    QCOMPARE(recorderStatusSignal.last().first().value<QMediaRecorder::Status>(), recorder.status());

    QVERIFY(errorSignal.isEmpty());
    QVERIFY(recorderErrorSignal.isEmpty());

    QString fileName = recorder.actualLocation().toLocalFile();
    QVERIFY(!fileName.isEmpty());

    QVERIFY(QFileInfo(fileName).size() > 0);
    QFile(fileName).remove();

    camera->setCaptureMode(QCamera::CaptureStillImage);
    QTRY_COMPARE(recorder.status(), QMediaRecorder::UnloadedStatus);
    QCOMPARE(recorderStatusSignal.last().first().value<QMediaRecorder::Status>(), recorder.status());
}

QTEST_MAIN(tst_QCameraBackend)

#include "tst_qcamerabackend.moc"
