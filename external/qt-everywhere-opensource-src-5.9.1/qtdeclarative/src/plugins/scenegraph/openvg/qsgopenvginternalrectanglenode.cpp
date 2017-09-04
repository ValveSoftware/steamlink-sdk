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

#include "qsgopenvginternalrectanglenode.h"
#include "qsgopenvghelpers.h"
#include <cmath>
#include <VG/vgu.h>

QSGOpenVGInternalRectangleNode::QSGOpenVGInternalRectangleNode()
{
    // Set Dummy material and geometry to avoid asserts
    setMaterial((QSGMaterial*)1);
    setGeometry((QSGGeometry*)1);
    createVGResources();
}

QSGOpenVGInternalRectangleNode::~QSGOpenVGInternalRectangleNode()
{
    destroyVGResources();
}


void QSGOpenVGInternalRectangleNode::setRect(const QRectF &rect)
{
    m_rect = rect;
    m_pathDirty = true;
}

void QSGOpenVGInternalRectangleNode::setColor(const QColor &color)
{
    m_fillColor = color;
    m_fillDirty = true;
}

void QSGOpenVGInternalRectangleNode::setPenColor(const QColor &color)
{
    m_strokeColor = color;
    m_strokeDirty = true;
}

void QSGOpenVGInternalRectangleNode::setPenWidth(qreal width)
{
    m_penWidth = width;
    m_strokeDirty = true;
    m_pathDirty = true;
}

//Move first stop by pos relative to seconds
static QGradientStop interpolateStop(const QGradientStop &firstStop, const QGradientStop &secondStop, double newPos)
{
    double distance = secondStop.first - firstStop.first;
    double distanceDelta = newPos - firstStop.first;
    double modifierValue = distanceDelta / distance;
    int redDelta = (secondStop.second.red() - firstStop.second.red()) * modifierValue;
    int greenDelta = (secondStop.second.green() - firstStop.second.green()) * modifierValue;
    int blueDelta = (secondStop.second.blue() - firstStop.second.blue()) * modifierValue;
    int alphaDelta = (secondStop.second.alpha() - firstStop.second.alpha()) * modifierValue;

    QGradientStop newStop;
    newStop.first = newPos;
    newStop.second = QColor(firstStop.second.red() + redDelta,
                            firstStop.second.green() + greenDelta,
                            firstStop.second.blue() + blueDelta,
                            firstStop.second.alpha() + alphaDelta);

    return newStop;
}

void QSGOpenVGInternalRectangleNode::setGradientStops(const QGradientStops &stops)
{

    //normalize stops
    bool needsNormalization = false;
    for (const QGradientStop &stop : qAsConst(stops)) {
        if (stop.first < 0.0 || stop.first > 1.0) {
            needsNormalization = true;
            continue;
        }
    }

    if (needsNormalization) {
        QGradientStops normalizedStops;
        if (stops.count() == 1) {
            //If there is only one stop, then the position does not matter
            //It is just treated as a color
            QGradientStop stop = stops.at(0);
            stop.first = 0.0;
            normalizedStops.append(stop);
        } else {
            //Clip stops to only the first below 0.0 and above 1.0
            int below = -1;
            int above = -1;
            QVector<int> between;
            for (int i = 0; i < stops.count(); ++i) {
                if (stops.at(i).first < 0.0) {
                    below = i;
                } else if (stops.at(i).first > 1.0) {
                    above = i;
                    break;
                } else {
                    between.append(i);
                }
            }

            //Interpoloate new color values for above and below
            if (below != -1 ) {
                //If there are more than one stops left, interpolate
                if (below + 1 < stops.count()) {
                    normalizedStops.append(interpolateStop(stops.at(below), stops.at(below + 1), 0.0));
                } else {
                    QGradientStop singleStop;
                    singleStop.first = 0.0;
                    singleStop.second = stops.at(below).second;
                    normalizedStops.append(singleStop);
                }
            }

            for (int i = 0; i < between.count(); ++i)
                normalizedStops.append(stops.at(between.at(i)));

            if (above != -1) {
                //If there stops before above, interpolate
                if (above >= 1) {
                    normalizedStops.append(interpolateStop(stops.at(above), stops.at(above - 1), 1.0));
                } else {
                    QGradientStop singleStop;
                    singleStop.first = 1.0;
                    singleStop.second = stops.at(above).second;
                    normalizedStops.append(singleStop);
                }
            }
        }

        m_gradientStops = normalizedStops;

    } else {
        m_gradientStops = stops;
    }

    m_fillDirty = true;
}

