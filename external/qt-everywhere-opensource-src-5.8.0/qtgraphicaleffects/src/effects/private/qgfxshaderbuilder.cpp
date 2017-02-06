/****************************************************************************
**
** Copyright (C) 2016 Jolla Ltd, author: <gunnar.sletta@jollamobile.com>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Graphical Effects module of the Qt Toolkit.
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

#include "qgfxshaderbuilder_p.h"

#include <QtCore/QDebug>
#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>

#include <qmath.h>
#include <qnumeric.h>

#ifndef GL_MAX_VARYING_COMPONENTS
#define GL_MAX_VARYING_COMPONENTS 0x8B4B
#endif

#ifndef GL_MAX_VARYING_FLOATS
#define GL_MAX_VARYING_FLOATS 0x8B4B
#endif

#ifndef GL_MAX_VARYING_VECTORS
#define GL_MAX_VARYING_VECTORS 0x8DFC
#endif

QGfxShaderBuilder::QGfxShaderBuilder()
{
    // The following code makes the assumption that an OpenGL context the GUI
    // thread will get the same capabilities as the render thread's OpenGL
    // context. Not 100% accurate, but it works...
    QOpenGLContext context;
    context.create();
    QOffscreenSurface surface;
    // In very odd cases, we can get incompatible configs here unless we pass the
    // GL context's format on to the offscreen format.
    surface.setFormat(context.format());
    surface.create();

    QOpenGLContext *oldContext = QOpenGLContext::currentContext();
    QSurface *oldSurface = oldContext ? oldContext->surface() : 0;
    if (context.makeCurrent(&surface)) {
        QOpenGLFunctions *gl = context.functions();
        if (context.isOpenGLES()) {
            gl->glGetIntegerv(GL_MAX_VARYING_VECTORS, &m_maxBlurSamples);
        } else if (context.format().majorVersion() >= 3) {
            int components;
            gl->glGetIntegerv(GL_MAX_VARYING_COMPONENTS, &components);
            m_maxBlurSamples = components / 2.0;
        } else {
            int floats;
            gl->glGetIntegerv(GL_MAX_VARYING_FLOATS, &floats);
            m_maxBlurSamples = floats / 2.0;
        }
        if (oldContext && oldSurface)
            oldContext->makeCurrent(oldSurface);
        else
            context.doneCurrent();
    } else {
        qDebug() << "failed to acquire GL context to resolve capabilities, using defaults..";
        m_maxBlurSamples = 8; // minimum number of varyings in the ES 2.0 spec.
    }
}

/*

    The algorithm works like this..

    For every two pixels we want to sample we take one sample between those
    two pixels and rely on linear interpoliation to get both values at the
    cost of one texture sample. The sample point is calculated based on the
    gaussian weights at the two texels.

    I've included the table here for future reference:

    Requested     Effective       Actual    Actual
    Samples       Radius/Kernel   Samples   Radius(*)
    -------------------------------------------------
    0             0 / 1x1         1         0
    1             0 / 1x1         1         0
    2             1 / 3x3         2         0
    3             1 / 3x3         2         0
    4             2 / 5x5         3         1
    5             2 / 5x5         3         1
    6             3 / 7x7         4         1
    7             3 / 7x7         4         1
    8             4 / 9x9         5         2
    9             4 / 9x9         5         2
    10            5 / 11x11       6         2
    11            5 / 11x11       6         2
    12            6 / 13x13       7         3
    13            6 / 13x13       7         3
    ...           ...             ...       ...

    When ActualSamples is an 'odd' nunber, sample center pixel separately:
    EffectiveRadius: 4
    EffectiveKernel: 9x9
    ActualSamples: 5
     -4  -3  -2  -1   0  +1  +2  +3  +4
    |   |   |   |   |   |   |   |   |   |
      \   /   \   /   |   \   /   \   /
       tL2     tL1    tC   tR1     tR2

    When ActualSamples is an 'even' number, sample 3 center pixels with two
    samples:
    EffectiveRadius: 3
    EffectiveKernel: 7x7
    ActualSamples: 4
     -3  -2  -1   0  +1  +2  +3
    |   |   |   |   |   |   |   |
      \   /   \   /   |    \   /
       tL1     tL0   tR0    tR2

    From this table we have the following formulas:
    EffectiveRadius = RequestedSamples / 2;
    EffectiveKernel = EffectiveRadius * 2 + 1
    ActualSamples   = 1 + RequstedSamples / 2;
    ActualRadius    = RequestedSamples / 4;

    (*) ActualRadius excludes the pixel pair sampled in the center
        for even 'actual sample' counts
*/

