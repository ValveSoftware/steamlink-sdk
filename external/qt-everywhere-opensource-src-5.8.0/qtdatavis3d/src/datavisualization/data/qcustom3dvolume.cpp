/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Data Visualization module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qcustom3dvolume_p.h"
#include "utils_p.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

/*!
 * \class QCustom3DVolume
 * \inmodule QtDataVisualization
 * \brief The QCustom3DVolume class is for creating volume rendered objects to be added to a graph.
 * \since QtDataVisualization 1.2
 *
 * This class is for creating volume rendered objects to be added to a graph. A volume rendered
 * object is a box with a 3D texture. Three slice planes are supported for the volume, one along
 * each main axis of the volume.
 *
 * Rendering volume objects is very performance intensive, especially when the volume is largely
 * transparent, as the contents of the volume are ray-traced. The performance scales nearly linearly
 * with the amount of pixels that the volume occupies on the screen, so showing the volume in a
 * smaller view or limiting the zoom level of the graph are easy ways to improve performance.
 * Similarly, the volume texture dimensions have a large impact on performance.
 * If the frame rate is more important than pixel-perfect rendering of the volume contents, consider
 * turning the high definition shader off by setting useHighDefShader property to \c{false}.
 *
 * \note Volumetric objects are only supported with orthographic projection.
 *
 * \note Volumetric objects utilize 3D textures, which are not supported in OpenGL ES2 environments.
 *
 * \sa QAbstract3DGraph::addCustomItem(), QAbstract3DGraph::orthoProjection, useHighDefShader
 */

/*!
 * \qmltype Custom3DVolume
 * \inqmlmodule QtDataVisualization
 * \since QtDataVisualization 1.2
 * \ingroup datavisualization_qml
 * \instantiates QCustom3DVolume
 * \inherits Custom3DItem
 * \brief The Custom3DVolume type is for creating volume rendered objects to be added to a graph.
 *
 * This class is for creating volume rendered objects to be added to a graph. A volume rendered
 * object is a box with a 3D texture. Three slice planes are supported for the volume, one along
 * each main axis of the volume.
 *
 * Rendering volume objects is very performance intensive, especially when the volume is largely
 * transparent, as the contents of the volume are ray-traced. The performance scales nearly linearly
 * with the amount of pixels that the volume occupies on the screen, so showing the volume in a
 * smaller view or limiting the zoom level of the graph are easy ways to improve performance.
 * Similarly, the volume texture dimensions have a large impact on performance.
 * If the frame rate is more important than pixel-perfect rendering of the volume contents, consider
 * turning the high definition shader off by setting useHighDefShader property to \c{false}.
 *
 * \note: Filling in the volume data would not typically be efficient or practical from pure QML,
 * so properties directly related to that are not fully supported from QML.
 * Make a hybrid QML/C++ application if you want to use volume objects with a QML UI.
 *
 * \note Volumetric objects are only supported with orthographic projection.
 *
 * \note Volumetric objects utilize 3D textures, which are not supported in OpenGL ES2 environments.
 *
 * \sa AbstractGraph3D::orthoProjection, useHighDefShader
 */

/*! \qmlproperty int Custom3DVolume::textureWidth
 *
 * The width of the 3D texture defining the volume content in pixels. Defaults to \c{0}.
 *
 * \note: Changing this property from QML is not supported, as the texture data cannot be resized
 * to match.
 */

/*! \qmlproperty int Custom3DVolume::textureHeight
 *
 * The height of the 3D texture defining the volume content in pixels. Defaults to \c{0}.
 *
 * \note: Changing this property from QML is not supported, as the texture data cannot be resized
 * to match.
 */

/*! \qmlproperty int Custom3DVolume::textureDepth
 *
 * The depth of the 3D texture defining the volume content in pixels. Defaults to \c{0}.
 *
 * \note: Changing this property from QML is not supported, as the texture data cannot be resized
 * to match.
 */

/*! \qmlproperty int Custom3DVolume::sliceIndexX
 *
 * The X dimension index into the texture data indicating which vertical slice to show.
 * Setting any dimension to negative indicates no slice or slice frame for that dimension is drawn.
 * If all dimensions are negative, no slices or slice frames are drawn and the volume is drawn
 * normally.
 * Defaults to \c{-1}.
 *
 * \sa QCustom3DVolume::textureData, drawSlices, drawSliceFrames
 */

/*! \qmlproperty int Custom3DVolume::sliceIndexY
 *
 * The Y dimension index into the texture data indicating which horizontal slice to show.
 * Setting any dimension to negative indicates no slice or slice frame for that dimension is drawn.
 * If all dimensions are negative, no slices or slice frames are drawn and the volume is drawn
 * normally.
 * Defaults to \c{-1}.
 *
 * \sa QCustom3DVolume::textureData, drawSlices, drawSliceFrames
 */

/*! \qmlproperty int Custom3DVolume::sliceIndexZ
 *
 * The Z dimension index into the texture data indicating which vertical slice to show.
 * Setting any dimension to negative indicates no slice or slice frame for that dimension is drawn.
 * If all dimensions are negative, no slices or slice frames are drawn and the volume is drawn
 * normally.
 * Defaults to \c{-1}.
 *
 * \sa QCustom3DVolume::textureData, drawSlices, drawSliceFrames
 */

/*!
 * \qmlproperty real Custom3DVolume::alphaMultiplier
 *
 * The alpha value of every texel of the volume texture is multiplied with this value at
 * the render time. This can be used to introduce uniform transparency to the volume.
 * If preserveOpacity is \c{true}, only texels with at least some transparency to begin with are
 * affected, and fully opaque texels are not affected.
 * The value must not be negative.
 * Defaults to \c{1.0}.
 *
 * \sa preserveOpacity
 */

