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

#ifndef MOCKCAMERACAPTURECONTROL_H
#define MOCKCAMERACAPTURECONTROL_H

#include <QDateTime>
#include <QTimer>
#include <QtMultimedia/qmediametadata.h>

#include "qcameraimagecapturecontrol.h"
#include "qcameracontrol.h"
#include "mockcameracontrol.h"

class MockCaptureControl : public QCameraImageCaptureControl
{
    Q_OBJECT
public:
    MockCaptureControl(MockCameraControl *cameraControl, QObject *parent = 0)
        : QCameraImageCaptureControl(parent), m_cameraControl(cameraControl), m_captureRequest(0), m_ready(true), m_captureCanceled(false)
    {
    }

    ~MockCaptureControl()
    {
    }

    QCameraImageCapture::DriveMode driveMode() const { return QCameraImageCapture::SingleImageCapture; }
    void setDriveMode(QCameraImageCapture::DriveMode) {}

    bool isReadyForCapture() const { return m_ready && m_cameraControl->state() == QCamera::ActiveState; }

    int capture(const QString &fileName)
    {
        if (isReadyForCapture()) {
            m_fileName = fileName;
            m_captureRequest++;
            emit readyForCaptureChanged(m_ready = false);
            QTimer::singleShot(5, this, SLOT(captured()));
            return m_captureRequest;
        } else {
            emit error(-1, QCameraImageCapture::NotReadyError,
                       QLatin1String("Could not capture in stopped state"));
        }

        return -1;
    }

    void cancelCapture()
    {
        m_captureCanceled = true;
    }

private Q_SLOTS:
    void captured()
    {
        if (!m_captureCanceled) {
            emit imageCaptured(m_captureRequest, QImage());

            emit imageMetadataAvailable(m_captureRequest,
                                        QMediaMetaData::FocalLengthIn35mmFilm,
                                        QVariant(50));

            emit imageMetadataAvailable(m_captureRequest,
                                        QMediaMetaData::DateTimeOriginal,
                                        QVariant(QDateTime::currentDateTime()));

            emit imageMetadataAvailable(m_captureRequest,
                                        QLatin1String("Answer to the Ultimate Question of Life, the Universe, and Everything"),
                                        QVariant(42));
        }

        if (!m_ready)
        {
            emit readyForCaptureChanged(m_ready = true);
            emit imageExposed(m_captureRequest);
        }

        if (!m_captureCanceled)
            emit imageSaved(m_captureRequest, m_fileName);

        m_captureCanceled = false;
    }

private:
    MockCameraControl *m_cameraControl;
    QString m_fileName;
    int m_captureRequest;
    bool m_ready;
    bool m_captureCanceled;
};

#endif // MOCKCAMERACAPTURECONTROL_H
