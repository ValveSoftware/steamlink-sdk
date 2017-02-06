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

#ifndef QSGD3D12ENGINE_P_H
#define QSGD3D12ENGINE_P_H

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

#include <QWindow>
#include <QImage>
#include <QVector4D>
#include <qsggeometry.h>
#include <qsgrendererinterface.h>
#include <qt_windows.h>

QT_BEGIN_NAMESPACE

// No D3D or COM headers must be pulled in here. All that has to be isolated
// to engine_p_p.h and engine.cpp.

class QSGD3D12EnginePrivate;

// Shader bytecode and other strings are expected to be static so that a
// different pointer means a different shader.

enum QSGD3D12Format {
    FmtUnknown = 0,

    FmtFloat4 = 2,    // DXGI_FORMAT_R32G32B32A32_FLOAT
    FmtFloat3 = 6,    // DXGI_FORMAT_R32G32B32_FLOAT
    FmtFloat2 = 16,   // DXGI_FORMAT_R32G32_FLOAT
    FmtFloat = 41,    // DXGI_FORMAT_R32_FLOAT

    // glVertexAttribPointer with GL_UNSIGNED_BYTE and normalized == true maps to the UNORM formats below
    FmtUNormByte4 = 28,   // DXGI_FORMAT_R8G8B8A8_UNORM
    FmtUNormByte2 = 49,   // DXGI_FORMAT_R8G8_UNORM
    FmtUNormByte = 61,    // DXGI_FORMAT_R8_UNORM

    // Index data types
    FmtUnsignedShort = 57,  // DXGI_FORMAT_R16_UINT
    FmtUnsignedInt = 42     // DXGI_FORMAT_R32_UINT
};

struct QSGD3D12InputElement
{
    const char *semanticName = nullptr;
    int semanticIndex = 0;
    QSGD3D12Format format = FmtFloat4;
    quint32 slot = 0;
    quint32 offset = 0;

    bool operator==(const QSGD3D12InputElement &other) const {
        return semanticName == other.semanticName && semanticIndex == other.semanticIndex
                && format == other.format && slot == other.slot && offset == other.offset;
    }
};

inline uint qHash(const QSGD3D12InputElement &key, uint seed = 0)
{
    return qHash(key.semanticName, seed) + key.semanticIndex + key.format + key.offset;
}

struct QSGD3D12TextureView
{
    enum Filter {
        FilterNearest = 0,
        FilterLinear = 0x15,
        FilterMinMagNearestMipLinear = 0x1,
        FilterMinMagLinearMipNearest = 0x14
    };

    enum AddressMode {
        AddressWrap = 1,
        AddressClamp = 3
    };

    Filter filter = FilterLinear;
    AddressMode addressModeHoriz = AddressClamp;
    AddressMode addressModeVert = AddressClamp;

    bool operator==(const QSGD3D12TextureView &other) const {
        return filter == other.filter
                && addressModeHoriz == other.addressModeHoriz
                && addressModeVert == other.addressModeVert;
    }
};

inline uint qHash(const QSGD3D12TextureView &key, uint seed = 0)
{
    Q_UNUSED(seed);
    return key.filter + key.addressModeHoriz + key.addressModeVert;
}

const int QSGD3D12_MAX_TEXTURE_VIEWS = 8;

struct QSGD3D12RootSignature
{
    int textureViewCount = 0;
    QSGD3D12TextureView textureViews[QSGD3D12_MAX_TEXTURE_VIEWS];

    bool operator==(const QSGD3D12RootSignature &other) const {
        if (textureViewCount != other.textureViewCount)
            return false;
        for (int i = 0; i < textureViewCount; ++i)
            if (!(textureViews[i] == other.textureViews[i]))
                return false;
        return true;
    }
};

inline uint qHash(const QSGD3D12RootSignature &key, uint seed = 0)
{
    return key.textureViewCount + (key.textureViewCount > 0 ? qHash(key.textureViews[0], seed) : 0);
}

