/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "qgstreamerimagecapturecontrol.h"
#include <QtCore/QDebug>
#include <QtCore/QDir>

QGstreamerImageCaptureControl::QGstreamerImageCaptureControl(QGstreamerCaptureSession *session)
    :QCameraImageCaptureControl(session), m_session(session), m_ready(false), m_lastId(0)
{
    connect(m_session, SIGNAL(stateChanged(QGstreamerCaptureSession::State)), SLOT(updateState()));
    connect(m_session, SIGNAL(imageExposed(int)), this, SIGNAL(imageExposed(int)));
    connect(m_session, SIGNAL(imageCaptured(int,QImage)), this, SIGNAL(imageCaptured(int,QImage)));
    connect(m_session, SIGNAL(imageSaved(int,QString)), this, SIGNAL(imageSaved(int,QString)));
}

QGstreamerImageCaptureControl::~QGstreamerImageCaptureControl()
{
}

bool QGstreamerImageCaptureControl::isReadyForCapture() const
{
    return m_ready;
}

int QGstreamerImageCaptureControl::capture(const QString &fileName)
{
    m_lastId++;

    //it's allowed to request image capture while camera is starting
    if (m_session->pendingState() == QGstreamerCaptureSession::StoppedState ||
            !(m_session->captureMode() & QGstreamerCaptureSession::Image)) {
        //emit error in the next event loop,
        //so application can associate it with returned request id.
        QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                  Q_ARG(int, m_lastId),
                                  Q_ARG(int, QCameraImageCapture::NotReadyError),
                                  Q_ARG(QString,tr("Not ready to capture")));

        return m_lastId;
    }

    QString path = fileName;
    if (path.isEmpty()) {
        int lastImage = 0;
        QDir outputDir = QDir::currentPath();
        const auto list = outputDir.entryList(QStringList() << "img_*.jpg");
        for (const QString &fileName : list) {
            int imgNumber = fileName.midRef(4, fileName.size()-8).toInt();
            lastImage = qMax(lastImage, imgNumber);
        }

        path = QString("img_%1.jpg").arg(lastImage+1,
                                         4, //fieldWidth
                                         10,
                                         QLatin1Char('0'));
    }

    m_session->captureImage(m_lastId, path);

    return m_lastId;
}

void QGstreamerImageCaptureControl::cancelCapture()
{

}

void QGstreamerImageCaptureControl::updateState()
{
    bool ready = (m_session->state() == QGstreamerCaptureSession::PreviewState) &&
            (m_session->captureMode() & QGstreamerCaptureSession::Image);

    if (m_ready != ready) {
        emit readyForCaptureChanged(m_ready = ready);
    }
}
