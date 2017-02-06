/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
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
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest/QTest>
#include <Qt3DRender/qrendertargetoutput.h>
#include <Qt3DRender/private/uniform_p.h>
#include <Qt3DRender/private/graphicshelpergl3_3_p.h>
#include <Qt3DRender/private/attachmentpack_p.h>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>

#if !defined(QT_OPENGL_ES_2) && defined(QT_OPENGL_3_2)

#define TEST_SHOULD_BE_PERFORMED 1

using namespace Qt3DRender;
using namespace Qt3DRender::Render;

namespace {

const QByteArray vertCode = QByteArrayLiteral(
            "#version 330 core\n" \
            "in vec3 vertexPosition;\n" \
            "in vec2 vertexTexCoord;\n" \
            "out vec2 texCoord;\n" \
            "void main()\n" \
            "{\n" \
            "   texCoord = vertexTexCoord;\n" \
            "   gl_Position = vec4(vertexPosition, 1.0);\n" \
            "}\n");

const QByteArray vertCodeUniformBuffer = QByteArrayLiteral(
            "#version 330 core\n" \
            "in vec3 vertexPosition;\n" \
            "in vec2 vertexTexCoord;\n" \
            "in int vertexColorIndex;\n" \
            "out vec2 texCoord;\n" \
            "flat out int colorIndex;\n" \
            "void main()\n" \
            "{\n" \
            "   texCoord = vertexTexCoord;\n" \
            "   colorIndex = vertexColorIndex;\n" \
            "   gl_Position = vec4(vertexPosition, 1.0);\n" \
            "}\n");

const QByteArray fragCodeFragOutputs = QByteArrayLiteral(
            "#version 330 core\n" \
            "out vec4 color;\n" \
            "out vec2 temp;\n" \
            "void main()\n" \
            "{\n" \
            "   color = vec4(1.0, 0.0, 0.0, 1.0);\n" \
            "   temp = vec2(1.0, 0.3);\n" \
            "}\n");

const QByteArray fragCodeUniformsFloat = QByteArrayLiteral(
            "#version 330 core\n" \
            "out vec4 color;\n" \
            "uniform float multiplier;\n" \
            "uniform vec2 multiplierVec2;\n" \
            "uniform vec3 multiplierVec3;\n" \
            "uniform vec4 multiplierVec4;\n" \
            "void main()\n" \
            "{\n" \
            "   vec4 randomMult = multiplierVec4 + vec4(multiplierVec3, 0.0) + vec4(multiplierVec2, 0.0, 0.0);\n" \
            "   color = vec4(1.0, 0.0, 0.0, 1.0) * randomMult * multiplier;\n" \
            "}\n");

const QByteArray fragCodeUniformsInt = QByteArrayLiteral(
            "#version 330 core\n" \
            "out ivec4 color;\n" \
            "uniform int multiplier;\n" \
            "uniform ivec2 multiplierVec2;\n" \
            "uniform ivec3 multiplierVec3;\n" \
            "uniform ivec4 multiplierVec4;\n" \
            "void main()\n" \
            "{\n" \
            "   ivec4 randomMult = multiplierVec4 + ivec4(multiplierVec3, 0) + ivec4(multiplierVec2, 0, 0);\n" \
            "   color = ivec4(1, 0, 0, 1) * randomMult * multiplier;\n" \
            "}\n");

const QByteArray fragCodeUniformsUInt = QByteArrayLiteral(
            "#version 330 core\n" \
            "out uvec4 color;\n" \
            "uniform uint multiplier;\n" \
            "uniform uvec2 multiplierVec2;\n" \
            "uniform uvec3 multiplierVec3;\n" \
            "uniform uvec4 multiplierVec4;\n" \
            "void main()\n" \
            "{\n" \
            "   uvec4 randomMult = multiplierVec4 + uvec4(multiplierVec3, 0) + uvec4(multiplierVec2, 0, 0);\n" \
            "   color = uvec4(1, 0, 0, 1) * randomMult * multiplier;\n" \
            "}\n");

const QByteArray fragCodeUniformsFloatMatrices = QByteArrayLiteral(
            "#version 330 core\n" \
            "out vec4 color;\n" \
            "uniform mat2 m2;\n" \
            "uniform mat2x3 m23;\n" \
            "uniform mat3x2 m32;\n" \
            "uniform mat2x4 m24;\n" \
            "uniform mat4x2 m42;\n" \
            "uniform mat3 m3;\n" \
            "uniform mat3x4 m34;\n" \
            "uniform mat4x3 m43;\n" \
            "uniform mat4 m4;\n" \
            "void main()\n" \
            "{\n" \
            "   float lengthSum = m2[0][0] + m23[0][0] + m32[0][0] + m24[0][0] + m42[0][0] + m3[0][0] + m34[0][0] + m43[0][0] + m4[0][0];\n" \
            "   color = vec4(1, 0, 0, 1) * lengthSum;\n" \
            "}\n");

const QByteArray fragCodeUniformBuffer = QByteArrayLiteral(
            "#version 330 core\n" \
            "out vec4 color;\n" \
            "flat in int colorIndex;\n" \
            "uniform ColorArray\n" \
            "{\n" \
            "   vec4 colors[256];\n" \
            "};\n" \
            "void main()\n" \
            "{\n" \
            "   color = colors[colorIndex];\n" \
            "}\n");

const QByteArray fragCodeSamplers = QByteArrayLiteral(
            "#version 330 core\n" \
            "in vec2 texCoord;\n" \
            "out vec4 color;\n" \
            "uniform sampler1D s1;\n" \
            "uniform sampler2D s2;\n" \
            "uniform sampler2DArray s2a;\n" \
            "uniform sampler3D s3;\n" \
            "uniform samplerCube scube;\n" \
            "uniform sampler2DRect srect;\n" \
            "void main()\n" \
            "{\n" \
            "   color = vec4(1, 0, 0, 1) *" \
            " texture(s1, texCoord.x) *" \
            " texture(s2, texCoord) *" \
            " texture(s2a, vec3(texCoord, 0.0)) *" \
            " texture(s3, vec3(texCoord, 0.0)) *" \
            " texture(scube, vec3(texCoord, 0)) *" \
            " texture(srect, texCoord);\n" \
            "}\n");

} // anonymous

class tst_GraphicsHelperGL3_3 : public QObject
{
    Q_OBJECT
private Q_SLOTS:

    void init()
    {
        m_window.reset(new QWindow);
        m_window->setSurfaceType(QWindow::OpenGLSurface);
        m_window->setGeometry(0, 0, 10, 10);

        QSurfaceFormat format;
        format.setVersion(3, 3);
        format.setProfile(QSurfaceFormat::CoreProfile);
        format.setDepthBufferSize(24);
        format.setSamples(4);
        format.setStencilBufferSize(8);
        m_window->setFormat(format);
        m_glContext.setFormat(format);

        m_window->create();

        if (!m_glContext.create()) {
            qWarning() << "Failed to create OpenGL context";
            return;
        }

        if (!m_glContext.makeCurrent(m_window.data())) {
            qWarning() << "Failed to maed OpenGL context current";
            return;
        }

        if ((m_func = m_glContext.versionFunctions<QOpenGLFunctions_3_3_Core>()) != nullptr) {
            m_glHelper.initializeHelper(&m_glContext, m_func);
            m_initializationSuccessful = true;
        }
    }

