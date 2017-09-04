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

#include "qheightmapsurfacedataproxy_p.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

// Default ranges correspond value axis defaults
const float defaultMinValue = 0.0f;
const float defaultMaxValue = 10.0f;

/*!
 * \class QHeightMapSurfaceDataProxy
 * \inmodule QtDataVisualization
 * \brief Base proxy class for Q3DSurface.
 * \since QtDataVisualization 1.0
 *
 * QHeightMapSurfaceDataProxy takes care of surface related height map data handling. It provides a
 * way to give a height map to be visualized as a surface plot.
 *
 * Since height maps do not contain values for X or Z axes, those values need to be given
 * separately using minXValue, maxXValue, minZValue, and maxZValue properties. X-value corresponds
 * to image horizontal direction and Z-value to the vertical. Setting any of these
 * properties triggers asynchronous re-resolving of any existing height map.
 *
 * \sa QSurfaceDataProxy, {Qt Data Visualization Data Handling}
 */

/*!
 * \qmltype HeightMapSurfaceDataProxy
 * \inqmlmodule QtDataVisualization
 * \since QtDataVisualization 1.0
 * \ingroup datavisualization_qml
 * \instantiates QHeightMapSurfaceDataProxy
 * \inherits SurfaceDataProxy
 * \brief Base proxy type for Surface3D.
 *
 * HeightMapSurfaceDataProxy takes care of surface related height map data handling. It provides a
 * way to give a height map to be visualized as a surface plot.
 *
 * For more complete description, see QHeightMapSurfaceDataProxy.
 *
 * \sa {Qt Data Visualization Data Handling}
 */

/*!
 * \qmlproperty string HeightMapSurfaceDataProxy::heightMapFile
 *
 * A file with a height map image to be visualized. Setting this property replaces current data
 * with height map data.
 *
 * There are several formats the image file can be given in, but if it is not in a directly usable
 * format, a conversion is made.
 *
 * \note If the result seems wrong, the automatic conversion failed
 * and you should try converting the image yourself before setting it. Preferred format is
 * QImage::Format_RGB32 in grayscale.
 *
 * The height of the image is read from the red component of the pixels if the image is in grayscale,
 * otherwise it is an average calculated from red, green and blue components of the pixels. Using
 * grayscale images may improve data conversion speed for large images.
 *
 * Since height maps do not contain values for X or Z axes, those values need to be given
 * separately using minXValue, maxXValue, minZValue, and maxZValue properties. X-value corresponds
 * to image horizontal direction and Z-value to the vertical. Setting any of these
 * properties triggers asynchronous re-resolving of any existing height map.
 *
 * Not recommended formats: all mono formats (for example QImage::Format_Mono).
 */

/*!
 * \qmlproperty real HeightMapSurfaceDataProxy::minXValue
 *
 * The minimum X value for the generated surface points. Defaults to \c{0.0}.
 * When setting this property the corresponding maximum value is adjusted if necessary,
 * to ensure that the range remains valid.
 */

/*!
 * \qmlproperty real HeightMapSurfaceDataProxy::maxXValue
 *
 * The maximum X value for the generated surface points. Defaults to \c{10.0}.
 * When setting this property the corresponding minimum value is adjusted if necessary,
 * to ensure that the range remains valid.
 */

/*!
 * \qmlproperty real HeightMapSurfaceDataProxy::minZValue
 *
 * The minimum Z value for the generated surface points. Defaults to \c{0.0}.
 * When setting this property the corresponding maximum value is adjusted if necessary,
 * to ensure that the range remains valid.
 */

/*!
 * \qmlproperty real HeightMapSurfaceDataProxy::maxZValue
 *
 * The maximum Z value for the generated surface points. Defaults to \c{10.0}.
 * When setting this property the corresponding minimum value is adjusted if necessary,
 * to ensure that the range remains valid.
 */

/*!
 * Constructs QHeightMapSurfaceDataProxy with the given \a parent.
 */
QHeightMapSurfaceDataProxy::QHeightMapSurfaceDataProxy(QObject *parent) :
    QSurfaceDataProxy(new QHeightMapSurfaceDataProxyPrivate(this), parent)
{
}