static qreal qgfx_gaussian(qreal x, qreal d)
{
    return qExp(- x * x / (2 * d * d));
}

struct QGfxGaussSample
{
    QByteArray name;
    qreal pos;
    qreal weight;
    inline void set(const QByteArray &n, qreal p, qreal w) {
        name = n;
        pos = p;
        weight = w;
    }
};

static void qgfx_declareBlurVaryings(QByteArray &shader, QGfxGaussSample *s, int samples)
{
    for (int i=0; i<samples; ++i) {
        shader += "varying highp vec2 ";
        shader += s[i].name;
        shader += ";\n";
    }
}

static void qgfx_buildGaussSamplePoints(QGfxGaussSample *p, int samples, int radius, qreal deviation)
{

    if ((samples % 2) == 1) {
        p[radius].set("tC", 0, 1);
        for (int i=0; i<radius; ++i) {
            qreal p0 = (i + 1) * 2 - 1;
            qreal p1 = (i + 1) * 2;
            qreal w0 = qgfx_gaussian(p0, deviation);
            qreal w1 = qgfx_gaussian(p1, deviation);
            qreal w = w0 + w1;
            qreal samplePos = (p0 * w0 + p1 * w1) / w;
            if (qIsNaN(samplePos)) {
                samplePos = 0;
                w = 0;
            }
            p[radius - i - 1].set("tL" + QByteArray::number(i), samplePos, w);
            p[radius + i + 1].set("tR" + QByteArray::number(i), -samplePos, w);
        }
    } else {
        { // tL0
            qreal wl = qgfx_gaussian(-1.0, deviation);
            qreal wc = qgfx_gaussian(0.0, deviation);
            qreal w = wl + wc;
            p[radius].set("tL0", -1.0 * wl / w, w);
            p[radius+1].set("tR0", 1.0, wl);  // reuse wl as gauss(-1)==gauss(1);
        }
        for (int i=0; i<radius; ++i) {
            qreal p0 = (i + 1) * 2;
            qreal p1 = (i + 1) * 2 + 1;
            qreal w0 = qgfx_gaussian(p0, deviation);
            qreal w1 = qgfx_gaussian(p1, deviation);
            qreal w = w0 + w1;
            qreal samplePos = (p0 * w0 + p1 * w1) / w;
            if (qIsNaN(samplePos)) {
                samplePos = 0;
                w = 0;
            }
            p[radius - i - 1].set("tL" + QByteArray::number(i+1), samplePos, w);
            p[radius + i + 2].set("tR" + QByteArray::number(i+1), -samplePos, w);

        }
    }
}

QByteArray qgfx_gaussianVertexShader(QGfxGaussSample *p, int samples)
{
    QByteArray shader;
    shader.reserve(1024);
    shader += "attribute highp vec4 qt_Vertex;\n"
              "attribute highp vec2 qt_MultiTexCoord0;\n\n"
              "uniform highp mat4 qt_Matrix;\n"
              "uniform highp float spread;\n"
              "uniform highp vec2 dirstep;\n\n";

    qgfx_declareBlurVaryings(shader, p, samples);

    shader += "\nvoid main() {\n"
              "    gl_Position = qt_Matrix * qt_Vertex;\n\n";

    for (int i=0; i<samples; ++i) {
        shader += "    ";
        shader += p[i].name;
        shader += " = qt_MultiTexCoord0";
        if (p[i].pos != 0.0) {
            shader += " + spread * dirstep * float(";
            shader += QByteArray::number(p[i].pos);
            shader += ')';
        }
        shader += ";\n";
    }

    shader += "}\n";

    return shader;
}


