/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the demonstration applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
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

#include <qguiapplication.h>

#include <qsgmaterial.h>
#include <qsgnode.h>

#include <qquickitem.h>
#include <qquickview.h>

#include <qsgsimplerectnode.h>

#include <qsgsimplematerial.h>

//! [1]
struct State
{
    QColor color;

    int compare(const State *other) const {
        uint rgb = color.rgba();
        uint otherRgb = other->color.rgba();

        if (rgb == otherRgb) {
            return 0;
        } else if (rgb < otherRgb) {
            return -1;
        } else {
            return 1;
        }
    }
};
//! [1]

//! [2]
class Shader : public QSGSimpleMaterialShader<State>
{
    QSG_DECLARE_SIMPLE_COMPARABLE_SHADER(Shader, State);
//! [2] //! [3]
public:

    const char *vertexShader() const {
        return
                "attribute highp vec4 aVertex;                              \n"
                "attribute highp vec2 aTexCoord;                            \n"
                "uniform highp mat4 qt_Matrix;                              \n"
                "varying highp vec2 texCoord;                               \n"
                "void main() {                                              \n"
                "    gl_Position = qt_Matrix * aVertex;                     \n"
                "    texCoord = aTexCoord;                                  \n"
                "}";
    }

    const char *fragmentShader() const {
        return
                "uniform lowp float qt_Opacity;                             \n"
                "uniform lowp vec4 color;                                   \n"
                "varying highp vec2 texCoord;                               \n"
                "void main ()                                               \n"
                "{                                                          \n"
                "    gl_FragColor = texCoord.y * texCoord.x * color * qt_Opacity;  \n"
                "}";
    }
//! [3] //! [4]
    QList<QByteArray> attributes() const
    {
        return QList<QByteArray>() << "aVertex" << "aTexCoord";
    }
//! [4] //! [5]
    void updateState(const State *state, const State *)
    {
        program()->setUniformValue(id_color, state->color);
    }
//! [5] //! [6]
    void resolveUniforms()
    {
        id_color = program()->uniformLocation("color");
    }

private:
    int id_color;
//! [6]
};


//! [7]
class ColorNode : public QSGGeometryNode
{
public:
    ColorNode()
        : m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4)
    {
        setGeometry(&m_geometry);

        QSGSimpleMaterial<State> *material = Shader::createMaterial();
        material->setFlag(QSGMaterial::Blending);
        setMaterial(material);
        setFlag(OwnsMaterial);
    }

    QSGGeometry m_geometry;
};
//! [7]


//! [8]
class Item : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)

public:

    Item()
    {
        setFlag(ItemHasContents, true);
    }

    void setColor(const QColor &color) {
        if (m_color != color) {
            m_color = color;
            emit colorChanged();
            update();
        }
    }
    QColor color() const {
        return m_color;
    }

signals:
    void colorChanged();

private:
  QColor m_color;

//! [8] //! [9]
public:
    QSGNode *updatePaintNode(QSGNode *node, UpdatePaintNodeData *)
    {
        ColorNode *n = static_cast<ColorNode *>(node);
        if (!node)
            n = new ColorNode();

        QSGGeometry::updateTexturedRectGeometry(n->geometry(), boundingRect(), QRectF(0, 0, 1, 1));
        static_cast<QSGSimpleMaterial<State>*>(n->material())->state()->color = m_color;

        n->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);

        return n;
    }
};
//! [9] //! [11]
int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    qmlRegisterType<Item>("SimpleMaterial", 1, 0, "SimpleMaterialItem");

    QQuickView view;
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setSource(QUrl("qrc:///scenegraph/simplematerial/main.qml"));
    view.show();

    return app.exec();
}

#include "simplematerial.moc"
//! [11]