/*!
 * Constructs QHeightMapSurfaceDataProxy with the given \a image and \a parent. Height map is set
 * by calling setHeightMap() with \a image.
 *
 * \sa heightMap
 */
QHeightMapSurfaceDataProxy::QHeightMapSurfaceDataProxy(const QImage &image, QObject *parent) :
    QSurfaceDataProxy(new QHeightMapSurfaceDataProxyPrivate(this), parent)
{
    setHeightMap(image);
}

/*!
 * Constructs QHeightMapSurfaceDataProxy from the given image \a filename and \a parent. Height map is set
 * by calling setHeightMapFile() with \a filename.
 *
 * \sa heightMapFile
 */
QHeightMapSurfaceDataProxy::QHeightMapSurfaceDataProxy(const QString &filename, QObject *parent) :
    QSurfaceDataProxy(new QHeightMapSurfaceDataProxyPrivate(this), parent)
{
    setHeightMapFile(filename);
}

/*!
 * \internal
 */
QHeightMapSurfaceDataProxy::QHeightMapSurfaceDataProxy(
        QHeightMapSurfaceDataProxyPrivate *d, QObject *parent) :
    QSurfaceDataProxy(d, parent)
{
}

/*!
 * Destroys QHeightMapSurfaceDataProxy.
 */
QHeightMapSurfaceDataProxy::~QHeightMapSurfaceDataProxy()
{
}

/*!
 * \property QHeightMapSurfaceDataProxy::heightMap
 *
 * \brief The height map image to be visualized.
 */

/*!
 * Replaces current data with the height map data specified by \a image.
 *
 * There are several formats the \a image can be given in, but if it is not in a directly usable
 * format, a conversion is made.
 *
 * \note If the result seems wrong, the automatic conversion failed
 * and you should try converting the \a image yourself before setting it. Preferred format is
 * QImage::Format_RGB32 in grayscale.
 *
 * The height of the \a image is read from the red component of the pixels if the \a image is in
 * grayscale, otherwise it is an average calculated from red, green, and blue components of the
 * pixels. Using grayscale images may improve data conversion speed for large images.
 *
 * Not recommended formats: all mono formats (for example QImage::Format_Mono).
 *
 * The height map is resolved asynchronously. QSurfaceDataProxy::arrayReset() is emitted when the
 * data has been resolved.
 */
void QHeightMapSurfaceDataProxy::setHeightMap(const QImage &image)
{
    dptr()->m_heightMap = image;

    // We do resolving asynchronously to make qml onArrayReset handlers actually get the initial reset
    if (!dptr()->m_resolveTimer.isActive())
        dptr()->m_resolveTimer.start(0);
}

QImage QHeightMapSurfaceDataProxy::heightMap() const
{
    return dptrc()->m_heightMap;
}

/*!
 * \property QHeightMapSurfaceDataProxy::heightMapFile
 *
 * \brief The name of the file with a height map image to be visualized.
 */

/*!
 * Replaces current data with height map data from the file specified by
 * \a filename.
 *
 * \sa heightMap
 */
void QHeightMapSurfaceDataProxy::setHeightMapFile(const QString &filename)
{
    dptr()->m_heightMapFile = filename;
    setHeightMap(QImage(filename));
    emit heightMapFileChanged(filename);
}

QString QHeightMapSurfaceDataProxy::heightMapFile() const
{
    return dptrc()->m_heightMapFile;
}

/*!
 * A convenience function for setting all minimum (\a minX and \a minZ) and maximum
 * (\a maxX and \a maxZ) values at the same time. The minimum values must be smaller than the
 * corresponding maximum value. Otherwise the values get adjusted so that they are valid.
 */
void QHeightMapSurfaceDataProxy::setValueRanges(float minX, float maxX, float minZ, float maxZ)
{
    dptr()->setValueRanges(minX, maxX, minZ, maxZ);
}

/*!
 * \property QHeightMapSurfaceDataProxy::minXValue
 *
 * \brief The minimum X value for the generated surface points.
 *
 * Defaults to \c{0.0}.
 *
 * When setting this property the corresponding maximum value is adjusted if necessary,
 * to ensure that the range remains valid.
 */
