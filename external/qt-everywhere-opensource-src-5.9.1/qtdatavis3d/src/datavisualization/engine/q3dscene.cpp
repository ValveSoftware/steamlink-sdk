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

#include "q3dscene_p.h"
#include "q3dcamera_p.h"
#include "q3dlight_p.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

/*!
 * \class Q3DScene
 * \inmodule QtDataVisualization
 * \brief Q3DScene class provides description of the 3D scene being visualized.
 * \since QtDataVisualization 1.0
 *
 * The 3D scene contains a single active camera and a single active light source.
 * Visualized data is assumed to be at a fixed location.
 *
 * The 3D scene also keeps track of the viewport in which visualization rendering is done,
 * the primary subviewport inside the viewport where the main 3D data visualization view resides
 * and the secondary subviewport where the 2D sliced view of the data resides. The subviewports are
 * by default resized by the \a Q3DScene. To override the resize behavior you need to listen to both
 * \l viewportChanged() and \l slicingActiveChanged() signals and recalculate the subviewports accordingly.
 *
 * Also the scene has flag for tracking if the secondary 2D slicing view is currently active or not.
 * \note Not all visualizations support the secondary 2D slicing view.
 */

/*!
 * \class Q3DSceneChangeBitField
 * \internal
 */

/*!
 * \qmltype Scene3D
 * \inqmlmodule QtDataVisualization
 * \since QtDataVisualization 1.0
 * \ingroup datavisualization_qml
 * \instantiates Q3DScene
 * \brief Scene3D type provides description of the 3D scene being visualized.
 *
 * The 3D scene contains a single active camera and a single active light source.
 * Visualized data is assumed to be at a fixed location.
 *
 * The 3D scene also keeps track of the viewport in which visualization rendering is done,
 * the primary subviewport inside the viewport where the main 3D data visualization view resides
 * and the secondary subviewport where the 2D sliced view of the data resides.
 *
 * Also the scene has flag for tracking if the secondary 2D slicing view is currently active or not.
 * \note Not all visualizations support the secondary 2D slicing view.
 */

/*!
 * \qmlproperty rect Scene3D::viewport
 *
 * The current viewport rectangle where all 3D rendering is targeted.
 */

/*!
 * \qmlproperty rect Scene3D::primarySubViewport
 *
 * The current subviewport rectangle inside the viewport where the
 * primary view of the data visualization is targeted.
 *
 * If slicingActive is \c false, the primary sub viewport will be equal to the
 * viewport. If slicingActive is \c true and the primary sub viewport has not
 * been explicitly set, it will be one fifth of the viewport.
 * \note Setting primarySubViewport larger than or outside of viewport resizes viewport accordingly.
 */

/*!
 * \qmlproperty rect Scene3D::secondarySubViewport
 *
 * The secondary viewport rectangle inside the viewport. The secondary viewport
 * is used for drawing the 2D slice view in some visualizations. If it has not
 * been explicitly set, it will be null. If slicingActive is \c true, it will
 * be equal to the viewport.
 * \note If the secondary sub viewport is larger than or outside of the
 * viewport, the viewport is resized accordingly.
*/

/*!
 * \qmlproperty point Scene3D::selectionQueryPosition
 *
 * The coordinates for the user input that should be processed
 * by the scene as a selection. If this property is set to a value other than
 * invalidSelectionPoint, the
 * graph tries to select a data item at the given point within the primary viewport.
 * After the rendering pass, the property is returned to its default state of
 * invalidSelectionPoint.
 */

/*!
 * \qmlproperty point Scene3D::graphPositionQuery
 *
 * The coordinates for the user input that should be processed by the scene as a
 * graph position query. If this property is set to value other than
 * invalidSelectionPoint, the graph tries to match a graph position to the given point
 * within the primary viewport.
 * After the rendering pass, this property is returned to its default state of
 * invalidSelectionPoint. The queried graph position can be read from the
 * AbstractGraph3D::queriedGraphPosition property after the next render pass.
 *
 * There is no single correct 3D coordinate to match a particular screen position, so to be
 * consistent, the queries are always done against the inner sides of an invisible box surrounding
 * the graph.
 *
 * \note Bar graphs allow graph position queries only at the graph floor level.
 *
 * \sa AbstractGraph3D::queriedGraphPosition
 */

