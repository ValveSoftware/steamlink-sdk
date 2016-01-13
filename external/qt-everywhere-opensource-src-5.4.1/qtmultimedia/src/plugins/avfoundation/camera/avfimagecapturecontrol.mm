/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "avfcameradebug.h"
#include "avfimagecapturecontrol.h"
#include "avfcamerasession.h"
#include "avfcameraservice.h"
#include "avfcameracontrol.h"

#include <QtCore/qurl.h>
#include <QtCore/qfile.h>
#include <QtCore/qbuffer.h>
#include <QtGui/qimagereader.h>

QT_USE_NAMESPACE

AVFImageCaptureControl::AVFImageCaptureControl(AVFCameraService *service, QObject *parent)
   : QCameraImageCaptureControl(parent)
   , m_service(service)
   , m_session(service->session())
   , m_cameraControl(service->cameraControl())
   , m_ready(false)
   , m_lastCaptureId(0)
   , m_videoConnection(nil)
{
    m_stillImageOutput = [[AVCaptureStillImageOutput alloc] init];

    NSDictionary *outputSettings = [[NSDictionary alloc] initWithObjectsAndKeys:
                                        AVVideoCodecJPEG, AVVideoCodecKey, nil];

    [m_stillImageOutput setOutputSettings:outputSettings];
    [outputSettings release];

    connect(m_cameraControl, SIGNAL(captureModeChanged(QCamera::CaptureModes)), SLOT(updateReadyStatus()));
    connect(m_cameraControl, SIGNAL(statusChanged(QCamera::Status)), SLOT(updateReadyStatus()));

    connect(m_session, SIGNAL(readyToConfigureConnections()), SLOT(updateCaptureConnection()));
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

    int captureId = m_lastCaptureId;
    [m_stillImageOutput captureStillImageAsynchronouslyFromConnection:m_videoConnection
                        completionHandler: ^(CMSampleBufferRef imageSampleBuffer, NSError *error) {
        if (error) {
            QStringList messageParts;
            messageParts << QString::fromUtf8([[error localizedDescription] UTF8String]);
            messageParts << QString::fromUtf8([[error localizedFailureReason] UTF8String]);
            messageParts << QString::fromUtf8([[error localizedRecoverySuggestion] UTF8String]);

            QString errorMessage = messageParts.join(" ");
            qDebugCamera() << "Image capture failed:" << errorMessage;

            QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                      Q_ARG(int, captureId),
                                      Q_ARG(int, QCameraImageCapture::ResourceError),
                                      Q_ARG(QString, errorMessage));
        } else {
            qDebugCamera() << "Image captured:" << actualFileName;
            //we can't find the exact time the image is exposed,
            //but image capture is very fast on desktop, so emit it here
            QMetaObject::invokeMethod(this, "imageExposed", Qt::QueuedConnection,
                                      Q_ARG(int, captureId));

            NSData *nsJpgData = [AVCaptureStillImageOutput jpegStillImageNSDataRepresentation:imageSampleBuffer];
            QByteArray jpgData = QByteArray::fromRawData((const char *)[nsJpgData bytes], [nsJpgData length]);

            //Generate snap preview as downscalled image
            {
                QBuffer buffer(&jpgData);
                QImageReader imageReader(&buffer);
                QSize imgSize = imageReader.size();
                int downScaleSteps = 0;
                while (imgSize.width() > 800 && downScaleSteps < 8) {
                    imgSize.rwidth() /= 2;
                    imgSize.rheight() /= 2;
                    downScaleSteps++;
                }

                imageReader.setScaledSize(imgSize);
                QImage snapPreview = imageReader.read();

                QMetaObject::invokeMethod(this, "imageCaptured", Qt::QueuedConnection,
                                          Q_ARG(int, captureId),
                                          Q_ARG(QImage, snapPreview));
            }

            qDebugCamera() << "Image captured" << actualFileName;

            QFile f(actualFileName);
            if (f.open(QFile::WriteOnly)) {
                if (f.write(jpgData) != -1) {
                    QMetaObject::invokeMethod(this, "imageSaved", Qt::QueuedConnection,
                                              Q_ARG(int, captureId),
                                              Q_ARG(QString, actualFileName));
                } else {
                    QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                              Q_ARG(int, captureId),
                                              Q_ARG(int, QCameraImageCapture::OutOfSpaceError),
                                              Q_ARG(QString, f.errorString()));
                }
            } else {
                QString errorMessage = tr("Could not open destination file:\n%1").arg(actualFileName);
                QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                          Q_ARG(int, captureId),
                                          Q_ARG(int, QCameraImageCapture::ResourceError),
                                          Q_ARG(QString, errorMessage));
            }
        }
    }];

    return captureId;
}

void AVFImageCaptureControl::cancelCapture()
{
    //not supported
}

void AVFImageCaptureControl::updateCaptureConnection()
{
    if (!m_videoConnection &&
            m_cameraControl->captureMode().testFlag(QCamera::CaptureStillImage)) {
        qDebugCamera() << Q_FUNC_INFO;
        AVCaptureSession *captureSession = m_session->captureSession();

        if ([captureSession canAddOutput:m_stillImageOutput]) {
            [captureSession addOutput:m_stillImageOutput];

            for (AVCaptureConnection *connection in m_stillImageOutput.connections) {
                for (AVCaptureInputPort *port in [connection inputPorts]) {
                    if ([[port mediaType] isEqual:AVMediaTypeVideo] ) {
                        m_videoConnection = connection;
                        break;
                    }
                }

                if (m_videoConnection)
                    break;
            }
        }

        updateReadyStatus();
    }
}

#include "moc_avfimagecapturecontrol.cpp"