void QHeightMapSurfaceDataProxy::setMinXValue(float min)
{
    dptr()->setMinXValue(min);
}

float QHeightMapSurfaceDataProxy::minXValue() const
{
    return dptrc()->m_minXValue;
}

/*!
 * \property QHeightMapSurfaceDataProxy::maxXValue
 *
 * \brief The maximum X value for the generated surface points.
 *
 * Defaults to \c{10.0}.
 *
 * When setting this property the corresponding minimum value is adjusted if necessary,
 * to ensure that the range remains valid.
 */
void QHeightMapSurfaceDataProxy::setMaxXValue(float max)
{
    dptr()->setMaxXValue(max);
}

float QHeightMapSurfaceDataProxy::maxXValue() const
{
    return dptrc()->m_maxXValue;
}

/*!
 * \property QHeightMapSurfaceDataProxy::minZValue
 *
 * \brief The minimum Z value for the generated surface points.
 *
 * Defaults to \c{0.0}.
 *
 * When setting this property the corresponding maximum value is adjusted if necessary,
 * to ensure that the range remains valid.
 */
void QHeightMapSurfaceDataProxy::setMinZValue(float min)
{
    dptr()->setMinZValue(min);
}

float QHeightMapSurfaceDataProxy::minZValue() const
{
    return dptrc()->m_minZValue;
}

/*!
 * \property QHeightMapSurfaceDataProxy::maxZValue
 *
 * \brief The maximum Z value for the generated surface points.
 *
 * Defaults to \c{10.0}.
 *
 * When setting this property the corresponding minimum value is adjusted if necessary,
 * to ensure that the range remains valid.
 */
void QHeightMapSurfaceDataProxy::setMaxZValue(float max)
{
    dptr()->setMaxZValue(max);
}

float QHeightMapSurfaceDataProxy::maxZValue() const
{
    return dptrc()->m_maxZValue;
}

/*!
 * \internal
 */
QHeightMapSurfaceDataProxyPrivate *QHeightMapSurfaceDataProxy::dptr()
{
    return static_cast<QHeightMapSurfaceDataProxyPrivate *>(d_ptr.data());
}

/*!
 * \internal
 */
const QHeightMapSurfaceDataProxyPrivate *QHeightMapSurfaceDataProxy::dptrc() const
{
    return static_cast<const QHeightMapSurfaceDataProxyPrivate *>(d_ptr.data());
}

//  QHeightMapSurfaceDataProxyPrivate

QHeightMapSurfaceDataProxyPrivate::QHeightMapSurfaceDataProxyPrivate(QHeightMapSurfaceDataProxy *q)
    : QSurfaceDataProxyPrivate(q),
      m_minXValue(defaultMinValue),
      m_maxXValue(defaultMaxValue),
      m_minZValue(defaultMinValue),
      m_maxZValue(defaultMaxValue)
{
    m_resolveTimer.setSingleShot(true);
    QObject::connect(&m_resolveTimer, &QTimer::timeout,
                     this, &QHeightMapSurfaceDataProxyPrivate::handlePendingResolve);
}

QHeightMapSurfaceDataProxyPrivate::~QHeightMapSurfaceDataProxyPrivate()
{
}

QHeightMapSurfaceDataProxy *QHeightMapSurfaceDataProxyPrivate::qptr()
{
    return static_cast<QHeightMapSurfaceDataProxy *>(q_ptr);
}

