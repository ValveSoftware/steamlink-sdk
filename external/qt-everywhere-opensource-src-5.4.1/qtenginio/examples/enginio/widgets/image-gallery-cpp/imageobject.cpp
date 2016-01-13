/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
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
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
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

#include "imageobject.h"

#include <QtNetwork>
#include <Enginio/enginioclient.h>
#include <Enginio/enginioreply.h>

ImageObject::ImageObject(EnginioClient *enginio)
    : m_enginio(enginio)
{}

void ImageObject::setObject(const QJsonObject &object)
{
    m_object = object;
    QString fileId;
    fileId = object.value("file").toObject().value("id").toString();

    if (!fileId.isEmpty()) {
        QJsonObject fileObject;
        fileObject.insert("id", fileId);
        fileObject.insert("variant", QString("thumbnail"));
        EnginioReply *reply = m_enginio->downloadUrl(fileObject);
        connect(reply, SIGNAL(finished(EnginioReply*)), this, SLOT(replyFinished(EnginioReply*)));
    } else {
        // Try to fall back to the local file
        QString localPath = object.value("localPath").toString();
        if (QFile::exists(localPath)) {
            m_image = QImage(localPath);
            emit imageChanged(object.value("id").toString());
        }
    }
}

void ImageObject::replyFinished(EnginioReply *enginioReply)
{
    QString url = enginioReply->data().value("expiringUrl").toString();
    QNetworkRequest request(url);
    m_reply = m_enginio->networkManager()->get(request);
    m_reply->setParent(this);
    connect(m_reply, SIGNAL(finished()), this, SLOT(downloadFinished()));
}

void ImageObject::downloadFinished()
{
    QByteArray imageData = m_reply->readAll();
    m_image.loadFromData(imageData);
    emit imageChanged(m_object.value("id").toString());

    m_reply->deleteLater();
    m_reply = 0;
}

QPixmap ImageObject::thumbnail()
{
    if (m_image.isNull() || !m_thumbnail.size().isNull())
        return m_thumbnail;
    m_thumbnail = QPixmap::fromImage(m_image.scaled(100, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    return m_thumbnail;
}
