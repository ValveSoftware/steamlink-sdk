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

#include "qsgopenvghelpers.h"
#include <cmath>

QT_BEGIN_NAMESPACE

namespace QSGOpenVGHelpers {

VGPath qPainterPathToVGPath(const QPainterPath &path)
{
    int count = path.elementCount();

    VGPath vgpath = vgCreatePath(VG_PATH_FORMAT_STANDARD,
                                 VG_PATH_DATATYPE_F,
                                 1.0f,        // scale
                                 0.0f,        // bias
                                 count + 1,   // segmentCapacityHint
                                 count * 2,   // coordCapacityHint
                                 VG_PATH_CAPABILITY_ALL);

    if (count == 0)
        return vgpath;

    QVector<VGfloat> coords;
    QVector<VGubyte> segments;

    int curvePos = 0;
    QPointF temp;

    // Keep track of the start and end of each sub-path.  QPainterPath
    // does not have an "implicit close" flag like QVectorPath does.
    // We therefore have to detect closed paths by looking for a LineTo
    // element that connects back to the initial MoveTo element.
    qreal startx = 0.0;
    qreal starty = 0.0;
    qreal endx = 0.0;
    qreal endy = 0.0;
    bool haveStart = false;
    bool haveEnd = false;

    for (int i = 0; i < count; ++i) {
        const QPainterPath::Element element = path.elementAt(i);
        switch (element.type) {

        case QPainterPath::MoveToElement:
        {
            if (haveStart && haveEnd && startx == endx && starty == endy) {
                // Implicitly close the previous sub-path.
                segments.append(VG_CLOSE_PATH);
            }
            temp = QPointF(element.x, element.y);
            startx = temp.x();
            starty = temp.y();
            coords.append(startx);
            coords.append(starty);
            haveStart = true;
            haveEnd = false;
            segments.append(VG_MOVE_TO_ABS);
        }
            break;

        case QPainterPath::LineToElement:
        {
            temp = QPointF(element.x, element.y);
            endx = temp.x();
            endy = temp.y();
            coords.append(endx);
            coords.append(endy);
            haveEnd = true;
            segments.append(VG_LINE_TO_ABS);
        }
            break;

        case QPainterPath::CurveToElement:
        {
            temp = QPointF(element.x, element.y);
            coords.append(temp.x());
            coords.append(temp.y());
            haveEnd = false;
            curvePos = 2;
        }
            break;

        case QPainterPath::CurveToDataElement:
        {
            temp = QPointF(element.x, element.y);
            coords.append(temp.x());
            coords.append(temp.y());
            haveEnd = false;
            curvePos += 2;
            if (curvePos == 6) {
                curvePos = 0;
                segments.append(VG_CUBIC_TO_ABS);
            }
        }
            break;

        }
    }

    if (haveStart && haveEnd && startx == endx && starty == endy) {
        // Implicitly close the last sub-path.
        segments.append(VG_CLOSE_PATH);
    }

    vgAppendPathData(vgpath, segments.count(),
                     segments.constData(), coords.constData());

    return vgpath;
}


void qDrawTiled(VGImage image, const QSize imageSize, const QRectF &targetRect, const QPointF offset, float scaleX, float scaleY) {

    //Check for valid image size and targetRect
    if (imageSize.width() <= 0 || imageSize.height() <= 0)
        return;
    if (targetRect.width() <= 0 || targetRect.height() <= 0)
        return;

    // This logic is mostly from the Qt Raster PaintEngine's qt_draw_tile
    qreal drawH;
    qreal drawW;
    qreal xPos;
    qreal xOff;
    qreal yPos = targetRect.y();
    qreal yOff;

    if (offset.y() < 0)
        yOff = imageSize.height() - qRound(-offset.y()) % imageSize.height();
    else
        yOff = qRound(offset.y()) % imageSize.height();


    // Save the current image transform matrix
    vgSeti(VG_MATRIX_MODE, VG_MATRIX_IMAGE_USER_TO_SURFACE);
    QVector<float> originalMatrix(9);
    vgGetMatrix(originalMatrix.data());

    while (!qFuzzyCompare(yPos, targetRect.y() + targetRect.height()) &&
           yPos < targetRect.y() + targetRect.height()) {
        drawH = imageSize.height() - yOff; // Cropping first row
        if (yPos + drawH * scaleY > targetRect.y() + targetRect.height()) { // Cropping last row
            // Check that values aren't equal
            if (!qFuzzyCompare((float)(yPos + drawH * scaleY), (float)(targetRect.y() + targetRect.height())))
                drawH = targetRect.y() + targetRect.height() - yPos;
        }
        xPos = targetRect.x();
        if (offset.x() < 0)
            xOff = imageSize.width() - qRound(-offset.x()) % imageSize.width();
        else
            xOff = qRound(offset.x()) % imageSize.width();

        while (!qFuzzyCompare(xPos, targetRect.x() + targetRect.width()) &&
               xPos < targetRect.x() + targetRect.width()) {
            drawW = imageSize.width() - xOff; // Cropping first column
            if (xPos + drawW * scaleX > targetRect.x() + targetRect.width()) {
                // Check that values aren't equal
                if (!qFuzzyCompare((float)(xPos + drawW * scaleX), (float)(targetRect.x() + targetRect.width())))
                    drawW = targetRect.x() + targetRect.width() - xPos;
            }
            if (round(drawW) > 0 && round(drawH) > 0) { // Can't source image less than 1 width or height
                //Draw here
                VGImage childRectImage = vgChildImage(image, xOff, yOff, round(drawW), round(drawH));
                vgTranslate(xPos, yPos);
                vgScale(scaleX, scaleY);
                vgDrawImage(childRectImage);
                vgDestroyImage(childRectImage);
                vgLoadMatrix(originalMatrix.constData());
            }
            if ( drawW > 0)
                xPos += drawW * scaleX;
            xOff = 0;
        }
        if ( drawH > 0)
            yPos += drawH * scaleY;
        yOff = 0;

    }
}

void qDrawBorderImage(VGImage image, const QSizeF &textureSize, const QRectF &targetRect, const QRectF &innerTargetRect, const QRectF &subSourceRect)
{
    // Create normalized margins
    QMarginsF margins(qMax(innerTargetRect.left() - targetRect.left(), 0.0),
                      qMax(innerTargetRect.top() - targetRect.top(), 0.0),
                      qMax(targetRect.right() - innerTargetRect.right(), 0.0),
                      qMax(targetRect.bottom() - innerTargetRect.bottom(), 0.0));

    QRectF sourceRect(0, 0, textureSize.width(), textureSize.height());

    // Create all the subRects
    QRectF topLeftSourceRect(sourceRect.topLeft(), QSizeF(margins.left(), margins.top()));
    QRectF topRightSourceRect(sourceRect.width() - margins.right(), sourceRect.top(), margins.right(), margins.top());
    QRectF bottomLeftSourceRect(sourceRect.left(), sourceRect.height() - margins.bottom(), margins.left(), margins.bottom());
    QRectF bottomRightSourceRect(sourceRect.width() - margins.right(), sourceRect.height() - margins.bottom(), margins.right(), margins.bottom());

    QRectF topSourceRect(margins.left(), 0.0, sourceRect.width() - (margins.right() + margins.left()), margins.top());
    QRectF topTargetRect(margins.left(), 0.0, innerTargetRect.width(), margins.top());
    QRectF bottomSourceRect(margins.left(), sourceRect.height() - margins.bottom(), sourceRect.width() - (margins.right() + margins.left()), margins.bottom());
    QRectF bottomTargetRect(margins.left(), targetRect.height() - margins.bottom(), innerTargetRect.width(), margins.bottom());
    QRectF leftSourceRect(0.0, margins.top(), margins.left(), sourceRect.height() - (margins.bottom() + margins.top()));
    QRectF leftTargetRect(0.0, margins.top(), margins.left(), innerTargetRect.height());
    QRectF rightSourceRect(sourceRect.width() - margins.right(), margins.top(), margins.right(), sourceRect.height() - (margins.bottom() + margins.top()));
    QRectF rightTargetRect(targetRect.width() - margins.right(), margins.top(), margins.right(), innerTargetRect.height());

    QRectF centerSourceRect(margins.left(), margins.top(), sourceRect.width() - (margins.right() + margins.left()), sourceRect.height() - (margins.top() + margins.bottom()));

    // Draw the 9 different sections
    // (1) Top Left (unscaled)
    qDrawSubImage(image,
                 topLeftSourceRect,
                 targetRect.topLeft());

    // (3) Top Right (unscaled)
    qDrawSubImage(image,
                 topRightSourceRect,
                 QPointF(targetRect.width() - margins.right(), 0.0));

    // (7) Bottom Left (unscaled)
    qDrawSubImage(image,
                 bottomLeftSourceRect,
                 QPointF(targetRect.left(), targetRect.height() - margins.bottom()));

    // (9) Bottom Right (unscaled)
    qDrawSubImage(image,
                 bottomRightSourceRect,
                 QPointF(targetRect.width() - margins.right(), targetRect.height() - margins.bottom()));

    double scaledWidth = 1.0;
    double scaledHeight = 1.0;

    // (2) Top (scaled via horizontalTileRule)
    VGImage topImage = vgChildImage(image, topSourceRect.x(), topSourceRect.y(), topSourceRect.width(), topSourceRect.height());
    scaledWidth = (topTargetRect.width() / subSourceRect.width()) / topSourceRect.width();

    QSGOpenVGHelpers::qDrawTiled(topImage, topSourceRect.size().toSize(), topTargetRect, QPoint(0.0, 0.0), scaledWidth, 1);

    vgDestroyImage(topImage);

    // (8) Bottom (scaled via horizontalTileRule)
    VGImage bottomImage = vgChildImage(image, bottomSourceRect.x(), bottomSourceRect.y(), bottomSourceRect.width(), bottomSourceRect.height());
    scaledWidth = (bottomTargetRect.width() / subSourceRect.width()) / bottomSourceRect.width();

    QSGOpenVGHelpers::qDrawTiled(bottomImage, bottomSourceRect.size().toSize(), bottomTargetRect, QPoint(0.0, 0.0), scaledWidth, 1);

    vgDestroyImage(bottomImage);

    // (4) Left (scaled via verticalTileRule)
    VGImage leftImage = vgChildImage(image, leftSourceRect.x(), leftSourceRect.y(), leftSourceRect.width(), leftSourceRect.height());
    scaledHeight = (leftTargetRect.height() / subSourceRect.height()) / leftSourceRect.height();
    QSGOpenVGHelpers::qDrawTiled(leftImage, leftSourceRect.size().toSize(), leftTargetRect, QPointF(0.0, 0.0), 1, scaledHeight);

    vgDestroyImage(leftImage);

    // (6) Right (scaled via verticalTileRule)
    VGImage rightImage = vgChildImage(image, rightSourceRect.x(), rightSourceRect.y(), rightSourceRect.width(), rightSourceRect.height());
    scaledHeight = (rightTargetRect.height() / subSourceRect.height()) / rightSourceRect.height();

    QSGOpenVGHelpers::qDrawTiled(rightImage, rightSourceRect.size().toSize(), rightTargetRect, QPointF(0, 0), 1, scaledHeight);

    vgDestroyImage(rightImage);

    // (5) Center (saled via verticalTileRule and horizontalTileRule)
    VGImage centerImage = vgChildImage(image, centerSourceRect.x(), centerSourceRect.y(), centerSourceRect.width(), centerSourceRect.height());

    scaledWidth = (innerTargetRect.width() / subSourceRect.width()) / centerSourceRect.width();
    scaledHeight = (innerTargetRect.height() / subSourceRect.height()) / centerSourceRect.height();

    QSGOpenVGHelpers::qDrawTiled(centerImage, centerSourceRect.size().toSize(), innerTargetRect, QPointF(0, 0), scaledWidth, scaledHeight);

    vgDestroyImage(centerImage);
}

void qDrawSubImage(VGImage image, const QRectF &sourceRect, const QPointF &destOffset)
{
    // Check for valid source size
    if (sourceRect.width() <= 0 || sourceRect.height() <= 0)
        return;

    // Save the current image transform matrix
    vgSeti(VG_MATRIX_MODE, VG_MATRIX_IMAGE_USER_TO_SURFACE);
    QVector<float> originalMatrix(9);
    vgGetMatrix(originalMatrix.data());

    // Get the child Image
    VGImage childRectImage = vgChildImage(image, sourceRect.x(), sourceRect.y(), sourceRect.width(), sourceRect.height());
    vgTranslate(destOffset.x(), destOffset.y());
    vgDrawImage(childRectImage);
    vgDestroyImage(childRectImage);

    // Pop Matrix
    vgLoadMatrix(originalMatrix.constData());
}

const QVector<VGfloat> qColorToVGColor(const QColor &color, float opacity)
{
    QVector<VGfloat> vgColor(4);
    vgColor[0] = color.redF();
    vgColor[1] = color.greenF();
    vgColor[2] = color.blueF();
    vgColor[3] = color.alphaF() * opacity;
    return vgColor;
}

VGImageFormat qImageFormatToVGImageFormat(QImage::Format format)
{
    VGImageFormat vgFormat;

    switch (format) {
    case QImage::Format_Mono:
    case QImage::Format_MonoLSB:
        vgFormat = VG_BW_1;
        break;
    case QImage::Format_RGB32:
        vgFormat = VG_sXRGB_8888;
        break;
    case QImage::Format_ARGB32:
        vgFormat = VG_sARGB_8888;
        break;
    case QImage::Format_ARGB32_Premultiplied:
        vgFormat = VG_sARGB_8888_PRE;
        break;
    case QImage::Format_RGB16:
        vgFormat = VG_sRGB_565;
        break;
    case QImage::Format_RGBX8888:
        vgFormat = VG_sRGBX_8888;
        break;
    case QImage::Format_RGBA8888:
        vgFormat = VG_sRGBA_8888;
        break;
    case QImage::Format_RGBA8888_Premultiplied:
        vgFormat = VG_sRGBA_8888_PRE;
        break;
    case QImage::Format_Alpha8:
        vgFormat = VG_A_8;
        break;
    case QImage::Format_Grayscale8:
        vgFormat = VG_sL_8;
        break;
    default:
        //Invalid
        vgFormat = (VGImageFormat)-1;
        break;
    }

    return vgFormat;
}

QImage::Format qVGImageFormatToQImageFormat(VGImageFormat format)
{
    QImage::Format qImageFormat;

    switch (format) {
    case VG_BW_1:
        qImageFormat = QImage::Format_Mono;
        break;
    case VG_sXRGB_8888:
        qImageFormat = QImage::Format_RGB32;
        break;
    case VG_sARGB_8888:
        qImageFormat = QImage::Format_ARGB32;
        break;
    case VG_sARGB_8888_PRE:
        qImageFormat = QImage::Format_ARGB32_Premultiplied;
        break;
    case VG_sRGB_565:
        qImageFormat = QImage::Format_RGB16;
        break;
    case VG_sRGBX_8888:
        qImageFormat = QImage::Format_RGBX8888;
        break;
    case VG_sRGBA_8888:
        qImageFormat = QImage::Format_RGBA8888;
        break;
    case VG_sRGBA_8888_PRE:
        qImageFormat = QImage::Format_RGBA8888_Premultiplied;
        break;
    case VG_A_8:
        qImageFormat = QImage::Format_Alpha8;
        break;
    case VG_sL_8:
        qImageFormat = QImage::Format_Grayscale8;
    default:
        qImageFormat = QImage::Format_ARGB32;
        break;
    }

    return qImageFormat;
}

} // end namespace QSGOpenVGHelpers

QT_END_NAMESPACE