void QHeightMapSurfaceDataProxyPrivate::setValueRanges(float minX, float maxX,
                                                       float minZ, float maxZ)
{
    bool minXChanged = false;
    bool maxXChanged = false;
    bool minZChanged = false;
    bool maxZChanged = false;
    if (m_minXValue != minX) {
        m_minXValue = minX;
        minXChanged = true;
    }
    if (m_minZValue != minZ) {
        m_minZValue = minZ;
        minZChanged = true;
    }
    if (m_maxXValue != maxX || minX >= maxX) {
        if (minX >= maxX) {
            m_maxXValue = minX + 1.0f;
            qWarning() << "Warning: Tried to set invalid range for X value range."
                          " Range automatically adjusted to a valid one:"
                       << minX << "-" << maxX << "-->" << m_minXValue << "-" << m_maxXValue;
        } else {
            m_maxXValue = maxX;
        }
        maxXChanged = true;
    }
    if (m_maxZValue != maxZ || minZ >= maxZ) {
        if (minZ >= maxZ) {
            m_maxZValue = minZ + 1.0f;
            qWarning() << "Warning: Tried to set invalid range for Z value range."
                          " Range automatically adjusted to a valid one:"
                       << minZ << "-" << maxZ << "-->" << m_minZValue << "-" << m_maxZValue;
        } else {
            m_maxZValue = maxZ;
        }
        maxZChanged = true;
    }

    if (minXChanged)
        emit qptr()->minXValueChanged(m_minXValue);
    if (minZChanged)
        emit qptr()->minZValueChanged(m_minZValue);
    if (maxXChanged)
        emit qptr()->maxXValueChanged(m_maxXValue);
    if (maxZChanged)
        emit qptr()->maxZValueChanged(m_maxZValue);

    if ((minXChanged || minZChanged || maxXChanged || maxZChanged) && !m_resolveTimer.isActive())
        m_resolveTimer.start(0);
}

void QHeightMapSurfaceDataProxyPrivate::setMinXValue(float min)
{
    if (min != m_minXValue) {
        bool maxChanged = false;
        if (min >= m_maxXValue) {
            float oldMax = m_maxXValue;
            m_maxXValue = min + 1.0f;
            qWarning() << "Warning: Tried to set minimum X to equal or larger than maximum X for"
                          " value range. Maximum automatically adjusted to a valid one:"
                       << oldMax <<  "-->" << m_maxXValue;
            maxChanged = true;
        }
        m_minXValue = min;
        emit qptr()->minXValueChanged(m_minXValue);
        if (maxChanged)
            emit qptr()->maxXValueChanged(m_maxXValue);

        if (!m_resolveTimer.isActive())
            m_resolveTimer.start(0);
    }
}

void QHeightMapSurfaceDataProxyPrivate::setMaxXValue(float max)
{
    if (m_maxXValue != max) {
        bool minChanged = false;
        if (max <= m_minXValue) {
            float oldMin = m_minXValue;
            m_minXValue = max - 1.0f;
            qWarning() << "Warning: Tried to set maximum X to equal or smaller than minimum X for"
                          " value range. Minimum automatically adjusted to a valid one:"
                       << oldMin <<  "-->" << m_minXValue;
            minChanged = true;
        }
        m_maxXValue = max;
        emit qptr()->maxXValueChanged(m_maxXValue);
        if (minChanged)
            emit qptr()->minXValueChanged(m_minXValue);

        if (!m_resolveTimer.isActive())
            m_resolveTimer.start(0);
    }
}

void QHeightMapSurfaceDataProxyPrivate::setMinZValue(float min)
{
    if (min != m_minZValue) {
        bool maxChanged = false;
        if (min >= m_maxZValue) {
            float oldMax = m_maxZValue;
            m_maxZValue = min + 1.0f;
            qWarning() << "Warning: Tried to set minimum Z to equal or larger than maximum Z for"
                          " value range. Maximum automatically adjusted to a valid one:"
                       << oldMax <<  "-->" << m_maxZValue;
            maxChanged = true;
        }
        m_minZValue = min;
        emit qptr()->minZValueChanged(m_minZValue);
        if (maxChanged)
            emit qptr()->maxZValueChanged(m_maxZValue);

        if (!m_resolveTimer.isActive())
            m_resolveTimer.start(0);
    }
}

