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

#include "abstract3drenderer_p.h"
#include "texturehelper_p.h"
#include "q3dcamera_p.h"
#include "q3dtheme_p.h"
#include "qvalue3daxisformatter_p.h"
#include "shaderhelper_p.h"
#include "qcustom3ditem_p.h"
#include "qcustom3dlabel_p.h"
#include "qcustom3dvolume_p.h"
#include "scatter3drenderer_p.h"

#include <QtCore/qmath.h>
#include <QtGui/QOffscreenSurface>
#include <QtCore/QThread>

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

// Defined in shaderhelper.cpp
extern void discardDebugMsgs(QtMsgType type, const QMessageLogContext &context, const QString &msg);

const qreal doublePi(M_PI * 2.0);
const int polarGridRoundness(64);
const qreal polarGridAngle(doublePi / qreal(polarGridRoundness));
const float polarGridAngleDegrees(float(360.0 / qreal(polarGridRoundness)));
const qreal polarGridHalfAngle(polarGridAngle / 2.0);

Abstract3DRenderer::Abstract3DRenderer(Abstract3DController *controller)
    : QObject(0),
      m_hasNegativeValues(false),
      m_cachedTheme(new Q3DTheme()),
      m_drawer(new Drawer(m_cachedTheme)),
      m_cachedShadowQuality(QAbstract3DGraph::ShadowQualityMedium),
      m_autoScaleAdjustment(1.0f),
      m_cachedSelectionMode(QAbstract3DGraph::SelectionNone),
      m_cachedOptimizationHint(QAbstract3DGraph::OptimizationDefault),
      m_textureHelper(0),
      m_depthTexture(0),
      m_cachedScene(new Q3DScene()),
      m_selectionDirty(true),
      m_selectionState(SelectNone),
      m_devicePixelRatio(1.0f),
      m_selectionLabelDirty(true),
      m_clickResolved(false),
      m_graphPositionQueryPending(false),
      m_graphPositionQueryResolved(false),
      m_clickedSeries(0),
      m_clickedType(QAbstract3DGraph::ElementNone),
      m_selectedLabelIndex(-1),
      m_selectedCustomItemIndex(-1),
      m_selectionLabelItem(0),
      m_visibleSeriesCount(0),
      m_customItemShader(0),
      m_volumeTextureShader(0),
      m_volumeTextureLowDefShader(0),
      m_volumeTextureSliceShader(0),
      m_volumeSliceFrameShader(0),
      m_labelShader(0),
      m_cursorPositionShader(0),
      m_cursorPositionFrameBuffer(0),
      m_cursorPositionTexture(0),
      m_useOrthoProjection(false),
      m_xFlipped(false),
      m_yFlipped(false),
      m_zFlipped(false),
      m_yFlippedForGrid(false),
      m_backgroundObj(0),
      m_gridLineObj(0),
      m_labelObj(0),
      m_positionMapperObj(0),
      m_graphAspectRatio(2.0f),
      m_graphHorizontalAspectRatio(0.0f),
      m_polarGraph(false),
      m_radialLabelOffset(1.0f),
      m_polarRadius(2.0f),
      m_xRightAngleRotation(QQuaternion::fromAxisAndAngle(1.0f, 0.0f, 0.0f, 90.0f)),
      m_yRightAngleRotation(QQuaternion::fromAxisAndAngle(0.0f, 1.0f, 0.0f, 90.0f)),
      m_zRightAngleRotation(QQuaternion::fromAxisAndAngle(0.0f, 0.0f, 1.0f, 90.0f)),
      m_xRightAngleRotationNeg(QQuaternion::fromAxisAndAngle(1.0f, 0.0f, 0.0f, -90.0f)),
      m_yRightAngleRotationNeg(QQuaternion::fromAxisAndAngle(0.0f, 1.0f, 0.0f, -90.0f)),
      m_zRightAngleRotationNeg(QQuaternion::fromAxisAndAngle(0.0f, 0.0f, 1.0f, -90.0f)),
      m_xFlipRotation(QQuaternion::fromAxisAndAngle(1.0f, 0.0f, 0.0f, -180.0f)),
      m_zFlipRotation(QQuaternion::fromAxisAndAngle(0.0f, 0.0f, 1.0f, -180.0f)),
      m_requestedMargin(-1.0f),
      m_vBackgroundMargin(0.1f),
      m_hBackgroundMargin(0.1f),
      m_scaleXWithBackground(0.0f),
      m_scaleYWithBackground(0.0f),
      m_scaleZWithBackground(0.0f),
      m_oldCameraTarget(QVector3D(2000.0f, 2000.0f, 2000.0f)), // Just random invalid target
      m_reflectionEnabled(false),
      m_reflectivity(0.5),
#if !defined(QT_OPENGL_ES_2)
      m_funcs_2_1(0),
#endif
      m_context(0),
      m_dummySurfaceAtDelete(0),
      m_isOpenGLES(true)

{
    initializeOpenGLFunctions();
    m_isOpenGLES = Utils::isOpenGLES();
#if !defined(QT_OPENGL_ES_2)
    if (!m_isOpenGLES) {
        // Discard warnings about deprecated functions
        QtMessageHandler handler = qInstallMessageHandler(discardDebugMsgs);

        m_funcs_2_1 = QOpenGLContext::currentContext()->versionFunctions<QOpenGLFunctions_2_1>();
        m_funcs_2_1->initializeOpenGLFunctions();

        // Restore original message handler
        qInstallMessageHandler(handler);
    }
#endif
    QObject::connect(m_drawer, &Drawer::drawerChanged, this, &Abstract3DRenderer::updateTextures);
    QObject::connect(this, &Abstract3DRenderer::needRender, controller,
                     &Abstract3DController::needRender, Qt::QueuedConnection);
    QObject::connect(this, &Abstract3DRenderer::requestShadowQuality, controller,
                     &Abstract3DController::handleRequestShadowQuality, Qt::QueuedConnection);
}

Abstract3DRenderer::~Abstract3DRenderer()
{
    delete m_drawer;
    delete m_cachedScene;
    delete m_cachedTheme;
    delete m_selectionLabelItem;
    delete m_customItemShader;
    delete m_volumeTextureShader;
    delete m_volumeTextureLowDefShader;
    delete m_volumeSliceFrameShader;
    delete m_volumeTextureSliceShader;
    delete m_labelShader;
    delete m_cursorPositionShader;

    foreach (SeriesRenderCache *cache, m_renderCacheList) {
        cache->cleanup(m_textureHelper);
        delete cache;
    }
    m_renderCacheList.clear();

    foreach (CustomRenderItem *item, m_customRenderCache) {
        GLuint texture = item->texture();
        m_textureHelper->deleteTexture(&texture);
        delete item;
    }
    m_customRenderCache.clear();

    ObjectHelper::releaseObjectHelper(this, m_backgroundObj);
    ObjectHelper::releaseObjectHelper(this, m_gridLineObj);
    ObjectHelper::releaseObjectHelper(this, m_labelObj);
    ObjectHelper::releaseObjectHelper(this, m_positionMapperObj);

    if (m_textureHelper) {
        m_textureHelper->deleteTexture(&m_depthTexture);
        m_textureHelper->deleteTexture(&m_cursorPositionTexture);

        if (QOpenGLContext::currentContext())
            m_textureHelper->glDeleteFramebuffers(1, &m_cursorPositionFrameBuffer);

        delete m_textureHelper;
    }

    m_axisCacheX.clearLabels();
    m_axisCacheY.clearLabels();
    m_axisCacheZ.clearLabels();

    restoreContextAfterDelete();
}

void Abstract3DRenderer::initializeOpenGL()
{
    m_context = QOpenGLContext::currentContext();

    // Set OpenGL features
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

#if !defined(QT_OPENGL_ES_2)
    if (!m_isOpenGLES) {
        glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
        glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
    }
#endif

    m_textureHelper = new TextureHelper();
    m_drawer->initializeOpenGL();

    axisCacheForOrientation(QAbstract3DAxis::AxisOrientationX).setDrawer(m_drawer);
    axisCacheForOrientation(QAbstract3DAxis::AxisOrientationY).setDrawer(m_drawer);
    axisCacheForOrientation(QAbstract3DAxis::AxisOrientationZ).setDrawer(m_drawer);

    initLabelShaders(QStringLiteral(":/shaders/vertexLabel"),
                     QStringLiteral(":/shaders/fragmentLabel"));

    initCursorPositionShaders(QStringLiteral(":/shaders/vertexPosition"),
                              QStringLiteral(":/shaders/fragmentPositionMap"));

    loadLabelMesh();
    loadPositionMapperMesh();
}

