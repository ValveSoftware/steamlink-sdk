/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qsgadaptationlayer_p.h"

#include <qmath.h>
#include <QtQuick/private/qsgdistancefieldutil_p.h>
#include <QtQuick/private/qsgdistancefieldglyphnode_p.h>
#include <QtQuick/private/qsgcontext_p.h>
#include <private/qrawfont_p.h>
#include <QtGui/qguiapplication.h>
#include <qdir.h>
#include <qsgrendernode.h>

#include <private/qquickprofiler_p.h>
#include <QElapsedTimer>

QT_BEGIN_NAMESPACE

static QElapsedTimer qsg_render_timer;

QSGDistanceFieldGlyphCache::Texture QSGDistanceFieldGlyphCache::s_emptyTexture;

QSGDistanceFieldGlyphCache::QSGDistanceFieldGlyphCache(QSGDistanceFieldGlyphCacheManager *man, QOpenGLContext *c, const QRawFont &font)
    : m_manager(man)
    , m_pendingGlyphs(64)
{
    Q_ASSERT(font.isValid());

    QRawFontPrivate *fontD = QRawFontPrivate::get(font);
    m_glyphCount = fontD->fontEngine->glyphCount();

    m_doubleGlyphResolution = qt_fontHasNarrowOutlines(font) && m_glyphCount < QT_DISTANCEFIELD_HIGHGLYPHCOUNT();

    m_referenceFont = font;
    // we set the same pixel size as used by the distance field internally.
    // this allows us to call pathForGlyph once and reuse the result.
    m_referenceFont.setPixelSize(QT_DISTANCEFIELD_BASEFONTSIZE(m_doubleGlyphResolution) * QT_DISTANCEFIELD_SCALE(m_doubleGlyphResolution));
    Q_ASSERT(m_referenceFont.isValid());
#ifndef QT_NO_OPENGL
    m_coreProfile = (c->format().profile() == QSurfaceFormat::CoreProfile);
#else
    Q_UNUSED(c)
#endif
}

QSGDistanceFieldGlyphCache::~QSGDistanceFieldGlyphCache()
{
}

QSGDistanceFieldGlyphCache::GlyphData &QSGDistanceFieldGlyphCache::glyphData(glyph_t glyph)
{
    QHash<glyph_t, GlyphData>::iterator data = m_glyphsData.find(glyph);
    if (data == m_glyphsData.end()) {
        GlyphData gd;
        gd.texture = &s_emptyTexture;
        gd.path = m_referenceFont.pathForGlyph(glyph);
        // need bounding rect in base font size scale
        qreal scaleFactor = qreal(1) / QT_DISTANCEFIELD_SCALE(m_doubleGlyphResolution);
        QTransform scaleDown;
        scaleDown.scale(scaleFactor, scaleFactor);
        gd.boundingRect = scaleDown.mapRect(gd.path.boundingRect());
        data = m_glyphsData.insert(glyph, gd);
    }
    return data.value();
}

QSGDistanceFieldGlyphCache::Metrics QSGDistanceFieldGlyphCache::glyphMetrics(glyph_t glyph, qreal pixelSize)
{
    GlyphData &gd = glyphData(glyph);
    qreal scale = fontScale(pixelSize);

    Metrics m;
    m.width = gd.boundingRect.width() * scale;
    m.height = gd.boundingRect.height() * scale;
    m.baselineX = gd.boundingRect.x() * scale;
    m.baselineY = -gd.boundingRect.y() * scale;

    return m;
}