/*!
 * \qmlproperty bool Scene3D::slicingActive
 *
 * Defines whether the 2D slicing view is currently active. If \c true,
 * AbstractGraph3D::selectionMode must have either the
 * \l{QAbstract3DGraph::SelectionRow}{AbstractGraph3D.SelectionRow} or
 * \l{QAbstract3DGraph::SelectionColumn}{AbstractGraph3D.SelectionColumn}
 * set to a valid selection.
 * \note Not all visualizations support the 2D slicing view.
 */

/*!
 * \qmlproperty bool Scene3D::secondarySubviewOnTop
 *
 * Defines whether the 2D slicing view or the 3D view is drawn on top.
 */

/*!
 * \qmlproperty Camera3D Scene3D::activeCamera
 *
 * The currently active camera in the 3D scene.
 * When a Camera3D is set in the property, it is automatically added as child of
 * the scene.
 */

/*!
 * \qmlproperty Light3D Scene3D::activeLight
 *
 * The currently active light in the 3D scene.
 * When a Light3D is set in the property, it is automatically added as child of
 * the scene.
 */

/*!
 * \qmlproperty float Scene3D::devicePixelRatio
 *
 * The current device pixel ratio that is used when mapping input
 * coordinates to pixel coordinates.
 */

/*!
 * \qmlproperty point Scene3D::invalidSelectionPoint
 * A constant property providing an invalid point for selection.
 */

/*!
 * Constructs a basic scene with one light and one camera in it. An
 * optional \a parent parameter can be given and is then passed to QObject constructor.
 */
Q3DScene::Q3DScene(QObject *parent) :
    QObject(parent),
    d_ptr(new Q3DScenePrivate(this))
{
    setActiveCamera(new Q3DCamera(0));
    setActiveLight(new Q3DLight(0));
}

/*!
 * Destroys the 3D scene and all the objects contained within it.
 */
Q3DScene::~Q3DScene()
{
}

/*!
 * \property Q3DScene::viewport
 *
 * \brief A read only property that contains the current viewport rectangle
 * where all the 3D rendering is targeted.
 */
QRect Q3DScene::viewport() const
{
    return d_ptr->m_viewport;
}

/*!
 * \property Q3DScene::primarySubViewport
 *
 * \brief The current subviewport rectangle inside the viewport where the
 * primary view of the data visualization is targeted.
 *
 * If isSlicingActive() is \c false, the primary sub viewport is equal to
 * viewport(). If isSlicingActive() is \c true and the primary sub viewport has
 * not been explicitly set, it will be one fifth of viewport().
 *
 * \note Setting primarySubViewport larger than or outside of the viewport
 * resizes the viewport accordingly.
 */
QRect Q3DScene::primarySubViewport() const
{
    QRect primary = d_ptr->m_primarySubViewport;
    if (primary.isNull()) {
        if (d_ptr->m_isSlicingActive)
            primary = d_ptr->m_defaultSmallViewport;
        else
            primary = d_ptr->m_defaultLargeViewport;
    }
    return primary;
}

void Q3DScene::setPrimarySubViewport(const QRect &primarySubViewport)
{
    if (d_ptr->m_primarySubViewport != primarySubViewport) {
        if (!primarySubViewport.isValid() && !primarySubViewport.isNull()) {
            qWarning("Viewport is invalid.");
            return;
        }

        // If viewport is smaller than primarySubViewport, enlarge it
        if ((d_ptr->m_viewport.width() < (primarySubViewport.width()
                                          + primarySubViewport.x()))
                || (d_ptr->m_viewport.height() < (primarySubViewport.height()
                                                  + primarySubViewport.y()))) {
            d_ptr->m_viewport.setWidth(qMax(d_ptr->m_viewport.width(),
                                            primarySubViewport.width() + primarySubViewport.x()));
            d_ptr->m_viewport.setHeight(qMax(d_ptr->m_viewport.height(),
                                             primarySubViewport.height() + primarySubViewport.y()));
            d_ptr->calculateSubViewports();
        }

        d_ptr->m_primarySubViewport = primarySubViewport;
        d_ptr->updateGLSubViewports();
        d_ptr->m_changeTracker.primarySubViewportChanged = true;
        d_ptr->m_sceneDirty = true;

        emit primarySubViewportChanged(primarySubViewport);
        emit d_ptr->needRender();
    }
}

