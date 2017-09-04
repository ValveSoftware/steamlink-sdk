/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include "qcustom3ditem_p.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

/*!
 * \class QCustom3DItem
 * \inmodule QtDataVisualization
 * \brief The QCustom3DItem class adds a custom item to a graph.
 * \since QtDataVisualization 1.1
 *
 * A custom item has a custom mesh, position, scaling, rotation, and an optional
 * texture.
 *
 * \sa QAbstract3DGraph::addCustomItem()
 */

/*!
 * \qmltype Custom3DItem
 * \inqmlmodule QtDataVisualization
 * \since QtDataVisualization 1.1
 * \ingroup datavisualization_qml
 * \instantiates QCustom3DItem
 * \brief Adds a custom item to a graph.
 *
 * A custom item has a custom mesh, position, scaling, rotation, and an optional
 * texture.
 */

/*! \qmlproperty string Custom3DItem::meshFile
 *
 * The item mesh file name. The item in the file must be in Wavefront OBJ format and include
 * vertices, normals, and UVs. It also needs to be in triangles.
 */

/*! \qmlproperty string Custom3DItem::textureFile
 *
 * The texture file name for the item. If left unset, a solid gray texture will be
 * used.
 *
 * \note To conserve memory, the QImage loaded from the file is cleared after a
 * texture is created.
 */

/*! \qmlproperty vector3d Custom3DItem::position
 *
 * The item position as a \l vector3d type. Defaults to
 * \c {vector3d(0.0, 0.0, 0.0)}.
 *
 * Item position is specified either in data coordinates or in absolute
 * coordinates, depending on the value of the positionAbsolute property. When
 * using absolute coordinates, values between \c{-1.0...1.0} are
 * within axis ranges.
 *
 * \note Items positioned outside any axis range are not rendered if positionAbsolute is \c{false},
 * unless the item is a Custom3DVolume that would be partially visible and scalingAbsolute is also
 * \c{false}. In that case, the visible portion of the volume will be rendered.
 *
 * \sa positionAbsolute, scalingAbsolute
 */

/*! \qmlproperty bool Custom3DItem::positionAbsolute
 *
 * Defines whether item position is to be handled in data coordinates or in absolute
 * coordinates. Defaults to \c{false}. Items with absolute coordinates will always be rendered,
 * whereas items with data coordinates are only rendered if they are within axis ranges.
 *
 * \sa position
 */

/*! \qmlproperty vector3d Custom3DItem::scaling
 *
 * The item scaling as a \l vector3d type. Defaults to
 * \c {vector3d(0.1, 0.1, 0.1)}.
 *
 * Item scaling is specified either in data values or in absolute values,
 * depending on the value of the scalingAbsolute property. The default vector
 * interpreted as absolute values sets the item to
 * 10% of the height of the graph, provided the item mesh is normalized and the graph aspect ratios
 * have not been changed from the defaults.
 *
 * \sa scalingAbsolute
 */

/*! \qmlproperty bool Custom3DItem::scalingAbsolute
 * \since QtDataVisualization 1.2
 *
 * Defines whether item scaling is to be handled in data values or in absolute
 * values. Defaults to \c{true}. Items with absolute scaling will be rendered at the same
 * size, regardless of axis ranges. Items with data scaling will change their apparent size
 * according to the axis ranges. If positionAbsolute is \c{true}, this property is ignored
 * and scaling is interpreted as an absolute value. If the item has rotation, the data scaling
 * is calculated on the unrotated item. Similarly, for Custom3DVolume items, the range clipping
 * is calculated on the unrotated item.
 *
 * \note Only absolute scaling is supported for Custom3DLabel items or for custom items used in
 * \l{AbstractGraph3D::polar}{polar} graphs.
 *
 * \note The custom item's mesh must be normalized to the range \c{[-1 ,1]}, or the data
 * scaling will not be accurate.
 *
 * \sa scaling, positionAbsolute
 */

/*! \qmlproperty quaternion Custom3DItem::rotation
 *
 * The item rotation as a \l quaternion. Defaults to
 * \c {quaternion(0.0, 0.0, 0.0, 0.0)}.
 */

/*! \qmlproperty bool Custom3DItem::visible
 *
 * The visibility of the item. Defaults to \c{true}.
 */