// Shader bytecode blobs and root signature-related data.
struct QSGD3D12ShaderState
{
    const quint8 *vs = nullptr;
    quint32 vsSize = 0;
    const quint8 *ps = nullptr;
    quint32 psSize = 0;

    QSGD3D12RootSignature rootSig;

    bool operator==(const QSGD3D12ShaderState &other) const {
        return vs == other.vs && vsSize == other.vsSize
                && ps == other.ps && psSize == other.psSize
                && rootSig == other.rootSig;
    }
};

inline uint qHash(const QSGD3D12ShaderState &key, uint seed = 0)
{
    return qHash(key.vs, seed) + key.vsSize + qHash(key.ps, seed) + key.psSize + qHash(key.rootSig, seed);
}

const int QSGD3D12_MAX_INPUT_ELEMENTS = 8;

struct QSGD3D12PipelineState
{
    enum CullMode {
        CullNone = 1,
        CullFront,
        CullBack
    };

    enum CompareFunc {
        CompareNever = 1,
        CompareLess,
        CompareEqual,
        CompareLessEqual,
        CompareGreater,
        CompareNotEqual,
        CompareGreaterEqual,
        CompareAlways
    };

    enum StencilOp {
        StencilKeep = 1,
        StencilZero,
        StencilReplace,
        StencilIncrSat,
        StencilDecrSat,
        StencilInvert,
        StencilIncr,
        StencilDescr
    };

    enum TopologyType {
        TopologyTypePoint = 1,
        TopologyTypeLine,
        TopologyTypeTriangle
    };

    enum BlendType {
        BlendNone,
        BlendPremul, // == GL_ONE, GL_ONE_MINUS_SRC_ALPHA
        BlendColor // == GL_CONSTANT_COLOR, GL_ONE_MINUS_SRC_COLOR
    };

    QSGD3D12ShaderState shaders;

    int inputElementCount = 0;
    QSGD3D12InputElement inputElements[QSGD3D12_MAX_INPUT_ELEMENTS];

    CullMode cullMode = CullNone;
    bool frontCCW = true;
    bool colorWrite = true;
    BlendType blend = BlendNone;
    bool depthEnable = true;
    CompareFunc depthFunc = CompareLess;
    bool depthWrite = true;
    bool stencilEnable = false;
    CompareFunc stencilFunc = CompareEqual;
    StencilOp stencilFailOp = StencilKeep;
    StencilOp stencilDepthFailOp = StencilKeep;
    StencilOp stencilPassOp = StencilKeep;
    TopologyType topologyType = TopologyTypeTriangle;

    bool operator==(const QSGD3D12PipelineState &other) const {
        bool eq = shaders == other.shaders
                && inputElementCount == other.inputElementCount
                && cullMode == other.cullMode
                && frontCCW == other.frontCCW
                && colorWrite == other.colorWrite
                && blend == other.blend
                && depthEnable == other.depthEnable
                && (!depthEnable || depthFunc == other.depthFunc)
                && depthWrite == other.depthWrite
                && stencilEnable == other.stencilEnable
                && (!stencilEnable || stencilFunc == other.stencilFunc)
                && (!stencilEnable || stencilFailOp == other.stencilFailOp)
                && (!stencilEnable || stencilDepthFailOp == other.stencilDepthFailOp)
                && (!stencilEnable || stencilPassOp == other.stencilPassOp)
                && topologyType == other.topologyType;
        if (eq) {
            for (int i = 0; i < inputElementCount; ++i) {
                if (!(inputElements[i] == other.inputElements[i])) {
                    eq = false;
                    break;
                }
            }
        }
        return eq;
    }
};

inline uint qHash(const QSGD3D12PipelineState &key, uint seed = 0)
{
    return qHash(key.shaders, seed) + key.inputElementCount
            + key.cullMode + key.frontCCW
            + key.colorWrite + key.blend
            + key.depthEnable + key.depthWrite
            + key.stencilEnable
            + key.topologyType;
}

class QSGD3D12Engine
{
public:
    QSGD3D12Engine();
    ~QSGD3D12Engine();

