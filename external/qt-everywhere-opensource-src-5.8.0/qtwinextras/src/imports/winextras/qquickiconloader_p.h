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

#ifndef QQUICKICONLOADER_P_H
#define QQUICKICONLOADER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QObject>
#include <QVariant>
#include <QUrl>
#include <QIcon>
#include <QPixmap>
#include <QDebug>

QT_BEGIN_NAMESPACE

class QIcon;
class QQmlEngine;
class QNetworkReply;

class QQuickIconLoader
{
public:
    enum LoadResult {
        LoadOk,
        LoadError,
        LoadNetworkRequestStarted,
    };

    // Load a QIcon (pass type = QMetaType::QIcon) or a QPixmap (pass type =
    // QMetaType::QPixmap) from url. The function takes an object and member
    // function pointer to a slot accepting a QVariant. For resources that can
    // loaded synchronously ("file", "qrc" or "image"), the member function pointer
    // will be invoked immediately with the result. For network resources, it will be
    // connected to an object handling the network reply and invoked once it finishes.
    template <typename Object>
    static LoadResult load(const QUrl &url, const QQmlEngine *engine,
                           QVariant::Type type, const QSize &requestedSize,
                           Object *receiver, void (Object::*function)(const QVariant &));

private:
    QQuickIconLoader() {}
    static QVariant loadFromFile(const QUrl &url, QVariant::Type type);
    static QVariant loadFromImageProvider(const QUrl &url, const QQmlEngine *engine,
                                          QVariant::Type type, QSize requestedSize);
    static QNetworkReply *loadFromNetwork(const QUrl &url, const QQmlEngine *engine);
};

// Internal handler which loads the resource once QNetworkReply finishes
class QQuickIconLoaderNetworkReplyHandler : public QObject
{
    Q_OBJECT

public:
    explicit QQuickIconLoaderNetworkReplyHandler(QNetworkReply *reply, QVariant::Type);

Q_SIGNALS:
    void finished(const QVariant &);

private Q_SLOTS:
    void onRequestFinished();

private:
    const QVariant::Type m_type;
};

template <typename Object>
QQuickIconLoader::LoadResult
    QQuickIconLoader::load(const QUrl &url, const QQmlEngine *engine,
                           QVariant::Type type, const QSize &requestedSize,
                           Object *receiver, void (Object::*function)(const QVariant &))
{
    const QString scheme = url.scheme();
    if (scheme.startsWith(QLatin1String("http"))) {
        if (QNetworkReply *reply = QQuickIconLoader::loadFromNetwork(url, engine)) {
            QQuickIconLoaderNetworkReplyHandler *handler = new QQuickIconLoaderNetworkReplyHandler(reply, type);
            QObject::connect(handler, &QQuickIconLoaderNetworkReplyHandler::finished, receiver, function);
            return LoadNetworkRequestStarted;
        }
        return LoadError;
    }
    const QVariant resource = scheme == QLatin1String("image")
        ? QQuickIconLoader::loadFromImageProvider(url, engine, type, requestedSize)
        : QQuickIconLoader::loadFromFile(url, type); // qrc, file
    if (resource.isValid()) {
        (receiver->*function)(resource);
        return LoadOk;
    }
    return LoadError;
}

QT_END_NAMESPACE

#endif // QQUICKICONLOADER_P_H