/*! \qmlproperty bool Custom3DItem::shadowCasting
 *
 * Defines whether shadow casting for the item is enabled. Defaults to \c{true}.
 * If \c{false}, the item does not cast shadows regardless of
 * \l{QAbstract3DGraph::ShadowQuality}{ShadowQuality}.
 */

/*!
 * \qmlmethod void Custom3DItem::setRotationAxisAndAngle(vector3d axis, real angle)
 *
 * A convenience function to construct the rotation quaternion from \a axis and
 * \a angle.
 *
 * \sa rotation
 */

/*!
 * Constructs a custom 3D item with the specified \a parent.
 */
QCustom3DItem::QCustom3DItem(QObject *parent) :
    QObject(parent),
    d_ptr(new QCustom3DItemPrivate(this))
{
    setTextureImage(QImage());
}

/*!
 * \internal
 */
QCustom3DItem::QCustom3DItem(QCustom3DItemPrivate *d, QObject *parent) :
    QObject(parent),
    d_ptr(d)
{
    setTextureImage(QImage());
}

/*!
 * Constructs a custom 3D item with the specified \a meshFile, \a position, \a scaling,
 * \a rotation, \a texture image, and optional \a parent.
 */
QCustom3DItem::QCustom3DItem(const QString &meshFile, const QVector3D &position,
                             const QVector3D &scaling, const QQuaternion &rotation,
                             const QImage &texture, QObject *parent) :
    QObject(parent),
    d_ptr(new QCustom3DItemPrivate(this, meshFile, position, scaling, rotation))
{
    setTextureImage(texture);
}

/*!
 * Deletes the custom 3D item.
 */
QCustom3DItem::~QCustom3DItem()
{
}

/*! \property QCustom3DItem::meshFile
 *
 * \brief The item mesh file name.
 *
 * The item in the file must be in Wavefront OBJ format and include
 * vertices, normals, and UVs. It also needs to be in triangles.
 */
void QCustom3DItem::setMeshFile(const QString &meshFile)
{
    if (d_ptr->m_meshFile != meshFile) {
        d_ptr->m_meshFile = meshFile;
        d_ptr->m_dirtyBits.meshDirty = true;
        emit meshFileChanged(meshFile);
        emit d_ptr->needUpdate();
    }
}

QString QCustom3DItem::meshFile() const
{
    return d_ptr->m_meshFile;
}

/*! \property QCustom3DItem::position
 *
 * \brief The item position as a QVector3D.
 *
 * Defaults to \c {QVector3D(0.0, 0.0, 0.0)}.
 *
 * Item position is specified either in data coordinates or in absolute
 * coordinates, depending on the
 * positionAbsolute property. When using absolute coordinates, values between \c{-1.0...1.0} are
 * within axis ranges.
 *
 * \note Items positioned outside any axis range are not rendered if positionAbsolute is \c{false},
 * unless the item is a QCustom3DVolume that would be partially visible and scalingAbsolute is also
 * \c{false}. In that case, the visible portion of the volume will be rendered.
 *
 * \sa positionAbsolute
 */
void QCustom3DItem::setPosition(const QVector3D &position)
{
    if (d_ptr->m_position != position) {
        d_ptr->m_position = position;
        d_ptr->m_dirtyBits.positionDirty = true;
        emit positionChanged(position);
        emit d_ptr->needUpdate();
    }
}

QVector3D QCustom3DItem::position() const
{
    return d_ptr->m_position;
}

/*! \property QCustom3DItem::positionAbsolute
 *
 * \brief Whether item position is to be handled in data coordinates or in absolute
 * coordinates.
 *
 * Defaults to \c{false}. Items with absolute coordinates will always be rendered,
 * whereas items with data coordinates are only rendered if they are within axis ranges.
 *
 * \sa position
 */
void QCustom3DItem::setPositionAbsolute(bool positionAbsolute)
{
    if (d_ptr->m_positionAbsolute != positionAbsolute) {
        d_ptr->m_positionAbsolute = positionAbsolute;
        d_ptr->m_dirtyBits.positionDirty = true;
        emit positionAbsoluteChanged(positionAbsolute);
        emit d_ptr->needUpdate();
    }
}

bool QCustom3DItem::isPositionAbsolute() const
{
    return d_ptr->m_positionAbsolute;
}

