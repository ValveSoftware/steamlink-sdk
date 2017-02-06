/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
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

#include "scene3ditem_p.h"
#include "scene3dcleaner_p.h"
#include "scene3dlogging_p.h"
#include "scene3drenderer_p.h"
#include "scene3dsgnode_p.h"

#include <Qt3DCore/QAspectEngine>
#include <Qt3DCore/qentity.h>
#include <Qt3DRender/qcamera.h>
#include <Qt3DRender/QRenderAspect>
#include <Qt3DRender/qrendersurfaceselector.h>
#include <Qt3DRender/private/qrendersurfaceselector_p.h>
#include <Qt3DInput/QInputAspect>
#include <Qt3DInput/qinputsettings.h>
#include <Qt3DLogic/qlogicaspect.h>


#include <QtQuick/qquickwindow.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

/*!
    \class Qt3DCore::Scene3DItem
    \internal

    \brief The Qt3DCore::Scene3DItem class is a QQuickItem subclass used to integrate
    a Qt3D scene into a QtQuick 2 scene.

    The Qt3DCore::Scene3DItem class renders a Qt3D scene, provided by a Qt3DCore::QEntity
    into a multisampled Framebuffer object that is later blitted into a non
    multisampled Framebuffer object to be then renderer through the use of a
    Qt3DCore::Scene3DSGNode with premultiplied alpha.
 */
Scene3DItem::Scene3DItem(QQuickItem *parent)
    : QQuickItem(parent)
    , m_entity(nullptr)
    , m_aspectEngine(new Qt3DCore::QAspectEngine())
    , m_renderAspect(nullptr)
    , m_renderer(nullptr)
    , m_rendererCleaner(new Scene3DCleaner())
    , m_multisample(true)
    , m_cameraAspectRatioMode(AutomaticAspectRatio)
{
    setFlag(QQuickItem::ItemHasContents, true);
    setAcceptedMouseButtons(Qt::MouseButtonMask);
    // TO DO: register the event source in the main thread
}

Scene3DItem::~Scene3DItem()
{
    // When the window is closed, it first destroys all of its children. At
    // this point, Scene3DItem is destroyed but the Renderer, AspectEngine and
    // Scene3DSGNode still exist and will perform their cleanup on their own.
}

QStringList Scene3DItem::aspects() const
{
    return m_aspects;
}

Qt3DCore::QEntity *Scene3DItem::entity() const
{
    return m_entity;
}

void Scene3DItem::setAspects(const QStringList &aspects)
{
    if (!m_aspects.isEmpty()) {
        qWarning() << "Aspects already set on the Scene3D, ignoring";
        return;
    }

    m_aspects = aspects;

    // Aspects are owned by the aspect engine
    for (const QString &aspect : qAsConst(m_aspects)) {
        if (aspect == QLatin1String("render")) // This one is hardwired anyway
            continue;
        if (aspect == QLatin1String("input"))  {
            m_aspectEngine->registerAspect(new Qt3DInput::QInputAspect);
            continue;
        }
        if (aspect == QLatin1String("logic"))  {
            m_aspectEngine->registerAspect(new Qt3DLogic::QLogicAspect);
            continue;
        }
        m_aspectEngine->registerAspect(aspect);
    }

    emit aspectsChanged();
}

void Scene3DItem::setEntity(Qt3DCore::QEntity *entity)
{
    if (entity == m_entity)
        return;

    m_entity = entity;
    emit entityChanged();
}

void Scene3DItem::setCameraAspectRatioMode(CameraAspectRatioMode mode)
{
    if (m_cameraAspectRatioMode == mode)
        return;

    m_cameraAspectRatioMode = mode;
    setCameraAspectModeHelper();
    emit cameraAspectRatioModeChanged(mode);
}

void Scene3DItem::setHoverEnabled(bool enabled)
{
    if (enabled != acceptHoverEvents()) {
        setAcceptHoverEvents(enabled);
        emit hoverEnabledChanged();
    }
}

Scene3DItem::CameraAspectRatioMode Scene3DItem::cameraAspectRatioMode() const
{
    return m_cameraAspectRatioMode;
}