/*!
 * \qmlproperty bool Custom3DVolume::preserveOpacity
 *
 * If this property value is \c{true}, alphaMultiplier is only applied to texels that already have
 * some transparency. If it is \c{false}, the multiplier is applied to the alpha value of all
 * texels.
 * Defaults to \c{true}.
 *
 * \sa alphaMultiplier
 */

/*!
 * \qmlproperty bool Custom3DVolume::useHighDefShader
 *
 * If this property value is \c{true}, a high definition shader is used to render the volume.
 * If it is \c{false}, a low definition shader is used.
 *
 * The high definition shader guarantees that every visible texel of the volume texture is sampled
 * when the volume is rendered.
 * The low definition shader renders only a rough approximation of the volume contents,
 * but at much higher frame rate. The low definition shader doesn't guarantee every texel of the
 * volume texture is sampled, so there may be flickering if the volume contains distinct thin
 * features.
 *
 * \note This value doesn't affect the level of detail when rendering the slices of the volume.
 *
 * Defaults to \c{true}.
 */

/*!
 * \qmlproperty bool Custom3DVolume::drawSlices
 *
 * If this property value is \c{true}, the slices indicated by slice index properties
 * will be drawn instead of the full volume.
 * If it is \c{false}, the full volume will always be drawn.
 * Defaults to \c{false}.
 *
 * \note The slices are always drawn along the item axes, so if the item is rotated, the slices are
 * rotated as well.
 *
 * \sa sliceIndexX, sliceIndexY, sliceIndexZ
 */

/*!
 * \qmlproperty bool Custom3DVolume::drawSliceFrames
 *
 * If this property value is \c{true}, the frames of slices indicated by slice index properties
 * will be drawn around the volume.
 * If it is \c{false}, no slice frames will be drawn.
 * Drawing slice frames is independent of drawing slices, so you can show the full volume and
 * still draw the slice frames around it.
 * Defaults to \c{false}.
 *
 * \sa sliceIndexX, sliceIndexY, sliceIndexZ, drawSlices
 */

/*!
 * \qmlproperty color Custom3DVolume::sliceFrameColor
 *
 * Indicates the color of the slice frame. Transparent slice frame color is not supported.
 *
 * Defaults to black.
 *
 * \sa drawSliceFrames
 */

/*!
 * \qmlproperty vector3d Custom3DVolume::sliceFrameWidths
 *
 * Indicates the widths of the slice frame. The width can be different on different dimensions,
 * so you can for example omit drawing the frames on certain sides of the volume by setting the
 * value for that dimension to zero. The values are fractions of the volume thickness in the same
 * dimension. The values cannot be negative.
 *
 * Defaults to \c{vector3d(0.01, 0.01, 0.01)}.
 *
 * \sa drawSliceFrames
 */

/*!
 * \qmlproperty vector3d Custom3DVolume::sliceFrameGaps
 *
 * Indicates the amount of air gap left between the volume itself and the frame in each dimension.
 * The gap can be different on different dimensions. The values are fractions of the volume
 * thickness in the same dimension. The values cannot be negative.
 *
 * Defaults to \c{vector3d(0.01, 0.01, 0.01)}.
 *
 * \sa drawSliceFrames
 */

/*!
 * \qmlproperty vector3d Custom3DVolume::sliceFrameThicknesses
 *
 * Indicates the thickness of the slice frames for each dimension. The values are fractions of
 * the volume thickness in the same dimension. The values cannot be negative.
 *
 * Defaults to \c{vector3d(0.01, 0.01, 0.01)}.
 *
 * \sa drawSliceFrames
 */

/*!
 * Constructs QCustom3DVolume with given \a parent.
 */
QCustom3DVolume::QCustom3DVolume(QObject *parent) :
    QCustom3DItem(new QCustom3DVolumePrivate(this), parent)
{
}

/*!
 * Constructs QCustom3DVolume with given \a position, \a scaling, \a rotation,
 * \a textureWidth, \a textureHeight, \a textureDepth, \a textureData, \a textureFormat,
 * \a colorTable, and optional \a parent.
 *
 * \sa textureData, setTextureFormat(), colorTable
 */
QCustom3DVolume::QCustom3DVolume(const QVector3D &position, const QVector3D &scaling,
                                 const QQuaternion &rotation, int textureWidth,
                                 int textureHeight, int textureDepth,
                                 QVector<uchar> *textureData, QImage::Format textureFormat,
                                 const QVector<QRgb> &colorTable, QObject *parent) :
    QCustom3DItem(new QCustom3DVolumePrivate(this, position, scaling, rotation, textureWidth,
                                             textureHeight, textureDepth,
                                             textureData, textureFormat, colorTable), parent)
{
}


/*!
 * Destroys QCustom3DVolume.
 */
QCustom3DVolume::~QCustom3DVolume()
{
}

/*! \property QCustom3DVolume::textureWidth
 *
 * The width of the 3D texture defining the volume content in pixels. Defaults to \c{0}.
 *
 * \note The textureData may need to be resized or recreated if this value is changed.
 * Defaults to \c{0}.
 *
 * \sa textureData, textureHeight, textureDepth, setTextureFormat(), textureDataWidth()
 */
void QCustom3DVolume::setTextureWidth(int value)
{
    if (value >= 0) {
        if (dptr()->m_textureWidth != value) {
            dptr()->m_textureWidth = value;
            dptr()->m_dirtyBitsVolume.textureDimensionsDirty = true;
            emit textureWidthChanged(value);
            emit dptr()->needUpdate();
        }
    } else {
        qWarning() << __FUNCTION__ << "Cannot set negative value.";
    }
}

int QCustom3DVolume::textureWidth() const
{
    return dptrc()->m_textureWidth;
}

/*! \property QCustom3DVolume::textureHeight
 *
 * The height of the 3D texture defining the volume content in pixels. Defaults to \c{0}.
 *
 * \note The textureData may need to be resized or recreated if this value is changed.
 * Defaults to \c{0}.
 *
 * \sa textureData, textureWidth, textureDepth, setTextureFormat()
 */