QByteArray qgfx_gaussianFragmentShader(QGfxGaussSample *p, int samples, bool alphaOnly)
{
    QByteArray shader;
    shader.reserve(1024);
    shader += "uniform lowp sampler2D source;\n"
              "uniform lowp float qt_Opacity;\n";

    if (alphaOnly) {
        shader += "uniform lowp vec4 color;\n"
                  "uniform lowp float thickness;\n";
    }

    shader += "\n";



    qgfx_declareBlurVaryings(shader, p, samples);

    shader += "\nvoid main() {\n"
              "    gl_FragColor = ";
    if (alphaOnly)
        shader += "mix(vec4(0), color, clamp((";
    else
        shader += "(";

    qreal sum = 0;
    for (int i=0; i<samples; ++i)
        sum += p[i].weight;

    for (int i=0; i<samples; ++i) {
        shader += "\n                    + float(";
        shader += QByteArray::number(p[i].weight / sum);
        shader += ") * texture2D(source, ";
        shader += p[i].name;
        shader += ")";
        if (alphaOnly)
            shader += ".a";
    }

    shader += "\n                   )";
    if (alphaOnly)
        shader += "/thickness, 0.0, 1.0))";
    shader += "* qt_Opacity;\n}";

    return shader;
}


QVariantMap QGfxShaderBuilder::gaussianBlur(const QJSValue &parameters)
{
    int requestedRadius = qMax(0.0, parameters.property(QStringLiteral("radius")).toNumber());
    qreal deviation = parameters.property(QStringLiteral("deviation")).toNumber();
    bool masked = parameters.property(QStringLiteral("masked")).toBool();
    bool alphaOnly = parameters.property(QStringLiteral("alphaOnly")).toBool();

    int requestedSamples = requestedRadius * 2 + 1;
    int samples = 1 + requestedSamples / 2;
    int radius = requestedSamples / 4;
    bool fallback = parameters.property(QStringLiteral("fallback")).toBool();

    QVariantMap result;

    if (samples > m_maxBlurSamples || masked || fallback) {
        QByteArray fragShader;
        if (masked)
            fragShader += "uniform mediump sampler2D mask;\n";
        fragShader +=
            "uniform highp sampler2D source;\n"
            "uniform lowp float qt_Opacity;\n"
            "uniform mediump float spread;\n"
            "uniform highp vec2 dirstep;\n";
        if (alphaOnly) {
            fragShader += "uniform lowp vec4 color;\n"
                          "uniform lowp float thickness;\n";
        }
        fragShader +=
            "\n"
            "varying highp vec2 qt_TexCoord0;\n"
            "\n"
            "void main() {\n";
        if (alphaOnly)
            fragShader += "    mediump float result = 0.0;\n";
        else
            fragShader += "    mediump vec4 result = vec4(0);\n";
        fragShader += "    highp vec2 pixelStep = dirstep * spread;\n";
        if (masked)
            fragShader += "    pixelStep *= texture2D(mask, qt_TexCoord0).a;\n";

        float wSum = 0;
        for (int r=-requestedRadius; r<=requestedRadius; ++r) {
            float w = qgfx_gaussian(r, deviation);
            wSum += w;
            fragShader += "    result += float(";
            fragShader += QByteArray::number(w);
            fragShader += ") * texture2D(source, qt_TexCoord0 + pixelStep * float(";
            fragShader += QByteArray::number(r);
            fragShader += "))";
            if (alphaOnly)
                fragShader += ".a";
            fragShader += ";\n";
        }
        fragShader += "    const mediump float wSum = float(";
        fragShader += QByteArray::number(wSum);
        fragShader += ");\n"
            "    gl_FragColor = ";
        if (alphaOnly)
            fragShader += "mix(vec4(0), color, clamp((result / wSum) / thickness, 0.0, 1.0)) * qt_Opacity;\n";
        else
            fragShader += "(qt_Opacity / wSum) * result;\n";
        fragShader += "}\n";
        result[QStringLiteral("fragmentShader")] = fragShader;

        result[QStringLiteral("vertexShader")] =
            "attribute highp vec4 qt_Vertex;\n"
            "attribute highp vec2 qt_MultiTexCoord0;\n"
            "uniform highp mat4 qt_Matrix;\n"
            "varying highp vec2 qt_TexCoord0;\n"
            "void main() {\n"
            "    gl_Position = qt_Matrix * qt_Vertex;\n"
            "    qt_TexCoord0 = qt_MultiTexCoord0;\n"
            "}\n";
        return result;
    }

    QVarLengthArray<QGfxGaussSample, 64> p(samples);
    qgfx_buildGaussSamplePoints(p.data(), samples, radius, deviation);

    result[QStringLiteral("fragmentShader")] = qgfx_gaussianFragmentShader(p.data(), samples, alphaOnly);
    result[QStringLiteral("vertexShader")] = qgfx_gaussianVertexShader(p.data(), samples);

    return result;
}

