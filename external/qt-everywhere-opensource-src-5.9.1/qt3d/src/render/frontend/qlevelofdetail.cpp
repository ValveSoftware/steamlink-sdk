/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include "qlevelofdetail.h"
#include "qlevelofdetail_p.h"
#include "qcamera.h"
#include <Qt3DCore/qpropertyupdatedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

QLevelOfDetailPrivate::QLevelOfDetailPrivate()
    : QComponentPrivate()
    , m_camera(nullptr)
    , m_currentIndex(0)
    , m_thresholdType(QLevelOfDetail::DistanceToCameraThreshold)
    , m_volumeOverride()
{
}

/*!
    \class Qt3DRender::QLevelOfDetail
    \inmodule Qt3DRender
    \since 5.9
    \brief The QLevelOfDetail class provides a way of controlling the complexity
    of rendered entities based on their size on the screen.

    QLevelOfDetail can be used to control the representation of an entity
    based on distance from the observer or size on the screen.

    In order to improve rendering performance, objects that are very small
    can be rendered using far fewer details, in geometry or texture.

    The component is controlled by specifying thresholds of values which are interpreted
    as either distances from the camera or screen size.

    As the point of view changes, the currentIndex property will change to
    reflect matching value in the range array.

    The currentIndex property can then be used, for example, to enable or
    disable entities, change material, etc.

    The LevelOfDetail component is not shareable between multiple \l [QML]{Entity}{entities}.

    \code
     #include <Qt3DCore/QEntity>
     #include <Qt3DRender/QGeometryRenderer>
     #include <Qt3DRender/QLevelOfDetail>

    // Scene
    Qt3DCore::QEntity *rootEntity = new Qt3DCore::Qt3DCore::QEntity;

    Qt3DCore::QEntity *renderableEntity = new Qt3DCore::QEntity(rootEntity);
    Qt3DRender::QGeometryRenderer *geometryRenderer = new Qt3DCore::QGeometryRenderer(renderableEntity);
    renderableEntity->addComponent(geometryRenderer);
    Qt3DRender::QLevelOfDetail* lod = new Qt3Render::QLevelOfDetail(renderableEntity);
    QVector<qreal> thresholds = {20, 35, 50, 65};
    lod->setThresholds(thresholds);
    lod->setCamera(mainCamera);
    renderableEntity->addComponent(lod);

    // connect to QLevelOfDetail::currentIndexChanged to toggle rendering
    ...
    \endcode
*/

/*!
    \qmltype LevelOfDetail
    \instantiates Qt3DRender::QLevelOfDetail
    \inherits Component3D
    \inqmlmodule Qt3D.Render
    \since 5.9
    \brief LevelOfDetail provides a way of controlling the complexity of
    rendered entities based on their size on the screen.

    LevelOfDetail can be used to control the representation of an entity
    based on distance from the observer or size on the screen.

    In order to improve rendering performance, objects that are very small
    can be rendered using far fewer details, in geometry or texture.

    The component is controlled by specifying thresholds of values which are interpreted
    as either distances from the camera or screen size.

    As the point of view changes, the currentIndex property will change to
    reflect matching value in the range array.

    The currentIndex property can then be used, for example, to enable or
    disable entities, change source in an EntityLoader, or material.

    The LevelOfDetail component is not shareable between multiple Entity's.

    \code
    import Qt3D.Core 2.0
    import Qt3D.Render 2.0

    Entity {
        id: root

        // Scene
        Camera { id: mainCamera }

        LevelOfDetail {
            id: lod
            camera: mainCamera
            thresholds: [20, 35, 50, 65]
            thresholdType: LevelOfDetail.DistanceToCameraThreshold
        }

        CylinderMesh {
            id: mesh

            property var sliceValues: [20, 10, 6, 4]
            slices: sliceValues[lod.currentIndex]
        }

        Entity {
            id: renderableEntity
            components: [ mesh, lod ]
        }
    }
    \endcode
*/


/*!
 * \enum Qt3DRender::QLevelOfDetail::ThresholdType
 *
 * Specifies how the values in the thresholds are interpreted
 *
 * \value DistanceToCameraThreshold Distance from the entity to the selected camera
 * \value ProjectedScreenPixelSizeThreshold Size of the entity when projected on the
 *          screen as seen from the selected camera, expressed in number of
 *          pixels on the side of the bounding square in screen space.
 */

