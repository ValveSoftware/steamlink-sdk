/****************************************************************************
**
** Copyright (C) 2014-2016 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
** Copyright (C) 2013 Klarälvdalens Datakonsult AB (KDAB).
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWaylandCompositor module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwaylandoutput.h"
#include "qwaylandoutput_p.h"

#include <QtWaylandCompositor/QWaylandCompositor>
#include <QtWaylandCompositor/QWaylandView>

#include <QtWaylandCompositor/private/qwaylandsurface_p.h>
#include <QtWaylandCompositor/private/qwaylandcompositor_p.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QtMath>
#include <QtGui/QWindow>
#include <QtGui/QExposeEvent>
#include <QtGui/QScreen>
#include <private/qobject_p.h>

QT_BEGIN_NAMESPACE

static QtWaylandServer::wl_output::subpixel toWlSubpixel(const QWaylandOutput::Subpixel &value)
{
    switch (value) {
    case QWaylandOutput::SubpixelUnknown:
        return QtWaylandServer::wl_output::subpixel_unknown;
    case QWaylandOutput::SubpixelNone:
        return QtWaylandServer::wl_output::subpixel_none;
    case QWaylandOutput::SubpixelHorizontalRgb:
        return QtWaylandServer::wl_output::subpixel_horizontal_rgb;
    case QWaylandOutput::SubpixelHorizontalBgr:
        return QtWaylandServer::wl_output::subpixel_horizontal_bgr;
    case QWaylandOutput::SubpixelVerticalRgb:
        return QtWaylandServer::wl_output::subpixel_vertical_rgb;
    case QWaylandOutput::SubpixelVerticalBgr:
        return QtWaylandServer::wl_output::subpixel_vertical_bgr;
    default:
        break;
    }

    return QtWaylandServer::wl_output::subpixel_unknown;
}

static QtWaylandServer::wl_output::transform toWlTransform(const QWaylandOutput::Transform &value)
{
    switch (value) {
    case QWaylandOutput::Transform90:
        return QtWaylandServer::wl_output::transform_90;
    case QWaylandOutput::Transform180:
        return QtWaylandServer::wl_output::transform_180;
    case QWaylandOutput::Transform270:
        return QtWaylandServer::wl_output::transform_270;
    case QWaylandOutput::TransformFlipped:
        return QtWaylandServer::wl_output::transform_flipped;
    case QWaylandOutput::TransformFlipped90:
        return QtWaylandServer::wl_output::transform_flipped_90;
    case QWaylandOutput::TransformFlipped180:
        return QtWaylandServer::wl_output::transform_flipped_180;
    case QWaylandOutput::TransformFlipped270:
        return QtWaylandServer::wl_output::transform_flipped_270;
    default:
        break;
    }

    return QtWaylandServer::wl_output::transform_normal;
}

QWaylandOutputPrivate::QWaylandOutputPrivate()
    : QtWaylandServer::wl_output()
    , compositor(Q_NULLPTR)
    , window(Q_NULLPTR)
    , currentMode(-1)
    , preferredMode(-1)
    , subpixel(QWaylandOutput::SubpixelUnknown)
    , transform(QWaylandOutput::TransformNormal)
    , scaleFactor(1)
    , sizeFollowsWindow(false)
    , initialized(false)
{
}

QWaylandOutputPrivate::~QWaylandOutputPrivate()
{
}

void QWaylandOutputPrivate::output_bind_resource(Resource *resource)
{
    sendGeometry(resource);

    for (const QWaylandOutputMode &mode : modes)
        sendMode(resource, mode);

    if (resource->version() >= 2) {
        send_scale(resource->handle, scaleFactor);
        send_done(resource->handle);
    }
}

void QWaylandOutputPrivate::sendGeometry(const Resource *resource)
{
    send_geometry(resource->handle,
                  position.x(), position.y(),
                  physicalSize.width(), physicalSize.height(),
                  toWlSubpixel(subpixel), manufacturer, model,
                  toWlTransform(transform));
}

void QWaylandOutputPrivate::sendGeometryInfo()
{
    for (const Resource *resource : resourceMap().values()) {
        sendGeometry(resource);
        if (resource->version() >= 2)
            send_done(resource->handle);
    }
}

void QWaylandOutputPrivate::sendMode(const Resource *resource, const QWaylandOutputMode &mode)
{
    quint32 flags = 0;
    if (currentMode == modes.indexOf(mode))
        flags |= QtWaylandServer::wl_output::mode_current;
    if (preferredMode == modes.indexOf(mode))
        flags |= QtWaylandServer::wl_output::mode_preferred;

    send_mode(resource->handle, flags,
              mode.size().width(), mode.size().height(),
              mode.refreshRate());
}

void QWaylandOutputPrivate::sendModesInfo()
{
    for (const Resource *resource : resourceMap().values()) {
        for (const QWaylandOutputMode &mode : modes)
            sendMode(resource, mode);
        if (resource->version() >= 2)
            send_done(resource->handle);
    }
}

void QWaylandOutputPrivate::addView(QWaylandView *view, QWaylandSurface *surface)
{
    for (int i = 0; i < surfaceViews.size(); i++) {
        if (surface == surfaceViews.at(i).surface) {
            if (!surfaceViews.at(i).views.contains(view)) {
                surfaceViews[i].views.append(view);
            }
            return;
        }
    }

    surfaceViews.append(QWaylandSurfaceViewMapper(surface,view));
}

void QWaylandOutputPrivate::removeView(QWaylandView *view, QWaylandSurface *surface)
{
    Q_Q(QWaylandOutput);
    for (int i = 0; i < surfaceViews.size(); i++) {
        if (surface == surfaceViews.at(i).surface) {
            bool removed = surfaceViews[i].views.removeOne(view);
            if (surfaceViews.at(i).views.isEmpty() && removed) {
                if (surfaceViews.at(i).has_entered)
                    q->surfaceLeave(surface);
                surfaceViews.remove(i);
            }
            return;
        }
    }
    qWarning("%s Could not find view %p for surface %p to remove. Possible invalid state", Q_FUNC_INFO, view, surface);
}

QWaylandOutput::QWaylandOutput()
    : QWaylandObject(*new QWaylandOutputPrivate())
{
}

/*!
   \qmltype WaylandOutput
   \inqmlmodule QtWayland.Compositor
   \since 5.8
   \brief Provides access to a displayable area managed by the compositor.

   The WaylandOutput manages a rectangular area within bounds of the compositor's
   geometry, to use it for displaying client content. This could, for instance, be
   a screen managed by the WaylandCompositor.

   The type corresponds to the \c wl_output interface in the Wayland protocol.
*/