void QHeightMapSurfaceDataProxyPrivate::setMaxZValue(float max)
{
    if (m_maxZValue != max) {
        bool minChanged = false;
        if (max <= m_minZValue) {
            float oldMin = m_minZValue;
            m_minZValue = max - 1.0f;
            qWarning() << "Warning: Tried to set maximum Z to equal or smaller than minimum Z for"
                          " value range. Minimum automatically adjusted to a valid one:"
                       << oldMin <<  "-->" << m_minZValue;
            minChanged = true;
        }
        m_maxZValue = max;
        emit qptr()->maxZValueChanged(m_maxZValue);
        if (minChanged)
            emit qptr()->minZValueChanged(m_minZValue);

        if (!m_resolveTimer.isActive())
            m_resolveTimer.start(0);
    }
}

void QHeightMapSurfaceDataProxyPrivate::handlePendingResolve()
{
    QImage heightImage = m_heightMap;

    // Convert to RGB32 to be sure we're reading the right bytes
    if (heightImage.format() != QImage::Format_RGB32)
        heightImage = heightImage.convertToFormat(QImage::Format_RGB32);

    uchar *bits = heightImage.bits();

    int imageHeight = heightImage.height();
    int imageWidth = heightImage.width();
    int bitCount = imageWidth * 4 * (imageHeight - 1);
    int widthBits = imageWidth * 4;
    float height = 0;

    // Do not recreate array if dimensions have not changed
    QSurfaceDataArray *dataArray = m_dataArray;
    if (imageWidth != qptr()->columnCount() || imageHeight != dataArray->size()) {
        dataArray = new QSurfaceDataArray;
        dataArray->reserve(imageHeight);
        for (int i = 0; i < imageHeight; i++) {
            QSurfaceDataRow *newProxyRow = new QSurfaceDataRow(imageWidth);
            dataArray->append(newProxyRow);
        }
    }

    float xMul = (m_maxXValue - m_minXValue) / float(imageWidth - 1);
    float zMul = (m_maxZValue - m_minZValue) / float(imageHeight - 1);

    // Last row and column are explicitly set to max values, as relying
    // on multiplier can cause rounding errors, resulting in the value being
    // slightly over the specified maximum, which in turn can lead to it not
    // getting rendered.
    int lastRow = imageHeight - 1;
    int lastCol = imageWidth - 1;

    if (heightImage.isGrayscale()) {
        // Grayscale, it's enough to read Red byte
        for (int i = 0; i < imageHeight; i++, bitCount -= widthBits) {
            QSurfaceDataRow &newRow = *dataArray->at(i);
            float zVal;
            if (i == lastRow)
                zVal = m_maxZValue;
            else
                zVal = (float(i) * zMul) + m_minZValue;
            int j = 0;
            for (; j < lastCol; j++)
                newRow[j].setPosition(QVector3D((float(j) * xMul) + m_minXValue,
                                                float(bits[bitCount + (j * 4)]),
                                      zVal));
            newRow[j].setPosition(QVector3D(m_maxXValue,
                                            float(bits[bitCount + (j * 4)]),
                                  zVal));
        }
    } else {
        // Not grayscale, we'll need to calculate height from RGB
        for (int i = 0; i < imageHeight; i++, bitCount -= widthBits) {
            QSurfaceDataRow &newRow = *dataArray->at(i);
            float zVal;
            if (i == lastRow)
                zVal = m_maxZValue;
            else
                zVal = (float(i) * zMul) + m_minZValue;
            int j = 0;
            int nextpixel = 0;
            for (; j < lastCol; j++) {
                nextpixel = j * 4;
                height = (float(bits[bitCount + nextpixel])
                        + float(bits[1 + bitCount + nextpixel])
                        + float(bits[2 + bitCount + nextpixel]));
                newRow[j].setPosition(QVector3D((float(j) * xMul) + m_minXValue,
                                                height / 3.0f,
                                                zVal));
            }
            nextpixel = j * 4;
            height = (float(bits[bitCount + nextpixel])
                    + float(bits[1 + bitCount + nextpixel])
                    + float(bits[2 + bitCount + nextpixel]));
            newRow[j].setPosition(QVector3D(m_maxXValue,
                                            height / 3.0f,
                                            zVal));
        }
    }

    qptr()->resetArray(dataArray);
    emit qptr()->heightMapChanged(m_heightMap);
}

QT_END_NAMESPACE_DATAVISUALIZATION