/*!
 * Returns whether the given \a point resides inside the primary subview or not.
 * \return \c true if the point is inside the primary subview.
 * \note If subviews are superimposed, and the given \a point resides inside both, result is
 * \c true only when the primary subview is on top.
 */
bool Q3DScene::isPointInPrimarySubView(const QPoint &point)
{
    int x = point.x();
    int y = point.y();
    bool isInSecondary = d_ptr->isInArea(secondarySubViewport(), x, y);
    if (!isInSecondary || (isInSecondary && !d_ptr->m_isSecondarySubviewOnTop))
        return d_ptr->isInArea(primarySubViewport(), x, y);
    else
        return false;
}

/*!
 * Returns whether the given \a point resides inside the secondary subview or not.
 * \return \c true if the point is inside the secondary subview.
 * \note If subviews are superimposed, and the given \a point resides inside both, result is
 * \c true only when the secondary subview is on top.
 */
bool Q3DScene::isPointInSecondarySubView(const QPoint &point)
{
    int x = point.x();
    int y = point.y();
    bool isInPrimary = d_ptr->isInArea(primarySubViewport(), x, y);
    if (!isInPrimary || (isInPrimary && d_ptr->m_isSecondarySubviewOnTop))
        return d_ptr->isInArea(secondarySubViewport(), x, y);
    else
        return false;
}

/*!
 * \property Q3DScene::secondarySubViewport
 *
 * \brief The secondary viewport rectangle inside the viewport.
 *
 * The secondary viewport is used for drawing the 2D slice view in some
 * visualizations. If it has not been explicitly set, it will be equal to
 * QRect. If isSlicingActive() is \c true, it will be equal to \l viewport.
 * \note If the secondary sub viewport is larger than or outside of the
 * viewport, the viewport is resized accordingly.
 */
QRect Q3DScene::secondarySubViewport() const
{
    QRect secondary = d_ptr->m_secondarySubViewport;
    if (secondary.isNull() && d_ptr->m_isSlicingActive)
        secondary = d_ptr->m_defaultLargeViewport;
    return secondary;
}

void Q3DScene::setSecondarySubViewport(const QRect &secondarySubViewport)
{
    if (d_ptr->m_secondarySubViewport != secondarySubViewport) {
        if (!secondarySubViewport.isValid() && !secondarySubViewport.isNull()) {
            qWarning("Viewport is invalid.");
            return;
        }

        // If viewport is smaller than secondarySubViewport, enlarge it
        if ((d_ptr->m_viewport.width() < (secondarySubViewport.width()
                                          + secondarySubViewport.x()))
                || (d_ptr->m_viewport.height() < (secondarySubViewport.height()
                                                  + secondarySubViewport.y()))) {
            d_ptr->m_viewport.setWidth(qMax(d_ptr->m_viewport.width(),
                                            secondarySubViewport.width()
                                            + secondarySubViewport.x()));
            d_ptr->m_viewport.setHeight(qMax(d_ptr->m_viewport.height(),
                                             secondarySubViewport.height()
                                             + secondarySubViewport.y()));
            d_ptr->calculateSubViewports();
        }

        d_ptr->m_secondarySubViewport = secondarySubViewport;
        d_ptr->updateGLSubViewports();
        d_ptr->m_changeTracker.secondarySubViewportChanged = true;
        d_ptr->m_sceneDirty = true;

        emit secondarySubViewportChanged(secondarySubViewport);
        emit d_ptr->needRender();
    }
}

/*!
 * \property Q3DScene::selectionQueryPosition
 *
 * \brief The coordinates for the user input that should be processed
 * by the scene as a selection.
 *
 * If this property is set to a value other than invalidSelectionPoint(), the
 * graph tries to select a data item, axis label, or a custom item at the
 * specified coordinates within the primary viewport.
 * After the rendering pass, the property is returned to its default state of
 * invalidSelectionPoint().
 *
 * \sa QAbstract3DGraph::selectedElement
 */
