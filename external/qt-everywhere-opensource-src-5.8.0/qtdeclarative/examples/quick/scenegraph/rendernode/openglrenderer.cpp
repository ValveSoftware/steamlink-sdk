/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "openglrenderer.h"
#include <QQuickItem>

#ifndef QT_NO_OPENGL

#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>

OpenGLRenderNode::OpenGLRenderNode(QQuickItem *item)
    : m_item(item)
{
}

OpenGLRenderNode::~OpenGLRenderNode()
{
    releaseResources();
}

void OpenGLRenderNode::releaseResources()
{
    delete m_program;
    m_program = nullptr;
    delete m_vbo;
    m_vbo = nullptr;
}

void OpenGLRenderNode::init()
{
    m_program = new QOpenGLShaderProgram;

    static const char *vertexShaderSource =
        "attribute highp vec4 posAttr;\n"
        "attribute lowp vec4 colAttr;\n"
        "varying lowp vec4 col;\n"
        "uniform highp mat4 matrix;\n"
        "void main() {\n"
        "   col = colAttr;\n"
        "   gl_Position = matrix * posAttr;\n"
        "}\n";

    static const char *fragmentShaderSource =
        "varying lowp vec4 col;\n"
        "uniform lowp float opacity;\n"
        "void main() {\n"
        "   gl_FragColor = col * opacity;\n"
        "}\n";

    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);
    m_program->bindAttributeLocation("posAttr", 0);
    m_program->bindAttributeLocation("colAttr", 1);
    m_program->link();

    m_matrixUniform = m_program->uniformLocation("matrix");
    m_opacityUniform = m_program->uniformLocation("opacity");

    const int VERTEX_SIZE = 6 * sizeof(GLfloat);

    static GLfloat colors[] = {
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f
    };

    m_vbo = new QOpenGLBuffer;
    m_vbo->create();
    m_vbo->bind();
    m_vbo->allocate(VERTEX_SIZE + sizeof(colors));
    m_vbo->write(VERTEX_SIZE, colors, sizeof(colors));
    m_vbo->release();
}

void OpenGLRenderNode::render(const RenderState *state)
{
    if (!m_program)
        init();

    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();

    m_program->bind();
    m_program->setUniformValue(m_matrixUniform, *state->projectionMatrix() * *matrix());
    m_program->setUniformValue(m_opacityUniform, float(inheritedOpacity()));

    m_vbo->bind();

    QPointF p0(m_item->width() - 1, m_item->height() - 1);
    QPointF p1(0, 0);
    QPointF p2(0, m_item->height() - 1);

    GLfloat vertices[6] = { GLfloat(p0.x()), GLfloat(p0.y()),
                            GLfloat(p1.x()), GLfloat(p1.y()),
                            GLfloat(p2.x()), GLfloat(p2.y()) };
    m_vbo->write(0, vertices, sizeof(vertices));

    m_program->setAttributeBuffer(0, GL_FLOAT, 0, 2);
    m_program->setAttributeBuffer(1, GL_FLOAT, sizeof(vertices), 3);
    m_program->enableAttributeArray(0);
    m_program->enableAttributeArray(1);

    // Note that clipping (scissor or stencil) is ignored in this example.

    f->glEnable(GL_BLEND);
    f->glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    f->glDrawArrays(GL_TRIANGLES, 0, 3);
}

QSGRenderNode::StateFlags OpenGLRenderNode::changedStates() const
{
    return BlendState;
}

QSGRenderNode::RenderingFlags OpenGLRenderNode::flags() const
{
    return BoundedRectRendering | DepthAwareRendering;
}

QRectF OpenGLRenderNode::rect() const
{
    return QRect(0, 0, m_item->width(), m_item->height());
}

#endif // QT_NO_OPENGL
