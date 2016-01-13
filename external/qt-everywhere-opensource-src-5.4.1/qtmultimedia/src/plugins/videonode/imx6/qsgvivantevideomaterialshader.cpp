/****************************************************************************
**
** Copyright (C) 2014 Pelagicore AG
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qsgvivantevideomaterialshader.h"
#include "qsgvivantevideonode.h"
#include "qsgvivantevideomaterial.h"

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
            ""
            "varying highp vec2 qt_TexCoord;"
            ""
            "void main()"
            "{"
            "  gl_FragColor = vec4(texture2D( texture, qt_TexCoord ).rgb, 1.0) * opacity;\n"
            "}";
    return shader;
}


void QSGVivanteVideoMaterialShader::initialize() {
    mIdMatrix = program()->uniformLocation("qt_Matrix");
    mIdTexture = program()->uniformLocation("texture");
    mIdOpacity = program()->uniformLocation("opacity");
}
