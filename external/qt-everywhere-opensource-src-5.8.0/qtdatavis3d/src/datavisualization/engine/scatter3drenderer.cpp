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

#include "scatter3drenderer_p.h"
#include "q3dcamera_p.h"
#include "shaderhelper_p.h"
#include "texturehelper_p.h"
#include "utils_p.h"
#include "scatterseriesrendercache_p.h"
#include "scatterobjectbufferhelper_p.h"
#include "scatterpointbufferhelper_p.h"

#include <QtCore/qmath.h>

// You can verify that depth buffer drawing works correctly by uncommenting this.
// You should see the scene from  where the light is
//#define SHOW_DEPTH_TEXTURE_SCENE

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

const GLfloat defaultMinSize = 0.01f;
const GLfloat defaultMaxSize = 0.1f;
const GLfloat itemScaler = 3.0f;

Scatter3DRenderer::Scatter3DRenderer(Scatter3DController *controller)
    : Abstract3DRenderer(controller),
      m_selectedItem(0),
      m_updateLabels(false),
      m_dotShader(0),
      m_dotGradientShader(0),
      m_staticSelectedItemGradientShader(0),
      m_staticSelectedItemShader(0),
      m_pointShader(0),
      m_depthShader(0),
      m_selectionShader(0),
      m_backgroundShader(0),
      m_staticGradientPointShader(0),
      m_bgrTexture(0),
      m_selectionTexture(0),
      m_depthFrameBuffer(0),
      m_selectionFrameBuffer(0),
      m_selectionDepthBuffer(0),
      m_shadowQualityToShader(100.0f),
      m_shadowQualityMultiplier(3),
      m_scaleX(0.0f),
      m_scaleY(0.0f),
      m_scaleZ(0.0f),
      m_selectedItemIndex(Scatter3DController::invalidSelectionIndex()),
      m_selectedSeriesCache(0),
      m_oldSelectedSeriesCache(0),
      m_dotSizeScale(1.0f),
      m_maxItemSize(0.0f),
      m_clickedIndex(Scatter3DController::invalidSelectionIndex()),
      m_havePointSeries(false),
      m_haveMeshSeries(false),
      m_haveUniformColorMeshSeries(false),
      m_haveGradientMeshSeries(false)
{
    initializeOpenGL();
}

Scatter3DRenderer::~Scatter3DRenderer()
{
    fixContextBeforeDelete();

    if (QOpenGLContext::currentContext()) {
        m_textureHelper->glDeleteFramebuffers(1, &m_selectionFrameBuffer);
        m_textureHelper->glDeleteRenderbuffers(1, &m_selectionDepthBuffer);
        m_textureHelper->deleteTexture(&m_selectionTexture);
        m_textureHelper->glDeleteFramebuffers(1, &m_depthFrameBuffer);
        m_textureHelper->deleteTexture(&m_bgrTexture);
    }
    delete m_dotShader;
    delete m_staticSelectedItemGradientShader;
    delete m_staticSelectedItemShader;
    delete m_dotGradientShader;
    delete m_depthShader;
    delete m_selectionShader;
    delete m_backgroundShader;
    delete m_staticGradientPointShader;
}

void Scatter3DRenderer::initializeOpenGL()
{
    Abstract3DRenderer::initializeOpenGL();

    // Initialize shaders

    if (!m_isOpenGLES) {
        initDepthShader(); // For shadows
        loadGridLineMesh();
    } else {
        initPointShader();
    }

    // Init selection shader
    initSelectionShader();

    // Set view port
    glViewport(m_primarySubViewport.x(),
               m_primarySubViewport.y(),
               m_primarySubViewport.width(),
               m_primarySubViewport.height());

    // Load background mesh (we need to be initialized first)
    loadBackgroundMesh();
}

void Scatter3DRenderer::fixCameraTarget(QVector3D &target)
{
    target.setX(target.x() * m_scaleX);
    target.setY(target.y() * m_scaleY);
    target.setZ(target.z() * -m_scaleZ);
}

void Scatter3DRenderer::getVisibleItemBounds(QVector3D &minBounds, QVector3D &maxBounds)
{
    // The inputs are the item bounds in OpenGL coordinates.
    // The outputs limit these bounds to visible ranges, normalized to range [-1, 1]
    // Volume shader flips the Y and Z axes, so we need to set negatives of actual values to those
    float itemRangeX = (maxBounds.x() - minBounds.x());
    float itemRangeY = (maxBounds.y() - minBounds.y());
    float itemRangeZ = (maxBounds.z() - minBounds.z());

    if (minBounds.x() < -m_scaleX)
        minBounds.setX(-1.0f + (2.0f * qAbs(minBounds.x() + m_scaleX) / itemRangeX));
    else
        minBounds.setX(-1.0f);

    if (minBounds.y() < -m_scaleY)
        minBounds.setY(-(-1.0f + (2.0f * qAbs(minBounds.y() + m_scaleY) / itemRangeY)));
    else
        minBounds.setY(1.0f);

    if (minBounds.z() < -m_scaleZ)
        minBounds.setZ(-(-1.0f + (2.0f * qAbs(minBounds.z() + m_scaleZ) / itemRangeZ)));
    else
        minBounds.setZ(1.0f);

    if (maxBounds.x() > m_scaleX)
        maxBounds.setX(1.0f - (2.0f * qAbs(maxBounds.x() - m_scaleX) / itemRangeX));
    else
        maxBounds.setX(1.0f);

    if (maxBounds.y() > m_scaleY)
        maxBounds.setY(-(1.0f - (2.0f * qAbs(maxBounds.y() - m_scaleY) / itemRangeY)));
    else
        maxBounds.setY(-1.0f);

    if (maxBounds.z() > m_scaleZ)
        maxBounds.setZ(-(1.0f - (2.0f * qAbs(maxBounds.z() - m_scaleZ) / itemRangeZ)));
    else
        maxBounds.setZ(-1.0f);
}

void Scatter3DRenderer::updateData()
{
    calculateSceneScalingFactors();
    int totalDataSize = 0;

    foreach (SeriesRenderCache *baseCache, m_renderCacheList) {
        ScatterSeriesRenderCache *cache = static_cast<ScatterSeriesRenderCache *>(baseCache);
        if (cache->isVisible()) {
            const QScatter3DSeries *currentSeries = cache->series();
            ScatterRenderItemArray &renderArray = cache->renderArray();
            QScatterDataProxy *dataProxy = currentSeries->dataProxy();
            const QScatterDataArray &dataArray = *dataProxy->array();
            int dataSize = dataArray.size();
            totalDataSize += dataSize;
            if (cache->dataDirty()) {
                if (dataSize != renderArray.size())
                    renderArray.resize(dataSize);

                for (int i = 0; i < dataSize; i++)
                    updateRenderItem(dataArray.at(i), renderArray[i]);

                if (m_cachedOptimizationHint.testFlag(QAbstract3DGraph::OptimizationStatic))
                    cache->setStaticBufferDirty(true);

                cache->setDataDirty(false);
            }
        }
    }

    if (totalDataSize) {
        m_dotSizeScale = GLfloat(qBound(defaultMinSize,
                                        2.0f / float(qSqrt(qreal(totalDataSize))),
                                        defaultMaxSize));
    }

    if (m_cachedOptimizationHint.testFlag(QAbstract3DGraph::OptimizationStatic)) {
        foreach (SeriesRenderCache *baseCache, m_renderCacheList) {
            ScatterSeriesRenderCache *cache = static_cast<ScatterSeriesRenderCache *>(baseCache);
            if (cache->isVisible()) {
                ScatterRenderItemArray &renderArray = cache->renderArray();
                const int renderArraySize = renderArray.size();

                if (cache->mesh() == QAbstract3DSeries::MeshPoint) {
                    ScatterPointBufferHelper *points = cache->bufferPoints();
                    if (!points) {
                        points = new ScatterPointBufferHelper();
                        cache->setBufferPoints(points);
                    }
                    points->setScaleY(m_scaleY);
                    points->load(cache);
                } else {
                    ScatterObjectBufferHelper *object = cache->bufferObject();
                    if (!object) {
                        object = new ScatterObjectBufferHelper();
                        cache->setBufferObject(object);
                    }
                    if (renderArraySize != cache->oldArraySize()
                            || cache->object()->objectFile() != cache->oldMeshFileName()
                            || cache->staticBufferDirty()) {
                        object->setScaleY(m_scaleY);
                        object->fullLoad(cache, m_dotSizeScale);
                        cache->setOldArraySize(renderArraySize);
                        cache->setOldMeshFileName(cache->object()->objectFile());
                    } else {
                        object->update(cache, m_dotSizeScale);
                    }
                }

                cache->setStaticBufferDirty(false);
            }
        }
    }

    updateSelectedItem(m_selectedItemIndex,
                       m_selectedSeriesCache ? m_selectedSeriesCache->series() : 0);
}

void Scatter3DRenderer::updateSeries(const QList<QAbstract3DSeries *> &seriesList)
{
    int seriesCount = seriesList.size();

    // Check OptimizationStatic specific issues before populate marks changeTracker done
    if (m_cachedOptimizationHint.testFlag(QAbstract3DGraph::OptimizationStatic)) {
        for (int i = 0; i < seriesCount; i++) {
            QScatter3DSeries *scatterSeries = static_cast<QScatter3DSeries *>(seriesList[i]);
            if (scatterSeries->isVisible()) {
                QAbstract3DSeriesChangeBitField &changeTracker = scatterSeries->d_ptr->m_changeTracker;
                ScatterSeriesRenderCache *cache =
                        static_cast<ScatterSeriesRenderCache *>(m_renderCacheList.value(scatterSeries));
                if (cache) {
                    if (changeTracker.baseGradientChanged || changeTracker.colorStyleChanged)
                        cache->setStaticObjectUVDirty(true);
                    if (cache->itemSize() != scatterSeries->itemSize())
                        cache->setStaticBufferDirty(true);
                }
            }
        }
    }

    Abstract3DRenderer::updateSeries(seriesList);

    float maxItemSize = 0.0f;
    float itemSize = 0.0f;
    bool noSelection = true;

    m_havePointSeries = false;
    m_haveMeshSeries = false;
    m_haveUniformColorMeshSeries = false;
    m_haveGradientMeshSeries = false;

    for (int i = 0; i < seriesCount; i++) {
        QScatter3DSeries *scatterSeries = static_cast<QScatter3DSeries *>(seriesList[i]);
        if (scatterSeries->isVisible()) {
            ScatterSeriesRenderCache *cache =
                    static_cast<ScatterSeriesRenderCache *>(m_renderCacheList.value(scatterSeries));
            itemSize = scatterSeries->itemSize();
            if (maxItemSize < itemSize)
                maxItemSize = itemSize;
            if (cache->itemSize() != itemSize)
                cache->setItemSize(itemSize);
            if (noSelection
                    && scatterSeries->selectedItem() != QScatter3DSeries::invalidSelectionIndex()) {
                if (m_selectionLabel != cache->itemLabel())
                    m_selectionLabelDirty = true;
                noSelection = false;
            }

            if (cache->mesh() == QAbstract3DSeries::MeshPoint) {
                m_havePointSeries = true;
            } else {
                m_haveMeshSeries = true;
                if (cache->colorStyle() == Q3DTheme::ColorStyleUniform)
                    m_haveUniformColorMeshSeries = true;
                else
                    m_haveGradientMeshSeries = true;
            }

            if (cache->staticBufferDirty()) {
                if (cache->mesh() != QAbstract3DSeries::MeshPoint) {
                    ScatterObjectBufferHelper *object = cache->bufferObject();
                    object->update(cache, m_dotSizeScale);
                }
                cache->setStaticBufferDirty(false);
            }
            if (cache->staticObjectUVDirty()) {
                if (cache->mesh() == QAbstract3DSeries::MeshPoint) {
                    ScatterPointBufferHelper *object = cache->bufferPoints();
                    object->updateUVs(cache);
                } else {
                    ScatterObjectBufferHelper *object = cache->bufferObject();
                    object->updateUVs(cache);
                }
                cache->setStaticObjectUVDirty(false);
            }
        }
    }
    m_maxItemSize = maxItemSize;
    calculateSceneScalingFactors();

    if (noSelection) {
        if (!selectionLabel().isEmpty())
            m_selectionLabelDirty = true;
        m_selectedSeriesCache = 0;
    }
}

SeriesRenderCache *Scatter3DRenderer::createNewCache(QAbstract3DSeries *series)
{
    return new ScatterSeriesRenderCache(series, this);
}

