/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Quick 3D.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QSSGCUSTOMMATERIAL_H
#define QSSGCUSTOMMATERIAL_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtQuick3D/private/qquick3dmaterial_p.h>
#include <QtCore/qvector.h>

#include <QtQuick3DRender/private/qssgrenderbasetypes_p.h>

#include <QtQuick3DRuntimeRender/private/qssgrenderdynamicobjectsystemcommands_p.h>

QT_BEGIN_NAMESPACE

class QQuick3DCustomMaterialShader;

class Q_QUICK3D_EXPORT QQuick3DCustomMaterialTextureInput : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQuick3DTexture *texture READ texture WRITE setTexture)
    Q_PROPERTY(bool enabled MEMBER enabled)

public:
    QQuick3DCustomMaterialTextureInput() = default;
    virtual ~QQuick3DCustomMaterialTextureInput() = default;
    QQuick3DTexture *m_texture = nullptr;
    bool enabled = true;
    QQuick3DTexture *texture() const
    {
        return m_texture;
    }

public Q_SLOTS:
    void setTexture(QQuick3DTexture *texture)
    {
        if (m_texture == texture)
            return;

        QObject *p = parent();
        while (p != nullptr) {
            if (QQuick3DMaterial *mat = qobject_cast<QQuick3DMaterial *>(p)) {
                mat->setDynamicTextureMap(texture);
                break;
            }
        }

        m_texture = texture;
        Q_EMIT textureDirty(this);
    }

Q_SIGNALS:
    void textureDirty(QQuick3DCustomMaterialTextureInput *texture);
};

class Q_QUICK3D_EXPORT QQuick3DCustomMaterialBuffer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(TextureFormat format READ format WRITE setFormat)
    Q_PROPERTY(TextureFilterOperation textureFilterOperation READ textureFilterOperation WRITE setTextureFilterOperation)
    Q_PROPERTY(TextureCoordOperation textureCoordOperation READ textureCoordOperation WRITE setTextureCoordOperation)
    Q_PROPERTY(float sizeMultiplier MEMBER sizeMultiplier)
    Q_PROPERTY(AllocateBufferFlagValues bufferFlags READ bufferFlags WRITE setBufferFlags)
    Q_PROPERTY(QByteArray name MEMBER name)
public:
    QQuick3DCustomMaterialBuffer() = default;
    ~QQuick3DCustomMaterialBuffer() override = default;

    enum class TextureFilterOperation
    {
        Unknown = 0,
        Nearest,
        Linear
    };
    Q_ENUM(TextureFilterOperation)

    enum class TextureCoordOperation
    {
        Unknown = 0,
        ClampToEdge,
        MirroredRepeat,
        Repeat
    };
    Q_ENUM(TextureCoordOperation)

    enum class AllocateBufferFlagValues
    {
        None = 0,
        SceneLifetime = 1,
    };
    Q_ENUM(AllocateBufferFlagValues)

    enum class TextureFormat {
        Unknown = 0,
        R8,
        R16,
        R16F,
        R32I,
        R32UI,
        R32F,
        RG8,
        RGBA8,
        RGB8,
        SRGB8,
        SRGB8A8,
        RGB565,
        RGBA16F,
        RG16F,
        RG32F,
        RGB32F,
        RGBA32F,
        R11G11B10,
        RGB9E5,
        Depth16,
        Depth24,
        Depth32,
        Depth24Stencil8
    };
    Q_ENUM(TextureFormat)

    dynamic::QSSGAllocateBuffer command {};
    TextureFilterOperation textureFilterOperation() const { return TextureFilterOperation(command.m_filterOp); }
    void setTextureFilterOperation(TextureFilterOperation op) { command.m_filterOp = QSSGRenderTextureMagnifyingOp(op); }

    TextureCoordOperation textureCoordOperation() const { return TextureCoordOperation(command.m_texCoordOp); }
    void setTextureCoordOperation(TextureCoordOperation texCoordOp) { command.m_texCoordOp = QSSGRenderTextureCoordOp(texCoordOp); }
    float &sizeMultiplier = command.m_sizeMultiplier;
    dynamic::QSSGCommand *getCommand() { return &command; }

    TextureFormat format() const;
    void setFormat(TextureFormat format);

    AllocateBufferFlagValues bufferFlags() const { return AllocateBufferFlagValues(int(command.m_bufferFlags)); }
    void setBufferFlags(AllocateBufferFlagValues flag) { command.m_bufferFlags = quint32(flag);}

    QByteArray &name = command.m_name;

    static TextureFormat mapRenderTextureFormat(QSSGRenderTextureFormat::Format fmt);
    static QSSGRenderTextureFormat::Format mapTextureFormat(TextureFormat fmt);
};

