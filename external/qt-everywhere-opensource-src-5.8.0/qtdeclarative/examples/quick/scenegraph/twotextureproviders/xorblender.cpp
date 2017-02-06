/****************************************************************************
**
** Copyright (C) 2014 Gunnar Sletta <gunnar@sletta.org>
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

#include "xorblender.h"

#include <QtCore/QPointer>

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>

#include <QtQuick/QSGSimpleMaterial>
#include <QtQuick/QSGTexture>
#include <QtQuick/QSGGeometryNode>
#include <QtQuick/QSGTextureProvider>

/* This example could just as well have been implemented all in QML using
 * a ShaderEffect, and for 90% of all usecases, using a ShaderEffect will
 * be sufficient. This example exists to illustrate how to consume
 * texture providers from C++ and how to use multiple texture sources in
 * a custom material.
 */

struct XorBlendState {
    QSGTexture *texture1;
    QSGTexture *texture2;
};

class XorBlendShader : public QSGSimpleMaterialShader<XorBlendState>
{
    QSG_DECLARE_SIMPLE_SHADER(XorBlendShader, XorBlendState)
public:

    const char *vertexShader() const {
        return
        "attribute highp vec4 aVertex;              \n"
        "attribute highp vec2 aTexCoord;            \n"
        "uniform highp mat4 qt_Matrix;              \n"
        "varying highp vec2 vTexCoord;              \n"
        "void main() {                              \n"
        "    gl_Position = qt_Matrix * aVertex;     \n"
        "    vTexCoord = aTexCoord;                 \n"
        "}";
    }

    const char *fragmentShader() const {
        return
        "uniform lowp float qt_Opacity;                                             \n"
        "uniform lowp sampler2D uSource1;                                           \n"
        "uniform lowp sampler2D uSource2;                                           \n"
        "varying highp vec2 vTexCoord;                                              \n"
        "void main() {                                                              \n"
        "    lowp vec4 p1 = texture2D(uSource1, vTexCoord);                         \n"
        "    lowp vec4 p2 = texture2D(uSource2, vTexCoord);                         \n"
        "    gl_FragColor = (p1 * (1.0 - p2.a) + p2 * (1.0 - p1.a)) * qt_Opacity;   \n"
        "}";
    }

    QList<QByteArray> attributes() const {
        return QList<QByteArray>() << "aVertex" << "aTexCoord";
    }

    void updateState(const XorBlendState *state, const XorBlendState *) {
        QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
        // We bind the textures in inverse order so that we leave the updateState
        // function with GL_TEXTURE0 as the active texture unit. This is maintain
        // the "contract" that updateState should not mess up the GL state beyond
        // what is needed for this material.
        f->glActiveTexture(GL_TEXTURE1);
        state->texture2->bind();
        f->glActiveTexture(GL_TEXTURE0);
        state->texture1->bind();
    }

    void resolveUniforms() {
        // The texture units never change, only the texturess we bind to them so
        // we set these once and for all here.
        program()->setUniformValue("uSource1", 0); // GL_TEXTURE0
        program()->setUniformValue("uSource2", 1); // GL_TEXTURE1
    }
};

/* The rendering is split into two nodes. The top-most node is not actually
 * rendering anything, but is responsible for managing the texture providers.
 * The XorNode also has a geometry node internally which it uses to render
 * the texture providers using the XorBlendShader when all providers and
 * textures are all present.
 *
 * The texture providers are updated solely on the render thread (when rendering
 * is happening on a separate thread). This is why we are using preprocess
 * and direct signals between the the texture providers and the node rather
 * than updating state in updatePaintNode.
 */
class XorNode : public QObject, public QSGNode
{
    Q_OBJECT
public:
    XorNode(QSGTextureProvider *p1, QSGTextureProvider *p2)
        : m_provider1(p1)
        , m_provider2(p2)
    {
        setFlag(QSGNode::UsePreprocess, true);

        // Set up material so it is all set for later..
        m_material = XorBlendShader::createMaterial();
        m_material->state()->texture1 = 0;
        m_material->state()->texture2 = 0;
        m_material->setFlag(QSGMaterial::Blending);
        m_node.setMaterial(m_material);
        m_node.setFlag(QSGNode::OwnsMaterial);

        // Set up geometry, actual vertices will be initialized in updatePaintNode
        m_node.setGeometry(new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4));
        m_node.setFlag(QSGNode::OwnsGeometry);