void Q3DScene::setSelectionQueryPosition(const QPoint &point)
{
    if (point != d_ptr->m_selectionQueryPosition) {
        d_ptr->m_selectionQueryPosition = point;
        d_ptr->m_changeTracker.selectionQueryPositionChanged = true;
        d_ptr->m_sceneDirty = true;

        emit selectionQueryPositionChanged(point);
        emit d_ptr->needRender();
    }
}

QPoint Q3DScene::selectionQueryPosition() const
{
    return d_ptr->m_selectionQueryPosition;
}

/*!
 * \return a QPoint signifying an invalid selection position.
 */
QPoint Q3DScene::invalidSelectionPoint()
{
    static const QPoint invalidSelectionPos(-1, -1);
    return invalidSelectionPos;
}

/*!
 * \property Q3DScene::graphPositionQuery
 *
 * \brief The coordinates for the user input that should be processed
 * by the scene as a graph position query.
 *
 * If this property is set to a value other than invalidSelectionPoint(), the
 * graph tries to match a graph position to the specified coordinates
 * within the primary viewport.
 * After the rendering pass, this property is returned to its default state of
 * invalidSelectionPoint(). The queried graph position can be read from the
 * QAbstract3DGraph::queriedGraphPosition property after the next render pass.
 *
 * There is no single correct 3D coordinate to match a particular screen position, so to be
 * consistent, the queries are always done against the inner sides of an invisible box surrounding
 * the graph.
 *
 * \note Bar graphs allow graph position queries only at the graph floor level.
 *
 * \sa QAbstract3DGraph::queriedGraphPosition
 */
void Q3DScene::setGraphPositionQuery(const QPoint &point)
{
    if (point != d_ptr->m_graphPositionQueryPosition) {
        d_ptr->m_graphPositionQueryPosition = point;
        d_ptr->m_changeTracker.graphPositionQueryPositionChanged = true;
        d_ptr->m_sceneDirty = true;

        emit graphPositionQueryChanged(point);
        emit d_ptr->needRender();
    }
}

QPoint Q3DScene::graphPositionQuery() const
{
    return d_ptr->m_graphPositionQueryPosition;
}

/*!
 * \property Q3DScene::slicingActive
 *
 * \brief Whether the 2D slicing view is currently active.
 *
 * If \c true, QAbstract3DGraph::selectionMode must have either
 * QAbstract3DGraph::SelectionRow or QAbstract3DGraph::SelectionColumn set
 * to a valid selection.
 * \note Not all visualizations support the 2D slicing view.
 */
bool Q3DScene::isSlicingActive() const
{
    return d_ptr->m_isSlicingActive;
}

void Q3DScene::setSlicingActive(bool isSlicing)
{
    if (d_ptr->m_isSlicingActive != isSlicing) {
        d_ptr->m_isSlicingActive = isSlicing;
        d_ptr->m_changeTracker.slicingActivatedChanged = true;
        d_ptr->m_sceneDirty = true;

        // Set secondary subview behind primary to achieve default functionality (= clicking on
        // primary disables slice)
        setSecondarySubviewOnTop(!isSlicing);

        d_ptr->calculateSubViewports();
        emit slicingActiveChanged(isSlicing);
        emit d_ptr->needRender();
    }
}

/*!
 * \property Q3DScene::secondarySubviewOnTop
 *
 * \brief Whether the 2D slicing view or the 3D view is drawn on top.
 */
bool Q3DScene::isSecondarySubviewOnTop() const
{
    return d_ptr->m_isSecondarySubviewOnTop;
}

void Q3DScene::setSecondarySubviewOnTop(bool isSecondaryOnTop)
{
    if (d_ptr->m_isSecondarySubviewOnTop != isSecondaryOnTop) {
        d_ptr->m_isSecondarySubviewOnTop = isSecondaryOnTop;
        d_ptr->m_changeTracker.subViewportOrderChanged = true;
        d_ptr->m_sceneDirty = true;

        emit secondarySubviewOnTopChanged(isSecondaryOnTop);
        emit d_ptr->needRender();
    }
}

/*!
 * \property Q3DScene::activeCamera
 *
 * \brief The currently active camera in the 3D scene.
 *
 * When a new Q3DCamera object is set, it is automatically added as child of
 * the scene.
 */