void QCustom3DVolume::setTextureHeight(int value)
{
    if (value >= 0) {
        if (dptr()->m_textureHeight != value) {
            dptr()->m_textureHeight = value;
            dptr()->m_dirtyBitsVolume.textureDimensionsDirty = true;
            emit textureHeightChanged(value);
            emit dptr()->needUpdate();
        }
    } else {
        qWarning() << __FUNCTION__ << "Cannot set negative value.";
    }

}

int QCustom3DVolume::textureHeight() const
{
    return dptrc()->m_textureHeight;
}

/*! \property QCustom3DVolume::textureDepth
 *
 * The depth of the 3D texture defining the volume content in pixels. Defaults to \c{0}.
 *
 * \note The textureData may need to be resized or recreated if this value is changed.
 * Defaults to \c{0}.
 *
 * \sa textureData, textureWidth, textureHeight, setTextureFormat()
 */
void QCustom3DVolume::setTextureDepth(int value)
{
    if (value >= 0) {
        if (dptr()->m_textureDepth != value) {
            dptr()->m_textureDepth = value;
            dptr()->m_dirtyBitsVolume.textureDimensionsDirty = true;
            emit textureDepthChanged(value);
            emit dptr()->needUpdate();
        }
    } else {
        qWarning() << __FUNCTION__ << "Cannot set negative value.";
    }
}

int QCustom3DVolume::textureDepth() const
{
    return dptrc()->m_textureDepth;
}

/*!
 * A convenience function for setting all three texture dimensions
 * (\a width, \a height, and \a depth) at once.
 *
 * \sa textureData
 */
void QCustom3DVolume::setTextureDimensions(int width, int height, int depth)
{
    setTextureWidth(width);
    setTextureHeight(height);
    setTextureDepth(depth);
}

/*!
 * \return the actual texture data width. When the texture format is QImage::Format_Indexed8,
 * this is textureWidth aligned to 32bit boundary. Otherwise this is four times textureWidth.
 */
int QCustom3DVolume::textureDataWidth() const
{
    int dataWidth = dptrc()->m_textureWidth;

    if (dptrc()->m_textureFormat == QImage::Format_Indexed8)
        dataWidth += dataWidth % 4;
    else
        dataWidth *= 4;

    return dataWidth;
}

/*! \property QCustom3DVolume::sliceIndexX
 *
 * The X dimension index into the texture data indicating which vertical slice to show.
 * Setting any dimension to negative indicates no slice or slice frame for that dimension is drawn.
 * If all dimensions are negative, no slices or slice frames are drawn and the volume is drawn
 * normally.
 * Defaults to \c{-1}.
 *
 * \sa textureData, drawSlices, drawSliceFrames
 */
void QCustom3DVolume::setSliceIndexX(int value)
{
    if (dptr()->m_sliceIndexX != value) {
        dptr()->m_sliceIndexX = value;
        dptr()->m_dirtyBitsVolume.slicesDirty = true;
        emit sliceIndexXChanged(value);
        emit dptr()->needUpdate();
    }
}

int QCustom3DVolume::sliceIndexX() const
{
    return dptrc()->m_sliceIndexX;
}

/*! \property QCustom3DVolume::sliceIndexY
 *
 * The Y dimension index into the texture data indicating which horizontal slice to show.
 * Setting any dimension to negative indicates no slice or slice frame for that dimension is drawn.
 * If all dimensions are negative, no slices or slice frames are drawn and the volume is drawn
 * normally.
 * Defaults to \c{-1}.
 *
 * \sa textureData, drawSlices, drawSliceFrames
 */
void QCustom3DVolume::setSliceIndexY(int value)
{
    if (dptr()->m_sliceIndexY != value) {
        dptr()->m_sliceIndexY = value;
        dptr()->m_dirtyBitsVolume.slicesDirty = true;
        emit sliceIndexYChanged(value);
        emit dptr()->needUpdate();
    }
}

int QCustom3DVolume::sliceIndexY() const
{
    return dptrc()->m_sliceIndexY;
}

/*! \property QCustom3DVolume::sliceIndexZ
 *
 * The Z dimension index into the texture data indicating which vertical slice to show.
 * Setting any dimension to negative indicates no slice or slice frame for that dimension is drawn.
 * If all dimensions are negative, no slices or slice frames are drawn and the volume is drawn
 * normally.
 * Defaults to \c{-1}.
 *
 * \sa textureData, drawSlices, drawSliceFrames
 */
void QCustom3DVolume::setSliceIndexZ(int value)
{
    if (dptr()->m_sliceIndexZ != value) {
        dptr()->m_sliceIndexZ = value;
        dptr()->m_dirtyBitsVolume.slicesDirty = true;
        emit sliceIndexZChanged(value);
        emit dptr()->needUpdate();
    }
}

int QCustom3DVolume::sliceIndexZ() const
{
    return dptrc()->m_sliceIndexZ;
}

/*!
 * A convenience function for setting all three slice indices (\a x, \a y, and \a z) at once.
 *
 * \sa textureData
 */
void QCustom3DVolume::setSliceIndices(int x, int y, int z)
{
    setSliceIndexX(x);
    setSliceIndexY(y);
    setSliceIndexZ(z);
}

/*! \property QCustom3DVolume::colorTable
 *
 * The array containing the colors for indexed texture formats. If the texture format is not
 * indexed, this array is not used and can be empty.
 *
 * Defaults to \c{0}.
 *
 * \sa textureData, setTextureFormat(), QImage::colorTable()
 */
void QCustom3DVolume::setColorTable(const QVector<QRgb> &colors)
{
    if (dptr()->m_colorTable != colors) {
        dptr()->m_colorTable = colors;
        dptr()->m_dirtyBitsVolume.colorTableDirty = true;
        emit colorTableChanged();
        emit dptr()->needUpdate();
    }
}

