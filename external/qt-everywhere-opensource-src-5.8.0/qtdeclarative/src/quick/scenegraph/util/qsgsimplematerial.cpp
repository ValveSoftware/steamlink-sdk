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

/*!
    \class QSGSimpleMaterialShader

    \brief The QSGSimpleMaterialShader class provides a convenient way of
    building custom OpenGL-based materials for the scene graph.

    \inmodule QtQuick
    \ingroup qtquick-scenegraph-materials

    \warning This utility class is only functional when running with the OpenGL
    backend of the Qt Quick scenegraph.

    Where the QSGMaterial and QSGMaterialShader API requires a bit of
    boilerplate code to create a functioning material, the
    QSGSimpleMaterialShader tries to hide some of this through the use
    of templates.

    QSGSimpleMaterialShader::vertexShader() and
    QSGSimpleMaterialShader::fragmentShader() are used to specify the
    actual shader source code. The names of the vertex attributes
    should be listed in the QSGSimpleMaterialShader::attributes()

    QSGSimpleMaterialShader::updateState() is used to push the material
    state to the OpenGL shader program.

    The actual OpenGL shader program is accessible through the
    QSGSimpleMaterialShader::program() function.

    Each QSGSimpleMaterialShader implementation operates on a unique
    state struct. The state struct must be declared using the
    \c {QSG_DECLARE_SIMPLE_SHADER} macro.

    Here is a simple example of a custom solid-color:

    \code
    struct Color
    {
        float r, g, b, a;
    };

    class MinimalShader : public QSGSimpleMaterialShader<Color>
    {
        QSG_DECLARE_SIMPLE_SHADER(MinimalShader, Color)
    public:

        const char *vertexShader() const {
            return
            "attribute highp vec4 vertex;               \n"
            "uniform highp mat4 qt_Matrix;              \n"
            "void main() {                              \n"
            "    gl_Position = qt_Matrix * vertex;      \n"
            "}";
        }

        const char *fragmentShader() const {
            return
            "uniform lowp float qt_Opacity;             \n"
            "uniform lowp vec4 color;                   \n"
            "void main() {                              \n"
            "    gl_FragColor = color * qt_Opacity;     \n"
            "}";
        }

        QList<QByteArray> attributes() const {
            return QList<QByteArray>() << "vertex";
        }

        void updateState(const Color *color, const Color *) {
            program()->setUniformValue("color", color->r, color->g, color->b, color->a);
        }

    };
    \endcode

    Instances of materials using this shader can be created using the
    createMaterial() function which will be defined by the
    QSG_DECLARE_SIMPLE_SHADER macro.

    \code
        QSGSimpleMaterial<Color> *material = MinimalShader::createMaterial();
        material->state()->r = 1;
        material->state()->g = 0;
        material->state()->b = 0;
        material->state()->a = 1;

        node->setMaterial(material);
    \endcode

    The scene graph will often try to find materials that have the
    same or at least similar state so that these can be batched
    together inside the renderer, which gives better performance. To
    specify sortable material states, use
    QSG_DECLARE_SIMPLE_COMPARABLE_SHADER instead of
    QSG_DECLARE_SIMPLE_SHADER. The state struct must then also define
    the function:

    \code
    int compare(const Type *other) const;
    \endcode

    \warning The QSGSimpleMaterialShader relies on template
    instantiation to create a QSGMaterialType which the scene graph
    renderer internally uses to identify this shader. For this reason,
    the unique QSGSimpleMaterialShader implementation must be
    instantiated with a unique C++ type.

    \note All classes with QSG prefix should be used solely on the scene graph's
    rendering thread. See \l {Scene Graph and Rendering} for more information.

    \sa {Scene Graph - Simple Material}
 */

/*!
    \macro QSG_DECLARE_SIMPLE_SHADER(Shader, State)
    \relates QSGSimpleMaterialShader

    This macro is used to declare a QSGMaterialType and a \c
    createMaterial() function for \a Shader with the given \a State.
    */

/*!
    \macro QSG_DECLARE_SIMPLE_COMPARABLE_SHADER(Shader, State)
    \relates QSGSimpleMaterialShader

    This macro is used to declare a QSGMaterialType and a \c
    createMaterial() function for \a Shader with the given \a State,
    where the \a State class must define a compare function on the
    form:

    \code
    int compare(const State *other) const;
    \endcode
*/


/*!
    \fn char const *const *QSGSimpleMaterialShader::attributeNames() const
    \internal
 */

/*!
    \fn void QSGSimpleMaterialShader::initialize()
    \internal
 */

/*!
    \fn void QSGSimpleMaterialShader::resolveUniforms()

    Reimplement this function to resolve the location of named uniforms
    in the shader program.

    This function is called when the material shader is initialized.
 */

/*!
    \fn const char *QSGSimpleMaterialShader::uniformMatrixName() const

    Reimplement this function to give a different name to the uniform for
    item transformation. The default value is \c qt_Matrix.

 */

/*!
    \fn const char *QSGSimpleMaterialShader::uniformOpacityName() const

    Reimplement this function to give a different name to the uniform for
    item opacity. The default value is \c qt_Opacity.

    If the shader program does not implement the item opacity, the
    implemented function should return a null pointer.
 */

/*!
    \fn void QSGSimpleMaterialShader::updateState(const RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial)
    \internal
 */


/*!
    \fn QList<QByteArray> QSGSimpleMaterialShader::attributes() const

    Returns a list of names, declaring the vertex attributes in the
    vertex shader.
*/

/*!
    \fn void QSGSimpleMaterialShader::updateState(const State *newState, const State *oldState)

    Called whenever the state of this shader should be updated from
    \a oldState to \a newState, typical for each new set of
    geometries being drawn.

    Both the old and the new state are passed in so that the
    implementation can compare and minimize the state changes when
    applicable.
*/

/*!
    \class QSGSimpleMaterial

    \inmodule QtQuick
    \ingroup qtquick-scenegraph-materials

    \brief The QSGSimpleMaterial class is a template generated class
    used to store the state used with a QSGSimpleMateralShader.

    The state of the material is accessible through the template
    generated state() function.

    \inmodule QtQuick

    \note All classes with QSG prefix should be used solely on the scene graph's
    rendering thread. See \l {Scene Graph and Rendering} for more information.

    \sa QSGSimpleMaterialShader
*/


