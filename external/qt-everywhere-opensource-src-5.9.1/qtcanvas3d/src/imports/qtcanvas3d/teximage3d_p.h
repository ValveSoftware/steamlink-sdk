/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCanvas3D module of the Qt Toolkit.
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

//
//  W A R N I N G
//  -------------
//
// This file is not part of the QtCanvas3D API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.

#ifndef TEXIMAGE3D_P_H
#define TEXIMAGE3D_P_H

#include "context3d_p.h"
#include "abstractobject3d_p.h"

#include <QtCore/QUrl>
#include <QtCore/QPointer>
#include <QtGui/QImage>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

QT_BEGIN_NAMESPACE

QT_CANVAS3D_BEGIN_NAMESPACE

class CanvasTextureImageFactory : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(CanvasTextureImageFactory)

public:
    static QObject *texture_image_factory_provider(QQmlEngine *engine, QJSEngine *scriptEngine);
    static CanvasTextureImageFactory *factory(QQmlEngine *engine);
    explicit CanvasTextureImageFactory(QQmlEngine *engine, QObject *parent = 0);
    ~CanvasTextureImageFactory();

    void handleImageLoadingStarted(CanvasTextureImage *image);
    void notifyLoadedImages();

    Q_INVOKABLE QJSValue newTexImage();
    void handleImageDestroyed(CanvasTextureImage *image);

private:
    QQmlEngine *m_qmlEngine;
    QList<CanvasTextureImage *> m_loadingImagesList;
};

class CanvasTextureImage : public CanvasAbstractObject
{
    Q_OBJECT
    Q_DISABLE_COPY(CanvasTextureImage)
    Q_PROPERTY(QUrl src READ src WRITE setSrc NOTIFY srcChanged)
    Q_PROPERTY(TextureImageState imageState READ imageState NOTIFY imageStateChanged)
    Q_PROPERTY(int width READ width NOTIFY widthChanged)
    Q_PROPERTY(int height READ height NOTIFY heightChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_ENUMS(TextureImageState)

public:
    enum TextureImageState {
        INITIALIZED = 0,
        LOAD_PENDING,
        LOADING,
        LOADING_FINISHED,
        LOADING_ERROR
    };

    explicit CanvasTextureImage(CanvasTextureImageFactory *parent, QQmlEngine *engine);
    virtual ~CanvasTextureImage();

    Q_INVOKABLE QJSValue create();
    Q_INVOKABLE ulong id();
    Q_INVOKABLE QJSValue resize(int width, int height);

    QVariant *anything() const;
    void setAnything(QVariant *value);

    const QUrl &src() const;
    void setSrc(const QUrl &src);
    TextureImageState imageState() const;
    int width() const;
    int height() const;
    QString errorString() const;

    void emitImageLoaded();
    void emitImageLoadingError();

    void load();
    void handleReply();
    QImage &getImage();
    uchar *convertToFormat(CanvasContext::glEnums format, bool flipY = false, bool premultipliedAlpha = false);

    friend QDebug operator<< (QDebug d, const CanvasTextureImage *buffer);

private:
    void setImageState(TextureImageState state);
    explicit CanvasTextureImage(const QImage &source, int width, int height,
                                CanvasTextureImageFactory *parent, QQmlEngine *engine);

signals:
    void srcChanged(QUrl source);
    void imageStateChanged(TextureImageState state);
    void widthChanged(int width);
    void heightChanged(int height);
    void errorStringChanged(const QString errorString);
    void anythingChanged(QVariant *value);
    void imageLoadingStarted(CanvasTextureImage *image);
    void imageLoaded(CanvasTextureImage *image);
    void imageLoadingFailed(CanvasTextureImage *image);

private:
    void cleanupNetworkReply();

    QQmlEngine *m_engine;
    QNetworkAccessManager *m_networkAccessManager; // not owned
    QNetworkReply *m_networkReply;
    QImage m_image;
    QUrl m_source;
    TextureImageState m_state;
    QString m_errorString;
    uchar *m_pixelCache;
    CanvasContext::glEnums m_pixelCacheFormat;
    bool m_pixelCacheFlipY;
    QImage m_glImage;
    QVariant *m_anyValue;
    QPointer<CanvasTextureImageFactory> m_parentFactory;
};

QT_CANVAS3D_END_NAMESPACE

QT_END_NAMESPACE

#endif // TEXIMAGE3D_P_H
