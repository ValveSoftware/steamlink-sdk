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

#ifndef QSGD3D12ENGINE_P_P_H
#define QSGD3D12ENGINE_P_P_H

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

#include "qsgd3d12engine_p.h"
#include <QCache>

#include <d3d12.h>
#include <dxgi1_4.h>
#include <dcomp.h>
#include <wrl/client.h>

using namespace Microsoft::WRL;

// No moc-related features (Q_OBJECT, signals, etc.) can be used here to due
// moc-generated code failing to compile when combined with COM stuff.

// Recommended reading before moving further: https://github.com/Microsoft/DirectXTK/wiki/ComPtr
// Note esp. operator= vs. Attach and operator& vs. GetAddressOf

// ID3D12* is never passed to Qt containers directly. Always use ComPtr and put it into a struct.

QT_BEGIN_NAMESPACE

class QSGD3D12CPUDescriptorHeapManager
{
public:
    void initialize(ID3D12Device *device);

    void releaseResources();

    D3D12_CPU_DESCRIPTOR_HANDLE allocate(D3D12_DESCRIPTOR_HEAP_TYPE type);
    void release(D3D12_CPU_DESCRIPTOR_HANDLE handle, D3D12_DESCRIPTOR_HEAP_TYPE type);
    quint32 handleSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const { return m_handleSizes[type]; }

private:
    ID3D12Device *m_device = nullptr;
    struct Heap {
        D3D12_DESCRIPTOR_HEAP_TYPE type;
        ComPtr<ID3D12DescriptorHeap> heap;
        D3D12_CPU_DESCRIPTOR_HANDLE start;
        quint32 handleSize;
        quint32 freeMap[8];
    };
    QVector<Heap> m_heaps;
    quint32 m_handleSizes[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
};

class QSGD3D12DeviceManager
{
public:
    ID3D12Device *ref();
    void unref();
    void deviceLossDetected();
    IDXGIFactory4 *dxgi();

    struct DeviceLossObserver {
        virtual void deviceLost() = 0;
    };
    void registerDeviceLossObserver(DeviceLossObserver *observer);

private:
    void ensureCreated();

    ComPtr<ID3D12Device> m_device;
    ComPtr<IDXGIFactory4> m_factory;
    QAtomicInt m_ref;
    QVector<DeviceLossObserver *> m_observers;
};

struct QSGD3D12CPUWaitableFence
{
    ~QSGD3D12CPUWaitableFence() {
        if (event)
            CloseHandle(event);
    }
    ComPtr<ID3D12Fence> fence;
    HANDLE event = nullptr;
    QAtomicInt value;
};

class QSGD3D12EnginePrivate : public QSGD3D12DeviceManager::DeviceLossObserver
{
public:
    void initialize(WId w, const QSize &size, float dpr, int surfaceFormatSamples, bool alpha);
    bool isInitialized() const { return initialized; }
    void releaseResources();
    void setWindowSize(const QSize &size, float dpr);
    WId currentWindow() const { return window; }
    QSize currentWindowSize() const { return windowSize; }
    float currentWindowDpr() const { return windowDpr; }
    uint currentWindowSamples() const { return windowSamples; }

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

    void queueViewport(const QRect &rect);
    void queueScissor(const QRect &rect);
    void queueSetRenderTarget(uint id);
    void queueClearRenderTarget(const QColor &color);
    void queueClearDepthStencil(float depthValue, quint8 stencilValue, QSGD3D12Engine::ClearFlags which);
    void queueSetBlendFactor(const QVector4D &factor);
    void queueSetStencilRef(quint32 ref);

    void finalizePipeline(const QSGD3D12PipelineState &pipelineState);

    void queueDraw(const QSGD3D12Engine::DrawParams &params);

    void present();
    void waitGPU();

