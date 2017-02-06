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
#include <Qt3DRender/private/graphicshelpergl2_p.h>
#include <Qt3DRender/private/attachmentpack_p.h>
#include <QtOpenGLExtensions/QOpenGLExtensions>
#include <QOpenGLContext>
#include <QOpenGLFunctions_2_0>
#include <QOpenGLShaderProgram>
#include <QSurfaceFormat>

#ifndef QT_OPENGL_ES_2

#define TEST_SHOULD_BE_PERFORMED 1

QT_BEGIN_NAMESPACE

using namespace Qt3DRender;
using namespace Qt3DRender::Render;

namespace {

const QByteArray vertCode = QByteArrayLiteral(
            "#version 120\n" \
            "attribute vec3 vertexPosition;\n" \
            "attribute vec2 vertexTexCoord;\n" \
            "varying vec2 texCoord;\n" \
            "void main()\n" \
            "{\n" \
            "   texCoord = vertexTexCoord;\n" \
            "   gl_Position = vec4(vertexPosition, 1.0);\n" \
            "}\n");

const QByteArray fragCodeUniformsFloat = QByteArrayLiteral(
            "#version 120\n" \
            "uniform float multiplier;\n" \
            "uniform vec2 multiplierVec2;\n" \
            "uniform vec3 multiplierVec3;\n" \
            "uniform vec4 multiplierVec4;\n" \
            "void main()\n" \
            "{\n" \
            "   vec4 randomMult = multiplierVec4 + vec4(multiplierVec3, 0.0) + vec4(multiplierVec2, 0.0, 0.0);\n" \
            "   gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0) * randomMult * multiplier;\n" \
            "}\n");

const QByteArray fragCodeUniformsInt = QByteArrayLiteral(
            "#version 120\n" \
            "uniform int multiplier;\n" \
            "uniform ivec2 multiplierVec2;\n" \
            "uniform ivec3 multiplierVec3;\n" \
            "uniform ivec4 multiplierVec4;\n" \
            "void main()\n" \
            "{\n" \
            "   ivec4 randomMult = multiplierVec4 + ivec4(multiplierVec3, 0) + ivec4(multiplierVec2, 0, 0);\n" \
            "   gl_FragColor = ivec4(1, 0, 0, 1) * randomMult * multiplier;\n" \
            "}\n");

const QByteArray fragCodeUniformsFloatMatrices = QByteArrayLiteral(
            "#version 120\n" \
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
            "   gl_FragColor = vec4(1, 0, 0, 1) * lengthSum;\n" \
            "}\n");


const QByteArray fragCodeSamplers = QByteArrayLiteral(
            "#version 120\n" \
            "varying vec2 texCoord;\n" \
            "uniform sampler1D s1;\n" \
            "uniform sampler2D s2;\n" \
            "uniform sampler3D s3;\n" \
            "uniform samplerCube scube;\n" \
            "void main()\n" \
            "{\n" \
            "   gl_FragColor = vec4(1, 0, 0, 1) *" \
            " texture1D(s1, texCoord.x) *" \
            " texture2D(s2, texCoord) *" \
            " texture3D(s3, vec3(texCoord, 0.0)) *" \
            " textureCube(scube, vec3(texCoord, 0));\n" \
            "}\n");

} // anonymous

class tst_GraphicsHelperGL2 : public QObject
{
    Q_OBJECT
private Q_SLOTS:

    void init()
    {
        m_window.reset(new QWindow);
        m_window->setSurfaceType(QWindow::OpenGLSurface);
        m_window->setGeometry(0, 0, 10, 10);
        m_window->create();

        QSurfaceFormat format;
        format.setVersion(2, 0);
        format.setProfile(QSurfaceFormat::NoProfile);
        format.setDepthBufferSize(24);
        format.setSamples(4);
        format.setStencilBufferSize(8);
        m_window->setFormat(format);
        m_glContext.setFormat(format);

        if (!m_glContext.create()) {
            qWarning() << "Failed to create OpenGL context";
            return;
        }

        if (!m_glContext.makeCurrent(m_window.data())) {
            qWarning() << "Failed to make OpenGL context current";
            return;
        }

        if ((m_func = m_glContext.versionFunctions<QOpenGLFunctions_2_0>()) != nullptr) {
            if (m_glContext.hasExtension(QByteArrayLiteral("GL_ARB_framebuffer_object"))) {
                m_fboFuncs = new QOpenGLExtension_ARB_framebuffer_object();
                m_fboFuncs->initializeOpenGLFunctions();
            }
            m_glHelper.initializeHelper(&m_glContext, m_func);
            m_initializationSuccessful = true;
        }
    }

