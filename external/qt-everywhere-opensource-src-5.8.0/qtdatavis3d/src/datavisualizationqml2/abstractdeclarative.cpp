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

#include "abstractdeclarative_p.h"
#include "declarativetheme_p.h"
#include "declarativerendernode_p.h"
#include <QtGui/QGuiApplication>
#if defined(Q_OS_IOS)
#include <QtCore/QTimer>
#endif
#if defined(Q_OS_OSX)
#include <qpa/qplatformnativeinterface.h>
#endif

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

static QList<const QQuickWindow *> clearList;
static QHash<AbstractDeclarative *, QQuickWindow *> graphWindowList;
static QHash<QQuickWindow *, bool> windowClearList;

AbstractDeclarative::AbstractDeclarative(QQuickItem *parent) :
    QQuickItem(parent),
    m_controller(0),
    m_contextWindow(0),
    m_renderMode(RenderIndirect),
    m_samples(0),
    m_windowSamples(0),
    m_initialisedSize(0, 0),
    #ifdef USE_SHARED_CONTEXT
    m_context(0),
    #else
    m_stateStore(0),
    #endif
    m_qtContext(0),
    m_mainThread(QThread::currentThread()),
    m_contextThread(0)
{
    m_nodeMutex = QSharedPointer<QMutex>(new QMutex);

    connect(this, &QQuickItem::windowChanged, this, &AbstractDeclarative::handleWindowChanged);

    // Set contents to false in case we are in qml designer to make component look nice
    m_runningInDesigner = QGuiApplication::applicationDisplayName() == "Qml2Puppet";
    setFlag(ItemHasContents, !m_runningInDesigner);
}

AbstractDeclarative::~AbstractDeclarative()
{
    destroyContext();

    disconnect(this, 0, this, 0);
    checkWindowList(0);

    m_nodeMutex.clear();
}

void AbstractDeclarative::setRenderingMode(AbstractDeclarative::RenderingMode mode)
{
    if (mode == m_renderMode)
        return;

    RenderingMode previousMode = m_renderMode;

    m_renderMode = mode;

    QQuickWindow *win = window();

    switch (mode) {
    case RenderDirectToBackground:
        // Intentional flowthrough
    case RenderDirectToBackground_NoClear:
        m_initialisedSize = QSize(0, 0);
        if (previousMode == RenderIndirect) {
            update();
            setFlag(ItemHasContents, false);
            if (win) {
                QObject::connect(win, &QQuickWindow::beforeRendering, this,
                                 &AbstractDeclarative::render, Qt::DirectConnection);
                checkWindowList(win);
                setAntialiasing(m_windowSamples > 0);
                if (m_windowSamples != m_samples)
                    emit msaaSamplesChanged(m_windowSamples);
            }
        }
        break;
    case RenderIndirect:
        m_initialisedSize = QSize(0, 0);
        setFlag(ItemHasContents, !m_runningInDesigner);
        update();
        if (win) {
            QObject::disconnect(win, &QQuickWindow::beforeRendering, this,
                                &AbstractDeclarative::render);
            checkWindowList(win);
        }
        setAntialiasing(m_samples > 0);
        if (m_windowSamples != m_samples)
            emit msaaSamplesChanged(m_samples);
        break;
    }

    updateWindowParameters();

    emit renderingModeChanged(mode);
}

AbstractDeclarative::RenderingMode AbstractDeclarative::renderingMode() const
{
    return m_renderMode;
}

QSGNode *AbstractDeclarative::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    QSize boundingSize = boundingRect().size().toSize() * m_controller->scene()->devicePixelRatio();
    if (m_runningInDesigner || boundingSize.width() <= 0 || boundingSize.height() <= 0
            || m_controller.isNull() || !window()) {
        delete oldNode;
        return 0;
    }
    DeclarativeRenderNode *node = static_cast<DeclarativeRenderNode *>(oldNode);

    if (!node) {
        node = new DeclarativeRenderNode(this, m_nodeMutex);
        node->setController(m_controller.data());
        node->setQuickWindow(window());
    }

    node->setSize(boundingSize);
    node->setSamples(m_samples);
    node->update();
    node->markDirty(QSGNode::DirtyMaterial);

    return node;
}