    void alphaTest()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");
        // Deprecated
    }

    void bindBufferBase()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        GLuint bufferId = 0;
        // WHEN
        m_func->glGenBuffers(1, &bufferId);
        // THEN
        QVERIFY(bufferId != 0);


        // WHEN
        m_func->glBindBuffer(GL_UNIFORM_BUFFER, bufferId);
        m_glHelper.bindBufferBase(GL_UNIFORM_BUFFER, 2, bufferId);
        // THEN
        const GLint error = m_func->glGetError();
        QVERIFY(error == 0);
        GLint boundToPointBufferId = 0;
        m_func->glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, 2,  &boundToPointBufferId);
        QVERIFY(boundToPointBufferId == GLint(bufferId));

        // Restore to sane state
        m_func->glBindBuffer(GL_UNIFORM_BUFFER, 0);
        m_func->glDeleteBuffers(1, &bufferId);
    }

    void bindFragDataLocation()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        QOpenGLShaderProgram shaderProgram;
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertCode);
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragCodeFragOutputs);

        // WHEN
        QHash<QString, int> fragLocations;
        fragLocations.insert(QStringLiteral("temp"), 2);
        fragLocations.insert(QStringLiteral("color"), 1);
        m_glHelper.bindFragDataLocation(shaderProgram.programId(), fragLocations);

        // THEN
        QVERIFY(shaderProgram.link());
        const GLint error = m_func->glGetError();
        QVERIFY(error == 0);
        const GLint tempLocation = m_func->glGetFragDataLocation(shaderProgram.programId(), "temp");
        const GLint colorLocation = m_func->glGetFragDataLocation(shaderProgram.programId(), "color");
        QCOMPARE(tempLocation, 2);
        QCOMPARE(colorLocation, 1);
    }

    void bindFrameBufferAttachment()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        {
            // GIVEN
            GLuint fboId;
            m_func->glGenFramebuffers(1, &fboId);

            Attachment attachment;
            attachment.m_point = QRenderTargetOutput::Color0;

            GLint maxAttachmentsCount = 0;
            m_func->glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxAttachmentsCount);

            // THEN
            QVERIFY(fboId != 0);
            QVERIFY(maxAttachmentsCount >= 3);

            // WHEN
            m_func->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboId);

            QOpenGLTexture texture(QOpenGLTexture::Target2D);
            texture.setSize(512, 512);
            texture.setFormat(QOpenGLTexture::RGBA8U);
            texture.setMinificationFilter(QOpenGLTexture::Linear);
            texture.setMagnificationFilter(QOpenGLTexture::Linear);
            texture.setWrapMode(QOpenGLTexture::ClampToEdge);
            if (!texture.create())
                qWarning() << "Texture creation failed";
            texture.allocateStorage();
            QVERIFY(texture.isStorageAllocated());
            GLint error = m_func->glGetError();
            QVERIFY(error == 0);
            m_glHelper.bindFrameBufferAttachment(&texture, attachment);

            // THEN
            GLenum status = m_func->glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
            QCOMPARE(int(status), GL_FRAMEBUFFER_COMPLETE);

            error = m_func->glGetError();
            QVERIFY(error == 0);
            GLint textureAttachmentId = 0;
            m_func->glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,
                                                          GL_COLOR_ATTACHMENT0,
                                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                          &textureAttachmentId);
            QCOMPARE(GLuint(textureAttachmentId), texture.textureId());

            // Restore state
            m_func->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            m_func->glDeleteFramebuffers(1, &fboId);
        }
        {
            // GIVEN
            QOpenGLTexture texture(QOpenGLTexture::TargetCubeMap);
            texture.setSize(512, 512);
            texture.setFormat(QOpenGLTexture::RGBA32F);
            texture.setMinificationFilter(QOpenGLTexture::Linear);
            texture.setMagnificationFilter(QOpenGLTexture::Linear);
            texture.setWrapMode(QOpenGLTexture::ClampToEdge);
            if (!texture.create())
                qWarning() << "Texture creation failed";
            texture.allocateStorage();
            QVERIFY(texture.isStorageAllocated());
            GLint error = m_func->glGetError();
            QVERIFY(error == 0);

            { // Check All Faces

                // GIVEN
                GLuint fboId;
                m_func->glGenFramebuffers(1, &fboId);

                // THEN
                QVERIFY(fboId != 0);

                // WHEN
                m_func->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboId);

                Attachment attachment;
                attachment.m_point = QRenderTargetOutput::Color0;
                attachment.m_face = Qt3DRender::QAbstractTexture::AllFaces;

                m_glHelper.bindFrameBufferAttachment(&texture, attachment);

                // THEN
                GLenum status = m_func->glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
                QVERIFY(status == GL_FRAMEBUFFER_COMPLETE);

                error = m_func->glGetError();
                QVERIFY(error == 0);
                GLint textureIsLayered = 0;
                m_func->glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,
                                                              GL_COLOR_ATTACHMENT0,
                                                              GL_FRAMEBUFFER_ATTACHMENT_LAYERED,
                                                              &textureIsLayered);
                QCOMPARE(textureIsLayered, GL_TRUE);

                // Restore state
                m_func->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                m_func->glDeleteFramebuffers(1, &fboId);
            }
            { // Check Specific Faces

                // GIVEN
                GLuint fboId;
                m_func->glGenFramebuffers(1, &fboId);

                // THEN
                QVERIFY(fboId != 0);

                // WHEN
                m_func->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboId);

                Attachment attachment;
                attachment.m_point = QRenderTargetOutput::Color0;
                attachment.m_face = Qt3DRender::QAbstractTexture::CubeMapNegativeZ;

                m_glHelper.bindFrameBufferAttachment(&texture, attachment);

                // THEN
                GLenum status = m_func->glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
                QVERIFY(status == GL_FRAMEBUFFER_COMPLETE);

                error = m_func->glGetError();
                QVERIFY(error == 0);
                GLint textureFace = 0;
                m_func->glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,
                                                              GL_COLOR_ATTACHMENT0,
                                                              GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE,
                                                              &textureFace);
                QCOMPARE(textureFace, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z);
                GLint textureIsLayered = 0;
                m_func->glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,
                                                              GL_COLOR_ATTACHMENT0,
                                                              GL_FRAMEBUFFER_ATTACHMENT_LAYERED,
                                                              &textureIsLayered);
                QCOMPARE(textureIsLayered, GL_FALSE);

                // Restore state
                m_func->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                m_func->glDeleteFramebuffers(1, &fboId);
            }
        }
    }

    void bindFrameBufferObject()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        GLuint fboId;
        m_func->glGenFramebuffers(1, &fboId);

        // THEN
        QVERIFY(fboId != 0);

        // WHEN
        m_glHelper.bindFrameBufferObject(fboId);

        // THEN
        const GLint error = m_func->glGetError();
        QVERIFY(error == 0);
        GLint boundindFBOId = 0;
        m_func->glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &boundindFBOId);
        QVERIFY(GLuint(boundindFBOId) == fboId);

        // Cleanup
        m_func->glDeleteFramebuffers(1, &fboId);
    }

    void bindShaderStorageBlock()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");
        // Not supported in OpenGL 3.3
    }

    void bindUniformBlock()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        QOpenGLShaderProgram shaderProgram;
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertCodeUniformBuffer);
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragCodeUniformBuffer);
        QVERIFY(shaderProgram.link());

        // WHEN
        GLint index = m_func->glGetUniformBlockIndex(shaderProgram.programId(), "ColorArray");
        m_glHelper.bindUniformBlock(shaderProgram.programId(), index, 1);

        // THEN
        const GLint error = m_func->glGetError();
        QVERIFY(error == 0);
    }

    void blendEquation()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        GLint equation = 0;
        m_func->glGetIntegerv(GL_BLEND_EQUATION_RGB, &equation);
        QCOMPARE(equation, GL_FUNC_ADD);

        // WHEN
        m_glHelper.blendEquation(GL_FUNC_REVERSE_SUBTRACT);

        // THEN
        m_func->glGetIntegerv(GL_BLEND_EQUATION_RGB, &equation);
        QCOMPARE(equation, GL_FUNC_REVERSE_SUBTRACT);
    }

    void blendFunci()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");
        // Not supported by OpenGL 3.3
    }

    void blendFuncSeparatei()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // Not supported by OpenGL 3.3
    }

    void boundFrameBufferObject()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        GLuint fboId;
        m_func->glGenFramebuffers(1, &fboId);

        // WHEN
        m_func->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboId);

        // THEN
        GLint boundBuffer = 0;
        m_func->glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &boundBuffer);
        QCOMPARE(GLuint(boundBuffer), fboId);

        // THEN
        QCOMPARE(m_glHelper.boundFrameBufferObject(), fboId);

        // Reset state
        m_func->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        m_func->glDeleteFramebuffers(1, &fboId);
    }

    void checkFrameBufferComplete()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");
        // GIVEN
        GLuint fboId;
        m_func->glGenFramebuffers(1, &fboId);

        Attachment attachment;
        attachment.m_point = QRenderTargetOutput::Color0;

        m_func->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboId);

        QOpenGLTexture texture(QOpenGLTexture::Target2D);
        texture.setSize(512, 512);
        texture.setFormat(QOpenGLTexture::RGBA8U);
        texture.setMinificationFilter(QOpenGLTexture::Linear);
        texture.setMagnificationFilter(QOpenGLTexture::Linear);
        texture.create();
        texture.allocateStorage();
        m_glHelper.bindFrameBufferAttachment(&texture, attachment);

        // THEN
        GLenum status = m_func->glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
        QVERIFY(status == GL_FRAMEBUFFER_COMPLETE);

        QVERIFY(m_glHelper.checkFrameBufferComplete());

        // Restore
        m_func->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        m_func->glDeleteFramebuffers(1, &fboId);
    }

    void clearBufferf()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        // GIVEN
        GLuint fboId;
        m_func->glGenFramebuffers(1, &fboId);

        // THEN
        QVERIFY(fboId != 0);

        // WHEN
        m_func->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboId);
        // Create 4 attachments
        QOpenGLTexture *textures[4];
        for (int i = 0; i < 4; ++i) {
            Attachment attachment;
            attachment.m_point = static_cast<QRenderTargetOutput::AttachmentPoint>(i);

            QOpenGLTexture *texture = new QOpenGLTexture(QOpenGLTexture::Target2D);
            textures[i] = texture;
            texture->setSize(512, 512);
            texture->setFormat(QOpenGLTexture::RGBA32F);
            texture->setMinificationFilter(QOpenGLTexture::Linear);
            texture->setMagnificationFilter(QOpenGLTexture::Linear);
            texture->setWrapMode(QOpenGLTexture::ClampToEdge);
            if (!texture->create())
                qWarning() << "Texture creation failed";
            texture->allocateStorage();
            QVERIFY(texture->isStorageAllocated());
            GLint error = m_func->glGetError();
            QVERIFY(error == 0);
            m_glHelper.bindFrameBufferAttachment(texture, attachment);
        }

        GLenum status = m_func->glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
        QVERIFY(status == GL_FRAMEBUFFER_COMPLETE);

        // Set Draw buffers
        GLenum clearBufferEnum = GL_COLOR_ATTACHMENT3;
        m_func->glDrawBuffers(1, &clearBufferEnum);

        const GLint bufferIndex = 0; // index of the element in the draw buffers
        GLint error = m_func->glGetError();
        QVERIFY(error == 0);

        // WHEN
        const QVector4D clearValue1 = QVector4D(0.5f, 0.2f, 0.4f, 0.8f);
        m_func->glClearBufferfv(GL_COLOR, bufferIndex, reinterpret_cast<const float *>(&clearValue1));
        error = m_func->glGetError();
        QVERIFY(error == 0);

        // THEN
        QVector<QVector4D> colors(512 * 512);
        textures[3]->bind();
        m_func->glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, colors.data());
        textures[3]->release();

        for (const QVector4D c : colors) {
            QVERIFY(c == clearValue1);
        }

        // WHEN
        const QVector4D clearValue2 = QVector4D(0.4f, 0.5f, 0.4f, 1.0f);
        m_glHelper.clearBufferf(bufferIndex, clearValue2);

        // THEN
        textures[3]->bind();
        m_func->glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, colors.data());
        textures[3]->release();
        for (const QVector4D c : colors) {
            QVERIFY(c == clearValue2);
        }
        // Restore
        m_func->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        m_func->glDeleteFramebuffers(1, &fboId);
        for (int i = 0; i < 4; ++i)
            delete textures[i];
    }

    void createFrameBufferObject()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // WHEN
        const GLuint fboId = m_glHelper.createFrameBufferObject();

        // THEN
        QVERIFY(fboId != 0);

        // Restore
        m_func->glDeleteFramebuffers(1, &fboId);
    }

    void depthMask()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        GLboolean depthWritingEnabled = false;
        m_func->glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWritingEnabled);

        // THEN
        QVERIFY(depthWritingEnabled);

        // WHEN
        m_glHelper.depthMask(GL_FALSE);

        // THEN
        m_func->glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWritingEnabled);
        QVERIFY(!depthWritingEnabled);

        // WHEN
        m_glHelper.depthMask(GL_TRUE);

        // THEN
        m_func->glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWritingEnabled);
        QVERIFY(depthWritingEnabled);
    }

    void depthTest()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        m_func->glDisable(GL_DEPTH_TEST);
        m_func->glDepthFunc(GL_LESS);

        // WHEN
        m_glHelper.depthTest(GL_LEQUAL);

        // THEN
        QVERIFY(m_func->glIsEnabled(GL_DEPTH_TEST));
        GLint depthMode = 0;
        m_func->glGetIntegerv(GL_DEPTH_FUNC, &depthMode);
        QCOMPARE(depthMode, GL_LEQUAL);

        // WHEN
        m_glHelper.depthTest(GL_LESS);
        QVERIFY(m_func->glIsEnabled(GL_DEPTH_TEST));
        m_func->glGetIntegerv(GL_DEPTH_FUNC, &depthMode);
        QCOMPARE(depthMode, GL_LESS);
    }

    void disableClipPlane()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        m_func->glEnable(GL_CLIP_DISTANCE0 + 5);

        // THEN
        QVERIFY(m_func->glIsEnabled(GL_CLIP_DISTANCE0 + 5));

        // WHEN
        m_glHelper.disableClipPlane(5);

        // THEN
        QVERIFY(!m_func->glIsEnabled(GL_CLIP_DISTANCE0 + 5));
    }

    void disablei()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        m_func->glEnablei(GL_BLEND, 2);

        // THEN
        QVERIFY(m_func->glIsEnabledi(GL_BLEND, 2));

        // WHEN
        m_glHelper.disablei(GL_BLEND, 2);

        // THEN
        QVERIFY(!m_func->glIsEnabledi(GL_BLEND, 2));
    }

    void disablePrimitiveRestart()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        m_func->glEnable(GL_PRIMITIVE_RESTART);

        // THEN
        QVERIFY(m_func->glIsEnabled(GL_PRIMITIVE_RESTART));

        // WHEN
        m_glHelper.disablePrimitiveRestart();

        // THEN
        QVERIFY(!m_func->glIsEnabled(GL_PRIMITIVE_RESTART));
    }

    void drawBuffers()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        GLuint fboId;
        m_func->glGenFramebuffers(1, &fboId);

        // THEN
        QVERIFY(fboId != 0);

        // WHEN
        m_func->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboId);
        QOpenGLTexture *textures[4];

        // Create 4 attachments
        for (int i = 0; i < 4; ++i) {
            Attachment attachment;
            attachment.m_point = static_cast<QRenderTargetOutput::AttachmentPoint>(i);

            QOpenGLTexture *texture = new QOpenGLTexture(QOpenGLTexture::Target2D);
            textures[i] = texture;
            texture->setSize(512, 512);
            texture->setFormat(QOpenGLTexture::RGBA32F);
            texture->setMinificationFilter(QOpenGLTexture::Linear);
            texture->setMagnificationFilter(QOpenGLTexture::Linear);
            texture->setWrapMode(QOpenGLTexture::ClampToEdge);
            if (!texture->create())
                qWarning() << "Texture creation failed";
            texture->allocateStorage();
            QVERIFY(texture->isStorageAllocated());
            GLint error = m_func->glGetError();
            QVERIFY(error == 0);
            m_glHelper.bindFrameBufferAttachment(texture, attachment);
        }
        // THEN
        GLenum status = m_func->glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
        QVERIFY(status == GL_FRAMEBUFFER_COMPLETE);

        // WHEN
        GLenum bufferEnum = GL_COLOR_ATTACHMENT4;
        m_func->glDrawBuffers(1, &bufferEnum);

        // THEN
        GLint enumValue = -1;
        m_func->glGetIntegerv(GL_DRAW_BUFFER0, &enumValue);
        QCOMPARE(enumValue, GL_COLOR_ATTACHMENT4);

        // WHEN
        GLint newBufferEnum = 2;
        m_glHelper.drawBuffers(1, &newBufferEnum);

        // THEN
        m_func->glGetIntegerv(GL_DRAW_BUFFER0, &enumValue);
        QCOMPARE(enumValue, GL_COLOR_ATTACHMENT0 + newBufferEnum);

        // WHEN
        newBufferEnum = 0;
        m_glHelper.drawBuffers(1, &newBufferEnum);

        // THEN
        m_func->glGetIntegerv(GL_DRAW_BUFFER0, &enumValue);
        QCOMPARE(enumValue, GL_COLOR_ATTACHMENT0 + newBufferEnum);

        // Restore
        m_func->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        m_func->glDeleteFramebuffers(1, &fboId);
        for (int i = 0; i < 4; ++i)
            delete textures[i];
    }

    void enableClipPlane()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        m_func->glDisable(GL_CLIP_DISTANCE0 + 4);

        // THEN
        QVERIFY(!m_func->glIsEnabled(GL_CLIP_DISTANCE0 + 4));

        // WHEN
        m_glHelper.enableClipPlane(4);

        // THEN
        QVERIFY(m_func->glIsEnabled(GL_CLIP_DISTANCE0 + 4));
    }

    void enablei()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        m_func->glDisablei(GL_BLEND, 4);

        // THEN
        QVERIFY(!m_func->glIsEnabledi(GL_BLEND, 4));

        // WHEN
        m_glHelper.enablei(GL_BLEND, 4);

        // THEN
        QVERIFY(m_func->glIsEnabledi(GL_BLEND, 4));
    }

    void enablePrimitiveRestart()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        m_func->glDisable(GL_PRIMITIVE_RESTART);

        // THEN
        QVERIFY(!m_func->glIsEnabled(GL_PRIMITIVE_RESTART));

        // WHEN
        m_glHelper.enablePrimitiveRestart(883);

        // THEN
        QVERIFY(m_func->glIsEnabled(GL_PRIMITIVE_RESTART));
        GLint restartIndex = 0;
        m_func->glGetIntegerv(GL_PRIMITIVE_RESTART_INDEX, &restartIndex);
        QCOMPARE(restartIndex, 883);

        // Restore
        m_func->glDisable(GL_PRIMITIVE_RESTART);
    }

    void frontFace()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        m_func->glFrontFace(GL_CW);

        // THEN
        GLint face = 0;
        m_func->glGetIntegerv(GL_FRONT_FACE, &face);
        QCOMPARE(face, GL_CW);

        // WHEN
        m_glHelper.frontFace(GL_CCW);

        // THEN
        m_func->glGetIntegerv(GL_FRONT_FACE, &face);
        QCOMPARE(face, GL_CCW);
    }

    void getRenderBufferDimensions()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        GLuint renderBufferId = 0;
        m_func->glGenRenderbuffers(1, &renderBufferId);
        QVERIFY(renderBufferId != 0);

        // WHEN
        m_func->glBindRenderbuffer(GL_RENDERBUFFER, renderBufferId);
        m_func->glRenderbufferStorage(GL_RENDERBUFFER, GL_SRGB8_ALPHA8, 512, 512);
        m_func->glBindRenderbuffer(GL_RENDERBUFFER, 0);
        const QSize dimensions = m_glHelper.getRenderBufferDimensions(renderBufferId);

        // THEN
        QCOMPARE(dimensions, QSize(512, 512));

        // Restore
        m_func->glDeleteRenderbuffers(1, &renderBufferId);
    }

    void getTextureDimensions()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        QOpenGLTexture texture(QOpenGLTexture::Target2D);
        texture.setSize(512, 512);
        texture.setFormat(QOpenGLTexture::RGBA8U);
        texture.setMinificationFilter(QOpenGLTexture::Linear);
        texture.setMagnificationFilter(QOpenGLTexture::Linear);
        texture.create();
        texture.allocateStorage();

        // WHEN
        const QSize dimensions = m_glHelper.getTextureDimensions(texture.textureId(), GL_TEXTURE_2D);

        // THEN
        QCOMPARE(dimensions, QSize(512, 512));
    }

    void pointSize()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        m_func->glEnable(GL_PROGRAM_POINT_SIZE);

        // THEN
        QVERIFY(m_func->glIsEnabled(GL_PROGRAM_POINT_SIZE));
        GLfloat size = 0;
        m_func->glGetFloatv(GL_POINT_SIZE, &size);
        QCOMPARE(size, 1.0f);

        // WHEN
        m_glHelper.pointSize(false, 0.5f);

        // THEN
        QVERIFY(!m_func->glIsEnabled(GL_PROGRAM_POINT_SIZE));
        m_func->glGetFloatv(GL_POINT_SIZE, &size);
        QCOMPARE(size, 0.5f);
    }

    void maxClipPlaneCount()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        GLint maxCount = -1;
        m_func->glGetIntegerv(GL_MAX_CLIP_PLANES, &maxCount);

        // THEN
        QCOMPARE(maxCount, m_glHelper.maxClipPlaneCount());
    }

    void programUniformBlock()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        QOpenGLShaderProgram shaderProgram;
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertCodeUniformBuffer);
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragCodeUniformBuffer);
        QVERIFY(shaderProgram.link());

        // WHEN
        const QVector<ShaderUniformBlock> activeUniformBlocks = m_glHelper.programUniformBlocks(shaderProgram.programId());

        // THEN
        QCOMPARE(activeUniformBlocks.size(), 1);
        const ShaderUniformBlock uniformBlock = activeUniformBlocks.first();

        QCOMPARE(uniformBlock.m_activeUniformsCount, 1);
        QCOMPARE(uniformBlock.m_name, QStringLiteral("ColorArray"));

        GLint blockIndex = m_func->glGetUniformBlockIndex(shaderProgram.programId(), "ColorArray");
        GLint blockBinding = -1;
        m_func->glGetActiveUniformBlockiv(shaderProgram.programId(), blockIndex, GL_UNIFORM_BLOCK_BINDING, &blockBinding);
        QCOMPARE(blockIndex, uniformBlock.m_index);
        QCOMPARE(blockBinding, uniformBlock.m_binding);
    }

    void programAttributesAndLocations()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        QOpenGLShaderProgram shaderProgram;
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertCode);
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragCodeSamplers);
        QVERIFY(shaderProgram.link());

        // WHEN
        QVector<ShaderAttribute> activeAttributes = m_glHelper.programAttributesAndLocations(shaderProgram.programId());

        // THEN
        QCOMPARE(activeAttributes.size(), 2);
        std::sort(activeAttributes.begin(), activeAttributes.end(), [] (const ShaderAttribute &a, const ShaderAttribute &b) { return a.m_name < b.m_name; });

        const ShaderAttribute attribute1 = activeAttributes.at(0);
        QCOMPARE(attribute1.m_name, QStringLiteral("vertexPosition"));
        QCOMPARE(attribute1.m_size, 1);
        QCOMPARE(attribute1.m_location, shaderProgram.attributeLocation("vertexPosition"));
        QCOMPARE(attribute1.m_type, GLenum(GL_FLOAT_VEC3));

        const ShaderAttribute attribute2 = activeAttributes.at(1);
        QCOMPARE(attribute2.m_name, QStringLiteral("vertexTexCoord"));
        QCOMPARE(attribute2.m_size, 1);
        QCOMPARE(attribute2.m_location, shaderProgram.attributeLocation("vertexTexCoord"));
        QCOMPARE(attribute2.m_type, GLenum(GL_FLOAT_VEC2));
    }

    void programUniformsAndLocations()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        QOpenGLShaderProgram shaderProgram;
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertCode);
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragCodeUniformsFloat);
        QVERIFY(shaderProgram.link());

        // WHEN
        QVector<ShaderUniform> activeUniforms = m_glHelper.programUniformsAndLocations(shaderProgram.programId());

        // THEN
        QCOMPARE(activeUniforms.size(), 4);
        std::sort(activeUniforms.begin(), activeUniforms.end(), [] (const ShaderUniform &a, const ShaderUniform &b) { return a.m_name < b.m_name; });

        const ShaderUniform uniform1 = activeUniforms.at(0);
        QCOMPARE(uniform1.m_location, shaderProgram.uniformLocation("multiplier"));
        QCOMPARE(uniform1.m_offset, -1);
        QCOMPARE(uniform1.m_blockIndex, -1);
        QCOMPARE(uniform1.m_arrayStride, -1);
        QCOMPARE(uniform1.m_matrixStride, -1);
        QCOMPARE(uniform1.m_size, 1);
        QCOMPARE(uniform1.m_type, GLenum(GL_FLOAT));
        QCOMPARE(uniform1.m_name, QStringLiteral("multiplier"));

        const ShaderUniform uniform2 = activeUniforms.at(1);
        QCOMPARE(uniform2.m_location, shaderProgram.uniformLocation("multiplierVec2"));
        QCOMPARE(uniform2.m_offset, -1);
        QCOMPARE(uniform2.m_blockIndex, -1);
        QCOMPARE(uniform2.m_arrayStride, -1);
        QCOMPARE(uniform2.m_matrixStride, -1);
        QCOMPARE(uniform2.m_size, 1);
        QCOMPARE(uniform2.m_type, GLenum(GL_FLOAT_VEC2));
        QCOMPARE(uniform2.m_name, QStringLiteral("multiplierVec2"));

        const ShaderUniform uniform3 = activeUniforms.at(2);
        QCOMPARE(uniform3.m_location, shaderProgram.uniformLocation("multiplierVec3"));
        QCOMPARE(uniform3.m_offset, -1);
        QCOMPARE(uniform3.m_blockIndex, -1);
        QCOMPARE(uniform3.m_arrayStride, -1);
        QCOMPARE(uniform3.m_matrixStride, -1);
        QCOMPARE(uniform3.m_size, 1);
        QCOMPARE(uniform3.m_type, GLenum(GL_FLOAT_VEC3));
        QCOMPARE(uniform3.m_name, QStringLiteral("multiplierVec3"));

        const ShaderUniform uniform4 = activeUniforms.at(3);
        QCOMPARE(uniform4.m_location, shaderProgram.uniformLocation("multiplierVec4"));
        QCOMPARE(uniform4.m_offset, -1);
        QCOMPARE(uniform4.m_blockIndex, -1);
        QCOMPARE(uniform4.m_arrayStride, -1);
        QCOMPARE(uniform4.m_matrixStride, -1);
        QCOMPARE(uniform4.m_size, 1);
        QCOMPARE(uniform4.m_type, GLenum(GL_FLOAT_VEC4));
        QCOMPARE(uniform4.m_name, QStringLiteral("multiplierVec4"));
    }

    void programShaderStorageBlock()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // Not supported in 3.3
    }

    void releaseFrameBufferObject()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        GLuint fboId;
        m_func->glGenFramebuffers(1, &fboId);

        // THEN
        QVERIFY(fboId != 0);

        // WHEN
        m_glHelper.releaseFrameBufferObject(fboId);

        // THEN
        QVERIFY(!m_func->glIsFramebuffer(fboId));
    }

    void setMSAAEnabled()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        m_func->glDisable(GL_MULTISAMPLE);

        // THEN
        QVERIFY(!m_func->glIsEnabled(GL_MULTISAMPLE));

        // WHEN
        m_glHelper.setMSAAEnabled(true);

        // THEN
        QVERIFY(m_func->glIsEnabled(GL_MULTISAMPLE));

        // WHEN
        m_glHelper.setMSAAEnabled(false);

        // THEN
        QVERIFY(!m_func->glIsEnabled(GL_MULTISAMPLE));
    }

    void setAlphaCoverageEnabled()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        m_func->glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);

        // THEN
        QVERIFY(!m_func->glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE));

        // WHEN
        m_glHelper.setAlphaCoverageEnabled(true);

        // THEN
        QVERIFY(m_func->glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE));

        // WHEN
        m_glHelper.setAlphaCoverageEnabled(false);

        // THEN
        QVERIFY(!m_func->glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE));
    }

    void setClipPlane()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // Deprecated in 3.3 core
    }

    void setSeamlessCubemap()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        m_func->glDisable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
        QVERIFY(!m_func->glIsEnabled(GL_TEXTURE_CUBE_MAP_SEAMLESS));

        // WHEN
        m_glHelper.setSeamlessCubemap(true);

        // THEN
        QVERIFY(m_func->glIsEnabled(GL_TEXTURE_CUBE_MAP_SEAMLESS));

        // WHEN
        m_glHelper.setSeamlessCubemap(false);

        // THEN
        QVERIFY(!m_func->glIsEnabled(GL_TEXTURE_CUBE_MAP_SEAMLESS));
    }

    void setVerticesPerPatch()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        m_func->glDisable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

        // THEN
        QVERIFY(!m_func->glIsEnabled(GL_TEXTURE_CUBE_MAP_SEAMLESS));

        // WHEN
        m_glHelper.setSeamlessCubemap(true);

        // THEN
        QVERIFY(m_func->glIsEnabled(GL_TEXTURE_CUBE_MAP_SEAMLESS));

        // WHEN
        m_glHelper.setSeamlessCubemap(false);

        // THEN
        QVERIFY(!m_func->glIsEnabled(GL_TEXTURE_CUBE_MAP_SEAMLESS));
    }