    uint genTexture();
    void createTexture(uint id, const QSize &size, QImage::Format format, QSGD3D12Engine::TextureCreateFlags flags);
    void queueTextureResize(uint id, const QSize &size);
    void queueTextureUpload(uint id, const QVector<QImage> &images, const QVector<QPoint> &dstPos,
                            QSGD3D12Engine::TextureUploadFlags flags);
    void releaseTexture(uint id);
    void useTexture(uint id);

    uint genRenderTarget();
    void createRenderTarget(uint id, const QSize &size, const QVector4D &clearColor, uint samples);
    void releaseRenderTarget(uint id);
    void useRenderTargetAsTexture(uint id);
    uint activeRenderTarget() const { return currentRenderTarget; }

    QImage executeAndWaitReadbackRenderTarget(uint id);

    void simulateDeviceLoss();

    void *getResource(QSGRendererInterface::Resource resource) const;

    // the device is intentionally hidden here. all resources have to go
    // through the engine and, unlike with GL, cannot just be created in random
    // places due to the need for proper tracking, managing and releasing.
private:
    void ensureDevice();
    void setupDefaultRenderTargets();
    void deviceLost() override;

    bool createCbvSrvUavHeap(int pframeIndex, int descriptorCount);
    void setDescriptorHeaps(bool force = false);
    void ensureGPUDescriptorHeap(int cbvSrvUavDescriptorCount);

    DXGI_SAMPLE_DESC makeSampleDesc(DXGI_FORMAT format, uint samples);
    ID3D12Resource *createColorBuffer(D3D12_CPU_DESCRIPTOR_HANDLE viewHandle, const QSize &size,
                                      const QVector4D &clearColor, uint samples);
    ID3D12Resource *createDepthStencil(D3D12_CPU_DESCRIPTOR_HANDLE viewHandle, const QSize &size, uint samples);

    QSGD3D12CPUWaitableFence *createCPUWaitableFence() const;
    void waitForGPU(QSGD3D12CPUWaitableFence *f) const;

    void transitionResource(ID3D12Resource *resource, ID3D12GraphicsCommandList *commandList,
                            D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) const;
    void resolveMultisampledTarget(ID3D12Resource *msaa, ID3D12Resource *resolve, D3D12_RESOURCE_STATES resolveUsage,
                                   ID3D12GraphicsCommandList *commandList) const;
    void uavBarrier(ID3D12Resource *resource, ID3D12GraphicsCommandList *commandList) const;

    ID3D12Resource *createBuffer(int size);

    typedef QVector<QPair<int, int> > DirtyList;
    void addDirtyRange(DirtyList *dirty, int offset, int size, int bufferSize);

    struct PersistentFrameData {
        ComPtr<ID3D12DescriptorHeap> gpuCbvSrvUavHeap;
        int gpuCbvSrvUavHeapSize;
        int cbvSrvUavNextFreeDescriptorIndex;
        QSet<uint> pendingTextureUploads;
        QSet<uint> pendingTextureMipMap;
        struct DeleteQueueEntry {
            ComPtr<ID3D12Resource> res;
            ComPtr<ID3D12DescriptorHeap> descHeap;
            SIZE_T cpuDescriptorPtr = 0;
            D3D12_DESCRIPTOR_HEAP_TYPE descHeapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        };
        QVector<DeleteQueueEntry> deleteQueue;
        QVector<DeleteQueueEntry> outOfFrameDeleteQueue;
        QSet<uint> buffersUsedInDrawCallSet;
        QSet<uint> buffersUsedInFrame;
        struct PendingRelease {
            enum Type {
                TypeTexture,
                TypeBuffer
            };
            Type type = TypeTexture;
            uint id = 0;
            PendingRelease(Type type, uint id) : type(type), id(id) { }
            PendingRelease() { }
            bool operator==(const PendingRelease &other) const { return type == other.type && id == other.id; }
        };
        QSet<PendingRelease> pendingReleases;
        QSet<PendingRelease> outOfFramePendingReleases;
    };
    friend uint qHash(const PersistentFrameData::PendingRelease &pr, uint seed);

