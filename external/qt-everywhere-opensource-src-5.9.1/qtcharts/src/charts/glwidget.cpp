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

#ifndef QT_NO_OPENGL

#include "private/glwidget_p.h"
#include "private/glxyseriesdata_p.h"
#include "private/qabstractseries_p.h"
#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLBuffer>

//#define QDEBUG_TRACE_GL_FPS
#ifdef QDEBUG_TRACE_GL_FPS
#  include <QElapsedTimer>
#endif

QT_CHARTS_BEGIN_NAMESPACE

GLWidget::GLWidget(GLXYSeriesDataManager *xyDataManager, QtCharts::QChart *chart,
                   QGraphicsView *parent)
    : QOpenGLWidget(parent->viewport()),
      m_program(nullptr),
      m_shaderAttribLoc(-1),
      m_colorUniformLoc(-1),
      m_minUniformLoc(-1),
      m_deltaUniformLoc(-1),
      m_pointSizeUniformLoc(-1),
      m_xyDataManager(xyDataManager),
      m_antiAlias(parent->renderHints().testFlag(QPainter::Antialiasing)),
      m_view(parent),
      m_selectionFbo(nullptr),
      m_chart(chart),
      m_recreateSelectionFbo(true),
      m_selectionRenderNeeded(true),
      m_mousePressed(false),
      m_lastPressSeries(nullptr),
      m_lastHoverSeries(nullptr)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_AlwaysStackOnTop);

    QSurfaceFormat surfaceFormat;
    surfaceFormat.setDepthBufferSize(0);
    surfaceFormat.setStencilBufferSize(0);
    surfaceFormat.setRedBufferSize(8);
    surfaceFormat.setGreenBufferSize(8);
    surfaceFormat.setBlueBufferSize(8);
    surfaceFormat.setAlphaBufferSize(8);
    surfaceFormat.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    surfaceFormat.setRenderableType(QSurfaceFormat::DefaultRenderableType);
    surfaceFormat.setSamples(m_antiAlias ? 4 : 0);
    setFormat(surfaceFormat);

    connect(xyDataManager, &GLXYSeriesDataManager::seriesRemoved,
            this, &GLWidget::cleanXYSeriesResources);

    setMouseTracking(true);
}

GLWidget::~GLWidget()
{
    cleanup();
}

void GLWidget::cleanup()
{
    makeCurrent();

    delete m_program;
    m_program = 0;

    foreach (QOpenGLBuffer *buffer, m_seriesBufferMap.values())
        delete buffer;
    m_seriesBufferMap.clear();

    doneCurrent();
}

void GLWidget::cleanXYSeriesResources(const QXYSeries *series)
{
    makeCurrent();
    if (series) {
        delete m_seriesBufferMap.take(series);
    } else {
        // Null series means all series were removed
        foreach (QOpenGLBuffer *buffer, m_seriesBufferMap.values())
            delete buffer;
        m_seriesBufferMap.clear();
    }
    doneCurrent();
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

void GLWidget::initializeGL()
{
    connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &GLWidget::cleanup);

    initializeOpenGLFunctions();
    glClearColor(0, 0, 0, 0);

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

    glEnableVertexAttribArray(0);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);

#if !defined(QT_OPENGL_ES_2)
    if (!QOpenGLContext::currentContext()->isOpenGLES()) {
        // Make it possible to change point primitive size and use textures with them in
        // the shaders. These are implicitly enabled in ES2.
        glEnable(GL_PROGRAM_POINT_SIZE);
    }
#endif

    m_program->release();
}

void GLWidget::paintGL()
{
    render(false);

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

void GLWidget::resizeGL(int w, int h)
{
    m_fboSize.setWidth(w);
    m_fboSize.setHeight(h);
    m_recreateSelectionFbo = true;
    m_selectionRenderNeeded = true;
}

void GLWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    QXYSeries *series = findSeriesAtEvent(event);
    if (series)
        emit series->doubleClicked(series->d_ptr->domain()->calculateDomainPoint(event->pos()));
}

void GLWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_view->hasMouseTracking() && !event->buttons()) {
        QXYSeries *series = findSeriesAtEvent(event);
        if (series != m_lastHoverSeries) {
            if (m_lastHoverSeries) {
                if (chartSeries(m_lastHoverSeries)) {
                    emit m_lastHoverSeries->hovered(
                                m_lastHoverSeries->d_ptr->domain()->calculateDomainPoint(
                                    event->pos()), false);
                }
            }
            if (series) {
                emit series->hovered(
                            series->d_ptr->domain()->calculateDomainPoint(event->pos()), true);
            }
            m_lastHoverSeries = series;
        }
    } else {
        event->ignore();
    }
}