#define SUPPORTS_FEATURE(Feature, IsSupported) \
    QVERIFY(m_glHelper.supportsFeature(Feature) == IsSupported);

    void supportsFeature()
    {
        SUPPORTS_FEATURE(GraphicsHelperInterface::MRT, true);
        SUPPORTS_FEATURE(GraphicsHelperInterface::UniformBufferObject, true);
        SUPPORTS_FEATURE(GraphicsHelperInterface::BindableFragmentOutputs, true);
        SUPPORTS_FEATURE(GraphicsHelperInterface::PrimitiveRestart, true);
        SUPPORTS_FEATURE(GraphicsHelperInterface::RenderBufferDimensionRetrieval, true);
        SUPPORTS_FEATURE(GraphicsHelperInterface::TextureDimensionRetrieval, true);
        SUPPORTS_FEATURE(GraphicsHelperInterface::UniformBufferObject, true);
        SUPPORTS_FEATURE(GraphicsHelperInterface::ShaderStorageObject, false);
        SUPPORTS_FEATURE(GraphicsHelperInterface::Compute, false);
        SUPPORTS_FEATURE(GraphicsHelperInterface::DrawBuffersBlend, false);
        // Tesselation could be true or false depending on extensions so not tested
        SUPPORTS_FEATURE(GraphicsHelperInterface::BlitFramebuffer, true);
    }