/*!
 * \qmlproperty enumeration LevelOfDetail::ThresholdType
 *
 * Specifies how the values in the thresholds are interpreted
 *
 * \list
 * \li DistanceToCameraThreshold Distance from the entity to the selected camera
 * \li ProjectedScreenPixelSizeThreshold Size of the entity when projected on the
 *     screen as seen from the selected camera, expressed in number of
 *     pixels on the side of the bounding square in screen space.
 * \endlist
 * \sa Qt3DRender::QLevelOfDetail::ThresholdType
 */

/*!
 * \qmlproperty Camera LevelOfDetail::camera
 *
 * Holds the id of the camera that is used to compute the actual distance or the screen size.
 */

/*!
 * \property QLevelOfDetail::camera
 *
 * Holds the id of the camera that is used to compute the actual distance or the screen size.
 */

/*!
 * \qmlproperty int LevelOfDetail::currentIndex
 *
 * The index in the range array which matches the current distance to the camera or screen size.
 */

/*!
 * \property QLevelOfDetail::currentIndex
 *
 * The index in the range array which matches the current distance to the camera or screen size.
 */

/*!
 * \qmlproperty int LevelOfDetail::thresholdType
 *
 * Specifies how range values are interpreted.
 */

/*!
 * \property QLevelOfDetail::thresholdType
 *
 * Specifies how range values are interpreted.
 * \sa Qt3DRender::QLevelOfDetail::ThresholdType
 */

/*!
 * \qmlproperty QVector<qreal> LevelOfDetail::thresholds
 *
 * Array of range values as float point numbers. The value for the most detailed representation
 * should be specified first.
 *
 * If LevelOfDetail::thresholdType is set to LevelOfDetail.DistanceToCameraThreshold, values should
 * be specified in ascending order, in camera space coordinates
 *
 * If LevelOfDetail::thresholdType is set to LevelOfDetail.ProjectedScreenPixelSizeThreshold, values
 * should be specified in descending order, in screen space pixels.
 */

/*!
 * \property QLevelOfDetail::thresholds
 *
 * Array of range values as float point numbers. The value for the most detailed representation
 * should be specified first.
 *
 * If Qt3DRender::QLevelOfDetail::thresholdType is set to
 * Qt3DRender::QLevelOfDetail::DistanceToCameraThreshold, values should be specified in
 * ascending order, in camera space coordinates
 *
 * If Qt3DRender::QLevelOfDetail::thresholdType is set to
 * Qt3DRender::QLevelOfDetail::ProjectedScreenPixelSizeThreshold, values should
 * be specified in descending order, in screen space pixels.
 *
 * \sa Qt3DRender::QLevelOfDetail::ThresholdType
 */

/*!
 * \qmlproperty int LevelOfDetail::volumeOverride
 *
 * Specifies what is used to evaluate the distance or screen size.
 *
 * The default is a sphere of radius 1 and centered at the local origin of the entity.
 * This proxy volume is used to compute the distance to the camera or the size
 * of the screen projection.
 *
 * If this value to null, the bounding volume of the entity is used. Care must be
 * taken that this bounding volume never becomes invalid.
 *
 * \sa Qt3DRender::QLevelOfDetailBoundingSphere
 */

/*!
 * \property QLevelOfDetail::volumeOverride
 *
 * The default is a sphere of radius 1 and centered at the local origin of the entity.
 * This proxy volume is used to compute the distance to the camera or the size
 * of the screen projection.
 *
 * If this value to nullptr, the bounding volume of the entity is used. Care must be
 * taken that this bounding volume never becomes invalid.
 *
 * \sa Qt3dRender::QLevelOfDetailBoundingSphere
 */


/*! \fn Qt3DRender::QLevelOfDetail::QLevelOfDetail(Qt3DCore::QNode *parent)
  Constructs a new QLevelOfDetail with the specified \a parent.
 */
QLevelOfDetail::QLevelOfDetail(QNode *parent)
    : QComponent(*new QLevelOfDetailPrivate, parent)
{
}

/*! \internal */
QLevelOfDetail::~QLevelOfDetail()
{
}

/*! \internal */
QLevelOfDetail::QLevelOfDetail(QLevelOfDetailPrivate &dd, QNode *parent)
    : QComponent(dd, parent)
{
}