        // If this node is used as in a shader effect source, we need to propegate
        // changes that will occur in this node outwards.
        connect(m_provider1.data(), &QSGTextureProvider::textureChanged, this, &XorNode::textureChange, Qt::DirectConnection);
        connect(m_provider2.data(), &QSGTextureProvider::textureChanged, this, &XorNode::textureChange, Qt::DirectConnection);
    }

    void preprocess() {
        XorBlendState *state = m_material->state();
        // Update the textures from the providers, calling into QSGDynamicTexture if required
        if (m_provider1) {
            state->texture1 = m_provider1->texture();
            if (QSGDynamicTexture *dt1 = qobject_cast<QSGDynamicTexture *>(state->texture1))
                dt1->updateTexture();
        }
        if (m_provider2) {
            state->texture2 = m_provider2->texture();
            if (QSGDynamicTexture *dt2 = qobject_cast<QSGDynamicTexture *>(state->texture2))
                dt2->updateTexture();
        }

        // Remove node from the scene graph if it is there and either texture is missing...
        if (m_node.parent() && (!state->texture1 || !state->texture2))
            removeChildNode(&m_node);

        // Add it if it is not already there and both textures are present..
        else if (!m_node.parent() && state->texture1 && state->texture2)
            appendChildNode(&m_node);
    }

    void setRect(const QRectF &rect) {
        // Update geometry if it has changed and mark the change in the scene graph.
        if (m_rect != rect) {
            m_rect = rect;
            QSGGeometry::updateTexturedRectGeometry(m_node.geometry(), m_rect, QRectF(0, 0, 1, 1));
            m_node.markDirty(QSGNode::DirtyGeometry);
        }
    }

public slots:
    void textureChange() {
        // When our sources change, we will look different, so signal the change to the
        // scene graph.
        markDirty(QSGNode::DirtyMaterial);
    }

private:
    QRectF m_rect;
    QSGSimpleMaterial<XorBlendState> *m_material;
    QSGGeometryNode m_node;
    QPointer<QSGTextureProvider> m_provider1;
    QPointer<QSGTextureProvider> m_provider2;
};

XorBlender::XorBlender(QQuickItem *parent)
    : QQuickItem(parent)
    , m_source1(0)
    , m_source2(0)
    , m_source1Changed(false)
    , m_source2Changed(false)
{
    setFlag(ItemHasContents, true);
}

void XorBlender::setSource1(QQuickItem *i)
{
    if (i == m_source1)
        return;
    m_source1 = i;
    emit source1Changed(m_source1);
    m_source1Changed = true;
    update();
}

void XorBlender::setSource2(QQuickItem *i)
{
    if (i == m_source2)
        return;
    m_source2 = i;
    emit source2Changed(m_source2);
    m_source2Changed = true;
    update();
}

QSGNode *XorBlender::updatePaintNode(QSGNode *old, UpdatePaintNodeData *)
{
    // Check if our input is valid and abort if not, deleting the old node.
    bool abort = false;
    if (!m_source1 || !m_source1->isTextureProvider()) {
        qDebug() << "source1 is missing or not a texture provider";
        abort = true;
    }
    if (!m_source2 || !m_source2->isTextureProvider()) {
        qDebug() << "source2 is missing or not a texture provider";
        abort = true;
    }
    if (abort) {
        delete old;
        return 0;
    }

    XorNode *node = static_cast<XorNode *>(old);

    // If the sources have changed, recreate the nodes
    if (m_source1Changed || m_source2Changed) {
        delete node;
        node = 0;
        m_source1Changed = false;
        m_source2Changed = false;
    }

    // Create a new XorNode for us to render with.
    if (!node)
        node = new XorNode(m_source1->textureProvider(), m_source2->textureProvider());

    // Update the geometry of the node to match the new bounding rect
    node->setRect(boundingRect());

    return node;
}

#include "xorblender.moc"
