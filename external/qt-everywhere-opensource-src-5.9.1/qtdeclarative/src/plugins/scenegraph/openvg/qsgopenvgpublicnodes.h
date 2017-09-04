#ifndef QSGOPENVGPUBLICNODES_H
#define QSGOPENVGPUBLICNODES_H

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

#include <QtQuick/qsgrectanglenode.h>
#include <QtQuick/qsgimagenode.h>
#include <QtQuick/qsgninepatchnode.h>

#include "qsgopenvgrenderable.h"

QT_BEGIN_NAMESPACE

class QSGOpenVGRectangleNode : public QSGRectangleNode, public QSGOpenVGRenderable
{
public:
    QSGOpenVGRectangleNode();
    ~QSGOpenVGRectangleNode();

    void setRect(const QRectF &rect) override;
    QRectF rect() const override;

    void setColor(const QColor &color) override;
    QColor color() const override;

    void setTransform(const QOpenVGMatrix &transform) override;

    void render() override;

private:
    QRectF m_rect;
    QColor m_color;


    bool m_pathDirty = true;
    bool m_paintDirty = true;

    VGPath m_rectPath;
    VGPaint m_rectPaint;
};

class QSGOpenVGImageNode : public QSGImageNode, public QSGOpenVGRenderable
{
public:
    QSGOpenVGImageNode();
    ~QSGOpenVGImageNode();

    void setRect(const QRectF &rect) override;
    QRectF rect() const override;

    void setSourceRect(const QRectF &r) override;
    QRectF sourceRect() const override;

    void setTexture(QSGTexture *texture) override;
    QSGTexture *texture() const override;

    void setFiltering(QSGTexture::Filtering filtering) override;
    QSGTexture::Filtering filtering() const override;

    void setMipmapFiltering(QSGTexture::Filtering) override;
    QSGTexture::Filtering mipmapFiltering() const override;

    void setTextureCoordinatesTransform(TextureCoordinatesTransformMode transformNode) override;
    TextureCoordinatesTransformMode textureCoordinatesTransform() const override;

    void setOwnsTexture(bool owns) override;
    bool ownsTexture() const override;

    void render() override;

private:
    QSGTexture *m_texture;
    QRectF m_rect;
    QRectF m_sourceRect;
    bool m_owns;
    QSGTexture::Filtering m_filtering;
    TextureCoordinatesTransformMode m_transformMode;
};

class QSGOpenVGNinePatchNode : public QSGNinePatchNode, public QSGOpenVGRenderable
{
public:
    QSGOpenVGNinePatchNode();

    void setTexture(QSGTexture *texture) override;
    void setBounds(const QRectF &bounds) override;
    void setDevicePixelRatio(qreal ratio) override;
    void setPadding(qreal left, qreal top, qreal right, qreal bottom) override;
    void update() override;

    void render() override;

    QRectF bounds() const;

private:
    QSGTexture *m_texture;
    QRectF m_bounds;
    qreal m_pixelRatio;
    QMarginsF m_margins;
};

QT_END_NAMESPACE

#endif // QSGOPENVGPUBLICNODES_H