/*! \property QCustom3DItem::scaling
 *
 * \brief The item scaling as a QVector3D.
 *
 * Defaults to \c {QVector3D(0.1, 0.1, 0.1)}.
 *
 * Item scaling is either in data values or in absolute values, depending on the
 * scalingAbsolute property. The default vector interpreted as absolute values sets the item to
 * 10% of the height of the graph, provided the item mesh is normalized and the graph aspect ratios
 * have not been changed from the defaults.
 *
 * \sa scalingAbsolute
 */
void QCustom3DItem::setScaling(const QVector3D &scaling)
{
    if (d_ptr->m_scaling != scaling) {
        d_ptr->m_scaling = scaling;
        d_ptr->m_dirtyBits.scalingDirty = true;
        emit scalingChanged(scaling);
        emit d_ptr->needUpdate();
    }
}

QVector3D QCustom3DItem::scaling() const
{
    return d_ptr->m_scaling;
}

/*! \property QCustom3DItem::scalingAbsolute
 * \since QtDataVisualization 1.2
 *
 * \brief Whether item scaling is to be handled in data values or in absolute
 * values.
 *
 * Defaults to \c{true}.
 *
 * Items with absolute scaling will be rendered at the same
 * size, regardless of axis ranges. Items with data scaling will change their apparent size
 * according to the axis ranges. If positionAbsolute is \c{true}, this property is ignored
 * and scaling is interpreted as an absolute value. If the item has rotation, the data scaling
 * is calculated on the unrotated item. Similarly, for QCustom3DVolume items, the range clipping
 * is calculated on the unrotated item.
 *
 * \note Only absolute scaling is supported for QCustom3DLabel items or for custom items used in
 * \l{QAbstract3DGraph::polar}{polar} graphs.
 *
 * \note The custom item's mesh must be normalized to the range \c{[-1 ,1]}, or the data
 * scaling will not be accurate.
 *
 * \sa scaling, positionAbsolute
 */
void QCustom3DItem::setScalingAbsolute(bool scalingAbsolute)
{
    if (d_ptr->m_isLabelItem && !scalingAbsolute) {
        qWarning() << __FUNCTION__ << "Data bounds are not supported for label items.";
    } else if (d_ptr->m_scalingAbsolute != scalingAbsolute) {
        d_ptr->m_scalingAbsolute = scalingAbsolute;
        d_ptr->m_dirtyBits.scalingDirty = true;
        emit scalingAbsoluteChanged(scalingAbsolute);
        emit d_ptr->needUpdate();
    }
}

bool QCustom3DItem::isScalingAbsolute() const
{
    return d_ptr->m_scalingAbsolute;
}

/*! \property QCustom3DItem::rotation
 *
 * \brief The item rotation as a QQuaternion.
 *
 * Defaults to \c {QQuaternion(0.0, 0.0, 0.0, 0.0)}.
 */
void QCustom3DItem::setRotation(const QQuaternion &rotation)
{
    if (d_ptr->m_rotation != rotation) {
        d_ptr->m_rotation = rotation;
        d_ptr->m_dirtyBits.rotationDirty = true;
        emit rotationChanged(rotation);
        emit d_ptr->needUpdate();
    }
}

QQuaternion QCustom3DItem::rotation()
{
    return d_ptr->m_rotation;
}

/*! \property QCustom3DItem::visible
 *
 * \brief The visibility of the item.
 *
 * Defaults to \c{true}.
 */
void QCustom3DItem::setVisible(bool visible)
{
    if (d_ptr->m_visible != visible) {
        d_ptr->m_visible = visible;
        d_ptr->m_dirtyBits.visibleDirty = true;
        emit visibleChanged(visible);
        emit d_ptr->needUpdate();
    }
}

bool QCustom3DItem::isVisible() const
{
    return d_ptr->m_visible;
}


/*! \property QCustom3DItem::shadowCasting
 *
 * \brief Whether shadow casting for the item is enabled.
 *
 * Defaults to \c{true}.
 * If \c{false}, the item does not cast shadows regardless of QAbstract3DGraph::ShadowQuality.
 */
void QCustom3DItem::setShadowCasting(bool enabled)
{
    if (d_ptr->m_shadowCasting != enabled) {
        d_ptr->m_shadowCasting = enabled;
        d_ptr->m_dirtyBits.shadowCastingDirty = true;
        emit shadowCastingChanged(enabled);
        emit d_ptr->needUpdate();
    }
}

