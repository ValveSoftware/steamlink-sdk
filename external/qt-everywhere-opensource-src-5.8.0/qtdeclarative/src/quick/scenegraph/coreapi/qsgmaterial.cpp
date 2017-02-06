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

#include "qsgmaterial.h"
#include "qsgrenderer_p.h"
#include "qsgmaterialshader_p.h"
#ifndef QT_NO_OPENGL
# include <private/qsgshadersourcebuilder_p.h>
# include <private/qsgdefaultcontext_p.h>
# include <private/qsgdefaultrendercontext_p.h>
# include <QtGui/QOpenGLFunctions>
# include <QtGui/QOpenGLContext>
#endif

QT_BEGIN_NAMESPACE

#ifndef QT_NO_DEBUG
bool qsg_material_failure = false;
bool qsg_test_and_clear_material_failure()
{
    bool fail = qsg_material_failure;
    qsg_material_failure = false;
    return fail;
}

void qsg_set_material_failure()
{
    qsg_material_failure = true;
}
#endif
#ifndef QT_NO_OPENGL
const char *QSGMaterialShaderPrivate::loadShaderSource(QOpenGLShader::ShaderType type) const
{
    const QStringList files = m_sourceFiles[type];
    QSGShaderSourceBuilder builder;
    for (const QString &file : files)
        builder.appendSourceFile(file);
    m_sources[type] = builder.source();
    return m_sources[type].constData();
}
#endif

#ifndef QT_NO_DEBUG
static const bool qsg_leak_check = !qEnvironmentVariableIsEmpty("QML_LEAK_CHECK");
#endif

/*!
    \group qtquick-scenegraph-materials
    \title Qt Quick Scene Graph Material Classes
    \brief classes used to define materials in the Qt Quick Scene Graph.

    This page lists the material classes in \l {Qt Quick}'s
    \l {scene graph}{Qt Quick Scene Graph}.
 */

/*!
    \class QSGMaterialShader
    \brief The QSGMaterialShader class represents an OpenGL shader program
    in the renderer.
    \inmodule QtQuick
    \ingroup qtquick-scenegraph-materials

    The QSGMaterialShader API is very low-level. A more convenient API, which
    provides almost all the same features, is available through
    QSGSimpleMaterialShader.

    The QSGMaterial and QSGMaterialShader form a tight relationship. For one
    scene graph (including nested graphs), there is one unique QSGMaterialShader
    instance which encapsulates the QOpenGLShaderProgram the scene graph uses
    to render that material, such as a shader to flat coloring of geometry.
    Each QSGGeometryNode can have a unique QSGMaterial containing the
    how the shader should be configured when drawing that node, such as
    the actual color used to render the geometry.

    An instance of QSGMaterialShader is never created explicitly by the user,
    it will be created on demand by the scene graph through
    QSGMaterial::createShader(). The scene graph will make sure that there
    is only one instance of each shader implementation through a scene graph.

    The source code returned from vertexShader() is used to control what the
    material does with the vertiex data that comes in from the geometry.
    The source code returned from the fragmentShader() is used to control
    what how the material should fill each individual pixel in the geometry.
    The vertex and fragment source code is queried once during initialization,
    changing what is returned from these functions later will not have
    any effect.

    The activate() function is called by the scene graph when a shader is
    is starting to be used. The deactivate function is called by the scene
    graph when the shader is no longer going to be used. While active,
    the scene graph may make one or more calls to updateState() which
    will update the state of the shader for each individual geometry to
    render.

    The attributeNames() returns the name of the attributes used in the
    vertexShader(). These are used in the default implementation of
    activate() and deactivate() to decide whice vertex registers are enabled.

    The initialize() function is called during program creation to allow
    subclasses to prepare for use, such as resolve uniform names in the
    vertexShader() and fragmentShader().

    A minimal example:
    \code
        class Shader : public QSGMaterialShader
        {
        public:
            const char *vertexShader() const {
                return
                "attribute highp vec4 vertex;          \n"
                "uniform highp mat4 matrix;            \n"
                "void main() {                         \n"
                "    gl_Position = matrix * vertex;    \n"
                "}";
            }

            const char *fragmentShader() const {
                return
                "uniform lowp float opacity;                            \n"
                "void main() {                                          \n"
                        "    gl_FragColor = vec4(1, 0, 0, 1) * opacity; \n"
                "}";
            }

            char const *const *attributeNames() const
            {
                static char const *const names[] = { "vertex", 0 };
                return names;
            }

            void initialize()
            {
                QSGMaterialShader::initialize();
                m_id_matrix = program()->uniformLocation("matrix");
                m_id_opacity = program()->uniformLocation("opacity");
            }

            void updateState(const RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial)
            {
                Q_ASSERT(program()->isLinked());
                if (state.isMatrixDirty())
                    program()->setUniformValue(m_id_matrix, state.combinedMatrix());
                if (state.isOpacityDirty())
                    program()->setUniformValue(m_id_opacity, state.opacity());
            }

        private:
            int m_id_matrix;
            int m_id_opacity;
        };
    \endcode

    \note All classes with QSG prefix should be used solely on the scene graph's
    rendering thread. See \l {Scene Graph and Rendering} for more information.

 */