/*!
   \class QWaylandOutput
   \inmodule QtWaylandCompositor
   \since 5.8
   \brief The QWaylandOutput class represents a displayable area managed by the compositor.

   The QWaylandOutput manages a rectangular area within bounds of the compositor's
   geometry, to use it for displaying client content. This could, for instance, be
   a screen managed by the WaylandCompositor.

   The class corresponds to the \c wl_output interface in the Wayland protocol.
*/

/*!
 * Constructs a QWaylandOutput in \a compositor and with the specified \a window. The
 * \l{QWaylandCompositor::create()}{create()} function must be called on the
 * \a compositor before constructing a QWaylandOutput for it.
 *
 * The QWaylandOutput object is initialized later, in reaction to an event.
 * At this point it is added as an output for the \a compositor. If it is the
 * first QWaylandOutput object created for this \a compositor, it becomes the
 * \l{QWaylandCompositor::defaultOutput()}{default output}.
 */
QWaylandOutput::QWaylandOutput(QWaylandCompositor *compositor, QWindow *window)
    : QWaylandObject(*new QWaylandOutputPrivate())
{
    Q_D(QWaylandOutput);
    d->compositor = compositor;
    d->window = window;
    QWaylandCompositorPrivate::get(compositor)->addPolishObject(this);
}

/*!
 * Destroys the QWaylandOutput.
 */
QWaylandOutput::~QWaylandOutput()
{
    Q_D(QWaylandOutput);
    if (d->compositor)
        QWaylandCompositorPrivate::get(d->compositor)->removeOutput(this);
}

/*!
 * \internal
 */