bool QCustom3DItem::isShadowCasting() const
{
    return d_ptr->m_shadowCasting;
}

/*!
 * A convenience function to construct the rotation quaternion from \a axis and
 * \a angle.
 *
 * \sa rotation
 */
void QCustom3DItem::setRotationAxisAndAngle(const QVector3D &axis, float angle)
{
    setRotation(QQuaternion::fromAxisAndAngle(axis, angle));
}

/*!
 * Sets the value of \a textureImage as a QImage for the item. The texture
 * defaults to solid gray.
 *
 * \note To conserve memory, the given QImage is cleared after a texture is
 * created.
 */
void QCustom3DItem::setTextureImage(const QImage &textureImage)
{
    if (textureImage != d_ptr->m_textureImage) {
        if (textureImage.isNull()) {
            // Make a solid gray texture
            d_ptr->m_textureImage = QImage(2, 2, QImage::Format_RGB32);
            d_ptr->m_textureImage.fill(Qt::gray);
        } else {
            d_ptr->m_textureImage = textureImage;
        }

        if (!d_ptr->m_textureFile.isEmpty()) {
            d_ptr->m_textureFile.clear();
            emit textureFileChanged(d_ptr->m_textureFile);
        }
        d_ptr->m_dirtyBits.textureDirty = true;
        emit d_ptr->needUpdate();
    }
}

/*! \property QCustom3DItem::textureFile
 *
 * \brief The texture file name for the item.
 *
 * If both this property and the texture image are unset, a solid
 * gray texture will be used.
 *
 * \note To conserve memory, the QImage loaded from the file is cleared after a
 * texture is created.
 */
void QCustom3DItem::setTextureFile(const QString &textureFile)
{
    if (d_ptr->m_textureFile != textureFile) {
        d_ptr->m_textureFile = textureFile;
        if (!textureFile.isEmpty()) {
            d_ptr->m_textureImage = QImage(textureFile);
        } else {
            d_ptr->m_textureImage = QImage(2, 2, QImage::Format_RGB32);
            d_ptr->m_textureImage.fill(Qt::gray);
        }
        emit textureFileChanged(textureFile);
        d_ptr->m_dirtyBits.textureDirty = true;
        emit d_ptr->needUpdate();
    }
}

QString QCustom3DItem::textureFile() const
{
    return d_ptr->m_textureFile;
}

QCustom3DItemPrivate::QCustom3DItemPrivate(QCustom3DItem *q) :
    q_ptr(q),
    m_textureImage(QImage(1, 1, QImage::Format_ARGB32)),
    m_position(QVector3D(0.0f, 0.0f, 0.0f)),
    m_positionAbsolute(false),
    m_scaling(QVector3D(0.1f, 0.1f, 0.1f)),
    m_scalingAbsolute(true),
    m_rotation(identityQuaternion),
    m_visible(true),
    m_shadowCasting(true),
    m_isLabelItem(false),
    m_isVolumeItem(false)
{
}

QCustom3DItemPrivate::QCustom3DItemPrivate(QCustom3DItem *q, const QString &meshFile,
                                           const QVector3D &position, const QVector3D &scaling,
                                           const QQuaternion &rotation) :
    q_ptr(q),
    m_textureImage(QImage(1, 1, QImage::Format_ARGB32)),
    m_meshFile(meshFile),
    m_position(position),
    m_positionAbsolute(false),
    m_scaling(scaling),
    m_scalingAbsolute(true),
    m_rotation(rotation),
    m_visible(true),
    m_shadowCasting(true),
    m_isLabelItem(false),
    m_isVolumeItem(false)
{
}

QCustom3DItemPrivate::~QCustom3DItemPrivate()
{
}

QImage QCustom3DItemPrivate::textureImage()
{
    return m_textureImage;
}

void QCustom3DItemPrivate::clearTextureImage()
{
    m_textureImage = QImage();
    m_textureFile.clear();
}

void QCustom3DItemPrivate::resetDirtyBits()
{
    m_dirtyBits.textureDirty = false;
    m_dirtyBits.meshDirty = false;
    m_dirtyBits.positionDirty = false;
    m_dirtyBits.scalingDirty = false;
    m_dirtyBits.rotationDirty = false;
    m_dirtyBits.visibleDirty = false;
    m_dirtyBits.shadowCastingDirty = false;
}

QT_END_NAMESPACE_DATAVISUALIZATION