    bool attachToWindow(WId window, const QSize &size, float dpr, int surfaceFormatSamples, bool alpha);
    void releaseResources();
    bool hasResources() const;
    void setWindowSize(const QSize &size, float dpr);
    WId window() const;
    QSize windowSize() const;
    float windowDevicePixelRatio() const;
    uint windowSamples() const;

    void beginFrame();
    void endFrame();
    void beginLayer();
    void endLayer();
    void invalidateCachedFrameState();
    void restoreFrameState(bool minimal = false);

    uint genBuffer();
    void releaseBuffer(uint id);
    void resetBuffer(uint id, const quint8 *data, int size);
    void markBufferDirty(uint id, int offset, int size);

    enum ClearFlag {
        ClearDepth = 0x1,
        ClearStencil = 0x2
    };
    Q_DECLARE_FLAGS(ClearFlags, ClearFlag)

    void queueViewport(const QRect &rect);
    void queueScissor(const QRect &rect);
    void queueSetRenderTarget(uint id = 0);
    void queueClearRenderTarget(const QColor &color);
    void queueClearDepthStencil(float depthValue, quint8 stencilValue, ClearFlags which);
    void queueSetBlendFactor(const QVector4D &factor);
    void queueSetStencilRef(quint32 ref);

    void finalizePipeline(const QSGD3D12PipelineState &pipelineState);

    struct DrawParams {
        QSGGeometry::DrawingMode mode = QSGGeometry::DrawTriangles;
        int count = 0;
        uint vertexBuf = 0;
        uint indexBuf = 0;
        uint constantBuf = 0;
        int vboOffset = 0;
        int vboSize = 0;
        int vboStride = 0;
        int cboOffset = 0;
        int startIndexIndex = -1;
        QSGD3D12Format indexFormat = FmtUnsignedShort;
    };

    void queueDraw(const DrawParams &params);

    void present();
    void waitGPU();

    static quint32 alignedConstantBufferSize(quint32 size);
    static QSGD3D12Format toDXGIFormat(QSGGeometry::Type sgtype, int tupleSize = 1, int *size = nullptr);
    static int mipMapLevels(const QSize &size);
    static QSize mipMapAdjustedSourceSize(const QSize &size);

    enum TextureCreateFlag {
        TextureWithAlpha = 0x01,
        TextureWithMipMaps = 0x02,
        TextureAlways32Bit = 0x04
    };
    Q_DECLARE_FLAGS(TextureCreateFlags, TextureCreateFlag)

    enum TextureUploadFlag {
        TextureUploadAlways32Bit = 0x01
    };
    Q_DECLARE_FLAGS(TextureUploadFlags, TextureUploadFlag)

    uint genTexture();
    void createTexture(uint id, const QSize &size, QImage::Format format, TextureCreateFlags flags);
    void queueTextureResize(uint id, const QSize &size);
    void queueTextureUpload(uint id, const QImage &image, const QPoint &dstPos = QPoint(), TextureUploadFlags flags = 0);
    void queueTextureUpload(uint id, const QVector<QImage> &images, const QVector<QPoint> &dstPos, TextureUploadFlags flags = 0);
    void releaseTexture(uint id);
    void useTexture(uint id);

    uint genRenderTarget();
    void createRenderTarget(uint id, const QSize &size, const QVector4D &clearColor, uint samples);
    void releaseRenderTarget(uint id);
    void useRenderTargetAsTexture(uint id);
    uint activeRenderTarget() const;

    QImage executeAndWaitReadbackRenderTarget(uint id = 0);

    void simulateDeviceLoss();

    void *getResource(QQuickWindow *window, QSGRendererInterface::Resource resource) const;

private:
    QSGD3D12EnginePrivate *d;
    Q_DISABLE_COPY(QSGD3D12Engine)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QSGD3D12Engine::ClearFlags)
Q_DECLARE_OPERATORS_FOR_FLAGS(QSGD3D12Engine::TextureCreateFlags)
Q_DECLARE_OPERATORS_FOR_FLAGS(QSGD3D12Engine::TextureUploadFlags)

QT_END_NAMESPACE

#endif // QSGD3D12ENGINE_P_H