void QWaylandOutput::initialize()
{
    Q_D(QWaylandOutput);

    Q_ASSERT(!d->initialized);
    Q_ASSERT(d->compositor);
    Q_ASSERT(d->compositor->isCreated());

    // Replace modes with one that follows the window size and refresh rate,
    // but only if window size is valid
    if (d->window && d->sizeFollowsWindow) {
        QWaylandOutputMode mode(d->window->size(),
                                qFloor(d->window->screen()->refreshRate() * 1000));
        if (mode.isValid()) {
            d->modes.clear();
            addMode(mode, true);
            setCurrentMode(mode);
        }
    }

    QWaylandCompositorPrivate::get(d->compositor)->addOutput(this);

    if (d->window) {
        QObject::connect(d->window, &QWindow::widthChanged, this, &QWaylandOutput::handleSetWidth);
        QObject::connect(d->window, &QWindow::heightChanged, this, &QWaylandOutput::handleSetHeight);
        QObject::connect(d->window, &QObject::destroyed, this, &QWaylandOutput::handleWindowDestroyed);
    }

    d->init(d->compositor->display(), 2);

    d->initialized = true;
}

/*!
 * Returns the QWaylandOutput corresponding to \a resource.
 */
QWaylandOutput *QWaylandOutput::fromResource(wl_resource *resource)
{
    return static_cast<QWaylandOutputPrivate *>(QWaylandOutputPrivate::Resource::fromResource(resource)->output_object)->q_func();
}

/*!
 * \internal
 */
struct ::wl_resource *QWaylandOutput::resourceForClient(QWaylandClient *client) const
{
    Q_D(const QWaylandOutput);
    QWaylandOutputPrivate::Resource *r = d->resourceMap().value(client->client());
    if (r)
        return r->handle;

    return Q_NULLPTR;
}

/*!
 * Schedules a QEvent::UpdateRequest to be delivered to the QWaylandOutput's \l{window()}{window}.
 *
 * \sa QWindow::requestUpdate()
 */
void QWaylandOutput::update()
{
    Q_D(QWaylandOutput);
    if (!d->window)
        return;
    d->window->requestUpdate();
}

/*!
 * \qmlproperty object QtWaylandCompositor::WaylandOutput::compositor
 *
 * This property holds the compositor displaying content on this WaylandOutput.
 *
 * \note This property can be set only once, before the WaylandOutput component
 * is completed.
 */

/*!
 * Returns the compositor for this QWaylandOutput.
 */
QWaylandCompositor *QWaylandOutput::compositor() const
{
    return d_func()->compositor;
}

/*!
 * \internal
 */
void QWaylandOutput::setCompositor(QWaylandCompositor *compositor)
{
    Q_D(QWaylandOutput);

    if (d->compositor == compositor)
        return;

    if (d->initialized) {
        qWarning("Setting QWaylandCompositor %p on QWaylandOutput %p is not supported after QWaylandOutput has been initialized\n", compositor, this);
        return;
    }
    if (d->compositor && d->compositor != compositor) {
        qWarning("Possible initialization error. Moving QWaylandOutput %p between compositor instances.\n", this);
    }

    d->compositor = compositor;

    QWaylandCompositorPrivate::get(compositor)->addPolishObject(this);
}

/*!
 * \qmlproperty string QtWaylandCompositor::WaylandOutput::manufacturer
 *
 * This property holds a textual description of the manufacturer of this WaylandOutput.
 */

/*!
 * \property QWaylandOutput::manufacturer
 *
 * This property holds a textual description of the manufacturer of this QWaylandOutput.
 */
QString QWaylandOutput::manufacturer() const
{
    return d_func()->manufacturer;
}

void QWaylandOutput::setManufacturer(const QString &manufacturer)
{
    Q_D(QWaylandOutput);

    if (d->manufacturer == manufacturer)
        return;

    d->manufacturer = manufacturer;
    d->sendGeometryInfo();
    Q_EMIT manufacturerChanged();
}

/*!
 * \qmlproperty string QtWaylandCompositor::WaylandOutput::model
 *
 * This property holds a textual description of the model of this WaylandOutput.
 */

/*!
 * \property QWaylandOutput::model
 *
 * This property holds a textual description of the model of this QWaylandOutput.
 */
QString QWaylandOutput::model() const
{
    return d_func()->model;
}

void QWaylandOutput::setModel(const QString &model)
{
    Q_D(QWaylandOutput);

    if (d->model == model)
        return;

    d->model = model;
    d->sendGeometryInfo();
    Q_EMIT modelChanged();
}