#define ADD_UNIFORM_ENTRY(FragShader, Name, Type, ComponentSize, ExpectedRawSize) \
    QTest::newRow(#FragShader"_"#Type) << FragShader << QStringLiteral(Name) << Type << ComponentSize << ExpectedRawSize;

    void uniformsByteSize_data()
    {
        QTest::addColumn<QByteArray>("fragShader");
        QTest::addColumn<QString>("name");
        QTest::addColumn<int>("type");
        QTest::addColumn<int>("componentSize");
        QTest::addColumn<int>("expectedByteSize");

        ADD_UNIFORM_ENTRY(fragCodeUniformsFloat, "multiplier", GL_FLOAT, 1, 4);
        ADD_UNIFORM_ENTRY(fragCodeUniformsFloat, "multiplierVec2", GL_FLOAT_VEC2, 1, 4 * 2);
        ADD_UNIFORM_ENTRY(fragCodeUniformsFloat, "multiplierVec3",GL_FLOAT_VEC3, 1, 4 * 3);
        ADD_UNIFORM_ENTRY(fragCodeUniformsFloat, "multiplierVec4", GL_FLOAT_VEC4, 1, 4 * 4);

        ADD_UNIFORM_ENTRY(fragCodeUniformsInt, "multiplier", GL_INT, 1, 4);
        ADD_UNIFORM_ENTRY(fragCodeUniformsInt, "multiplierVec2", GL_INT_VEC2, 1, 4 * 2);
        ADD_UNIFORM_ENTRY(fragCodeUniformsInt, "multiplierVec3", GL_INT_VEC3, 1, 4 * 3);
        ADD_UNIFORM_ENTRY(fragCodeUniformsInt, "multiplierVec4", GL_INT_VEC4, 1, 4 * 4);

        ADD_UNIFORM_ENTRY(fragCodeUniformsUInt, "multiplier", GL_UNSIGNED_INT, 1, 4);
        ADD_UNIFORM_ENTRY(fragCodeUniformsUInt, "multiplierVec2", GL_UNSIGNED_INT_VEC2, 1, 4 * 2);
        ADD_UNIFORM_ENTRY(fragCodeUniformsUInt, "multiplierVec3", GL_UNSIGNED_INT_VEC3, 1, 4 * 3);
        ADD_UNIFORM_ENTRY(fragCodeUniformsUInt, "multiplierVec4", GL_UNSIGNED_INT_VEC4, 1, 4 * 4);

        ADD_UNIFORM_ENTRY(fragCodeUniformsFloatMatrices, "m2",  GL_FLOAT_MAT2, 1, 4 * 2 * 2);
        ADD_UNIFORM_ENTRY(fragCodeUniformsFloatMatrices, "m23", GL_FLOAT_MAT2x3, 1, 4 * 2 * 3);
        ADD_UNIFORM_ENTRY(fragCodeUniformsFloatMatrices, "m32", GL_FLOAT_MAT3x2, 1, 4 * 3 * 2);
        ADD_UNIFORM_ENTRY(fragCodeUniformsFloatMatrices, "m24", GL_FLOAT_MAT2x4, 1, 4 * 2 * 4);
        ADD_UNIFORM_ENTRY(fragCodeUniformsFloatMatrices, "m42", GL_FLOAT_MAT4x2, 1, 4 * 4 * 2);
        ADD_UNIFORM_ENTRY(fragCodeUniformsFloatMatrices, "m3", GL_FLOAT_MAT3, 1, 4 * 3 * 3);
        ADD_UNIFORM_ENTRY(fragCodeUniformsFloatMatrices, "m34", GL_FLOAT_MAT3x4, 1, 4 * 3 * 4);
        ADD_UNIFORM_ENTRY(fragCodeUniformsFloatMatrices, "m43", GL_FLOAT_MAT4x3, 1, 4 * 4 * 3);
        ADD_UNIFORM_ENTRY(fragCodeUniformsFloatMatrices, "m4", GL_FLOAT_MAT4, 1, 4 * 4 * 4);

        ADD_UNIFORM_ENTRY(fragCodeSamplers, "s1", GL_SAMPLER_1D, 1, 4);
        ADD_UNIFORM_ENTRY(fragCodeSamplers, "s2", GL_SAMPLER_2D, 1, 4);
        ADD_UNIFORM_ENTRY(fragCodeSamplers, "s2a", GL_SAMPLER_2D_ARRAY, 1, 4);
        ADD_UNIFORM_ENTRY(fragCodeSamplers, "s3", GL_SAMPLER_3D, 1, 4);
        ADD_UNIFORM_ENTRY(fragCodeSamplers, "scube", GL_SAMPLER_CUBE, 1, 4);
        ADD_UNIFORM_ENTRY(fragCodeSamplers, "srect", GL_SAMPLER_2D_RECT, 1, 4);
    }

    void uniformsByteSize()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        QFETCH(QByteArray, fragShader);
        QFETCH(QString, name);
        QFETCH(int, type);
        QFETCH(int, componentSize);
        QFETCH(int, expectedByteSize);

        QOpenGLShaderProgram shaderProgram;
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertCode);
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragShader);
        QVERIFY(shaderProgram.link());

        GLint location = shaderProgram.uniformLocation(name);
        // WHEN
        const QVector<ShaderUniform> activeUniforms = m_glHelper.programUniformsAndLocations(shaderProgram.programId());
        ShaderUniform matchingUniform;
        for (const ShaderUniform &u : activeUniforms) {
            if (u.m_location == location) {
                matchingUniform = u;
                break;
            }
        }


        // THEN
        QCOMPARE(matchingUniform.m_location, location);
        QCOMPARE(matchingUniform.m_type, GLuint(type));
        QCOMPARE(matchingUniform.m_size, componentSize);

        // WHEN
        const int computedRawByteSize = m_glHelper.uniformByteSize(matchingUniform);

        // THEN
        QCOMPARE(expectedByteSize, computedRawByteSize);

        // Restore
        m_func->glUseProgram(0);
    }


    void useProgram()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        QOpenGLShaderProgram shaderProgram;
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertCode);
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragCodeFragOutputs);

        // THEN
        QVERIFY(shaderProgram.link());

        GLint currentProg = 0;
        m_func->glGetIntegerv(GL_CURRENT_PROGRAM, &currentProg);
        QVERIFY(currentProg == 0);

        // WHEN
        m_glHelper.useProgram(shaderProgram.programId());

        // THEN
        m_func->glGetIntegerv(GL_CURRENT_PROGRAM, &currentProg);
        QCOMPARE(GLuint(currentProg), shaderProgram.programId());

        // WHEN
        m_glHelper.useProgram(0);

        // THEN
        m_func->glGetIntegerv(GL_CURRENT_PROGRAM, &currentProg);
        QVERIFY(currentProg == 0);
    }

    void vertexAttribDivisor()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");
    }

    void glUniform1fv()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        QOpenGLShaderProgram shaderProgram;
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertCode);
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragCodeUniformsFloat);
        QVERIFY(shaderProgram.link());

        // WHEN
        m_func->glUseProgram(shaderProgram.programId());
        GLfloat value = 883.0f;
        const GLint location = shaderProgram.uniformLocation("multiplier");
        m_glHelper.glUniform1fv(location, 1, &value);

        // THEN
        GLfloat setValue = 0.0f;
        m_func->glGetUniformfv(shaderProgram.programId(), location, &setValue);
        QCOMPARE(value, setValue);

        // Restore
        m_func->glUseProgram(0);
    }

    void glUniform2fv()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        QOpenGLShaderProgram shaderProgram;
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertCode);
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragCodeUniformsFloat);
        QVERIFY(shaderProgram.link());

        // WHEN
        m_func->glUseProgram(shaderProgram.programId());
        GLfloat values[2] = { 383.0f, 427.0f };
        const GLint location = shaderProgram.uniformLocation("multiplierVec2");
        m_glHelper.glUniform2fv(location, 1, values);

        // THEN
        GLfloat setValues[2] = { 0.0f, 0.0f };
        m_func->glGetUniformfv(shaderProgram.programId(), location, setValues);
        for (int i = 0; i < 2; ++i)
            QCOMPARE(setValues[i], values[i]);

        // Restore
        m_func->glUseProgram(0);
    }

    void glUniform3fv()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        QOpenGLShaderProgram shaderProgram;
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertCode);
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragCodeUniformsFloat);
        QVERIFY(shaderProgram.link());

        // WHEN
        m_func->glUseProgram(shaderProgram.programId());
        GLfloat values[3] = { 572.0f, 1340.0f, 1584.0f };
        const GLint location = shaderProgram.uniformLocation("multiplierVec3");
        m_glHelper.glUniform3fv(location, 1, values);

        // THEN
        GLfloat setValues[3] = { 0.0f, 0.0f, 0.0f };
        m_func->glGetUniformfv(shaderProgram.programId(), location, setValues);
        for (int i = 0; i < 3; ++i)
            QCOMPARE(setValues[i], values[i]);

        // Restore
        m_func->glUseProgram(0);
    }

    void glUniform4fv()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        QOpenGLShaderProgram shaderProgram;
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertCode);
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragCodeUniformsFloat);
        QVERIFY(shaderProgram.link());

        // WHEN
        m_func->glUseProgram(shaderProgram.programId());
        GLfloat values[4] = { 454.0f, 350.0f, 883.0f, 355.0f };
        const GLint location = shaderProgram.uniformLocation("multiplierVec4");
        m_glHelper.glUniform4fv(location, 1, values);

        // THEN
        GLfloat setValues[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        m_func->glGetUniformfv(shaderProgram.programId(), location, setValues);
        for (int i = 0; i < 4; ++i)
            QCOMPARE(setValues[i], values[i]);

        // Restore
        m_func->glUseProgram(0);
    }

    void glUniform1iv()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        QOpenGLShaderProgram shaderProgram;
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertCode);
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragCodeUniformsInt);
        QVERIFY(shaderProgram.link());

        // WHEN
        m_func->glUseProgram(shaderProgram.programId());
        GLint value = 883;
        const GLint location = shaderProgram.uniformLocation("multiplier");
        m_glHelper.glUniform1iv(location, 1, &value);

        // THEN
        GLint setValue = 0;
        m_func->glGetUniformiv(shaderProgram.programId(), location, &setValue);
        QCOMPARE(value, setValue);

        // Restore
        m_func->glUseProgram(0);
    }

    void glUniform2iv()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        QOpenGLShaderProgram shaderProgram;
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertCode);
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragCodeUniformsInt);
        QVERIFY(shaderProgram.link());

        // WHEN
        m_func->glUseProgram(shaderProgram.programId());
        GLint values[2] = { 383, 427 };
        const GLint location = shaderProgram.uniformLocation("multiplierVec2");
        m_glHelper.glUniform2iv(location, 1, values);

        // THEN
        GLint setValues[2] = { 0, 0 };
        m_func->glGetUniformiv(shaderProgram.programId(), location, setValues);
        for (int i = 0; i < 2; ++i)
            QCOMPARE(values[i], setValues[i]);

        // Restore
        m_func->glUseProgram(0);
    }

    void glUniform3iv()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        QOpenGLShaderProgram shaderProgram;
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertCode);
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragCodeUniformsInt);
        QVERIFY(shaderProgram.link());

        // WHEN
        m_func->glUseProgram(shaderProgram.programId());
        GLint values[3] = { 572, 1340, 1584 };
        const GLint location = shaderProgram.uniformLocation("multiplierVec3");
        m_glHelper.glUniform3iv(location, 1, values);

        // THEN
        GLint setValues[3] = { 0, 0, 0 };
        m_func->glGetUniformiv(shaderProgram.programId(), location, setValues);
        for (int i = 0; i < 3; ++i)
            QCOMPARE(values[i], setValues[i]);

        // Restore
        m_func->glUseProgram(0);
    }

    void glUniform4iv()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        QOpenGLShaderProgram shaderProgram;
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertCode);
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragCodeUniformsInt);
        QVERIFY(shaderProgram.link());

        // WHEN
        m_func->glUseProgram(shaderProgram.programId());
        GLint values[4] = { 454, 350, 883, 355 };
        const GLint location = shaderProgram.uniformLocation("multiplierVec4");
        m_glHelper.glUniform4iv(location, 1, values);

        // THEN
        GLint setValues[4] = { 0, 0, 0, 0 };
        m_func->glGetUniformiv(shaderProgram.programId(), location, setValues);
        for (int i = 0; i < 4; ++i)
            QCOMPARE(values[i], setValues[i]);

        // Restore
        m_func->glUseProgram(0);
    }

    void glUniform1uiv()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        QOpenGLShaderProgram shaderProgram;
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertCode);
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragCodeUniformsUInt);
        QVERIFY(shaderProgram.link());

        // WHEN
        m_func->glUseProgram(shaderProgram.programId());
        GLuint value = 883U;
        const GLint location = shaderProgram.uniformLocation("multiplier");
        m_glHelper.glUniform1uiv(location, 1, &value);

        // THEN
        GLuint setValue = 0U;
        m_func->glGetUniformuiv(shaderProgram.programId(), location, &setValue);
        QCOMPARE(value, setValue);

        // Restore
        m_func->glUseProgram(0);
    }

    void glUniform2uiv()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        QOpenGLShaderProgram shaderProgram;
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertCode);
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragCodeUniformsUInt);
        QVERIFY(shaderProgram.link());

        // WHEN
        m_func->glUseProgram(shaderProgram.programId());
        GLuint values[2] = { 383U, 427U };
        const GLint location = shaderProgram.uniformLocation("multiplierVec2");
        m_glHelper.glUniform2uiv(location, 1, values);

        // THEN
        GLuint setValues[2] = { 0U, 0U };
        m_func->glGetUniformuiv(shaderProgram.programId(), location, setValues);
        for (int i = 0; i < 2; ++i)
            QCOMPARE(values[i], setValues[i]);

        // Restore
        m_func->glUseProgram(0);
    }

    void glUniform3uiv()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        QOpenGLShaderProgram shaderProgram;
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertCode);
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragCodeUniformsUInt);
        QVERIFY(shaderProgram.link());

        // WHEN
        m_func->glUseProgram(shaderProgram.programId());
        GLuint values[3] = { 572U, 1340U, 1584U };
        const GLint location = shaderProgram.uniformLocation("multiplierVec3");
        m_glHelper.glUniform3uiv(location, 1, values);

        // THEN
        GLuint setValues[3] = { 0U, 0U, 0U };
        m_func->glGetUniformuiv(shaderProgram.programId(), location, setValues);
        for (int i = 0; i < 3; ++i)
            QCOMPARE(values[i], setValues[i]);

        // Restore
        m_func->glUseProgram(0);
    }

    void glUniform4uiv()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        QOpenGLShaderProgram shaderProgram;
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertCode);
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragCodeUniformsUInt);
        QVERIFY(shaderProgram.link());

        // WHEN
        m_func->glUseProgram(shaderProgram.programId());
        GLuint values[4] = { 454U, 350U, 883U, 355U };
        const GLint location = shaderProgram.uniformLocation("multiplierVec4");
        m_glHelper.glUniform4uiv(location, 1, values);

        // THEN
        GLuint setValues[4] = { 0U, 0U, 0U, 0U };
        m_func->glGetUniformuiv(shaderProgram.programId(), location, setValues);
        for (int i = 0; i < 4; ++i)
            QCOMPARE(values[i], setValues[i]);

        // Restore
        m_func->glUseProgram(0);
    }

    void glUniformMatrix2fv()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        QOpenGLShaderProgram shaderProgram;
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertCode);
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragCodeUniformsFloatMatrices);
        QVERIFY(shaderProgram.link());

        // WHEN
        m_func->glUseProgram(shaderProgram.programId());
        GLfloat values[4] = { 454.0f, 350.0f, 883.0f, 355.0f };
        const GLint location = shaderProgram.uniformLocation("m2");
        m_glHelper.glUniformMatrix2fv(location, 1, values);

        // THEN
        GLfloat setValues[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        m_func->glGetUniformfv(shaderProgram.programId(), location, setValues);
        for (int i = 0; i < 4; ++i)
            QCOMPARE(values[i], setValues[i]);

        // Restore
        m_func->glUseProgram(0);
    }

    void glUniformMatrix3fv()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        QOpenGLShaderProgram shaderProgram;
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertCode);
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragCodeUniformsFloatMatrices);
        QVERIFY(shaderProgram.link());

        // WHEN
        m_func->glUseProgram(shaderProgram.programId());
        GLfloat values[9] = { 454.0f, 350.0f, 883.0f, 355.0f, 1340.0f, 1584.0f, 1200.0f, 427.0f, 396.0f };
        const GLint location = shaderProgram.uniformLocation("m3");
        m_glHelper.glUniformMatrix3fv(location, 1, values);

        // THEN
        GLfloat setValues[9] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        m_func->glGetUniformfv(shaderProgram.programId(), location, setValues);
        for (int i = 0; i < 9; ++i)
            QCOMPARE(values[i], setValues[i]);

        // Restore
        m_func->glUseProgram(0);
    }

    void glUniformMatrix4fv()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        QOpenGLShaderProgram shaderProgram;
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertCode);
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragCodeUniformsFloatMatrices);
        QVERIFY(shaderProgram.link());

        // WHEN
        m_func->glUseProgram(shaderProgram.programId());
        GLfloat values[16] = { 454.0f, 350.0f, 883.0f, 355.0f, 1340.0f, 1584.0f, 1200.0f, 427.0f, 396.0f, 1603.0f, 55.0f, 5.7f, 383.0f, 6.2f, 5.3f, 327.0f };
        const GLint location = shaderProgram.uniformLocation("m4");
        m_glHelper.glUniformMatrix4fv(location, 1, values);

        // THEN
        GLfloat setValues[16] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        m_func->glGetUniformfv(shaderProgram.programId(), location, setValues);
        for (int i = 0; i < 16; ++i)
            QCOMPARE(values[i], setValues[i]);

        // Restore
        m_func->glUseProgram(0);
    }

    void glUniformMatrix2x3fv()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        QOpenGLShaderProgram shaderProgram;
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertCode);
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragCodeUniformsFloatMatrices);
        QVERIFY(shaderProgram.link());

        // WHEN
        m_func->glUseProgram(shaderProgram.programId());
        GLfloat values[6] = { 454.0f, 350.0f, 883.0f, 355.0f, 1340.0f, 1584.0f};
        const GLint location = shaderProgram.uniformLocation("m23");
        m_glHelper.glUniformMatrix2x3fv(location, 1, values);

        // THEN
        GLfloat setValues[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        m_func->glGetUniformfv(shaderProgram.programId(), location, setValues);
        for (int i = 0; i < 6; ++i)
            QCOMPARE(values[i], setValues[i]);

        // Restore
        m_func->glUseProgram(0);
    }

    void glUniformMatrix3x2fv()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        QOpenGLShaderProgram shaderProgram;
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertCode);
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragCodeUniformsFloatMatrices);
        QVERIFY(shaderProgram.link());

        // WHEN
        m_func->glUseProgram(shaderProgram.programId());
        GLfloat values[6] = { 454.0f, 350.0f, 883.0f, 355.0f, 1340.0f, 1584.0f};
        const GLint location = shaderProgram.uniformLocation("m32");
        m_glHelper.glUniformMatrix3x2fv(location, 1, values);

        // THEN
        GLfloat setValues[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        m_func->glGetUniformfv(shaderProgram.programId(), location, setValues);
        for (int i = 0; i < 6; ++i)
            QCOMPARE(values[i], setValues[i]);

        // Restore
        m_func->glUseProgram(0);
    }

    void glUniformMatrix2x4fv()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        QOpenGLShaderProgram shaderProgram;
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertCode);
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragCodeUniformsFloatMatrices);
        QVERIFY(shaderProgram.link());

        // WHEN
        m_func->glUseProgram(shaderProgram.programId());
        GLfloat values[8] = { 383.0f, 427.0f, 454.0f, 350.0f, 883.0f, 355.0f, 1340.0f, 1584.0f};
        const GLint location = shaderProgram.uniformLocation("m24");
        m_glHelper.glUniformMatrix2x4fv(location, 1, values);

        // THEN
        GLfloat setValues[8] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        m_func->glGetUniformfv(shaderProgram.programId(), location, setValues);
        for (int i = 0; i < 8; ++i)
            QCOMPARE(values[i], setValues[i]);

        // Restore
        m_func->glUseProgram(0);
    }

    void glUniformMatrix4x2fv()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        QOpenGLShaderProgram shaderProgram;
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertCode);
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragCodeUniformsFloatMatrices);
        QVERIFY(shaderProgram.link());

        // WHEN
        m_func->glUseProgram(shaderProgram.programId());
        GLfloat values[8] = { 383.0f, 427.0f, 454.0f, 350.0f, 883.0f, 355.0f, 1340.0f, 1584.0f};
        const GLint location = shaderProgram.uniformLocation("m42");
        m_glHelper.glUniformMatrix4x2fv(location, 1, values);

        // THEN
        GLfloat setValues[8] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        m_func->glGetUniformfv(shaderProgram.programId(), location, setValues);
        for (int i = 0; i < 8; ++i)
            QCOMPARE(values[i], setValues[i]);

        // Restore
        m_func->glUseProgram(0);
    }

    void glUniformMatrix3x4fv()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        QOpenGLShaderProgram shaderProgram;
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertCode);
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragCodeUniformsFloatMatrices);
        QVERIFY(shaderProgram.link());

        // WHEN
        m_func->glUseProgram(shaderProgram.programId());
        GLfloat values[12] = { 55.0f, 5.7f, 383.0f, 6.2f, 5.3f, 383.0f, 427.0f, 454.0f, 350.0f, 883.0f, 355.0f, 1340.0f};
        const GLint location = shaderProgram.uniformLocation("m34");
        m_glHelper.glUniformMatrix3x4fv(location, 1, values);

        // THEN
        GLfloat setValues[12] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        m_func->glGetUniformfv(shaderProgram.programId(), location, setValues);
        for (int i = 0; i < 12; ++i)
            QCOMPARE(values[i], setValues[i]);

        // Restore
        m_func->glUseProgram(0);
    }

    void glUniformMatrix4x3fv()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        QOpenGLShaderProgram shaderProgram;
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertCode);
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragCodeUniformsFloatMatrices);
        QVERIFY(shaderProgram.link());

        // WHEN
        m_func->glUseProgram(shaderProgram.programId());
        GLfloat values[12] = {  55.0f, 5.7f, 383.0f, 6.2f, 383.0f, 427.0f, 454.0f, 350.0f, 883.0f, 355.0f, 1340.0f, 1584.0f};
        const GLint location = shaderProgram.uniformLocation("m43");
        m_glHelper.glUniformMatrix4x3fv(location, 1, values);

        // THEN
        GLfloat setValues[12] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        m_func->glGetUniformfv(shaderProgram.programId(), location, setValues);
        for (int i = 0; i < 12; ++i)
            QCOMPARE(values[i], setValues[i]);

        // Restore
        m_func->glUseProgram(0);
    }


    void blitFramebuffer()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 3.3 Core functions not supported");

        // GIVEN
        GLuint fbos[2];
        GLuint fboTextures[2];

        m_func->glGenFramebuffers(2, fbos);
        m_func->glGenTextures(2, fboTextures);

        m_func->glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, fboTextures[0]);
        m_func->glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA8, 10, 10, true);
        m_func->glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);

        m_func->glBindTexture(GL_TEXTURE_2D, fboTextures[1]);
        m_func->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 10, 10, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        m_func->glBindTexture(GL_TEXTURE_2D, 0);

        m_func->glBindFramebuffer(GL_FRAMEBUFFER, fbos[1]);
        m_func->glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, fboTextures[1], 0);

        GLenum status = m_func->glCheckFramebufferStatus(GL_FRAMEBUFFER);
        QVERIFY(status == GL_FRAMEBUFFER_COMPLETE);

        m_func->glBindFramebuffer(GL_FRAMEBUFFER, fbos[0]);
        m_func->glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, fboTextures[0], 0);

        status = m_func->glCheckFramebufferStatus(GL_FRAMEBUFFER);
        QVERIFY(status == GL_FRAMEBUFFER_COMPLETE);

        m_func->glEnable(GL_MULTISAMPLE);
        m_func->glClearColor(0.2f, 0.2f, 0.2f, 0.2f);
        m_func->glClear(GL_COLOR_BUFFER_BIT);
        m_func->glDisable(GL_MULTISAMPLE);

        m_func->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbos[1]);

        // WHEN
        m_glHelper.blitFramebuffer(0,0,10,10,0,0,10,10, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        m_func->glBindFramebuffer(GL_READ_FRAMEBUFFER, fbos[1]);

        GLuint result[10*10];
        m_func->glReadPixels(0,0,10,10,GL_RGBA, GL_UNSIGNED_BYTE, result);

        // THEN
        GLuint v = (0.2f) * 255;
        v = v | (v<<8) | (v<<16) | (v<<24);
        for (GLuint value : result) {
            QCOMPARE(value, v);
        }
        m_func->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        m_func->glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

        m_func->glDeleteFramebuffers(2, fbos);
        m_func->glDeleteTextures(2, fboTextures);
    }

