/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Charts module of the Qt Toolkit.
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

#include "declarativeopenglrendernode.h"

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOpenGLFramebufferObjectFormat>
#include <QtGui/QOpenGLFramebufferObject>
#include <QOpenGLShaderProgram>
#include <QtGui/QOpenGLBuffer>

//#define QDEBUG_TRACE_GL_FPS
#ifdef QDEBUG_TRACE_GL_FPS
#  include <QElapsedTimer>
#endif

QT_CHARTS_BEGIN_NAMESPACE

// This node draws the xy series data on a transparent background using OpenGL.
// It is used as a child node of the chart node.
DeclarativeOpenGLRenderNode::DeclarativeOpenGLRenderNode(QQuickWindow *window) :
    QObject(),
    m_texture(nullptr),
    m_imageNode(nullptr),
    m_window(window),
    m_textureOptions(QQuickWindow::TextureHasAlphaChannel),
    m_textureSize(1, 1),
    m_recreateFbo(false),
    m_fbo(nullptr),
    m_resolvedFbo(nullptr),
    m_selectionFbo(nullptr),
    m_program(nullptr),
    m_shaderAttribLoc(-1),
    m_colorUniformLoc(-1),
    m_minUniformLoc(-1),
    m_deltaUniformLoc(-1),
    m_pointSizeUniformLoc(-1),
    m_renderNeeded(true),
    m_antialiasing(false),
    m_selectionRenderNeeded(true),
    m_mousePressed(false),
    m_lastPressSeries(nullptr),
    m_lastHoverSeries(nullptr)
{
    initializeOpenGLFunctions();

    connect(m_window, &QQuickWindow::beforeRendering,
            this, &DeclarativeOpenGLRenderNode::render);
}

DeclarativeOpenGLRenderNode::~DeclarativeOpenGLRenderNode()
{
    cleanXYSeriesResources(0);

    delete m_texture;
    delete m_fbo;
    delete m_resolvedFbo;
    delete m_selectionFbo;
    delete m_program;

    qDeleteAll(m_mouseEvents);
}

static const char *vertexSource =
        "attribute highp vec2 points;\n"
        "uniform highp vec2 min;\n"
        "uniform highp vec2 delta;\n"
        "uniform highp float pointSize;\n"
        "uniform highp mat4 matrix;\n"
        "void main() {\n"
        "  vec2 normalPoint = vec2(-1, -1) + ((points - min) / delta);\n"
        "  gl_Position = matrix * vec4(normalPoint, 0, 1);\n"
        "  gl_PointSize = pointSize;\n"
        "}";
static const char *fragmentSource =
        "uniform highp vec3 color;\n"
        "void main() {\n"
        "  gl_FragColor = vec4(color,1);\n"
        "}\n";

// Must be called on render thread and in context
void DeclarativeOpenGLRenderNode::initGL()
{
    recreateFBO();

    m_program = new QOpenGLShaderProgram;
    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexSource);
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentSource);
    m_program->bindAttributeLocation("points", 0);
    m_program->link();

    m_program->bind();
    m_colorUniformLoc = m_program->uniformLocation("color");
    m_minUniformLoc = m_program->uniformLocation("min");
    m_deltaUniformLoc = m_program->uniformLocation("delta");
    m_pointSizeUniformLoc = m_program->uniformLocation("pointSize");
    m_matrixUniformLoc = m_program->uniformLocation("matrix");

    // Create a vertex array object. In OpenGL ES 2.0 and OpenGL 2.x
    // implementations this is optional and support may not be present
    // at all. Nonetheless the below code works in all cases and makes
    // sure there is a VAO when one is needed.
    m_vao.create();
    QOpenGLVertexArrayObject::Binder vaoBinder(&m_vao);

#if !defined(QT_OPENGL_ES_2)
    if (!QOpenGLContext::currentContext()->isOpenGLES()) {
        // Make it possible to change point primitive size and use textures with them in
        // the shaders. These are implicitly enabled in ES2.
        // Qt Quick doesn't change these flags, so it should be safe to just enable them
        // at initialization.
        glEnable(GL_PROGRAM_POINT_SIZE);
    }