/*!
 * \qmlproperty point QtWaylandCompositor::WaylandOutput::position
 *
 * This property holds the position of this WaylandOutput in the compositor's coordinate system.
 */

/*!
 * \property QWaylandOutput::position
 *
 * This property holds the position of this QWaylandOutput in the compositor's coordinate system.
 */
QPoint QWaylandOutput::position() const
{
    return d_func()->position;
}

void QWaylandOutput::setPosition(const QPoint &pt)
{
    Q_D(QWaylandOutput);
    if (d->position == pt)
        return;

    d->position = pt;

    d->sendGeometryInfo();

    Q_EMIT positionChanged();
    Q_EMIT geometryChanged();
}

/*!
 * Returns the list of modes.
 */
QList<QWaylandOutputMode> QWaylandOutput::modes() const
{
    Q_D(const QWaylandOutput);
    return d->modes.toList();
}

/*!
 * Adds the mode \a mode to the output and mark it as preferred
 * if \a preferred is \c true.
 * Please note there can only be one preferred mode.
 */
void QWaylandOutput::addMode(const QWaylandOutputMode &mode, bool preferred)
{
    Q_D(QWaylandOutput);

    if (!mode.isValid()) {
        qWarning("Cannot add an invalid mode");
        return;
    }

    d->modes.append(mode);

    if (preferred)
        d->preferredMode = d->modes.indexOf(mode);

    emit modeAdded();
}

/*!
 * Returns the output's size in pixels and refresh rate in mHz.
 * If the current mode is not set it will return an invalid mode.
 *
 * \sa QWaylandOutput::modes
 * \sa QWaylandOutputMode
 */
QWaylandOutputMode QWaylandOutput::currentMode() const
{
    Q_D(const QWaylandOutput);

    if (d->currentMode >= 0 && d->currentMode <= d->modes.size() - 1)
        return d->modes.at(d->currentMode);
    return QWaylandOutputMode();
}

/*!
 * Sets the current mode.
 * The mode \a mode must have been previously added.
 *
 * \sa QWaylandOutput::modes
 * \sa QWaylandOutputMode
 */
void QWaylandOutput::setCurrentMode(const QWaylandOutputMode &mode)
{
    Q_D(QWaylandOutput);

    int index = d->modes.indexOf(mode);
    if (index < 0) {
        qWarning("Cannot set an unknown QWaylandOutput mode as current");
        return;
    }

    d->currentMode = index;

    Q_EMIT currentModeChanged();
    Q_EMIT geometryChanged();

    d->sendModesInfo();
}

/*!
 * \qmlproperty rect QtWaylandCompositor::WaylandOutput::geometry
 *
 * This property holds the geometry of the WaylandOutput.
 */

/*!
 * \property QWaylandOutput::geometry
 *
 * This property holds the geometry of the QWaylandOutput.
 *
 * \sa QWaylandOutput::currentMode
 */
QRect QWaylandOutput::geometry() const
{
    Q_D(const QWaylandOutput);
    return QRect(d->position, currentMode().size());
}

/*!
 * \qmlproperty rect QtWaylandCompositor::WaylandOutput::availableGeometry
 *
 * This property holds the geometry of the WaylandOutput available for displaying content.
 * The available geometry is in output coordinates space, starts from 0,0 and it's as big
 * as the output by default.
 *
 * \sa QWaylandOutput::geometry
 */

/*!
 * \property QWaylandOutput::availableGeometry
 *
 * This property holds the geometry of the QWaylandOutput available for displaying content.
 * The available geometry is in output coordinates space, starts from 0,0 and it's as big
 * as the output by default.
 *
 * \sa QWaylandOutput::currentMode, QWaylandOutput::geometry
 */
QRect QWaylandOutput::availableGeometry() const
{
    Q_D(const QWaylandOutput);

    if (!d->availableGeometry.isValid())
        return QRect(QPoint(0, 0), currentMode().size());

    return d->availableGeometry;
}

void QWaylandOutput::setAvailableGeometry(const QRect &availableGeometry)
{
    Q_D(QWaylandOutput);
    if (d->availableGeometry == availableGeometry)
        return;

    if (availableGeometry.topLeft().x() < 0 || availableGeometry.topLeft().y() < 0)
        qWarning("Available geometry should be a portion of the output");

    d->availableGeometry = availableGeometry;

    Q_EMIT availableGeometryChanged();
}