    void deferredDelete(ComPtr<ID3D12Resource> res);
    void deferredDelete(ComPtr<ID3D12DescriptorHeap> dh);
    void deferredDelete(D3D12_CPU_DESCRIPTOR_HANDLE h, D3D12_DESCRIPTOR_HEAP_TYPE type);

    struct Buffer;
    void ensureBuffer(Buffer *buf);
    void updateBuffer(Buffer *buf);

    void beginDrawCalls();
    void beginFrameDraw();
    void endDrawCalls(bool lastInFrame = false);

    static const int MAX_SWAP_CHAIN_BUFFER_COUNT = 4;
    static const int MAX_FRAME_IN_FLIGHT_COUNT = 4;

    bool initialized = false;
    bool inFrame = false;
    WId window = 0;
    QSize windowSize;
    float windowDpr;
    uint windowSamples;
    bool windowAlpha;
    int swapChainBufferCount;
    int frameInFlightCount;
    int waitableSwapChainMaxLatency;
    ID3D12Device *device;
    ComPtr<ID3D12CommandQueue> commandQueue;
    ComPtr<ID3D12CommandQueue> copyCommandQueue;
    ComPtr<IDXGISwapChain3> swapChain;
    HANDLE swapEvent;
    ComPtr<ID3D12Resource> backBufferRT[MAX_SWAP_CHAIN_BUFFER_COUNT];
    ComPtr<ID3D12Resource> defaultRT[MAX_SWAP_CHAIN_BUFFER_COUNT];
    D3D12_CPU_DESCRIPTOR_HANDLE defaultRTV[MAX_SWAP_CHAIN_BUFFER_COUNT];
    ComPtr<ID3D12Resource> defaultDS;
    D3D12_CPU_DESCRIPTOR_HANDLE defaultDSV;
    ComPtr<ID3D12CommandAllocator> frameCommandAllocator[MAX_FRAME_IN_FLIGHT_COUNT];
    ComPtr<ID3D12CommandAllocator> copyCommandAllocator;
    ComPtr<ID3D12GraphicsCommandList> frameCommandList;
    ComPtr<ID3D12GraphicsCommandList> copyCommandList;
    QSGD3D12CPUDescriptorHeapManager cpuDescHeapManager;
    quint64 presentFrameIndex;
    quint64 frameIndex;
    QSGD3D12CPUWaitableFence *presentFence = nullptr;
    QSGD3D12CPUWaitableFence *frameFence[MAX_FRAME_IN_FLIGHT_COUNT];

    PersistentFrameData pframeData[MAX_FRAME_IN_FLIGHT_COUNT];
    int currentPFrameIndex;
    ID3D12GraphicsCommandList *commandList = nullptr;
    int activeLayers = 0;
    int currentLayerDepth = 0;

    struct PSOCacheEntry {
        ComPtr<ID3D12PipelineState> pso;
    };
    QCache<QSGD3D12PipelineState, PSOCacheEntry> psoCache;
    struct RootSigCacheEntry {
        ComPtr<ID3D12RootSignature> rootSig;
    };
    QCache<QSGD3D12RootSignature, RootSigCacheEntry> rootSigCache;

    struct Texture {
        enum Flag {
            EntryInUse = 0x01,
            Alpha = 0x02,
            MipMap = 0x04
        };
        int flags = 0;
        bool entryInUse() const { return flags & EntryInUse; }
        bool alpha() const { return flags & Alpha; }
        bool mipmap() const { return flags & MipMap; }
        ComPtr<ID3D12Resource> texture;
        D3D12_CPU_DESCRIPTOR_HANDLE srv;
        quint64 fenceValue = 0;
        quint64 lastWaitFenceValue = 0;
        struct StagingHeap {
            ComPtr<ID3D12Heap> heap;
        };
        QVector<StagingHeap> stagingHeaps;
        struct StagingBuffer {
            ComPtr<ID3D12Resource> buffer;
        };
        QVector<StagingBuffer> stagingBuffers;
        QVector<D3D12_CPU_DESCRIPTOR_HANDLE> mipUAVs;
    };