void QSGOpenVGInternalRectangleNode::setRadius(qreal radius)
{
    m_radius = radius;
    m_pathDirty = true;
}

void QSGOpenVGInternalRectangleNode::setAligned(bool aligned)
{
    m_aligned = aligned;
}

void QSGOpenVGInternalRectangleNode::update()
{
}

void QSGOpenVGInternalRectangleNode::render()
{
    // Set Transform
    if (transform().isAffine()) {
        // Use current transform matrix
        vgSeti(VG_MATRIX_MODE, VG_MATRIX_PATH_USER_TO_SURFACE);
        vgLoadMatrix(transform().constData());
        if (m_offscreenSurface) {
            delete m_offscreenSurface;
            m_offscreenSurface = nullptr;
        }
    } else {
        vgSeti(VG_MATRIX_MODE, VG_MATRIX_PATH_USER_TO_SURFACE);
        vgLoadIdentity();
        if (m_radius > 0) {
            // Fallback to rendering to an image for rounded rects with perspective transforms
            if (m_offscreenSurface == nullptr || m_offscreenSurface->size() != QSize(std::ceil(m_rect.width()), std::ceil(m_rect.height()))) {
                delete m_offscreenSurface;
                m_offscreenSurface = new QOpenVGOffscreenSurface(QSize(std::ceil(m_rect.width()), std::ceil(m_rect.height())));
            }

            m_offscreenSurface->makeCurrent();
        } else if (m_offscreenSurface) {
            delete m_offscreenSurface;
            m_offscreenSurface = nullptr;
        }
    }


    // If path is dirty
    if (m_pathDirty) {
        vgClearPath(m_rectanglePath, VG_PATH_CAPABILITY_APPEND_TO);
        vgClearPath(m_borderPath, VG_PATH_CAPABILITY_APPEND_TO);

        if (m_penWidth == 0) {
            generateRectanglePath(m_rect, m_radius, m_rectanglePath);
        } else {
            generateRectangleAndBorderPaths(m_rect, m_penWidth, m_radius, m_rectanglePath, m_borderPath);
        }

        m_pathDirty = false;
    }

    //If fill is drity
    if (m_fillDirty) {
        if (m_gradientStops.isEmpty()) {
            vgSetParameteri(m_rectanglePaint, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
            vgSetParameterfv(m_rectanglePaint, VG_PAINT_COLOR, 4, QSGOpenVGHelpers::qColorToVGColor(m_fillColor, opacity()).constData());
        } else {
            // Linear Gradient
            vgSetParameteri(m_rectanglePaint, VG_PAINT_TYPE, VG_PAINT_TYPE_LINEAR_GRADIENT);
            const VGfloat verticalLinearGradient[] = {
                0.0f,
                0.0f,
                0.0f,
                static_cast<VGfloat>(m_rect.height())
            };
            vgSetParameterfv(m_rectanglePaint, VG_PAINT_LINEAR_GRADIENT, 4, verticalLinearGradient);
            vgSetParameteri(m_rectanglePaint, VG_PAINT_COLOR_RAMP_SPREAD_MODE, VG_COLOR_RAMP_SPREAD_PAD);
            vgSetParameteri(m_rectanglePaint, VG_PAINT_COLOR_RAMP_PREMULTIPLIED, false);

            QVector<VGfloat> stops;
            for (const QGradientStop &stop : qAsConst(m_gradientStops)) {
                // offset
                stops.append(stop.first);
                // color
                stops.append(QSGOpenVGHelpers::qColorToVGColor(stop.second, opacity()));
            }

            vgSetParameterfv(m_rectanglePaint, VG_PAINT_COLOR_RAMP_STOPS, stops.length(), stops.constData());
        }

        m_fillDirty = false;
    }

    //If stroke is dirty
    if (m_strokeDirty) {
        vgSetParameteri(m_borderPaint, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
        vgSetParameterfv(m_borderPaint, VG_PAINT_COLOR, 4, QSGOpenVGHelpers::qColorToVGColor(m_strokeColor, opacity()).constData());

        m_strokeDirty = false;
    }

    //Draw
    if (m_penWidth > 0) {
        vgSetPaint(m_borderPaint, VG_FILL_PATH);
        vgDrawPath(m_borderPath, VG_FILL_PATH);
        vgSetPaint(m_rectanglePaint, VG_FILL_PATH);
        vgDrawPath(m_rectanglePath, VG_FILL_PATH);
    } else {
        vgSetPaint(m_rectanglePaint, VG_FILL_PATH);
        vgDrawPath(m_rectanglePath, VG_FILL_PATH);
    }

    if (!transform().isAffine() && m_radius > 0) {
        m_offscreenSurface->doneCurrent();
        //  Render offscreen surface
        vgSeti(VG_MATRIX_MODE, VG_MATRIX_IMAGE_USER_TO_SURFACE);
        vgLoadMatrix(transform().constData());
        vgDrawImage(m_offscreenSurface->image());
    }
}

void QSGOpenVGInternalRectangleNode::setOpacity(float opacity)
{
    if (opacity != QSGOpenVGRenderable::opacity()) {
        QSGOpenVGRenderable::setOpacity(opacity);
        m_strokeDirty = true;
        m_fillDirty = true;
    }
}

void QSGOpenVGInternalRectangleNode::setTransform(const QOpenVGMatrix &transform)
{
    // if there transform matrix is not affine, regenerate the path
    if (transform.isAffine())
        m_pathDirty = true;

    QSGOpenVGRenderable::setTransform(transform);
}

void QSGOpenVGInternalRectangleNode::createVGResources()
{
    m_rectanglePaint = vgCreatePaint();
    m_borderPaint = vgCreatePaint();
    m_rectanglePath = vgCreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F, 1, 0, 0, 0,
                                   VG_PATH_CAPABILITY_APPEND_TO);
    m_borderPath = vgCreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F, 1, 0, 0, 0,
                                   VG_PATH_CAPABILITY_APPEND_TO);
}

