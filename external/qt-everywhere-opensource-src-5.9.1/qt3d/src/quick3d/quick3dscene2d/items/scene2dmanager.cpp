/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include "qscene2d.h"
#include "qscene2d_p.h"
#include "scene2d_p.h"
#include "scene2dmanager_p.h"
#include "scene2devent_p.h"

#include <Qt3DCore/QPropertyUpdatedChange>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {

namespace Quick {

class RenderControl : public QQuickRenderControl
{
public:
    RenderControl(QWindow *w) : m_window(w) { }
    QWindow *renderWindow(QPoint *offset) Q_DECL_OVERRIDE;

private:
    QWindow *m_window;
};

QWindow *RenderControl::renderWindow(QPoint *offset)
{
    if (offset)
        *offset = QPoint(0, 0);
    return m_window;
}

/*!
    \internal
     Constructs qml render manager.
 */
Scene2DManager::Scene2DManager(QScene2DPrivate *priv)
    : m_rootItem(nullptr)
    , m_item(nullptr)
    , m_priv(priv)
    , m_sharedObject(new Scene2DSharedObject(this))
    , m_renderPolicy(QScene2D::Continuous)
    , m_requested(false)
    , m_initialized(false)
    , m_renderSyncRequested(false)
    , m_backendInitialized(false)
    , m_mouseEnabled(true)
{
    m_sharedObject->m_surface = new QOffscreenSurface;
    m_sharedObject->m_surface->setFormat(QSurfaceFormat::defaultFormat());
    m_sharedObject->m_surface->create();

    // Create render control
    m_sharedObject->m_renderControl = new RenderControl(nullptr);

    // Create window to render the QML with
    m_sharedObject->m_quickWindow = new QQuickWindow(m_sharedObject->m_renderControl);
    m_sharedObject->m_quickWindow->setClearBeforeRendering(false);

    connect(m_sharedObject->m_renderControl, &QQuickRenderControl::renderRequested,
            this, &Scene2DManager::requestRender);
    connect(m_sharedObject->m_renderControl, &QQuickRenderControl::sceneChanged,
            this, &Scene2DManager::requestRenderSync);
}

Scene2DManager::~Scene2DManager()
{
    m_sharedObject = nullptr;
}

void Scene2DManager::requestRender()
{
    // Don't request render until the backend is initialized.
    if (m_sharedObject->canRender()) {
        if (!m_requested) {
            m_requested = true;
            QCoreApplication::postEvent(this, new Scene2DEvent(Scene2DEvent::Render));
        }
    }
}

void Scene2DManager::requestRenderSync()
{
    // Don't request render until the backed is initialized.
    if (m_sharedObject->canRender()) {
        if (!m_requested) {
            m_requested = true;
            QCoreApplication::postEvent(this, new Scene2DEvent(Scene2DEvent::RenderSync));
        }
    } else {
        m_renderSyncRequested = true;
    }
}

void Scene2DManager::startIfInitialized()
{
    if (!m_initialized && m_backendInitialized && m_item != nullptr) {
        m_rootItem = m_item;

        // Associate root item with the window.
        m_rootItem->setParentItem(m_sharedObject->m_quickWindow->contentItem());

        // Update window size.
        updateSizes();

        m_initialized = true;
        m_sharedObject->setInitialized();
    }
}

void Scene2DManager::stopAndClean()
{
    if (m_sharedObject->isInitialized()) {
        QMutexLocker lock(&m_sharedObject->m_mutex);
        m_sharedObject->requestQuit();
        m_sharedObject->wait();
        m_sharedObject->cleanup();
    }
}

void Scene2DManager::updateSizes()
{
    const int width = m_rootItem->width();
    const int height = m_rootItem->height();
    if (width == 0 || height == 0) {
        qWarning() << "QScene2D: Root item size not set.";
        return;
    }
    m_sharedObject->m_quickWindow->setGeometry(0, 0, width, height);
}

void Scene2DManager::setItem(QQuickItem *item)
{
    m_item = item;
    startIfInitialized();
}

bool Scene2DManager::event(QEvent *e)
{
    switch (static_cast<Scene2DEvent::Type>(e->type())) {

    case Scene2DEvent::Render: {
        // just render request, don't need to call sync in render thread
        QMutexLocker lock(&m_sharedObject->m_mutex);
        m_sharedObject->requestRender(false);
        m_requested = false;
        return true;
    }

    case Scene2DEvent::RenderSync: {
        // sync and render request, main and render threads must be synchronized
        if (!m_sharedObject->isQuit())
            doRenderSync();
        m_requested = false;
        return true;
    }

    case Scene2DEvent::Prepare: {
        m_sharedObject->m_renderControl->prepareThread(m_sharedObject->m_renderThread);
        m_sharedObject->setPrepared();

        if (m_renderSyncRequested) {
            if (!m_requested) {
                m_requested = true;
                QCoreApplication::postEvent(this, new Scene2DEvent(Scene2DEvent::RenderSync));
            }
            m_renderSyncRequested = false;
        }
        return true;
    }

    case Scene2DEvent::Initialized: {
        // backend is initialized, start the qml
        m_backendInitialized = true;
        startIfInitialized();
        return true;
    }

    case Scene2DEvent::Rendered: {
        // render is done, excellent, now clean anything not needed anymore.
        stopAndClean();
        return true;
    }

    default:
        break;
    }
    return QObject::event(e);
}

void Scene2DManager::doRenderSync()
{
    QMutexLocker lock(&m_sharedObject->m_mutex);

    m_sharedObject->requestRender(true);
    m_sharedObject->m_renderControl->polishItems();

    // begin waiting render thread
    m_sharedObject->wait();
    m_requested = false;
}

void Scene2DManager::cleanup()
{
    stopAndClean();
}

} // namespace Quick
} // namespace Qt3DRender

QT_END_NAMESPACE