    void alphaTest()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");
        // Deprecated
    }

    void bindBufferBase()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");

        // Not supported by GL2
    }

    void bindFragDataLocation()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");
        // Not supported by GL2
    }

    void bindFrameBufferAttachment()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");

        if (!m_fboFuncs)
            QSKIP("FBO not supported by OpenGL 2.0");

        // GIVEN
        GLuint fboId;
        m_fboFuncs->glGenFramebuffers(1, &fboId);

        Attachment attachment;
        attachment.m_point = QRenderTargetOutput::Color0;

        // THEN
        QVERIFY(fboId != 0);

        // WHEN
        m_fboFuncs->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboId);

        QOpenGLTexture texture(QOpenGLTexture::Target2D);
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
        m_glHelper.bindFrameBufferAttachment(&texture, attachment);

        // THEN
        GLenum status = m_fboFuncs->glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
        QVERIFY(status == GL_FRAMEBUFFER_COMPLETE);

        error = m_func->glGetError();
        QVERIFY(error == 0);
        GLint textureAttachmentId = 0;
        m_fboFuncs->glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,
                                                      GL_COLOR_ATTACHMENT0,
                                                      GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                      &textureAttachmentId);
        QCOMPARE(GLuint(textureAttachmentId), texture.textureId());

        // Restore state
        m_fboFuncs->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        m_fboFuncs->glDeleteFramebuffers(1, &fboId);
    }

    void bindFrameBufferObject()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");
        if (!m_fboFuncs)
            QSKIP("FBO not supported by OpenGL 2.0");

        // GIVEN
        GLuint fboId;
        m_fboFuncs->glGenFramebuffers(1, &fboId);

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
        m_fboFuncs->glDeleteFramebuffers(1, &fboId);
    }

    void bindShaderStorageBlock()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");
        // Not supported by GL2
    }

    void bindUniformBlock()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");
        // Not supported by GL2
    }

    void blendEquation()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");

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
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");
        // Not supported by GL2
    }

    void blendFuncSeparatei()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");
        // Not supported by GL2
    }

    void boundFrameBufferObject()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");
        if (!m_fboFuncs)
            QSKIP("FBO not supported by OpenGL 2.0");

        // GIVEN
        GLuint fboId;
        m_fboFuncs->glGenFramebuffers(1, &fboId);

        // WHEN
        m_fboFuncs->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboId);

        // THEN
        GLint boundBuffer = 0;
        m_func->glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &boundBuffer);
        QCOMPARE(GLuint(boundBuffer), fboId);

        // THEN
        QCOMPARE(m_glHelper.boundFrameBufferObject(), fboId);

        // Reset state
        m_fboFuncs->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        m_fboFuncs->glDeleteFramebuffers(1, &fboId);
    }

    void checkFrameBufferComplete()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");
        if (!m_fboFuncs)
            QSKIP("FBO not supported by OpenGL 2.0");

        // GIVEN
        GLuint fboId;
        m_fboFuncs->glGenFramebuffers(1, &fboId);

        Attachment attachment;
        attachment.m_point = QRenderTargetOutput::Color0;

        m_fboFuncs->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboId);

        QOpenGLTexture texture(QOpenGLTexture::Target2D);
        texture.setSize(512, 512);
        texture.setFormat(QOpenGLTexture::RGBA8U);
        texture.setMinificationFilter(QOpenGLTexture::Linear);
        texture.setMagnificationFilter(QOpenGLTexture::Linear);
        texture.create();
        texture.allocateStorage();
        m_glHelper.bindFrameBufferAttachment(&texture, attachment);

        // THEN
        GLenum status = m_fboFuncs->glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
        QVERIFY(status == GL_FRAMEBUFFER_COMPLETE);

        QVERIFY(m_glHelper.checkFrameBufferComplete());

        // Restore
        m_fboFuncs->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        m_fboFuncs->glDeleteFramebuffers(1, &fboId);
    }

    void clearBufferf()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");
        // Not supported by GL2
    }

    void createFrameBufferObject()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");
        if (!m_fboFuncs)
            QSKIP("FBO not supported by OpenGL 2.0");

        // WHEN
        const GLuint fboId = m_glHelper.createFrameBufferObject();

        // THEN
        QVERIFY(fboId != 0);

        // Restore
        m_fboFuncs->glDeleteFramebuffers(1, &fboId);
    }

    void depthMask()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");

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
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");

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
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");

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
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");
        // Not supported by GL2
    }

    void disablePrimitiveRestart()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");
        // Not supported by GL2
    }

    void drawBuffers()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");

        if (!m_fboFuncs)
            QSKIP("FBO not supported by OpenGL 2.0");

        // GIVEN
        GLuint fboId;
        m_fboFuncs->glGenFramebuffers(1, &fboId);

        // THEN
        QVERIFY(fboId != 0);

        // WHEN
        m_fboFuncs->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboId);
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
        GLenum status = m_fboFuncs->glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
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
        m_fboFuncs->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        m_fboFuncs->glDeleteFramebuffers(1, &fboId);
        for (int i = 0; i < 4; ++i)
            delete textures[i];
    }

    void enableClipPlane()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");

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
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");
        // Not supported by GL2
    }

    void enablePrimitiveRestart()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");
        // Not supported by GL2
    }

    void frontFace()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");

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
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");
        // Not supported by GL2
    }

    void getTextureDimensions()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");

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
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");

        // WHEN
        m_glHelper.pointSize(false, 0.5f);
        // THEN
        GLfloat size = 0.0f;
        m_func->glGetFloatv(GL_POINT_SIZE, &size);
        QCOMPARE(size, 0.5f);
    }

    void maxClipPlaneCount()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");

        // GIVEN
        GLint maxCount = -1;
        m_func->glGetIntegerv(GL_MAX_CLIP_PLANES, &maxCount);

        // THEN
        QCOMPARE(maxCount, m_glHelper.maxClipPlaneCount());
    }

    void programUniformBlock()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");

        // Not supported by GL2
    }

    void programAttributesAndLocations()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");

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
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");

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
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");
        // Not supported by GL2
    }

    void releaseFrameBufferObject()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");
        if (!m_fboFuncs)
            QSKIP("FBO not supported by OpenGL 2.0");
                // GIVEN
        GLuint fboId;
        m_fboFuncs->glGenFramebuffers(1, &fboId);

        // THEN
        QVERIFY(fboId != 0);

        // WHEN
        m_glHelper.releaseFrameBufferObject(fboId);

        // THEN
        QVERIFY(!m_fboFuncs->glIsFramebuffer(fboId));
    }

    void setMSAAEnabled()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");

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
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");

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
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");

        // Deprecated in 3.3 core
    }

    void setSeamlessCubemap()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");
        // Not supported in GL2
    }

    void setVerticesPerPatch()
    {
        // Not supported in GL2
    }