#endif

    m_program->release();
}

void DeclarativeOpenGLRenderNode::recreateFBO()
{
    QOpenGLFramebufferObjectFormat fboFormat;
    fboFormat.setAttachment(QOpenGLFramebufferObject::NoAttachment);

    int samples = 0;
    QOpenGLContext *context = QOpenGLContext::currentContext();

    if (m_antialiasing && (!context->isOpenGLES() || context->format().majorVersion() >= 3))
        samples = 4;
    fboFormat.setSamples(samples);

    delete m_fbo;
    delete m_resolvedFbo;
    delete m_selectionFbo;
    m_resolvedFbo = nullptr;

    m_fbo = new QOpenGLFramebufferObject(m_textureSize, fboFormat);
    if (samples > 0)
        m_resolvedFbo = new QOpenGLFramebufferObject(m_textureSize);
    m_selectionFbo = new QOpenGLFramebufferObject(m_textureSize);

    delete m_texture;
    uint textureId = m_resolvedFbo ? m_resolvedFbo->texture() : m_fbo->texture();
    m_texture = m_window->createTextureFromId(textureId, m_textureSize, m_textureOptions);
    if (!m_imageNode) {
        m_imageNode = m_window->createImageNode();
        m_imageNode->setFiltering(QSGTexture::Linear);
        m_imageNode->setTextureCoordinatesTransform(QSGImageNode::MirrorVertically);
        m_imageNode->setFlag(OwnedByParent);
        if (!m_rect.isEmpty())
            m_imageNode->setRect(m_rect);
        appendChildNode(m_imageNode);
    }
    m_imageNode->setTexture(m_texture);

    m_recreateFbo = false;
}

// Must be called on render thread and in context
void DeclarativeOpenGLRenderNode::setTextureSize(const QSize &size)
{
    m_textureSize = size;
    m_recreateFbo = true;
    m_renderNeeded = true;
    m_selectionRenderNeeded = true;
}

// Must be called on render thread while gui thread is blocked, and in context
void DeclarativeOpenGLRenderNode::setSeriesData(bool mapDirty, const GLXYDataMap &dataMap)
{
    bool dirty = false;
    if (mapDirty) {
        // Series have changed, recreate map, but utilize old data where feasible
        GLXYDataMap oldMap = m_xyDataMap;
        m_xyDataMap.clear();

        GLXYDataMapIterator i(dataMap);
        while (i.hasNext()) {
            i.next();
            GLXYSeriesData *data = oldMap.take(i.key());
            const GLXYSeriesData *newData = i.value();
            if (!data || newData->dirty) {
                data = new GLXYSeriesData;
                *data = *newData;
            }
            m_xyDataMap.insert(i.key(), data);
        }
        // Delete remaining old data
        i = oldMap;
        while (i.hasNext()) {
            i.next();
            delete i.value();
            cleanXYSeriesResources(i.key());
        }
        dirty = true;
    } else {
        // Series have not changed, so just copy dirty data over
        GLXYDataMapIterator i(dataMap);
        while (i.hasNext()) {
            i.next();
            const GLXYSeriesData *newData = i.value();
            if (i.value()->dirty) {
                dirty = true;
                GLXYSeriesData *data = m_xyDataMap.value(i.key());
                if (data)
                    *data = *newData;
            }
        }
    }
    if (dirty) {
        markDirty(DirtyMaterial);
        m_renderNeeded = true;
        m_selectionRenderNeeded = true;
    }
}

void DeclarativeOpenGLRenderNode::setRect(const QRectF &rect)
{
    m_rect = rect;

    if (m_imageNode)
        m_imageNode->setRect(rect);
}

void DeclarativeOpenGLRenderNode::setAntialiasing(bool enable)
{
    if (m_antialiasing != enable) {
        m_antialiasing = enable;
        m_recreateFbo = true;
        m_renderNeeded = true;
    }
}

void DeclarativeOpenGLRenderNode::addMouseEvents(const QVector<QMouseEvent *> &events)
{
    if (events.size()) {
        m_mouseEvents.append(events);
        markDirty(DirtyMaterial);
    }
}