/*!
 * \qmlproperty size QtWaylandCompositor::WaylandOutput::physicalSize
 *
 * This property holds the physical size of the WaylandOutput in millimeters.
 *
 * \sa QWaylandOutput::geometry
 */

/*!
 * \property QWaylandOutput::physicalSize
 *
 * This property holds the physical size of the QWaylandOutput in millimeters.
 *
 * \sa QWaylandOutput::geometry, QWaylandOutput::currentMode
 */
QSize QWaylandOutput::physicalSize() const
{
    return d_func()->physicalSize;
}

void QWaylandOutput::setPhysicalSize(const QSize &size)
{
    Q_D(QWaylandOutput);
    if (d->physicalSize == size)
        return;

    d->physicalSize = size;

    d->sendGeometryInfo();

    Q_EMIT physicalSizeChanged();
}

/*!
 * \enum QWaylandOutput::Subpixel
 *
 * This enum type is used to specify the subpixel arrangement of a QWaylandOutput.
 *
 * \value SubpixelUnknown The subpixel arrangement is not set.
 * \value SubpixelNone There are no subpixels.
 * \value SubpixelHorizontalRgb The subpixels are arranged horizontally in red, green, blue order.
 * \value SubpixelHorizontalBgr The subpixels are arranged horizontally in blue, green, red order.
 * \value SubpixelVerticalRgb The subpixels are arranged vertically in red, green, blue order.
 * \value SubpixelVerticalBgr The subpixels are arranged vertically in blue, green, red order.
 *
 * \sa QWaylandOutput::subpixel
 */

/*!
 * \qmlproperty enum QtWaylandCompositor::WaylandOutput::subpixel
 *
 * This property holds the subpixel arrangement of this WaylandOutput.
 *
 * \list
 * \li WaylandOutput.SubpixelUnknown The subpixel arrangement is not set.
 * \li WaylandOutput.SubpixelNone There are no subpixels.
 * \li WaylandOutput.SubpixelHorizontalRgb The subpixels are arranged horizontally in red, green, blue order.
 * \li WaylandOutput.SubpixelHorizontalBgr The subpixels are arranged horizontally in blue, green, red order.
 * \li WaylandOutput.SubpixelVerticalRgb The subpixels are arranged vertically in red, green, blue order.
 * \li WaylandOutput.SubpixelVerticalBgr The subpixels are arranged vertically in blue, green, red order.
 * \endlist
 *
 * The default is WaylandOutput.SubpixelUnknown.
 */

/*!
 * \property QWaylandOutput::subpixel
 *
 * This property holds the subpixel arrangement of this QWaylandOutput. The default is
 * QWaylandOutput::SubpixelUnknown.
 */
QWaylandOutput::Subpixel QWaylandOutput::subpixel() const
{
    return d_func()->subpixel;
}

void QWaylandOutput::setSubpixel(const Subpixel &subpixel)
{
    Q_D(QWaylandOutput);
    if (d->subpixel == subpixel)
        return;

    d->subpixel = subpixel;

    d->sendGeometryInfo();

    Q_EMIT subpixelChanged();
}

/*! \enum QWaylandOutput::Transform
 *
 * This enum type is used to specify the orientation of a QWaylandOutput.
 *
 * \value TransformNormal The orientation is normal.
 * \value Transform90 The orientation is rotated 90 degrees.
 * \value Transform180 The orientation is rotated 180 degrees.
 * \value Transform270 The orientation is rotated 270 degrees.
 * \value TransformFlipped The orientation is mirrored.
 * \value TransformFlipped90 The orientation is mirrored, and rotated 90 degrees.
 * \value TransformFlipped180 The orientation is mirrored, and rotated 180 degrees.
 * \value TransformFlipped270 The orientation is mirrored, and rotated 270 degrees.
 *
 * \sa QWaylandOutput::transform
*/