void GLWidget::mousePressEvent(QMouseEvent *event)
{
    QXYSeries *series = findSeriesAtEvent(event);
    if (series) {
        m_mousePressed = true;
        m_mousePressPos = event->pos();
        m_lastPressSeries = series;
        emit series->pressed(series->d_ptr->domain()->calculateDomainPoint(event->pos()));
    }
}

void GLWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (chartSeries(m_lastPressSeries)) {
        emit m_lastPressSeries->released(
                    m_lastPressSeries->d_ptr->domain()->calculateDomainPoint(m_mousePressPos));
        if (m_mousePressed) {
            emit m_lastPressSeries->clicked(
                        m_lastPressSeries->d_ptr->domain()->calculateDomainPoint(m_mousePressPos));
        }
        if (m_lastHoverSeries == m_lastPressSeries
                && m_lastHoverSeries != findSeriesAtEvent(event)) {
            if (chartSeries(m_lastHoverSeries)) {
                emit m_lastHoverSeries->hovered(
                            m_lastHoverSeries->d_ptr->domain()->calculateDomainPoint(
                                event->pos()), false);
            }
            m_lastHoverSeries = nullptr;
        }
        m_lastPressSeries = nullptr;
        m_mousePressed = false;
    } else {
        event->ignore();
    }
}

QXYSeries *GLWidget::findSeriesAtEvent(QMouseEvent *event)
{
    QXYSeries *series = nullptr;
    int index = -1;

    if (m_xyDataManager->dataMap().size()) {
        makeCurrent();

        if (m_recreateSelectionFbo)
            recreateSelectionFbo();

        m_selectionFbo->bind();

        if (m_selectionRenderNeeded) {
            m_selectionVector.resize(m_xyDataManager->dataMap().size());
            render(true);
            m_selectionRenderNeeded = false;
        }

        GLubyte pixel[4] = {0, 0, 0, 0};
        glReadPixels(event->pos().x(), m_fboSize.height() - event->pos().y(),
                     1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                     (void *)pixel);
        if (pixel[3] == 0xff)
            index = pixel[0] + (pixel[1] << 8) + (pixel[2] << 16);

        glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());

        doneCurrent();
    }

    if (index >= 0) {
        const QXYSeries *cSeries = nullptr;
        if (index < m_selectionVector.size())
            cSeries = m_selectionVector.at(index);

        series = chartSeries(cSeries);
    }
    if (series)
        event->accept();
    else
        event->ignore();

    return series;
}

void GLWidget::render(bool selection)
{
    glClear(GL_COLOR_BUFFER_BIT);

    QOpenGLVertexArrayObject::Binder vaoBinder(&m_vao);
    m_program->bind();

    GLXYDataMapIterator i(m_xyDataManager->dataMap());
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
            bool dirty = data->dirty;
            if (!vbo) {
                vbo = new QOpenGLBuffer;
                m_seriesBufferMap.insert(i.key(), vbo);
                vbo->create();
                dirty = true;
            }
            vbo->bind();
            if (dirty) {
                vbo->allocate(data->array.constData(), data->array.count() * sizeof(GLfloat));
                dirty = false;
                m_selectionRenderNeeded = true;
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
    m_program->release();
}

void GLWidget::recreateSelectionFbo()
{
    QOpenGLFramebufferObjectFormat fboFormat;
    fboFormat.setAttachment(QOpenGLFramebufferObject::NoAttachment);

    delete m_selectionFbo;

    const QSize deviceSize = m_fboSize * devicePixelRatioF();
    m_selectionFbo = new QOpenGLFramebufferObject(deviceSize, fboFormat);
    m_recreateSelectionFbo = false;
    m_selectionRenderNeeded = true;
}

// This function makes sure the series we are dealing with has not been removed from the
// chart since we stored the pointer.
QXYSeries *GLWidget::chartSeries(const QXYSeries *cSeries)
{
    QXYSeries *series = nullptr;
    if (cSeries) {
        Q_FOREACH (QAbstractSeries *chartSeries, m_chart->series()) {
            if (cSeries == chartSeries) {
                series = qobject_cast<QXYSeries *>(chartSeries);
                break;
            }
        }
    }
    return series;
}

bool GLWidget::needsReset() const
{
    return m_view->renderHints().testFlag(QPainter::Antialiasing) != m_antiAlias;
}

QT_CHARTS_END_NAMESPACE

#endif
