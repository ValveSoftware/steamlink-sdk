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

#include "qandroidcameraimagecapturecontrol.h"

#include "qandroidcamerasession.h"

QT_BEGIN_NAMESPACE

QAndroidCameraImageCaptureControl::QAndroidCameraImageCaptureControl(QAndroidCameraSession *session)
    : QCameraImageCaptureControl()
    , m_session(session)
{
    connect(m_session, SIGNAL(readyForCaptureChanged(bool)), this, SIGNAL(readyForCaptureChanged(bool)));
    connect(m_session, SIGNAL(imageExposed(int)), this, SIGNAL(imageExposed(int)));
    connect(m_session, SIGNAL(imageCaptured(int,QImage)), this, SIGNAL(imageCaptured(int,QImage)));
    connect(m_session, SIGNAL(imageMetadataAvailable(int,QString,QVariant)), this, SIGNAL(imageMetadataAvailable(int,QString,QVariant)));
    connect(m_session, SIGNAL(imageAvailable(int,QVideoFrame)), this, SIGNAL(imageAvailable(int,QVideoFrame)));
    connect(m_session, SIGNAL(imageSaved(int,QString)), this, SIGNAL(imageSaved(int,QString)));
    connect(m_session, SIGNAL(imageCaptureError(int,int,QString)), this, SIGNAL(error(int,int,QString)));
}

bool QAndroidCameraImageCaptureControl::isReadyForCapture() const
{
    return m_session->isReadyForCapture();
}

QCameraImageCapture::DriveMode QAndroidCameraImageCaptureControl::driveMode() const
{
    return m_session->driveMode();
}

void QAndroidCameraImageCaptureControl::setDriveMode(QCameraImageCapture::DriveMode mode)
{
    m_session->setDriveMode(mode);
}

int QAndroidCameraImageCaptureControl::capture(const QString &fileName)
{
    return m_session->capture(fileName);
}

void QAndroidCameraImageCaptureControl::cancelCapture()
{
    m_session->cancelCapture();
}

QT_END_NAMESPACE