void Abstract3DRenderer::render(const GLuint defaultFboHandle)
{
    if (defaultFboHandle) {
        glDepthMask(true);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glDisable(GL_BLEND); // For QtQuick2 blending is enabled by default, but we don't want it to be
    }

    // Clear the graph background to the theme color
    glViewport(m_viewport.x(),
               m_viewport.y(),
               m_viewport.width(),
               m_viewport.height());
    glScissor(m_viewport.x(),
              m_viewport.y(),
              m_viewport.width(),
              m_viewport.height());
    glEnable(GL_SCISSOR_TEST);
    QVector4D clearColor = Utils::vectorFromColor(m_cachedTheme->windowColor());
    glClearColor(clearColor.x(), clearColor.y(), clearColor.z(), 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
}

void Abstract3DRenderer::updateSelectionState(SelectionState state)
{
    m_selectionState = state;
}

void Abstract3DRenderer::initGradientShaders(const QString &vertexShader,
                                             const QString &fragmentShader)
{
    // Do nothing by default
    Q_UNUSED(vertexShader)
    Q_UNUSED(fragmentShader)
}

void Abstract3DRenderer::initStaticSelectedItemShaders(const QString &vertexShader,
                                                       const QString &fragmentShader,
                                                       const QString &gradientVertexShader,
                                                       const QString &gradientFragmentShader)
{
    // Do nothing by default
    Q_UNUSED(vertexShader)
    Q_UNUSED(fragmentShader)
    Q_UNUSED(gradientVertexShader)
    Q_UNUSED(gradientFragmentShader)
}

void Abstract3DRenderer::initCustomItemShaders(const QString &vertexShader,
                                               const QString &fragmentShader)
{
    delete m_customItemShader;
    m_customItemShader = new ShaderHelper(this, vertexShader, fragmentShader);
    m_customItemShader->initialize();
}

void Abstract3DRenderer::initVolumeTextureShaders(const QString &vertexShader,
                                                  const QString &fragmentShader,
                                                  const QString &fragmentLowDefShader,
                                                  const QString &sliceShader,
                                                  const QString &sliceFrameVertexShader,
                                                  const QString &sliceFrameShader)
{

    delete m_volumeTextureShader;
    m_volumeTextureShader = new ShaderHelper(this, vertexShader, fragmentShader);
    m_volumeTextureShader->initialize();

    delete m_volumeTextureLowDefShader;
    m_volumeTextureLowDefShader = new ShaderHelper(this, vertexShader, fragmentLowDefShader);
    m_volumeTextureLowDefShader->initialize();

    delete m_volumeTextureSliceShader;
    m_volumeTextureSliceShader = new ShaderHelper(this, vertexShader, sliceShader);
    m_volumeTextureSliceShader->initialize();

    delete m_volumeSliceFrameShader;
    m_volumeSliceFrameShader = new ShaderHelper(this, sliceFrameVertexShader, sliceFrameShader);
    m_volumeSliceFrameShader->initialize();
}

void Abstract3DRenderer::initLabelShaders(const QString &vertexShader, const QString &fragmentShader)
{
    delete m_labelShader;
    m_labelShader = new ShaderHelper(this, vertexShader, fragmentShader);
    m_labelShader->initialize();
}

void Abstract3DRenderer::initCursorPositionShaders(const QString &vertexShader,
                                                   const QString &fragmentShader)
{
    // Init the shader
    delete m_cursorPositionShader;
    m_cursorPositionShader = new ShaderHelper(this, vertexShader, fragmentShader);
    m_cursorPositionShader->initialize();
}

void Abstract3DRenderer::initCursorPositionBuffer()
{
    m_textureHelper->deleteTexture(&m_cursorPositionTexture);
    m_textureHelper->glDeleteFramebuffers(1, &m_cursorPositionFrameBuffer);
    m_cursorPositionFrameBuffer = 0;

    if (m_primarySubViewport.size().isEmpty())
        return;

    m_cursorPositionTexture =
            m_textureHelper->createCursorPositionTexture(m_primarySubViewport.size(),
                                                         m_cursorPositionFrameBuffer);
}

void Abstract3DRenderer::updateTheme(Q3DTheme *theme)
{
    // Synchronize the controller theme with renderer
    bool updateDrawer = theme->d_ptr->sync(*m_cachedTheme->d_ptr);

    if (updateDrawer)
        m_drawer->setTheme(m_cachedTheme);
}

void Abstract3DRenderer::updateScene(Q3DScene *scene)
{
    m_viewport = scene->d_ptr->glViewport();
    m_secondarySubViewport = scene->d_ptr->glSecondarySubViewport();

    if (m_primarySubViewport != scene->d_ptr->glPrimarySubViewport()) {
        // Resize of primary subviewport means resizing shadow and selection buffers
        m_primarySubViewport = scene->d_ptr->glPrimarySubViewport();
        handleResize();
    }

    if (m_devicePixelRatio != scene->devicePixelRatio()) {
        m_devicePixelRatio = scene->devicePixelRatio();
        handleResize();
    }

    QPoint logicalPixelPosition = scene->selectionQueryPosition();
    m_inputPosition = QPoint(logicalPixelPosition.x() * m_devicePixelRatio,
                             logicalPixelPosition.y() * m_devicePixelRatio);

    QPoint logicalGraphPosition = scene->graphPositionQuery();
    m_graphPositionQuery = QPoint(logicalGraphPosition.x() * m_devicePixelRatio,
                                  logicalGraphPosition.y() * m_devicePixelRatio);

    // Synchronize the renderer scene to controller scene
    scene->d_ptr->sync(*m_cachedScene->d_ptr);

    updateCameraViewport();

    if (Q3DScene::invalidSelectionPoint() == logicalPixelPosition) {
        updateSelectionState(SelectNone);
    } else {
        if (scene->isSlicingActive()) {
            if (scene->isPointInPrimarySubView(logicalPixelPosition))
                updateSelectionState(SelectOnOverview);
            else if (scene->isPointInSecondarySubView(logicalPixelPosition))
                updateSelectionState(SelectOnSlice);
            else
                updateSelectionState(SelectNone);
        } else {
            updateSelectionState(SelectOnScene);
        }
    }

    if (Q3DScene::invalidSelectionPoint() != logicalGraphPosition)
        m_graphPositionQueryPending = true;

    // Queue up another render when we have a query that needs resolving.
    // This is needed because QtQuick scene graph can sometimes do a sync without following it up
    // with a render.
    if (m_graphPositionQueryPending || m_selectionState != SelectNone)
        emit needRender();
}

void Abstract3DRenderer::updateTextures()
{
    m_axisCacheX.updateTextures();
    m_axisCacheY.updateTextures();
    m_axisCacheZ.updateTextures();
}

void Abstract3DRenderer::reInitShaders()
{
    if (!m_isOpenGLES) {
        if (m_cachedShadowQuality > QAbstract3DGraph::ShadowQualityNone) {
            if (m_cachedOptimizationHint.testFlag(QAbstract3DGraph::OptimizationStatic)
                    && qobject_cast<Scatter3DRenderer *>(this)) {
                initGradientShaders(QStringLiteral(":/shaders/vertexShadow"),
                                    QStringLiteral(":/shaders/fragmentShadow"));
                initStaticSelectedItemShaders(QStringLiteral(":/shaders/vertexShadow"),
                                              QStringLiteral(":/shaders/fragmentShadowNoTex"),
                                              QStringLiteral(":/shaders/vertexShadow"),
                                              QStringLiteral(":/shaders/fragmentShadowNoTexColorOnY"));
                initShaders(QStringLiteral(":/shaders/vertexShadowNoMatrices"),
                            QStringLiteral(":/shaders/fragmentShadowNoTex"));
            } else {
                initGradientShaders(QStringLiteral(":/shaders/vertexShadow"),
                                    QStringLiteral(":/shaders/fragmentShadowNoTexColorOnY"));
                initShaders(QStringLiteral(":/shaders/vertexShadow"),
                            QStringLiteral(":/shaders/fragmentShadowNoTex"));
            }
            initBackgroundShaders(QStringLiteral(":/shaders/vertexShadow"),
                                  QStringLiteral(":/shaders/fragmentShadowNoTex"));
            initCustomItemShaders(QStringLiteral(":/shaders/vertexShadow"),
                                  QStringLiteral(":/shaders/fragmentShadow"));
        } else {
            if (m_cachedOptimizationHint.testFlag(QAbstract3DGraph::OptimizationStatic)
                    && qobject_cast<Scatter3DRenderer *>(this)) {
                initGradientShaders(QStringLiteral(":/shaders/vertexTexture"),
                                    QStringLiteral(":/shaders/fragmentTexture"));
                initStaticSelectedItemShaders(QStringLiteral(":/shaders/vertex"),
                                              QStringLiteral(":/shaders/fragment"),
                                              QStringLiteral(":/shaders/vertex"),
                                              QStringLiteral(":/shaders/fragmentColorOnY"));
                initShaders(QStringLiteral(":/shaders/vertexNoMatrices"),
                            QStringLiteral(":/shaders/fragment"));
            } else {
                initGradientShaders(QStringLiteral(":/shaders/vertex"),
                                    QStringLiteral(":/shaders/fragmentColorOnY"));
                initShaders(QStringLiteral(":/shaders/vertex"),
                            QStringLiteral(":/shaders/fragment"));
            }
            initBackgroundShaders(QStringLiteral(":/shaders/vertex"),
                                  QStringLiteral(":/shaders/fragment"));
            initCustomItemShaders(QStringLiteral(":/shaders/vertexTexture"),
                                  QStringLiteral(":/shaders/fragmentTexture"));
        }
        initVolumeTextureShaders(QStringLiteral(":/shaders/vertexTexture3D"),
                                 QStringLiteral(":/shaders/fragmentTexture3D"),
                                 QStringLiteral(":/shaders/fragmentTexture3DLowDef"),
                                 QStringLiteral(":/shaders/fragmentTexture3DSlice"),
                                 QStringLiteral(":/shaders/vertexPosition"),
                                 QStringLiteral(":/shaders/fragment3DSliceFrames"));
    } else  {
        if (m_cachedOptimizationHint.testFlag(QAbstract3DGraph::OptimizationStatic)
                && qobject_cast<Scatter3DRenderer *>(this)) {
            initGradientShaders(QStringLiteral(":/shaders/vertexTexture"),
                                QStringLiteral(":/shaders/fragmentTextureES2"));
            initStaticSelectedItemShaders(QStringLiteral(":/shaders/vertex"),
                                          QStringLiteral(":/shaders/fragmentES2"),
                                          QStringLiteral(":/shaders/vertex"),
                                          QStringLiteral(":/shaders/fragmentColorOnYES2"));
            initShaders(QStringLiteral(":/shaders/vertexNoMatrices"),
                        QStringLiteral(":/shaders/fragmentES2"));
        } else {
            initGradientShaders(QStringLiteral(":/shaders/vertex"),
                                QStringLiteral(":/shaders/fragmentColorOnYES2"));
            initShaders(QStringLiteral(":/shaders/vertex"),
                        QStringLiteral(":/shaders/fragmentES2"));
        }
        initBackgroundShaders(QStringLiteral(":/shaders/vertex"),
                              QStringLiteral(":/shaders/fragmentES2"));
        initCustomItemShaders(QStringLiteral(":/shaders/vertexTexture"),
                              QStringLiteral(":/shaders/fragmentTextureES2"));
    }
}

void Abstract3DRenderer::handleShadowQualityChange()
{
    reInitShaders();

    if (m_isOpenGLES && m_cachedShadowQuality != QAbstract3DGraph::ShadowQualityNone) {
        emit requestShadowQuality(QAbstract3DGraph::ShadowQualityNone);
        qWarning("Shadows are not yet supported for OpenGL ES2");
        m_cachedShadowQuality = QAbstract3DGraph::ShadowQualityNone;
    }
}

void Abstract3DRenderer::updateSelectionMode(QAbstract3DGraph::SelectionFlags mode)
{
    m_cachedSelectionMode = mode;
    m_selectionDirty = true;
}

void Abstract3DRenderer::updateAspectRatio(float ratio)
{
    m_graphAspectRatio = ratio;
    foreach (SeriesRenderCache *cache, m_renderCacheList)
        cache->setDataDirty(true);
}

void Abstract3DRenderer::updateHorizontalAspectRatio(float ratio)
{
    m_graphHorizontalAspectRatio = ratio;
    foreach (SeriesRenderCache *cache, m_renderCacheList)
        cache->setDataDirty(true);
}

void Abstract3DRenderer::updatePolar(bool enable)
{
    m_polarGraph = enable;
    foreach (SeriesRenderCache *cache, m_renderCacheList)
        cache->setDataDirty(true);
}

void Abstract3DRenderer::updateRadialLabelOffset(float offset)
{
    m_radialLabelOffset = offset;
}

void Abstract3DRenderer::updateMargin(float margin)
{
    m_requestedMargin = margin;
}

void Abstract3DRenderer::updateOptimizationHint(QAbstract3DGraph::OptimizationHints hint)
{
    m_cachedOptimizationHint = hint;
    foreach (SeriesRenderCache *cache, m_renderCacheList)
        cache->setDataDirty(true);
}

void Abstract3DRenderer::handleResize()
{
    if (m_primarySubViewport.width() == 0 || m_primarySubViewport.height() == 0)
        return;

    // Recalculate zoom
    calculateZoomLevel();

    // Re-init selection buffer
    initSelectionBuffer();

    // Re-init depth buffer
    updateDepthBuffer();

    initCursorPositionBuffer();
}

void Abstract3DRenderer::calculateZoomLevel()
{
    // Calculate zoom level based on aspect ratio
    GLfloat div;
    GLfloat zoomAdjustment;
    div = qMin(m_primarySubViewport.width(), m_primarySubViewport.height());
    zoomAdjustment = defaultRatio
            * ((m_primarySubViewport.width() / div)
               / (m_primarySubViewport.height() / div));
    m_autoScaleAdjustment = qMin(zoomAdjustment, 1.0f); // clamp to 1.0f
}

void Abstract3DRenderer::updateAxisType(QAbstract3DAxis::AxisOrientation orientation,
                                        QAbstract3DAxis::AxisType type)
{
    axisCacheForOrientation(orientation).setType(type);
}

void Abstract3DRenderer::updateAxisTitle(QAbstract3DAxis::AxisOrientation orientation,
                                         const QString &title)
{
    axisCacheForOrientation(orientation).setTitle(title);
}

void Abstract3DRenderer::updateAxisLabels(QAbstract3DAxis::AxisOrientation orientation,
                                          const QStringList &labels)
{
    axisCacheForOrientation(orientation).setLabels(labels);
}

void Abstract3DRenderer::updateAxisRange(QAbstract3DAxis::AxisOrientation orientation,
                                         float min, float max)
{
    AxisRenderCache &cache = axisCacheForOrientation(orientation);
    cache.setMin(min);
    cache.setMax(max);

    foreach (SeriesRenderCache *cache, m_renderCacheList)
        cache->setDataDirty(true);
}

void Abstract3DRenderer::updateAxisSegmentCount(QAbstract3DAxis::AxisOrientation orientation,
                                                int count)
{
    AxisRenderCache &cache = axisCacheForOrientation(orientation);
    cache.setSegmentCount(count);
}

void Abstract3DRenderer::updateAxisSubSegmentCount(QAbstract3DAxis::AxisOrientation orientation,
                                                   int count)
{
    AxisRenderCache &cache = axisCacheForOrientation(orientation);
    cache.setSubSegmentCount(count);
}

void Abstract3DRenderer::updateAxisLabelFormat(QAbstract3DAxis::AxisOrientation orientation,
                                               const QString &format)
{
    axisCacheForOrientation(orientation).setLabelFormat(format);
}

void Abstract3DRenderer::updateAxisReversed(QAbstract3DAxis::AxisOrientation orientation,
                                            bool enable)
{
    axisCacheForOrientation(orientation).setReversed(enable);
    foreach (SeriesRenderCache *cache, m_renderCacheList)
        cache->setDataDirty(true);
}

void Abstract3DRenderer::updateAxisFormatter(QAbstract3DAxis::AxisOrientation orientation,
                                             QValue3DAxisFormatter *formatter)
{
    AxisRenderCache &cache = axisCacheForOrientation(orientation);
    if (cache.ctrlFormatter() != formatter) {
        delete cache.formatter();
        cache.setFormatter(formatter->createNewInstance());
        cache.setCtrlFormatter(formatter);
    }
    formatter->d_ptr->populateCopy(*(cache.formatter()));
    cache.markPositionsDirty();

    foreach (SeriesRenderCache *cache, m_renderCacheList)
        cache->setDataDirty(true);
}

void Abstract3DRenderer::updateAxisLabelAutoRotation(QAbstract3DAxis::AxisOrientation orientation,
                                                     float angle)
{
    AxisRenderCache &cache = axisCacheForOrientation(orientation);
    if (cache.labelAutoRotation() != angle)
        cache.setLabelAutoRotation(angle);
}

void Abstract3DRenderer::updateAxisTitleVisibility(QAbstract3DAxis::AxisOrientation orientation,
                                                   bool visible)
{
    AxisRenderCache &cache = axisCacheForOrientation(orientation);
    if (cache.isTitleVisible() != visible)
        cache.setTitleVisible(visible);
}

void Abstract3DRenderer::updateAxisTitleFixed(QAbstract3DAxis::AxisOrientation orientation,
                                              bool fixed)
{
    AxisRenderCache &cache = axisCacheForOrientation(orientation);
    if (cache.isTitleFixed() != fixed)
        cache.setTitleFixed(fixed);
}

void Abstract3DRenderer::modifiedSeriesList(const QVector<QAbstract3DSeries *> &seriesList)
{
    foreach (QAbstract3DSeries *series, seriesList) {
        SeriesRenderCache *cache = m_renderCacheList.value(series, 0);
        if (cache)
            cache->setDataDirty(true);
    }
}

void Abstract3DRenderer::fixMeshFileName(QString &fileName, QAbstract3DSeries::Mesh mesh)
{
    // Default implementation does nothing.
    Q_UNUSED(fileName)
    Q_UNUSED(mesh)
}

void Abstract3DRenderer::updateSeries(const QList<QAbstract3DSeries *> &seriesList)
{
    foreach (SeriesRenderCache *cache, m_renderCacheList)
        cache->setValid(false);

    m_visibleSeriesCount = 0;
    int seriesCount = seriesList.size();
    for (int i = 0; i < seriesCount; i++) {
        QAbstract3DSeries *series = seriesList.at(i);
        SeriesRenderCache *cache = m_renderCacheList.value(series);
        bool newSeries = false;
        if (!cache) {
            cache = createNewCache(series);
            m_renderCacheList[series] = cache;
            newSeries = true;
        }
        cache->setValid(true);
        cache->populate(newSeries);
        if (cache->isVisible())
            m_visibleSeriesCount++;
    }

    // Remove non-valid objects from the cache list
    foreach (SeriesRenderCache *cache, m_renderCacheList) {
        if (!cache->isValid())
            cleanCache(cache);
    }
}

void Abstract3DRenderer::updateCustomData(const QList<QCustom3DItem *> &customItems)
{
    if (customItems.isEmpty() && m_customRenderCache.isEmpty())
        return;

    foreach (CustomRenderItem *item, m_customRenderCache)
        item->setValid(false);

    int itemCount = customItems.size();
    // Check custom item list for items that are not yet in render item cache
    for (int i = 0; i < itemCount; i++) {
        QCustom3DItem *item = customItems.at(i);
        CustomRenderItem *renderItem = m_customRenderCache.value(item);
        if (!renderItem)
            renderItem = addCustomItem(item);
        renderItem->setValid(true);
        renderItem->setIndex(i); // always update index, as it must match the custom item index
    }

    // Check render item cache and remove items that are not in customItems list anymore
    foreach (CustomRenderItem *renderItem, m_customRenderCache) {
        if (!renderItem->isValid()) {
            m_customRenderCache.remove(renderItem->itemPointer());
            GLuint texture = renderItem->texture();
            m_textureHelper->deleteTexture(&texture);
            delete renderItem;
        }
    }
}

void Abstract3DRenderer::updateCustomItems()
{
    // Check all items
    foreach (CustomRenderItem *item, m_customRenderCache)
        updateCustomItem(item);
}

SeriesRenderCache *Abstract3DRenderer::createNewCache(QAbstract3DSeries *series)
{
    return new SeriesRenderCache(series, this);
}

void Abstract3DRenderer::cleanCache(SeriesRenderCache *cache)
{
    m_renderCacheList.remove(cache->series());
    cache->cleanup(m_textureHelper);
    delete cache;
}

AxisRenderCache &Abstract3DRenderer::axisCacheForOrientation(
        QAbstract3DAxis::AxisOrientation orientation)
{
    switch (orientation) {
    case QAbstract3DAxis::AxisOrientationX:
        return m_axisCacheX;
    case QAbstract3DAxis::AxisOrientationY:
        return m_axisCacheY;
    case QAbstract3DAxis::AxisOrientationZ:
        return m_axisCacheZ;
    default:
        qFatal("Abstract3DRenderer::axisCacheForOrientation");
        return m_axisCacheX;
    }
}

void Abstract3DRenderer::lowerShadowQuality()
{
    QAbstract3DGraph::ShadowQuality newQuality = QAbstract3DGraph::ShadowQualityNone;

    switch (m_cachedShadowQuality) {
    case QAbstract3DGraph::ShadowQualityHigh:
        qWarning("Creating high quality shadows failed. Changing to medium quality.");
        newQuality = QAbstract3DGraph::ShadowQualityMedium;
        break;
    case QAbstract3DGraph::ShadowQualityMedium:
        qWarning("Creating medium quality shadows failed. Changing to low quality.");
        newQuality = QAbstract3DGraph::ShadowQualityLow;
        break;
    case QAbstract3DGraph::ShadowQualityLow:
        qWarning("Creating low quality shadows failed. Switching shadows off.");
        newQuality = QAbstract3DGraph::ShadowQualityNone;
        break;
    case QAbstract3DGraph::ShadowQualitySoftHigh:
        qWarning("Creating soft high quality shadows failed. Changing to soft medium quality.");
        newQuality = QAbstract3DGraph::ShadowQualitySoftMedium;
        break;
    case QAbstract3DGraph::ShadowQualitySoftMedium:
        qWarning("Creating soft medium quality shadows failed. Changing to soft low quality.");
        newQuality = QAbstract3DGraph::ShadowQualitySoftLow;
        break;
    case QAbstract3DGraph::ShadowQualitySoftLow:
        qWarning("Creating soft low quality shadows failed. Switching shadows off.");
        newQuality = QAbstract3DGraph::ShadowQualityNone;
        break;
    default:
        // You'll never get here
        break;
    }

    emit requestShadowQuality(newQuality);
    updateShadowQuality(newQuality);
}

void Abstract3DRenderer::drawAxisTitleY(const QVector3D &sideLabelRotation,
                                        const QVector3D &backLabelRotation,
                                        const QVector3D &sideLabelTrans,
                                        const QVector3D &backLabelTrans,
                                        const QQuaternion &totalSideRotation,
                                        const QQuaternion &totalBackRotation,
                                        AbstractRenderItem &dummyItem,
                                        const Q3DCamera *activeCamera,
                                        float labelsMaxWidth,
                                        const QMatrix4x4 &viewMatrix,
                                        const QMatrix4x4 &projectionMatrix,
                                        ShaderHelper *shader)
{
    float scaleFactor = m_drawer->scaledFontSize() / m_axisCacheY.titleItem().size().height();
    float titleOffset = 2.0f * (labelMargin + (labelsMaxWidth * scaleFactor));
    float yRotation;
    QVector3D titleTrans;
    QQuaternion totalRotation;
    if (m_xFlipped == m_zFlipped) {
        yRotation = backLabelRotation.y();
        titleTrans = backLabelTrans;
        totalRotation = totalBackRotation;
    } else {
        yRotation = sideLabelRotation.y();
        titleTrans = sideLabelTrans;
        totalRotation = totalSideRotation;
    }

    QQuaternion offsetRotator = QQuaternion::fromAxisAndAngle(0.0f, 1.0f, 0.0f, yRotation);
    QVector3D titleOffsetVector =
            offsetRotator.rotatedVector(QVector3D(-titleOffset, 0.0f, 0.0f));

    QQuaternion titleRotation;
    if (m_axisCacheY.isTitleFixed()) {
        titleRotation = QQuaternion::fromAxisAndAngle(0.0f, 1.0f, 0.0f, yRotation)
                * m_zRightAngleRotation;
    } else {
        titleRotation = totalRotation * m_zRightAngleRotation;
    }
    dummyItem.setTranslation(titleTrans + titleOffsetVector);

    m_drawer->drawLabel(dummyItem, m_axisCacheY.titleItem(), viewMatrix,
                        projectionMatrix, zeroVector, titleRotation, 0,
                        m_cachedSelectionMode, shader, m_labelObj, activeCamera,
                        true, true, Drawer::LabelMid, Qt::AlignBottom);
}

void Abstract3DRenderer::drawAxisTitleX(const QVector3D &labelRotation,
                                        const QVector3D &labelTrans,
                                        const QQuaternion &totalRotation,
                                        AbstractRenderItem &dummyItem,
                                        const Q3DCamera *activeCamera,
                                        float labelsMaxWidth,
                                        const QMatrix4x4 &viewMatrix,
                                        const QMatrix4x4 &projectionMatrix,
                                        ShaderHelper *shader,
                                        bool radial)
{
    float scaleFactor = m_drawer->scaledFontSize() / m_axisCacheX.titleItem().size().height();
    float titleOffset;
    if (radial)
        titleOffset = -2.0f * (labelMargin + m_drawer->scaledFontSize());
    else
        titleOffset = 2.0f * (labelMargin + (labelsMaxWidth * scaleFactor));
    float zRotation = 0.0f;
    float yRotation = 0.0f;
    float xRotation = -90.0f + labelRotation.z();
    float offsetRotation = labelRotation.z();
    float extraRotation = -90.0f;
    Qt::AlignmentFlag alignment = Qt::AlignTop;
    if (m_yFlippedForGrid) {
        alignment = Qt::AlignBottom;
        zRotation = 180.0f;
        if (m_zFlipped) {
            titleOffset = -titleOffset;
            if (m_xFlipped) {
                offsetRotation = -offsetRotation;
                extraRotation = -extraRotation;
            } else {
                xRotation = -90.0f - labelRotation.z();
            }
        } else {
            yRotation = 180.0f;
            if (m_xFlipped) {
                offsetRotation = -offsetRotation;
                xRotation = -90.0f - labelRotation.z();
            } else {
                extraRotation = -extraRotation;
            }
        }
    } else {
        if (m_zFlipped) {
            titleOffset = -titleOffset;
            if (m_xFlipped) {
                yRotation = 180.0f;
                offsetRotation = -offsetRotation;
            } else {
                yRotation = 180.0f;
                xRotation = -90.0f - labelRotation.z();
                extraRotation = -extraRotation;
            }
        } else {
            if (m_xFlipped) {
                offsetRotation = -offsetRotation;
                xRotation = -90.0f - labelRotation.z();
                extraRotation = -extraRotation;
            }
        }
    }

    if (radial) {
        if (m_zFlipped) {
            titleOffset = -titleOffset;
        } else {
            if (m_yFlippedForGrid)
                alignment = Qt::AlignTop;
            else
                alignment = Qt::AlignBottom;
        }
    }

    if (offsetRotation == 180.0f || offsetRotation == -180.0f)
        offsetRotation = 0.0f;
    QQuaternion offsetRotator = QQuaternion::fromAxisAndAngle(1.0f, 0.0f, 0.0f, offsetRotation);
    QVector3D titleOffsetVector =
            offsetRotator.rotatedVector(QVector3D(0.0f, 0.0f, titleOffset));

    QQuaternion titleRotation;
    if (m_axisCacheX.isTitleFixed()) {
        titleRotation = QQuaternion::fromAxisAndAngle(0.0f, 0.0f, 1.0f, zRotation)
                * QQuaternion::fromAxisAndAngle(0.0f, 1.0f, 0.0f, yRotation)
                * QQuaternion::fromAxisAndAngle(1.0f, 0.0f, 0.0f, xRotation);
    } else {
        titleRotation = totalRotation
                * QQuaternion::fromAxisAndAngle(0.0f, 0.0f, 1.0f, extraRotation);
    }
    dummyItem.setTranslation(labelTrans + titleOffsetVector);

    m_drawer->drawLabel(dummyItem, m_axisCacheX.titleItem(), viewMatrix,
                        projectionMatrix, zeroVector, titleRotation, 0,
                        m_cachedSelectionMode, shader, m_labelObj, activeCamera,
                        true, true, Drawer::LabelMid, alignment);
}

void Abstract3DRenderer::drawAxisTitleZ(const QVector3D &labelRotation,
                                        const QVector3D &labelTrans,
                                        const QQuaternion &totalRotation,
                                        AbstractRenderItem &dummyItem,
                                        const Q3DCamera *activeCamera,
                                        float labelsMaxWidth,
                                        const QMatrix4x4 &viewMatrix,
                                        const QMatrix4x4 &projectionMatrix,
                                        ShaderHelper *shader)
{
    float scaleFactor = m_drawer->scaledFontSize() / m_axisCacheZ.titleItem().size().height();
    float titleOffset = 2.0f * (labelMargin + (labelsMaxWidth * scaleFactor));
    float zRotation = labelRotation.z();
    float yRotation = -90.0f;
    float xRotation = -90.0f;
    float extraRotation = 90.0f;
    Qt::AlignmentFlag alignment = Qt::AlignTop;
    if (m_yFlippedForGrid) {
        alignment = Qt::AlignBottom;
        xRotation = -xRotation;
        if (m_zFlipped) {
            if (m_xFlipped) {
                titleOffset = -titleOffset;
                zRotation = -zRotation;
                extraRotation = -extraRotation;
            } else {
                zRotation = -zRotation;
                yRotation = -yRotation;
            }
        } else {
            if (m_xFlipped) {
                titleOffset = -titleOffset;
            } else {
                extraRotation = -extraRotation;
                yRotation = -yRotation;
            }
        }
    } else {
        if (m_zFlipped) {
            zRotation = -zRotation;
            if (m_xFlipped) {
                titleOffset = -titleOffset;
            } else {
                extraRotation = -extraRotation;
                yRotation = -yRotation;
            }
        } else {
            if (m_xFlipped) {
                titleOffset = -titleOffset;
                extraRotation = -extraRotation;
            } else {
                yRotation = -yRotation;
            }
        }
    }

    float offsetRotation = zRotation;
    if (offsetRotation == 180.0f || offsetRotation == -180.0f)
        offsetRotation = 0.0f;
    QQuaternion offsetRotator = QQuaternion::fromAxisAndAngle(0.0f, 0.0f, 1.0f, offsetRotation);
    QVector3D titleOffsetVector =
            offsetRotator.rotatedVector(QVector3D(titleOffset, 0.0f, 0.0f));

    QQuaternion titleRotation;
    if (m_axisCacheZ.isTitleFixed()) {
        titleRotation = QQuaternion::fromAxisAndAngle(0.0f, 0.0f, 1.0f, zRotation)
                * QQuaternion::fromAxisAndAngle(0.0f, 1.0f, 0.0f, yRotation)
                * QQuaternion::fromAxisAndAngle(1.0f, 0.0f, 0.0f, xRotation);
    } else {
        titleRotation = totalRotation
                * QQuaternion::fromAxisAndAngle(0.0f, 0.0f, 1.0f, extraRotation);
    }
    dummyItem.setTranslation(labelTrans + titleOffsetVector);

    m_drawer->drawLabel(dummyItem, m_axisCacheZ.titleItem(), viewMatrix,
                        projectionMatrix, zeroVector, titleRotation, 0,
                        m_cachedSelectionMode, shader, m_labelObj, activeCamera,
                        true, true, Drawer::LabelMid, alignment);
}

void Abstract3DRenderer::loadGridLineMesh()
{
    ObjectHelper::resetObjectHelper(this, m_gridLineObj,
                                    QStringLiteral(":/defaultMeshes/plane"));
}

void Abstract3DRenderer::loadLabelMesh()
{
    ObjectHelper::resetObjectHelper(this, m_labelObj,
                                    QStringLiteral(":/defaultMeshes/plane"));
}

void Abstract3DRenderer::loadPositionMapperMesh()
{
    ObjectHelper::resetObjectHelper(this, m_positionMapperObj,
                                    QStringLiteral(":/defaultMeshes/barFull"));
}

void Abstract3DRenderer::generateBaseColorTexture(const QColor &color, GLuint *texture)
{
    m_textureHelper->deleteTexture(texture);
    *texture = m_textureHelper->createUniformTexture(color);
}

void Abstract3DRenderer::fixGradientAndGenerateTexture(QLinearGradient *gradient,
                                                       GLuint *gradientTexture)
{
    // Readjust start/stop to match gradient texture size
    gradient->setStart(qreal(gradientTextureWidth), qreal(gradientTextureHeight));
    gradient->setFinalStop(0.0, 0.0);

    m_textureHelper->deleteTexture(gradientTexture);

    *gradientTexture = m_textureHelper->createGradientTexture(*gradient);
}

LabelItem &Abstract3DRenderer::selectionLabelItem()
{
    if (!m_selectionLabelItem)
        m_selectionLabelItem = new LabelItem;
    return *m_selectionLabelItem;
}

void Abstract3DRenderer::setSelectionLabel(const QString &label)
{
    if (m_selectionLabelItem)
        m_selectionLabelItem->clear();
    m_selectionLabel = label;
}

QString &Abstract3DRenderer::selectionLabel()
{
    return m_selectionLabel;
}

QVector4D Abstract3DRenderer::indexToSelectionColor(GLint index)
{
    GLubyte idxRed = index & 0xff;
    GLubyte idxGreen = (index & 0xff00) >> 8;
    GLubyte idxBlue = (index & 0xff0000) >> 16;

    return QVector4D(idxRed, idxGreen, idxBlue, 0);
}

CustomRenderItem *Abstract3DRenderer::addCustomItem(QCustom3DItem *item)
{
    CustomRenderItem *newItem = new CustomRenderItem();
    newItem->setRenderer(this);
    newItem->setItemPointer(item); // Store pointer for render item updates
    newItem->setMesh(item->meshFile());
    newItem->setOrigPosition(item->position());
    newItem->setOrigScaling(item->scaling());
    newItem->setScalingAbsolute(item->isScalingAbsolute());
    newItem->setPositionAbsolute(item->isPositionAbsolute());
    QImage textureImage = item->d_ptr->textureImage();
    bool facingCamera = false;
    GLuint texture = 0;
    if (item->d_ptr->m_isLabelItem) {
        QCustom3DLabel *labelItem = static_cast<QCustom3DLabel *>(item);
        newItem->setLabelItem(true);
        float pointSize = labelItem->font().pointSizeF();
        // Check do we have custom visuals or need to use theme
        if (!labelItem->dptr()->m_customVisuals) {
            // Recreate texture using theme
            labelItem->dptr()->createTextureImage(m_cachedTheme->labelBackgroundColor(),
                                                  m_cachedTheme->labelTextColor(),
                                                  m_cachedTheme->isLabelBackgroundEnabled(),
                                                  m_cachedTheme->isLabelBorderEnabled());
            pointSize = m_cachedTheme->font().pointSizeF();
            textureImage = item->d_ptr->textureImage();
        }
        // Calculate scaling based on text (texture size), font size and asked scaling
        float scaledFontSize = (0.05f + pointSize / 500.0f) / float(textureImage.height());
        QVector3D scaling = newItem->origScaling();
        scaling.setX(scaling.x() * textureImage.width() * scaledFontSize);
        scaling.setY(scaling.y() * textureImage.height() * scaledFontSize);
        newItem->setOrigScaling(scaling);
        // Check if facing camera
        facingCamera = labelItem->isFacingCamera();
    } else if (item->d_ptr->m_isVolumeItem && !m_isOpenGLES) {
        QCustom3DVolume *volumeItem = static_cast<QCustom3DVolume *>(item);
        newItem->setTextureWidth(volumeItem->textureWidth());
        newItem->setTextureHeight(volumeItem->textureHeight());
        newItem->setTextureDepth(volumeItem->textureDepth());
        if (volumeItem->textureFormat() == QImage::Format_Indexed8)
            newItem->setColorTable(volumeItem->colorTable());
        newItem->setTextureFormat(volumeItem->textureFormat());
        newItem->setVolume(true);
        newItem->setBlendNeeded(true);
        texture = m_textureHelper->create3DTexture(volumeItem->textureData(),
                                                   volumeItem->textureWidth(),
                                                   volumeItem->textureHeight(),
                                                   volumeItem->textureDepth(),
                                                   volumeItem->textureFormat());
        newItem->setSliceIndexX(volumeItem->sliceIndexX());
        newItem->setSliceIndexY(volumeItem->sliceIndexY());
        newItem->setSliceIndexZ(volumeItem->sliceIndexZ());
        newItem->setAlphaMultiplier(volumeItem->alphaMultiplier());
        newItem->setPreserveOpacity(volumeItem->preserveOpacity());
        newItem->setUseHighDefShader(volumeItem->useHighDefShader());

        newItem->setDrawSlices(volumeItem->drawSlices());
        newItem->setDrawSliceFrames(volumeItem->drawSliceFrames());
        newItem->setSliceFrameColor(volumeItem->sliceFrameColor());
        newItem->setSliceFrameWidths(volumeItem->sliceFrameWidths());
        newItem->setSliceFrameGaps(volumeItem->sliceFrameGaps());
        newItem->setSliceFrameThicknesses(volumeItem->sliceFrameThicknesses());
    }
    recalculateCustomItemScalingAndPos(newItem);
    newItem->setRotation(item->rotation());

    // In OpenGL ES we simply draw volumes as regular custom item placeholders.
    if (!item->d_ptr->m_isVolumeItem || m_isOpenGLES)
    {
        newItem->setBlendNeeded(textureImage.hasAlphaChannel());
        texture = m_textureHelper->create2DTexture(textureImage, true, true, true);
    }
    newItem->setTexture(texture);
    item->d_ptr->clearTextureImage();
    newItem->setVisible(item->isVisible());
    newItem->setShadowCasting(item->isShadowCasting());
    newItem->setFacingCamera(facingCamera);
    m_customRenderCache.insert(item, newItem);
    return newItem;
}

void Abstract3DRenderer::recalculateCustomItemScalingAndPos(CustomRenderItem *item)
{
    if (!m_polarGraph && !item->isLabel() && !item->isScalingAbsolute()
            && !item->isPositionAbsolute()) {
        QVector3D scale = item->origScaling() / 2.0f;
        QVector3D pos = item->origPosition();
        QVector3D minBounds(pos.x() - scale.x(),
                            pos.y() - scale.y(),
                            pos.z() + scale.z());
        QVector3D maxBounds(pos.x() + scale.x(),
                            pos.y() + scale.y(),
                            pos.z() - scale.z());
        QVector3D minCorner = convertPositionToTranslation(minBounds, false);
        QVector3D maxCorner = convertPositionToTranslation(maxBounds, false);
        scale = QVector3D(qAbs(maxCorner.x() - minCorner.x()),
                          qAbs(maxCorner.y() - minCorner.y()),
                          qAbs(maxCorner.z() - minCorner.z())) / 2.0f;
        if (item->isVolume()) {
            // Only volume items need to scale and reposition according to bounds
            QVector3D minBoundsNormal = minCorner;
            QVector3D maxBoundsNormal = maxCorner;
            // getVisibleItemBounds returns bounds normalized for fragment shader [-1,1]
            // Y and Z are also flipped.
            getVisibleItemBounds(minBoundsNormal, maxBoundsNormal);
            item->setMinBounds(minBoundsNormal);
            item->setMaxBounds(maxBoundsNormal);
            // For scaling calculations, we want [0,1] normalized values
            minBoundsNormal = item->minBoundsNormal();
            maxBoundsNormal = item->maxBoundsNormal();

            // Rescale and reposition the item so that it doesn't go over the edges
            QVector3D adjScaling =
                    QVector3D(scale.x() * (maxBoundsNormal.x() - minBoundsNormal.x()),
                              scale.y() * (maxBoundsNormal.y() - minBoundsNormal.y()),
                              scale.z() * (maxBoundsNormal.z() - minBoundsNormal.z()));

            item->setScaling(adjScaling);

            QVector3D adjPos = item->origPosition();
            QVector3D dataExtents = QVector3D(maxBounds.x() - minBounds.x(),
                                              maxBounds.y() - minBounds.y(),
                                              maxBounds.z() - minBounds.z()) / 2.0f;
            adjPos.setX(adjPos.x() + (dataExtents.x() * minBoundsNormal.x())
                        - (dataExtents.x() * (1.0f - maxBoundsNormal.x())));
            adjPos.setY(adjPos.y() + (dataExtents.y() * minBoundsNormal.y())
                        - (dataExtents.y() * (1.0f - maxBoundsNormal.y())));
            adjPos.setZ(adjPos.z() + (dataExtents.z() * minBoundsNormal.z())
                        - (dataExtents.z() * (1.0f - maxBoundsNormal.z())));
            item->setPosition(adjPos);
        } else {
            // Only scale for non-volume items, and do not readjust position
            item->setScaling(scale);
            item->setPosition(item->origPosition());
        }
    } else {
        item->setScaling(item->origScaling());
        item->setPosition(item->origPosition());
        if (item->isVolume()) {
            // Y and Z need to be flipped as shader flips those axes
            item->setMinBounds(QVector3D(-1.0f, 1.0f, 1.0f));
            item->setMaxBounds(QVector3D(1.0f, -1.0f, -1.0f));
        }
    }
    QVector3D translation = convertPositionToTranslation(item->position(),
                                                         item->isPositionAbsolute());
    item->setTranslation(translation);
}

void Abstract3DRenderer::updateCustomItem(CustomRenderItem *renderItem)
{
    QCustom3DItem *item = renderItem->itemPointer();
    if (item->d_ptr->m_dirtyBits.meshDirty) {
        renderItem->setMesh(item->meshFile());
        item->d_ptr->m_dirtyBits.meshDirty = false;
    }
    if (item->d_ptr->m_dirtyBits.positionDirty) {
        renderItem->setOrigPosition(item->position());
        renderItem->setPositionAbsolute(item->isPositionAbsolute());
        if (!item->d_ptr->m_dirtyBits.scalingDirty)
            recalculateCustomItemScalingAndPos(renderItem);
        item->d_ptr->m_dirtyBits.positionDirty = false;
    }
    if (item->d_ptr->m_dirtyBits.scalingDirty) {
        QVector3D scaling = item->scaling();
        renderItem->setOrigScaling(scaling);
        renderItem->setScalingAbsolute(item->isScalingAbsolute());
        // In case we have label item, we need to recreate texture for scaling adjustment
        if (item->d_ptr->m_isLabelItem) {
            QCustom3DLabel *labelItem = static_cast<QCustom3DLabel *>(item);
            float pointSize = labelItem->font().pointSizeF();
            // Check do we have custom visuals or need to use theme
            if (labelItem->dptr()->m_customVisuals) {
                // Recreate texture
                labelItem->dptr()->createTextureImage();
            } else {
                // Recreate texture using theme
                labelItem->dptr()->createTextureImage(m_cachedTheme->labelBackgroundColor(),
                                                      m_cachedTheme->labelTextColor(),
                                                      m_cachedTheme->isLabelBackgroundEnabled(),
                                                      m_cachedTheme->isLabelBorderEnabled());
                pointSize = m_cachedTheme->font().pointSizeF();
            }
            QImage textureImage = item->d_ptr->textureImage();
            // Calculate scaling based on text (texture size), font size and asked scaling
            float scaledFontSize = (0.05f + pointSize / 500.0f) / float(textureImage.height());
            scaling.setX(scaling.x() * textureImage.width() * scaledFontSize);
            scaling.setY(scaling.y() * textureImage.height() * scaledFontSize);
            item->d_ptr->clearTextureImage();
            renderItem->setOrigScaling(scaling);
        }
        recalculateCustomItemScalingAndPos(renderItem);
        item->d_ptr->m_dirtyBits.scalingDirty = false;
    }
    if (item->d_ptr->m_dirtyBits.rotationDirty) {
        renderItem->setRotation(item->rotation());
        item->d_ptr->m_dirtyBits.rotationDirty = false;
    }
    if (item->d_ptr->m_dirtyBits.textureDirty) {
        QImage textureImage = item->d_ptr->textureImage();
        if (item->d_ptr->m_isLabelItem) {
            QCustom3DLabel *labelItem = static_cast<QCustom3DLabel *>(item);
            // Check do we have custom visuals or need to use theme
            if (!labelItem->dptr()->m_customVisuals) {
                // Recreate texture using theme
                labelItem->dptr()->createTextureImage(m_cachedTheme->labelBackgroundColor(),
                                                      m_cachedTheme->labelTextColor(),
                                                      m_cachedTheme->isLabelBackgroundEnabled(),
                                                      m_cachedTheme->isLabelBorderEnabled());
                textureImage = item->d_ptr->textureImage();
            }
        } else if (!item->d_ptr->m_isVolumeItem || m_isOpenGLES) {
            renderItem->setBlendNeeded(textureImage.hasAlphaChannel());
            GLuint oldTexture = renderItem->texture();
            m_textureHelper->deleteTexture(&oldTexture);
            GLuint texture = m_textureHelper->create2DTexture(textureImage, true, true, true);
            renderItem->setTexture(texture);
        }
        item->d_ptr->clearTextureImage();
        item->d_ptr->m_dirtyBits.textureDirty = false;
    }
    if (item->d_ptr->m_dirtyBits.visibleDirty) {
        renderItem->setVisible(item->isVisible());
        item->d_ptr->m_dirtyBits.visibleDirty = false;
    }
    if (item->d_ptr->m_dirtyBits.shadowCastingDirty) {
        renderItem->setShadowCasting(item->isShadowCasting());
        item->d_ptr->m_dirtyBits.shadowCastingDirty = false;
    }
    if (item->d_ptr->m_isLabelItem) {
        QCustom3DLabel *labelItem = static_cast<QCustom3DLabel *>(item);
        if (labelItem->dptr()->m_facingCameraDirty) {
            renderItem->setFacingCamera(labelItem->isFacingCamera());
            labelItem->dptr()->m_facingCameraDirty = false;
        }
    } else if (item->d_ptr->m_isVolumeItem && !m_isOpenGLES) {
        QCustom3DVolume *volumeItem = static_cast<QCustom3DVolume *>(item);
        if (volumeItem->dptr()->m_dirtyBitsVolume.colorTableDirty) {
            renderItem->setColorTable(volumeItem->colorTable());
            volumeItem->dptr()->m_dirtyBitsVolume.colorTableDirty = false;
        }
        if (volumeItem->dptr()->m_dirtyBitsVolume.textureDimensionsDirty
                || volumeItem->dptr()->m_dirtyBitsVolume.textureDataDirty
                || volumeItem->dptr()->m_dirtyBitsVolume.textureFormatDirty) {
            GLuint oldTexture = renderItem->texture();
            m_textureHelper->deleteTexture(&oldTexture);
            GLuint texture = m_textureHelper->create3DTexture(volumeItem->textureData(),
                                                              volumeItem->textureWidth(),
                                                              volumeItem->textureHeight(),
                                                              volumeItem->textureDepth(),
                                                              volumeItem->textureFormat());
            renderItem->setTexture(texture);
            renderItem->setTextureWidth(volumeItem->textureWidth());
            renderItem->setTextureHeight(volumeItem->textureHeight());
            renderItem->setTextureDepth(volumeItem->textureDepth());
            renderItem->setTextureFormat(volumeItem->textureFormat());
            volumeItem->dptr()->m_dirtyBitsVolume.textureDimensionsDirty = false;
            volumeItem->dptr()->m_dirtyBitsVolume.textureDataDirty = false;
            volumeItem->dptr()->m_dirtyBitsVolume.textureFormatDirty = false;
        }
        if (volumeItem->dptr()->m_dirtyBitsVolume.slicesDirty) {
            renderItem->setDrawSlices(volumeItem->drawSlices());
            renderItem->setDrawSliceFrames(volumeItem->drawSliceFrames());
            renderItem->setSliceFrameColor(volumeItem->sliceFrameColor());
            renderItem->setSliceFrameWidths(volumeItem->sliceFrameWidths());
            renderItem->setSliceFrameGaps(volumeItem->sliceFrameGaps());
            renderItem->setSliceFrameThicknesses(volumeItem->sliceFrameThicknesses());
            renderItem->setSliceIndexX(volumeItem->sliceIndexX());
            renderItem->setSliceIndexY(volumeItem->sliceIndexY());
            renderItem->setSliceIndexZ(volumeItem->sliceIndexZ());
            volumeItem->dptr()->m_dirtyBitsVolume.slicesDirty = false;
        }
        if (volumeItem->dptr()->m_dirtyBitsVolume.alphaDirty) {
            renderItem->setAlphaMultiplier(volumeItem->alphaMultiplier());
            renderItem->setPreserveOpacity(volumeItem->preserveOpacity());
            volumeItem->dptr()->m_dirtyBitsVolume.alphaDirty = false;
        }
        if (volumeItem->dptr()->m_dirtyBitsVolume.shaderDirty) {
            renderItem->setUseHighDefShader(volumeItem->useHighDefShader());
            volumeItem->dptr()->m_dirtyBitsVolume.shaderDirty = false;
        }
    }
}

void Abstract3DRenderer::updateCustomItemPositions()
{
    foreach (CustomRenderItem *renderItem, m_customRenderCache)
        recalculateCustomItemScalingAndPos(renderItem);
}

void Abstract3DRenderer::drawCustomItems(RenderingState state,
                                         ShaderHelper *regularShader,
                                         const QMatrix4x4 &viewMatrix,
                                         const QMatrix4x4 &projectionViewMatrix,
                                         const QMatrix4x4 &depthProjectionViewMatrix,
                                         GLuint depthTexture,
                                         GLfloat shadowQuality,
                                         GLfloat reflection)
{
    if (m_customRenderCache.isEmpty())
        return;

    ShaderHelper *shader = regularShader;
    shader->bind();

    if (RenderingNormal == state) {
        shader->setUniformValue(shader->lightP(), m_cachedScene->activeLight()->position());
        shader->setUniformValue(shader->ambientS(), m_cachedTheme->ambientLightStrength());
        shader->setUniformValue(shader->lightColor(),
                                Utils::vectorFromColor(m_cachedTheme->lightColor()));
        shader->setUniformValue(shader->view(), viewMatrix);
    }

    // Draw custom items - first regular and then volumes
    bool volumeDetected = false;
    int loopCount = 0;
    while (loopCount < 2) {
        foreach (CustomRenderItem *item, m_customRenderCache) {
            // Check that the render item is visible, and skip drawing if not
            // Also check if reflected item is on the "wrong" side, and skip drawing if it is
            if (!item->isVisible() || ((m_reflectionEnabled && reflection < 0.0f)
                    && (m_yFlipped == (item->translation().y() >= 0.0)))) {
                continue;
            }
            if (loopCount == 0) {
                if (item->isVolume()) {
                    volumeDetected = true;
                    continue;
                }
            } else {
                if (!item->isVolume())
                    continue;
            }

            // If the render item is in data coordinates and not within axis ranges, skip it
            if (!item->isPositionAbsolute()
                    && (item->position().x() < m_axisCacheX.min()
                        || item->position().x() > m_axisCacheX.max()
                        || item->position().z() < m_axisCacheZ.min()
                        || item->position().z() > m_axisCacheZ.max()
                        || item->position().y() < m_axisCacheY.min()
                        || item->position().y() > m_axisCacheY.max())) {
                continue;
            }

            QMatrix4x4 modelMatrix;
            QMatrix4x4 itModelMatrix;
            QMatrix4x4 MVPMatrix;

            QQuaternion rotation = item->rotation();
            // Check if the (label) item should be facing camera, and adjust rotation accordingly
            if (item->isFacingCamera()) {
                float camRotationX = m_cachedScene->activeCamera()->xRotation();
                float camRotationY = m_cachedScene->activeCamera()->yRotation();
                rotation = QQuaternion::fromAxisAndAngle(0.0f, 1.0f, 0.0f, -camRotationX)
                        * QQuaternion::fromAxisAndAngle(1.0f, 0.0f, 0.0f, -camRotationY);
            }

            if (m_reflectionEnabled) {
                if (reflection < 0.0f) {
                    if (item->itemPointer()->d_ptr->m_isLabelItem)
                        continue;
                    else
                        glCullFace(GL_FRONT);
                } else {
                    glCullFace(GL_BACK);
                }
                QVector3D trans = item->translation();
                trans.setY(reflection * trans.y());
                modelMatrix.translate(trans);
                if (reflection < 0.0f) {
                    QQuaternion mirror = QQuaternion(rotation.scalar(),
                                                     -rotation.x(), rotation.y(), -rotation.z());
                    modelMatrix.rotate(mirror);
                    itModelMatrix.rotate(mirror);
                } else {
                    modelMatrix.rotate(rotation);
                    itModelMatrix.rotate(rotation);
                }
                QVector3D scale = item->scaling();
                scale.setY(reflection * scale.y());
                modelMatrix.scale(scale);
            } else {
                modelMatrix.translate(item->translation());
                modelMatrix.rotate(rotation);
                modelMatrix.scale(item->scaling());
                itModelMatrix.rotate(rotation);
            }
            if (!item->isFacingCamera())
                itModelMatrix.scale(item->scaling());
            MVPMatrix = projectionViewMatrix * modelMatrix;

            if (RenderingNormal == state) {
                // Normal render
                ShaderHelper *prevShader = shader;
                if (item->isVolume() && !m_isOpenGLES) {
                    if (item->drawSlices() &&
                            (item->sliceIndexX() >= 0
                             || item->sliceIndexY() >= 0
                             || item->sliceIndexZ() >= 0)) {
                        shader = m_volumeTextureSliceShader;
                    } else if (item->useHighDefShader()) {
                        shader = m_volumeTextureShader;
                    } else {
                        shader = m_volumeTextureLowDefShader;
                    }
                } else if (item->isLabel()) {
                    shader = m_labelShader;
                } else {
                    shader = regularShader;
                }
                if (shader != prevShader)
                    shader->bind();
                shader->setUniformValue(shader->model(), modelMatrix);
                shader->setUniformValue(shader->MVP(), MVPMatrix);
                shader->setUniformValue(shader->nModel(), itModelMatrix.inverted().transposed());

                if (item->isBlendNeeded()) {
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    if (!item->isVolume() && !m_isOpenGLES)
                        glDisable(GL_CULL_FACE);
                } else {
                    glDisable(GL_BLEND);
                    glEnable(GL_CULL_FACE);
                }

                if (!m_isOpenGLES && m_cachedShadowQuality > QAbstract3DGraph::ShadowQualityNone
                        && !item->isVolume()) {
                    // Set shadow shader bindings
                    shader->setUniformValue(shader->shadowQ(), shadowQuality);
                    shader->setUniformValue(shader->depth(), depthProjectionViewMatrix * modelMatrix);
                    shader->setUniformValue(shader->lightS(), m_cachedTheme->lightStrength() / 10.0f);
                    m_drawer->drawObject(shader, item->mesh(), item->texture(), depthTexture);
                } else {
                    // Set shadowless shader bindings
                    if (item->isVolume() && !m_isOpenGLES) {
                        QVector3D cameraPos = m_cachedScene->activeCamera()->position();
                        cameraPos = MVPMatrix.inverted().map(cameraPos);
                        // Adjust camera position according to min/max bounds
                        cameraPos = -(cameraPos
                                      + ((oneVector - cameraPos) * item->minBoundsNormal())
                                      - ((oneVector + cameraPos) * (oneVector - item->maxBoundsNormal())));
                        shader->setUniformValue(shader->cameraPositionRelativeToModel(), cameraPos);
                        GLint color8Bit = (item->textureFormat() == QImage::Format_Indexed8) ? 1 : 0;
                        if (color8Bit) {
                            shader->setUniformValueArray(shader->colorIndex(),
                                                         item->colorTable().constData(), 256);
                        }
                        shader->setUniformValue(shader->color8Bit(), color8Bit);
                        shader->setUniformValue(shader->alphaMultiplier(), item->alphaMultiplier());
                        shader->setUniformValue(shader->preserveOpacity(),
                                                item->preserveOpacity() ? 1 : 0);

                        shader->setUniformValue(shader->minBounds(), item->minBounds());
                        shader->setUniformValue(shader->maxBounds(), item->maxBounds());

                        if (shader == m_volumeTextureSliceShader) {
                            shader->setUniformValue(shader->volumeSliceIndices(),
                                                    item->sliceFractions());
                        } else {
                            // Precalculate texture dimensions so we can optimize
                            // ray stepping to hit every texture layer.
                            QVector3D textureDimensions(1.0f / float(item->textureWidth()),
                                                        1.0f / float(item->textureHeight()),
                                                        1.0f / float(item->textureDepth()));

                            // Worst case scenario sample count
                            int sampleCount;
                            if (shader == m_volumeTextureLowDefShader) {
                                sampleCount = qMax(item->textureWidth(),
                                                   qMax(item->textureDepth(), item->textureHeight()));
                                // Further improve speed with big textures by simply dropping every
                                // other sample:
                                if (sampleCount > 256)
                                    sampleCount /= 2;
                            } else {
                                sampleCount = item->textureWidth() + item->textureHeight()
                                        + item->textureDepth();
                            }
                            shader->setUniformValue(shader->textureDimensions(), textureDimensions);
                            shader->setUniformValue(shader->sampleCount(), sampleCount);
                        }
                        if (item->drawSliceFrames()) {
                            // Set up the slice frame shader
                            glDisable(GL_CULL_FACE);
                            m_volumeSliceFrameShader->bind();
                            m_volumeSliceFrameShader->setUniformValue(
                                        m_volumeSliceFrameShader->color(), item->sliceFrameColor());

                            // Draw individual slice frames.
                            if (item->sliceIndexX() >= 0)
                                drawVolumeSliceFrame(item, Qt::XAxis, projectionViewMatrix);
                            if (item->sliceIndexY() >= 0)
                                drawVolumeSliceFrame(item, Qt::YAxis, projectionViewMatrix);
                            if (item->sliceIndexZ() >= 0)
                                drawVolumeSliceFrame(item, Qt::ZAxis, projectionViewMatrix);

                            glEnable(GL_CULL_FACE);
                            shader->bind();
                        }
                        m_drawer->drawObject(shader, item->mesh(), 0, 0, item->texture());
                    } else {
                        shader->setUniformValue(shader->lightS(), m_cachedTheme->lightStrength());
                        m_drawer->drawObject(shader, item->mesh(), item->texture());
                    }
                }
            } else if (RenderingSelection == state) {
                // Selection render
                shader->setUniformValue(shader->MVP(), MVPMatrix);
                QVector4D itemColor = indexToSelectionColor(item->index());
                itemColor.setW(customItemAlpha);
                itemColor /= 255.0f;
                shader->setUniformValue(shader->color(), itemColor);
                m_drawer->drawObject(shader, item->mesh());
            } else if (item->isShadowCasting()) {
                // Depth render
                shader->setUniformValue(shader->MVP(), depthProjectionViewMatrix * modelMatrix);
                m_drawer->drawObject(shader, item->mesh());
            }
        }
        loopCount++;
        if (!volumeDetected)
            loopCount++; // Skip second run if no volumes detected
    }

    if (RenderingNormal == state) {
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
    }
}

void Abstract3DRenderer::drawVolumeSliceFrame(const CustomRenderItem *item, Qt::Axis axis,
                                              const QMatrix4x4 &projectionViewMatrix)
{
    QVector2D frameWidth;
    QVector3D frameScaling;
    QVector3D translation = item->translation();
    QQuaternion rotation = item->rotation();
    float fracTrans;
    bool needRotate = !rotation.isIdentity();
    QMatrix4x4 rotationMatrix;
    if (needRotate)
        rotationMatrix.rotate(rotation);

    if (axis == Qt::XAxis) {
        fracTrans = item->sliceFractions().x();
        float range = item->maxBoundsNormal().x() - item->minBoundsNormal().x();
        float minMult = item->minBoundsNormal().x() / range;
        float maxMult = (1.0f - item->maxBoundsNormal().x()) / range;
        fracTrans = fracTrans - ((1.0f - fracTrans) * minMult) + ((1.0f + fracTrans) * maxMult);
        if (needRotate)
            translation += rotationMatrix.map(QVector3D(fracTrans * item->scaling().x(), 0.0f, 0.0f));
        else
            translation.setX(translation.x() + fracTrans * item->scaling().x());
        frameScaling = QVector3D(item->scaling().z()
                                + (item->scaling().z() * item->sliceFrameGaps().z())
                                + (item->scaling().z() * item->sliceFrameWidths().z()),
                                item->scaling().y()
                                + (item->scaling().y() * item->sliceFrameGaps().y())
                                + (item->scaling().y() * item->sliceFrameWidths().y()),
                                item->scaling().x() * item->sliceFrameThicknesses().x());
        frameWidth = QVector2D(item->scaling().z() * item->sliceFrameWidths().z(),
                               item->scaling().y() * item->sliceFrameWidths().y());
        rotation *= m_yRightAngleRotation;
    } else if (axis == Qt::YAxis) {
        fracTrans = item->sliceFractions().y();
        float range = item->maxBoundsNormal().y() - item->minBoundsNormal().y();
        // Y axis is logically flipped, so we need to swam min and max bounds
        float minMult = (1.0f - item->maxBoundsNormal().y()) / range;
        float maxMult = item->minBoundsNormal().y() / range;
        fracTrans = fracTrans - ((1.0f - fracTrans) * minMult) + ((1.0f + fracTrans) * maxMult);
        if (needRotate)
            translation -= rotationMatrix.map(QVector3D(0.0f, fracTrans * item->scaling().y(), 0.0f));
        else
            translation.setY(translation.y() - fracTrans * item->scaling().y());
        frameScaling = QVector3D(item->scaling().x()
                                + (item->scaling().x() * item->sliceFrameGaps().x())
                                + (item->scaling().x() * item->sliceFrameWidths().x()),
                                item->scaling().z()
                                + (item->scaling().z() * item->sliceFrameGaps().z())
                                + (item->scaling().z() * item->sliceFrameWidths().z()),
                                item->scaling().y() * item->sliceFrameThicknesses().y());
        frameWidth = QVector2D(item->scaling().x() * item->sliceFrameWidths().x(),
                               item->scaling().z() * item->sliceFrameWidths().z());
        rotation *= m_xRightAngleRotation;
    } else { // Z axis
        fracTrans = item->sliceFractions().z();
        float range = item->maxBoundsNormal().z() - item->minBoundsNormal().z();
        // Z axis is logically flipped, so we need to swam min and max bounds
        float minMult = (1.0f - item->maxBoundsNormal().z()) / range;
        float maxMult = item->minBoundsNormal().z() / range;
        fracTrans = fracTrans - ((1.0f - fracTrans) * minMult) + ((1.0f + fracTrans) * maxMult);
        if (needRotate)
            translation -= rotationMatrix.map(QVector3D(0.0f, 0.0f, fracTrans * item->scaling().z()));
        else
            translation.setZ(translation.z() - fracTrans * item->scaling().z());
        frameScaling = QVector3D(item->scaling().x()
                                + (item->scaling().x() * item->sliceFrameGaps().x())
                                + (item->scaling().x() * item->sliceFrameWidths().x()),
                                item->scaling().y()
                                + (item->scaling().y() * item->sliceFrameGaps().y())
                                + (item->scaling().y() * item->sliceFrameWidths().y()),
                                item->scaling().z() * item->sliceFrameThicknesses().z());
        frameWidth = QVector2D(item->scaling().x() * item->sliceFrameWidths().x(),
                               item->scaling().y() * item->sliceFrameWidths().y());
    }

    // If the slice is outside the shown area, don't show the frame
    if (fracTrans < -1.0 || fracTrans > 1.0)
        return;

    // Shader needs the width of clear space in the middle.
    frameWidth.setX(1.0f - (frameWidth.x() / frameScaling.x()));
    frameWidth.setY(1.0f - (frameWidth.y() / frameScaling.y()));

    QMatrix4x4 modelMatrix;
    QMatrix4x4 mvpMatrix;

    modelMatrix.translate(translation);
    modelMatrix.rotate(rotation);
    modelMatrix.scale(frameScaling);
    mvpMatrix = projectionViewMatrix * modelMatrix;
    m_volumeSliceFrameShader->setUniformValue(m_volumeSliceFrameShader->MVP(), mvpMatrix);
    m_volumeSliceFrameShader->setUniformValue(m_volumeSliceFrameShader->sliceFrameWidth(),
                                              frameWidth);

    m_drawer->drawObject(m_volumeSliceFrameShader, item->mesh());

}

void Abstract3DRenderer::queriedGraphPosition(const QMatrix4x4 &projectionViewMatrix,
                                              const QVector3D &scaling,
                                              GLuint defaultFboHandle)
{
    m_cursorPositionShader->bind();

    // Set up mapper framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, m_cursorPositionFrameBuffer);
    glViewport(0, 0,
               m_primarySubViewport.width(),
               m_primarySubViewport.height());
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DITHER); // Dither may affect colors if enabled
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

    // Draw a cube scaled to the graph dimensions
    QMatrix4x4 modelMatrix;
    QMatrix4x4 MVPMatrix;

    modelMatrix.scale(scaling);

    MVPMatrix = projectionViewMatrix * modelMatrix;
    m_cursorPositionShader->setUniformValue(m_cursorPositionShader->MVP(), MVPMatrix);
    m_drawer->drawObject(m_cursorPositionShader, m_positionMapperObj);

    QVector4D dataColor = Utils::getSelection(m_graphPositionQuery,
                                              m_primarySubViewport.height());
    if (dataColor.w() > 0.0f) {
        // If position is outside the graph, set the position well outside the graph boundaries
        dataColor = QVector4D(-10000.0f, -10000.0f, -10000.0f, 0.0f);
    } else {
        // Normalize to range [0.0, 1.0]
        dataColor /= 255.0f;
    }

    // Restore state
    glEnable(GL_DITHER);
    glCullFace(GL_BACK);

    // Note: Zeroing the frame buffer before resetting it is a workaround for flickering that occurs
    // during zoom in some environments.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, defaultFboHandle);
    glViewport(m_primarySubViewport.x(),
               m_primarySubViewport.y(),
               m_primarySubViewport.width(),
               m_primarySubViewport.height());

    QVector3D normalizedValues = dataColor.toVector3D() * 2.0f;
    normalizedValues -= oneVector;
    m_queriedGraphPosition = QVector3D(normalizedValues.x(),
                                       normalizedValues.y(),
                                       normalizedValues.z());
    m_graphPositionQueryResolved = true;
    m_graphPositionQueryPending = false;
}