/*!
 * \qmlproperty enum QtWaylandCompositor::WaylandOutput::transform
 *
 * This property holds the transformation that the QWaylandCompositor applies to a surface
 * to compensate for the orientation of the QWaylandOutput.
 *
 * \list
 * \li WaylandOutput.TransformNormal The orientation is normal.
 * \li WaylandOutput.Transform90 The orientation is rotated 90 degrees.
 * \li WaylandOutput.Transform180 The orientation is rotated 180 degrees.
 * \li WaylandOutput.Transform270 The orientation is rotated 270 degrees.
 * \li WaylandOutput.TransformFlipped The orientation is mirrored.
 * \li WaylandOutput.TransformFlipped90 The orientation is mirrored, then rotated 90 degrees.
 * \li WaylandOutput.TransformFlipped180 The orientation is mirrored, then rotated 180 degrees.
 * \li WaylandOutput.TransformFlipped270 The orientation is mirrored, then rotated 270 degrees.
 * \endlist
 *
 * The default is WaylandOutput.TransformNormal.
 */

/*!
 * \property QWaylandOutput::transform
 *
 * This property holds the transformation that the QWaylandCompositor applies to a surface
 * to compensate for the orientation of the QWaylandOutput.
 *
 * The default is QWaylandOutput::TransformNormal.
 */
QWaylandOutput::Transform QWaylandOutput::transform() const
{
    return d_func()->transform;
}

void QWaylandOutput::setTransform(const Transform &transform)
{
    Q_D(QWaylandOutput);
    if (d->transform == transform)
        return;

    d->transform = transform;

    d->sendGeometryInfo();

    Q_EMIT transformChanged();
}

/*!
 * \qmlproperty int QtWaylandCompositor::WaylandOutput::scaleFactor
 *
 * This property holds the factor by which the WaylandCompositor scales surface buffers
 * before they are displayed. It is used on high density output devices where unscaled content
 * would be too small to be practical. The client can in turn set the scale factor of its
 * buffer to match the output if it prefers to provide high resolution content that is
 * suitable for the output device.
 *
 * The default is 1 (no scaling).
 */

/*!
 * \property QWaylandOutput::scaleFactor
 *
 * This property holds the factor by which the QWaylandCompositor scales surface buffers
 * before they are displayed. This is used on high density output devices where unscaled content
 * would be too small to be practical. The client can in turn set the scale factor of its
 * buffer to match the output if it prefers to provide high resolution content that is
 * suitable for the output device.
 *
 * The default is 1 (no scaling).
 */
int QWaylandOutput::scaleFactor() const
{
    return d_func()->scaleFactor;
}

void QWaylandOutput::setScaleFactor(int scale)
{
    Q_D(QWaylandOutput);
    if (d->scaleFactor == scale)
        return;

    d->scaleFactor = scale;

    Q_FOREACH (QWaylandOutputPrivate::Resource *resource, d->resourceMap().values()) {
        if (resource->version() >= 2) {
            d->send_scale(resource->handle, scale);
            d->send_done(resource->handle);
        }
    }

    Q_EMIT scaleFactorChanged();
}

/*!
 * \qmlproperty bool QtWaylandCompositor::WaylandOutput::sizeFollowsWindow
 *
 * This property controls whether the size of the WaylandOutput matches the
 * size of its window.
 *
 * If this property is true, all modes previously added are replaced by a
 * mode that matches window size and screen refresh rate.
 *
 * The default is false.
 */

/*!
 * \property QWaylandOutput::sizeFollowsWindow
 *
 * This property controls whether the size of the QWaylandOutput matches the
 * size of its window.
 *
 * If this property is true, all modes previously added are replaced by a
 * mode that matches window size and screen refresh rate.
 *
 * The default is false.
 */
bool QWaylandOutput::sizeFollowsWindow() const
{
    return d_func()->sizeFollowsWindow;
}

void QWaylandOutput::setSizeFollowsWindow(bool follow)
{
    Q_D(QWaylandOutput);

    if (!d->window) {
        qWarning("Setting QWaylandOutput::sizeFollowsWindow without a window has no effect");
        return;
    }

    if (follow != d->sizeFollowsWindow) {
        d->sizeFollowsWindow = follow;
        Q_EMIT sizeFollowsWindowChanged();
    }
}

/*!
 * \qmlproperty object QtWaylandCompositor::WaylandOutput::window
 *
 * This property holds the Window for this WaylandOutput.
 *
 * \note This property can be set only once, before the WaylandOutput
 * component is completed.
 */

/*!
 * \property QWaylandOutput::window
 *
 * This property holds the QWindow for this QWaylandOutput.
 */
QWindow *QWaylandOutput::window() const
{
    return d_func()->window;
}