Declarative3DScene* AbstractDeclarative::scene() const
{
    return static_cast<Declarative3DScene *>(m_controller->scene());
}

void AbstractDeclarative::setTheme(Q3DTheme *theme)
{
    m_controller->setActiveTheme(theme, isComponentComplete());
}

Q3DTheme *AbstractDeclarative::theme() const
{
    return m_controller->activeTheme();
}

void AbstractDeclarative::clearSelection()
{
    m_controller->clearSelection();
}

void AbstractDeclarative::setSelectionMode(SelectionFlags mode)
{
    int intmode = int(mode);
    m_controller->setSelectionMode(QAbstract3DGraph::SelectionFlags(intmode));
}

AbstractDeclarative::SelectionFlags AbstractDeclarative::selectionMode() const
{
    int intmode = int(m_controller->selectionMode());
    return SelectionFlags(intmode);
}

void AbstractDeclarative::setShadowQuality(ShadowQuality quality)
{
    m_controller->setShadowQuality(QAbstract3DGraph::ShadowQuality(quality));
}

AbstractDeclarative::ShadowQuality AbstractDeclarative::shadowQuality() const
{
    return ShadowQuality(m_controller->shadowQuality());
}

bool AbstractDeclarative::shadowsSupported() const
{
    return m_controller->shadowsSupported();
}

int AbstractDeclarative::addCustomItem(QCustom3DItem *item)
{
    return m_controller->addCustomItem(item);
}

void AbstractDeclarative::removeCustomItems()
{
    m_controller->deleteCustomItems();
}

void AbstractDeclarative::removeCustomItem(QCustom3DItem *item)
{
    m_controller->deleteCustomItem(item);
}

void AbstractDeclarative::removeCustomItemAt(const QVector3D &position)
{
    m_controller->deleteCustomItem(position);
}

void AbstractDeclarative::releaseCustomItem(QCustom3DItem *item)
{
    m_controller->releaseCustomItem(item);
}

int AbstractDeclarative::selectedLabelIndex() const
{
    return m_controller->selectedLabelIndex();
}

QAbstract3DAxis *AbstractDeclarative::selectedAxis() const
{
    return m_controller->selectedAxis();
}

int AbstractDeclarative::selectedCustomItemIndex() const
{
    return m_controller->selectedCustomItemIndex();
}

QCustom3DItem *AbstractDeclarative::selectedCustomItem() const
{
    return m_controller->selectedCustomItem();
}

QQmlListProperty<QCustom3DItem> AbstractDeclarative::customItemList()
{
    return QQmlListProperty<QCustom3DItem>(this, this,
                                           &AbstractDeclarative::appendCustomItemFunc,
                                           &AbstractDeclarative::countCustomItemFunc,
                                           &AbstractDeclarative::atCustomItemFunc,
                                           &AbstractDeclarative::clearCustomItemFunc);
}

void AbstractDeclarative::appendCustomItemFunc(QQmlListProperty<QCustom3DItem> *list,
                                               QCustom3DItem *item)
{
    AbstractDeclarative *decl = reinterpret_cast<AbstractDeclarative *>(list->data);
    decl->addCustomItem(item);
}

int AbstractDeclarative::countCustomItemFunc(QQmlListProperty<QCustom3DItem> *list)
{
    return reinterpret_cast<AbstractDeclarative *>(list->data)->m_controller->m_customItems.size();
}

QCustom3DItem *AbstractDeclarative::atCustomItemFunc(QQmlListProperty<QCustom3DItem> *list,
                                                     int index)
{
    return reinterpret_cast<AbstractDeclarative *>(list->data)->m_controller->m_customItems.at(index);
}

void AbstractDeclarative::clearCustomItemFunc(QQmlListProperty<QCustom3DItem> *list)
{
    AbstractDeclarative *decl = reinterpret_cast<AbstractDeclarative *>(list->data);
    decl->removeCustomItems();
}