void Abstract3DRenderer::fixContextBeforeDelete()
{
    // Only need to fix context if the current context is null.
    // Otherwise we expect it to be our shared context, so we can use it for cleanup.
    if (!QOpenGLContext::currentContext() && !m_context.isNull()
            && QThread::currentThread() == this->thread()) {
        m_dummySurfaceAtDelete = new QOffscreenSurface();
        m_dummySurfaceAtDelete->setFormat(m_context->format());
        m_dummySurfaceAtDelete->create();

        m_context->makeCurrent(m_dummySurfaceAtDelete);
    }
}

void Abstract3DRenderer::restoreContextAfterDelete()
{
    if (m_dummySurfaceAtDelete)
        m_context->doneCurrent();

    delete m_dummySurfaceAtDelete;
    m_dummySurfaceAtDelete = 0;
}

void Abstract3DRenderer::calculatePolarXZ(const QVector3D &dataPos, float &x, float &z) const
{
    // x is angular, z is radial
    qreal angle = m_axisCacheX.formatter()->positionAt(dataPos.x()) * doublePi;
    qreal radius = m_axisCacheZ.formatter()->positionAt(dataPos.z());

    // Convert angle & radius to X and Z coords
    x = float(radius * qSin(angle)) * m_polarRadius;
    z = -float(radius * qCos(angle)) * m_polarRadius;
}

