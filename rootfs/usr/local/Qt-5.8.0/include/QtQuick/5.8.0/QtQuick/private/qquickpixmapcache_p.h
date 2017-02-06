/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#ifndef QQUICKPIXMAPCACHE_H
#define QQUICKPIXMAPCACHE_H

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

#include <QtCore/qcoreapplication.h>
#include <QtCore/qstring.h>
#include <QtGui/qpixmap.h>
#include <QtCore/qurl.h>
#include <private/qtquickglobal_p.h>
#include <QtQuick/qquickimageprovider.h>

#include <private/qintrusivelist_p.h>

QT_BEGIN_NAMESPACE

class QQmlEngine;
class QQuickPixmapData;
class QQuickTextureFactory;

enum AutoTransform {
    UsePluginDefault = -1,
    ApplyTransform = 0,
    DoNotApplyTransform = 1
};

class QQuickDefaultTextureFactory : public QQuickTextureFactory
{
    Q_OBJECT
public:
    QQuickDefaultTextureFactory(const QImage &i);
    QSGTexture *createTexture(QQuickWindow *window) const;
    QSize textureSize() const { return size; }
    int textureByteCount() const { return size.width() * size.height() * 4; }
    QImage image() const { return im; }

private:
    QImage im;
    QSize size;
};

class Q_QUICK_PRIVATE_EXPORT QQuickPixmap
{
    Q_DECLARE_TR_FUNCTIONS(QQuickPixmap)
public:
    QQuickPixmap();
    QQuickPixmap(QQmlEngine *, const QUrl &);
    QQuickPixmap(QQmlEngine *, const QUrl &, const QSize &);
    QQuickPixmap(const QUrl &, const QImage &image);
    ~QQuickPixmap();

    enum Status { Null, Ready, Error, Loading };

    enum Option {
        Asynchronous = 0x00000001,
        Cache        = 0x00000002
    };
    Q_DECLARE_FLAGS(Options, Option)

    bool isNull() const;
    bool isReady() const;
    bool isError() const;
    bool isLoading() const;

    Status status() const;
    QString error() const;
    const QUrl &url() const;
    const QSize &implicitSize() const;
    const QSize &requestSize() const;
    AutoTransform autoTransform() const;
    QImage image() const;
    void setImage(const QImage &);
    void setPixmap(const QQuickPixmap &other);

    QQuickTextureFactory *textureFactory() const;

    QRect rect() const;
    int width() const;
    int height() const;

    void load(QQmlEngine *, const QUrl &);
    void load(QQmlEngine *, const QUrl &, QQuickPixmap::Options options);
    void load(QQmlEngine *, const QUrl &, const QSize &);
    void load(QQmlEngine *, const QUrl &, const QSize &, QQuickPixmap::Options options);
    void load(QQmlEngine *, const QUrl &, const QSize &, QQuickPixmap::Options options, AutoTransform autoTransform);

    void clear();
    void clear(QObject *);

    bool connectFinished(QObject *, const char *);
    bool connectFinished(QObject *, int);
    bool connectDownloadProgress(QObject *, const char *);
    bool connectDownloadProgress(QObject *, int);

    static void purgeCache();
    static bool isCached(const QUrl &url, const QSize &requestSize);

private:
    Q_DISABLE_COPY(QQuickPixmap)
    QQuickPixmapData *d;
    QIntrusiveListNode dataListNode;
    friend class QQuickPixmapData;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QQuickPixmap::Options)

QT_END_NAMESPACE

#endif // QQUICKPIXMAPCACHE_H
