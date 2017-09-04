/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

#ifndef STREAM_VIDEO_NODE_H
#define STREAM_VIDEO_NODE_H

#include <QtQuick/qsgmaterial.h>
#include <QtQuick/qsgnode.h>

QT_FORWARD_DECLARE_CLASS(QSGTexture)

namespace QtWebEngineCore {

// These classes duplicate, QtQuick style, the logic of GLRenderer::DrawStreamVideoQuad.
// Their behavior should stay as close as possible to GLRenderer.

enum TextureTarget { ExternalTarget, RectangleTarget };

class StreamVideoMaterial : public QSGMaterial
{
public:
    StreamVideoMaterial(QSGTexture *texture, TextureTarget target);

    QSGMaterialType *type() const override
    {
        static QSGMaterialType theType;
        return &theType;
    }

    QSGMaterialShader *createShader() const override;

    QSGTexture *m_texture;
    QMatrix4x4 m_texMatrix;
    TextureTarget m_target;
};

class StreamVideoNode : public QSGGeometryNode
{
public:
    StreamVideoNode(QSGTexture *texture, bool flip, TextureTarget target);
    void setRect(const QRectF &rect);
    void setTextureMatrix(const QMatrix4x4 &matrix);

private:
    QSGGeometry m_geometry;
    bool m_flip;
    StreamVideoMaterial *m_material;
};

} // namespace

#endif // STREAM_VIDEO_NODE_H