void Abstract3DRenderer::drawRadialGrid(ShaderHelper *shader, float yFloorLinePos,
                                        const QMatrix4x4 &projectionViewMatrix,
                                        const QMatrix4x4 &depthMatrix)
{
    static QVector<QQuaternion> lineRotations;
    if (!lineRotations.size()) {
        lineRotations.resize(polarGridRoundness);
        for (int j = 0; j < polarGridRoundness; j++) {
            lineRotations[j] = QQuaternion::fromAxisAndAngle(0.0f, 1.0f, 0.0f,
                                                             polarGridAngleDegrees * float(j));
        }
    }
    int gridLineCount = m_axisCacheZ.gridLineCount();
    const QVector<float> &gridPositions = m_axisCacheZ.formatter()->gridPositions();
    const QVector<float> &subGridPositions = m_axisCacheZ.formatter()->subGridPositions();
    int mainSize = gridPositions.size();
    QVector3D translateVector(0.0f, yFloorLinePos, 0.0f);
    QQuaternion finalRotation = m_xRightAngleRotationNeg;
    if (m_yFlippedForGrid)
        finalRotation *= m_xFlipRotation;

    for (int i = 0; i < gridLineCount; i++) {
        float gridPosition = (i >= mainSize)
                ? subGridPositions.at(i - mainSize) : gridPositions.at(i);
        float radiusFraction = m_polarRadius * gridPosition;
        QVector3D gridLineScaler(radiusFraction * float(qSin(polarGridHalfAngle)),
                                 gridLineWidth, gridLineWidth);
        translateVector.setZ(gridPosition * m_polarRadius);
        for (int j = 0; j < polarGridRoundness; j++) {
            QMatrix4x4 modelMatrix;
            QMatrix4x4 itModelMatrix;
            modelMatrix.rotate(lineRotations.at(j));
            itModelMatrix.rotate(lineRotations.at(j));
            modelMatrix.translate(translateVector);
            modelMatrix.scale(gridLineScaler);
            itModelMatrix.scale(gridLineScaler);
            modelMatrix.rotate(finalRotation);
            itModelMatrix.rotate(finalRotation);
            QMatrix4x4 MVPMatrix = projectionViewMatrix * modelMatrix;

            shader->setUniformValue(shader->model(), modelMatrix);
            shader->setUniformValue(shader->nModel(), itModelMatrix.inverted().transposed());
            shader->setUniformValue(shader->MVP(), MVPMatrix);
            if (!m_isOpenGLES) {
                if (m_cachedShadowQuality > QAbstract3DGraph::ShadowQualityNone) {
                    // Set shadow shader bindings
                    QMatrix4x4 depthMVPMatrix = depthMatrix * modelMatrix;
                    shader->setUniformValue(shader->depth(), depthMVPMatrix);
                    // Draw the object
                    m_drawer->drawObject(shader, m_gridLineObj, 0, m_depthTexture);
                } else {
                    // Draw the object
                    m_drawer->drawObject(shader, m_gridLineObj);
                }
            } else {
                m_drawer->drawLine(shader);
            }
        }
    }
}