void QSGDistanceFieldGlyphCache::populate(const QVector<glyph_t> &glyphs)
{
    QSet<glyph_t> referencedGlyphs;
    QSet<glyph_t> newGlyphs;
    int count = glyphs.count();
    for (int i = 0; i < count; ++i) {
        glyph_t glyphIndex = glyphs.at(i);
        if ((int) glyphIndex >= glyphCount() && glyphCount() > 0) {
            qWarning("Warning: distance-field glyph is not available with index %d", glyphIndex);
            continue;
        }

        GlyphData &gd = glyphData(glyphIndex);
        ++gd.ref;
        referencedGlyphs.insert(glyphIndex);

        if (gd.texCoord.isValid() || m_populatingGlyphs.contains(glyphIndex))
            continue;

        m_populatingGlyphs.insert(glyphIndex);

        if (gd.boundingRect.isEmpty()) {
            gd.texCoord.width = 0;
            gd.texCoord.height = 0;
        } else {
            newGlyphs.insert(glyphIndex);
        }
    }

    referenceGlyphs(referencedGlyphs);
    if (!newGlyphs.isEmpty())
        requestGlyphs(newGlyphs);
}

void QSGDistanceFieldGlyphCache::release(const QVector<glyph_t> &glyphs)
{
    QSet<glyph_t> unusedGlyphs;
    int count = glyphs.count();
    for (int i = 0; i < count; ++i) {
        glyph_t glyphIndex = glyphs.at(i);
        GlyphData &gd = glyphData(glyphIndex);
        if (--gd.ref == 0 && !gd.texCoord.isNull())
            unusedGlyphs.insert(glyphIndex);
    }
    releaseGlyphs(unusedGlyphs);
}

void QSGDistanceFieldGlyphCache::update()
{
    m_populatingGlyphs.clear();

    if (m_pendingGlyphs.isEmpty())
        return;

    bool profileFrames = QSG_LOG_TIME_GLYPH().isDebugEnabled();
    if (profileFrames)
        qsg_render_timer.start();
    Q_QUICK_SG_PROFILE_START(QQuickProfiler::SceneGraphAdaptationLayerFrame);

    QList<QDistanceField> distanceFields;
    const int pendingGlyphsSize = m_pendingGlyphs.size();
    distanceFields.reserve(pendingGlyphsSize);
    for (int i = 0; i < pendingGlyphsSize; ++i) {
        GlyphData &gd = glyphData(m_pendingGlyphs.at(i));
        distanceFields.append(QDistanceField(gd.path,
                                             m_pendingGlyphs.at(i),
                                             m_doubleGlyphResolution));
        gd.path = QPainterPath(); // no longer needed, so release memory used by the painter path
    }

    qint64 renderTime = 0;
    int count = m_pendingGlyphs.size();
    if (profileFrames)
        renderTime = qsg_render_timer.nsecsElapsed();
    Q_QUICK_SG_PROFILE_RECORD(QQuickProfiler::SceneGraphAdaptationLayerFrame,
                              QQuickProfiler::SceneGraphAdaptationLayerGlyphRender);

    m_pendingGlyphs.reset();

    storeGlyphs(distanceFields);

#if defined(QSG_DISTANCEFIELD_CACHE_DEBUG)
    for (Texture texture : qAsConst(m_textures))
        saveTexture(texture.textureId, texture.size.width(), texture.size.height());
#endif

    if (QSG_LOG_TIME_GLYPH().isDebugEnabled()) {
        quint64 now = qsg_render_timer.elapsed();
        qCDebug(QSG_LOG_TIME_GLYPH,
                "distancefield: %d glyphs prepared in %dms, rendering=%d, upload=%d",
                count,
                (int) now,
                int(renderTime / 1000000),
                int((now - (renderTime / 1000000))));
    }
    Q_QUICK_SG_PROFILE_END_WITH_PAYLOAD(QQuickProfiler::SceneGraphAdaptationLayerFrame,
                                        QQuickProfiler::SceneGraphAdaptationLayerGlyphStore,
                                        (qint64)count);
}