QVector<QRgb> QCustom3DVolume::colorTable() const
{
    return dptrc()->m_colorTable;
}

/*! \property QCustom3DVolume::textureData
 *
 * The array containing the texture data in the format specified by textureFormat.
 * The size of this array must be at least
 * (\c{textureDataWidth * textureHeight * textureDepth * texture format color depth in bytes}).
 *
 * A 3D texture is defined by a stack of 2D subtextures. Each subtexture must be of identical size
 * (\c{textureDataWidth * textureHeight}), and the depth of the stack is defined by textureDepth
 * property. Each 2D texture data is identical to a QImage data with the same format, so
 * QImage::bits() can be used to supply the data for each subtexture.
 *
 * Ownership of the new array transfers to QCustom3DVolume instance.
 * If another array is set, the previous array is deleted.
 * If the same array is set again, it is assumed that the array contents have been changed and the
 * graph rendering is triggered.
 *
 * \note Each X-line of the data needs to be 32bit aligned. If the textureFormat is
 * QImage::Format_Indexed8 and textureWidth is not divisible by four, padding bytes need
 * to be added to each X-line of the \a data. You can get the padded byte count with
 * textureDataWidth() function. The padding bytes should indicate an fully transparent color
 * to avoid rendering artifacts.
 *
 * Defaults to \c{0}.
 *
 * \sa colorTable, setTextureFormat(), setSubTextureData(), textureDataWidth()
 */
void QCustom3DVolume::setTextureData(QVector<uchar> *data)
{
    if (dptr()->m_textureData != data)
        delete dptr()->m_textureData;

    // Even if the pointer is same as previously, consider this property changed, as the values
    // can be changed unbeknownst to us via the array pointer.
    dptr()->m_textureData = data;
    dptr()->m_dirtyBitsVolume.textureDataDirty = true;
    emit textureDataChanged(data);
    emit dptr()->needUpdate();
}

/*!
 * This function creates a new texture data array from an array of \a images and sets it as
 * textureData for this volume object. The texture dimensions are also set according to image
 * and array dimensions. All of the images in the array must be the same size. If the images are not
 * all in QImage::Format_Indexed8 format, all texture data will be converted into
 * QImage::Format_ARGB32 format. If the images are in QImage::Format_Indexed8 format, the colorTable
 * for the entire volume will be taken from the first image.
 *
 * \return pointer to the newly created array.
 *
 * \sa textureData, textureWidth, textureHeight, textureDepth, setTextureFormat()
 */
QVector<uchar> *QCustom3DVolume::createTextureData(const QVector<QImage *> &images)
{
    int imageCount = images.size();
    if (imageCount) {
        QImage *currentImage = images.at(0);
        int imageWidth = currentImage->width();
        int imageHeight = currentImage->height();
        QImage::Format imageFormat = currentImage->format();
        bool convert = false;
        if (imageFormat != QImage::Format_Indexed8 && imageFormat != QImage::Format_ARGB32) {
            convert = true;
            imageFormat = QImage::Format_ARGB32;
        } else {
            for (int i = 0; i < imageCount; i++) {
                currentImage = images.at(i);
                if (imageWidth != currentImage->width() || imageHeight != currentImage->height()) {
                    qWarning() << __FUNCTION__ << "Not all images were of the same size.";
                    setTextureData(0);
                    setTextureWidth(0);
                    setTextureHeight(0);
                    setTextureDepth(0);
                    return 0;

                }
                if (currentImage->format() != imageFormat) {
                    convert = true;
                    imageFormat = QImage::Format_ARGB32;
                    break;
                }
            }
        }
        int colorBytes = (imageFormat == QImage::Format_Indexed8) ? 1 : 4;
        int imageByteWidth = (imageFormat == QImage::Format_Indexed8)
                ? currentImage->bytesPerLine() : imageWidth;
        int frameSize = imageByteWidth * imageHeight * colorBytes;
        QVector<uchar> *newTextureData = new QVector<uchar>;
        newTextureData->resize(frameSize * imageCount);
        uchar *texturePtr = newTextureData->data();
        QImage convertedImage;

        for (int i = 0; i < imageCount; i++) {
            currentImage = images.at(i);
            if (convert) {
                convertedImage = currentImage->convertToFormat(imageFormat);
                currentImage = &convertedImage;
            }
            memcpy(texturePtr, static_cast<void *>(currentImage->bits()), frameSize);
            texturePtr += frameSize;
        }

        if (imageFormat == QImage::Format_Indexed8)
            setColorTable(images.at(0)->colorTable());
        setTextureData(newTextureData);
        setTextureFormat(imageFormat);
        setTextureWidth(imageWidth);
        setTextureHeight(imageHeight);
        setTextureDepth(imageCount);
    } else {
        setTextureData(0);
        setTextureWidth(0);
        setTextureHeight(0);
        setTextureDepth(0);
    }
    return dptr()->m_textureData;
}

QVector<uchar> *QCustom3DVolume::textureData() const
{
    return dptrc()->m_textureData;
}

/*!
 * This function allows setting a single 2D subtexture of the 3D texture along the specified
 * \a axis of the volume.
 * The \a index parameter specifies the subtexture to set.
 * The texture \a data must be in the format specified by textureFormat property and have size of
 * the cross-section of the volume texture along the specified axis multiplied by
 * the texture format color depth in bytes.
 * The \a data is expected to be ordered similarly to the data in images produced by renderSlice()
 * method along the same axis.
 *
 * \note Each X-line of the data needs to be 32bit aligned when targeting Y-axis or Z-axis.
 * If the textureFormat is QImage::Format_Indexed8 and textureWidth is not divisible by four,
 * padding bytes need to be added to each X-line of the \a data in cases it is not already
 * properly aligned. The padding bytes should indicate an fully transparent color to avoid
 * rendering artifacts.
 *
 * \sa textureData, renderSlice()
 */