void QWaylandOutput::setWindow(QWindow *window)
{
    Q_D(QWaylandOutput);
    if (d->window == window)
        return;
    if (d->initialized) {
        qWarning("Setting QWindow %p on QWaylandOutput %p is not supported after QWaylandOutput has been initialized\n", window, this);
        return;
    }
    d->window = window;
    emit windowChanged();
}

/*!
 * Informs QWaylandOutput that a frame has started.
 */
void QWaylandOutput::frameStarted()
{
    Q_D(QWaylandOutput);
    for (int i = 0; i < d->surfaceViews.size(); i++) {
        QWaylandSurfaceViewMapper &surfacemapper = d->surfaceViews[i];
        if (surfacemapper.maybePrimaryView())
            surfacemapper.surface->frameStarted();
    }
}

/*!
 * Sends pending frame callbacks.
 */
void QWaylandOutput::sendFrameCallbacks()
{
    Q_D(QWaylandOutput);
    for (int i = 0; i < d->surfaceViews.size(); i++) {
        const QWaylandSurfaceViewMapper &surfacemapper = d->surfaceViews.at(i);
        if (surfacemapper.surface && surfacemapper.surface->hasContent()) {
            if (!surfacemapper.has_entered) {
                surfaceEnter(surfacemapper.surface);
                d->surfaceViews[i].has_entered = true;
            }
            if (surfacemapper.maybePrimaryView())
                surfacemapper.surface->sendFrameCallbacks();
        }
    }
    wl_display_flush_clients(d->compositor->display());
}

/*!
 * \internal
 */
void QWaylandOutput::surfaceEnter(QWaylandSurface *surface)
{
    if (!surface)
        return;

    auto clientResource = resourceForClient(surface->client());
    if (clientResource)
        QWaylandSurfacePrivate::get(surface)->send_enter(clientResource);
}

/*!
 * \internal
 */
void QWaylandOutput::surfaceLeave(QWaylandSurface *surface)
{
    if (!surface || !surface->client())
        return;
    QWaylandSurfacePrivate::get(surface)->send_leave(resourceForClient(surface->client()));
}

/*!
 * \internal
 */
void QWaylandOutput::handleSetWidth(int newWidth)
{
    Q_D(QWaylandOutput);

    if (!d->window || !d->sizeFollowsWindow)
        return;

    if (d->currentMode <= d->modes.size() - 1) {
        if (d->currentMode >= 0) {
            QWaylandOutputMode mode = d->modes.at(d->currentMode);
            mode.setWidth(newWidth);
            d->modes.replace(d->currentMode, mode);
            d->sendModesInfo();
        } else {
            // We didn't add a mode during the initialization because the window
            // size was invalid, let's add it now
            QWaylandOutputMode mode(d->window->size(),
                                    qFloor(d->window->screen()->refreshRate() * 1000));
            if (mode.isValid()) {
                d->modes.clear();
                addMode(mode, true);
                setCurrentMode(mode);
            }
        }
    }
}

/*!
 * \internal
 */
void QWaylandOutput::handleSetHeight(int newHeight)
{
    Q_D(QWaylandOutput);

    if (!d->window || !d->sizeFollowsWindow)
        return;

    if (d->currentMode <= d->modes.size() - 1) {
        if (d->currentMode >= 0) {
            QWaylandOutputMode mode = d->modes.at(d->currentMode);
            mode.setHeight(newHeight);
            d->modes.replace(d->currentMode, mode);
            d->sendModesInfo();
        } else {
            // We didn't add a mode during the initialization because the window
            // size was invalid, let's add it now
            QWaylandOutputMode mode(d->window->size(),
                                    qFloor(d->window->screen()->refreshRate() * 1000));
            if (mode.isValid()) {
                d->modes.clear();
                addMode(mode, true);
                setCurrentMode(mode);
            }
        }
    }
}

/*!
 * \internal
 */
void QWaylandOutput::handleWindowDestroyed()
{
    Q_D(QWaylandOutput);
    d->window = Q_NULLPTR;
    emit windowDestroyed();
}

/*!
 * \internal
 */
bool QWaylandOutput::event(QEvent *event)
{
    if (event->type() == QEvent::Polish)
        initialize();
    return QObject::event(event);
}

QT_END_NAMESPACE