void QSGDistanceFieldGlyphCache::setGlyphsPosition(const QList<GlyphPosition> &glyphs)
{
    QVector<quint32> invalidatedGlyphs;

    int count = glyphs.count();
    for (int i = 0; i < count; ++i) {
        GlyphPosition glyph = glyphs.at(i);
        GlyphData &gd = glyphData(glyph.glyph);

        if (!gd.texCoord.isNull())
            invalidatedGlyphs.append(glyph.glyph);

        gd.texCoord.xMargin = QT_DISTANCEFIELD_RADIUS(m_doubleGlyphResolution) / qreal(QT_DISTANCEFIELD_SCALE(m_doubleGlyphResolution));
        gd.texCoord.yMargin = QT_DISTANCEFIELD_RADIUS(m_doubleGlyphResolution) / qreal(QT_DISTANCEFIELD_SCALE(m_doubleGlyphResolution));
        gd.texCoord.x = glyph.position.x();
        gd.texCoord.y = glyph.position.y();
        gd.texCoord.width = gd.boundingRect.width();
        gd.texCoord.height = gd.boundingRect.height();
    }

    if (!invalidatedGlyphs.isEmpty()) {
        QLinkedList<QSGDistanceFieldGlyphConsumer *>::iterator it = m_registeredNodes.begin();
        while (it != m_registeredNodes.end()) {
            (*it)->invalidateGlyphs(invalidatedGlyphs);
            ++it;
        }
    }
}

void QSGDistanceFieldGlyphCache::registerOwnerElement(QQuickItem *ownerElement)
{
    Q_UNUSED(ownerElement);
}

void QSGDistanceFieldGlyphCache::unregisterOwnerElement(QQuickItem *ownerElement)
{
    Q_UNUSED(ownerElement);
}

void QSGDistanceFieldGlyphCache::processPendingGlyphs()
{
    /* Intentionally empty */
}

void QSGDistanceFieldGlyphCache::setGlyphsTexture(const QVector<glyph_t> &glyphs, const Texture &tex)
{
    int i = m_textures.indexOf(tex);
    if (i == -1) {
        m_textures.append(tex);
        i = m_textures.size() - 1;
    } else {
        m_textures[i].size = tex.size;
    }
    Texture *texture = &(m_textures[i]);

    QVector<quint32> invalidatedGlyphs;

    int count = glyphs.count();
    for (int j = 0; j < count; ++j) {
        glyph_t glyphIndex = glyphs.at(j);
        GlyphData &gd = glyphData(glyphIndex);
        if (gd.texture != &s_emptyTexture)
            invalidatedGlyphs.append(glyphIndex);
        gd.texture = texture;
    }

    if (!invalidatedGlyphs.isEmpty()) {
        QLinkedList<QSGDistanceFieldGlyphConsumer*>::iterator it = m_registeredNodes.begin();
        while (it != m_registeredNodes.end()) {
            (*it)->invalidateGlyphs(invalidatedGlyphs);
            ++it;
        }
    }
}

void QSGDistanceFieldGlyphCache::markGlyphsToRender(const QVector<glyph_t> &glyphs)
{
    int count = glyphs.count();
    for (int i = 0; i < count; ++i)
        m_pendingGlyphs.add(glyphs.at(i));
}

void QSGDistanceFieldGlyphCache::updateTexture(uint oldTex, uint newTex, const QSize &newTexSize)
{
    int count = m_textures.count();
    for (int i = 0; i < count; ++i) {
        Texture &tex = m_textures[i];
        if (tex.textureId == oldTex) {
            tex.textureId = newTex;
            tex.size = newTexSize;
            return;
        }
    }
}

#if defined(QSG_DISTANCEFIELD_CACHE_DEBUG)
#include <QtGui/qopenglfunctions.h>