void QCustom3DVolume::setSubTextureData(Qt::Axis axis, int index, const uchar *data)
{
    if (data) {
        int lineSize = textureDataWidth();
        int frameSize = lineSize * dptr()->m_textureHeight;
        int dataSize = dptr()->m_textureData->size();
        int pixelWidth = (dptr()->m_textureFormat == QImage::Format_Indexed8) ? 1 : 4;
        int targetIndex;
        uchar *dataPtr = dptr()->m_textureData->data();
        bool invalid = (index < 0);
        if (axis == Qt::XAxis) {
            targetIndex = index * pixelWidth;
            if (index >= dptr()->m_textureWidth
                    || (frameSize * (dptr()->m_textureDepth - 1) + targetIndex) > dataSize) {
                invalid = true;
            }
        } else if (axis == Qt::YAxis) {
            targetIndex = (index * lineSize) + (frameSize * (dptr()->m_textureDepth - 1));
            if (index >= dptr()->m_textureHeight || (targetIndex + lineSize > dataSize))
                invalid = true;
        } else {
            targetIndex = index * frameSize;
            if (index >= dptr()->m_textureDepth || ((targetIndex + frameSize) > dataSize))
                invalid = true;
        }

        if (invalid) {
            qWarning() << __FUNCTION__ << "Attempted to set invalid subtexture.";
        } else {
            const uchar *sourcePtr = data;
            uchar *targetPtr = dataPtr + targetIndex;
            if (axis == Qt::XAxis) {
                int targetWidth = dptr()->m_textureDepth;
                int targetHeight = dptr()->m_textureHeight;
                for (int i = 0; i < targetHeight; i++) {
                    targetPtr = dataPtr + targetIndex + (lineSize * i);
                    for (int j = 0; j < targetWidth; j++) {
                        for (int k = 0; k < pixelWidth; k++)
                            *targetPtr++ = *sourcePtr++;
                        targetPtr += (frameSize - pixelWidth);
                    }
                }
            } else if (axis == Qt::YAxis) {
                int targetHeight = dptr()->m_textureDepth;
                for (int i = 0; i < targetHeight; i++){
                    for (int j = 0; j < lineSize; j++)
                        *targetPtr++ = *sourcePtr++;
                    targetPtr -= (frameSize + lineSize);
                }
            } else {
                void *subTexPtr = dataPtr + targetIndex;
                memcpy(subTexPtr, static_cast<const void *>(data), frameSize);
            }
            dptr()->m_dirtyBitsVolume.textureDataDirty = true;
            emit textureDataChanged(dptr()->m_textureData);
            emit dptr()->needUpdate();
        }
    } else {
        qWarning() << __FUNCTION__ << "Tried to set null data.";
    }
}

/*!
 * This function allows setting a single 2D subtexture of the 3D texture along the specified
 * \a axis of the volume.
 * The \a index parameter specifies the subtexture to set.
 * The source \a image must be in the format specified by the textureFormat property if the
 * textureFormat is indexed. If the textureFormat is QImage::Format_ARGB32, the image is converted
 * to that format. The image must have the size of the cross-section of the volume texture along
 * the specified axis. The orientation of the image should correspond to the orientation of
 * the slice image produced by renderSlice() method along the same axis.
 *
 * \note Each X-line of the data needs to be 32bit aligned when targeting Y-axis or Z-axis.
 * If the textureFormat is QImage::Format_Indexed8 and textureWidth is not divisible by four,
 * padding bytes need to be added to each X-line of the \a image in cases it is not already
 * properly aligned. The padding bytes should indicate an fully transparent color to avoid
 * rendering artifacts. It is not guaranteed QImage will do this automatically.
 *
 * \sa textureData, renderSlice()
 */
void QCustom3DVolume::setSubTextureData(Qt::Axis axis, int index, const QImage &image)
{
    int sourceWidth = image.width();
    int sourceHeight = image.height();
    int targetWidth;
    int targetHeight;
    if (axis == Qt::XAxis) {
        targetWidth = dptr()->m_textureDepth;
        targetHeight = dptr()->m_textureHeight;
    } else if (axis == Qt::YAxis) {
        targetWidth = dptr()->m_textureWidth;
        targetHeight = dptr()->m_textureDepth;
    } else {
        targetWidth = dptr()->m_textureWidth;
        targetHeight = dptr()->m_textureHeight;
    }

    if (sourceWidth == targetWidth
            && sourceHeight == targetHeight
            && (image.format() == dptr()->m_textureFormat
                || dptr()->m_textureFormat == QImage::Format_ARGB32)) {
        QImage convertedImage;
        if (dptr()->m_textureFormat == QImage::Format_ARGB32
                && image.format() != QImage::Format_ARGB32) {
            convertedImage = image.convertToFormat(QImage::Format_ARGB32);
        } else {
            convertedImage = image;
        }
        setSubTextureData(axis, index, convertedImage.bits());
    } else {
        qWarning() << __FUNCTION__ << "Invalid image size or format.";
    }
}

// Note: textureFormat is not a Q_PROPERTY to work around an issue in meta object system that
// doesn't allow QImage::format to be a property type. Qt 5.2.1 at least has this problem.

/*!
 * Sets the format of the textureData to \a format. Only two formats are supported currently:
 * QImage::Format_Indexed8 and QImage::Format_ARGB32. If an indexed format is specified, colorTable
 * must also be set.
 * Defaults to QImage::Format_ARGB32.
 *
 * \sa colorTable, textureData
 */
void QCustom3DVolume::setTextureFormat(QImage::Format format)
{
    if (format == QImage::Format_ARGB32 || format == QImage::Format_Indexed8) {
        if (dptr()->m_textureFormat != format) {
            dptr()->m_textureFormat = format;
            dptr()->m_dirtyBitsVolume.textureFormatDirty = true;
            emit textureFormatChanged(format);
            emit dptr()->needUpdate();
        }
    } else {
        qWarning() << __FUNCTION__ << "Attempted to set invalid texture format.";
    }
}