void QSGOpenVGInternalRectangleNode::destroyVGResources()
{
    if (m_offscreenSurface)
        delete m_offscreenSurface;

    vgDestroyPaint(m_rectanglePaint);
    vgDestroyPaint(m_borderPaint);
    vgDestroyPath(m_rectanglePath);
    vgDestroyPath(m_borderPath);
}

void QSGOpenVGInternalRectangleNode::generateRectanglePath(const QRectF &rect, float radius, VGPath path) const
{
    if (radius == 0) {
        // Generate a rectangle
        if (transform().isAffine()) {
            // Create command list
            static const VGubyte rectCommands[] = {
                VG_MOVE_TO_ABS,
                VG_HLINE_TO_REL,
                VG_VLINE_TO_REL,
                VG_HLINE_TO_REL,
                VG_CLOSE_PATH
            };

            // Create command data
            QVector<VGfloat> coordinates(5);
            coordinates[0] = rect.x();
            coordinates[1] = rect.y();
            coordinates[2] = rect.width();
            coordinates[3] = rect.height();
            coordinates[4] = -rect.width();
            vgAppendPathData(path, 5, rectCommands, coordinates.constData());
        } else {
            // Pre-transform path
            static const VGubyte rectCommands[] = {
                VG_MOVE_TO_ABS,
                VG_LINE_TO_ABS,
                VG_LINE_TO_ABS,
                VG_LINE_TO_ABS,
                VG_CLOSE_PATH
            };

            QVector<VGfloat> coordinates(8);
            const QPointF topLeft = transform().map(rect.topLeft());
            const QPointF topRight = transform().map(rect.topRight());
            const QPointF bottomLeft = transform().map(rect.bottomLeft());
            const QPointF bottomRight = transform().map(rect.bottomRight());
            coordinates[0] = bottomLeft.x();
            coordinates[1] = bottomLeft.y();
            coordinates[2] = bottomRight.x();
            coordinates[3] = bottomRight.y();
            coordinates[4] = topRight.x();
            coordinates[5] = topRight.y();
            coordinates[6] = topLeft.x();
            coordinates[7] = topLeft.y();

            vgAppendPathData(path, 5, rectCommands, coordinates.constData());
        }
    } else {
        // Generate a rounded rectangle
        //Radius should never exceeds half of the width or half of the height
        float adjustedRadius = qMin((float)qMin(rect.width(), rect.height()) * 0.5f, radius);

        // OpenVG expectes radius to be 2x what we expect
        adjustedRadius *= 2;

        // Create command list
        static const VGubyte roundedRectCommands[] = {
            VG_MOVE_TO_ABS,
            VG_HLINE_TO_REL,
            VG_SCCWARC_TO_REL,
            VG_VLINE_TO_REL,
            VG_SCCWARC_TO_REL,
            VG_HLINE_TO_REL,
            VG_SCCWARC_TO_REL,
            VG_VLINE_TO_REL,
            VG_SCCWARC_TO_REL,
            VG_CLOSE_PATH
        };

        // Create command data
        QVector<VGfloat> coordinates(26);

        coordinates[0] =  rect.x() + adjustedRadius / 2;
        coordinates[1] =  rect.y();

        coordinates[2] = rect.width() - adjustedRadius;

        coordinates[3] = adjustedRadius / 2;
        coordinates[4] = adjustedRadius / 2;
        coordinates[5] = 0;
        coordinates[6] = adjustedRadius / 2;
        coordinates[7] = adjustedRadius / 2;

        coordinates[8] = rect.height() - adjustedRadius;

        coordinates[9] = adjustedRadius / 2;
        coordinates[10] = adjustedRadius / 2;
        coordinates[11] = 0;
        coordinates[12] = -adjustedRadius / 2;
        coordinates[13] = adjustedRadius / 2;

        coordinates[14] = -(rect.width() - adjustedRadius);

        coordinates[15] = adjustedRadius / 2;
        coordinates[16] = adjustedRadius / 2;
        coordinates[17] = 0;
        coordinates[18] = -adjustedRadius / 2;
        coordinates[19] = -adjustedRadius / 2;

        coordinates[20] = -(rect.height() - adjustedRadius);

        coordinates[21] = adjustedRadius / 2;
        coordinates[22] = adjustedRadius / 2;
        coordinates[23] = 0;
        coordinates[24] = adjustedRadius / 2;
        coordinates[25] = -adjustedRadius / 2;

        vgAppendPathData(path, 10, roundedRectCommands, coordinates.constData());
    }
}