void AbstractDeclarative::setSharedController(Abstract3DController *controller)
{
    Q_ASSERT(controller);
    m_controller = controller;

    if (!m_controller->isOpenGLES())
        m_samples = 4;
    setAntialiasing(m_samples > 0);

    // Reset default theme, as the default C++ theme is Q3DTheme, not DeclarativeTheme3D.
    DeclarativeTheme3D *defaultTheme = new DeclarativeTheme3D;
    defaultTheme->d_ptr->setDefaultTheme(true);
    defaultTheme->setType(Q3DTheme::ThemeQt);
    m_controller->setActiveTheme(defaultTheme);

    QObject::connect(m_controller.data(), &Abstract3DController::shadowQualityChanged, this,
                     &AbstractDeclarative::handleShadowQualityChange);
    QObject::connect(m_controller.data(), &Abstract3DController::activeInputHandlerChanged, this,
                     &AbstractDeclarative::inputHandlerChanged);
    QObject::connect(m_controller.data(), &Abstract3DController::activeThemeChanged, this,
                     &AbstractDeclarative::themeChanged);
    QObject::connect(m_controller.data(), &Abstract3DController::selectionModeChanged, this,
                     &AbstractDeclarative::handleSelectionModeChange);
    QObject::connect(m_controller.data(), &Abstract3DController::elementSelected, this,
                     &AbstractDeclarative::handleSelectedElementChange);

    QObject::connect(m_controller.data(), &Abstract3DController::axisXChanged, this,
                     &AbstractDeclarative::handleAxisXChanged);
    QObject::connect(m_controller.data(), &Abstract3DController::axisYChanged, this,
                     &AbstractDeclarative::handleAxisYChanged);
    QObject::connect(m_controller.data(), &Abstract3DController::axisZChanged, this,
                     &AbstractDeclarative::handleAxisZChanged);

    QObject::connect(m_controller.data(), &Abstract3DController::measureFpsChanged, this,
                     &AbstractDeclarative::measureFpsChanged);
    QObject::connect(m_controller.data(), &Abstract3DController::currentFpsChanged, this,
                     &AbstractDeclarative::currentFpsChanged);

    QObject::connect(m_controller.data(), &Abstract3DController::orthoProjectionChanged, this,
                     &AbstractDeclarative::orthoProjectionChanged);

    QObject::connect(m_controller.data(), &Abstract3DController::aspectRatioChanged, this,
                     &AbstractDeclarative::aspectRatioChanged);
    QObject::connect(m_controller.data(), &Abstract3DController::optimizationHintsChanged, this,
                     &AbstractDeclarative::handleOptimizationHintChange);
    QObject::connect(m_controller.data(), &Abstract3DController::polarChanged, this,
                     &AbstractDeclarative::polarChanged);
    QObject::connect(m_controller.data(), &Abstract3DController::radialLabelOffsetChanged, this,
                     &AbstractDeclarative::radialLabelOffsetChanged);
    QObject::connect(m_controller.data(), &Abstract3DController::horizontalAspectRatioChanged, this,
                     &AbstractDeclarative::horizontalAspectRatioChanged);
    QObject::connect(m_controller.data(), &Abstract3DController::reflectionChanged, this,
                     &AbstractDeclarative::reflectionChanged);
    QObject::connect(m_controller.data(), &Abstract3DController::reflectivityChanged, this,
                     &AbstractDeclarative::reflectivityChanged);
    QObject::connect(m_controller.data(), &Abstract3DController::localeChanged, this,
                     &AbstractDeclarative::localeChanged);
    QObject::connect(m_controller.data(), &Abstract3DController::queriedGraphPositionChanged, this,
                     &AbstractDeclarative::queriedGraphPositionChanged);
    QObject::connect(m_controller.data(), &Abstract3DController::marginChanged, this,
                     &AbstractDeclarative::marginChanged);
}