void QSGDistanceFieldGlyphCache::saveTexture(GLuint textureId, int width, int height) const
{
    QOpenGLFunctions *functions = QOpenGLContext::currentContext()->functions();

    GLuint fboId;
    functions->glGenFramebuffers(1, &fboId);

    GLuint tmpTexture = 0;
    functions->glGenTextures(1, &tmpTexture);
    functions->glBindTexture(GL_TEXTURE_2D, tmpTexture);
    functions->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    functions->glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    functions->glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    functions->glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    functions->glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    functions->glBindTexture(GL_TEXTURE_2D, 0);

    functions->glBindFramebuffer(GL_FRAMEBUFFER_EXT, fboId);
    functions->glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D,
                                      tmpTexture, 0);

    functions->glActiveTexture(GL_TEXTURE0);
    functions->glBindTexture(GL_TEXTURE_2D, textureId);

    functions->glDisable(GL_STENCIL_TEST);
    functions->glDisable(GL_DEPTH_TEST);
    functions->glDisable(GL_SCISSOR_TEST);
    functions->glDisable(GL_BLEND);

    GLfloat textureCoordinateArray[8];
    textureCoordinateArray[0] = 0.0f;
    textureCoordinateArray[1] = 0.0f;
    textureCoordinateArray[2] = 1.0f;
    textureCoordinateArray[3] = 0.0f;
    textureCoordinateArray[4] = 1.0f;
    textureCoordinateArray[5] = 1.0f;
    textureCoordinateArray[6] = 0.0f;
    textureCoordinateArray[7] = 1.0f;

    GLfloat vertexCoordinateArray[8];
    vertexCoordinateArray[0] = -1.0f;
    vertexCoordinateArray[1] = -1.0f;
    vertexCoordinateArray[2] =  1.0f;
    vertexCoordinateArray[3] = -1.0f;
    vertexCoordinateArray[4] =  1.0f;
    vertexCoordinateArray[5] =  1.0f;
    vertexCoordinateArray[6] = -1.0f;
    vertexCoordinateArray[7] =  1.0f;

    functions->glViewport(0, 0, width, height);
    functions->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertexCoordinateArray);
    functions->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, textureCoordinateArray);

    {
        static const char *vertexShaderSource =
                "attribute vec4      vertexCoordsArray; \n"
                "attribute vec2      textureCoordArray; \n"
                "varying   vec2      textureCoords;     \n"
                "void main(void) \n"
                "{ \n"
                "    gl_Position = vertexCoordsArray;   \n"
                "    textureCoords = textureCoordArray; \n"
                "} \n";

        static const char *fragmentShaderSource =
                "varying   vec2      textureCoords; \n"
                "uniform   sampler2D         texture;       \n"
                "void main() \n"
                "{ \n"
                "    gl_FragColor = texture2D(texture, textureCoords); \n"
                "} \n";

        GLuint vertexShader = functions->glCreateShader(GL_VERTEX_SHADER);
        GLuint fragmentShader = functions->glCreateShader(GL_FRAGMENT_SHADER);

        if (vertexShader == 0 || fragmentShader == 0) {
            GLenum error = functions->glGetError();
            qWarning("QSGDistanceFieldGlyphCache::saveTexture: Failed to create shaders. (GL error: %x)",
                     error);
            return;
        }

        functions->glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
        functions->glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
        functions->glCompileShader(vertexShader);

        GLint len = 1;
        functions->glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &len);

        char infoLog[2048];
        functions->glGetShaderInfoLog(vertexShader, 2048, NULL, infoLog);
        if (qstrlen(infoLog) > 0)
            qWarning("Problems compiling vertex shader:\n %s", infoLog);

        functions->glCompileShader(fragmentShader);
        functions->glGetShaderInfoLog(fragmentShader, 2048, NULL, infoLog);
        if (qstrlen(infoLog) > 0)
            qWarning("Problems compiling fragment shader:\n %s", infoLog);

        GLuint shaderProgram = functions->glCreateProgram();
        functions->glAttachShader(shaderProgram, vertexShader);
        functions->glAttachShader(shaderProgram, fragmentShader);

        functions->glBindAttribLocation(shaderProgram, 0, "vertexCoordsArray");
        functions->glBindAttribLocation(shaderProgram, 1, "textureCoordArray");

        functions->glLinkProgram(shaderProgram);
        functions->glGetProgramInfoLog(shaderProgram, 2048, NULL, infoLog);
        if (qstrlen(infoLog) > 0)
            qWarning("Problems linking shaders:\n %s", infoLog);

        functions->glUseProgram(shaderProgram);
        functions->glEnableVertexAttribArray(0);
        functions->glEnableVertexAttribArray(1);

        int textureUniformLocation = functions->glGetUniformLocation(shaderProgram, "texture");
        functions->glUniform1i(textureUniformLocation, 0);
    }

    functions->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    {
        GLenum error = functions->glGetError();
        if (error != GL_NO_ERROR)
            qWarning("glDrawArrays reported error 0x%x", error);
    }

    uchar *data = new uchar[width * height * 4];

    functions->glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);

    QImage image(data, width, height, QImage::Format_ARGB32);

    QByteArray fileName = m_referenceFont.familyName().toLatin1() + '_' + QByteArray::number(textureId);
    fileName = fileName.replace('/', '_').replace(' ', '_') + ".png";

    image.save(QString::fromLocal8Bit(fileName));

    {
        GLenum error = functions->glGetError();
        if (error != GL_NO_ERROR)
            qWarning("glReadPixels reported error 0x%x", error);
    }

    functions->glDisableVertexAttribArray(0);
    functions->glDisableVertexAttribArray(1);

    functions->glDeleteFramebuffers(1, &fboId);
    functions->glDeleteTextures(1, &tmpTexture);

    delete[] data;
}
#endif