void Abstract3DRenderer::drawAngularGrid(ShaderHelper *shader, float yFloorLinePos,
                                         const QMatrix4x4 &projectionViewMatrix,
                                         const QMatrix4x4 &depthMatrix)
{
    float halfRatio((m_polarRadius + (labelMargin / 2.0f)) / 2.0f);
    QVector3D gridLineScaler(gridLineWidth, gridLineWidth, halfRatio);
    int gridLineCount = m_axisCacheX.gridLineCount();
    const QVector<float> &gridPositions = m_axisCacheX.formatter()->gridPositions();
    const QVector<float> &subGridPositions = m_axisCacheX.formatter()->subGridPositions();
    int mainSize = gridPositions.size();
    QVector3D translateVector(0.0f, yFloorLinePos, -halfRatio);
    QQuaternion finalRotation;
    if (m_isOpenGLES)
        finalRotation = m_yRightAngleRotationNeg;
    else
        finalRotation = m_xRightAngleRotationNeg;
    if (m_yFlippedForGrid)
        finalRotation *= m_xFlipRotation;
    for (int i = 0; i < gridLineCount; i++) {
        QMatrix4x4 modelMatrix;
        QMatrix4x4 itModelMatrix;
        float gridPosition = (i >= mainSize)
                ? subGridPositions.at(i - mainSize) : gridPositions.at(i);
        QQuaternion lineRotation = QQuaternion::fromAxisAndAngle(upVector, gridPosition * 360.0f);
        modelMatrix.rotate(lineRotation);
        itModelMatrix.rotate(lineRotation);
        modelMatrix.translate(translateVector);
        modelMatrix.scale(gridLineScaler);
        itModelMatrix.scale(gridLineScaler);
        modelMatrix.rotate(finalRotation);
        itModelMatrix.rotate(finalRotation);
        QMatrix4x4 MVPMatrix = projectionViewMatrix * modelMatrix;

        shader->setUniformValue(shader->model(), modelMatrix);
        shader->setUniformValue(shader->nModel(), itModelMatrix.inverted().transposed());
        shader->setUniformValue(shader->MVP(), MVPMatrix);
        if (m_isOpenGLES) {
            m_drawer->drawLine(shader);
        } else {
            if (m_cachedShadowQuality > QAbstract3DGraph::ShadowQualityNone) {
                // Set shadow shader bindings
                QMatrix4x4 depthMVPMatrix = depthMatrix * modelMatrix;
                shader->setUniformValue(shader->depth(), depthMVPMatrix);
                // Draw the object
                m_drawer->drawObject(shader, m_gridLineObj, 0, m_depthTexture);
            } else {
                // Draw the object
                m_drawer->drawObject(shader, m_gridLineObj);
            }
        }
    }
}

