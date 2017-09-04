/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qsgvivantevideomaterialshader.h"
#include "qsgvivantevideonode.h"
#include "qsgvivantevideomaterial.h"

QSGVivanteVideoMaterialShader::QSGVivanteVideoMaterialShader() :
    mUScale(1),
    mVScale(1),
    mNewUVScale(true)
{
}

void QSGVivanteVideoMaterialShader::updateState(const RenderState &state,
                                                QSGMaterial *newMaterial,
                                                QSGMaterial *oldMaterial)
{
    Q_UNUSED(oldMaterial);

    QSGVivanteVideoMaterial *mat = static_cast<QSGVivanteVideoMaterial *>(newMaterial);
    program()->setUniformValue(mIdTexture, 0);
    mat->bind();
    if (state.isOpacityDirty()) {
        mat->setOpacity(state.opacity());
        program()->setUniformValue(mIdOpacity, state.opacity());
    }
    if (mNewUVScale) {
        program()->setUniformValue(mIdUVScale, mUScale, mVScale);
        mNewUVScale = false;
    }
    if (state.isMatrixDirty())
        program()->setUniformValue(mIdMatrix, state.combinedMatrix());
}

const char * const *QSGVivanteVideoMaterialShader::attributeNames() const {
    static const char *names[] = {
        "qt_VertexPosition",
        "qt_VertexTexCoord",
        0
    };
    return names;
}

void QSGVivanteVideoMaterialShader::setUVScale(float uScale, float vScale)
{
    mUScale = uScale;
    mVScale = vScale;
    mNewUVScale = true;
}

const char *QSGVivanteVideoMaterialShader::vertexShader() const {
    static const char *shader =
            "uniform highp mat4 qt_Matrix;                      \n"
            "attribute highp vec4 qt_VertexPosition;            \n"
            "attribute highp vec2 qt_VertexTexCoord;            \n"
            "varying highp vec2 qt_TexCoord;                    \n"
            "void main() {                                      \n"
            "    qt_TexCoord = qt_VertexTexCoord;               \n"
            "    gl_Position = qt_Matrix * qt_VertexPosition;   \n"
            "}";
    return shader;
}

const char *QSGVivanteVideoMaterialShader::fragmentShader() const {
    static const char *shader =
            "uniform sampler2D texture;"
            "uniform lowp float opacity;"
            "uniform highp vec2 uvScale;"
            ""
            "varying highp vec2 qt_TexCoord;"
            ""
            "void main()"
            "{"
            "  gl_FragColor = vec4(texture2D( texture, qt_TexCoord * uvScale ).rgb, 1.0) * opacity;\n"
            "}";
    return shader;
}


void QSGVivanteVideoMaterialShader::initialize() {
    mIdMatrix = program()->uniformLocation("qt_Matrix");
    mIdTexture = program()->uniformLocation("texture");
    mIdOpacity = program()->uniformLocation("opacity");
    mIdUVScale = program()->uniformLocation("uvScale");
}