/*!
    Creates a new QSGMaterialShader.
 */
QSGMaterialShader::QSGMaterialShader()
    : d_ptr(new QSGMaterialShaderPrivate)
{
}

/*!
    \internal
 */
QSGMaterialShader::QSGMaterialShader(QSGMaterialShaderPrivate &dd)
    : d_ptr(&dd)
{
}

/*!
    \internal
 */
QSGMaterialShader::~QSGMaterialShader()
{
}

/*!
    \fn char const *const *QSGMaterialShader::attributeNames() const

    Returns a zero-terminated array describing the names of the
    attributes used in the vertex shader.

    This function is called when the shader is compiled to specify
    which attributes exist. The order of the attribute names
    defines the attribute register position in the vertex shader.
 */

#ifndef QT_NO_OPENGL
/*!
    \fn const char *QSGMaterialShader::vertexShader() const

    Called when the shader is being initialized to get the vertex
    shader source code.

    The contents returned from this function should never change.
*/
const char *QSGMaterialShader::vertexShader() const
{
    Q_D(const QSGMaterialShader);
    return d->loadShaderSource(QOpenGLShader::Vertex);
}


/*!
   \fn const char *QSGMaterialShader::fragmentShader() const

    Called when the shader is being initialized to get the fragment
    shader source code.

    The contents returned from this function should never change.
*/
const char *QSGMaterialShader::fragmentShader() const
{
    Q_D(const QSGMaterialShader);
    return d->loadShaderSource(QOpenGLShader::Fragment);
}


/*!
    \fn QOpenGLShaderProgram *QSGMaterialShader::program()

    Returns the shader program used by this QSGMaterialShader.
 */
#endif

/*!
    \fn void QSGMaterialShader::initialize()

    Reimplement this function to do one-time initialization when the
    shader program is compiled. The OpenGL shader program is compiled
    and linked, but not bound, when this function is called.
 */


/*!
    This function is called by the scene graph to indicate that geometry is
    about to be rendered using this shader.

    State that is global for all uses of the shader, independent of the geometry
    that is being drawn, can be setup in this function.
 */

void QSGMaterialShader::activate()
{
}



/*!
    This function is called by the scene graph to indicate that geometry will
    no longer to be rendered using this shader.
 */

void QSGMaterialShader::deactivate()
{
}



/*!
    This function is called by the scene graph before geometry is rendered
    to make sure the shader is in the right state.

    The current rendering \a state is passed from the scene graph. If the state
    indicates that any state is dirty, the updateState implementation must
    update accordingly for the geometry to render correctly.

    The subclass specific state, such as the color of a flat color material, should
    be extracted from \a newMaterial to update the color uniforms accordingly.

    The \a oldMaterial can be used to minimze state changes when updating
    material states. The \a oldMaterial is 0 if this shader was just activated.

    \sa activate(), deactivate()
 */

void QSGMaterialShader::updateState(const RenderState & /* state */, QSGMaterial * /* newMaterial */, QSGMaterial * /* oldMaterial */)
{
}