/*! \internal */
Qt3DCore::QNodeCreatedChangeBasePtr QLevelOfDetail::createNodeCreationChange() const
{
    auto creationChange = Qt3DCore::QNodeCreatedChangePtr<QLevelOfDetailData>::create(this);
    auto &data = creationChange->data;

    Q_D(const QLevelOfDetail);
    if (d->m_camera)
        data.camera = d->m_camera->id();
    data.currentIndex = d->m_currentIndex;
    data.thresholdType = d->m_thresholdType;
    data.thresholds = d->m_thresholds;
    data.volumeOverride = d->m_volumeOverride;

    return creationChange;
}

/*! \internal */
void QLevelOfDetail::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &change)
{
    Q_D(QLevelOfDetail);
    Qt3DCore::QPropertyUpdatedChangePtr e = qSharedPointerCast<Qt3DCore::QPropertyUpdatedChange>(change);
    if (e->type() == Qt3DCore::PropertyUpdated) {
        if (e->propertyName() == QByteArrayLiteral("currentIndex")) {
            int ndx = e->value().value<int>();
            d->m_currentIndex = ndx;
            emit currentIndexChanged(ndx);
        }
    }
}

QCamera *QLevelOfDetail::camera() const
{
    Q_D(const QLevelOfDetail);
    return d->m_camera;
}

/*!
 * Sets the \a camera relative to which distance and size are computed.
 */
void QLevelOfDetail::setCamera(QCamera *camera)
{
    Q_D(QLevelOfDetail);
    if (d->m_camera != camera) {
        d->m_camera = camera;
        emit cameraChanged(d->m_camera);
    }
}

int QLevelOfDetail::currentIndex() const
{
    Q_D(const QLevelOfDetail);
    return d->m_currentIndex;
}

/*!
 * Sets the \a currentIndex.
 *
 * \note This should not normally be set by the user.
 *
 * However, if the component is disabled, then changing the
 * current index is a simple way of switching between multiple
 * representations.
 */
void QLevelOfDetail::setCurrentIndex(int currentIndex)
{
    Q_D(QLevelOfDetail);
    if (d->m_currentIndex != currentIndex) {
        d->m_currentIndex = currentIndex;
        emit currentIndexChanged(d->m_currentIndex);
    }
}

QLevelOfDetail::ThresholdType QLevelOfDetail::thresholdType() const
{
    Q_D(const QLevelOfDetail);
    return d->m_thresholdType;
}

/*!
 * Sets the way thresholds values are interpreted
 * with parameter \a thresholdType
 * \sa Qt3DRender::QLevelOfDetail::ThresholdType
 */
void QLevelOfDetail::setThresholdType(QLevelOfDetail::ThresholdType thresholdType)
{
    Q_D(QLevelOfDetail);
    if (d->m_thresholdType != thresholdType) {
        d->m_thresholdType = thresholdType;
        emit thresholdTypeChanged(d->m_thresholdType);
    }
}

QVector<qreal> QLevelOfDetail::thresholds() const
{
    Q_D(const QLevelOfDetail);
    return d->m_thresholds;
}

QLevelOfDetailBoundingSphere QLevelOfDetail::createBoundingSphere(const QVector3D &center, float radius)
{
    return QLevelOfDetailBoundingSphere(center, radius);
}

/*!
 * Sets the range values in \a thresholds.
 * \sa Qt3DRender::QLevelOfDetail::thresholdType
 */
void QLevelOfDetail::setThresholds(const QVector<qreal> &thresholds)
{
    Q_D(QLevelOfDetail);
    if (d->m_thresholds != thresholds) {
        d->m_thresholds = thresholds;
        emit thresholdsChanged(d->m_thresholds);
    }
}

QLevelOfDetailBoundingSphere QLevelOfDetail::volumeOverride() const
{
    Q_D(const QLevelOfDetail);
    return d->m_volumeOverride;
}

void QLevelOfDetail::setVolumeOverride(const QLevelOfDetailBoundingSphere &volumeOverride)
{
    Q_D(QLevelOfDetail);
    if (d->m_volumeOverride != volumeOverride) {
        d->m_volumeOverride = volumeOverride;
        emit volumeOverrideChanged(volumeOverride);
    }
}

} // namespace Qt3DRender

QT_END_NAMESPACE

#include "moc_qlevelofdetail.cpp"
