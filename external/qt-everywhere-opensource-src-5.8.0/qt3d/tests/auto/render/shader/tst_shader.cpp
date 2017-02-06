/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
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
#include <qbackendnodetester.h>
#include <Qt3DRender/private/shader_p.h>
#include <Qt3DRender/qshaderprogram.h>

class tst_RenderShader : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT
private slots:

    void hasCoherentInitialState();
    void matchesFrontendPeer();
    void cleanupLeavesACoherentState();
};


Qt3DRender::QShaderProgram *createFrontendShader()
{
    Qt3DRender::QShaderProgram *shader = new Qt3DRender::QShaderProgram();

    shader->setVertexShaderCode(QByteArrayLiteral(
                                    "#version 150"\
                                    "in vec3 vertexPosition;"\
                                    "in vec3 vertexColor; "\
                                    "out vec3 color;"\
                                    "void main()"\
                                    "{"\
                                    "    color = vertexColor;"\
                                    "    gl_Position = vec4( vertexPosition, 1.0 );"\
                                    "}"));

    shader->setFragmentShaderCode(QByteArrayLiteral(
                                      "#version 150"\
                                      "in vec3 color;"\
                                      "out vec4 fragColor;"\
                                      "void main()"\
                                      "{"\
                                      "   fragColor = vec4( color, 1.0 );"\
                                      "}"));

    return shader;
}

void tst_RenderShader::hasCoherentInitialState()
{
    Qt3DRender::Render::Shader *shader = new Qt3DRender::Render::Shader();

    QCOMPARE(shader->isLoaded(), false);
    QCOMPARE(shader->dna(), 0U);
    QVERIFY(shader->uniformsNames().isEmpty());
    QVERIFY(shader->attributesNames().isEmpty());
    QVERIFY(shader->uniformBlockNames().isEmpty());
    QVERIFY(shader->uniforms().isEmpty());
    QVERIFY(shader->attributes().isEmpty());
    QVERIFY(shader->uniformBlocks().isEmpty());
}

void tst_RenderShader::matchesFrontendPeer()
{
    Qt3DRender::QShaderProgram *frontend = createFrontendShader();
    Qt3DRender::Render::Shader *backend = new Qt3DRender::Render::Shader();

    simulateInitialization(frontend, backend);
    QCOMPARE(backend->isLoaded(), false);
    QVERIFY(backend->dna() != 0U);

    for (int i = Qt3DRender::QShaderProgram::Vertex; i <= Qt3DRender::QShaderProgram::Compute; ++i)
        QCOMPARE(backend->shaderCode()[i],
                 frontend->shaderCode( static_cast<const Qt3DRender::QShaderProgram::ShaderType>(i)));
}

void tst_RenderShader::cleanupLeavesACoherentState()
{
    Qt3DRender::QShaderProgram *frontend = createFrontendShader();
    Qt3DRender::Render::Shader *shader = new Qt3DRender::Render::Shader();

    simulateInitialization(frontend, shader);

    shader->cleanup();

    QCOMPARE(shader->isLoaded(), false);
    QCOMPARE(shader->dna(), 0U);
    QVERIFY(shader->uniformsNames().isEmpty());
    QVERIFY(shader->attributesNames().isEmpty());
    QVERIFY(shader->uniformBlockNames().isEmpty());
    QVERIFY(shader->uniforms().isEmpty());
    QVERIFY(shader->attributes().isEmpty());
    QVERIFY(shader->uniformBlocks().isEmpty());
}

QTEST_APPLESS_MAIN(tst_RenderShader)

#include "tst_shader.moc"