Q3DCamera *Q3DScene::activeCamera() const
{
    return d_ptr->m_camera;
}

void Q3DScene::setActiveCamera(Q3DCamera *camera)
{
    Q_ASSERT(camera);

    // Add new camera as child of the scene
    if (camera->parent() != this)
        camera->setParent(this);

    if (camera != d_ptr->m_camera) {
        if (d_ptr->m_camera) {
            disconnect(d_ptr->m_camera, &Q3DCamera::xRotationChanged, d_ptr.data(),
                       &Q3DScenePrivate::needRender);
            disconnect(d_ptr->m_camera, &Q3DCamera::yRotationChanged, d_ptr.data(),
                       &Q3DScenePrivate::needRender);
            disconnect(d_ptr->m_camera, &Q3DCamera::zoomLevelChanged, d_ptr.data(),
                       &Q3DScenePrivate::needRender);
        }

        d_ptr->m_camera = camera;
        d_ptr->m_changeTracker.cameraChanged = true;
        d_ptr->m_sceneDirty = true;


        if (camera) {
            connect(camera, &Q3DCamera::xRotationChanged, d_ptr.data(),
                    &Q3DScenePrivate::needRender);
            connect(camera, &Q3DCamera::yRotationChanged, d_ptr.data(),
                    &Q3DScenePrivate::needRender);
            connect(camera, &Q3DCamera::zoomLevelChanged, d_ptr.data(),
                    &Q3DScenePrivate::needRender);
        }

        emit activeCameraChanged(camera);
        emit d_ptr->needRender();
    }
}

/*!
 * \property Q3DScene::activeLight
 *
 * \brief The currently active light in the 3D scene.
 *
 * When a new Q3DLight objects is set, it is automatically added as child of
 * the scene.
 */
Q3DLight *Q3DScene::activeLight() const
{
    return d_ptr->m_light;
}

void Q3DScene::setActiveLight(Q3DLight *light)
{
    Q_ASSERT(light);

    // Add new light as child of the scene
    if (light->parent() != this)
        light->setParent(this);

    if (light != d_ptr->m_light) {
        d_ptr->m_light = light;
        d_ptr->m_changeTracker.lightChanged = true;
        d_ptr->m_sceneDirty = true;

        emit activeLightChanged(light);
        emit d_ptr->needRender();
    }
}

/*!
 * \property Q3DScene::devicePixelRatio
 *
 * \brief The device pixel ratio that is used when mapping input
 * coordinates to pixel coordinates.
 */
float Q3DScene::devicePixelRatio() const
{
    return d_ptr->m_devicePixelRatio;
}

void Q3DScene::setDevicePixelRatio(float pixelRatio)
{
    if (d_ptr->m_devicePixelRatio != pixelRatio) {
        d_ptr->m_devicePixelRatio = pixelRatio;
        d_ptr->m_changeTracker.devicePixelRatioChanged = true;
        d_ptr->m_sceneDirty = true;

        emit devicePixelRatioChanged(pixelRatio);
        d_ptr->updateGLViewport();
        emit d_ptr->needRender();
    }
}

Q3DScenePrivate::Q3DScenePrivate(Q3DScene *q) :
    QObject(0),
    q_ptr(q),
    m_isSecondarySubviewOnTop(true),
    m_devicePixelRatio(1.f),
    m_camera(),
    m_light(),
    m_isUnderSideCameraEnabled(false),
    m_isSlicingActive(false),
    m_selectionQueryPosition(Q3DScene::invalidSelectionPoint()),
    m_graphPositionQueryPosition(Q3DScene::invalidSelectionPoint()),
    m_windowSize(QSize(0, 0)),
    m_sceneDirty(true)
{
}

Q3DScenePrivate::~Q3DScenePrivate()
{
    delete m_camera;
    delete m_light;
}