    QVector<Texture> textures;
    ComPtr<ID3D12Fence> textureUploadFence;
    QAtomicInt nextTextureUploadFenceValue;

    struct TransientFrameData {
        QSGGeometry::DrawingMode drawingMode;
        uint currentIndexBuffer;
        struct ActiveTexture {
            enum Type {
                TypeTexture,
                TypeRenderTarget
            };
            Type type = TypeTexture;
            uint id = 0;
            ActiveTexture(Type type, uint id) : type(type), id(id) { }
            ActiveTexture() { }
        };
        int activeTextureCount;
        ActiveTexture activeTextures[QSGD3D12_MAX_TEXTURE_VIEWS];
        int drawCount;
        ID3D12PipelineState *lastPso;
        ID3D12RootSignature *lastRootSig;
        bool descHeapSet;

        QRect viewport;
        QRect scissor;
        QVector4D blendFactor = QVector4D(1, 1, 1, 1);
        quint32 stencilRef = 1;
        QSGD3D12PipelineState pipelineState;
    };
    TransientFrameData tframeData;

    struct MipMapGen {
        bool initialize(QSGD3D12EnginePrivate *enginePriv);
        void releaseResources();
        void queueGenerate(const Texture &t);

        QSGD3D12EnginePrivate *engine;
        ComPtr<ID3D12RootSignature> rootSig;
        ComPtr<ID3D12PipelineState> pipelineState;
    };

    MipMapGen mipmapper;

    struct RenderTarget {
        enum Flag {
            EntryInUse = 0x01,
            NeedsReadBarrier = 0x02,
            Multisample = 0x04
        };
        int flags = 0;
        bool entryInUse() const { return flags & EntryInUse; }
        ComPtr<ID3D12Resource> color;
        ComPtr<ID3D12Resource> colorResolve;
        D3D12_CPU_DESCRIPTOR_HANDLE rtv;
        ComPtr<ID3D12Resource> ds;
        D3D12_CPU_DESCRIPTOR_HANDLE dsv;
        D3D12_CPU_DESCRIPTOR_HANDLE srv;
    };

    QVector<RenderTarget> renderTargets;
    uint currentRenderTarget;

    struct CPUBufferRef {
        const quint8 *p = nullptr;
        quint32 size = 0;
        DirtyList dirty;
        CPUBufferRef() { dirty.reserve(16); }
    };

    struct Buffer {
        enum Flag {
            EntryInUse = 0x01
        };
        int flags = 0;
        bool entryInUse() const { return flags & EntryInUse; }
        struct InFlightData {
            ComPtr<ID3D12Resource> buffer;
            DirtyList dirty;
            quint32 dataSize = 0;
            quint32 resourceSize = 0;
            InFlightData() { dirty.reserve(16); }
        };
        InFlightData d[MAX_FRAME_IN_FLIGHT_COUNT];
        CPUBufferRef cpuDataRef;
    };

    QVector<Buffer> buffers;

    struct DeviceLossTester {
        bool initialize(QSGD3D12EnginePrivate *enginePriv);
        void releaseResources();
        void killDevice();

        QSGD3D12EnginePrivate *engine;
        ComPtr<ID3D12PipelineState> computeState;
        ComPtr<ID3D12RootSignature> computeRootSignature;
    };

    DeviceLossTester devLossTest;

#ifndef Q_OS_WINRT
    ComPtr<IDCompositionDevice> dcompDevice;
    ComPtr<IDCompositionTarget> dcompTarget;
    ComPtr<IDCompositionVisual> dcompVisual;
#endif
};

inline uint qHash(const QSGD3D12EnginePrivate::PersistentFrameData::PendingRelease &pr, uint seed = 0)
{
    Q_UNUSED(seed);
    return pr.id + pr.type;
}

QT_END_NAMESPACE

#endif