#ifndef QT_NO_OPENGL
/*!
    Sets the GLSL source file for the shader stage \a type to \a sourceFile. The
    default implementation of the vertexShader() and fragmentShader() functions
    will load the source files set by this function.

    This function is useful when you have a single source file for a given shader
    stage. If your shader consists of multiple source files then use
    setShaderSourceFiles()

    \sa setShaderSourceFiles(), vertexShader(), fragmentShader()
 */
void QSGMaterialShader::setShaderSourceFile(QOpenGLShader::ShaderType type, const QString &sourceFile)
{
    Q_D(QSGMaterialShader);
    d->m_sourceFiles[type] = (QStringList() << sourceFile);
}

/*!
    Sets the GLSL source files for the shader stage \a type to \a sourceFiles. The
    default implementation of the vertexShader() and fragmentShader() functions
    will load the source files set by this function in the order given.

    \sa setShaderSourceFile(), vertexShader(), fragmentShader()
 */
void QSGMaterialShader::setShaderSourceFiles(QOpenGLShader::ShaderType type, const QStringList &sourceFiles)
{
    Q_D(QSGMaterialShader);
    d->m_sourceFiles[type] = sourceFiles;
}

/*!
    This function is called when the shader is initialized to compile the
    actual QOpenGLShaderProgram. Do not call it explicitly.

    The default implementation will extract the vertexShader() and
    fragmentShader() and bind the names returned from attributeNames()
    to consecutive vertex attribute registers starting at 0.
 */

void QSGMaterialShader::compile()
{
    Q_ASSERT_X(!m_program.isLinked(), "QSGSMaterialShader::compile()", "Compile called multiple times!");

    program()->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader());
    program()->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShader());

    char const *const *attr = attributeNames();
#ifndef QT_NO_DEBUG
    int maxVertexAttribs = 0;
    QOpenGLFunctions *funcs = QOpenGLContext::currentContext()->functions();
    funcs->glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);
    for (int i = 0; attr[i]; ++i) {
        if (i >= maxVertexAttribs) {
            qFatal("List of attribute names is either too long or not null-terminated.\n"
                   "Maximum number of attributes on this hardware is %i.\n"
                   "Vertex shader:\n%s\n"
                   "Fragment shader:\n%s\n",
                   maxVertexAttribs, vertexShader(), fragmentShader());
        }
        if (*attr[i])
            program()->bindAttributeLocation(attr[i], i);
    }
#else
    for (int i = 0; attr[i]; ++i) {
        if (*attr[i])
            program()->bindAttributeLocation(attr[i], i);
    }
#endif

    if (!program()->link()) {
        qWarning("QSGMaterialShader: Shader compilation failed:");
        qWarning() << program()->log();
    }
}

#endif

/*!
    \class QSGMaterialShader::RenderState
    \brief The QSGMaterialShader::RenderState encapsulates the current rendering state
    during a call to QSGMaterialShader::updateState().
    \inmodule QtQuick

    The render state contains a number of accessors that the shader needs to respect
    in order to conform to the current state of the scene graph.

    The instance is only valid inside a call to QSGMaterialShader::updateState() and
    should not be used outisde this function.
 */



/*!
    \enum QSGMaterialShader::RenderState::DirtyState

    \value DirtyMatrix Used to indicate that the matrix has changed and must be updated.

    \value DirtyOpacity Used to indicate that the opacity has changed and must be updated.
 */



/*!
    \fn bool QSGMaterialShader::RenderState::isMatrixDirty() const

    Returns \c true if the dirtyStates() contain the dirty matrix state,
    otherwise returns \c false.
 */



/*!
    \fn bool QSGMaterialShader::RenderState::isOpacityDirty() const

    Returns \c true if the dirtyStates() contains the dirty opacity state,
    otherwise returns \c false.
 */



/*!
    \fn QSGMaterialShader::RenderState::DirtyStates QSGMaterialShader::RenderState::dirtyStates() const

    Returns which rendering states that have changed and needs to be updated
    for geometry rendered with this material to conform to the current
    rendering state.
 */