void Scatter3DRenderer::updateItems(const QVector<Scatter3DController::ChangeItem> &items)
{
    ScatterSeriesRenderCache *cache = 0;
    const QScatter3DSeries *prevSeries = 0;
    const QScatterDataArray *dataArray = 0;
    const bool optimizationStatic = m_cachedOptimizationHint.testFlag(
                QAbstract3DGraph::OptimizationStatic);

    foreach (Scatter3DController::ChangeItem item, items) {
        QScatter3DSeries *currentSeries = item.series;
        if (currentSeries != prevSeries) {
            cache = static_cast<ScatterSeriesRenderCache *>(m_renderCacheList.value(currentSeries));
            prevSeries = currentSeries;
            dataArray = item.series->dataProxy()->array();
            // Invisible series render caches are not updated, but instead just marked dirty, so that
            // they can be completely recalculated when they are turned visible.
            if (!cache->isVisible() && !cache->dataDirty())
                cache->setDataDirty(true);
        }
        if (cache->isVisible()) {
            const int index = item.index;
            if (index >= cache->renderArray().size())
                continue; // Items removed from array for same render
            bool oldVisibility;
            ScatterRenderItem &item = cache->renderArray()[index];
            if (optimizationStatic)
                oldVisibility = item.isVisible();
            updateRenderItem(dataArray->at(index), item);
            if (optimizationStatic) {
                if (!cache->visibilityChanged() && oldVisibility != item.isVisible())
                    cache->setVisibilityChanged(true);
                cache->updateIndices().append(index);
            }
        }
    }
    if (optimizationStatic) {
        foreach (SeriesRenderCache *baseCache, m_renderCacheList) {
            ScatterSeriesRenderCache *cache = static_cast<ScatterSeriesRenderCache *>(baseCache);
            if (cache->isVisible() && cache->updateIndices().size()) {
                if (cache->mesh() == QAbstract3DSeries::MeshPoint) {
                    cache->bufferPoints()->update(cache);
                    if (cache->colorStyle() == Q3DTheme::ColorStyleRangeGradient)
                        cache->bufferPoints()->updateUVs(cache);
                } else {
                    if (cache->visibilityChanged()) {
                        // If any change changes item visibility, full load is needed to
                        // resize the buffers.
                        cache->updateIndices().clear();
                        cache->bufferObject()->fullLoad(cache, m_dotSizeScale);
                    } else {
                        cache->bufferObject()->update(cache, m_dotSizeScale);
                        if (cache->colorStyle() == Q3DTheme::ColorStyleRangeGradient)
                            cache->bufferObject()->updateUVs(cache);
                    }
                }
                cache->updateIndices().clear();
            }
            cache->setVisibilityChanged(false);
        }
    }
}

void Scatter3DRenderer::updateScene(Q3DScene *scene)
{
    scene->activeCamera()->d_ptr->setMinYRotation(-90.0f);

    Abstract3DRenderer::updateScene(scene);
}

void Scatter3DRenderer::updateAxisLabels(QAbstract3DAxis::AxisOrientation orientation,
                                         const QStringList &labels)
{
    Abstract3DRenderer::updateAxisLabels(orientation, labels);

    // Angular axis label dimensions affect the chart dimensions
    if (m_polarGraph && orientation == QAbstract3DAxis::AxisOrientationX)
        calculateSceneScalingFactors();
}

void Scatter3DRenderer::updateAxisTitleVisibility(QAbstract3DAxis::AxisOrientation orientation,
                                                  bool visible)
{
    Abstract3DRenderer::updateAxisTitleVisibility(orientation, visible);

    // Angular axis title existence affects the chart dimensions
    if (m_polarGraph && orientation == QAbstract3DAxis::AxisOrientationX)
        calculateSceneScalingFactors();
}

void Scatter3DRenderer::updateOptimizationHint(QAbstract3DGraph::OptimizationHints hint)
{
    Abstract3DRenderer::updateOptimizationHint(hint);

    Abstract3DRenderer::reInitShaders();

    if (m_isOpenGLES && hint.testFlag(QAbstract3DGraph::OptimizationStatic)
            && !m_staticGradientPointShader) {
        initStaticPointShaders(QStringLiteral(":/shaders/vertexPointES2_UV"),
                               QStringLiteral(":/shaders/fragmentLabel"));
    }
}

void Scatter3DRenderer::updateMargin(float margin)
{
    Abstract3DRenderer::updateMargin(margin);
    calculateSceneScalingFactors();
}

void Scatter3DRenderer::resetClickedStatus()
{
    m_clickedIndex = Scatter3DController::invalidSelectionIndex();
    m_clickedSeries = 0;
}

void Scatter3DRenderer::render(GLuint defaultFboHandle)
{
    // Handle GL state setup for FBO buffers and clearing of the render surface
    Abstract3DRenderer::render(defaultFboHandle);

    if (m_axisCacheX.positionsDirty())
        m_axisCacheX.updateAllPositions();
    if (m_axisCacheY.positionsDirty())
        m_axisCacheY.updateAllPositions();
    if (m_axisCacheZ.positionsDirty())
        m_axisCacheZ.updateAllPositions();

    // Draw dots scene
    drawScene(defaultFboHandle);
}

void Scatter3DRenderer::drawScene(const GLuint defaultFboHandle)
{
    GLfloat backgroundRotation = 0;
    GLfloat selectedItemSize = 0.0f;

    // Get the optimization flag
    const bool optimizationDefault =
            !m_cachedOptimizationHint.testFlag(QAbstract3DGraph::OptimizationStatic);

    const Q3DCamera *activeCamera = m_cachedScene->activeCamera();

    QVector4D lightColor = Utils::vectorFromColor(m_cachedTheme->lightColor());

    // Specify viewport
    glViewport(m_primarySubViewport.x(),
               m_primarySubViewport.y(),
               m_primarySubViewport.width(),
               m_primarySubViewport.height());

    // Set up projection matrix
    QMatrix4x4 projectionMatrix;
    GLfloat viewPortRatio = (GLfloat)m_primarySubViewport.width()
            / (GLfloat)m_primarySubViewport.height();
    if (m_useOrthoProjection) {
        GLfloat orthoRatio = 2.0f;
        projectionMatrix.ortho(-viewPortRatio * orthoRatio, viewPortRatio * orthoRatio,
                               -orthoRatio, orthoRatio,
                               0.0f, 100.0f);
    } else {
        projectionMatrix.perspective(45.0f, viewPortRatio, 0.1f, 100.0f);
    }

    // Calculate view matrix
    QMatrix4x4 viewMatrix = activeCamera->d_ptr->viewMatrix();
    QMatrix4x4 projectionViewMatrix = projectionMatrix * viewMatrix;

    // Calculate label flipping
    if (viewMatrix.row(0).x() > 0)
        m_zFlipped = false;
    else
        m_zFlipped = true;
    if (viewMatrix.row(0).z() <= 0)
        m_xFlipped = false;
    else
        m_xFlipped = true;

    // Check if we're viewing the scene from below
    if (viewMatrix.row(2).y() < 0)
        m_yFlipped = true;
    else
        m_yFlipped = false;
    m_yFlippedForGrid = m_yFlipped; // Polar axis grid drawing in abstract needs this

    // Calculate background rotation
    if (!m_zFlipped && !m_xFlipped)
        backgroundRotation = 270.0f;
    else if (!m_zFlipped && m_xFlipped)
        backgroundRotation = 180.0f;
    else if (m_zFlipped && m_xFlipped)
        backgroundRotation = 90.0f;
    else if (m_zFlipped && !m_xFlipped)
        backgroundRotation = 0.0f;

    // Get light position from the scene
    QVector3D lightPos = m_cachedScene->activeLight()->position();

    // Introduce regardless of shadow quality to simplify logic
    QMatrix4x4 depthProjectionViewMatrix;

    ShaderHelper *pointSelectionShader;
    if (!m_isOpenGLES) {
#if !defined(QT_OPENGL_ES_2)
        if (m_havePointSeries) {
            glEnable(GL_POINT_SMOOTH);
            glEnable(GL_PROGRAM_POINT_SIZE);
        }

        if (m_cachedShadowQuality > QAbstract3DGraph::ShadowQualityNone) {
            // Render scene into a depth texture for using with shadow mapping
            // Bind depth shader
            m_depthShader->bind();

            // Set viewport for depth map rendering. Must match texture size. Larger values give smoother shadows.
            glViewport(0, 0,
                       m_primarySubViewport.width() * m_shadowQualityMultiplier,
                       m_primarySubViewport.height() * m_shadowQualityMultiplier);

            // Enable drawing to framebuffer
            glBindFramebuffer(GL_FRAMEBUFFER, m_depthFrameBuffer);
            glClear(GL_DEPTH_BUFFER_BIT);

            // Set front face culling to reduce self-shadowing issues
            glCullFace(GL_FRONT);

            QMatrix4x4 depthViewMatrix;
            QMatrix4x4 depthProjectionMatrix;

            // Get the depth view matrix
            // It may be possible to hack lightPos here if we want to make some tweaks to shadow
            QVector3D depthLightPos = activeCamera->d_ptr->calculatePositionRelativeToCamera(
                        zeroVector, 0.0f, 2.5f / m_autoScaleAdjustment);
            depthViewMatrix.lookAt(depthLightPos, zeroVector, upVector);
            // Set the depth projection matrix
            depthProjectionMatrix.perspective(15.0f, viewPortRatio, 3.0f, 100.0f);
            depthProjectionViewMatrix = depthProjectionMatrix * depthViewMatrix;

            // Draw dots to depth buffer
            foreach (SeriesRenderCache *baseCache, m_renderCacheList) {
                if (baseCache->isVisible()) {
                    ScatterSeriesRenderCache *cache =
                            static_cast<ScatterSeriesRenderCache *>(baseCache);
                    ObjectHelper *dotObj = cache->object();
                    QQuaternion seriesRotation(cache->meshRotation());
                    const ScatterRenderItemArray &renderArray = cache->renderArray();
                    const int renderArraySize = renderArray.size();
                    bool drawingPoints = (cache->mesh() == QAbstract3DSeries::MeshPoint);
                    float itemSize = cache->itemSize() / itemScaler;
                    if (itemSize == 0.0f)
                        itemSize = m_dotSizeScale;
                    if (drawingPoints) {
                        // Scale points based on shadow quality for shadows, not by zoom level
                        m_funcs_2_1->glPointSize(itemSize * 100.0f * m_shadowQualityMultiplier);
                    }
                    QVector3D modelScaler(itemSize, itemSize, itemSize);

                    if (!optimizationDefault
                            && ((drawingPoints && cache->bufferPoints()->indexCount() == 0)
                                || (!drawingPoints && cache->bufferObject()->indexCount() == 0))) {
                        continue;
                    }

                    int loopCount = 1;
                    if (optimizationDefault)
                        loopCount = renderArraySize;
                    for (int dot = 0; dot < loopCount; dot++) {
                        const ScatterRenderItem &item = renderArray.at(dot);
                        if (!item.isVisible() && optimizationDefault)
                            continue;

                        QMatrix4x4 modelMatrix;
                        QMatrix4x4 MVPMatrix;

                        if (optimizationDefault) {
                            modelMatrix.translate(item.translation());
                            if (!drawingPoints) {
                                if (!seriesRotation.isIdentity() || !item.rotation().isIdentity())
                                    modelMatrix.rotate(seriesRotation * item.rotation());
                                modelMatrix.scale(modelScaler);
                            }
                        }

                        MVPMatrix = depthProjectionViewMatrix * modelMatrix;

                        m_depthShader->setUniformValue(m_depthShader->MVP(), MVPMatrix);

                        if (drawingPoints) {
                            if (optimizationDefault)
                                m_drawer->drawPoint(m_depthShader);
                            else
                                m_drawer->drawPoints(m_depthShader, cache->bufferPoints(), 0);
                        } else {
                            if (optimizationDefault) {
                                // 1st attribute buffer : vertices
                                glEnableVertexAttribArray(m_depthShader->posAtt());
                                glBindBuffer(GL_ARRAY_BUFFER, dotObj->vertexBuf());
                                glVertexAttribPointer(m_depthShader->posAtt(), 3, GL_FLOAT, GL_FALSE, 0,
                                                      (void *)0);

                                // Index buffer
                                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dotObj->elementBuf());

                                // Draw the triangles
                                glDrawElements(GL_TRIANGLES, dotObj->indexCount(),
                                               GL_UNSIGNED_INT, (void *)0);

                                // Free buffers
                                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
                                glBindBuffer(GL_ARRAY_BUFFER, 0);

                                glDisableVertexAttribArray(m_depthShader->posAtt());
                            } else {
                                ScatterObjectBufferHelper *object = cache->bufferObject();
                                // 1st attribute buffer : vertices
                                glEnableVertexAttribArray(m_depthShader->posAtt());
                                glBindBuffer(GL_ARRAY_BUFFER, object->vertexBuf());
                                glVertexAttribPointer(m_depthShader->posAtt(), 3, GL_FLOAT, GL_FALSE, 0,
                                                      (void *)0);

                                // Index buffer
                                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, object->elementBuf());

                                // Draw the triangles
                                glDrawElements(GL_TRIANGLES, object->indexCount(),
                                               GL_UNSIGNED_INT, (void *)0);

                                // Free buffers
                                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
                                glBindBuffer(GL_ARRAY_BUFFER, 0);

                                glDisableVertexAttribArray(m_depthShader->posAtt());
                            }
                        }
                    }
                }
            }

            Abstract3DRenderer::drawCustomItems(RenderingDepth, m_depthShader, viewMatrix,
                                                projectionViewMatrix,
                                                depthProjectionViewMatrix, m_depthTexture,
                                                m_shadowQualityToShader);

            // Disable drawing to framebuffer (= enable drawing to screen)
            glBindFramebuffer(GL_FRAMEBUFFER, defaultFboHandle);

            // Reset culling to normal
            glCullFace(GL_BACK);

            // Revert to original viewport
            glViewport(m_primarySubViewport.x(),
                       m_primarySubViewport.y(),
                       m_primarySubViewport.width(),
                       m_primarySubViewport.height());
        }
