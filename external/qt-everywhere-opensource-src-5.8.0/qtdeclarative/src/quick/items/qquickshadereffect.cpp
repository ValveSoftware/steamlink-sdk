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

#include <private/qquickshadereffect_p.h>
#include <private/qsgcontextplugin_p.h>
#include <private/qquickitem_p.h>
#ifndef QT_NO_OPENGL
#include <private/qquickopenglshadereffect_p.h>
#endif
#include <private/qquickgenericshadereffect_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype ShaderEffect
    \instantiates QQuickShaderEffect
    \inqmlmodule QtQuick
    \inherits Item
    \ingroup qtquick-effects
    \brief Applies custom shaders to a rectangle

    The ShaderEffect type applies a custom
    \l{vertexShader}{vertex} and \l{fragmentShader}{fragment (pixel)} shader to a
    rectangle. It allows you to write effects such as drop shadow, blur,
    colorize and page curl directly in QML.

    \note Depending on the Qt Quick scenegraph backend in use, the ShaderEffect
    type may not be supported (for example, with the software backend), or may
    use a different shading language with rules and expectations different from
    OpenGL and GLSL.

    \section1 OpenGL and GLSL

    There are two types of input to the \l vertexShader:
    uniform variables and attributes. Some are predefined:
    \list
    \li uniform mat4 qt_Matrix - combined transformation
       matrix, the product of the matrices from the root item to this
       ShaderEffect, and an orthogonal projection.
    \li uniform float qt_Opacity - combined opacity, the product of the
       opacities from the root item to this ShaderEffect.
    \li attribute vec4 qt_Vertex - vertex position, the top-left vertex has
       position (0, 0), the bottom-right (\l{Item::width}{width},
       \l{Item::height}{height}).
    \li attribute vec2 qt_MultiTexCoord0 - texture coordinate, the top-left
       coordinate is (0, 0), the bottom-right (1, 1). If \l supportsAtlasTextures
       is true, coordinates will be based on position in the atlas instead.
    \endlist

    In addition, any property that can be mapped to an OpenGL Shading Language
    (GLSL) type is available as a uniform variable. The following list shows
    how properties are mapped to GLSL uniform variables:
    \list
    \li bool, int, qreal -> bool, int, float - If the type in the shader is not
       the same as in QML, the value is converted automatically.
    \li QColor -> vec4 - When colors are passed to the shader, they are first
       premultiplied. Thus Qt.rgba(0.2, 0.6, 1.0, 0.5) becomes
       vec4(0.1, 0.3, 0.5, 0.5) in the shader, for example.
    \li QRect, QRectF -> vec4 - Qt.rect(x, y, w, h) becomes vec4(x, y, w, h) in
       the shader.
    \li QPoint, QPointF, QSize, QSizeF -> vec2
    \li QVector3D -> vec3
    \li QVector4D -> vec4
    \li QTransform -> mat3
    \li QMatrix4x4 -> mat4
    \li QQuaternion -> vec4, scalar value is \c w.
    \li \l Image -> sampler2D - Origin is in the top-left corner, and the
        color values are premultiplied. The texture is provided as is,
        excluding the Image item's fillMode. To include fillMode, use a
        ShaderEffectSource or Image::layer::enabled.
    \li \l ShaderEffectSource -> sampler2D - Origin is in the top-left
        corner, and the color values are premultiplied.
    \endlist

    The QML scene graph back-end may choose to allocate textures in texture
    atlases. If a texture allocated in an atlas is passed to a ShaderEffect,
    it is by default copied from the texture atlas into a stand-alone texture
    so that the texture coordinates span from 0 to 1, and you get the expected
    wrap modes. However, this will increase the memory usage. To avoid the
    texture copy, set \l supportsAtlasTextures for simple shaders using
    qt_MultiTexCoord0, or for each "uniform sampler2D <name>" declare a
    "uniform vec4 qt_SubRect_<name>" which will be assigned the texture's
    normalized source rectangle. For stand-alone textures, the source rectangle
    is [0, 1]x[0, 1]. For textures in an atlas, the source rectangle corresponds
    to the part of the texture atlas where the texture is stored.
    The correct way to calculate the texture coordinate for a texture called
    "source" within a texture atlas is
    "qt_SubRect_source.xy + qt_SubRect_source.zw * qt_MultiTexCoord0".

    The output from the \l fragmentShader should be premultiplied. If
    \l blending is enabled, source-over blending is used. However, additive
    blending can be achieved by outputting zero in the alpha channel.

    \table 70%
    \row
    \li \image declarative-shadereffectitem.png
    \li \qml
        import QtQuick 2.0

        Rectangle {
            width: 200; height: 100
            Row {
                Image { id: img;
                        sourceSize { width: 100; height: 100 } source: "qt-logo.png" }
                ShaderEffect {
                    width: 100; height: 100
                    property variant src: img
                    vertexShader: "
                        uniform highp mat4 qt_Matrix;
                        attribute highp vec4 qt_Vertex;
                        attribute highp vec2 qt_MultiTexCoord0;
                        varying highp vec2 coord;
                        void main() {
                            coord = qt_MultiTexCoord0;
                            gl_Position = qt_Matrix * qt_Vertex;
                        }"
                    fragmentShader: "
                        varying highp vec2 coord;
                        uniform sampler2D src;
                        uniform lowp float qt_Opacity;
                        void main() {
                            lowp vec4 tex = texture2D(src, coord);
                            gl_FragColor = vec4(vec3(dot(tex.rgb,
                                                vec3(0.344, 0.5, 0.156))),
                                                     tex.a) * qt_Opacity;
                        }"
                }
            }
        }
        \endqml
    \endtable

    \note Scene Graph textures have origin in the top-left corner rather than
    bottom-left which is common in OpenGL.

    For information about the GLSL version being used, see \l QtQuick::GraphicsInfo.

    Starting from Qt 5.8 ShaderEffect also supports reading the GLSL source
    code from files. Whenever the fragmentShader or vertexShader property value
    is a URL with the \c file or \c qrc schema, it is treated as a file
    reference and the source code is read from the specified file.

    \section1 Direct3D and HLSL

    Direct3D backends provide ShaderEffect support with HLSL. The Direct3D 12
    backend requires using at least Shader Model 5.0 both for vertex and pixel
    shaders. When necessary, GraphicsInfo.shaderType can be used to decide
    at runtime what kind of value to assign to \l fragmentShader or
    \l vertexShader.

    All concepts described above for OpenGL and GLSL apply to Direct3D and HLSL
    as well. There are however a number of notable practical differences, which
    are the following:

    Instead of uniforms, HLSL shaders are expected to use a single constant
    buffer, assigned to register \c b0. The special names \c qt_Matrix,
    \c qt_Opacity, and \c qt_SubRect_<name> function the same way as with GLSL.
    All other members of the buffer are expected to map to properties in the
    ShaderEffect item.

    \note The buffer layout must be compatible for both shaders. This means
    that application-provided shaders must make sure \c qt_Matrix and
    \c qt_Opacity are included in the buffer, starting at offset 0, when custom
    code is provided for one type of shader only, leading to ShaderEffect
    providing the other shader. This is due to ShaderEffect's built-in shader code
    declaring a constant buffer containing \c{float4x4 qt_Matrix; float qt_Opacity;}.

    Unlike GLSL's attributes, no names are used for vertex input elements.
    Therefore qt_Vertex and qt_MultiTexCoord0 are not relevant. Instead, the
    standard Direct3D semantics, \c POSITION and \c TEXCOORD (or \c TEXCOORD0)
    are used for identifying the correct input layout.

    Unlike GLSL's samplers, texture and sampler objects are separate in HLSL.
    Shaders are expected to expect 2D, non-array, non-multisample textures.
    Both the texture and sampler binding points are expected to be sequential
    and start from 0 (meaning registers \c{t0, t1, ...}, and \c{s0, s1, ...},
    respectively). Unlike with OpenGL, samplers are not mapped to Qt Quick item
    properties and therefore the name of the sampler is not relevant. Instead,
    it is the textures that map to properties referencing \l Image or
    \l ShaderEffectSource items.

    Unlike OpenGL, backends for modern APIs will typically prefer offline
    compilation and shipping pre-compiled bytecode with applications instead of
    inlined shader source strings. In this case the string properties for
    vertex and fragment shaders are treated as URLs referring to local files or
    files shipped via the Qt resource system.

    To check at runtime what is supported, use the
    GraphicsInfo.shaderSourceType and GraphicsInfo.shaderCompilationType
    properties. Note that these are bitmasks, because some backends may support
    multiple approaches.

    In case of Direct3D 12, all combinations are supported. If the vertexShader
    and fragmentShader properties form a valid URL with the \c file or \c qrc
    schema, the bytecode or HLSL source code is read from the specified file.
    The type of the file contents is detected automatically. Otherwise, the
    string is treated as HLSL source code and is compiled at runtime, assuming
    Shader Model 5.0 and an entry point of \c{"main"}. This allows dynamically
    constructing shader strings. However, whenever the shader source code is
    static, it is strongly recommended to pre-compile to bytecode using the
    \c fxc tool and refer to these files from QML. This will be a lot more
    efficient at runtime and allows catching syntax errors in the shaders at
    compile time.

    Unlike OpenGL, the Direct3D backend is able to perform runtime shader
    compilation on dedicated threads. This is managed transparently to the
    applications, and means that ShaderEffect items that contain HLSL source
    strings do not block the rendering or other parts of the application until
    the bytecode is ready.

    Using files with bytecode is more flexible also when it comes to the entry
    point name (it can be anything, not limited to \c main) and the shader
    model (it can be something newer than 5.0, for instance 5.1).

    \table 70%
    \row
    \li \qml
        import QtQuick 2.0

        Rectangle {
            width: 200; height: 100
            Row {
                Image { id: img;
                        sourceSize { width: 100; height: 100 } source: "qt-logo.png" }
                ShaderEffect {
                    width: 100; height: 100
                    property variant src: img
                    fragmentShader: "qrc:/effect_ps.cso"
                }
            }
        }
        \endqml
    \row
    \li where \c effect_ps.cso is the compiled bytecode for the following HLSL shader:
        \code
        cbuffer ConstantBuffer : register(b0)
        {
            float4x4 qt_Matrix;
            float qt_Opacity;
        };
        Texture2D src : register(t0);
        SamplerState srcSampler : register(s0);
        float4 ExamplePixelShader(float4 position : SV_POSITION, float2 coord : TEXCOORD0) : SV_TARGET
        {
            float4 tex = src.Sample(srcSampler, coord);
            float3 col = dot(tex.rgb, float3(0.344, 0.5, 0.156));
            return float4(col, tex.a) * qt_Opacity;
        }
        \endcode
    \endtable

    The above is equivalent to the OpenGL example presented earlier. The vertex
    shader is provided implicitly by ShaderEffect. Note that the output of the
    pixel shader is using premultiplied alpha and that \c qt_Matrix is present
    in the constant buffer at offset 0, even though the pixel shader does not
    use the value.

    If desired, the HLSL source code can be placed directly into the QML
    source, similarly to how its done with GLSL. The only difference in this
    case is the entry point name, which must be \c main when using inline
    source strings.

    Alternatively, we could also have referred to a file containing the source
    of the effect instead of the compiled bytecode version.

    Some effects will want to provide a vertex shader as well. Below is a
    similar effect with both the vertex and fragment shader provided by the
    application. This time the colorization factor is provided by the QML item
    instead of hardcoding it in the shader. This can allow, among others,
    animating the value using QML's and Qt Quick's standard facilities.

    \table 70%
    \row
    \li \qml
        import QtQuick 2.0

        Rectangle {
            width: 200; height: 100
            Row {
                Image { id: img;
                        sourceSize { width: 100; height: 100 } source: "qt-logo.png" }
                ShaderEffect {
                    width: 100; height: 100
                    property variant src: img
                    property variant color: Qt.vector3d(0.344, 0.5, 0.156)
                    vertexShader: "qrc:/effect_vs.cso"
                    fragmentShader: "qrc:/effect_ps.cso"
                }
            }
        }
        \endqml
    \row
    \li where \c effect_vs.cso and \c effect_ps.cso are the compiled bytecode
    for \c ExampleVertexShader and \c ExamplePixelShader. The source code is
    presented as one snippet here, the shaders can however be placed in
    separate source files as well.
        \code
        cbuffer ConstantBuffer : register(b0)
        {
            float4x4 qt_Matrix;
            float qt_Opacity;
            float3 color;
        };
        Texture2D src : register(t0);
        SamplerState srcSampler : register(s0);
        struct PSInput
        {
            float4 position : SV_POSITION;
            float2 coord : TEXCOORD0;
        };
        PSInput ExampleVertexShader(float4 position : POSITION, float2 coord : TEXCOORD0)
        {
            PSInput result;
            result.position = mul(qt_Matrix, position);
            result.coord = coord;
            return result;
        }
        float4 ExamplePixelShader(PSInput input) : SV_TARGET
        {
            float4 tex = src.Sample(srcSampler, coord);
            float3 col = dot(tex.rgb, color);
            return float4(col, tex.a) * qt_Opacity;
        }
        \endcode
    \endtable

    \note With OpenGL the \c y coordinate runs from bottom to top whereas with
    Direct 3D it goes top to bottom. For shader effect sources Qt Quick hides
    the difference by treating QtQuick::ShaderEffectSource::textureMirroring as
    appropriate, meaning texture coordinates in HLSL version of the shaders
    will not need any adjustments compared to the equivalent GLSL code.

    \section1 Cross-platform, Cross-API ShaderEffect Items

    Some applications will want to be functional with multiple accelerated
    graphics backends. This has consequences for ShaderEffect items because the
    supported shading languages may vary from backend to backend.

    There are two approaches to handle this: either write conditional property
    values based on GraphicsInfo.shaderType, or use file selectors. In practice
    the latter is strongly recommended as it leads to more concise and cleaner
    application code. The only case it is not suitable is when the source
    strings are constructed dynamically.

    \table 70%
    \row
    \li \qml
        import QtQuick 2.8 // for GraphicsInfo

        Rectangle {
            width: 200; height: 100
            Row {
                Image { id: img;
                        sourceSize { width: 100; height: 100 } source: "qt-logo.png" }
                ShaderEffect {
                    width: 100; height: 100
                    property variant src: img
                    property variant color: Qt.vector3d(0.344, 0.5, 0.156)
                    fragmentShader: GraphicsInfo.shaderType === GraphicsInfo.GLSL ?
                        "varying highp vec2 coord;
                        uniform sampler2D src;
                        uniform lowp float qt_Opacity;
                        void main() {
                            lowp vec4 tex = texture2D(src, coord);
                            gl_FragColor = vec4(vec3(dot(tex.rgb,
                                                vec3(0.344, 0.5, 0.156))),
                                                     tex.a) * qt_Opacity;"
                    : GraphicsInfo.shaderType === GraphicsInfo.HLSL ?
                        "cbuffer ConstantBuffer : register(b0)
                        {
                            float4x4 qt_Matrix;
                            float qt_Opacity;
                        };
                        Texture2D src : register(t0);
                        SamplerState srcSampler : register(s0);
                        float4 ExamplePixelShader(float4 position : SV_POSITION, float2 coord : TEXCOORD0) : SV_TARGET
                        {
                            float4 tex = src.Sample(srcSampler, coord);
                            float3 col = dot(tex.rgb, float3(0.344, 0.5, 0.156));
                            return float4(col, tex.a) * qt_Opacity;
                        }"
                    : ""
                }
            }
        }
        \endqml
    \row

    \li This is the first approach based on GraphicsInfo. Note that the value
    reported by GraphicsInfo is not up-to-date until the ShaderEffect item gets
    associated with a QQuickWindow. Before that, the reported value is
    GraphicsInfo.UnknownShadingLanguage. The alternative is to place the GLSL
    source code and the compiled D3D bytecode into the files
    \c{shaders/effect.frag} and \c{shaders/+hlsl/effect.frag}, include them in
    the Qt resource system, and let the ShaderEffect's internal QFileSelector
    do its job. The selector-less version is the GLSL source, while the \c hlsl
    selector is used when running on the D3D12 backend. The file under
    \c{+hlsl} can then contain either HLSL source code or compiled bytecode
    from the \c fxc tool. Additionally, when using a version 3.2 or newer core
    profile context with OpenGL, GLSL sources with a core profile compatible
    syntax can be placed under \c{+glslcore}.
        \qml
        import QtQuick 2.8 // for GraphicsInfo

        Rectangle {
            width: 200; height: 100
            Row {
                Image { id: img;
                        sourceSize { width: 100; height: 100 } source: "qt-logo.png" }
                ShaderEffect {
                    width: 100; height: 100
                    property variant src: img
                    property variant color: Qt.vector3d(0.344, 0.5, 0.156)
                    fragmentShader: "qrc:shaders/effect.frag" // selects the correct variant automatically
                }
            }
        }
        \endqml
    \endtable

    \section1 ShaderEffect and Item Layers

    The ShaderEffect type can be combined with \l {Item Layers} {layered items}.

    \table
    \row
      \li \b {Layer with effect disabled} \inlineimage qml-shadereffect-nolayereffect.png
      \li \b {Layer with effect enabled} \inlineimage qml-shadereffect-layereffect.png
    \row
      \li \snippet qml/layerwitheffect.qml 1
    \endtable

    It is also possible to combine multiple layered items:

    \table
    \row
      \li \inlineimage qml-shadereffect-opacitymask.png
    \row
      \li \snippet qml/opacitymask.qml 1
    \endtable

    \section1 Other notes

    By default, the ShaderEffect consists of four vertices, one for each
    corner. For non-linear vertex transformations, like page curl, you can
    specify a fine grid of vertices by specifying a \l mesh resolution.

    The \l {Qt Graphical Effects} module contains several ready-made effects
    for using with Qt Quick applications.

    \sa {Item Layers}
*/

class QQuickShaderEffectPrivate : public QQuickItemPrivate
{
    Q_DECLARE_PUBLIC(QQuickShaderEffect)

public:
    void updatePolish() override;
};

QSGContextFactoryInterface::Flags qsg_backend_flags();

QQuickShaderEffect::QQuickShaderEffect(QQuickItem *parent)
    : QQuickItem(*new QQuickShaderEffectPrivate, parent),
#ifndef QT_NO_OPENGL
      m_glImpl(nullptr),
#endif
      m_impl(nullptr)
{
    setFlag(QQuickItem::ItemHasContents);

#ifndef QT_NO_OPENGL
    if (!qsg_backend_flags().testFlag(QSGContextFactoryInterface::SupportsShaderEffectNode))
        m_glImpl = new QQuickOpenGLShaderEffect(this, this);

    if (!m_glImpl)
#endif
        m_impl = new QQuickGenericShaderEffect(this, this);
}

/*!
    \qmlproperty string QtQuick::ShaderEffect::fragmentShader

    This property holds the fragment (pixel) shader's source code or a
    reference to the pre-compiled bytecode. Some APIs, like OpenGL, always
    support runtime compilation and therefore the traditional Qt Quick way of
    inlining shader source strings is functional. Qt Quick backends for other
    APIs may however limit support to pre-compiled bytecode like SPIR-V or D3D
    shader bytecode. There the string is simply a filename, which may be a file
    in the filesystem or bundled with the executable via Qt's resource system.

    With GLSL the default shader expects the texture coordinate to be passed
    from the vertex shader as \c{varying highp vec2 qt_TexCoord0}, and it
    samples from a sampler2D named \c source. With HLSL the texture is named
    \c source, while the vertex shader is expected to provide
    \c{float2 coord : TEXCOORD0} in its output in addition to
    \c{float4 position : SV_POSITION} (names can differ since linking is done
    based on the semantics).

    \sa vertexShader, GraphicsInfo
*/

QByteArray QQuickShaderEffect::fragmentShader() const
{
#ifndef QT_NO_OPENGL
    if (m_glImpl)
        return m_glImpl->fragmentShader();
#endif
    return m_impl->fragmentShader();
}

void QQuickShaderEffect::setFragmentShader(const QByteArray &code)
{
#ifndef QT_NO_OPENGL
    if (m_glImpl) {
        m_glImpl->setFragmentShader(code);
        return;
    }
#endif
    m_impl->setFragmentShader(code);
}

/*!
    \qmlproperty string QtQuick::ShaderEffect::vertexShader

    This property holds the vertex shader's source code or a reference to the
    pre-compiled bytecode. Some APIs, like OpenGL, always support runtime
    compilation and therefore the traditional Qt Quick way of inlining shader
    source strings is functional. Qt Quick backends for other APIs may however
    limit support to pre-compiled bytecode like SPIR-V or D3D shader bytecode.
    There the string is simply a filename, which may be a file in the
    filesystem or bundled with the executable via Qt's resource system.

    With GLSL the default shader passes the texture coordinate along to the
    fragment shader as \c{varying highp vec2 qt_TexCoord0}. With HLSL it is
    enough to use the standard \c TEXCOORD0 semantic, for example
    \c{float2 coord : TEXCOORD0}.

    \sa fragmentShader, GraphicsInfo
*/

QByteArray QQuickShaderEffect::vertexShader() const
{
#ifndef QT_NO_OPENGL
    if (m_glImpl)
        return m_glImpl->vertexShader();
#endif
    return m_impl->vertexShader();
}

void QQuickShaderEffect::setVertexShader(const QByteArray &code)
{
#ifndef QT_NO_OPENGL
    if (m_glImpl) {
        m_glImpl->setVertexShader(code);
        return;
    }
#endif
    m_impl->setVertexShader(code);
}

/*!
    \qmlproperty bool QtQuick::ShaderEffect::blending

    If this property is true, the output from the \l fragmentShader is blended
    with the background using source-over blend mode. If false, the background
    is disregarded. Blending decreases the performance, so you should set this
    property to false when blending is not needed. The default value is true.
*/

bool QQuickShaderEffect::blending() const
{
#ifndef QT_NO_OPENGL
    if (m_glImpl)
        return m_glImpl->blending();
#endif
    return m_impl->blending();
}

void QQuickShaderEffect::setBlending(bool enable)
{
#ifndef QT_NO_OPENGL
    if (m_glImpl) {
        m_glImpl->setBlending(enable);
        return;
    }
#endif
    m_impl->setBlending(enable);
}

/*!
    \qmlproperty variant QtQuick::ShaderEffect::mesh

    This property defines the mesh used to draw the ShaderEffect. It can hold
    any \l GridMesh object.
    If a size value is assigned to this property, the ShaderEffect implicitly
    uses a \l GridMesh with the value as
    \l{GridMesh::resolution}{mesh resolution}. By default, this property is
    the size 1x1.

    \sa GridMesh
*/

QVariant QQuickShaderEffect::mesh() const
{
#ifndef QT_NO_OPENGL
    if (m_glImpl)
        return m_glImpl->mesh();
#endif
    return m_impl->mesh();
}

void QQuickShaderEffect::setMesh(const QVariant &mesh)
{
#ifndef QT_NO_OPENGL
    if (m_glImpl) {
        m_glImpl->setMesh(mesh);
        return;
    }
#endif
    m_impl->setMesh(mesh);
}

/*!
    \qmlproperty enumeration QtQuick::ShaderEffect::cullMode

    This property defines which sides of the item should be visible.

    \list
    \li ShaderEffect.NoCulling - Both sides are visible
    \li ShaderEffect.BackFaceCulling - only front side is visible
    \li ShaderEffect.FrontFaceCulling - only back side is visible
    \endlist

    The default is NoCulling.
*/

QQuickShaderEffect::CullMode QQuickShaderEffect::cullMode() const
{
#ifndef QT_NO_OPENGL
    if (m_glImpl)
        return m_glImpl->cullMode();
#endif
    return m_impl->cullMode();
}

void QQuickShaderEffect::setCullMode(CullMode face)
{
#ifndef QT_NO_OPENGL
    if (m_glImpl) {
        m_glImpl->setCullMode(face);
        return;
    }
#endif
    return m_impl->setCullMode(face);
}

/*!
    \qmlproperty bool QtQuick::ShaderEffect::supportsAtlasTextures

    Set this property true to confirm that your shader code doesn't rely on
    qt_MultiTexCoord0 ranging from (0,0) to (1,1) relative to the mesh.
    In this case the range of qt_MultiTexCoord0 will rather be based on the position
    of the texture within the atlas. This property currently has no effect if there
    is less, or more, than one sampler uniform used as input to your shader.

    This differs from providing qt_SubRect_<name> uniforms in that the latter allows
    drawing one or more textures from the atlas in a single ShaderEffect item, while
    supportsAtlasTextures allows multiple instances of a ShaderEffect component using
    a different source image from the atlas to be batched in a single draw.
    Both prevent a texture from being copied out of the atlas when referenced by a ShaderEffect.

    The default value is false.

    \since 5.4
    \since QtQuick 2.4
*/

bool QQuickShaderEffect::supportsAtlasTextures() const
{
#ifndef QT_NO_OPENGL
    if (m_glImpl)
        return m_glImpl->supportsAtlasTextures();
#endif
    return m_impl->supportsAtlasTextures();
}

void QQuickShaderEffect::setSupportsAtlasTextures(bool supports)
{
#ifndef QT_NO_OPENGL
    if (m_glImpl) {
        m_glImpl->setSupportsAtlasTextures(supports);
        return;
    }
#endif
    m_impl->setSupportsAtlasTextures(supports);
}

/*!
    \qmlproperty enumeration QtQuick::ShaderEffect::status

    This property tells the current status of the OpenGL shader program.

    \list
    \li ShaderEffect.Compiled - the shader program was successfully compiled and linked.
    \li ShaderEffect.Uncompiled - the shader program has not yet been compiled.
    \li ShaderEffect.Error - the shader program failed to compile or link.
    \endlist

    When setting the fragment or vertex shader source code, the status will
    become Uncompiled. The first time the ShaderEffect is rendered with new
    shader source code, the shaders are compiled and linked, and the status is
    updated to Compiled or Error.

    When runtime compilation is not in use and the shader properties refer to
    files with bytecode, the status is always Compiled. The contents of the
    shader is not examined (apart from basic reflection to discover vertex
    input elements and constant buffer data) until later in the rendering
    pipeline so potential errors (like layout or root signature mismatches)
    will only be detected at a later point.

    \sa log
*/

/*!
    \qmlproperty string QtQuick::ShaderEffect::log

    This property holds a log of warnings and errors from the latest attempt at
    compiling and linking the OpenGL shader program. It is updated at the same
    time \l status is set to Compiled or Error.

    \sa status
*/

QString QQuickShaderEffect::log() const
{
#ifndef QT_NO_OPENGL
    if (m_glImpl)
        return m_glImpl->log();
#endif
    return m_impl->log();
}

QQuickShaderEffect::Status QQuickShaderEffect::status() const
{
#ifndef QT_NO_OPENGL
    if (m_glImpl)
        return m_glImpl->status();
#endif
    return m_impl->status();
}

bool QQuickShaderEffect::event(QEvent *e)
{
#ifndef QT_NO_OPENGL
    if (m_glImpl) {
        m_glImpl->handleEvent(e);
        return QQuickItem::event(e);
    }
#endif
    m_impl->handleEvent(e);
    return QQuickItem::event(e);
}

void QQuickShaderEffect::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
#ifndef QT_NO_OPENGL
    if (m_glImpl) {
        m_glImpl->handleGeometryChanged(newGeometry, oldGeometry);
        QQuickItem::geometryChanged(newGeometry, oldGeometry);
        return;
    }
#endif
    m_impl->handleGeometryChanged(newGeometry, oldGeometry);
    QQuickItem::geometryChanged(newGeometry, oldGeometry);
}

