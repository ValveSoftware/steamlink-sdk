/****************************************************************************
 **
 ** Copyright (C) 2016 Ivan Vizir <define-true-false@yandex.com>
 ** Copyright (C) 2016 The Qt Company Ltd.
 ** Contact: https://www.qt.io/licensing/
 **
 ** This file is part of the QtWinExtras module of the Qt Toolkit.
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