#endif
        pointSelectionShader = m_selectionShader;
    } else {
        pointSelectionShader = m_pointShader;
    }

    ShaderHelper *selectionShader = m_selectionShader;

    // Do position mapping when necessary
    if (m_graphPositionQueryPending) {
        QVector3D graphDimensions(m_scaleX, m_scaleY, m_scaleZ);
        queriedGraphPosition(projectionViewMatrix, graphDimensions, defaultFboHandle);
        emit needRender();
    }

    // Skip selection mode drawing if we have no selection mode
    if (m_cachedSelectionMode > QAbstract3DGraph::SelectionNone
            && SelectOnScene == m_selectionState
            && (m_visibleSeriesCount > 0 || !m_customRenderCache.isEmpty())
            && m_selectionTexture) {
        // Draw dots to selection buffer
        glBindFramebuffer(GL_FRAMEBUFFER, m_selectionFrameBuffer);
        glViewport(0, 0,
                   m_primarySubViewport.width(),
                   m_primarySubViewport.height());

        glEnable(GL_DEPTH_TEST); // Needed, otherwise the depth render buffer is not used
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // Set clear color to white (= skipColor)
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Needed for clearing the frame buffer
        glDisable(GL_DITHER); // disable dithering, it may affect colors if enabled

        bool previousDrawingPoints = false;
        int totalIndex = 0;
        foreach (SeriesRenderCache *baseCache, m_renderCacheList) {
            if (baseCache->isVisible()) {
                ScatterSeriesRenderCache *cache =
                        static_cast<ScatterSeriesRenderCache *>(baseCache);
                ObjectHelper *dotObj = cache->object();
                QQuaternion seriesRotation(cache->meshRotation());
                const ScatterRenderItemArray &renderArray = cache->renderArray();
                const int renderArraySize = renderArray.size();
                bool drawingPoints = (cache->mesh() == QAbstract3DSeries::MeshPoint);
                float itemSize = cache->itemSize() / itemScaler;
                if (itemSize == 0.0f)
                    itemSize = m_dotSizeScale;
#if !defined(QT_OPENGL_ES_2)
                if (drawingPoints && !m_isOpenGLES)
                    m_funcs_2_1->glPointSize(itemSize * activeCamera->zoomLevel());
#endif
                QVector3D modelScaler(itemSize, itemSize, itemSize);

                // Rebind selection shader if it has changed
                if (!totalIndex || drawingPoints != previousDrawingPoints) {
                    previousDrawingPoints = drawingPoints;
                    if (drawingPoints)
                        selectionShader = pointSelectionShader;
                    else
                        selectionShader = m_selectionShader;

                    selectionShader->bind();
                }
                cache->setSelectionIndexOffset(totalIndex);
                for (int dot = 0; dot < renderArraySize; dot++) {
                    const ScatterRenderItem &item = renderArray.at(dot);
                    if (!item.isVisible()) {
                        totalIndex++;
                        continue;
                    }

                    QMatrix4x4 modelMatrix;
                    QMatrix4x4 MVPMatrix;

                    modelMatrix.translate(item.translation());
                    if (!drawingPoints) {
                        if (!seriesRotation.isIdentity() || !item.rotation().isIdentity())
                            modelMatrix.rotate(seriesRotation * item.rotation());
                        modelMatrix.scale(modelScaler);
                    }

                    MVPMatrix = projectionViewMatrix * modelMatrix;

                    QVector4D dotColor = indexToSelectionColor(totalIndex++);
                    dotColor /= 255.0f;

                    selectionShader->setUniformValue(selectionShader->MVP(), MVPMatrix);
                    selectionShader->setUniformValue(selectionShader->color(), dotColor);

                    if (drawingPoints)
                        m_drawer->drawPoint(selectionShader);
                    else
                        m_drawer->drawSelectionObject(selectionShader, dotObj);
                }
            }
        }

        Abstract3DRenderer::drawCustomItems(RenderingSelection, m_selectionShader,
                                            viewMatrix, projectionViewMatrix,
                                            depthProjectionViewMatrix, m_depthTexture,
                                            m_shadowQualityToShader);

        drawLabels(true, activeCamera, viewMatrix, projectionMatrix);

        glEnable(GL_DITHER);

        // Read color under cursor
        QVector4D clickedColor = Utils::getSelection(m_inputPosition,
                                                     m_viewport.height());
        selectionColorToSeriesAndIndex(clickedColor, m_clickedIndex, m_clickedSeries);
        m_clickResolved = true;

        emit needRender();

        // Revert to original fbo and viewport
        glBindFramebuffer(GL_FRAMEBUFFER, defaultFboHandle);
        glViewport(m_primarySubViewport.x(),
                   m_primarySubViewport.y(),
                   m_primarySubViewport.width(),
                   m_primarySubViewport.height());
    }

    // Draw dots
    ShaderHelper *dotShader = 0;
    GLuint gradientTexture = 0;
    bool dotSelectionFound = false;
    ScatterRenderItem *selectedItem(0);
    QVector4D baseColor;
    QVector4D dotColor;

    bool previousDrawingPoints = false;
    Q3DTheme::ColorStyle previousMeshColorStyle = Q3DTheme::ColorStyleUniform;
    if (m_haveMeshSeries) {
        // Set unchanging shader bindings
        if (m_haveGradientMeshSeries) {
            m_dotGradientShader->bind();
            m_dotGradientShader->setUniformValue(m_dotGradientShader->lightP(), lightPos);
            m_dotGradientShader->setUniformValue(m_dotGradientShader->view(), viewMatrix);
            m_dotGradientShader->setUniformValue(m_dotGradientShader->ambientS(),
                                                 m_cachedTheme->ambientLightStrength());
            m_dotGradientShader->setUniformValue(m_dotGradientShader->lightColor(), lightColor);
        }
        if (m_haveUniformColorMeshSeries) {
            m_dotShader->bind();
            m_dotShader->setUniformValue(m_dotShader->lightP(), lightPos);
            m_dotShader->setUniformValue(m_dotShader->view(), viewMatrix);
            m_dotShader->setUniformValue(m_dotShader->ambientS(),
                                         m_cachedTheme->ambientLightStrength());
            m_dotShader->setUniformValue(m_dotShader->lightColor(), lightColor);
            dotShader = m_dotShader;
        } else {
            dotShader = m_dotGradientShader;
            previousMeshColorStyle = Q3DTheme::ColorStyleRangeGradient;
            m_dotGradientShader->setUniformValue(m_dotGradientShader->gradientHeight(), 0.0f);
        }
    } else {
        dotShader = pointSelectionShader;
    }

    float rangeGradientYScaler = 0.5f / m_scaleY;

    foreach (SeriesRenderCache *baseCache, m_renderCacheList) {
        if (baseCache->isVisible()) {
            ScatterSeriesRenderCache *cache =
                    static_cast<ScatterSeriesRenderCache *>(baseCache);
            ObjectHelper *dotObj = cache->object();
            QQuaternion seriesRotation(cache->meshRotation());
            ScatterRenderItemArray &renderArray = cache->renderArray();
            const int renderArraySize = renderArray.size();
            bool selectedSeries = m_cachedSelectionMode > QAbstract3DGraph::SelectionNone
                    && (m_selectedSeriesCache == cache);
            bool drawingPoints = (cache->mesh() == QAbstract3DSeries::MeshPoint);
            Q3DTheme::ColorStyle colorStyle = cache->colorStyle();
            bool colorStyleIsUniform = (colorStyle == Q3DTheme::ColorStyleUniform);
            bool useColor = colorStyleIsUniform || drawingPoints;
            bool rangeGradientPoints = drawingPoints
                    && (colorStyle == Q3DTheme::ColorStyleRangeGradient);
            float itemSize = cache->itemSize() / itemScaler;
            if (itemSize == 0.0f)
                itemSize = m_dotSizeScale;
#if !defined(QT_OPENGL_ES_2)
            if (drawingPoints && !m_isOpenGLES)
                m_funcs_2_1->glPointSize(itemSize * activeCamera->zoomLevel());
#endif
            QVector3D modelScaler(itemSize, itemSize, itemSize);
            int gradientImageHeight = cache->gradientImage().height();
            int maxGradientPositition = gradientImageHeight - 1;

            if (!optimizationDefault
                    && ((drawingPoints && cache->bufferPoints()->indexCount() == 0)
                        || (!drawingPoints && cache->bufferObject()->indexCount() == 0))) {
                continue;
            }

            // Rebind shader if it has changed
            if (drawingPoints != previousDrawingPoints
                    || (!drawingPoints &&
                        (colorStyleIsUniform != (previousMeshColorStyle
                                                 == Q3DTheme::ColorStyleUniform)))
                    || (!optimizationDefault && drawingPoints)) {
                previousDrawingPoints = drawingPoints;
                if (drawingPoints) {
                    if (!optimizationDefault && rangeGradientPoints) {
                        if (m_isOpenGLES)
                            dotShader = m_staticGradientPointShader;
                        else
                            dotShader = m_labelShader;
                    } else {
                        dotShader = pointSelectionShader;
                    }
                } else {
                    if (colorStyleIsUniform)
                        dotShader = m_dotShader;
                    else
                        dotShader = m_dotGradientShader;
                }
                dotShader->bind();
            }

            if (!drawingPoints && !colorStyleIsUniform && previousMeshColorStyle != colorStyle) {
                if (colorStyle == Q3DTheme::ColorStyleObjectGradient) {
                    dotShader->setUniformValue(dotShader->gradientMin(), 0.0f);
                    dotShader->setUniformValue(dotShader->gradientHeight(),
                                               0.5f);
                } else {
                    // Each dot is of uniform color according to its Y-coordinate
                    dotShader->setUniformValue(dotShader->gradientHeight(),
                                               0.0f);
                }
            }

            if (!drawingPoints)
                previousMeshColorStyle = colorStyle;

            if (useColor) {
                baseColor = cache->baseColor();
                dotColor = baseColor;
            }
            int loopCount = 1;
            if (optimizationDefault)
                loopCount = renderArraySize;

            for (int i = 0; i < loopCount; i++) {
                ScatterRenderItem &item = renderArray[i];
                if (!item.isVisible() && optimizationDefault)
                    continue;

                QMatrix4x4 modelMatrix;
                QMatrix4x4 MVPMatrix;
                QMatrix4x4 itModelMatrix;

                if (optimizationDefault) {
                    modelMatrix.translate(item.translation());
                    if (!drawingPoints) {
                        if (!seriesRotation.isIdentity() || !item.rotation().isIdentity()) {
                            QQuaternion totalRotation = seriesRotation * item.rotation();
                            modelMatrix.rotate(totalRotation);
                            itModelMatrix.rotate(totalRotation);
                        }
                        modelMatrix.scale(modelScaler);
                        itModelMatrix.scale(modelScaler);
                    }
                }
#ifdef SHOW_DEPTH_TEXTURE_SCENE
                MVPMatrix = depthProjectionViewMatrix * modelMatrix;
#else
                MVPMatrix = projectionViewMatrix * modelMatrix;
#endif

                if (useColor) {
                    if (rangeGradientPoints) {
                        // Drawing points with range gradient
                        // Get color from gradient based on items y position converted to percent
                        int position = ((item.translation().y() + m_scaleY) * rangeGradientYScaler) * gradientImageHeight;
                        position = qMin(maxGradientPositition, position); // clamp to edge
                        dotColor = Utils::vectorFromColor(
                                    cache->gradientImage().pixel(0, position));
                    } else {
                        dotColor = baseColor;
                    }
                } else {
                    gradientTexture = cache->baseGradientTexture();
                }

                if (!optimizationDefault && rangeGradientPoints)
                    gradientTexture = cache->baseGradientTexture();

                GLfloat lightStrength = m_cachedTheme->lightStrength();
                if (optimizationDefault && selectedSeries && (m_selectedItemIndex == i)) {
                    if (useColor)
                        dotColor = cache->singleHighlightColor();
                    else
                        gradientTexture = cache->singleHighlightGradientTexture();
                    lightStrength = m_cachedTheme->highlightLightStrength();
                    // Save the reference to the item to be used in label drawing
                    selectedItem = &item;
                    dotSelectionFound = true;
                    // Save selected item size (adjusted with font size) for selection label
                    // positioning
                    selectedItemSize = itemSize + m_drawer->scaledFontSize() - 0.05f;
                }

                if (!drawingPoints) {
                    // Set shader bindings
                    dotShader->setUniformValue(dotShader->model(), modelMatrix);
                    dotShader->setUniformValue(dotShader->nModel(),
                                               itModelMatrix.inverted().transposed());
                }

                dotShader->setUniformValue(dotShader->MVP(), MVPMatrix);
                if (useColor) {
                    dotShader->setUniformValue(dotShader->color(), dotColor);
                } else if (colorStyle == Q3DTheme::ColorStyleRangeGradient) {
                    dotShader->setUniformValue(dotShader->gradientMin(),
                                               (item.translation().y() + m_scaleY)
                                               * rangeGradientYScaler);
                }
                if (m_cachedShadowQuality > QAbstract3DGraph::ShadowQualityNone && !m_isOpenGLES) {
                    if (!drawingPoints) {
                        // Set shadow shader bindings
                        QMatrix4x4 depthMVPMatrix = depthProjectionViewMatrix * modelMatrix;
                        dotShader->setUniformValue(dotShader->shadowQ(), m_shadowQualityToShader);
                        dotShader->setUniformValue(dotShader->depth(), depthMVPMatrix);
                        dotShader->setUniformValue(dotShader->lightS(), lightStrength / 10.0f);

                        // Draw the object
                        if (optimizationDefault) {
                            m_drawer->drawObject(dotShader, dotObj, gradientTexture,
                                                 m_depthTexture);
                        } else {
                            m_drawer->drawObject(dotShader, cache->bufferObject(), gradientTexture,
                                                 m_depthTexture);
                        }
                    } else {
                        // Draw the object
                        if (optimizationDefault)
                            m_drawer->drawPoint(dotShader);
                        else
                            m_drawer->drawPoints(dotShader, cache->bufferPoints(), gradientTexture);
                    }
                } else {
                    if (!drawingPoints) {
                        // Set shadowless shader bindings
                        dotShader->setUniformValue(dotShader->lightS(), lightStrength);
                        // Draw the object
                        if (optimizationDefault)
                            m_drawer->drawObject(dotShader, dotObj, gradientTexture);
                        else
                            m_drawer->drawObject(dotShader, cache->bufferObject(), gradientTexture);
                    } else {
                        // Draw the object
                        if (optimizationDefault)
                            m_drawer->drawPoint(dotShader);
                        else
                            m_drawer->drawPoints(dotShader, cache->bufferPoints(), gradientTexture);
                    }
                }
            }


            // Draw the selected item on static optimization
            if (!optimizationDefault && selectedSeries
                    && m_selectedItemIndex != Scatter3DController::invalidSelectionIndex()) {
                ScatterRenderItem &item = renderArray[m_selectedItemIndex];
                if (item.isVisible()) {
                    ShaderHelper *selectionShader;
                    if (drawingPoints) {
                        selectionShader = pointSelectionShader;
                    } else {
                        if (colorStyleIsUniform)
                            selectionShader = m_staticSelectedItemShader;
                        else
                            selectionShader = m_staticSelectedItemGradientShader;
                    }
                    selectionShader->bind();

                    ObjectHelper *dotObj = cache->object();

                    QMatrix4x4 modelMatrix;
                    QMatrix4x4 itModelMatrix;

                    modelMatrix.translate(item.translation());
                    if (!drawingPoints) {
                        if (!seriesRotation.isIdentity() || !item.rotation().isIdentity()) {
                            QQuaternion totalRotation = seriesRotation * item.rotation();
                            modelMatrix.rotate(totalRotation);
                            itModelMatrix.rotate(totalRotation);
                        }
                        modelMatrix.scale(modelScaler);
                        itModelMatrix.scale(modelScaler);

                        selectionShader->setUniformValue(selectionShader->lightP(),
                                                         lightPos);
                        selectionShader->setUniformValue(selectionShader->view(),
                                                         viewMatrix);
                        selectionShader->setUniformValue(selectionShader->ambientS(),
                                                         m_cachedTheme->ambientLightStrength());
                        selectionShader->setUniformValue(selectionShader->lightColor(),
                                                         lightColor);
                    }

                    QMatrix4x4 MVPMatrix;
#ifdef SHOW_DEPTH_TEXTURE_SCENE
                    MVPMatrix = depthProjectionViewMatrix * modelMatrix;
#else
                    MVPMatrix = projectionViewMatrix * modelMatrix;
#endif

                    if (useColor)
                        dotColor = cache->singleHighlightColor();
                    else
                        gradientTexture = cache->singleHighlightGradientTexture();
                    GLfloat lightStrength = m_cachedTheme->highlightLightStrength();
                    // Save the reference to the item to be used in label drawing
                    selectedItem = &item;
                    dotSelectionFound = true;
                    // Save selected item size (adjusted with font size) for selection label
                    // positioning
                    selectedItemSize = itemSize + m_drawer->scaledFontSize() - 0.05f;

                    if (!drawingPoints) {
                        // Set shader bindings
                        selectionShader->setUniformValue(selectionShader->model(), modelMatrix);
                        selectionShader->setUniformValue(selectionShader->nModel(),
                                                         itModelMatrix.inverted().transposed());
                        if (!colorStyleIsUniform) {
                            if (colorStyle == Q3DTheme::ColorStyleObjectGradient) {
                                selectionShader->setUniformValue(selectionShader->gradientMin(),
                                                                 0.0f);
                                selectionShader->setUniformValue(selectionShader->gradientHeight(),
                                                                 0.5f);
                            } else {
                                // Each dot is of uniform color according to its Y-coordinate
                                selectionShader->setUniformValue(selectionShader->gradientHeight(),
                                                                 0.0f);
                                selectionShader->setUniformValue(selectionShader->gradientMin(),
                                                                 (item.translation().y() + m_scaleY)
                                                                 * rangeGradientYScaler);
                            }
                        }
                    }

                    selectionShader->setUniformValue(selectionShader->MVP(), MVPMatrix);
                    if (useColor)
                        selectionShader->setUniformValue(selectionShader->color(), dotColor);

                    if (!drawingPoints) {
                        glEnable(GL_POLYGON_OFFSET_FILL);
                        glPolygonOffset(-1.0f, 1.0f);
                    }

                    if (m_cachedShadowQuality > QAbstract3DGraph::ShadowQualityNone
                            && !m_isOpenGLES) {
                        if (!drawingPoints) {
                            // Set shadow shader bindings
                            QMatrix4x4 depthMVPMatrix = depthProjectionViewMatrix * modelMatrix;
                            selectionShader->setUniformValue(selectionShader->shadowQ(),
                                                             m_shadowQualityToShader);
                            selectionShader->setUniformValue(selectionShader->depth(),
                                                             depthMVPMatrix);
                            selectionShader->setUniformValue(selectionShader->lightS(),
                                                             lightStrength / 10.0f);

                            // Draw the object
                            m_drawer->drawObject(selectionShader, dotObj, gradientTexture,
                                                 m_depthTexture);
                        } else {
                            // Draw the object
                            m_drawer->drawPoint(selectionShader);
                        }
                    } else {
                        if (!drawingPoints) {
                            // Set shadowless shader bindings
                            selectionShader->setUniformValue(selectionShader->lightS(),
                                                             lightStrength);
                            // Draw the object
                            m_drawer->drawObject(selectionShader, dotObj, gradientTexture);
                        } else {
                            // Draw the object
                            m_drawer->drawPoint(selectionShader);
                        }
                    }

                    if (!drawingPoints)
                        glDisable(GL_POLYGON_OFFSET_FILL);
                }
                dotShader->bind();
            }
        }
    }