float Abstract3DRenderer::calculatePolarBackgroundMargin()
{
    // Check each extents of each angular label
    // Calculate angular position
    QVector<float> &labelPositions = m_axisCacheX.formatter()->labelPositions();
    float actualLabelHeight = m_drawer->scaledFontSize() * 2.0f; // All labels are same height
    float maxNeededMargin = 0.0f;

    // Axis title needs to be accounted for
    if (m_axisCacheX.isTitleVisible())
        maxNeededMargin = 2.0f * actualLabelHeight + 3.0f * labelMargin;

    for (int label = 0; label < labelPositions.size(); label++) {
        QSize labelSize = m_axisCacheX.labelItems().at(label)->size();
        float actualLabelWidth = actualLabelHeight / labelSize.height() * labelSize.width();
        float labelPosition = labelPositions.at(label);
        qreal angle = labelPosition * M_PI * 2.0;
        float x = qAbs((m_polarRadius + labelMargin) * float(qSin(angle)))
                + actualLabelWidth - m_polarRadius + labelMargin;
        float z = qAbs(-(m_polarRadius + labelMargin) * float(qCos(angle)))
                + actualLabelHeight - m_polarRadius + labelMargin;
        float neededMargin = qMax(x, z);
        maxNeededMargin = qMax(maxNeededMargin, neededMargin);
    }

    return maxNeededMargin;
}

void Abstract3DRenderer::updateCameraViewport()
{
    QVector3D adjustedTarget = m_cachedScene->activeCamera()->target();
    fixCameraTarget(adjustedTarget);
    if (m_oldCameraTarget != adjustedTarget) {
        QVector3D cameraBase = cameraDistanceVector + adjustedTarget;

        m_cachedScene->activeCamera()->d_ptr->setBaseOrientation(cameraBase,
                                                                 adjustedTarget,
                                                                 upVector);
        m_oldCameraTarget = adjustedTarget;
    }
    m_cachedScene->activeCamera()->d_ptr->updateViewMatrix(m_autoScaleAdjustment);
    // Set light position (i.e rotate light with activeCamera, a bit above it)
    m_cachedScene->d_ptr->setLightPositionRelativeToCamera(defaultLightPos);
}

QT_END_NAMESPACE_DATAVISUALIZATION
