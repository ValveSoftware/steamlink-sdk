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

#ifndef QSGOPENVGINTERNALRECTANGLENODE_H
#define QSGOPENVGINTERNALRECTANGLENODE_H

#include <private/qsgadaptationlayer_p.h>
#include "qsgopenvgrenderable.h"
#include "qopenvgoffscreensurface.h"

#include <VG/openvg.h>

QT_BEGIN_NAMESPACE

class QSGOpenVGInternalRectangleNode : public QSGInternalRectangleNode, public QSGOpenVGRenderable
{
public:
    QSGOpenVGInternalRectangleNode();
    ~QSGOpenVGInternalRectangleNode();

    void setRect(const QRectF &rect) override;
    void setColor(const QColor &color) override;
    void setPenColor(const QColor &color) override;
    void setPenWidth(qreal width) override;
    void setGradientStops(const QGradientStops &stops) override;
    void setRadius(qreal radius) override;
    void setAligned(bool aligned) override;
    void update() override;

    void render() override;
    void setOpacity(float opacity) override;
    void setTransform(const QOpenVGMatrix &transform) override;

private:
    void createVGResources();
    void destroyVGResources();

    void generateRectanglePath(const QRectF &rect, float radius, VGPath path) const;
    void generateRectangleAndBorderPaths(const QRectF &rect, float penWidth, float radius, VGPath inside, VGPath outside) const;
    void generateBorderPath(const QRectF &rect, float borderWidth, float borderHeight, float radius, VGPath path) const;

    bool m_pathDirty = true;
    bool m_fillDirty = true;
    bool m_strokeDirty = true;

    QRectF m_rect;
    QColor m_fillColor;
    QColor m_strokeColor;
    qreal m_penWidth = 0.0;
    qreal m_radius = 0.0;
    bool m_aligned = false;
    QGradientStops m_gradientStops;

    VGPath m_rectanglePath;
    VGPath m_borderPath;
    VGPaint m_rectanglePaint;
    VGPaint m_borderPaint;

    QOpenVGOffscreenSurface *m_offscreenSurface = nullptr;
};

QT_END_NAMESPACE

#endif // QSGOPENVGINTERNALRECTANGLENODE_H