void DeclarativeOpenGLRenderNode::takeMouseEventResponses(QVector<MouseEventResponse> &responses)
{
    responses.append(m_mouseEventResponses);
    m_mouseEventResponses.clear();
}

void DeclarativeOpenGLRenderNode::renderGL(bool selection)
{
    glClearColor(0, 0, 0, 0);

    QOpenGLVertexArrayObject::Binder vaoBinder(&m_vao);
    m_program->bind();

    glClear(GL_COLOR_BUFFER_BIT);
    glEnableVertexAttribArray(0);

    glViewport(0, 0, m_textureSize.width(), m_textureSize.height());

    GLXYDataMapIterator i(m_xyDataMap);
    int counter = 0;
    while (i.hasNext()) {
        i.next();
        QOpenGLBuffer *vbo = m_seriesBufferMap.value(i.key());
        GLXYSeriesData *data = i.value();

        if (data->visible) {
            if (selection) {
                m_selectionVector[counter] = i.key();
                m_program->setUniformValue(m_colorUniformLoc, QVector3D((counter & 0xff) / 255.0f,
                                                                        ((counter & 0xff00) >> 8) / 255.0f,
                                                                        ((counter & 0xff0000) >> 16) / 255.0f));
                counter++;
            } else {
                m_program->setUniformValue(m_colorUniformLoc, data->color);
            }
            m_program->setUniformValue(m_minUniformLoc, data->min);
            m_program->setUniformValue(m_deltaUniformLoc, data->delta);
            m_program->setUniformValue(m_matrixUniformLoc, data->matrix);

            if (!vbo) {
                vbo = new QOpenGLBuffer;
                m_seriesBufferMap.insert(i.key(), vbo);
                vbo->create();
            }
            vbo->bind();
            if (data->dirty) {
                vbo->allocate(data->array.constData(), data->array.count() * sizeof(GLfloat));
                data->dirty = false;
            }

            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
            if (data->type == QAbstractSeries::SeriesTypeLine) {
                glLineWidth(data->width);
                glDrawArrays(GL_LINE_STRIP, 0, data->array.size() / 2);
            } else { // Scatter
                m_program->setUniformValue(m_pointSizeUniformLoc, data->width);
                glDrawArrays(GL_POINTS, 0, data->array.size() / 2);
            }
            vbo->release();
        }
    }
}

void DeclarativeOpenGLRenderNode::renderSelection()
{
    m_selectionFbo->bind();

    m_selectionVector.resize(m_xyDataMap.size());

    renderGL(true);

    m_selectionRenderNeeded = false;
}

void DeclarativeOpenGLRenderNode::renderVisual()
{
    m_fbo->bind();

    renderGL(false);

    if (m_resolvedFbo) {
        QRect rect(QPoint(0, 0), m_fbo->size());
        QOpenGLFramebufferObject::blitFramebuffer(m_resolvedFbo, rect, m_fbo, rect);
    }

    markDirty(DirtyMaterial);

#ifdef QDEBUG_TRACE_GL_FPS
    static QElapsedTimer stopWatch;
    static int frameCount = -1;
    if (frameCount == -1) {
        stopWatch.start();
        frameCount = 0;
    }
    frameCount++;
    int elapsed = stopWatch.elapsed();
    if (elapsed >= 1000) {
        elapsed = stopWatch.restart();
        qreal fps = qreal(0.1 * int(10000.0 * (qreal(frameCount) / qreal(elapsed))));
        qDebug() << "FPS:" << fps;
        frameCount = 0;
    }
#endif
}

// Must be called on render thread as response to beforeRendering signal
void DeclarativeOpenGLRenderNode::render()
{
    if (m_renderNeeded) {
        if (m_xyDataMap.size()) {
            if (!m_program)
                initGL();
            if (m_recreateFbo)
                recreateFBO();
            renderVisual();
        } else {
            if (m_imageNode && m_imageNode->rect() != QRectF()) {
                glClearColor(0, 0, 0, 0);
                m_fbo->bind();
                glClear(GL_COLOR_BUFFER_BIT);

                // If last series was removed, zero out the node rect
                setRect(QRectF());
            }
        }
        m_renderNeeded = false;
    }
    handleMouseEvents();
    m_window->resetOpenGLState();
}