#if !defined(QT_OPENGL_ES_2)
    if (m_havePointSeries) {
        glDisable(GL_POINT_SMOOTH);
        glDisable(GL_PROGRAM_POINT_SIZE);
    }
#endif

    // Bind background shader
    m_backgroundShader->bind();

    glCullFace(GL_BACK);

    // Draw background
    if (m_cachedTheme->isBackgroundEnabled() && m_backgroundObj) {
        QMatrix4x4 modelMatrix;
        QMatrix4x4 MVPMatrix;
        QMatrix4x4 itModelMatrix;

        QVector3D bgScale(m_scaleXWithBackground, m_scaleYWithBackground,
                          m_scaleZWithBackground);
        modelMatrix.scale(bgScale);
        // If we're viewing from below, background object must be flipped
        if (m_yFlipped) {
            modelMatrix.rotate(m_xFlipRotation);
            modelMatrix.rotate(270.0f - backgroundRotation, 0.0f, 1.0f, 0.0f);
        } else {
            modelMatrix.rotate(backgroundRotation, 0.0f, 1.0f, 0.0f);
        }
        itModelMatrix = modelMatrix; // Only scaling and rotations, can be used directly

#ifdef SHOW_DEPTH_TEXTURE_SCENE
        MVPMatrix = depthProjectionViewMatrix * modelMatrix;
#else
        MVPMatrix = projectionViewMatrix * modelMatrix;
#endif
        QVector4D backgroundColor = Utils::vectorFromColor(m_cachedTheme->backgroundColor());

        // Set shader bindings
        m_backgroundShader->setUniformValue(m_backgroundShader->lightP(), lightPos);
        m_backgroundShader->setUniformValue(m_backgroundShader->view(), viewMatrix);
        m_backgroundShader->setUniformValue(m_backgroundShader->model(), modelMatrix);
        m_backgroundShader->setUniformValue(m_backgroundShader->nModel(),
                                            itModelMatrix.inverted().transposed());
        m_backgroundShader->setUniformValue(m_backgroundShader->MVP(), MVPMatrix);
        m_backgroundShader->setUniformValue(m_backgroundShader->color(), backgroundColor);
        m_backgroundShader->setUniformValue(m_backgroundShader->ambientS(),
                                            m_cachedTheme->ambientLightStrength() * 2.0f);
        m_backgroundShader->setUniformValue(m_backgroundShader->lightColor(), lightColor);

        if (m_cachedShadowQuality > QAbstract3DGraph::ShadowQualityNone && !m_isOpenGLES) {
            // Set shadow shader bindings
            QMatrix4x4 depthMVPMatrix = depthProjectionViewMatrix * modelMatrix;
            m_backgroundShader->setUniformValue(m_backgroundShader->shadowQ(),
                                                m_shadowQualityToShader);
            m_backgroundShader->setUniformValue(m_backgroundShader->depth(), depthMVPMatrix);
            m_backgroundShader->setUniformValue(m_backgroundShader->lightS(),
                                                m_cachedTheme->lightStrength() / 10.0f);

            // Draw the object
            m_drawer->drawObject(m_backgroundShader, m_backgroundObj, 0, m_depthTexture);
        } else {
            // Set shadowless shader bindings
            m_backgroundShader->setUniformValue(m_backgroundShader->lightS(),
                                                m_cachedTheme->lightStrength());

            // Draw the object
            m_drawer->drawObject(m_backgroundShader, m_backgroundObj);
        }
    }

    // Draw grid lines
    QVector3D gridLineScaleX(m_scaleXWithBackground, gridLineWidth, gridLineWidth);
    QVector3D gridLineScaleZ(gridLineWidth, gridLineWidth, m_scaleZWithBackground);
    QVector3D gridLineScaleY(gridLineWidth, m_scaleYWithBackground, gridLineWidth);

    if (m_cachedTheme->isGridEnabled()) {
        ShaderHelper *lineShader;
        if (m_isOpenGLES)
            lineShader = m_selectionShader; // Plain color shader for GL_LINES
        else
            lineShader = m_backgroundShader;

        // Bind line shader
        lineShader->bind();

        // Set unchanging shader bindings
        QVector4D lineColor = Utils::vectorFromColor(m_cachedTheme->gridLineColor());
        lineShader->setUniformValue(lineShader->lightP(), lightPos);
        lineShader->setUniformValue(lineShader->view(), viewMatrix);
        lineShader->setUniformValue(lineShader->color(), lineColor);
        lineShader->setUniformValue(lineShader->ambientS(), m_cachedTheme->ambientLightStrength());
        lineShader->setUniformValue(lineShader->lightColor(), lightColor);
        if (m_cachedShadowQuality > QAbstract3DGraph::ShadowQualityNone && !m_isOpenGLES) {
            // Set shadowed shader bindings
            lineShader->setUniformValue(lineShader->shadowQ(), m_shadowQualityToShader);
            lineShader->setUniformValue(lineShader->lightS(),
                                        m_cachedTheme->lightStrength() / 20.0f);
        } else {
            // Set shadowless shader bindings
            lineShader->setUniformValue(lineShader->lightS(),
                                        m_cachedTheme->lightStrength() / 2.5f);
        }

        QQuaternion lineYRotation;
        QQuaternion lineXRotation;

        if (m_xFlipped)
            lineYRotation = m_yRightAngleRotationNeg;
        else
            lineYRotation = m_yRightAngleRotation;

        if (m_yFlippedForGrid)
            lineXRotation = m_xRightAngleRotation;
        else
            lineXRotation = m_xRightAngleRotationNeg;

        GLfloat yFloorLinePosition = -m_scaleYWithBackground + gridLineOffset;
        if (m_yFlippedForGrid)
            yFloorLinePosition = -yFloorLinePosition;

        // Rows (= Z)
        if (m_axisCacheZ.segmentCount() > 0) {
            // Floor lines
            int gridLineCount = m_axisCacheZ.gridLineCount();
            if (m_polarGraph) {
                drawRadialGrid(lineShader, yFloorLinePosition, projectionViewMatrix,
                               depthProjectionViewMatrix);
            } else {
                for (int line = 0; line < gridLineCount; line++) {
                    QMatrix4x4 modelMatrix;
                    QMatrix4x4 MVPMatrix;
                    QMatrix4x4 itModelMatrix;

                    modelMatrix.translate(0.0f, yFloorLinePosition,
                                          m_axisCacheZ.gridLinePosition(line));

                    modelMatrix.scale(gridLineScaleX);
                    itModelMatrix.scale(gridLineScaleX);

                    modelMatrix.rotate(lineXRotation);
                    itModelMatrix.rotate(lineXRotation);

                    MVPMatrix = projectionViewMatrix * modelMatrix;

                    // Set the rest of the shader bindings
                    lineShader->setUniformValue(lineShader->model(), modelMatrix);
                    lineShader->setUniformValue(lineShader->nModel(),
                                                itModelMatrix.inverted().transposed());
                    lineShader->setUniformValue(lineShader->MVP(), MVPMatrix);

                    if (m_isOpenGLES) {
                        m_drawer->drawLine(lineShader);
                    } else {
                        if (m_cachedShadowQuality > QAbstract3DGraph::ShadowQualityNone) {
                            QMatrix4x4 depthMVPMatrix = depthProjectionViewMatrix * modelMatrix;
                            // Set shadow shader bindings
                            lineShader->setUniformValue(lineShader->depth(), depthMVPMatrix);
                            // Draw the object
                            m_drawer->drawObject(lineShader, m_gridLineObj, 0, m_depthTexture);
                        } else {
                            // Draw the object
                            m_drawer->drawObject(lineShader, m_gridLineObj);
                        }
                    }
                }

                // Side wall lines
                GLfloat lineXTrans = m_scaleXWithBackground - gridLineOffset;

                if (!m_xFlipped)
                    lineXTrans = -lineXTrans;

                for (int line = 0; line < gridLineCount; line++) {
                    QMatrix4x4 modelMatrix;
                    QMatrix4x4 MVPMatrix;
                    QMatrix4x4 itModelMatrix;

                    modelMatrix.translate(lineXTrans, 0.0f, m_axisCacheZ.gridLinePosition(line));

                    modelMatrix.scale(gridLineScaleY);
                    itModelMatrix.scale(gridLineScaleY);

                    if (m_isOpenGLES) {
                        modelMatrix.rotate(m_zRightAngleRotation);
                        itModelMatrix.rotate(m_zRightAngleRotation);
                    } else {
                        modelMatrix.rotate(lineYRotation);
                        itModelMatrix.rotate(lineYRotation);
                    }

                    MVPMatrix = projectionViewMatrix * modelMatrix;

                    // Set the rest of the shader bindings
                    lineShader->setUniformValue(lineShader->model(), modelMatrix);
                    lineShader->setUniformValue(lineShader->nModel(),
                                                itModelMatrix.inverted().transposed());
                    lineShader->setUniformValue(lineShader->MVP(), MVPMatrix);

                    if (!m_isOpenGLES) {
                        if (m_cachedShadowQuality > QAbstract3DGraph::ShadowQualityNone) {
                            // Set shadow shader bindings
                            QMatrix4x4 depthMVPMatrix = depthProjectionViewMatrix * modelMatrix;
                            lineShader->setUniformValue(lineShader->depth(), depthMVPMatrix);
                            // Draw the object
                            m_drawer->drawObject(lineShader, m_gridLineObj, 0, m_depthTexture);
                        } else {
                            // Draw the object
                            m_drawer->drawObject(lineShader, m_gridLineObj);
                        }
                    } else {
                        m_drawer->drawLine(lineShader);
                    }
                }
            }
        }

        // Columns (= X)
        if (m_axisCacheX.segmentCount() > 0) {
            if (m_isOpenGLES)
                lineXRotation = m_yRightAngleRotation;
            // Floor lines
            int gridLineCount = m_axisCacheX.gridLineCount();

            if (m_polarGraph) {
                drawAngularGrid(lineShader, yFloorLinePosition, projectionViewMatrix,
                                depthProjectionViewMatrix);
            } else {
                for (int line = 0; line < gridLineCount; line++) {
                    QMatrix4x4 modelMatrix;
                    QMatrix4x4 MVPMatrix;
                    QMatrix4x4 itModelMatrix;

                    modelMatrix.translate(m_axisCacheX.gridLinePosition(line), yFloorLinePosition,
                                          0.0f);

                    modelMatrix.scale(gridLineScaleZ);
                    itModelMatrix.scale(gridLineScaleZ);

                    modelMatrix.rotate(lineXRotation);
                    itModelMatrix.rotate(lineXRotation);

                    MVPMatrix = projectionViewMatrix * modelMatrix;

                    // Set the rest of the shader bindings
                    lineShader->setUniformValue(lineShader->model(), modelMatrix);
                    lineShader->setUniformValue(lineShader->nModel(),
                                                itModelMatrix.inverted().transposed());
                    lineShader->setUniformValue(lineShader->MVP(), MVPMatrix);

                    if (!m_isOpenGLES) {
                        if (m_cachedShadowQuality > QAbstract3DGraph::ShadowQualityNone) {
                            // Set shadow shader bindings
                            QMatrix4x4 depthMVPMatrix = depthProjectionViewMatrix * modelMatrix;
                            lineShader->setUniformValue(lineShader->depth(), depthMVPMatrix);
                            // Draw the object
                            m_drawer->drawObject(lineShader, m_gridLineObj, 0, m_depthTexture);
                        } else {
                            // Draw the object
                            m_drawer->drawObject(lineShader, m_gridLineObj);
                        }
                    } else {
                        m_drawer->drawLine(lineShader);
                    }
                }

                // Back wall lines
                GLfloat lineZTrans = m_scaleZWithBackground - gridLineOffset;

                if (!m_zFlipped)
                    lineZTrans = -lineZTrans;

                for (int line = 0; line < gridLineCount; line++) {
                    QMatrix4x4 modelMatrix;
                    QMatrix4x4 MVPMatrix;
                    QMatrix4x4 itModelMatrix;

                    modelMatrix.translate(m_axisCacheX.gridLinePosition(line), 0.0f, lineZTrans);

                    modelMatrix.scale(gridLineScaleY);
                    itModelMatrix.scale(gridLineScaleY);

                    if (m_isOpenGLES) {
                        modelMatrix.rotate(m_zRightAngleRotation);
                        itModelMatrix.rotate(m_zRightAngleRotation);
                    } else {
                        if (m_zFlipped) {
                            modelMatrix.rotate(m_xFlipRotation);
                            itModelMatrix.rotate(m_xFlipRotation);
                        }
                    }

                    MVPMatrix = projectionViewMatrix * modelMatrix;

                    // Set the rest of the shader bindings
                    lineShader->setUniformValue(lineShader->model(), modelMatrix);
                    lineShader->setUniformValue(lineShader->nModel(),
                                                itModelMatrix.inverted().transposed());
                    lineShader->setUniformValue(lineShader->MVP(), MVPMatrix);

                    if (!m_isOpenGLES) {
                        if (m_cachedShadowQuality > QAbstract3DGraph::ShadowQualityNone) {
                            // Set shadow shader bindings
                            QMatrix4x4 depthMVPMatrix = depthProjectionViewMatrix * modelMatrix;
                            lineShader->setUniformValue(lineShader->depth(), depthMVPMatrix);
                            // Draw the object
                            m_drawer->drawObject(lineShader, m_gridLineObj, 0, m_depthTexture);
                        } else {
                            // Draw the object
                            m_drawer->drawObject(lineShader, m_gridLineObj);
                        }
                    } else {
                        m_drawer->drawLine(lineShader);
                    }
                }
            }
        }

        // Horizontal wall lines
        if (m_axisCacheY.segmentCount() > 0) {
            // Back wall
            int gridLineCount = m_axisCacheY.gridLineCount();

            GLfloat lineZTrans = m_scaleZWithBackground - gridLineOffset;

            if (!m_zFlipped)
                lineZTrans = -lineZTrans;

            for (int line = 0; line < gridLineCount; line++) {
                QMatrix4x4 modelMatrix;
                QMatrix4x4 MVPMatrix;
                QMatrix4x4 itModelMatrix;

                modelMatrix.translate(0.0f, m_axisCacheY.gridLinePosition(line), lineZTrans);

                modelMatrix.scale(gridLineScaleX);
                itModelMatrix.scale(gridLineScaleX);

                if (m_zFlipped) {
                    modelMatrix.rotate(m_xFlipRotation);
                    itModelMatrix.rotate(m_xFlipRotation);
                }

                MVPMatrix = projectionViewMatrix * modelMatrix;

                // Set the rest of the shader bindings
                lineShader->setUniformValue(lineShader->model(), modelMatrix);
                lineShader->setUniformValue(lineShader->nModel(),
                                            itModelMatrix.inverted().transposed());
                lineShader->setUniformValue(lineShader->MVP(), MVPMatrix);

                if (!m_isOpenGLES) {
                    if (m_cachedShadowQuality > QAbstract3DGraph::ShadowQualityNone) {
                        // Set shadow shader bindings
                        QMatrix4x4 depthMVPMatrix = depthProjectionViewMatrix * modelMatrix;
                        lineShader->setUniformValue(lineShader->depth(), depthMVPMatrix);
                        // Draw the object
                        m_drawer->drawObject(lineShader, m_gridLineObj, 0, m_depthTexture);
                    } else {
                        // Draw the object
                        m_drawer->drawObject(lineShader, m_gridLineObj);
                    }
                } else {
                    m_drawer->drawLine(lineShader);
                }
            }

            // Side wall
            GLfloat lineXTrans = m_scaleXWithBackground - gridLineOffset;

            if (!m_xFlipped)
                lineXTrans = -lineXTrans;

            for (int line = 0; line < gridLineCount; line++) {
                QMatrix4x4 modelMatrix;
                QMatrix4x4 MVPMatrix;
                QMatrix4x4 itModelMatrix;

                modelMatrix.translate(lineXTrans, m_axisCacheY.gridLinePosition(line), 0.0f);

                modelMatrix.scale(gridLineScaleZ);
                itModelMatrix.scale(gridLineScaleZ);

                modelMatrix.rotate(lineYRotation);
                itModelMatrix.rotate(lineYRotation);

                MVPMatrix = projectionViewMatrix * modelMatrix;

                // Set the rest of the shader bindings
                lineShader->setUniformValue(lineShader->model(), modelMatrix);
                lineShader->setUniformValue(lineShader->nModel(),
                                            itModelMatrix.inverted().transposed());
                lineShader->setUniformValue(lineShader->MVP(), MVPMatrix);

                if (!m_isOpenGLES) {
                    if (m_cachedShadowQuality > QAbstract3DGraph::ShadowQualityNone) {
                        // Set shadow shader bindings
                        QMatrix4x4 depthMVPMatrix = depthProjectionViewMatrix * modelMatrix;
                        lineShader->setUniformValue(lineShader->depth(), depthMVPMatrix);
                        // Draw the object
                        m_drawer->drawObject(lineShader, m_gridLineObj, 0, m_depthTexture);
                    } else {
                        // Draw the object
                        m_drawer->drawObject(lineShader, m_gridLineObj);
                    }
                } else {
                    m_drawer->drawLine(lineShader);
                }
            }
        }
    }

    Abstract3DRenderer::drawCustomItems(RenderingNormal, m_customItemShader, viewMatrix,
                                        projectionViewMatrix, depthProjectionViewMatrix,
                                        m_depthTexture, m_shadowQualityToShader);

    drawLabels(false, activeCamera, viewMatrix, projectionMatrix);

    // Handle selection clearing and selection label drawing
    if (!dotSelectionFound) {
        // We have no ownership, don't delete. Just NULL the pointer.
        m_selectedItem = NULL;
    } else {
        glDisable(GL_DEPTH_TEST);
        // Draw the selection label
        LabelItem &labelItem = selectionLabelItem();
        if (m_selectedItem != selectedItem || m_updateLabels
                || !labelItem.textureId() || m_selectionLabelDirty) {
            QString labelText = selectionLabel();
            if (labelText.isNull() || m_selectionLabelDirty) {
                labelText = m_selectedSeriesCache->itemLabel();
                setSelectionLabel(labelText);
                m_selectionLabelDirty = false;
            }
            m_drawer->generateLabelItem(labelItem, labelText);
            m_selectedItem = selectedItem;
        }

        m_drawer->drawLabel(*selectedItem, labelItem, viewMatrix, projectionMatrix,
                            zeroVector, identityQuaternion, selectedItemSize, m_cachedSelectionMode,
                            m_labelShader, m_labelObj, activeCamera, true, false,
                            Drawer::LabelOver);

        // Reset label update flag; they should have been updated when we get here
        m_updateLabels = false;
        glEnable(GL_DEPTH_TEST);
    }

    glDisable(GL_BLEND);

    // Release shader
    glUseProgram(0);

    m_selectionDirty = false;
}