// Copies changed values from this scene to the other scene. If the other scene had same changes,
// those changes are discarded.
void Q3DScenePrivate::sync(Q3DScenePrivate &other)
{
    if (m_changeTracker.windowSizeChanged) {
        other.setWindowSize(windowSize());
        m_changeTracker.windowSizeChanged = false;
        other.m_changeTracker.windowSizeChanged = false;
    }
    if (m_changeTracker.viewportChanged) {
        other.setViewport(m_viewport);
        m_changeTracker.viewportChanged = false;
        other.m_changeTracker.viewportChanged = false;
    }
    if (m_changeTracker.subViewportOrderChanged) {
        other.q_ptr->setSecondarySubviewOnTop(q_ptr->isSecondarySubviewOnTop());
        m_changeTracker.subViewportOrderChanged = false;
        other.m_changeTracker.subViewportOrderChanged = false;
    }
    if (m_changeTracker.primarySubViewportChanged) {
        other.q_ptr->setPrimarySubViewport(q_ptr->primarySubViewport());
        m_changeTracker.primarySubViewportChanged = false;
        other.m_changeTracker.primarySubViewportChanged = false;
    }
    if (m_changeTracker.secondarySubViewportChanged) {
        other.q_ptr->setSecondarySubViewport(q_ptr->secondarySubViewport());
        m_changeTracker.secondarySubViewportChanged = false;
        other.m_changeTracker.secondarySubViewportChanged = false;
    }
    if (m_changeTracker.selectionQueryPositionChanged) {
        other.q_ptr->setSelectionQueryPosition(q_ptr->selectionQueryPosition());
        m_changeTracker.selectionQueryPositionChanged = false;
        other.m_changeTracker.selectionQueryPositionChanged = false;
    }
    if (m_changeTracker.graphPositionQueryPositionChanged) {
        other.q_ptr->setGraphPositionQuery(q_ptr->graphPositionQuery());
        m_changeTracker.graphPositionQueryPositionChanged = false;
        other.m_changeTracker.graphPositionQueryPositionChanged = false;
    }
    if (m_changeTracker.cameraChanged) {
        m_camera->setDirty(true);
        m_changeTracker.cameraChanged = false;
        other.m_changeTracker.cameraChanged = false;
    }
    m_camera->d_ptr->sync(*other.m_camera);

    if (m_changeTracker.lightChanged) {
        m_light->setDirty(true);
        m_changeTracker.lightChanged = false;
        other.m_changeTracker.lightChanged = false;
    }
    m_light->d_ptr->sync(*other.m_light);

    if (m_changeTracker.slicingActivatedChanged) {
        other.q_ptr->setSlicingActive(q_ptr->isSlicingActive());
        m_changeTracker.slicingActivatedChanged = false;
        other.m_changeTracker.slicingActivatedChanged = false;
    }

    if (m_changeTracker.devicePixelRatioChanged) {
        other.q_ptr->setDevicePixelRatio(q_ptr->devicePixelRatio());
        m_changeTracker.devicePixelRatioChanged = false;
        other.m_changeTracker.devicePixelRatioChanged = false;
    }

    m_sceneDirty = false;
    other.m_sceneDirty = false;
}

void Q3DScenePrivate::setViewport(const QRect &viewport)
{
    if (m_viewport != viewport && viewport.isValid()) {
        m_viewport = viewport;
        calculateSubViewports();
        emit needRender();
    }
}

void Q3DScenePrivate::setViewportSize(int width, int height)
{
    if (m_viewport.width() != width || m_viewport.height() != height) {
        m_viewport.setWidth(width);
        m_viewport.setHeight(height);
        calculateSubViewports();
        emit needRender();
    }
}

/*!
 * \internal
 * Sets the size of the window being rendered to. With widget based graphs, this
 * is equal to the size of the QWindow and is same as the bounding rectangle.
 * With declarative graphs this is equal to the size of the QQuickWindow and
 * can be different from the bounding rectangle.
 */
void Q3DScenePrivate::setWindowSize(const QSize &size)
{
    if (m_windowSize != size) {
        m_windowSize = size;
        updateGLViewport();
        m_changeTracker.windowSizeChanged = true;
        emit needRender();
    }
}

QSize Q3DScenePrivate::windowSize() const
{
    return m_windowSize;
}

void Q3DScenePrivate::calculateSubViewports()
{
    // Calculates the default subviewport layout, used when slicing
    const float smallerViewPortRatio = 0.2f;
    m_defaultSmallViewport = QRect(0, 0,
                                   m_viewport.width() * smallerViewPortRatio,
                                   m_viewport.height() * smallerViewPortRatio);
    m_defaultLargeViewport = QRect(0, 0,
                                   m_viewport.width(),
                                   m_viewport.height());

    updateGLViewport();
}