void QSGNodeVisitorEx::visitChildren(QSGNode *node)
{
    for (QSGNode *child = node->firstChild(); child; child = child->nextSibling()) {
        switch (child->type()) {
        case QSGNode::ClipNodeType: {
            QSGClipNode *c = static_cast<QSGClipNode*>(child);
            if (visit(c))
                visitChildren(c);
            endVisit(c);
            break;
        }
        case QSGNode::TransformNodeType: {
            QSGTransformNode *c = static_cast<QSGTransformNode*>(child);
            if (visit(c))
                visitChildren(c);
            endVisit(c);
            break;
        }
        case QSGNode::OpacityNodeType: {
            QSGOpacityNode *c = static_cast<QSGOpacityNode*>(child);
            if (visit(c))
                visitChildren(c);
            endVisit(c);
            break;
        }
        case QSGNode::GeometryNodeType: {
            if (child->flags() & QSGNode::IsVisitableNode) {
                QSGVisitableNode *v = static_cast<QSGVisitableNode*>(child);
                v->accept(this);
            } else {
                QSGGeometryNode *c = static_cast<QSGGeometryNode*>(child);
                if (visit(c))
                    visitChildren(c);
                endVisit(c);
            }
            break;
        }
        case QSGNode::RootNodeType: {
            QSGRootNode *root = static_cast<QSGRootNode*>(child);
            if (visit(root))
                visitChildren(root);
            endVisit(root);
            break;
        }
        case QSGNode::BasicNodeType: {
            visitChildren(child);
            break;
        }
        case QSGNode::RenderNodeType: {
            QSGRenderNode *r = static_cast<QSGRenderNode*>(child);
            if (visit(r))
                visitChildren(r);
            endVisit(r);
            break;
        }
        default:
            Q_UNREACHABLE();
            break;
        }
    }
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug debug, const QSGGuiThreadShaderEffectManager::ShaderInfo::InputParameter &p)
{
    QDebugStateSaver saver(debug);
    debug.space();
    debug << p.semanticName << "semindex" << p.semanticIndex;
    return debug;
}

QDebug operator<<(QDebug debug, const QSGGuiThreadShaderEffectManager::ShaderInfo::Variable &v)
{
    QDebugStateSaver saver(debug);
    debug.space();
    debug << v.name;
    switch (v.type) {
    case QSGGuiThreadShaderEffectManager::ShaderInfo::Constant:
        debug << "cvar" << "offset" << v.offset << "size" << v.size;
        break;
    case QSGGuiThreadShaderEffectManager::ShaderInfo::Sampler:
        debug << "sampler" << "bindpoint" << v.bindPoint;
        break;
    case QSGGuiThreadShaderEffectManager::ShaderInfo::Texture:
        debug << "texture" << "bindpoint" << v.bindPoint;
        break;
    default:
        break;
    }
    return debug;
}

QDebug operator<<(QDebug debug, const QSGShaderEffectNode::VariableData &vd)
{
    QDebugStateSaver saver(debug);
    debug.space();
    debug << vd.specialType;
    return debug;
}
#endif

QT_END_NAMESPACE