void Scatter3DRenderer::drawLabels(bool drawSelection, const Q3DCamera *activeCamera,
                                   const QMatrix4x4 &viewMatrix,
                                   const QMatrix4x4 &projectionMatrix) {
    ShaderHelper *shader = 0;
    GLfloat alphaForValueSelection = labelValueAlpha / 255.0f;
    GLfloat alphaForRowSelection = labelRowAlpha / 255.0f;
    GLfloat alphaForColumnSelection = labelColumnAlpha / 255.0f;
    if (drawSelection) {
        shader = m_selectionShader;
        // m_selectionShader is already bound
    } else {
        shader = m_labelShader;
        shader->bind();

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    glEnable(GL_POLYGON_OFFSET_FILL);

    float labelAutoAngle = m_axisCacheZ.labelAutoRotation();
    float labelAngleFraction = labelAutoAngle / 90.0f;
    float fractionCamY = activeCamera->yRotation() * labelAngleFraction;
    float fractionCamX = activeCamera->xRotation() * labelAngleFraction;
    float labelsMaxWidth = 0.0f;

    int startIndex;
    int endIndex;
    int indexStep;

    // Z Labels
    if (m_axisCacheZ.segmentCount() > 0) {
        int labelCount = m_axisCacheZ.labelCount();
        float labelXTrans = m_scaleXWithBackground + labelMargin;
        float labelYTrans = -m_scaleYWithBackground;
        if (m_polarGraph) {
            labelXTrans *= m_radialLabelOffset;
            // YTrans up only if over background
            if (m_radialLabelOffset < 1.0f)
                labelYTrans += gridLineOffset + gridLineWidth;
        }
        Qt::AlignmentFlag alignment = (m_xFlipped == m_zFlipped) ? Qt::AlignLeft : Qt::AlignRight;
        QVector3D labelRotation;
        if (m_xFlipped)
            labelXTrans = -labelXTrans;
        if (m_yFlipped)
            labelYTrans = -labelYTrans;
        if (labelAutoAngle == 0.0f) {
            if (m_zFlipped)
                labelRotation.setY(180.0f);
            if (m_yFlippedForGrid) {
                if (m_zFlipped)
                    labelRotation.setY(180.0f);
                else
                    labelRotation.setY(0.0f);
                labelRotation.setX(90.0f);
            } else {
                labelRotation.setX(-90.0f);
            }
        } else {
            if (m_zFlipped)
                labelRotation.setY(180.0f);
            if (m_yFlippedForGrid) {
                if (m_zFlipped) {
                    if (m_xFlipped) {
                        labelRotation.setX(90.0f - (labelAutoAngle - fractionCamX)
                                           * (-labelAutoAngle - fractionCamY) / labelAutoAngle);
                        labelRotation.setZ(labelAutoAngle + fractionCamY);
                    } else {
                        labelRotation.setX(90.0f + (labelAutoAngle + fractionCamX)
                                           * (labelAutoAngle + fractionCamY) / labelAutoAngle);
                        labelRotation.setZ(-labelAutoAngle - fractionCamY);
                    }
                } else {
                    if (m_xFlipped) {
                        labelRotation.setX(90.0f + (labelAutoAngle - fractionCamX)
                                           * -(labelAutoAngle + fractionCamY) / labelAutoAngle);
                        labelRotation.setZ(-labelAutoAngle - fractionCamY);
                    } else {
                        labelRotation.setX(90.0f - (labelAutoAngle + fractionCamX)
                                           * (labelAutoAngle + fractionCamY) / labelAutoAngle);
                        labelRotation.setZ(labelAutoAngle + fractionCamY);
                    }
                }
            } else {
                if (m_zFlipped) {
                    if (m_xFlipped) {
                        labelRotation.setX(-90.0f + (labelAutoAngle - fractionCamX)
                                           * (-labelAutoAngle + fractionCamY) / labelAutoAngle);
                        labelRotation.setZ(-labelAutoAngle + fractionCamY);
                    } else {
                        labelRotation.setX(-90.0f - (labelAutoAngle + fractionCamX)
                                           * (labelAutoAngle - fractionCamY) / labelAutoAngle);
                        labelRotation.setZ(labelAutoAngle - fractionCamY);
                    }
                } else {
                    if (m_xFlipped) {
                        labelRotation.setX(-90.0f - (labelAutoAngle - fractionCamX)
                                           * (-labelAutoAngle + fractionCamY) / labelAutoAngle);
                        labelRotation.setZ(labelAutoAngle - fractionCamY);
                    } else {
                        labelRotation.setX(-90.0f + (labelAutoAngle + fractionCamX)
                                           * (labelAutoAngle - fractionCamY) / labelAutoAngle);
                        labelRotation.setZ(-labelAutoAngle + fractionCamY);
                    }
                }
            }
        }
        QQuaternion totalRotation = Utils::calculateRotation(labelRotation);
        QVector3D labelTrans = QVector3D(labelXTrans, labelYTrans, 0.0f);
        if (m_zFlipped) {
            startIndex = 0;
            endIndex = labelCount;
            indexStep = 1;
        } else {
            startIndex = labelCount - 1;
            endIndex = -1;
            indexStep = -1;
        }
        float offsetValue = 0.0f;
        for (int label = startIndex; label != endIndex; label = label + indexStep) {
            glPolygonOffset(offsetValue++ / -10.0f, 1.0f);
            const LabelItem &axisLabelItem = *m_axisCacheZ.labelItems().at(label);
            // Draw the label here
            if (m_polarGraph) {
                float direction = m_zFlipped ? -1.0f : 1.0f;
                labelTrans.setZ((m_axisCacheZ.formatter()->labelPositions().at(label)
                                 * -m_polarRadius
                                 + m_drawer->scaledFontSize() + gridLineWidth) * direction);
            } else {
                labelTrans.setZ(m_axisCacheZ.labelPosition(label));
            }
            if (label == 0 || label == (labelCount - 1)) {
                // If the margin is small, adjust the position of the edge labels to avoid overlapping
                // with labels of the other axes.
                float scaleFactor = m_drawer->scaledFontSize() / axisLabelItem.size().height();
                float labelOverlap = qAbs(labelTrans.z())
                        + (scaleFactor * axisLabelItem.size().height() / 2.0f)
                        - m_scaleZWithBackground + labelMargin;
                // No need to adjust quite as much on the front edges
                if (label != startIndex)
                    labelOverlap /= 2.0f;
                if (labelOverlap > 0.0f) {
                    if (label == 0)
                        labelTrans.setZ(labelTrans.z() - labelOverlap);
                    else
                        labelTrans.setZ(labelTrans.z() + labelOverlap);
                }
            }
            m_dummyRenderItem.setTranslation(labelTrans);

            if (drawSelection) {
                QVector4D labelColor = QVector4D(label / 255.0f, 0.0f, 0.0f,
                                                 alphaForRowSelection);
                shader->setUniformValue(shader->color(), labelColor);
            }

            m_drawer->drawLabel(m_dummyRenderItem, axisLabelItem, viewMatrix, projectionMatrix,
                                zeroVector, totalRotation, 0, m_cachedSelectionMode,
                                shader, m_labelObj, activeCamera, true, true,
                                Drawer::LabelMid, alignment, false, drawSelection);
            labelsMaxWidth = qMax(labelsMaxWidth, float(axisLabelItem.size().width()));
        }
        if (!drawSelection && m_axisCacheZ.isTitleVisible()) {
            if (m_polarGraph) {
                float titleZ = -m_polarRadius / 2.0f;
                if (m_zFlipped)
                    titleZ = -titleZ;
                labelTrans.setZ(titleZ);
            } else {
                labelTrans.setZ(0.0f);
            }
            drawAxisTitleZ(labelRotation, labelTrans, totalRotation, m_dummyRenderItem,
                           activeCamera, labelsMaxWidth, viewMatrix, projectionMatrix, shader);
        }
    }

    // X Labels
    if (m_axisCacheX.segmentCount() > 0) {
        labelsMaxWidth = 0.0f;
        labelAutoAngle = m_axisCacheX.labelAutoRotation();
        labelAngleFraction = labelAutoAngle / 90.0f;
        fractionCamY = activeCamera->yRotation() * labelAngleFraction;
        fractionCamX = activeCamera->xRotation() * labelAngleFraction;
        int labelCount = m_axisCacheX.labelCount();
        float labelZTrans = 0.0f;
        float labelYTrans = -m_scaleYWithBackground;
        if (m_polarGraph)
            labelYTrans += gridLineOffset + gridLineWidth;
        else
            labelZTrans = m_scaleZWithBackground + labelMargin;

        Qt::AlignmentFlag alignment = (m_xFlipped != m_zFlipped) ? Qt::AlignLeft : Qt::AlignRight;
        QVector3D labelRotation;
        if (m_zFlipped)
            labelZTrans = -labelZTrans;
        if (m_yFlipped)
            labelYTrans = -labelYTrans;
        if (labelAutoAngle == 0.0f) {
            labelRotation = QVector3D(-90.0f, 90.0f, 0.0f);
            if (m_xFlipped)
                labelRotation.setY(-90.0f);
            if (m_yFlippedForGrid) {
                if (m_xFlipped)
                    labelRotation.setY(-90.0f);
                else
                    labelRotation.setY(90.0f);
                labelRotation.setX(90.0f);
            }
        } else {
            if (m_xFlipped)
                labelRotation.setY(-90.0f);
            else
                labelRotation.setY(90.0f);
            if (m_yFlippedForGrid) {
                if (m_zFlipped) {
                    if (m_xFlipped) {
                        labelRotation.setX(90.0f - (2.0f * labelAutoAngle - fractionCamX)
                                           * (labelAutoAngle + fractionCamY) / labelAutoAngle);
                        labelRotation.setZ(-labelAutoAngle - fractionCamY);
                    } else {
                        labelRotation.setX(90.0f - (2.0f * labelAutoAngle + fractionCamX)
                                           * (labelAutoAngle + fractionCamY) / labelAutoAngle);
                        labelRotation.setZ(labelAutoAngle + fractionCamY);
                    }
                } else {
                    if (m_xFlipped) {
                        labelRotation.setX(90.0f + fractionCamX
                                           * -(labelAutoAngle + fractionCamY) / labelAutoAngle);
                        labelRotation.setZ(labelAutoAngle + fractionCamY);
                    } else {
                        labelRotation.setX(90.0f - fractionCamX
                                           * (-labelAutoAngle - fractionCamY) / labelAutoAngle);
                        labelRotation.setZ(-labelAutoAngle - fractionCamY);
                    }
                }
            } else {
                if (m_zFlipped) {
                    if (m_xFlipped) {
                        labelRotation.setX(-90.0f + (2.0f * labelAutoAngle - fractionCamX)
                                           * (labelAutoAngle - fractionCamY) / labelAutoAngle);
                        labelRotation.setZ(labelAutoAngle - fractionCamY);
                    } else {
                        labelRotation.setX(-90.0f + (2.0f * labelAutoAngle + fractionCamX)
                                           * (labelAutoAngle - fractionCamY) / labelAutoAngle);
                        labelRotation.setZ(-labelAutoAngle + fractionCamY);
                    }
                } else {
                    if (m_xFlipped) {
                        labelRotation.setX(-90.0f - fractionCamX
                                           * (-labelAutoAngle + fractionCamY) / labelAutoAngle);
                        labelRotation.setZ(-labelAutoAngle + fractionCamY);
                    } else {
                        labelRotation.setX(-90.0f + fractionCamX
                                           * -(labelAutoAngle - fractionCamY) / labelAutoAngle);
                        labelRotation.setZ(labelAutoAngle - fractionCamY);
                    }
                }
            }
        }

        QQuaternion totalRotation = Utils::calculateRotation(labelRotation);
        if (m_polarGraph) {
            if ((!m_yFlippedForGrid && (m_zFlipped != m_xFlipped))
                    || (m_yFlippedForGrid && (m_zFlipped == m_xFlipped))) {
                totalRotation *= m_zRightAngleRotation;
            } else {
                totalRotation *= m_zRightAngleRotationNeg;
            }
        }
        QVector3D labelTrans = QVector3D(0.0f, labelYTrans, labelZTrans);
        if (m_xFlipped) {
            startIndex = labelCount - 1;
            endIndex = -1;
            indexStep = -1;
        } else {
            startIndex = 0;
            endIndex = labelCount;
            indexStep = 1;
        }
        float offsetValue = 0.0f;
        bool showLastLabel = false;
        QVector<float> &labelPositions = m_axisCacheX.formatter()->labelPositions();
        int lastLabelPosIndex = labelPositions.size() - 1;
        if (labelPositions.size()
                && (labelPositions.at(lastLabelPosIndex) != 1.0f || labelPositions.at(0) != 0.0f)) {
            // Avoid overlapping first and last label if they would get on same position
            showLastLabel = true;
        }

        for (int label = startIndex; label != endIndex; label = label + indexStep) {
            glPolygonOffset(offsetValue++ / -10.0f, 1.0f);
            // Draw the label here
            if (m_polarGraph) {
                // Calculate angular position
                if (label == lastLabelPosIndex && !showLastLabel)
                    continue;
                float labelPosition = labelPositions.at(label);
                qreal angle = labelPosition * M_PI * 2.0;
                labelTrans.setX((m_polarRadius + labelMargin) * float(qSin(angle)));
                labelTrans.setZ(-(m_polarRadius + labelMargin) * float(qCos(angle)));
                // Alignment depends on label angular position, as well as flips
                Qt::AlignmentFlag vAlignment = Qt::AlignCenter;
                Qt::AlignmentFlag hAlignment = Qt::AlignCenter;
                const float centerMargin = 0.005f;
                if (labelPosition < 0.25f - centerMargin || labelPosition > 0.75f + centerMargin)
                    vAlignment = m_zFlipped ? Qt::AlignTop : Qt::AlignBottom;
                else if (labelPosition > 0.25f + centerMargin && labelPosition < 0.75f - centerMargin)
                    vAlignment = m_zFlipped ? Qt::AlignBottom : Qt::AlignTop;

                if (labelPosition < 0.50f - centerMargin && labelPosition > centerMargin)
                    hAlignment = m_zFlipped ? Qt::AlignRight : Qt::AlignLeft;
                else if (labelPosition < 1.0f - centerMargin && labelPosition > 0.5f + centerMargin)
                    hAlignment = m_zFlipped ? Qt::AlignLeft : Qt::AlignRight;
                if (m_yFlippedForGrid && vAlignment != Qt::AlignCenter)
                    vAlignment = (vAlignment == Qt::AlignTop) ? Qt::AlignBottom : Qt::AlignTop;
                alignment = Qt::AlignmentFlag(vAlignment | hAlignment);
            } else {
                labelTrans.setX(m_axisCacheX.labelPosition(label));
            }
            const LabelItem &axisLabelItem = *m_axisCacheX.labelItems().at(label);
            if (label == 0 || label == (labelCount - 1)) {
                // If the margin is small, adjust the position of the edge labels to avoid overlapping
                // with labels of the other axes.
                float scaleFactor = m_drawer->scaledFontSize() / axisLabelItem.size().height();
                float labelOverlap = qAbs(labelTrans.x())
                        + (scaleFactor * axisLabelItem.size().height() / 2.0f)
                        - m_scaleXWithBackground + labelMargin;
                // No need to adjust quite as much on the front edges
                if (label != startIndex)
                    labelOverlap /= 2.0f;
                if (labelOverlap > 0.0f) {
                    if (label == 0)
                        labelTrans.setX(labelTrans.x() + labelOverlap);
                    else
                        labelTrans.setX(labelTrans.x() - labelOverlap);
                }
            }
            m_dummyRenderItem.setTranslation(labelTrans);

            if (drawSelection) {
                QVector4D labelColor = QVector4D(0.0f, label / 255.0f, 0.0f,
                                                 alphaForColumnSelection);
                shader->setUniformValue(shader->color(), labelColor);
            }

            m_drawer->drawLabel(m_dummyRenderItem, axisLabelItem, viewMatrix, projectionMatrix,
                                zeroVector, totalRotation, 0, m_cachedSelectionMode,
                                shader, m_labelObj, activeCamera, true, true,
                                Drawer::LabelMid, alignment, false, drawSelection);
            labelsMaxWidth = qMax(labelsMaxWidth, float(axisLabelItem.size().width()));
        }
        if (!drawSelection && m_axisCacheX.isTitleVisible()) {
            labelTrans.setX(0.0f);
            bool radial = false;
            if (m_polarGraph) {
                if (m_xFlipped == m_zFlipped)
                    totalRotation *= m_zRightAngleRotation;
                else
                    totalRotation *= m_zRightAngleRotationNeg;
                if (m_yFlippedForGrid)
                    totalRotation *= QQuaternion::fromAxisAndAngle(0.0f, 0.0f, 1.0f, -180.0f);
                labelTrans.setZ(-m_polarRadius);
                radial = true;
            }
            drawAxisTitleX(labelRotation, labelTrans, totalRotation, m_dummyRenderItem,
                           activeCamera, labelsMaxWidth, viewMatrix, projectionMatrix, shader,
                           radial);
        }
    }

    // Y Labels
    if (m_axisCacheY.segmentCount() > 0) {
        labelsMaxWidth = 0.0f;
        labelAutoAngle = m_axisCacheY.labelAutoRotation();
        labelAngleFraction = labelAutoAngle / 90.0f;
        fractionCamY = activeCamera->yRotation() * labelAngleFraction;
        fractionCamX = activeCamera->xRotation() * labelAngleFraction;
        int labelCount = m_axisCacheY.labelCount();

        float labelXTrans = m_scaleXWithBackground;
        float labelZTrans = m_scaleZWithBackground;

        // Back & side wall
        float labelMarginXTrans = labelMargin;
        float labelMarginZTrans = labelMargin;
        QVector3D backLabelRotation(0.0f, -90.0f, 0.0f);
        QVector3D sideLabelRotation(0.0f, 0.0f, 0.0f);
        Qt::AlignmentFlag backAlignment =
                (m_xFlipped != m_zFlipped) ? Qt::AlignLeft : Qt::AlignRight;
        Qt::AlignmentFlag sideAlignment =
                (m_xFlipped == m_zFlipped) ? Qt::AlignLeft : Qt::AlignRight;
        if (!m_xFlipped) {
            labelXTrans = -labelXTrans;
            labelMarginXTrans = -labelMargin;
        }
        if (m_zFlipped) {
            labelZTrans = -labelZTrans;
            labelMarginZTrans = -labelMargin;
        }
        if (labelAutoAngle == 0.0f) {
            if (!m_xFlipped)
                backLabelRotation.setY(90.0f);
            if (m_zFlipped)
                sideLabelRotation.setY(180.f);
        } else {
            // Orient side labels somewhat towards the camera
            if (m_xFlipped) {
                if (m_zFlipped)
                    sideLabelRotation.setY(180.0f + (2.0f * labelAutoAngle) - fractionCamX);
                else
                    sideLabelRotation.setY(-fractionCamX);
                backLabelRotation.setY(-90.0f + labelAutoAngle - fractionCamX);
            } else {
                if (m_zFlipped)
                    sideLabelRotation.setY(180.0f - (2.0f * labelAutoAngle) - fractionCamX);
                else
                    sideLabelRotation.setY(-fractionCamX);
                backLabelRotation.setY(90.0f - labelAutoAngle - fractionCamX);
            }
        }
        sideLabelRotation.setX(-fractionCamY);
        backLabelRotation.setX(-fractionCamY);

        QQuaternion totalSideRotation = Utils::calculateRotation(sideLabelRotation);
        QQuaternion totalBackRotation = Utils::calculateRotation(backLabelRotation);

        QVector3D labelTransBack = QVector3D(labelXTrans, 0.0f, labelZTrans + labelMarginZTrans);
        QVector3D labelTransSide(-labelXTrans - labelMarginXTrans, 0.0f, -labelZTrans);

        if (m_yFlipped) {
            startIndex = labelCount - 1;
            endIndex = -1;
            indexStep = -1;
        } else {
            startIndex = 0;
            endIndex = labelCount;
            indexStep = 1;
        }
        float offsetValue = 0.0f;
        for (int label = startIndex; label != endIndex; label = label + indexStep) {
            const LabelItem &axisLabelItem = *m_axisCacheY.labelItems().at(label);
            float labelYTrans = m_axisCacheY.labelPosition(label);

            glPolygonOffset(offsetValue++ / -10.0f, 1.0f);

            if (drawSelection) {
                QVector4D labelColor = QVector4D(0.0f, 0.0f, label / 255.0f,
                                                 alphaForValueSelection);
                shader->setUniformValue(shader->color(), labelColor);
            }

            if (label == startIndex) {
                // If the margin is small, adjust the position of the edge label to avoid
                // overlapping with labels of the other axes.
                float scaleFactor = m_drawer->scaledFontSize() / axisLabelItem.size().height();
                float labelOverlap = qAbs(labelYTrans)
                        + (scaleFactor * axisLabelItem.size().height() / 2.0f)
                        - m_scaleYWithBackground + labelMargin;
                if (labelOverlap > 0.0f) {
                    if (label == 0)
                        labelYTrans += labelOverlap;
                    else
                        labelYTrans -= labelOverlap;
                }
            }

            // Back wall
            labelTransBack.setY(labelYTrans);
            m_dummyRenderItem.setTranslation(labelTransBack);
            m_drawer->drawLabel(m_dummyRenderItem, axisLabelItem, viewMatrix, projectionMatrix,
                                zeroVector, totalBackRotation, 0, m_cachedSelectionMode,
                                shader, m_labelObj, activeCamera, true, true,
                                Drawer::LabelMid, backAlignment, false, drawSelection);

            // Side wall
            labelTransSide.setY(labelYTrans);
            m_dummyRenderItem.setTranslation(labelTransSide);
            m_drawer->drawLabel(m_dummyRenderItem, axisLabelItem, viewMatrix, projectionMatrix,
                                zeroVector, totalSideRotation, 0, m_cachedSelectionMode,
                                shader, m_labelObj, activeCamera, true, true,
                                Drawer::LabelMid, sideAlignment, false, drawSelection);
            labelsMaxWidth = qMax(labelsMaxWidth, float(axisLabelItem.size().width()));
        }
        if (!drawSelection && m_axisCacheY.isTitleVisible()) {
            labelTransSide.setY(0.0f);
            labelTransBack.setY(0.0f);
            drawAxisTitleY(sideLabelRotation, backLabelRotation, labelTransSide, labelTransBack,
                           totalSideRotation, totalBackRotation, m_dummyRenderItem, activeCamera,
                           labelsMaxWidth, viewMatrix, projectionMatrix,
                           shader);
        }
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
}

void Scatter3DRenderer::updateSelectedItem(int index, QScatter3DSeries *series)
{
    m_selectionDirty = true;
    m_selectionLabelDirty = true;
    m_selectedSeriesCache =
            static_cast<ScatterSeriesRenderCache *>(m_renderCacheList.value(series, 0));
    m_selectedItemIndex = Scatter3DController::invalidSelectionIndex();

    if (m_cachedOptimizationHint.testFlag(QAbstract3DGraph::OptimizationStatic)
            && m_oldSelectedSeriesCache
            && m_oldSelectedSeriesCache->mesh() == QAbstract3DSeries::MeshPoint) {
        m_oldSelectedSeriesCache->bufferPoints()->popPoint();
        m_oldSelectedSeriesCache = 0;
    }

    if (m_selectedSeriesCache) {
        const ScatterRenderItemArray &renderArray = m_selectedSeriesCache->renderArray();
        if (index < renderArray.size() && index >= 0) {
            m_selectedItemIndex = index;

            if (m_cachedOptimizationHint.testFlag(QAbstract3DGraph::OptimizationStatic)
                    && m_selectedSeriesCache->mesh() == QAbstract3DSeries::MeshPoint) {
                m_selectedSeriesCache->bufferPoints()->pushPoint(m_selectedItemIndex);
                m_oldSelectedSeriesCache = m_selectedSeriesCache;
            }
        }
    }
}

void Scatter3DRenderer::updateShadowQuality(QAbstract3DGraph::ShadowQuality quality)
{
    m_cachedShadowQuality = quality;
    switch (quality) {
    case QAbstract3DGraph::ShadowQualityLow:
        m_shadowQualityToShader = 33.3f;
        m_shadowQualityMultiplier = 1;
        break;
    case QAbstract3DGraph::ShadowQualityMedium:
        m_shadowQualityToShader = 100.0f;
        m_shadowQualityMultiplier = 3;
        break;
    case QAbstract3DGraph::ShadowQualityHigh:
        m_shadowQualityToShader = 200.0f;
        m_shadowQualityMultiplier = 5;
        break;
    case QAbstract3DGraph::ShadowQualitySoftLow:
        m_shadowQualityToShader = 5.0f;
        m_shadowQualityMultiplier = 1;
        break;
    case QAbstract3DGraph::ShadowQualitySoftMedium:
        m_shadowQualityToShader = 10.0f;
        m_shadowQualityMultiplier = 3;
        break;
    case QAbstract3DGraph::ShadowQualitySoftHigh:
        m_shadowQualityToShader = 15.0f;
        m_shadowQualityMultiplier = 4;
        break;
    default:
        m_shadowQualityToShader = 0.0f;
        m_shadowQualityMultiplier = 1;
        break;
    }

    handleShadowQualityChange();

    // Re-init depth buffer
    updateDepthBuffer();
}

void Scatter3DRenderer::loadBackgroundMesh()
{
    ObjectHelper::resetObjectHelper(this, m_backgroundObj,
                                    QStringLiteral(":/defaultMeshes/background"));
}

void Scatter3DRenderer::updateTextures()
{
    Abstract3DRenderer::updateTextures();

    // Drawer has changed; this flag needs to be checked when checking if we need to update labels
    m_updateLabels = true;

    if (m_polarGraph)
        calculateSceneScalingFactors();
}

void Scatter3DRenderer::fixMeshFileName(QString &fileName, QAbstract3DSeries::Mesh mesh)
{
    // Load full version of meshes that have it available
    if (mesh != QAbstract3DSeries::MeshSphere
            && mesh != QAbstract3DSeries::MeshMinimal
            && mesh != QAbstract3DSeries::MeshPoint
            && mesh != QAbstract3DSeries::MeshArrow) {
        fileName.append(QStringLiteral("Full"));
    }
}

void Scatter3DRenderer::calculateTranslation(ScatterRenderItem &item)
{
    // We need to normalize translations
    const QVector3D &pos = item.position();
    float xTrans;
    float yTrans = m_axisCacheY.positionAt(pos.y());
    float zTrans;
    if (m_polarGraph) {
        calculatePolarXZ(pos, xTrans, zTrans);
    } else {
        xTrans = m_axisCacheX.positionAt(pos.x());
        zTrans = m_axisCacheZ.positionAt(pos.z());
    }
    item.setTranslation(QVector3D(xTrans, yTrans, zTrans));
}

void Scatter3DRenderer::calculateSceneScalingFactors()
{
    if (m_requestedMargin < 0.0f) {
        if (m_maxItemSize > defaultMaxSize)
            m_hBackgroundMargin = m_maxItemSize / itemScaler;
        else
            m_hBackgroundMargin = defaultMaxSize;
        m_vBackgroundMargin = m_hBackgroundMargin;
    } else {
        m_hBackgroundMargin = m_requestedMargin;
        m_vBackgroundMargin = m_requestedMargin;
    }
    if (m_polarGraph) {
        float polarMargin = calculatePolarBackgroundMargin();
        m_hBackgroundMargin = qMax(m_hBackgroundMargin, polarMargin);
    }

    float horizontalAspectRatio;
    if (m_polarGraph)
        horizontalAspectRatio = 1.0f;
    else
        horizontalAspectRatio = m_graphHorizontalAspectRatio;

    QSizeF areaSize;
    if (horizontalAspectRatio == 0.0f) {
        areaSize.setHeight(m_axisCacheZ.max() -  m_axisCacheZ.min());
        areaSize.setWidth(m_axisCacheX.max() - m_axisCacheX.min());
    } else {
        areaSize.setHeight(1.0f);
        areaSize.setWidth(horizontalAspectRatio);
    }

    float horizontalMaxDimension;
    if (m_graphAspectRatio > 2.0f) {
        horizontalMaxDimension = 2.0f;
        m_scaleY = 2.0f / m_graphAspectRatio;
    } else {
        horizontalMaxDimension = m_graphAspectRatio;
        m_scaleY = 1.0f;
    }
    if (m_polarGraph)
        m_polarRadius = horizontalMaxDimension;

    float scaleFactor = qMax(areaSize.width(), areaSize.height());
    m_scaleX = horizontalMaxDimension * areaSize.width() / scaleFactor;
    m_scaleZ = horizontalMaxDimension * areaSize.height() / scaleFactor;

    m_scaleXWithBackground = m_scaleX + m_hBackgroundMargin;
    m_scaleYWithBackground = m_scaleY + m_vBackgroundMargin;
    m_scaleZWithBackground = m_scaleZ + m_hBackgroundMargin;

    m_axisCacheX.setScale(m_scaleX * 2.0f);
    m_axisCacheY.setScale(m_scaleY * 2.0f);
    m_axisCacheZ.setScale(-m_scaleZ * 2.0f);
    m_axisCacheX.setTranslate(-m_scaleX);
    m_axisCacheY.setTranslate(-m_scaleY);
    m_axisCacheZ.setTranslate(m_scaleZ);

    updateCameraViewport();
    updateCustomItemPositions();
}

void Scatter3DRenderer::initShaders(const QString &vertexShader, const QString &fragmentShader)
{
    delete m_dotShader;
    m_dotShader = new ShaderHelper(this, vertexShader, fragmentShader);
    m_dotShader->initialize();
}

void Scatter3DRenderer::initGradientShaders(const QString &vertexShader,
                                            const QString &fragmentShader)
{
    delete m_dotGradientShader;
    m_dotGradientShader = new ShaderHelper(this, vertexShader, fragmentShader);
    m_dotGradientShader->initialize();

}

void Scatter3DRenderer::initStaticSelectedItemShaders(const QString &vertexShader,
                                                      const QString &fragmentShader,
                                                      const QString &gradientVertexShader,
                                                      const QString &gradientFragmentShader)
{
    delete m_staticSelectedItemShader;
    m_staticSelectedItemShader = new ShaderHelper(this, vertexShader, fragmentShader);
    m_staticSelectedItemShader->initialize();

    delete m_staticSelectedItemGradientShader;
    m_staticSelectedItemGradientShader = new ShaderHelper(this, gradientVertexShader,
                                                          gradientFragmentShader);
    m_staticSelectedItemGradientShader->initialize();
}

void Scatter3DRenderer::initSelectionShader()
{
    delete m_selectionShader;
    m_selectionShader = new ShaderHelper(this, QStringLiteral(":/shaders/vertexPlainColor"),
                                         QStringLiteral(":/shaders/fragmentPlainColor"));
    m_selectionShader->initialize();
}

void Scatter3DRenderer::initSelectionBuffer()
{
    m_textureHelper->deleteTexture(&m_selectionTexture);

    if (m_primarySubViewport.size().isEmpty())
        return;

    m_selectionTexture = m_textureHelper->createSelectionTexture(m_primarySubViewport.size(),
                                                                 m_selectionFrameBuffer,
                                                                 m_selectionDepthBuffer);
}

void Scatter3DRenderer::initDepthShader()
{
    if (!m_isOpenGLES) {
        if (m_depthShader)
            delete m_depthShader;
        m_depthShader = new ShaderHelper(this, QStringLiteral(":/shaders/vertexDepth"),
                                         QStringLiteral(":/shaders/fragmentDepth"));
        m_depthShader->initialize();
    }
}

void Scatter3DRenderer::updateDepthBuffer()
{
    if (!m_isOpenGLES) {
        m_textureHelper->deleteTexture(&m_depthTexture);

        if (m_primarySubViewport.size().isEmpty())
            return;

        if (m_cachedShadowQuality > QAbstract3DGraph::ShadowQualityNone) {
            m_depthTexture = m_textureHelper->createDepthTextureFrameBuffer(m_primarySubViewport.size(),
                                                                            m_depthFrameBuffer,
                                                                            m_shadowQualityMultiplier);
            if (!m_depthTexture)
                lowerShadowQuality();
        }
    }
}

void Scatter3DRenderer::initPointShader()
{
    if (m_isOpenGLES) {
        if (m_pointShader)
            delete m_pointShader;
        m_pointShader = new ShaderHelper(this, QStringLiteral(":/shaders/vertexPointES2"),
                                         QStringLiteral(":/shaders/fragmentPlainColor"));
        m_pointShader->initialize();
    }
}

void Scatter3DRenderer::initBackgroundShaders(const QString &vertexShader,
                                              const QString &fragmentShader)
{
    if (m_backgroundShader)
        delete m_backgroundShader;
    m_backgroundShader = new ShaderHelper(this, vertexShader, fragmentShader);
    m_backgroundShader->initialize();
}

void Scatter3DRenderer::initStaticPointShaders(const QString &vertexShader,
                                               const QString &fragmentShader)
{
    if (m_staticGradientPointShader)
        delete m_staticGradientPointShader;
    m_staticGradientPointShader = new ShaderHelper(this, vertexShader, fragmentShader);
    m_staticGradientPointShader->initialize();
}

void Scatter3DRenderer::selectionColorToSeriesAndIndex(const QVector4D &color,
                                                       int &index,
                                                       QAbstract3DSeries *&series)
{
    m_clickedType = QAbstract3DGraph::ElementNone;
    m_selectedLabelIndex = -1;
    m_selectedCustomItemIndex = -1;
    if (color != selectionSkipColor) {
        if (color.w() == labelRowAlpha) {
            // Row selection
            index = Scatter3DController::invalidSelectionIndex();
            m_selectedLabelIndex = color.x();
            m_clickedType = QAbstract3DGraph::ElementAxisZLabel;
        } else if (color.w() == labelColumnAlpha) {
            // Column selection
            index = Scatter3DController::invalidSelectionIndex();
            m_selectedLabelIndex = color.y();
            m_clickedType = QAbstract3DGraph::ElementAxisXLabel;
        } else if (color.w() == labelValueAlpha) {
            // Value selection
            index = Scatter3DController::invalidSelectionIndex();
            m_selectedLabelIndex = color.z();
            m_clickedType = QAbstract3DGraph::ElementAxisYLabel;
        } else if (color.w() == customItemAlpha) {
            // Custom item selection
            index = Scatter3DController::invalidSelectionIndex();
            m_selectedCustomItemIndex = int(color.x())
                    + (int(color.y()) << 8)
                    + (int(color.z()) << 16);
            m_clickedType = QAbstract3DGraph::ElementCustomItem;
        } else {
            int totalIndex = int(color.x())
                    + (int(color.y()) << 8)
                    + (int(color.z()) << 16);
            // Find the series and adjust the index accordingly
            foreach (SeriesRenderCache *baseCache, m_renderCacheList) {
                if (baseCache->isVisible()) {
                    ScatterSeriesRenderCache *cache =
                            static_cast<ScatterSeriesRenderCache *>(baseCache);
                    int offset = cache->selectionIndexOffset();
                    if (totalIndex >= offset
                            && totalIndex < (offset + cache->renderArray().size())) {
                        index = totalIndex - offset;
                        series = cache->series();
                        m_clickedType = QAbstract3DGraph::ElementSeries;
                        return;
                    }
                }
            }
        }
    }

    // No valid match found
    index = Scatter3DController::invalidSelectionIndex();
    series = 0;
}

void Scatter3DRenderer::updateRenderItem(const QScatterDataItem &dataItem,
                                         ScatterRenderItem &renderItem)
{
    QVector3D dotPos = dataItem.position();
    if ((dotPos.x() >= m_axisCacheX.min() && dotPos.x() <= m_axisCacheX.max() )
            && (dotPos.y() >= m_axisCacheY.min() && dotPos.y() <= m_axisCacheY.max())
            && (dotPos.z() >= m_axisCacheZ.min() && dotPos.z() <= m_axisCacheZ.max())) {
        renderItem.setPosition(dotPos);
        renderItem.setVisible(true);
        if (!dataItem.rotation().isIdentity())
            renderItem.setRotation(dataItem.rotation().normalized());
        else
            renderItem.setRotation(identityQuaternion);
        calculateTranslation(renderItem);
    } else {
        renderItem.setVisible(false);
    }
}

QVector3D Scatter3DRenderer::convertPositionToTranslation(const QVector3D &position,
                                                          bool isAbsolute)
{
    float xTrans = 0.0f;
    float yTrans = 0.0f;
    float zTrans = 0.0f;
    if (!isAbsolute) {
        if (m_polarGraph) {
            calculatePolarXZ(position, xTrans, zTrans);
        } else {
            xTrans = m_axisCacheX.positionAt(position.x());
            zTrans = m_axisCacheZ.positionAt(position.z());
        }
        yTrans = m_axisCacheY.positionAt(position.y());
    } else {
        xTrans = position.x() * m_scaleX;
        yTrans = position.y() * m_scaleY;
        zTrans = position.z() * -m_scaleZ;
    }
    return QVector3D(xTrans, yTrans, zTrans);
}

QT_END_NAMESPACE_DATAVISUALIZATION