/*!
 * \return the format of the textureData.
 *
 * \sa setTextureFormat()
 */
QImage::Format QCustom3DVolume::textureFormat() const
{
    return dptrc()->m_textureFormat;
}

/*!
 * \fn void QCustom3DVolume::textureFormatChanged(QImage::Format format)
 *
 * This signal is emitted when the textureData \a format changes.
 *
 * \sa setTextureFormat()
 */

/*!
 * \property QCustom3DVolume::alphaMultiplier
 *
 * The alpha value of every texel of the volume texture is multiplied with this value at
 * the render time. This can be used to introduce uniform transparency to the volume.
 * If preserveOpacity is \c{true}, only texels with at least some transparency to begin with are
 * affected, and fully opaque texels are not affected.
 * The value must not be negative.
 * Defaults to \c{1.0f}.
 *
 * \sa preserveOpacity, textureData
 */
void QCustom3DVolume::setAlphaMultiplier(float mult)
{
    if (mult >= 0.0f) {
        if (dptr()->m_alphaMultiplier != mult) {
            dptr()->m_alphaMultiplier = mult;
            dptr()->m_dirtyBitsVolume.alphaDirty = true;
            emit alphaMultiplierChanged(mult);
            emit dptr()->needUpdate();
        }
    } else {
        qWarning() << __FUNCTION__ << "Attempted to set negative multiplier.";
    }
}

float QCustom3DVolume::alphaMultiplier() const
{
    return dptrc()->m_alphaMultiplier;
}

/*!
 * \property QCustom3DVolume::preserveOpacity
 *
 * If this property value is \c{true}, alphaMultiplier is only applied to texels that already have
 * some transparency. If it is \c{false}, the multiplier is applied to the alpha value of all
 * texels.
 * Defaults to \c{true}.
 *
 * \sa alphaMultiplier
 */
void QCustom3DVolume::setPreserveOpacity(bool enable)
{
    if (dptr()->m_preserveOpacity != enable) {
        dptr()->m_preserveOpacity = enable;
        dptr()->m_dirtyBitsVolume.alphaDirty = true;
        emit preserveOpacityChanged(enable);
        emit dptr()->needUpdate();
    }
}

bool QCustom3DVolume::preserveOpacity() const
{
    return dptrc()->m_preserveOpacity;
}

/*!
 * \property QCustom3DVolume::useHighDefShader
 *
 * If this property value is \c{true}, a high definition shader is used to render the volume.
 * If it is \c{false}, a low definition shader is used.
 *
 * The high definition shader guarantees that every visible texel of the volume texture is sampled
 * when the volume is rendered.
 * The low definition shader renders only a rough approximation of the volume contents,
 * but at much higher frame rate. The low definition shader doesn't guarantee every texel of the
 * volume texture is sampled, so there may be flickering if the volume contains distinct thin
 * features.
 *
 * \note This value doesn't affect the level of detail when rendering the slices of the volume.
 *
 * Defaults to \c{true}.
 *
 * \sa renderSlice()
 */
void QCustom3DVolume::setUseHighDefShader(bool enable)
{
    if (dptr()->m_useHighDefShader != enable) {
        dptr()->m_useHighDefShader = enable;
        dptr()->m_dirtyBitsVolume.shaderDirty = true;
        emit useHighDefShaderChanged(enable);
        emit dptr()->needUpdate();
    }
}

bool QCustom3DVolume::useHighDefShader() const
{
    return dptrc()->m_useHighDefShader;
}

/*!
 * \property QCustom3DVolume::drawSlices
 *
 * If this property value is \c{true}, the slices indicated by slice index properties
 * will be drawn instead of the full volume.
 * If it is \c{false}, the full volume will always be drawn.
 * Defaults to \c{false}.
 *
 * \note The slices are always drawn along the item axes, so if the item is rotated, the slices are
 * rotated as well.
 *
 * \sa sliceIndexX, sliceIndexY, sliceIndexZ
 */
void QCustom3DVolume::setDrawSlices(bool enable)
{
    if (dptr()->m_drawSlices != enable) {
        dptr()->m_drawSlices = enable;
        dptr()->m_dirtyBitsVolume.slicesDirty = true;
        emit drawSlicesChanged(enable);
        emit dptr()->needUpdate();
    }
}

bool QCustom3DVolume::drawSlices() const
{
    return dptrc()->m_drawSlices;
}

/*!
 * \property QCustom3DVolume::drawSliceFrames
 *
 * If this property value is \c{true}, the frames of slices indicated by slice index properties
 * will be drawn around the volume.
 * If it is \c{false}, no slice frames will be drawn.
 * Drawing slice frames is independent of drawing slices, so you can show the full volume and
 * still draw the slice frames around it. This is useful when using renderSlice() to display the
 * slices outside the graph itself.
 * Defaults to \c{false}.
 *
 * \sa sliceIndexX, sliceIndexY, sliceIndexZ, drawSlices, renderSlice()
 */
void QCustom3DVolume::setDrawSliceFrames(bool enable)
{
    if (dptr()->m_drawSliceFrames != enable) {
        dptr()->m_drawSliceFrames = enable;
        dptr()->m_dirtyBitsVolume.slicesDirty = true;
        emit drawSliceFramesChanged(enable);
        emit dptr()->needUpdate();
    }
}

bool QCustom3DVolume::drawSliceFrames() const
{
    return dptrc()->m_drawSliceFrames;
}

/*!
 * \property QCustom3DVolume::sliceFrameColor
 *
 * Indicates the color of the slice frame. Transparent slice frame color is not supported.
 *
 * Defaults to black.
 *
 * \sa drawSliceFrames
 */
