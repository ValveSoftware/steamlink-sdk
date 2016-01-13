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

#ifndef QQUICKICONLOADER_P_H
#define QQUICKICONLOADER_P_H

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