void DeclarativeOpenGLRenderNode::cleanXYSeriesResources(const QXYSeries *series)
{
    if (series) {
        delete m_seriesBufferMap.take(series);
        delete m_xyDataMap.take(series);
    } else {
        foreach (QOpenGLBuffer *buffer, m_seriesBufferMap.values())
            delete buffer;
        m_seriesBufferMap.clear();
        foreach (GLXYSeriesData *data, m_xyDataMap.values())
            delete data;
        m_xyDataMap.clear();
    }
}

void DeclarativeOpenGLRenderNode::handleMouseEvents()
{
    if (m_mouseEvents.size()) {
        if (m_xyDataMap.size()) {
            if (m_selectionRenderNeeded)
                renderSelection();
        }
        Q_FOREACH (QMouseEvent *event, m_mouseEvents) {
            const QXYSeries *series = findSeriesAtEvent(event);
            switch (event->type()) {
            case QEvent::MouseMove: {
                if (series != m_lastHoverSeries) {
                    if (m_lastHoverSeries) {
                        m_mouseEventResponses.append(
                                    MouseEventResponse(MouseEventResponse::HoverLeave,
                                                       event->pos(), m_lastHoverSeries));
                    }
                    if (series) {
                        m_mouseEventResponses.append(
                                    MouseEventResponse(MouseEventResponse::HoverEnter,
                                                       event->pos(), series));
                    }
                    m_lastHoverSeries = series;
                }
                break;
            }
            case QEvent::MouseButtonPress: {
                if (series) {
                    m_mousePressed = true;
                    m_mousePressPos = event->pos();
                    m_lastPressSeries = series;
                    m_mouseEventResponses.append(
                                MouseEventResponse(MouseEventResponse::Pressed,
                                                   event->pos(), series));
                }
                break;
            }
            case QEvent::MouseButtonRelease: {
                m_mouseEventResponses.append(
                            MouseEventResponse(MouseEventResponse::Released,
                                               m_mousePressPos, m_lastPressSeries));
                if (m_mousePressed) {
                    m_mouseEventResponses.append(
                                MouseEventResponse(MouseEventResponse::Clicked,
                                                   m_mousePressPos, m_lastPressSeries));
                }
                if (m_lastHoverSeries == m_lastPressSeries && m_lastHoverSeries != series) {
                    if (m_lastHoverSeries) {
                        m_mouseEventResponses.append(
                                    MouseEventResponse(MouseEventResponse::HoverLeave,
                                                       event->pos(), m_lastHoverSeries));
                    }
                    m_lastHoverSeries = nullptr;
                }
                m_lastPressSeries = nullptr;
                m_mousePressed = false;
                break;
            }
            case QEvent::MouseButtonDblClick: {
                if (series) {
                    m_mouseEventResponses.append(
                                MouseEventResponse(MouseEventResponse::DoubleClicked,
                                                   event->pos(), series));
                }
                break;
            }
            default:
                break;
            }
        }

        qDeleteAll(m_mouseEvents);
        m_mouseEvents.clear();
    }
}

const QXYSeries *DeclarativeOpenGLRenderNode::findSeriesAtEvent(QMouseEvent *event)
{
    const QXYSeries *series = nullptr;
    int index = -1;

    if (m_xyDataMap.size()) {
        m_selectionFbo->bind();

        GLubyte pixel[4] = {0, 0, 0, 0};
        glReadPixels(event->pos().x(), m_textureSize.height() - event->pos().y(),
                     1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                     (void *)pixel);
        if (pixel[3] == 0xff)
            index = pixel[0] + (pixel[1] << 8) + (pixel[2] << 16);
    }

    if (index >= 0 && index < m_selectionVector.size())
        series = m_selectionVector.at(index);

    return series;
}

QT_CHARTS_END_NAMESPACE