/*!
    Returns the accumulated opacity to be used for rendering.
 */

float QSGMaterialShader::RenderState::opacity() const
{
    Q_ASSERT(m_data);
    return static_cast<const QSGRenderer *>(m_data)->currentOpacity();
}

/*!
    Returns the modelview determinant to be used for rendering.
 */

float QSGMaterialShader::RenderState::determinant() const
{
    Q_ASSERT(m_data);
    return static_cast<const QSGRenderer *>(m_data)->determinant();
}

/*!
    Returns the matrix combined of modelview matrix and project matrix.
 */

QMatrix4x4 QSGMaterialShader::RenderState::combinedMatrix() const
{
    Q_ASSERT(m_data);
    return static_cast<const QSGRenderer *>(m_data)->currentCombinedMatrix();
}
/*!
   Returns the ratio between physical pixels and device-independent pixels
   to be used for rendering.
*/
float QSGMaterialShader::RenderState::devicePixelRatio() const
{
    Q_ASSERT(m_data);
    return static_cast<const QSGRenderer *>(m_data)->devicePixelRatio();
}



/*!
    Returns the model view matrix.

    If the material has the RequiresFullMatrix flag
    set, this is guaranteed to be the complete transform
    matrix calculated from the scenegraph.

    However, if this flag is not set, the renderer may
    choose to alter this matrix. For example, it may
    pre-transform vertices on the CPU and set this matrix
    to identity.

    In a situation such as the above, it is still possible
    to retrieve the actual matrix determinant by setting
    the RequiresDeterminant flag in the material and
    calling the determinant() accessor.
 */

QMatrix4x4 QSGMaterialShader::RenderState::modelViewMatrix() const
{
    Q_ASSERT(m_data);
    return static_cast<const QSGRenderer *>(m_data)->currentModelViewMatrix();
}

/*!
    Returns the projection matrix.
 */

QMatrix4x4 QSGMaterialShader::RenderState::projectionMatrix() const
{
    Q_ASSERT(m_data);
    return static_cast<const QSGRenderer *>(m_data)->currentProjectionMatrix();
}



/*!
    Returns the viewport rect of the surface being rendered to.
 */

QRect QSGMaterialShader::RenderState::viewportRect() const
{
    Q_ASSERT(m_data);
    return static_cast<const QSGRenderer *>(m_data)->viewportRect();
}



/*!
    Returns the device rect of the surface being rendered to
 */

QRect QSGMaterialShader::RenderState::deviceRect() const
{
    Q_ASSERT(m_data);
    return static_cast<const QSGRenderer *>(m_data)->deviceRect();
}

#ifndef QT_NO_OPENGL

/*!
    Returns the QOpenGLContext that is being used for rendering
 */

QOpenGLContext *QSGMaterialShader::RenderState::context() const
{
    // Only the QSGDefaultRenderContext will have an OpenGL Context to query
    auto openGLRenderContext = static_cast<const QSGDefaultRenderContext *>(static_cast<const QSGRenderer *>(m_data)->context());
    if (openGLRenderContext != nullptr)
        return openGLRenderContext->openglContext();
    else
        return nullptr;
}

#endif

#ifndef QT_NO_DEBUG
static int qt_material_count = 0;

static void qt_print_material_count()
{
    qDebug("Number of leaked materials: %i", qt_material_count);
    qt_material_count = -1;
}
#endif

/*!
    \class QSGMaterialType
    \brief The QSGMaterialType class is used as a unique type token in combination with QSGMaterial.
    \inmodule QtQuick
    \ingroup qtquick-scenegraph-materials

    It serves no purpose outside the QSGMaterial::type() function.

    \note All classes with QSG prefix should be used solely on the scene graph's
    rendering thread. See \l {Scene Graph and Rendering} for more information.
 */