void QSGOpenVGInternalRectangleNode::generateBorderPath(const QRectF &rect, float borderWidth, float borderHeight, float radius, VGPath path) const
{
    if (radius == 0) {
        // squared frame
        if (transform().isAffine()) {
            // Create command list
            static const VGubyte squaredBorderCommands[] = {
                VG_MOVE_TO_ABS,
                VG_HLINE_TO_REL,
                VG_VLINE_TO_REL,
                VG_HLINE_TO_REL,
                VG_MOVE_TO_ABS,
                VG_VLINE_TO_REL,
                VG_HLINE_TO_REL,
                VG_VLINE_TO_REL,
                VG_CLOSE_PATH
            };

            // Create command data
            QVector<VGfloat> coordinates(10);
            // Outside Square
            coordinates[0] = rect.x();
            coordinates[1] = rect.y();
            coordinates[2] = rect.width();
            coordinates[3] = rect.height();
            coordinates[4] = -rect.width();
            // Inside Square (opposite direction)
            coordinates[5] = rect.x() + borderWidth;
            coordinates[6] = rect.y() + borderHeight;
            coordinates[7] = rect.height() - (borderHeight * 2);
            coordinates[8] = rect.width() - (borderWidth * 2);
            coordinates[9] = -(rect.height() - (borderHeight * 2));

            vgAppendPathData(path, 9, squaredBorderCommands, coordinates.constData());
        } else {
            // persepective transform
            static const VGubyte squaredBorderCommands[] = {
                VG_MOVE_TO_ABS,
                VG_LINE_TO_ABS,
                VG_LINE_TO_ABS,
                VG_LINE_TO_ABS,
                VG_MOVE_TO_ABS,
                VG_LINE_TO_ABS,
                VG_LINE_TO_ABS,
                VG_LINE_TO_ABS,
                VG_CLOSE_PATH
            };

            QVector<VGfloat> coordinates(16);
            QRectF insideRect = rect.marginsRemoved(QMarginsF(borderWidth, borderHeight, borderWidth, borderHeight));
            QPointF outsideBottomLeft = transform().map(rect.bottomLeft());
            QPointF outsideBottomRight = transform().map(rect.bottomRight());
            QPointF outsideTopRight = transform().map(rect.topRight());
            QPointF outsideTopLeft = transform().map(rect.topLeft());
            QPointF insideBottomLeft = transform().map(insideRect.bottomLeft());
            QPointF insideTopLeft = transform().map(insideRect.topLeft());
            QPointF insideTopRight = transform().map(insideRect.topRight());
            QPointF insideBottomRight = transform().map(insideRect.bottomRight());

            // Outside
            coordinates[0] = outsideBottomLeft.x();
            coordinates[1] = outsideBottomLeft.y();
            coordinates[2] = outsideBottomRight.x();
            coordinates[3] = outsideBottomRight.y();
            coordinates[4] = outsideTopRight.x();
            coordinates[5] = outsideTopRight.y();
            coordinates[6] = outsideTopLeft.x();
            coordinates[7] = outsideTopLeft.y();
            // Inside
            coordinates[8] = insideBottomLeft.x();
            coordinates[9] = insideBottomLeft.y();
            coordinates[10] = insideTopLeft.x();
            coordinates[11] = insideTopLeft.y();
            coordinates[12] = insideTopRight.x();
            coordinates[13] = insideTopRight.y();
            coordinates[14] = insideBottomRight.x();
            coordinates[15] = insideBottomRight.y();

            vgAppendPathData(path, 9, squaredBorderCommands, coordinates.constData());
        }
    } else if (radius < qMax(borderWidth, borderHeight)){
        // rounded outside, squared inside
        // Create command list
        static const VGubyte roundedRectCommands[] = {
            VG_MOVE_TO_ABS,
            VG_HLINE_TO_REL,
            VG_SCCWARC_TO_REL,
            VG_VLINE_TO_REL,
            VG_SCCWARC_TO_REL,
            VG_HLINE_TO_REL,
            VG_SCCWARC_TO_REL,
            VG_VLINE_TO_REL,
            VG_SCCWARC_TO_REL,
            VG_MOVE_TO_ABS,
            VG_VLINE_TO_REL,
            VG_HLINE_TO_REL,
            VG_VLINE_TO_REL,
            VG_CLOSE_PATH
        };

        // Ajust for OpenVG's usage or radius
        float adjustedRadius = radius * 2;

        // Create command data
        QVector<VGfloat> coordinates(31);
        // Outside Rounded Rect
        coordinates[0] =  rect.x() + adjustedRadius / 2;
        coordinates[1] =  rect.y();

        coordinates[2] = rect.width() - adjustedRadius;

        coordinates[3] = adjustedRadius / 2;
        coordinates[4] = adjustedRadius / 2;
        coordinates[5] = 0;
        coordinates[6] = adjustedRadius / 2;
        coordinates[7] = adjustedRadius / 2;

        coordinates[8] = rect.height() - adjustedRadius;

        coordinates[9] = adjustedRadius / 2;
        coordinates[10] = adjustedRadius / 2;
        coordinates[11] = 0;
        coordinates[12] = -adjustedRadius / 2;
        coordinates[13] = adjustedRadius / 2;

        coordinates[14] = -(rect.width() - adjustedRadius);

        coordinates[15] = adjustedRadius / 2;
        coordinates[16] = adjustedRadius / 2;
        coordinates[17] = 0;
        coordinates[18] = -adjustedRadius / 2;
        coordinates[19] = -adjustedRadius / 2;

        coordinates[20] = -(rect.height() - adjustedRadius);

        coordinates[21] = adjustedRadius / 2;
        coordinates[22] = adjustedRadius / 2;
        coordinates[23] = 0;
        coordinates[24] = adjustedRadius / 2;
        coordinates[25] = -adjustedRadius / 2;

        // Inside Square (opposite direction)
        coordinates[26] = rect.x() + borderWidth;
        coordinates[27] = rect.y() + borderHeight;
        coordinates[28] = rect.height() - (borderHeight * 2);
        coordinates[29] = rect.width() - (borderWidth * 2);
        coordinates[30] = -(rect.height() - (borderHeight * 2));

        vgAppendPathData(path, 14, roundedRectCommands, coordinates.constData());
    } else {
        // rounded outside, rounded inside

        static const VGubyte roundedBorderCommands[] = {
            // Outer
            VG_MOVE_TO_ABS,
            VG_HLINE_TO_REL,
            VG_SCCWARC_TO_REL,
            VG_VLINE_TO_REL,
            VG_SCCWARC_TO_REL,
            VG_HLINE_TO_REL,
            VG_SCCWARC_TO_REL,
            VG_VLINE_TO_REL,
            VG_SCCWARC_TO_REL,
            // Inner
            VG_MOVE_TO_ABS,
            VG_SCWARC_TO_REL,
            VG_VLINE_TO_REL,
            VG_SCWARC_TO_REL,
            VG_HLINE_TO_REL,
            VG_SCWARC_TO_REL,
            VG_VLINE_TO_REL,
            VG_SCWARC_TO_REL,
            VG_HLINE_TO_REL,
            VG_CLOSE_PATH
        };

        // Adjust for OpenVG's usage or radius
        float adjustedRadius = radius * 2;
        float adjustedInnerRadius = (radius - qMax(borderWidth, borderHeight)) * 2;

        // Create command data
        QVector<VGfloat> coordinates(52);

        // Outer
        coordinates[0] =  rect.x() + adjustedRadius / 2;
        coordinates[1] =  rect.y();

        coordinates[2] = rect.width() - adjustedRadius;

        coordinates[3] = adjustedRadius / 2;
        coordinates[4] = adjustedRadius / 2;
        coordinates[5] = 0;
        coordinates[6] = adjustedRadius / 2;
        coordinates[7] = adjustedRadius / 2;

        coordinates[8] = rect.height() - adjustedRadius;

        coordinates[9] = adjustedRadius / 2;
        coordinates[10] = adjustedRadius / 2;
        coordinates[11] = 0;
        coordinates[12] = -adjustedRadius / 2;
        coordinates[13] = adjustedRadius / 2;

        coordinates[14] = -(rect.width() - adjustedRadius);

        coordinates[15] = adjustedRadius / 2;
        coordinates[16] = adjustedRadius / 2;
        coordinates[17] = 0;
        coordinates[18] = -adjustedRadius / 2;
        coordinates[19] = -adjustedRadius / 2;

        coordinates[20] = -(rect.height() - adjustedRadius);

        coordinates[21] = adjustedRadius / 2;
        coordinates[22] = adjustedRadius / 2;
        coordinates[23] = 0;
        coordinates[24] = adjustedRadius / 2;
        coordinates[25] = -adjustedRadius / 2;

        // Inner
        coordinates[26] =  rect.width() - (adjustedInnerRadius / 2 + borderWidth);
        coordinates[27] =  rect.height() - borderHeight;

        coordinates[28] = adjustedInnerRadius / 2;
        coordinates[29] = adjustedInnerRadius / 2;
        coordinates[30] = 0;
        coordinates[31] = adjustedInnerRadius / 2;
        coordinates[32] = -adjustedInnerRadius / 2;

        coordinates[33] = -((rect.height() - borderHeight * 2) - adjustedInnerRadius);

        coordinates[34] = adjustedInnerRadius / 2;
        coordinates[35] = adjustedInnerRadius / 2;
        coordinates[36] = 0;
        coordinates[37] = -adjustedInnerRadius / 2;
        coordinates[38] = -adjustedInnerRadius / 2;

        coordinates[39] = -((rect.width() - borderWidth * 2) - adjustedInnerRadius);

        coordinates[40] = adjustedInnerRadius / 2;
        coordinates[41] = adjustedInnerRadius / 2;
        coordinates[42] = 0;
        coordinates[43] = -adjustedInnerRadius / 2;
        coordinates[44] = adjustedInnerRadius / 2;

        coordinates[45] = (rect.height() - borderHeight * 2) - adjustedInnerRadius;

        coordinates[46] = adjustedInnerRadius / 2;
        coordinates[47] = adjustedInnerRadius / 2;
        coordinates[48] = 0;
        coordinates[49] = adjustedInnerRadius / 2;
        coordinates[50] = adjustedInnerRadius / 2;

        coordinates[51] = (rect.width() - borderWidth * 2) - adjustedInnerRadius;

        vgAppendPathData(path, 19, roundedBorderCommands, coordinates.constData());
    }
}

void QSGOpenVGInternalRectangleNode::generateRectangleAndBorderPaths(const QRectF &rect, float penWidth, float radius, VGPath inside, VGPath outside) const
{
    //Borders can not be more than half the height/width of a rect
    float borderWidth = qMin(penWidth, (float)rect.width() * 0.5f);
    float borderHeight = qMin(penWidth, (float)rect.height() * 0.5f);

    //Radius should never exceeds half of the width or half of the height
    float adjustedRadius = qMin((float)qMin(rect.width(), rect.height()) * 0.5f, radius);

    QRectF innerRect = rect;
    innerRect.adjust(borderWidth, borderHeight, -borderWidth, -borderHeight);

    if (radius == 0) {
        // Regular rect with border
        generateRectanglePath(innerRect, 0, inside);
        generateBorderPath(rect, borderWidth, borderHeight, 0, outside);
    } else {
        // Rounded Rect with border
        float innerRadius = radius - qMax(borderWidth, borderHeight);
        if (innerRadius < 0)
            innerRadius = 0.0f;

        generateRectanglePath(innerRect, innerRadius, inside);
        generateBorderPath(rect, borderWidth, borderHeight, adjustedRadius, outside);
    }
}
