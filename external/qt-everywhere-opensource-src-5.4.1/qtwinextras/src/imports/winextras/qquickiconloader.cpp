/****************************************************************************
 **
 ** Copyright (C) 2013 Ivan Vizir <define-true-false@yandex.com>
 ** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
 ** Contact: http://www.qt-project.org/legal
 **
 ** This file is part of the QtWinExtras module of the Qt Toolkit.
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

#include "qquickiconloader_p.h"

#include <QQmlEngine>
#include <QNetworkAccessManager>
#include <QFileInfo>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QQuickImageProvider>
#include <QQmlFile>
#include <qt_windows.h>

QT_BEGIN_NAMESPACE

QVariant QQuickIconLoader::loadFromFile(const QUrl &url, QVariant::Type type)
{
    const QString path = QQmlFile::urlToLocalFileOrQrc(url);
    if (QFileInfo(path).exists()) {
        switch (type) {
        case QMetaType::QIcon:
            return QVariant(QIcon(path));
        case QMetaType::QPixmap:
            return QVariant(QPixmap(path));
        default:
            qWarning("%s: Unsupported type: %d", Q_FUNC_INFO, int(type));
            break;
        }
    }
    return QVariant();
}

QNetworkReply *QQuickIconLoader::loadFromNetwork(const QUrl &url, const QQmlEngine *engine)
{
    return engine->networkAccessManager()->get(QNetworkRequest(url));
}

QVariant QQuickIconLoader::loadFromImageProvider(const QUrl &url, const QQmlEngine *engine,
                                                  QVariant::Type type, QSize requestedSize)
{
    const QString providerId = url.host();
    const QString imageId = url.toString(QUrl::RemoveScheme | QUrl::RemoveAuthority).mid(1);
    QQuickImageProvider::ImageType imageType = QQuickImageProvider::Invalid;
    QQuickImageProvider *provider = static_cast<QQuickImageProvider *>(engine->imageProvider(providerId));
    QSize size;
    if (!requestedSize.isValid())
        requestedSize = QSize(GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));
    if (provider)
        imageType = provider->imageType();
    QPixmap pixmap;
    switch (imageType) {
    case QQuickImageProvider::Image: {
        QImage image = provider->requestImage(imageId, &size, requestedSize);
        if (!image.isNull())
            pixmap = QPixmap::fromImage(image);
    }
        break;
    case QQuickImageProvider::Pixmap:
        pixmap = provider->requestPixmap(imageId, &size, requestedSize);
        break;
    default:
        break;
    }
    if (!pixmap.isNull()) {
        switch (type) {
        case QMetaType::QIcon:
            return QVariant(QIcon(pixmap));
        case QMetaType::QPixmap:
            return QVariant(pixmap);
        default:
            qWarning("%s: Unsupported type: %d", Q_FUNC_INFO, int(type));
            break;
        }
    }
    return QVariant();
}

QQuickIconLoaderNetworkReplyHandler::QQuickIconLoaderNetworkReplyHandler(QNetworkReply *reply, QVariant::Type type)
    : QObject(reply)
    , m_type(type)
{
    connect(reply, &QNetworkReply::finished, this, &QQuickIconLoaderNetworkReplyHandler::onRequestFinished);
}

void QQuickIconLoaderNetworkReplyHandler::onRequestFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    Q_ASSERT(reply);
    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << Q_FUNC_INFO << reply->url() << "failed:" << reply->errorString();
        return;
    }
    const QByteArray data = reply->readAll();
    QPixmap pixmap;
    if (pixmap.loadFromData(data)) {
        switch (m_type) {
        case QMetaType::QIcon:
            emit finished(QVariant(QIcon(pixmap)));
            break;
        case QMetaType::QPixmap:
            emit finished(QVariant(pixmap));
            break;
        default:
            qWarning("%s: Unsupported type: %d", Q_FUNC_INFO, int(m_type));
            break;
        }
    }
    reply->deleteLater();
}

QT_END_NAMESPACE