class Q_QUICK3D_EXPORT QQuick3DCustomMaterialRenderCommand : public QObject
{
    Q_OBJECT
public:
    QQuick3DCustomMaterialRenderCommand() = default;
    ~QQuick3DCustomMaterialRenderCommand() override = default;
    virtual dynamic::QSSGCommand *getCommand() { Q_ASSERT(0); return nullptr; }
    virtual int bufferCount() const { return 0; }
    virtual QQuick3DCustomMaterialBuffer *bufferAt(int idx) const { Q_UNUSED(idx) return nullptr; }
};

class Q_QUICK3D_EXPORT QQuick3DCustomMaterialBufferInput : public QQuick3DCustomMaterialRenderCommand
{
    Q_OBJECT
    Q_PROPERTY(QQuick3DCustomMaterialBuffer *buffer READ buffer WRITE setBuffer)
    Q_PROPERTY(QByteArray param MEMBER param)
public:
    QQuick3DCustomMaterialBufferInput() = default;
    ~QQuick3DCustomMaterialBufferInput() override = default;
    dynamic::QSSGApplyBufferValue command { QByteArray(), QByteArray() };
    QByteArray &param = command.m_paramName;
    dynamic::QSSGCommand *getCommand() override { return &command; }

    int bufferCount() const override { return (m_buffer != nullptr) ? 1 : 0; }
    QQuick3DCustomMaterialBuffer *bufferAt(int idx) const override
    {
        Q_ASSERT(idx < 1 && idx >= 0);
        return (m_buffer && idx == 0) ? m_buffer : nullptr;
    }

    QQuick3DCustomMaterialBuffer *buffer() const { return m_buffer; }
    void setBuffer(QQuick3DCustomMaterialBuffer *buffer) {
        if (m_buffer == buffer)
            return;

        if (buffer) {
            Q_ASSERT(!buffer->name.isEmpty());
            command.m_bufferName = buffer->name;
        }
        m_buffer = buffer;
    }

    QQuick3DCustomMaterialBuffer *m_buffer = nullptr;

};

class Q_QUICK3D_EXPORT QQuick3DCustomMaterialBufferBlit : public QQuick3DCustomMaterialRenderCommand
{
    Q_OBJECT
    Q_PROPERTY(QQuick3DCustomMaterialBuffer *source READ source WRITE setSource)
    Q_PROPERTY(QQuick3DCustomMaterialBuffer *destination READ destination WRITE setDestination)
public:
    QQuick3DCustomMaterialBufferBlit() = default;
    ~QQuick3DCustomMaterialBufferBlit() override = default;
    dynamic::QSSGApplyBlitFramebuffer command { QByteArray(), QByteArray() };
    dynamic::QSSGCommand *getCommand() override { return &command; }

    int bufferCount() const override {
        if (m_source != nullptr && m_destination != nullptr)
            return 2;
        if (m_source || m_destination)
            return 1;

        return 0;
    }

    QQuick3DCustomMaterialBuffer *bufferAt(int idx) const override
    {
        Q_ASSERT(idx < 2 && idx >= 0);
        if (idx == 0)
            return m_source ? m_source : m_destination;
        if (idx == 1 && m_destination)
            return m_destination;

        return nullptr;
    }

    QQuick3DCustomMaterialBuffer *source() const { return m_source; }
    void setSource(QQuick3DCustomMaterialBuffer *src)
    {
        if (src == m_source)
            return;

        if (src) {
            Q_ASSERT(!src->name.isEmpty());
            command.m_sourceBufferName = src->name;
        }
        m_source = src;
    }

    QQuick3DCustomMaterialBuffer *destination() const { return m_destination; }
    void setDestination(QQuick3DCustomMaterialBuffer *dest)
    {
        if (dest == m_destination)
            return;

        if (dest) {
            Q_ASSERT(!dest->name.isEmpty());
            command.m_destBufferName = dest->name;
        }
        m_destination = dest;
    }

    QQuick3DCustomMaterialBuffer *m_source = nullptr;
    QQuick3DCustomMaterialBuffer *m_destination = nullptr;
};

class Q_QUICK3D_EXPORT QQuick3DCustomMaterialBlending : public QQuick3DCustomMaterialRenderCommand
{
    Q_OBJECT
    Q_PROPERTY(SrcBlending srcBlending READ srcBlending WRITE setSrcBlending)
    Q_PROPERTY(DestBlending destBlending READ destBlending WRITE setDestBlending)

public:
    enum class SrcBlending
    {
        Unknown = 0,
        Zero,
        One,
        SrcColor,
        OneMinusSrcColor,
        DstColor,
        OneMinusDstColor,
        SrcAlpha,
        OneMinusSrcAlpha,
        DstAlpha,
        OneMinusDstAlpha,
        ConstantColor,
        OneMinusConstantColor,
        ConstantAlpha,
        OneMinusConstantAlpha,
        SrcAlphaSaturate
    };
    Q_ENUM(SrcBlending)