void QCustom3DVolume::setSliceFrameColor(const QColor &color)
{
    if (dptr()->m_sliceFrameColor != color) {
        dptr()->m_sliceFrameColor = color;
        dptr()->m_dirtyBitsVolume.slicesDirty = true;
        emit sliceFrameColorChanged(color);
        emit dptr()->needUpdate();
    }
}

QColor QCustom3DVolume::sliceFrameColor() const
{
    return dptrc()->m_sliceFrameColor;
}

/*!
 * \property QCustom3DVolume::sliceFrameWidths
 *
 * Indicates the widths of the slice frame. The width can be different on different dimensions,
 * so you can for example omit drawing the frames on certain sides of the volume by setting the
 * value for that dimension to zero. The values are fractions of the volume thickness in the same
 * dimension. The values cannot be negative.
 *
 * Defaults to \c{QVector3D(0.01, 0.01, 0.01)}.
 *
 * \sa drawSliceFrames
 */
void QCustom3DVolume::setSliceFrameWidths(const QVector3D &values)
{
    if (values.x() < 0.0f || values.y() < 0.0f || values.z() < 0.0f) {
        qWarning() << __FUNCTION__ << "Attempted to set negative values.";
    } else if (dptr()->m_sliceFrameWidths != values) {
        dptr()->m_sliceFrameWidths = values;
        dptr()->m_dirtyBitsVolume.slicesDirty = true;
        emit sliceFrameWidthsChanged(values);
        emit dptr()->needUpdate();
    }
}

QVector3D QCustom3DVolume::sliceFrameWidths() const
{
    return dptrc()->m_sliceFrameWidths;
}

/*!
 * \property QCustom3DVolume::sliceFrameGaps
 *
 * Indicates the amount of air gap left between the volume itself and the frame in each dimension.
 * The gap can be different on different dimensions. The values are fractions of the volume
 * thickness in the same dimension. The values cannot be negative.
 *
 * Defaults to \c{QVector3D(0.01, 0.01, 0.01)}.
 *
 * \sa drawSliceFrames
 */
void QCustom3DVolume::setSliceFrameGaps(const QVector3D &values)
{
    if (values.x() < 0.0f || values.y() < 0.0f || values.z() < 0.0f) {
        qWarning() << __FUNCTION__ << "Attempted to set negative values.";
    } else if (dptr()->m_sliceFrameGaps != values) {
        dptr()->m_sliceFrameGaps = values;
        dptr()->m_dirtyBitsVolume.slicesDirty = true;
        emit sliceFrameGapsChanged(values);
        emit dptr()->needUpdate();
    }
}

QVector3D QCustom3DVolume::sliceFrameGaps() const
{
    return dptrc()->m_sliceFrameGaps;
}

/*!
 * \property QCustom3DVolume::sliceFrameThicknesses
 *
 * Indicates the thickness of the slice frames for each dimension. The values are fractions of
 * the volume thickness in the same dimension. The values cannot be negative.
 *
 * Defaults to \c{QVector3D(0.01, 0.01, 0.01)}.
 *
 * \sa drawSliceFrames
 */
void QCustom3DVolume::setSliceFrameThicknesses(const QVector3D &values)
{
    if (values.x() < 0.0f || values.y() < 0.0f || values.z() < 0.0f) {
        qWarning() << __FUNCTION__ << "Attempted to set negative values.";
    } else if (dptr()->m_sliceFrameThicknesses != values) {
        dptr()->m_sliceFrameThicknesses = values;
        dptr()->m_dirtyBitsVolume.slicesDirty = true;
        emit sliceFrameThicknessesChanged(values);
        emit dptr()->needUpdate();
    }
}

QVector3D QCustom3DVolume::sliceFrameThicknesses() const
{
    return dptrc()->m_sliceFrameThicknesses;
}

/*!
 * Renders the slice specified by \a index along \a axis into an image.
 * The texture format of this object is used.
 *
 * \return the rendered image of the slice, or a null image if invalid index is specified.
 *
 * \sa setTextureFormat()
 */
QImage QCustom3DVolume::renderSlice(Qt::Axis axis, int index)
{
    return dptr()->renderSlice(axis, index);
}

/*!
 * \internal
 */
QCustom3DVolumePrivate *QCustom3DVolume::dptr()
{
    return static_cast<QCustom3DVolumePrivate *>(d_ptr.data());
}

/*!
 * \internal
 */
const QCustom3DVolumePrivate *QCustom3DVolume::dptrc() const
{
    return static_cast<const QCustom3DVolumePrivate *>(d_ptr.data());
}

QCustom3DVolumePrivate::QCustom3DVolumePrivate(QCustom3DVolume *q) :
    QCustom3DItemPrivate(q),
    m_textureWidth(0),
    m_textureHeight(0),
    m_textureDepth(0),
    m_sliceIndexX(-1),
    m_sliceIndexY(-1),
    m_sliceIndexZ(-1),
    m_textureFormat(QImage::Format_ARGB32),
    m_textureData(0),
    m_alphaMultiplier(1.0f),
    m_preserveOpacity(true),
    m_useHighDefShader(true),
    m_drawSlices(false),
    m_drawSliceFrames(false),
    m_sliceFrameColor(Qt::black),
    m_sliceFrameWidths(QVector3D(0.01f, 0.01f, 0.01f)),
    m_sliceFrameGaps(QVector3D(0.01f, 0.01f, 0.01f)),
    m_sliceFrameThicknesses(QVector3D(0.01f, 0.01f, 0.01f))
{
    m_isVolumeItem = true;
    m_meshFile = QStringLiteral(":/defaultMeshes/barFull");
}