void Scene3DItem::applyRootEntityChange()
{
    if (m_aspectEngine->rootEntity() != m_entity) {
        m_aspectEngine->setRootEntity(Qt3DCore::QEntityPtr(m_entity));

        // Set the render surface
        if (!m_entity)
            return;

        setWindowSurface(m_entity);

        if (m_cameraAspectRatioMode == AutomaticAspectRatio) {
            // Set aspect ratio of first camera to match the window
            QList<Qt3DRender::QCamera *> cameras
                = m_entity->findChildren<Qt3DRender::QCamera *>();
            if (cameras.isEmpty()) {
                qWarning() << "No camera found and automatic aspect ratio requested";
            } else {
                m_camera = cameras.first();
                setCameraAspectModeHelper();
            }
        }

        // Set ourselves up as a source of input events for the input aspect
        Qt3DInput::QInputSettings *inputSettings = m_entity->findChild<Qt3DInput::QInputSettings *>();
        if (inputSettings) {
            inputSettings->setEventSource(this);
        } else {
            qWarning() << "No Input Settings found, keyboard and mouse events won't be handled";
        }
    }
}

void Scene3DItem::setWindowSurface(QObject *rootObject)
{
    Qt3DRender::QRenderSurfaceSelector *surfaceSelector = Qt3DRender::QRenderSurfaceSelectorPrivate::find(rootObject);

    // Set the item's window surface if it appears
    // the surface wasn't set on the surfaceSelector
    if (surfaceSelector && !surfaceSelector->surface())
        surfaceSelector->setSurface(this->window());
}

void Scene3DItem::setItemArea(const QSize &area)
{
    Qt3DRender::QRenderSurfaceSelector *surfaceSelector = Qt3DRender::QRenderSurfaceSelectorPrivate::find(m_entity);
    if (surfaceSelector)
        surfaceSelector->setExternalRenderTargetSize(area);
}

bool Scene3DItem::isHoverEnabled() const
{
    return acceptHoverEvents();
}

void Scene3DItem::setCameraAspectModeHelper()
{
    switch (m_cameraAspectRatioMode) {
    case AutomaticAspectRatio:
        connect(this, &Scene3DItem::widthChanged, this, &Scene3DItem::updateCameraAspectRatio);
        connect(this, &Scene3DItem::heightChanged, this, &Scene3DItem::updateCameraAspectRatio);
        // Update the aspect ratio the first time the surface is set
        updateCameraAspectRatio();
        break;
    case UserAspectRatio:
        disconnect(this, &Scene3DItem::widthChanged, this, &Scene3DItem::updateCameraAspectRatio);
        disconnect(this, &Scene3DItem::heightChanged, this, &Scene3DItem::updateCameraAspectRatio);
        break;
    }
}

void Scene3DItem::updateCameraAspectRatio()
{
    if (m_camera) {
        m_camera->setAspectRatio(static_cast<float>(width()) /
                                 static_cast<float>(height()));
    }
}

/*!
    \return \c true if a multisample renderbuffer is in use.
 */
bool Scene3DItem::multisample() const
{
    return m_multisample;
}

/*!
    Enables or disables the usage of multisample renderbuffers based on \a enable.

    By default multisampling is enabled. If the OpenGL implementation has no
    support for multisample renderbuffers or framebuffer blits, the request to
    use multisampling is ignored.

    \note Refrain from changing the value frequently as it involves expensive
    and potentially slow initialization of framebuffers and other OpenGL
    resources.
 */
void Scene3DItem::setMultisample(bool enable)
{
    if (m_multisample != enable) {
        m_multisample = enable;
        emit multisampleChanged();
        update();
    }
}

QSGNode *Scene3DItem::updatePaintNode(QSGNode *node, QQuickItem::UpdatePaintNodeData *)
{
    // If the render aspect wasn't created yet, do so now
    if (m_renderAspect == nullptr) {
        m_renderAspect = new QRenderAspect(QRenderAspect::Synchronous);
        m_aspectEngine->registerAspect(m_renderAspect);
    }

    if (m_renderer == nullptr) {
        m_renderer = new Scene3DRenderer(this, m_aspectEngine, m_renderAspect);
        m_renderer->setCleanerHelper(m_rendererCleaner);
    }

    // The main thread is blocked, it is now time to sync data between the renderer and the item.
    m_renderer->synchronize();

    Scene3DSGNode *fboNode = static_cast<Scene3DSGNode *>(node);
    if (fboNode == nullptr) {
        fboNode = new Scene3DSGNode();
        m_renderer->setSGNode(fboNode);
    }
    fboNode->setRect(boundingRect());

    return fboNode;
}

void Scene3DItem::mousePressEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    //Prevent subsequent move and release events being disregarded my the default event->ignore() from QQuickItem
}

} // namespace Qt3DRender

QT_END_NAMESPACE