/*!
    \class QSGMaterial
    \brief The QSGMaterial class encapsulates rendering state for a shader program.
    \inmodule QtQuick
    \ingroup qtquick-scenegraph-materials

    The QSGMaterial API is very low-level. A more convenient API, which
    provides almost all the same features, is available through
    QSGSimpleMaterialShader.

    The QSGMaterial and QSGMaterialShader subclasses form a tight relationship. For
    one scene graph (including nested graphs), there is one unique QSGMaterialShader
    instance which encapsulates the QOpenGLShaderProgram the scene graph uses
    to render that material, such as a shader to flat coloring of geometry.
    Each QSGGeometryNode can have a unique QSGMaterial containing the
    how the shader should be configured when drawing that node, such as
    the actual color to used to render the geometry.

    The QSGMaterial has two virtual functions that both need to be implemented.
    The function type() should return a unique instance for all instances of a
    specific subclass. The createShader() function should return a new instance
    of QSGMaterialShader, specific to the subclass of QSGMaterial.

    A minimal QSGMaterial implementation could look like this:
    \code
        class Material : public QSGMaterial
        {
        public:
            QSGMaterialType *type() const { static QSGMaterialType type; return &type; }
            QSGMaterialShader *createShader() const { return new Shader; }
        };
    \endcode

    \note All classes with QSG prefix should be used solely on the scene graph's
    rendering thread. See \l {Scene Graph and Rendering} for more information.
 */

/*!
    \internal
 */

QSGMaterial::QSGMaterial()
    : m_flags(0)
{
    Q_UNUSED(m_reserved);
#ifndef QT_NO_DEBUG
    if (qsg_leak_check) {
        ++qt_material_count;
        static bool atexit_registered = false;
        if (!atexit_registered) {
            atexit(qt_print_material_count);
            atexit_registered = true;
        }
    }
#endif
}


/*!
    \internal
 */

QSGMaterial::~QSGMaterial()
{
#ifndef QT_NO_DEBUG
    if (qsg_leak_check) {
        --qt_material_count;
        if (qt_material_count < 0)
            qDebug("Material destroyed after qt_print_material_count() was called.");
    }
#endif
}



/*!
    \enum QSGMaterial::Flag

    \value Blending Set this flag to true if the material requires GL_BLEND to be
    enabled during rendering.

    \value RequiresDeterminant Set this flag to true if the material relies on
    the determinant of the matrix of the geometry nodes for rendering.

    \value RequiresFullMatrixExceptTranslate Set this flag to true if the material
    relies on the full matrix of the geometry nodes for rendering, except the translation part.

    \value RequiresFullMatrix Set this flag to true if the material relies on
    the full matrix of the geometry nodes for rendering.

    \value CustomCompileStep Starting with Qt 5.2, the scene graph will not always call
    QSGMaterialShader::compile() when its shader program is compiled and linked.
    Set this flag to enforce that the function is called.

 */

/*!
    \fn QSGMaterial::Flags QSGMaterial::flags() const

    Returns the material's flags.
 */



/*!
    Sets the flags \a flags on this material if \a on is true;
    otherwise clears the attribute.
*/

void QSGMaterial::setFlag(Flags flags, bool on)
{
    if (on)
        m_flags |= flags;
    else
        m_flags &= ~flags;
}



/*!
    Compares this material to \a other and returns 0 if they are equal; -1 if
    this material should sort before \a other and 1 if \a other should sort
    before.

    The scene graph can reorder geometry nodes to minimize state changes.
    The compare function is called during the sorting process so that
    the materials can be sorted to minimize state changes in each
    call to QSGMaterialShader::updateState().

    The this pointer and \a other is guaranteed to have the same type().
 */

int QSGMaterial::compare(const QSGMaterial *other) const
{
    Q_ASSERT(other && type() == other->type());
    return qint64(this) - qint64(other);
}



/*!
    \fn QSGMaterialType QSGMaterial::type() const

    This function is called by the scene graph to return a unique instance
    per subclass.
 */



/*!
    \fn QSGMaterialShader *QSGMaterial::createShader() const

    This function returns a new instance of a the QSGMaterialShader
    implementatation used to render geometry for a specific implementation
    of QSGMaterial.

    The function will be called only once for each material type that
    exists in the scene graph and will be cached internally.
*/


QT_END_NAMESPACE