void AbstractDeclarative::activateOpenGLContext(QQuickWindow *window)
{
#ifdef USE_SHARED_CONTEXT
    // We can assume we are not in middle of AbstractDeclarative destructor when we are here,
    // since m_context creation is always done when this function is called from
    // synchDataToRenderer(), which blocks main thread -> no need to mutex.
    if (!m_context || !m_qtContext || m_contextWindow != window) {
        QOpenGLContext *currentContext = QOpenGLContext::currentContext();

        // Note: Changing graph to different window when using multithreaded renderer will break!

        delete m_context;

        m_contextThread = QThread::currentThread();
        m_contextWindow = window;
        m_qtContext = currentContext;
        m_context = new QOpenGLContext();
        m_context->setFormat(m_qtContext->format());
        m_context->setShareContext(m_qtContext);
        m_context->create();

        m_context->makeCurrent(window);
        m_controller->initializeOpenGL();

        // Make sure context gets deleted.
        QObject::connect(m_contextThread, &QThread::finished, this,
                         &AbstractDeclarative::destroyContext, Qt::DirectConnection);
    } else {
        m_context->makeCurrent(window);
    }
#else
    // Shared contexts don't work properly in some platforms, so just store the
    // context state on those
    if (!m_stateStore || !m_qtContext || m_contextWindow != window) {
        QOpenGLContext *currentContext = QOpenGLContext::currentContext();

        // Note: Changing graph to different window when using multithreaded renderer will break!

        delete m_stateStore;

        m_contextThread = QThread::currentThread();
        m_contextWindow = window;
        m_qtContext = currentContext;

        m_stateStore = new GLStateStore(QOpenGLContext::currentContext());

        m_stateStore->storeGLState();
        m_controller->initializeOpenGL();

        // Make sure state store gets deleted.
        QObject::connect(m_contextThread, &QThread::finished, this,
                         &AbstractDeclarative::destroyContext, Qt::DirectConnection);
    } else {
        m_stateStore->storeGLState();
    }
#endif
}

void AbstractDeclarative::doneOpenGLContext(QQuickWindow *window)
{
#ifdef USE_SHARED_CONTEXT
    m_qtContext->makeCurrent(window);
#else
    Q_UNUSED(window)
    m_stateStore->restoreGLState();
#endif
}

void AbstractDeclarative::synchDataToRenderer()
{
    if (m_renderMode == RenderDirectToBackground && clearList.size())
        clearList.clear();

    QQuickWindow *win = window();
    activateOpenGLContext(win);
    m_controller->synchDataToRenderer();
    doneOpenGLContext(win);
}

int AbstractDeclarative::msaaSamples() const
{
    if (m_renderMode == RenderIndirect)
        return m_samples;
    else
        return m_windowSamples;
}

void AbstractDeclarative::setMsaaSamples(int samples)
{
    if (m_renderMode != RenderIndirect) {
        qWarning("Multisampling cannot be adjusted in this render mode");
    } else {
        if (m_controller->isOpenGLES()) {
            if (samples > 0)
                qWarning("Multisampling is not supported in OpenGL ES2");
        } else if (m_samples != samples) {
            m_samples = samples;
            setAntialiasing(m_samples > 0);
            emit msaaSamplesChanged(samples);
            update();
        }
    }
}