#define ADD_GL_TYPE_ENTRY(Type, Expected) \
    QTest::newRow(#Type) << Type << Expected;

    void uniformTypeFromGLType_data()
    {
        QTest::addColumn<int>("glType");
        QTest::addColumn<UniformType>("expected");

        ADD_GL_TYPE_ENTRY(GL_FLOAT, UniformType::Float);
        ADD_GL_TYPE_ENTRY(GL_FLOAT_VEC2, UniformType::Vec2);
        ADD_GL_TYPE_ENTRY(GL_FLOAT_VEC3, UniformType::Vec3);
        ADD_GL_TYPE_ENTRY(GL_FLOAT_VEC3, UniformType::Vec3);
        ADD_GL_TYPE_ENTRY(GL_FLOAT_VEC2, UniformType::Vec2);
        ADD_GL_TYPE_ENTRY(GL_FLOAT_VEC3, UniformType::Vec3);
        ADD_GL_TYPE_ENTRY(GL_FLOAT_VEC3, UniformType::Vec3);
        ADD_GL_TYPE_ENTRY(GL_INT, UniformType::Int);
        ADD_GL_TYPE_ENTRY(GL_INT_VEC2, UniformType::IVec2);
        ADD_GL_TYPE_ENTRY(GL_INT_VEC3, UniformType::IVec3);
        ADD_GL_TYPE_ENTRY(GL_INT_VEC4, UniformType::IVec4);
        ADD_GL_TYPE_ENTRY(GL_UNSIGNED_INT, UniformType::UInt);
        ADD_GL_TYPE_ENTRY(GL_UNSIGNED_INT_VEC2, UniformType::UIVec2);
        ADD_GL_TYPE_ENTRY(GL_UNSIGNED_INT_VEC3, UniformType::UIVec3);
        ADD_GL_TYPE_ENTRY(GL_UNSIGNED_INT_VEC4, UniformType::UIVec4);
        ADD_GL_TYPE_ENTRY(GL_BOOL, UniformType::Bool);
        ADD_GL_TYPE_ENTRY(GL_BOOL_VEC2, UniformType::BVec2);
        ADD_GL_TYPE_ENTRY(GL_BOOL_VEC3, UniformType::BVec3);
        ADD_GL_TYPE_ENTRY(GL_BOOL_VEC4, UniformType::BVec4);
        ADD_GL_TYPE_ENTRY(GL_FLOAT_MAT2, UniformType::Mat2);
        ADD_GL_TYPE_ENTRY(GL_FLOAT_MAT3, UniformType::Mat3);
        ADD_GL_TYPE_ENTRY(GL_FLOAT_MAT4, UniformType::Mat4);
        ADD_GL_TYPE_ENTRY(GL_FLOAT_MAT2x3, UniformType::Mat2x3);
        ADD_GL_TYPE_ENTRY(GL_FLOAT_MAT2x4, UniformType::Mat2x4);
        ADD_GL_TYPE_ENTRY(GL_FLOAT_MAT3x2, UniformType::Mat3x2);
        ADD_GL_TYPE_ENTRY(GL_FLOAT_MAT4x2, UniformType::Mat4x2);
        ADD_GL_TYPE_ENTRY(GL_FLOAT_MAT4x3, UniformType::Mat4x3);
        ADD_GL_TYPE_ENTRY(GL_FLOAT_MAT3x4, UniformType::Mat3x4);
        ADD_GL_TYPE_ENTRY(GL_SAMPLER_1D, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_SAMPLER_1D_ARRAY, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_SAMPLER_1D_SHADOW, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_SAMPLER_2D, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_SAMPLER_2D_ARRAY, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_SAMPLER_2D_RECT, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_SAMPLER_2D_MULTISAMPLE, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_SAMPLER_2D_MULTISAMPLE_ARRAY, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_SAMPLER_2D_SHADOW, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_SAMPLER_2D_ARRAY_SHADOW, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_SAMPLER_3D, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_SAMPLER_CUBE, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_SAMPLER_CUBE_SHADOW, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_SAMPLER_BUFFER, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_INT_SAMPLER_1D, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_INT_SAMPLER_2D, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_INT_SAMPLER_3D, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_INT_SAMPLER_BUFFER, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_INT_SAMPLER_2D_ARRAY, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_INT_SAMPLER_2D_MULTISAMPLE, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_INT_SAMPLER_CUBE, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_UNSIGNED_INT_SAMPLER_1D, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_UNSIGNED_INT_SAMPLER_2D, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_UNSIGNED_INT_SAMPLER_3D, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_UNSIGNED_INT_SAMPLER_BUFFER, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_UNSIGNED_INT_SAMPLER_2D_ARRAY, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_UNSIGNED_INT_SAMPLER_CUBE, UniformType::Sampler);
    }

    void uniformTypeFromGLType()
    {
        // GIVEN
        QFETCH(int, glType);
        QFETCH(UniformType, expected);

        // WHEN
        UniformType computed = m_glHelper.uniformTypeFromGLType(glType);

        // THEN
        QCOMPARE(computed, expected);
    }

private:
    QScopedPointer<QWindow> m_window;
    QOpenGLContext m_glContext;
    GraphicsHelperGL3_3 m_glHelper;
    QOpenGLFunctions_3_3_Core *m_func = nullptr;
    bool m_initializationSuccessful = false;
};

#endif

QT_BEGIN_NAMESPACE
QTEST_ADD_GPU_BLACKLIST_SUPPORT_DEFS
QT_END_NAMESPACE

int main(int argc, char *argv[])
{
#ifdef TEST_SHOULD_BE_PERFORMED
    QGuiApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);
    QTEST_ADD_GPU_BLACKLIST_SUPPORT
    tst_GraphicsHelperGL3_3 tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
#endif
    return 0;
}

#ifdef TEST_SHOULD_BE_PERFORMED
#include "tst_graphicshelpergl3_3.moc"
#endif