    enum class DestBlending
    {
        Unknown = 0,
        Zero,
        One,
        SrcColor,
        OneMinusSrcColor,
        DstColor,
        OneMinusDstColor,
        SrcAlpha,
        OneMinusSrcAlpha,
        DstAlpha,
        OneMinusDstAlpha,
        ConstantColor,
        OneMinusConstantColor,
        ConstantAlpha,
        OneMinusConstantAlpha
    };
    Q_ENUM(DestBlending)

    QQuick3DCustomMaterialBlending() = default;
    ~QQuick3DCustomMaterialBlending() override = default;
    dynamic::QSSGApplyBlending command { QSSGRenderSrcBlendFunc::Unknown, QSSGRenderDstBlendFunc::Unknown };
    DestBlending destBlending() const
    {
        return DestBlending(command.m_dstBlendFunc);
    }
    SrcBlending srcBlending() const
    {
        return SrcBlending(command.m_srcBlendFunc);
    }

    dynamic::QSSGCommand *getCommand() override { return &command; }

public Q_SLOTS:
    void setDestBlending(DestBlending destBlending)
    {
        command.m_dstBlendFunc = QSSGRenderDstBlendFunc(destBlending);
    }
    void setSrcBlending(SrcBlending srcBlending)
    {
        command.m_srcBlendFunc= QSSGRenderSrcBlendFunc(srcBlending);
    }
};

class Q_QUICK3D_EXPORT QQuick3DCustomMaterialRenderState : public QQuick3DCustomMaterialRenderCommand
{
    Q_OBJECT
    Q_PROPERTY(RenderState renderState READ renderState WRITE setRenderState)
    Q_PROPERTY(bool enabled MEMBER enabled)

public:
    enum class RenderState
    {
        Unknown = 0,
        Blend,
        DepthTest = 3,
        StencilTest,
        ScissorTest,
        DepthWrite,
        Multisample
    };
    Q_ENUM(RenderState)

    QQuick3DCustomMaterialRenderState() = default;
    ~QQuick3DCustomMaterialRenderState() override = default;
    dynamic::QSSGApplyRenderState command { QSSGRenderState::Unknown, false };
    bool &enabled = command.m_enabled;
    RenderState renderState() const
    {
        return RenderState(command.m_renderState);
    }

    dynamic::QSSGCommand *getCommand() override { return &command; }
public Q_SLOTS:
    void setRenderState(RenderState renderState)
    {
        command.m_renderState = QSSGRenderState(renderState);
    }
};

class Q_QUICK3D_EXPORT QQuick3DCustomMaterialRenderPass : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQmlListProperty<QQuick3DCustomMaterialRenderCommand> commands READ commands)
    Q_PROPERTY(QQuick3DCustomMaterialBuffer *output MEMBER outputBuffer)
    Q_PROPERTY(QQmlListProperty<QQuick3DCustomMaterialShader> shaders READ shaders)
public:
    QQuick3DCustomMaterialRenderPass() = default;
    ~QQuick3DCustomMaterialRenderPass() override = default;

    static void qmlAppendCommand(QQmlListProperty<QQuick3DCustomMaterialRenderCommand> *list, QQuick3DCustomMaterialRenderCommand *command);
    static QQuick3DCustomMaterialRenderCommand *qmlCommandAt(QQmlListProperty<QQuick3DCustomMaterialRenderCommand> *list, int index);
    static int qmlCommandCount(QQmlListProperty<QQuick3DCustomMaterialRenderCommand> *list);

    static void qmlAppendShader(QQmlListProperty<QQuick3DCustomMaterialShader> *list, QQuick3DCustomMaterialShader *shader);
    static QQuick3DCustomMaterialShader *qmlShaderAt(QQmlListProperty<QQuick3DCustomMaterialShader> *list, int index);
    static int qmlShaderCount(QQmlListProperty<QQuick3DCustomMaterialShader> *list);
    static void qmlShaderClear(QQmlListProperty<QQuick3DCustomMaterialShader> *list);

    QQmlListProperty<QQuick3DCustomMaterialRenderCommand> commands();
    QVector<QQuick3DCustomMaterialRenderCommand *> m_commands;
    QQuick3DCustomMaterialBuffer *outputBuffer = nullptr;
    QQmlListProperty<QQuick3DCustomMaterialShader> shaders();
    QVarLengthArray<QQuick3DCustomMaterialShader *, 5> m_shaders { nullptr, nullptr, nullptr, nullptr, nullptr };
};

class Q_QUICK3D_EXPORT QQuick3DCustomMaterialShaderInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QByteArray version MEMBER version)
    Q_PROPERTY(QByteArray type MEMBER type)
    Q_PROPERTY(qint32 shaderKey MEMBER shaderKey)