void AbstractDeclarative::handleWindowChanged(QQuickWindow *window)
{
    checkWindowList(window);

    if (!window)
        return;

#if defined(Q_OS_OSX)
    bool previousVisibility = window->isVisible();
    // Enable touch events for Mac touchpads
    window->setVisible(true);
    typedef void * (*EnableTouch)(QWindow*, bool);
    EnableTouch enableTouch =
            (EnableTouch)QGuiApplication::platformNativeInterface()->nativeResourceFunctionForIntegration("registertouchwindow");
    if (enableTouch)
        enableTouch(window, true);
    window->setVisible(previousVisibility);
#endif

    connect(window, &QObject::destroyed, this, &AbstractDeclarative::windowDestroyed);

    int oldWindowSamples = m_windowSamples;
    m_windowSamples = window->format().samples();
    if (m_windowSamples < 0)
        m_windowSamples = 0;

    connect(window, &QQuickWindow::beforeSynchronizing,
            this, &AbstractDeclarative::synchDataToRenderer,
            Qt::DirectConnection);

    if (m_renderMode == RenderDirectToBackground_NoClear
            || m_renderMode == RenderDirectToBackground) {
        connect(window, &QQuickWindow::beforeRendering, this, &AbstractDeclarative::render,
                Qt::DirectConnection);
        setAntialiasing(m_windowSamples > 0);
        if (m_windowSamples != oldWindowSamples)
            emit msaaSamplesChanged(m_windowSamples);
    }

    connect(m_controller.data(), &Abstract3DController::needRender, window, &QQuickWindow::update);

    updateWindowParameters();

#if defined(Q_OS_IOS)
    // Scenegraph render cycle in iOS sometimes misses update after beforeSynchronizing signal.
    // This ensures we don't end up displaying the graph without any data, in case update is
    // skipped after synchDataToRenderer.
    QTimer::singleShot(0, window, SLOT(update()));
#endif
}

void AbstractDeclarative::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChanged(newGeometry, oldGeometry);

    m_cachedGeometry = newGeometry;

    updateWindowParameters();
}

void AbstractDeclarative::itemChange(ItemChange change, const ItemChangeData & value)
{
    QQuickItem::itemChange(change, value);
    updateWindowParameters();
}

void AbstractDeclarative::updateWindowParameters()
{
    // Update the device pixel ratio, window size and bounding box
    QQuickWindow *win = window();
    if (win && !m_controller.isNull()) {
        Q3DScene *scene = m_controller->scene();
        if (win->devicePixelRatio() != scene->devicePixelRatio()) {
            scene->setDevicePixelRatio(win->devicePixelRatio());
            win->update();
        }

        bool directRender = m_renderMode == RenderDirectToBackground
                || m_renderMode == RenderDirectToBackground_NoClear;
        QSize windowSize;

        if (directRender)
            windowSize = win->size();
        else
            windowSize = m_cachedGeometry.size().toSize();

        if (windowSize != scene->d_ptr->windowSize()) {
            scene->d_ptr->setWindowSize(windowSize);
            win->update();
        }

        if (directRender) {
            // Origin mapping is needed when rendering directly to background
            QPointF point = QQuickItem::mapToScene(QPointF(0.0, 0.0));
            scene->d_ptr->setViewport(QRect(point.x() + 0.5f, point.y() + 0.5f,
                                            m_cachedGeometry.width() + 0.5f,
                                            m_cachedGeometry.height() + 0.5f));
        } else {
            // No translation needed when rendering to FBO
            scene->d_ptr->setViewport(QRect(0.0, 0.0, m_cachedGeometry.width() + 0.5f,
                                            m_cachedGeometry.height() + 0.5f));
        }
    }
}

void AbstractDeclarative::handleSelectionModeChange(QAbstract3DGraph::SelectionFlags mode)
{
    int intmode = int(mode);
    emit selectionModeChanged(SelectionFlags(intmode));
}

void AbstractDeclarative::handleShadowQualityChange(QAbstract3DGraph::ShadowQuality quality)
{
    emit shadowQualityChanged(ShadowQuality(quality));
}

void AbstractDeclarative::handleSelectedElementChange(QAbstract3DGraph::ElementType type)
{
    emit selectedElementChanged(ElementType(type));
}

void AbstractDeclarative::handleOptimizationHintChange(QAbstract3DGraph::OptimizationHints hints)
{
    int intHints = int(hints);
    emit optimizationHintsChanged(OptimizationHints(intHints));
}