QCustom3DVolumePrivate::QCustom3DVolumePrivate(QCustom3DVolume *q, const QVector3D &position,
                                               const QVector3D &scaling,
                                               const QQuaternion &rotation,
                                               int textureWidth, int textureHeight,
                                               int textureDepth, QVector<uchar> *textureData,
                                               QImage::Format textureFormat,
                                               const QVector<QRgb> &colorTable) :
    QCustom3DItemPrivate(q, QStringLiteral(":/defaultMeshes/barFull"), position, scaling, rotation),
    m_textureWidth(textureWidth),
    m_textureHeight(textureHeight),
    m_textureDepth(textureDepth),
    m_sliceIndexX(-1),
    m_sliceIndexY(-1),
    m_sliceIndexZ(-1),
    m_textureFormat(textureFormat),
    m_colorTable(colorTable),
    m_textureData(textureData),
    m_alphaMultiplier(1.0f),
    m_preserveOpacity(true),
    m_useHighDefShader(true),
    m_drawSlices(false),
    m_drawSliceFrames(false),
    m_sliceFrameColor(Qt::black),
    m_sliceFrameWidths(QVector3D(0.01f, 0.01f, 0.01f)),
    m_sliceFrameGaps(QVector3D(0.01f, 0.01f, 0.01f)),
    m_sliceFrameThicknesses(QVector3D(0.01f, 0.01f, 0.01f))
{
    m_isVolumeItem = true;
    m_shadowCasting = false;

    if (m_textureWidth < 0)
        m_textureWidth = 0;
    if (m_textureHeight < 0)
        m_textureHeight = 0;
    if (m_textureDepth < 0)
        m_textureDepth = 0;

    if (m_textureFormat != QImage::Format_Indexed8)
        m_textureFormat = QImage::Format_ARGB32;

}

QCustom3DVolumePrivate::~QCustom3DVolumePrivate()
{
    delete m_textureData;
}

void QCustom3DVolumePrivate::resetDirtyBits()
{
    QCustom3DItemPrivate::resetDirtyBits();

    m_dirtyBitsVolume.textureDimensionsDirty = false;
    m_dirtyBitsVolume.slicesDirty = false;
    m_dirtyBitsVolume.colorTableDirty = false;
    m_dirtyBitsVolume.textureDataDirty = false;
    m_dirtyBitsVolume.textureFormatDirty = false;
    m_dirtyBitsVolume.alphaDirty = false;
    m_dirtyBitsVolume.shaderDirty = false;
}

QImage QCustom3DVolumePrivate::renderSlice(Qt::Axis axis, int index)
{
    if (index < 0)
        return QImage();

    int x;
    int y;
    if (axis == Qt::XAxis) {
        if (index >= m_textureWidth)
            return QImage();
        x = m_textureDepth;
        y = m_textureHeight;
    } else if (axis == Qt::YAxis) {
        if (index >= m_textureHeight)
            return QImage();
        x = m_textureWidth;
        y = m_textureDepth;
    } else {
        if (index >= m_textureDepth)
            return QImage();
        x = m_textureWidth;
        y = m_textureHeight;
    }

    int padding = 0;
    int pixelWidth = 4;
    int dataWidth = qptr()->textureDataWidth();
    if (m_textureFormat == QImage::Format_Indexed8) {
        padding = x % 4;
        pixelWidth = 1;
    }
    QVector<uchar> data((x + padding) * y * pixelWidth);
    int frameSize = qptr()->textureDataWidth() * m_textureHeight;

    int dataIndex = 0;
    if (axis == Qt::XAxis) {
        for (int i = 0; i < y; i++) {
            const uchar *p = m_textureData->constData()
                    + (index * pixelWidth) + (dataWidth * i);
            for (int j = 0; j < x; j++) {
                for (int k = 0; k < pixelWidth; k++)
                    data[dataIndex++] = *(p + k);
                p += frameSize;
            }
        }
    } else if (axis == Qt::YAxis) {
        for (int i = y - 1; i >= 0; i--) {
            const uchar *p = m_textureData->constData() + (index * dataWidth)
                    + (frameSize * i);
            for (int j = 0; j < (x * pixelWidth); j++) {
                data[dataIndex++] = *p;
                p++;
            }
        }
    } else {
        for (int i = 0; i < y; i++) {
            const uchar *p = m_textureData->constData() + (index * frameSize) + (dataWidth * i);
            for (int j = 0; j < (x * pixelWidth); j++) {
                data[dataIndex++] = *p;
                p++;
            }
        }
    }

    if (m_textureFormat != QImage::Format_Indexed8 && m_alphaMultiplier != 1.0f) {
        for (int i = pixelWidth - 1; i < data.size(); i += pixelWidth)
            data[i] = static_cast<uchar>(multipliedAlphaValue(data.at(i)));
    }

    QImage image(data.constData(), x, y, x * pixelWidth, m_textureFormat);
    image.bits(); // Call bits() to detach the new image from local data
    if (m_textureFormat == QImage::Format_Indexed8) {
        QVector<QRgb> colorTable = m_colorTable;
        if (m_alphaMultiplier != 1.0f) {
            for (int i = 0; i < colorTable.size(); i++) {
                QRgb curCol = colorTable.at(i);
                int alpha = multipliedAlphaValue(qAlpha(curCol));
                if (alpha != qAlpha(curCol))
                    colorTable[i] = qRgba(qRed(curCol), qGreen(curCol), qBlue(curCol), alpha);
            }
        }
        image.setColorTable(colorTable);
    }

    return image;
}

int QCustom3DVolumePrivate::multipliedAlphaValue(int alpha)
{
    int modifiedAlpha = alpha;
    if (!m_preserveOpacity || alpha != 255) {
        modifiedAlpha = int(m_alphaMultiplier * float(alpha));
        modifiedAlpha = qMin(modifiedAlpha, 255);
    }
    return modifiedAlpha;
}

QCustom3DVolume *QCustom3DVolumePrivate::qptr()
{
    return static_cast<QCustom3DVolume *>(q_ptr);
}

QT_END_NAMESPACE_DATAVISUALIZATION
