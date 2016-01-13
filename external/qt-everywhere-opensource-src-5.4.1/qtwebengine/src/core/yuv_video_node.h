/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef YUV_VIDEO_NODE_H
#define YUV_VIDEO_NODE_H

#include <QtQuick/qsgmaterial.h>
#include <QtQuick/qsgnode.h>

QT_BEGIN_NAMESPACE
class QSGTexture;
QT_END_NAMESPACE

// These classes duplicate, QtQuick style, the logic of GLRenderer::DrawYUVVideoQuad.
// Their behavior should stay as close as possible to GLRenderer.

class YUVVideoMaterial : public QSGMaterial
{
public:
    YUVVideoMaterial(QSGTexture *yTexture, QSGTexture *uTexture, QSGTexture *vTexture, const QRectF &texCoordRect);

    virtual QSGMaterialType *type() const Q_DECL_OVERRIDE {
        static QSGMaterialType theType;
        return &theType;
    }

    virtual QSGMaterialShader *createShader() const Q_DECL_OVERRIDE;
    virtual int compare(const QSGMaterial *other) const Q_DECL_OVERRIDE;

    QSGTexture *m_yTexture;
    QSGTexture *m_uTexture;
    QSGTexture *m_vTexture;
    QRectF m_texCoordRect;
};

class YUVAVideoMaterial : public YUVVideoMaterial
{
public:
    YUVAVideoMaterial(QSGTexture *yTexture, QSGTexture *uTexture, QSGTexture *vTexture, QSGTexture *aTexture, const QRectF &texCoordRect);

    virtual QSGMaterialType *type() const Q_DECL_OVERRIDE{
        static QSGMaterialType theType;
        return &theType;
    }

    virtual QSGMaterialShader *createShader() const Q_DECL_OVERRIDE;
    virtual int compare(const QSGMaterial *other) const Q_DECL_OVERRIDE;

    QSGTexture *m_aTexture;
};

class YUVVideoNode : public QSGGeometryNode
{
public:
    YUVVideoNode(QSGTexture *yTexture, QSGTexture *uTexture, QSGTexture *vTexture, QSGTexture *aTexture, const QRectF &texCoordRect);
    void setRect(const QRectF &rect);

private:
    QSGGeometry m_geometry;
    YUVVideoMaterial *m_material;
};

#endif // YUV_VIDEO_NODE_H
