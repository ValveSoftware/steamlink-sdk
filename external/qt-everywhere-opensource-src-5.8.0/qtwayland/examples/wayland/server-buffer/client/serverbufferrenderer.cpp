/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

#include "serverbufferrenderer.h"

#include <QtGui/QOpenGLVertexArrayObject>
#include <QtGui/QOpenGLShaderProgram>

static const GLfloat uv_coords[] = {
    0,0,
    0,1,
    1,0,
    1,1,
};

static const GLfloat vertex_coords[] = {
    -1,-1,0,
    -1,1,0,
    1,-1,0,
    1,1,0,
};

static const char vertex_shader[] =
    "attribute vec3 vertexCoord;"
    "attribute vec2 textureCoord;"
    "uniform mat4 transform;"
    "varying mediump vec2 uv;"
    "void main() {"
    "   uv = textureCoord;"
    "   gl_Position = transform * vec4(vertexCoord,1);"
    "}";

static const char fragment_shader[] =
    "varying mediump vec2 uv;"
    "uniform sampler2D textureSampler;"
    "void main() {"
    "   gl_FragColor = texture2D(textureSampler, uv);"
    "}";

ServerBufferRenderer::ServerBufferRenderer(QtWaylandClient::QWaylandServerBuffer *serverBuffer)
    : QOpenGLFunctions(QOpenGLContext::currentContext())
    , m_server_buffer(serverBuffer)
    , m_texture(0)
    , m_vao(new QOpenGLVertexArrayObject())
    , m_program(new QOpenGLShaderProgram())
    , m_vertexbuffer(0)
    , m_texture_coords(0)

{
    Q_ASSERT(serverBuffer);
    if (serverBuffer->userData()) {
        qWarning("ServerBufferRenderer: Will over QWaylandServerBuffers %p userdata %p", serverBuffer, serverBuffer->userData());
    }
    serverBuffer->setUserData(this);
    m_vao->create();
    m_vao->bind();

    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertex_shader);
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragment_shader);
    if (!m_program->link()) {
        qDebug() << m_program->log();
    }

    glGenTextures(1,&m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    serverBuffer->bindTextureToBuffer();

    glGenBuffers(1, &m_vertexbuffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexbuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_coords), vertex_coords, GL_STATIC_DRAW);
    m_program->setAttributeBuffer("vertexCoord", GL_FLOAT, 0, 3, 0);

    glGenBuffers(1, &m_texture_coords);
    glBindBuffer(GL_ARRAY_BUFFER, m_texture_coords);
    glBufferData(GL_ARRAY_BUFFER, sizeof(uv_coords), uv_coords, GL_STATIC_DRAW);
    m_program->setAttributeBuffer("textureCoord", GL_FLOAT, 0, 2, 0);

    m_program->enableAttributeArray("vertexCoord");
    m_program->enableAttributeArray("textureCoord");

    m_vao->release();
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ServerBufferRenderer::render(const QMatrix4x4 &transform)
{
    m_vao->bind();

    m_program->bind();
    m_program->setUniformValue("transform", transform);

    glBindTexture(GL_TEXTURE_2D, m_texture);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    m_vao->release();
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}