QSGNode *QQuickShaderEffect::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *updatePaintNodeData)
{
#ifndef QT_NO_OPENGL
    if (m_glImpl)
        return m_glImpl->handleUpdatePaintNode(oldNode, updatePaintNodeData);
#endif
    return m_impl->handleUpdatePaintNode(oldNode, updatePaintNodeData);
}

void QQuickShaderEffect::componentComplete()
{
#ifndef QT_NO_OPENGL
    if (m_glImpl) {
        m_glImpl->maybeUpdateShaders();
        QQuickItem::componentComplete();
        return;
    }
#endif
    m_impl->maybeUpdateShaders();
    QQuickItem::componentComplete();
}

void QQuickShaderEffect::itemChange(ItemChange change, const ItemChangeData &value)
{
#ifndef QT_NO_OPENGL
    if (m_glImpl) {
        m_glImpl->handleItemChange(change, value);
        QQuickItem::itemChange(change, value);
        return;
    }
#endif
    m_impl->handleItemChange(change, value);
    QQuickItem::itemChange(change, value);
}

bool QQuickShaderEffect::isComponentComplete() const
{
    return QQuickItem::isComponentComplete();
}

QString QQuickShaderEffect::parseLog() // for OpenGL-based autotests
{
#ifndef QT_NO_OPENGL
    if (m_glImpl)
        return m_glImpl->parseLog();
#endif
    return m_impl->parseLog();
}

void QQuickShaderEffectPrivate::updatePolish()
{
    Q_Q(QQuickShaderEffect);
#ifndef QT_NO_OPENGL
    if (q->m_glImpl) {
        q->m_glImpl->maybeUpdateShaders();
        return;
    }
#endif
    q->m_impl->maybeUpdateShaders();
}

QT_END_NAMESPACE