public:
    QQuick3DCustomMaterialShaderInfo() = default;
    ~QQuick3DCustomMaterialShaderInfo() override = default;
    QByteArray version;
    QByteArray type; // I.e., GLSL
    QByteArray shaderPrefix = QByteArrayLiteral("#include \"customMaterial.glsllib\"\n");

    enum class MaterialShaderKeyValues
    {
        Diffuse = 1 << 0,
        Specular = 1 << 1,
        Cutout = 1 << 2,
        Refraction = 1 << 3,
        Transparent = 1 << 4,
        Displace = 1 << 5,
        Transmissive = 1 << 6,
        Glossy = Diffuse | Specular,
    };
    Q_ENUM(MaterialShaderKeyValues)
    Q_DECLARE_FLAGS(MaterialShaderKeyFlags, MaterialShaderKeyValues)

    qint32 shaderKey {0};
    bool isValid() const { return !(version.isEmpty() && type.isEmpty() && shaderPrefix.isEmpty()); }
};

class Q_QUICK3D_EXPORT QQuick3DCustomMaterialShader : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QByteArray shader MEMBER shader)
    Q_PROPERTY(Stage stage MEMBER stage)
public:
    QQuick3DCustomMaterialShader() = default;
    virtual ~QQuick3DCustomMaterialShader() = default;
    enum class Stage : quint8
    {
        Shared,
        Vertex,
        Fragment,
        Geometry,
        Compute
    };
    Q_ENUM(Stage)

    QByteArray shader;
    Stage stage;
};

class Q_QUICK3D_EXPORT QQuick3DCustomMaterial : public QQuick3DMaterial
{
    Q_OBJECT
    Q_PROPERTY(bool hasTransparency READ hasTransparency WRITE setHasTransparency NOTIFY hasTransparencyChanged)
    Q_PROPERTY(bool hasRefraction READ hasRefraction WRITE setHasRefraction NOTIFY hasRefractionChanged)
    Q_PROPERTY(bool alwaysDirty READ alwaysDirty WRITE setAlwaysDirty NOTIFY alwaysDirtyChanged)
    Q_PROPERTY(QQuick3DCustomMaterialShaderInfo *shaderInfo READ shaderInfo WRITE setShaderInfo)
    Q_PROPERTY(QQmlListProperty<QQuick3DCustomMaterialRenderPass> passes READ passes)

public:
    QQuick3DCustomMaterial();
    ~QQuick3DCustomMaterial() override;

    QQuick3DObject::Type type() const override;

    bool hasTransparency() const;
    bool hasRefraction() const;
    bool alwaysDirty() const;

    QQuick3DCustomMaterialShaderInfo *shaderInfo() const;
    QQmlListProperty<QQuick3DCustomMaterialRenderPass> passes();

public Q_SLOTS:
    void setHasTransparency(bool hasTransparency);
    void setHasRefraction(bool hasRefraction);
    void setShaderInfo(QQuick3DCustomMaterialShaderInfo *shaderInfo);
    void setAlwaysDirty(bool alwaysDirty);

Q_SIGNALS:
    void hasTransparencyChanged(bool hasTransparency);
    void hasRefractionChanged(bool hasRefraction);
    void alwaysDirtyChanged(bool alwaysDirty);

protected:
    QSSGRenderGraphObject *updateSpatialNode(QSSGRenderGraphObject *node) override;
    void markAllDirty() override;

private Q_SLOTS:
    void onPropertyDirty();
    void onTextureDirty(QQuick3DCustomMaterialTextureInput *texture);

private:
    enum Dirty {
        TextureDirty = 0x1,
        PropertyDirty = 0x2
    };

    // Passes
    static void qmlAppendPass(QQmlListProperty<QQuick3DCustomMaterialRenderPass> *list, QQuick3DCustomMaterialRenderPass *pass);
    static QQuick3DCustomMaterialRenderPass *qmlPassAt(QQmlListProperty<QQuick3DCustomMaterialRenderPass> *list, int index);
    static int qmlPassCount(QQmlListProperty<QQuick3DCustomMaterialRenderPass> *list);

    void markDirty(QQuick3DCustomMaterial::Dirty type)
    {
        if (!(m_dirtyAttributes & quint32(type))) {
            m_dirtyAttributes |= quint32(type);
            update();
        }
    }

    quint32 m_dirtyAttributes = 0xffffffff;
    bool m_hasTransparency = false;
    bool m_hasRefraction = false;
    QQuick3DCustomMaterialShaderInfo *m_shaderInfo = nullptr;
    QVector<QQuick3DCustomMaterialRenderPass *> m_passes;
    bool m_alwaysDirty = false;
};

QT_END_NAMESPACE

#endif // QSSGCUSTOMMATERIAL_H
