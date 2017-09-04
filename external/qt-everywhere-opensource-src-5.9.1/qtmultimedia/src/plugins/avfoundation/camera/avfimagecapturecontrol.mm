/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "avfcameradebug.h"
#include "avfimagecapturecontrol.h"
#include "avfcameraservice.h"
#include "avfcamerautility.h"
#include "avfcameracontrol.h"

#include <QtCore/qurl.h>
#include <QtCore/qfile.h>
#include <QtCore/qbuffer.h>
#include <QtConcurrent/qtconcurrentrun.h>
#include <QtGui/qimagereader.h>
#include <private/qvideoframe_p.h>

QT_USE_NAMESPACE

AVFImageCaptureControl::AVFImageCaptureControl(AVFCameraService *service, QObject *parent)
   : QCameraImageCaptureControl(parent)
   , m_session(service->session())
   , m_cameraControl(service->cameraControl())
   , m_ready(false)
   , m_lastCaptureId(0)
   , m_videoConnection(nil)
{
    Q_UNUSED(service);
    m_stillImageOutput = [[AVCaptureStillImageOutput alloc] init];

    NSDictionary *outputSettings = [[NSDictionary alloc] initWithObjectsAndKeys:
                                        AVVideoCodecJPEG, AVVideoCodecKey, nil];

    [m_stillImageOutput setOutputSettings:outputSettings];
    [outputSettings release];

    connect(m_cameraControl, SIGNAL(captureModeChanged(QCamera::CaptureModes)), SLOT(updateReadyStatus()));
    connect(m_cameraControl, SIGNAL(statusChanged(QCamera::Status)), SLOT(updateReadyStatus()));

    connect(m_session, SIGNAL(readyToConfigureConnections()), SLOT(updateCaptureConnection()));
    connect(m_cameraControl, SIGNAL(captureModeChanged(QCamera::CaptureModes)), SLOT(updateCaptureConnection()));

    connect(m_session, &AVFCameraSession::newViewfinderFrame,
            this, &AVFImageCaptureControl::onNewViewfinderFrame,
            Qt::DirectConnection);
}

AVFImageCaptureControl::~AVFImageCaptureControl()
{
}

bool AVFImageCaptureControl::isReadyForCapture() const
{
    return m_videoConnection &&
            m_cameraControl->captureMode().testFlag(QCamera::CaptureStillImage) &&
            m_cameraControl->status() == QCamera::ActiveStatus;
}

void AVFImageCaptureControl::updateReadyStatus()
{
    if (m_ready != isReadyForCapture()) {
        m_ready = !m_ready;
        qDebugCamera() << "ReadyToCapture status changed:" << m_ready;
        Q_EMIT readyForCaptureChanged(m_ready);
    }
}

int AVFImageCaptureControl::capture(const QString &fileName)
{
    m_lastCaptureId++;

    if (!isReadyForCapture()) {
        QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                  Q_ARG(int, m_lastCaptureId),
                                  Q_ARG(int, QCameraImageCapture::NotReadyError),
                                  Q_ARG(QString, tr("Camera not ready")));
        return m_lastCaptureId;
    }

    QString actualFileName = m_storageLocation.generateFileName(fileName,
                                                                QCamera::CaptureStillImage,
                                                                QLatin1String("img_"),
                                                                QLatin1String("jpg"));

    qDebugCamera() << "Capture image to" << actualFileName;

    CaptureRequest request = { m_lastCaptureId, new QSemaphore };
    m_requestsMutex.lock();
    m_captureRequests.enqueue(request);
    m_requestsMutex.unlock();

    [m_stillImageOutput captureStillImageAsynchronouslyFromConnection:m_videoConnection
                        completionHandler: ^(CMSampleBufferRef imageSampleBuffer, NSError *error) {

        // Wait for the preview to be generated before saving the JPEG
        request.previewReady->acquire();
        delete request.previewReady;

        if (error) {
            QStringList messageParts;
            messageParts << QString::fromUtf8([[error localizedDescription] UTF8String]);
            messageParts << QString::fromUtf8([[error localizedFailureReason] UTF8String]);
            messageParts << QString::fromUtf8([[error localizedRecoverySuggestion] UTF8String]);

            QString errorMessage = messageParts.join(" ");
            qDebugCamera() << "Image capture failed:" << errorMessage;

            QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                      Q_ARG(int, request.captureId),
                                      Q_ARG(int, QCameraImageCapture::ResourceError),
                                      Q_ARG(QString, errorMessage));
        } else {
            qDebugCamera() << "Image capture completed:" << actualFileName;

            NSData *nsJpgData = [AVCaptureStillImageOutput jpegStillImageNSDataRepresentation:imageSampleBuffer];
            QByteArray jpgData = QByteArray::fromRawData((const char *)[nsJpgData bytes], [nsJpgData length]);

            QFile f(actualFileName);
            if (f.open(QFile::WriteOnly)) {
                if (f.write(jpgData) != -1) {
                    QMetaObject::invokeMethod(this, "imageSaved", Qt::QueuedConnection,
                                              Q_ARG(int, request.captureId),
                                              Q_ARG(QString, actualFileName));
                } else {
                    QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                              Q_ARG(int, request.captureId),
                                              Q_ARG(int, QCameraImageCapture::OutOfSpaceError),
                                              Q_ARG(QString, f.errorString()));
                }
            } else {
                QString errorMessage = tr("Could not open destination file:\n%1").arg(actualFileName);
                QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                          Q_ARG(int, request.captureId),
                                          Q_ARG(int, QCameraImageCapture::ResourceError),
                                          Q_ARG(QString, errorMessage));
            }
        }
    }];

    return request.captureId;
}

void AVFImageCaptureControl::onNewViewfinderFrame(const QVideoFrame &frame)
{
    QMutexLocker locker(&m_requestsMutex);

    if (m_captureRequests.isEmpty())
        return;

    CaptureRequest request = m_captureRequests.dequeue();
    Q_EMIT imageExposed(request.captureId);

    QtConcurrent::run(this, &AVFImageCaptureControl::makeCapturePreview,
                      request,
                      frame,
                      0 /* rotation */);
}

void AVFImageCaptureControl::makeCapturePreview(CaptureRequest request,
                                                const QVideoFrame &frame,
                                                int rotation)
{
    QTransform transform;
    transform.rotate(rotation);

    Q_EMIT imageCaptured(request.captureId, qt_imageFromVideoFrame(frame).transformed(transform));

    request.previewReady->release();
}

void AVFImageCaptureControl::cancelCapture()
{
    //not supported
}

void AVFImageCaptureControl::updateCaptureConnection()
{
    if (m_cameraControl->captureMode().testFlag(QCamera::CaptureStillImage)) {
        qDebugCamera() << Q_FUNC_INFO;
        AVCaptureSession *captureSession = m_session->captureSession();

        if (![captureSession.outputs containsObject:m_stillImageOutput]) {
            if ([captureSession canAddOutput:m_stillImageOutput]) {
                // Lock the video capture device to make sure the active format is not reset
                const AVFConfigurationLock lock(m_session->videoCaptureDevice());
                [captureSession addOutput:m_stillImageOutput];
                m_videoConnection = [m_stillImageOutput connectionWithMediaType:AVMediaTypeVideo];
                updateReadyStatus();
            }
        } else {
            m_videoConnection = [m_stillImageOutput connectionWithMediaType:AVMediaTypeVideo];
        }
    }
}

#include "moc_avfimagecapturecontrol.cpp"
