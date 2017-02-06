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

#ifndef QSGTEXTURE_P_H
#define QSGTEXTURE_P_H

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

#include <QtQuick/qtquickglobal.h>
#include <private/qobject_p.h>
#ifndef QT_NO_OPENGL
# include <QtGui/qopengl.h>
#endif
#include "qsgtexture.h"
#include <QtQuick/private/qsgcontext_p.h>

QT_BEGIN_NAMESPACE

class QSGTexturePrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QSGTexture)
public:
    QSGTexturePrivate();

    uint wrapChanged : 1;
    uint filteringChanged : 1;

    uint horizontalWrap : 1;
    uint verticalWrap : 1;
    uint mipmapMode : 2;
    uint filterMode : 2;
};

class Q_QUICK_PRIVATE_EXPORT QSGPlainTexture : public QSGTexture
{
    Q_OBJECT
public:
    QSGPlainTexture();
    virtual ~QSGPlainTexture();

    void setOwnsTexture(bool owns) { m_owns_texture = owns; }
    bool ownsTexture() const { return m_owns_texture; }

    void setTextureId(int id);
    int textureId() const;
    void setTextureSize(const QSize &size) { m_texture_size = size; }
    QSize textureSize() const { return m_texture_size; }

    void setHasAlphaChannel(bool alpha) { m_has_alpha = alpha; }
    bool hasAlphaChannel() const { return m_has_alpha; }

    bool hasMipmaps() const { return mipmapFiltering() != QSGTexture::None; }

    void setImage(const QImage &image);
    const QImage &image() { return m_image; }

    virtual void bind();

    static QSGPlainTexture *fromImage(const QImage &image) {
        QSGPlainTexture *t = new QSGPlainTexture();
        t->setImage(image);
        return t;
    }

protected:
    QImage m_image;

    uint m_texture_id;
    QSize m_texture_size;
    QRectF m_texture_rect;

    uint m_has_alpha : 1;
    uint m_dirty_texture : 1;
    uint m_dirty_bind_options : 1;
    uint m_owns_texture : 1;
    uint m_mipmaps_generated : 1;
    uint m_retain_image: 1;
};

Q_QUICK_PRIVATE_EXPORT bool qsg_safeguard_texture(QSGTexture *);

QT_END_NAMESPACE

#endif // QSGTEXTURE_P_H
