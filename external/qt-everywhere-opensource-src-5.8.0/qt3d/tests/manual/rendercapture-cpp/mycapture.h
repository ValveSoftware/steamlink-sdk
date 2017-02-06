/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef MYCAPTURE_H
#define MYCAPTURE_H

#include <QLabel>
#include <Qt3DRender/QRenderCapture>

class MyCapture : public QObject
{
    Q_OBJECT
public:
    MyCapture(Qt3DRender::QRenderCapture* capture, QLabel *imageLabel)
        : m_capture(capture)
        , m_cid(1)
        , m_imageLabel(imageLabel)
        , m_reply(nullptr)
        , m_continuous(false)
    {
    }

public slots:
    void onCompleted(bool isComplete)
    {
        if (isComplete) {
            QObject::disconnect(connection);

            m_imageLabel->setPixmap(QPixmap::fromImage(m_reply->image()));

            ++m_cid;

            m_reply->saveToFile("capture.bmp");

            delete m_reply;
            m_reply = nullptr;

            if (m_continuous)
                capture();
        }
    }

    void setContinuous(bool continuos)
    {
        m_continuous = continuos;
    }

    void capture()
    {
        m_reply = m_capture->requestCapture(m_cid);
        connection = QObject::connect(m_reply, &Qt3DRender::QRenderCaptureReply::completeChanged,
                         this, &MyCapture::onCompleted);
    }

private:
    Qt3DRender::QRenderCapture* m_capture;
    Qt3DRender::QRenderCaptureReply *m_reply;
    QMetaObject::Connection connection;
    QLabel *m_imageLabel;
    bool m_continuous;
    int m_cid;
};

#endif