void AbstractDeclarative::render()
{
    updateWindowParameters();

    // If we're not rendering directly to the background, return
    if (m_renderMode != RenderDirectToBackground && m_renderMode != RenderDirectToBackground_NoClear)
        return;

    // Clear the background once per window as that is not done by default
    QQuickWindow *win = window();
    activateOpenGLContext(win);
    QOpenGLFunctions *funcs = QOpenGLContext::currentContext()->functions();
    if (m_renderMode == RenderDirectToBackground && !clearList.contains(win)) {
        clearList.append(win);
        QColor clearColor = win->color();
        funcs->glClearColor(clearColor.redF(), clearColor.greenF(), clearColor.blueF(), 1.0f);
        funcs->glClear(GL_COLOR_BUFFER_BIT);
    }

    if (isVisible()) {
        funcs->glDepthMask(GL_TRUE);
        funcs->glEnable(GL_DEPTH_TEST);
        funcs->glDepthFunc(GL_LESS);
        funcs->glEnable(GL_CULL_FACE);
        funcs->glCullFace(GL_BACK);
        funcs->glDisable(GL_BLEND);

        m_controller->render();

        funcs->glEnable(GL_BLEND);
    }
    doneOpenGLContext(win);
}

QAbstract3DInputHandler* AbstractDeclarative::inputHandler() const
{
    return m_controller->activeInputHandler();
}

void AbstractDeclarative::setInputHandler(QAbstract3DInputHandler *inputHandler)
{
    m_controller->setActiveInputHandler(inputHandler);
}

void AbstractDeclarative::mouseDoubleClickEvent(QMouseEvent *event)
{
    m_controller->mouseDoubleClickEvent(event);
}

void AbstractDeclarative::touchEvent(QTouchEvent *event)
{
    m_controller->touchEvent(event);
    window()->update();
}

void AbstractDeclarative::mousePressEvent(QMouseEvent *event)
{
    QPoint mousePos = event->pos();
    m_controller->mousePressEvent(event, mousePos);
}

void AbstractDeclarative::mouseReleaseEvent(QMouseEvent *event)
{
    QPoint mousePos = event->pos();
    m_controller->mouseReleaseEvent(event, mousePos);
}

void AbstractDeclarative::mouseMoveEvent(QMouseEvent *event)
{
    QPoint mousePos = event->pos();
    m_controller->mouseMoveEvent(event, mousePos);
}

void AbstractDeclarative::wheelEvent(QWheelEvent *event)
{
    m_controller->wheelEvent(event);
}

void AbstractDeclarative::checkWindowList(QQuickWindow *window)
{
    QQuickWindow *oldWindow = graphWindowList.value(this);

    graphWindowList[this] = window;

    if (oldWindow != window && oldWindow) {
        QObject::disconnect(oldWindow, &QObject::destroyed, this,
                            &AbstractDeclarative::windowDestroyed);
        QObject::disconnect(oldWindow, &QQuickWindow::beforeSynchronizing, this,
                            &AbstractDeclarative::synchDataToRenderer);
        QObject::disconnect(oldWindow, &QQuickWindow::beforeRendering, this,
                            &AbstractDeclarative::render);
        if (!m_controller.isNull()) {
            QObject::disconnect(m_controller.data(), &Abstract3DController::needRender,
                                oldWindow, &QQuickWindow::update);
        }
    }

    QList<QQuickWindow *> windowList;

    foreach (AbstractDeclarative *graph, graphWindowList.keys()) {
        if (graph->m_renderMode == RenderDirectToBackground
                || graph->m_renderMode == RenderDirectToBackground_NoClear) {
            windowList.append(graphWindowList.value(graph));
        }
    }

    if (oldWindow && !windowList.contains(oldWindow)
            && windowClearList.values(oldWindow).size() != 0) {
        // Return window clear value
        oldWindow->setClearBeforeRendering(windowClearList.value(oldWindow));
        windowClearList.remove(oldWindow);
    }

    if (!window) {
        graphWindowList.remove(this);
        return;
    }

    if ((m_renderMode == RenderDirectToBackground
         || m_renderMode == RenderDirectToBackground_NoClear)
            && windowClearList.values(window).size() == 0) {
        // Save old clear value
        windowClearList[window] = window->clearBeforeRendering();
        // Disable clearing of the window as we render underneath
        window->setClearBeforeRendering(false);
    }
}

void AbstractDeclarative::setMeasureFps(bool enable)
{
    m_controller->setMeasureFps(enable);
}