#define SUPPORTS_FEATURE(Feature, IsSupported) \
    QVERIFY(m_glHelper.supportsFeature(Feature) == IsSupported);

    void supportsFeature()
    {
        SUPPORTS_FEATURE(GraphicsHelperInterface::MRT, (m_fboFuncs != nullptr));
        SUPPORTS_FEATURE(GraphicsHelperInterface::UniformBufferObject, false);
        SUPPORTS_FEATURE(GraphicsHelperInterface::BindableFragmentOutputs, false);
        SUPPORTS_FEATURE(GraphicsHelperInterface::PrimitiveRestart, false);
        SUPPORTS_FEATURE(GraphicsHelperInterface::RenderBufferDimensionRetrieval, false);
        SUPPORTS_FEATURE(GraphicsHelperInterface::TextureDimensionRetrieval, true);
        SUPPORTS_FEATURE(GraphicsHelperInterface::UniformBufferObject, false);
        SUPPORTS_FEATURE(GraphicsHelperInterface::ShaderStorageObject, false);
        SUPPORTS_FEATURE(GraphicsHelperInterface::Compute, false);
        SUPPORTS_FEATURE(GraphicsHelperInterface::DrawBuffersBlend, false);
        SUPPORTS_FEATURE(GraphicsHelperInterface::Tessellation, false);
        SUPPORTS_FEATURE(GraphicsHelperInterface::BlitFramebuffer, false);
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

        ADD_UNIFORM_ENTRY(fragCodeUniformsFloatMatrices, "m2",  GL_FLOAT_MAT2, 1, 4 * 2 * 2);
        ADD_UNIFORM_ENTRY(fragCodeUniformsFloatMatrices, "m3", GL_FLOAT_MAT3, 1, 4 * 3 * 3);
        ADD_UNIFORM_ENTRY(fragCodeUniformsFloatMatrices, "m4", GL_FLOAT_MAT4, 1, 4 * 4 * 4);

        ADD_UNIFORM_ENTRY(fragCodeSamplers, "s1", GL_SAMPLER_1D, 1, 4);
        ADD_UNIFORM_ENTRY(fragCodeSamplers, "s2", GL_SAMPLER_2D, 1, 4);
        ADD_UNIFORM_ENTRY(fragCodeSamplers, "s3", GL_SAMPLER_3D, 1, 4);
        ADD_UNIFORM_ENTRY(fragCodeSamplers, "scube", GL_SAMPLER_CUBE, 1, 4);
    }

    void uniformsByteSize()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");

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
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");

        // GIVEN
        QOpenGLShaderProgram shaderProgram;
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertCode);
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragCodeUniformsFloat);

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
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");
        // Not available in 3.2
    }

    void glUniform1fv()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");

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
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");

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
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");

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
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");

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
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");

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
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");

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
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");

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
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");

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
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");
        // Not supported by GL2
    }

    void glUniform2uiv()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");
        // Not supported by GL2
    }

    void glUniform3uiv()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");
        // Not supported by GL2
    }

    void glUniform4uiv()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");
        // Not supported by GL2
    }

    void glUniformMatrix2fv()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");

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
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");

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
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");

        // GIVEN
        QOpenGLShaderProgram shaderProgram;
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertCode);
        shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragCodeUniformsFloatMatrices);
        QVERIFY(shaderProgram.link());

        // WHEN
        m_func->glUseProgram(shaderProgram.programId());
        GLfloat values[16] = { 454.0f, 350.0f, 883.0f, 355.0f, 1340.0f, 1584.0f, 1200.0f, 427.0f, 396.0f, 1603.0f, 55.0f, 5.7, 383.0f, 6.2f, 5.3f, 327.0f };
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
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");
        // Not supported by GL2
    }

    void glUniformMatrix3x2fv()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");
        // Not supported by GL2
    }

    void glUniformMatrix2x4fv()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");
        // Not supported by GL2
    }

    void glUniformMatrix4x2fv()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");
        // Not supported by GL2
    }

    void glUniformMatrix3x4fv()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");
        // Not supported by GL2
    }

    void glUniformMatrix4x3fv()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");
        // Not supported by GL2
    }

    void blitFramebuffer()
    {
        if (!m_initializationSuccessful)
            QSKIP("Initialization failed, OpenGL 2.0 functions not supported");
        // Not supported by GL2
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
        ADD_GL_TYPE_ENTRY(GL_BOOL, UniformType::Bool);
        ADD_GL_TYPE_ENTRY(GL_BOOL_VEC2, UniformType::BVec2);
        ADD_GL_TYPE_ENTRY(GL_BOOL_VEC3, UniformType::BVec3);
        ADD_GL_TYPE_ENTRY(GL_BOOL_VEC4, UniformType::BVec4);
        ADD_GL_TYPE_ENTRY(GL_FLOAT_MAT2, UniformType::Mat2);
        ADD_GL_TYPE_ENTRY(GL_FLOAT_MAT3, UniformType::Mat3);
        ADD_GL_TYPE_ENTRY(GL_FLOAT_MAT4, UniformType::Mat4);
        ADD_GL_TYPE_ENTRY(GL_SAMPLER_1D, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_SAMPLER_1D_SHADOW, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_SAMPLER_2D, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_SAMPLER_2D_SHADOW, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_SAMPLER_3D, UniformType::Sampler);
        ADD_GL_TYPE_ENTRY(GL_SAMPLER_CUBE, UniformType::Sampler);
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
    GraphicsHelperGL2 m_glHelper;
    QOpenGLFunctions_2_0 *m_func = nullptr;
    QOpenGLExtension_ARB_framebuffer_object *m_fboFuncs = nullptr;
    bool m_initializationSuccessful = false;
};

QT_END_NAMESPACE

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
    tst_GraphicsHelperGL2 tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
#endif
    return 0;
}

#ifdef TEST_SHOULD_BE_PERFORMED
#include "tst_graphicshelpergl2.moc"
#endif