void Q3DScenePrivate::updateGLViewport()
{
    // Update GL viewport
    m_glViewport.setX(m_viewport.x() * m_devicePixelRatio);
    m_glViewport.setY((m_windowSize.height() - (m_viewport.y() + m_viewport.height()))
                      * m_devicePixelRatio);
    m_glViewport.setWidth(m_viewport.width() * m_devicePixelRatio);
    m_glViewport.setHeight(m_viewport.height() * m_devicePixelRatio);

    m_changeTracker.viewportChanged = true;
    m_sceneDirty = true;

    // Do default subviewport changes first, then allow signal listeners to override.
    updateGLSubViewports();
    emit q_ptr->viewportChanged(m_viewport);
}

void Q3DScenePrivate::updateGLSubViewports()
{
    if (m_isSlicingActive) {
        QRect primary = m_primarySubViewport;
        QRect secondary = m_secondarySubViewport;
        if (primary.isNull())
            primary = m_defaultSmallViewport;
        if (secondary.isNull())
            secondary = m_defaultLargeViewport;

        m_glPrimarySubViewport.setX((primary.x() + m_viewport.x()) * m_devicePixelRatio);
        m_glPrimarySubViewport.setY((m_windowSize.height()
                                     - (primary.y() + primary.height() + m_viewport.y()))
                                    * m_devicePixelRatio);
        m_glPrimarySubViewport.setWidth(primary.width() * m_devicePixelRatio);
        m_glPrimarySubViewport.setHeight(primary.height() * m_devicePixelRatio);

        m_glSecondarySubViewport.setX((secondary.x() + m_viewport.x()) * m_devicePixelRatio);
        m_glSecondarySubViewport.setY((m_windowSize.height()
                                       - (secondary.y() + secondary.height() + m_viewport.y()))
                                      * m_devicePixelRatio);
        m_glSecondarySubViewport.setWidth(secondary.width() * m_devicePixelRatio);
        m_glSecondarySubViewport.setHeight(secondary.height() * m_devicePixelRatio);
    } else {
        m_glPrimarySubViewport.setX(m_viewport.x() * m_devicePixelRatio);
        m_glPrimarySubViewport.setY((m_windowSize.height() - (m_viewport.y() + m_viewport.height()))
                                    * m_devicePixelRatio);
        m_glPrimarySubViewport.setWidth(m_viewport.width() * m_devicePixelRatio);
        m_glPrimarySubViewport.setHeight(m_viewport.height() * m_devicePixelRatio);

        m_glSecondarySubViewport = QRect();
    }
}

QRect Q3DScenePrivate::glViewport()
{
    return m_glViewport;
}

QRect Q3DScenePrivate::glPrimarySubViewport()
{
    return m_glPrimarySubViewport;
}

QRect Q3DScenePrivate::glSecondarySubViewport()
{
    return m_glSecondarySubViewport;
}

/*!
 * \internal
 * Calculates and sets the light position relative to the currently active camera using the given
 * parameters.
 * The relative 3D offset to the current camera position is defined in \a relativePosition.
 * Optional \a fixedRotation fixes the light rotation around the data visualization area to the
 * given value in degrees.
 * Optional \a distanceModifier modifies the distance of the light from the data visualization.
 */
void Q3DScenePrivate::setLightPositionRelativeToCamera(const QVector3D &relativePosition,
                                                       float fixedRotation, float distanceModifier)
{
    m_light->setPosition(m_camera->d_ptr->calculatePositionRelativeToCamera(relativePosition,
                                                                            fixedRotation,
                                                                            distanceModifier));
}

void Q3DScenePrivate::markDirty()
{
    m_sceneDirty = true;
    emit needRender();
}

bool Q3DScenePrivate::isInArea(const QRect &area, int x, int y) const
{
    int areaMinX = area.x();
    int areaMaxX = area.x() + area.width();
    int areaMinY = area.y();
    int areaMaxY = area.y() + area.height();
    return ( x >= areaMinX && x <= areaMaxX && y >= areaMinY && y <= areaMaxY );
}

QT_END_NAMESPACE_DATAVISUALIZATION