bool AbstractDeclarative::measureFps() const
{
    return m_controller->measureFps();
}

qreal AbstractDeclarative::currentFps() const
{
    return m_controller->currentFps();
}

void AbstractDeclarative::setOrthoProjection(bool enable)
{
    m_controller->setOrthoProjection(enable);
}

bool AbstractDeclarative::isOrthoProjection() const
{
    return m_controller->isOrthoProjection();
}

AbstractDeclarative::ElementType AbstractDeclarative::selectedElement() const
{
    return ElementType(m_controller->selectedElement());
}

void AbstractDeclarative::setAspectRatio(qreal ratio)
{
    m_controller->setAspectRatio(ratio);
}

qreal AbstractDeclarative::aspectRatio() const
{
    return m_controller->aspectRatio();
}

void AbstractDeclarative::setOptimizationHints(OptimizationHints hints)
{
    int intmode = int(hints);
    m_controller->setOptimizationHints(QAbstract3DGraph::OptimizationHints(intmode));
}

AbstractDeclarative::OptimizationHints AbstractDeclarative::optimizationHints() const
{
    int intmode = int(m_controller->optimizationHints());
    return OptimizationHints(intmode);
}

void AbstractDeclarative::setPolar(bool enable)
{
    m_controller->setPolar(enable);
}

bool AbstractDeclarative::isPolar() const
{
    return m_controller->isPolar();
}

void AbstractDeclarative::setRadialLabelOffset(float offset)
{
    m_controller->setRadialLabelOffset(offset);
}

float AbstractDeclarative::radialLabelOffset() const
{
    return m_controller->radialLabelOffset();
}

void AbstractDeclarative::setHorizontalAspectRatio(qreal ratio)
{
    m_controller->setHorizontalAspectRatio(ratio);
}

qreal AbstractDeclarative::horizontalAspectRatio() const
{
    return m_controller->horizontalAspectRatio();
}

void AbstractDeclarative::setReflection(bool enable)
{
    m_controller->setReflection(enable);
}

bool AbstractDeclarative::isReflection() const
{
    return m_controller->reflection();
}

void AbstractDeclarative::setReflectivity(qreal reflectivity)
{
    m_controller->setReflectivity(reflectivity);
}

qreal AbstractDeclarative::reflectivity() const
{
    return m_controller->reflectivity();
}

void AbstractDeclarative::setLocale(const QLocale &locale)
{
    m_controller->setLocale(locale);
}

QLocale AbstractDeclarative::locale() const
{
    return m_controller->locale();
}

QVector3D AbstractDeclarative::queriedGraphPosition() const
{
    return m_controller->queriedGraphPosition();
}

void AbstractDeclarative::setMargin(qreal margin)
{
    m_controller->setMargin(margin);
}

qreal AbstractDeclarative::margin() const
{
    return m_controller->margin();
}

void AbstractDeclarative::windowDestroyed(QObject *obj)
{
    // Remove destroyed window from window lists
    QQuickWindow *win = static_cast<QQuickWindow *>(obj);
    QQuickWindow *oldWindow = graphWindowList.value(this);

    if (win == oldWindow)
        graphWindowList.remove(this);

    windowClearList.remove(win);
}

void AbstractDeclarative::destroyContext()
{
#ifdef USE_SHARED_CONTEXT
    // Context can be in another thread, don't delete it directly in that case
    if (m_contextThread && m_contextThread != m_mainThread) {
        if (m_context)
            m_context->deleteLater();
    } else {
        delete m_context;
    }
    m_context = 0;
#else
    if (m_contextThread && m_contextThread != m_mainThread) {
        if (m_stateStore)
            m_stateStore->deleteLater();
    } else {
        delete m_stateStore;
    }
    m_stateStore = 0;
#endif
    if (m_contextThread) {
        QObject::disconnect(m_contextThread, &QThread::finished, this,
                            &AbstractDeclarative::destroyContext);
        m_contextThread = 0;
    }
}

QT_END_NAMESPACE_DATAVISUALIZATION
