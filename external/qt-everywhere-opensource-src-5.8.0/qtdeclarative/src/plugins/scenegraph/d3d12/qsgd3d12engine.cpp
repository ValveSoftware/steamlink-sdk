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

#include "qsgd3d12engine_p.h"
#include "qsgd3d12engine_p_p.h"
#include "cs_mipmapgen.hlslh"
#include <QString>
#include <QColor>
#include <QLoggingCategory>
#include <qmath.h>
#include <qalgorithms.h>

// Comment out to disable DeviceLossTester functionality in order to reduce
// code size and improve startup perf a tiny bit.
#define DEVLOSS_TEST

#ifdef DEVLOSS_TEST
#include "cs_tdr.hlslh"
#endif

#ifdef Q_OS_WINRT
#include <QtCore/private/qeventdispatcher_winrt_p.h>
#include <functional>
#include <windows.ui.xaml.h>
#include <windows.ui.xaml.media.dxinterop.h>
#endif

#include <comdef.h>

QT_BEGIN_NAMESPACE

// NOTE: Avoid categorized logging. It is slow.

#define DECLARE_DEBUG_VAR(variable) \
    static bool debug_ ## variable() \
    { static bool value = qgetenv("QSG_RENDERER_DEBUG").contains(QT_STRINGIFY(variable)); return value; }

DECLARE_DEBUG_VAR(render)
DECLARE_DEBUG_VAR(descheap)
DECLARE_DEBUG_VAR(buffer)
DECLARE_DEBUG_VAR(texture)

// Except for system info on startup.
Q_LOGGING_CATEGORY(QSG_LOG_INFO_GENERAL, "qt.scenegraph.general")


// Any changes to the defaults below must be reflected in adaptations.qdoc as
// well and proven by qmlbench or similar.

static const int DEFAULT_SWAP_CHAIN_BUFFER_COUNT = 3;
static const int DEFAULT_FRAME_IN_FLIGHT_COUNT = 2;
static const int DEFAULT_WAITABLE_SWAP_CHAIN_MAX_LATENCY = 0;

static const int MAX_DRAW_CALLS_PER_LIST = 4096;

static const int MAX_CACHED_ROOTSIG = 16;
static const int MAX_CACHED_PSO = 64;

static const int GPU_CBVSRVUAV_DESCRIPTORS = 512;

static const DXGI_FORMAT RT_COLOR_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;

static const int BUCKETS_PER_HEAP = 8; // must match freeMap
static const int DESCRIPTORS_PER_BUCKET = 32; // the bit map (freeMap) is quint32
static const int MAX_DESCRIPTORS_PER_HEAP = BUCKETS_PER_HEAP * DESCRIPTORS_PER_BUCKET;

static QString comErrorMessage(HRESULT hr)
{
#ifndef Q_OS_WINRT
    const _com_error comError(hr);
#else
    const _com_error comError(hr, nullptr);
#endif
    QString result = QLatin1String("Error 0x") + QString::number(ulong(hr), 16);
    if (const wchar_t *msg = comError.ErrorMessage())
        result += QLatin1String(": ") + QString::fromWCharArray(msg);
    return result;
}

D3D12_CPU_DESCRIPTOR_HANDLE QSGD3D12CPUDescriptorHeapManager::allocate(D3D12_DESCRIPTOR_HEAP_TYPE type)
{
    D3D12_CPU_DESCRIPTOR_HANDLE h = {};
    for (Heap &heap : m_heaps) {
        if (heap.type == type) {
            for (int bucket = 0; bucket < _countof(heap.freeMap); ++bucket)
                if (heap.freeMap[bucket]) {
                    uint freePos = qCountTrailingZeroBits(heap.freeMap[bucket]);
                    heap.freeMap[bucket] &= ~(1UL << freePos);
                    if (Q_UNLIKELY(debug_descheap()))
                        qDebug("descriptor handle heap %p type %x reserve in bucket %d index %d", &heap, type, bucket, freePos);
                    freePos += bucket * DESCRIPTORS_PER_BUCKET;
                    h = heap.start;
                    h.ptr += freePos * heap.handleSize;
                    return h;
                }
        }
    }

    Heap heap;
    heap.type = type;
    heap.handleSize = m_handleSizes[type];

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = MAX_DESCRIPTORS_PER_HEAP;
    heapDesc.Type = type;
    // The heaps created here are _never_ shader-visible.

    HRESULT hr = m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap.heap));
    if (FAILED(hr)) {
        qWarning("Failed to create heap with type 0x%x: %s",
                 type, qPrintable(comErrorMessage(hr)));
        return h;
    }

    heap.start = heap.heap->GetCPUDescriptorHandleForHeapStart();

    if (Q_UNLIKELY(debug_descheap()))
        qDebug("new descriptor heap, type %x, start %llu", type, heap.start.ptr);

    heap.freeMap[0] = 0xFFFFFFFE;
    for (int i = 1; i < _countof(heap.freeMap); ++i)
        heap.freeMap[i] = 0xFFFFFFFF;

    h = heap.start;

    m_heaps.append(heap);

    return h;
}

void QSGD3D12CPUDescriptorHeapManager::release(D3D12_CPU_DESCRIPTOR_HANDLE handle, D3D12_DESCRIPTOR_HEAP_TYPE type)
{
    for (Heap &heap : m_heaps) {
        if (heap.type == type
                && handle.ptr >= heap.start.ptr
                && handle.ptr < heap.start.ptr + heap.handleSize * MAX_DESCRIPTORS_PER_HEAP) {
            unsigned long pos = (handle.ptr - heap.start.ptr) / heap.handleSize;
            const int bucket = pos / DESCRIPTORS_PER_BUCKET;
            const int indexInBucket = pos - bucket * DESCRIPTORS_PER_BUCKET;
            heap.freeMap[bucket] |= 1UL << indexInBucket;
            if (Q_UNLIKELY(debug_descheap()))
                qDebug("free descriptor handle heap %p type %x bucket %d index %d", &heap, type, bucket, indexInBucket);
            return;
        }
    }
    qWarning("QSGD3D12CPUDescriptorHeapManager: Attempted to release untracked descriptor handle %llu of type %d", handle.ptr, type);
}

void QSGD3D12CPUDescriptorHeapManager::initialize(ID3D12Device *device)
{
    m_device = device;

    for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
        m_handleSizes[i] = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE(i));
}

void QSGD3D12CPUDescriptorHeapManager::releaseResources()
{
    for (Heap &heap : m_heaps)
        heap.heap = nullptr;

    m_heaps.clear();

    m_device = nullptr;
}

// One device per process, one everything else (engine) per window.
Q_GLOBAL_STATIC(QSGD3D12DeviceManager, deviceManager)

static void getHardwareAdapter(IDXGIFactory1 *factory, IDXGIAdapter1 **outAdapter)
{
    const D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0;
    ComPtr<IDXGIAdapter1> adapter;
    DXGI_ADAPTER_DESC1 desc;

    for (int adapterIndex = 0; factory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND; ++adapterIndex) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        const QString name = QString::fromUtf16((char16_t *) desc.Description);
        qCDebug(QSG_LOG_INFO_GENERAL, "Adapter %d: '%s' (flags 0x%x)", adapterIndex, qPrintable(name), desc.Flags);
    }

    if (qEnvironmentVariableIsSet("QT_D3D_ADAPTER_INDEX")) {
        const int adapterIndex = qEnvironmentVariableIntValue("QT_D3D_ADAPTER_INDEX");
        if (SUCCEEDED(factory->EnumAdapters1(adapterIndex, &adapter))) {
            adapter->GetDesc1(&desc);
            const QString name = QString::fromUtf16((char16_t *) desc.Description);
            HRESULT hr = D3D12CreateDevice(adapter.Get(), fl, _uuidof(ID3D12Device), nullptr);
            if (SUCCEEDED(hr)) {
                qCDebug(QSG_LOG_INFO_GENERAL, "Using requested adapter '%s'", qPrintable(name));
                *outAdapter = adapter.Detach();
                return;
            } else {
                qWarning("Failed to create device for requested adapter '%s': %s",
                         qPrintable(name), qPrintable(comErrorMessage(hr)));
            }
        }
    }

    for (int adapterIndex = 0; factory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND; ++adapterIndex) {
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;

        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), fl, _uuidof(ID3D12Device), nullptr))) {
            const QString name = QString::fromUtf16((char16_t *) desc.Description);
            qCDebug(QSG_LOG_INFO_GENERAL, "Using adapter '%s'", qPrintable(name));
            break;
        }
    }

    *outAdapter = adapter.Detach();
}

ID3D12Device *QSGD3D12DeviceManager::ref()
{
    ensureCreated();
    m_ref.ref();
    return m_device.Get();
}

void QSGD3D12DeviceManager::unref()
{
    if (!m_ref.deref()) {
        if (Q_UNLIKELY(debug_render()))
            qDebug("destroying d3d device");
        m_device = nullptr;
        m_factory = nullptr;
    }
}

void QSGD3D12DeviceManager::deviceLossDetected()
{
    for (DeviceLossObserver *observer : qAsConst(m_observers))
        observer->deviceLost();

    // Nothing else to do here. All windows are expected to release their
    // resources and call unref() in response immediately.
}

IDXGIFactory4 *QSGD3D12DeviceManager::dxgi()
{
    ensureCreated();
    return m_factory.Get();
}

void QSGD3D12DeviceManager::ensureCreated()
{
    if (m_device)
        return;

    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&m_factory));
    if (FAILED(hr)) {
        qWarning("Failed to create DXGI: %s", qPrintable(comErrorMessage(hr)));
        return;
    }

    ComPtr<IDXGIAdapter1> adapter;
    getHardwareAdapter(m_factory.Get(), &adapter);

    bool warp = true;
    if (adapter) {
        HRESULT hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));
        if (SUCCEEDED(hr))
            warp = false;
        else
            qWarning("Failed to create device: %s", qPrintable(comErrorMessage(hr)));
    }

    if (warp) {
        qCDebug(QSG_LOG_INFO_GENERAL, "Using WARP");
        m_factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter));
        HRESULT hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));
        if (FAILED(hr)) {
            qWarning("Failed to create WARP device: %s", qPrintable(comErrorMessage(hr)));
            return;
        }
    }

    ComPtr<IDXGIAdapter3> adapter3;
    if (SUCCEEDED(adapter.As(&adapter3))) {
        DXGI_QUERY_VIDEO_MEMORY_INFO vidMemInfo;
        if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &vidMemInfo))) {
            qCDebug(QSG_LOG_INFO_GENERAL, "Video memory info: LOCAL: Budget %llu KB CurrentUsage %llu KB AvailableForReservation %llu KB CurrentReservation %llu KB",
                    vidMemInfo.Budget / 1024, vidMemInfo.CurrentUsage / 1024,
                    vidMemInfo.AvailableForReservation / 1024, vidMemInfo.CurrentReservation / 1024);
        }
        if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &vidMemInfo))) {
            qCDebug(QSG_LOG_INFO_GENERAL, "Video memory info: NON-LOCAL: Budget %llu KB CurrentUsage %llu KB AvailableForReservation %llu KB CurrentReservation %llu KB",
                    vidMemInfo.Budget / 1024, vidMemInfo.CurrentUsage / 1024,
                    vidMemInfo.AvailableForReservation / 1024, vidMemInfo.CurrentReservation / 1024);
        }
    }
}

void QSGD3D12DeviceManager::registerDeviceLossObserver(DeviceLossObserver *observer)
{
    if (!m_observers.contains(observer))
        m_observers.append(observer);
}

QSGD3D12Engine::QSGD3D12Engine()
{
    d = new QSGD3D12EnginePrivate;
}

QSGD3D12Engine::~QSGD3D12Engine()
{
    d->waitGPU();
    d->releaseResources();
    delete d;
}

bool QSGD3D12Engine::attachToWindow(WId window, const QSize &size, float dpr, int surfaceFormatSamples, bool alpha)
{
    if (d->isInitialized()) {
        qWarning("QSGD3D12Engine: Cannot attach active engine to window");
        return false;
    }

    d->initialize(window, size, dpr, surfaceFormatSamples, alpha);
    return d->isInitialized();
}

void QSGD3D12Engine::releaseResources()
{
    d->releaseResources();
}

bool QSGD3D12Engine::hasResources() const
{
    // An explicit releaseResources() or a device loss results in initialized == false.
    return d->isInitialized();
}

void QSGD3D12Engine::setWindowSize(const QSize &size, float dpr)
{
    d->setWindowSize(size, dpr);
}

WId QSGD3D12Engine::window() const
{
    return d->currentWindow();
}

QSize QSGD3D12Engine::windowSize() const
{
    return d->currentWindowSize();
}

float QSGD3D12Engine::windowDevicePixelRatio() const
{
    return d->currentWindowDpr();
}

uint QSGD3D12Engine::windowSamples() const
{
    return d->currentWindowSamples();
}

void QSGD3D12Engine::beginFrame()
{
    d->beginFrame();
}

void QSGD3D12Engine::endFrame()
{
    d->endFrame();
}

void QSGD3D12Engine::beginLayer()
{
    d->beginLayer();
}

void QSGD3D12Engine::endLayer()
{
    d->endLayer();
}

void QSGD3D12Engine::invalidateCachedFrameState()
{
    d->invalidateCachedFrameState();
}

void QSGD3D12Engine::restoreFrameState(bool minimal)
{
    d->restoreFrameState(minimal);
}

void QSGD3D12Engine::finalizePipeline(const QSGD3D12PipelineState &pipelineState)
{
    d->finalizePipeline(pipelineState);
}

uint QSGD3D12Engine::genBuffer()
{
    return d->genBuffer();
}

void QSGD3D12Engine::releaseBuffer(uint id)
{
    d->releaseBuffer(id);
}

void QSGD3D12Engine::resetBuffer(uint id, const quint8 *data, int size)
{
    d->resetBuffer(id, data, size);
}

void QSGD3D12Engine::markBufferDirty(uint id, int offset, int size)
{
    d->markBufferDirty(id, offset, size);
}

void QSGD3D12Engine::queueViewport(const QRect &rect)
{
    d->queueViewport(rect);
}

void QSGD3D12Engine::queueScissor(const QRect &rect)
{
    d->queueScissor(rect);
}

void QSGD3D12Engine::queueSetRenderTarget(uint id)
{
    d->queueSetRenderTarget(id);
}

void QSGD3D12Engine::queueClearRenderTarget(const QColor &color)
{
    d->queueClearRenderTarget(color);
}

void QSGD3D12Engine::queueClearDepthStencil(float depthValue, quint8 stencilValue, ClearFlags which)
{
    d->queueClearDepthStencil(depthValue, stencilValue, which);
}

void QSGD3D12Engine::queueSetBlendFactor(const QVector4D &factor)
{
    d->queueSetBlendFactor(factor);
}

void QSGD3D12Engine::queueSetStencilRef(quint32 ref)
{
    d->queueSetStencilRef(ref);
}

void QSGD3D12Engine::queueDraw(const DrawParams &params)
{
    d->queueDraw(params);
}

void QSGD3D12Engine::present()
{
    d->present();
}

void QSGD3D12Engine::waitGPU()
{
    d->waitGPU();
}

uint QSGD3D12Engine::genTexture()
{
    return d->genTexture();
}

void QSGD3D12Engine::createTexture(uint id, const QSize &size, QImage::Format format, TextureCreateFlags flags)
{
    d->createTexture(id, size, format, flags);
}

void QSGD3D12Engine::queueTextureResize(uint id, const QSize &size)
{
    d->queueTextureResize(id, size);
}

void QSGD3D12Engine::queueTextureUpload(uint id, const QImage &image, const QPoint &dstPos, TextureUploadFlags flags)
{
    d->queueTextureUpload(id, QVector<QImage>() << image, QVector<QPoint>() << dstPos, flags);
}

void QSGD3D12Engine::queueTextureUpload(uint id, const QVector<QImage> &images, const QVector<QPoint> &dstPos,
                                        TextureUploadFlags flags)
{
    d->queueTextureUpload(id, images, dstPos, flags);
}

void QSGD3D12Engine::releaseTexture(uint id)
{
    d->releaseTexture(id);
}

void QSGD3D12Engine::useTexture(uint id)
{
    d->useTexture(id);
}

uint QSGD3D12Engine::genRenderTarget()
{
    return d->genRenderTarget();
}

void QSGD3D12Engine::createRenderTarget(uint id, const QSize &size, const QVector4D &clearColor, uint samples)
{
    d->createRenderTarget(id, size, clearColor, samples);
}

void QSGD3D12Engine::releaseRenderTarget(uint id)
{
    d->releaseRenderTarget(id);
}

void QSGD3D12Engine::useRenderTargetAsTexture(uint id)
{
    d->useRenderTargetAsTexture(id);
}

uint QSGD3D12Engine::activeRenderTarget() const
{
    return d->activeRenderTarget();
}

QImage QSGD3D12Engine::executeAndWaitReadbackRenderTarget(uint id)
{
    return d->executeAndWaitReadbackRenderTarget(id);
}

void QSGD3D12Engine::simulateDeviceLoss()
{
    d->simulateDeviceLoss();
}

void *QSGD3D12Engine::getResource(QQuickWindow *, QSGRendererInterface::Resource resource) const
{
    return d->getResource(resource);
}

static inline quint32 alignedSize(quint32 size, quint32 byteAlign)
{
    return (size + byteAlign - 1) & ~(byteAlign - 1);
}

quint32 QSGD3D12Engine::alignedConstantBufferSize(quint32 size)
{
    return alignedSize(size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
}

QSGD3D12Format QSGD3D12Engine::toDXGIFormat(QSGGeometry::Type sgtype, int tupleSize, int *size)
{
    QSGD3D12Format format = FmtUnknown;

    static const QSGD3D12Format formatMap_ub[] = { FmtUnknown,
                                                   FmtUNormByte,
                                                   FmtUNormByte2,
                                                   FmtUnknown,
                                                   FmtUNormByte4 };

    static const QSGD3D12Format formatMap_f[] = { FmtUnknown,
                                                  FmtFloat,
                                                  FmtFloat2,
                                                  FmtFloat3,
                                                  FmtFloat4 };

    switch (sgtype) {
    case QSGGeometry::UnsignedByteType:
        format = formatMap_ub[tupleSize];
        if (size)
            *size = tupleSize;
        break;
    case QSGGeometry::FloatType:
        format = formatMap_f[tupleSize];
        if (size)
            *size = sizeof(float) * tupleSize;
        break;

    case QSGGeometry::UnsignedShortType:
        format = FmtUnsignedShort;
        if (size)
            *size = sizeof(ushort) * tupleSize;
        break;
    case QSGGeometry::UnsignedIntType:
        format = FmtUnsignedInt;
        if (size)
            *size = sizeof(uint) * tupleSize;
        break;

    case QSGGeometry::ByteType:
    case QSGGeometry::IntType:
    case QSGGeometry::ShortType:
        qWarning("no mapping for GL type 0x%x", sgtype);
        break;

    default:
        qWarning("unknown GL type 0x%x", sgtype);
        break;
    }

    return format;
}

int QSGD3D12Engine::mipMapLevels(const QSize &size)
{
    return ceil(log2(qMax(size.width(), size.height()))) + 1;
}

inline static bool isPowerOfTwo(int x)
{
    // Assumption: x >= 1
    return x == (x & -x);
}

QSize QSGD3D12Engine::mipMapAdjustedSourceSize(const QSize &size)
{
    if (size.isEmpty())
        return size;

    QSize adjustedSize = size;

    // ### for now only power-of-two sizes are mipmap-capable
    if (!isPowerOfTwo(size.width()))
        adjustedSize.setWidth(qNextPowerOfTwo(size.width()));
    if (!isPowerOfTwo(size.height()))
        adjustedSize.setHeight(qNextPowerOfTwo(size.height()));

    return adjustedSize;
}

void QSGD3D12EnginePrivate::releaseResources()
{
    if (!initialized)
        return;

    mipmapper.releaseResources();
    devLossTest.releaseResources();

    frameCommandList = nullptr;
    copyCommandList = nullptr;

    copyCommandAllocator = nullptr;
    for (int i = 0; i < frameInFlightCount; ++i) {
        frameCommandAllocator[i] = nullptr;
        pframeData[i].gpuCbvSrvUavHeap = nullptr;
        delete frameFence[i];
    }

    defaultDS = nullptr;
    for (int i = 0; i < swapChainBufferCount; ++i) {
        backBufferRT[i] = nullptr;
        defaultRT[i] = nullptr;
    }

    psoCache.clear();
    rootSigCache.clear();
    buffers.clear();
    textures.clear();
    renderTargets.clear();

    cpuDescHeapManager.releaseResources();

    commandQueue = nullptr;
    copyCommandQueue = nullptr;

#ifndef Q_OS_WINRT
    dcompTarget = nullptr;
    dcompVisual = nullptr;
    dcompDevice = nullptr;
#endif

    swapChain = nullptr;

    delete presentFence;
    textureUploadFence = nullptr;

    deviceManager()->unref();

    initialized = false;

    // 'window' must be kept, may just be a device loss
}

void QSGD3D12EnginePrivate::initialize(WId w, const QSize &size, float dpr, int surfaceFormatSamples, bool alpha)
{
    if (initialized)
        return;

    window = w;
    windowSize = size;
    windowDpr = dpr;
    windowSamples = qMax(1, surfaceFormatSamples); // may be -1 or 0, whereas windowSamples is uint and >= 1
    windowAlpha = alpha;

    swapChainBufferCount = qMin(qEnvironmentVariableIntValue("QT_D3D_BUFFER_COUNT"), MAX_SWAP_CHAIN_BUFFER_COUNT);
    if (swapChainBufferCount < 2)
        swapChainBufferCount = DEFAULT_SWAP_CHAIN_BUFFER_COUNT;

    frameInFlightCount = qMin(qEnvironmentVariableIntValue("QT_D3D_FRAME_COUNT"), MAX_FRAME_IN_FLIGHT_COUNT);
    if (frameInFlightCount < 1)
        frameInFlightCount = DEFAULT_FRAME_IN_FLIGHT_COUNT;

    static const char *latReqEnvVar = "QT_D3D_WAITABLE_SWAP_CHAIN_MAX_LATENCY";
    if (!qEnvironmentVariableIsSet(latReqEnvVar))
        waitableSwapChainMaxLatency = DEFAULT_WAITABLE_SWAP_CHAIN_MAX_LATENCY;
    else
        waitableSwapChainMaxLatency = qBound(0, qEnvironmentVariableIntValue(latReqEnvVar), 16);

    if (qEnvironmentVariableIsSet("QSG_INFO"))
        const_cast<QLoggingCategory &>(QSG_LOG_INFO_GENERAL()).setEnabled(QtDebugMsg, true);

    qCDebug(QSG_LOG_INFO_GENERAL, "d3d12 engine init. swap chain buffer count %d, max frames prepared without blocking %d",
            swapChainBufferCount, frameInFlightCount);
    if (waitableSwapChainMaxLatency)
        qCDebug(QSG_LOG_INFO_GENERAL, "Swap chain frame latency waitable object enabled. Frame latency is %d", waitableSwapChainMaxLatency);

    const bool debugLayer = qEnvironmentVariableIntValue("QT_D3D_DEBUG") != 0;
    if (debugLayer) {
        qCDebug(QSG_LOG_INFO_GENERAL, "Enabling debug layer");
#if !defined(Q_OS_WINRT) || !defined(NDEBUG)
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
            debugController->EnableDebugLayer();
#else
        qCDebug(QSG_LOG_INFO_GENERAL, "Using DebugInterface will not allow certification to pass");
#endif
    }

    QSGD3D12DeviceManager *dev = deviceManager();
    device = dev->ref();
    dev->registerDeviceLossObserver(this);

    if (debugLayer) {
        ComPtr<ID3D12InfoQueue> infoQueue;
        if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
            const bool breakOnWarning = qEnvironmentVariableIntValue("QT_D3D_DEBUG_BREAK_ON_WARNING") != 0;
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, breakOnWarning);
            D3D12_INFO_QUEUE_FILTER filter = {};
            D3D12_MESSAGE_ID suppressedMessages[] = {
                // When using a render target other than the default one we
                // have no way to know the custom clear color, if there is one.
                D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE
            };
            filter.DenyList.NumIDs = _countof(suppressedMessages);
            filter.DenyList.pIDList = suppressedMessages;
            // setting the filter would enable Info messages which we don't need
            D3D12_MESSAGE_SEVERITY infoSev = D3D12_MESSAGE_SEVERITY_INFO;
            filter.DenyList.NumSeverities = 1;
            filter.DenyList.pSeverityList = &infoSev;
            infoQueue->PushStorageFilter(&filter);
        }
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)))) {
        qWarning("Failed to create command queue");
        return;
    }

    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
    if (FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&copyCommandQueue)))) {
        qWarning("Failed to create copy command queue");
        return;
    }

#ifndef Q_OS_WINRT
    HWND hwnd = reinterpret_cast<HWND>(w);

    if (windowAlpha) {
        // Go through DirectComposition for semi-transparent windows since the
        // traditional approaches won't fly with flip model swapchains.
        HRESULT hr = DCompositionCreateDevice(nullptr, IID_PPV_ARGS(&dcompDevice));
        if (SUCCEEDED(hr)) {
            hr = dcompDevice->CreateTargetForHwnd(hwnd, true, &dcompTarget);
            if (SUCCEEDED(hr)) {
                hr = dcompDevice->CreateVisual(&dcompVisual);
                if (FAILED(hr)) {
                    qWarning("Failed to create DirectComposition visual: %s",
                             qPrintable(comErrorMessage(hr)));
                    windowAlpha = false;
                }
            } else {
                qWarning("Failed to create DirectComposition target: %s",
                         qPrintable(comErrorMessage(hr)));
                windowAlpha = false;
            }
        } else {
            qWarning("Failed to create DirectComposition device: %s",
                     qPrintable(comErrorMessage(hr)));
            windowAlpha = false;
        }
    }

    if (windowAlpha) {
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.Width = windowSize.width() * windowDpr;
        swapChainDesc.Height = windowSize.height() * windowDpr;
        swapChainDesc.Format = RT_COLOR_FORMAT;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = swapChainBufferCount;
        swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
        if (waitableSwapChainMaxLatency)
            swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

        ComPtr<IDXGISwapChain1> baseSwapChain;
        HRESULT hr = dev->dxgi()->CreateSwapChainForComposition(commandQueue.Get(), &swapChainDesc, nullptr, &baseSwapChain);
        if (SUCCEEDED(hr)) {
            if (SUCCEEDED(baseSwapChain.As(&swapChain))) {
                hr = dcompVisual->SetContent(swapChain.Get());
                if (SUCCEEDED(hr)) {
                    hr = dcompTarget->SetRoot(dcompVisual.Get());
                    if (FAILED(hr)) {
                        qWarning("SetRoot failed for DirectComposition target: %s",
                                 qPrintable(comErrorMessage(hr)));
                        windowAlpha = false;
                    }
                } else {
                    qWarning("SetContent failed for DirectComposition visual: %s",
                             qPrintable(comErrorMessage(hr)));
                    windowAlpha = false;
                }
            } else {
                qWarning("Failed to cast swap chain");
                windowAlpha = false;
            }
        } else {
            qWarning("Failed to create swap chain for composition: 0x%x", hr);
            windowAlpha = false;
        }
    }

    if (!windowAlpha) {
        DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
        swapChainDesc.BufferCount = swapChainBufferCount;
        swapChainDesc.BufferDesc.Width = windowSize.width() * windowDpr;
        swapChainDesc.BufferDesc.Height = windowSize.height() * windowDpr;
        swapChainDesc.BufferDesc.Format = RT_COLOR_FORMAT;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // D3D12 requires the flip model
        swapChainDesc.OutputWindow = hwnd;
        swapChainDesc.SampleDesc.Count = 1; // Flip does not support MSAA so no choice here
        swapChainDesc.Windowed = TRUE;
        if (waitableSwapChainMaxLatency)
            swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

        ComPtr<IDXGISwapChain> baseSwapChain;
        HRESULT hr = dev->dxgi()->CreateSwapChain(commandQueue.Get(), &swapChainDesc, &baseSwapChain);
        if (FAILED(hr)) {
            qWarning("Failed to create swap chain: %s", qPrintable(comErrorMessage(hr)));
            return;
        }
        hr = baseSwapChain.As(&swapChain);
        if (FAILED(hr)) {
            qWarning("Failed to cast swap chain: %s", qPrintable(comErrorMessage(hr)));
            return;
        }
    }

    dev->dxgi()->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
#else
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = windowSize.width() * windowDpr;
    swapChainDesc.Height = windowSize.height() * windowDpr;
    swapChainDesc.Format = RT_COLOR_FORMAT;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = swapChainBufferCount;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    if (waitableSwapChainMaxLatency)
        swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

    ComPtr<IDXGISwapChain1> baseSwapChain;
    HRESULT hr = dev->dxgi()->CreateSwapChainForComposition(commandQueue.Get(), &swapChainDesc, nullptr, &baseSwapChain);
    if (FAILED(hr)) {
        qWarning("Failed to create swap chain for composition: 0x%x", hr);
        return;
    }
    if (FAILED(baseSwapChain.As(&swapChain))) {
        qWarning("Failed to cast swap chain");
        return;
    }

    // The winrt platform plugin returns an ISwapChainPanel* from winId().
    ComPtr<ABI::Windows::UI::Xaml::Controls::ISwapChainPanel> swapChainPanel
            = reinterpret_cast<ABI::Windows::UI::Xaml::Controls::ISwapChainPanel *>(window);
    ComPtr<ISwapChainPanelNative> swapChainPanelNative;
    if (FAILED(swapChainPanel.As(&swapChainPanelNative))) {
        qWarning("Failed to cast swap chain panel to native");
        return;
    }
    hr = QEventDispatcherWinRT::runOnXamlThread([this, &swapChainPanelNative]() {
        return swapChainPanelNative->SetSwapChain(swapChain.Get());
    });
    if (FAILED(hr)) {
        qWarning("Failed to set swap chain on panel: 0x%x", hr);
        return;
    }
#endif

    if (waitableSwapChainMaxLatency) {
        if (FAILED(swapChain->SetMaximumFrameLatency(waitableSwapChainMaxLatency)))
            qWarning("Failed to set maximum frame latency to %d", waitableSwapChainMaxLatency);
        swapEvent = swapChain->GetFrameLatencyWaitableObject();
    }

    for (int i = 0; i < frameInFlightCount; ++i) {
        if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frameCommandAllocator[i])))) {
            qWarning("Failed to create command allocator");
            return;
        }
    }

    if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&copyCommandAllocator)))) {
        qWarning("Failed to create copy command allocator");
        return;
    }

    for (int i = 0; i < frameInFlightCount; ++i) {
        if (!createCbvSrvUavHeap(i, GPU_CBVSRVUAV_DESCRIPTORS))
            return;
    }

    cpuDescHeapManager.initialize(device);

    setupDefaultRenderTargets();

    if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frameCommandAllocator[0].Get(),
                                         nullptr, IID_PPV_ARGS(&frameCommandList)))) {
        qWarning("Failed to create command list");
        return;
    }
    // created in recording state, close it for now
    frameCommandList->Close();

    if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, copyCommandAllocator.Get(),
                                         nullptr, IID_PPV_ARGS(&copyCommandList)))) {
        qWarning("Failed to create copy command list");
        return;
    }
    copyCommandList->Close();

    frameIndex = 0;

    presentFence = createCPUWaitableFence();
    for (int i = 0; i < frameInFlightCount; ++i)
        frameFence[i] = createCPUWaitableFence();

    if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&textureUploadFence)))) {
        qWarning("Failed to create fence");
        return;
    }

    psoCache.setMaxCost(MAX_CACHED_PSO);
    rootSigCache.setMaxCost(MAX_CACHED_ROOTSIG);

    if (!mipmapper.initialize(this))
        return;

    if (!devLossTest.initialize(this))
        return;

    currentRenderTarget = 0;

    initialized = true;
}

bool QSGD3D12EnginePrivate::createCbvSrvUavHeap(int pframeIndex, int descriptorCount)
{
    D3D12_DESCRIPTOR_HEAP_DESC gpuDescHeapDesc = {};
    gpuDescHeapDesc.NumDescriptors = descriptorCount;
    gpuDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    gpuDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    if (FAILED(device->CreateDescriptorHeap(&gpuDescHeapDesc, IID_PPV_ARGS(&pframeData[pframeIndex].gpuCbvSrvUavHeap)))) {
        qWarning("Failed to create shader-visible CBV-SRV-UAV heap");
        return false;
    }

    pframeData[pframeIndex].gpuCbvSrvUavHeapSize = descriptorCount;

    return true;
}

DXGI_SAMPLE_DESC QSGD3D12EnginePrivate::makeSampleDesc(DXGI_FORMAT format, uint samples)
{
    DXGI_SAMPLE_DESC sampleDesc;
    sampleDesc.Count = 1;
    sampleDesc.Quality = 0;

    if (samples > 1) {
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msaaInfo = {};
        msaaInfo.Format = format;
        msaaInfo.SampleCount = samples;
        if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msaaInfo, sizeof(msaaInfo)))) {
            if (msaaInfo.NumQualityLevels > 0) {
                sampleDesc.Count = samples;
                sampleDesc.Quality = msaaInfo.NumQualityLevels - 1;
            } else {
                qWarning("No quality levels for multisampling with sample count %d", samples);
            }
        } else {
            qWarning("Failed to query multisample quality levels for sample count %d", samples);
        }
    }

    return sampleDesc;
}

ID3D12Resource *QSGD3D12EnginePrivate::createColorBuffer(D3D12_CPU_DESCRIPTOR_HANDLE viewHandle, const QSize &size,
                                                         const QVector4D &clearColor, uint samples)
{
    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = RT_COLOR_FORMAT;
    clearValue.Color[0] = clearColor.x();
    clearValue.Color[1] = clearColor.y();
    clearValue.Color[2] = clearColor.z();
    clearValue.Color[3] = clearColor.w();

    D3D12_HEAP_PROPERTIES heapProp = {};
    heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC rtDesc = {};
    rtDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rtDesc.Width = size.width();
    rtDesc.Height = size.height();
    rtDesc.DepthOrArraySize = 1;
    rtDesc.MipLevels = 1;
    rtDesc.Format = RT_COLOR_FORMAT;
    rtDesc.SampleDesc = makeSampleDesc(rtDesc.Format, samples);
    rtDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    ID3D12Resource *resource = nullptr;
    const D3D12_RESOURCE_STATES initialState = samples <= 1
            ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
            : D3D12_RESOURCE_STATE_RENDER_TARGET;
    if (FAILED(device->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &rtDesc,
                                               initialState, &clearValue, IID_PPV_ARGS(&resource)))) {
        qWarning("Failed to create offscreen render target of size %dx%d", size.width(), size.height());
        return nullptr;
    }

    device->CreateRenderTargetView(resource, nullptr, viewHandle);

    return resource;
}

ID3D12Resource *QSGD3D12EnginePrivate::createDepthStencil(D3D12_CPU_DESCRIPTOR_HANDLE viewHandle, const QSize &size, uint samples)
{
    D3D12_CLEAR_VALUE depthClearValue = {};
    depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthClearValue.DepthStencil.Depth = 1.0f;
    depthClearValue.DepthStencil.Stencil = 0;

    D3D12_HEAP_PROPERTIES heapProp = {};
    heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    bufDesc.Width = size.width();
    bufDesc.Height = size.height();
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    bufDesc.SampleDesc = makeSampleDesc(bufDesc.Format, samples);
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    bufDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    ID3D12Resource *resource = nullptr;
    if (FAILED(device->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &bufDesc,
                                               D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClearValue, IID_PPV_ARGS(&resource)))) {
        qWarning("Failed to create depth-stencil buffer of size %dx%d", size.width(), size.height());
        return nullptr;
    }

    D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
    depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthStencilDesc.ViewDimension = bufDesc.SampleDesc.Count <= 1 ? D3D12_DSV_DIMENSION_TEXTURE2D : D3D12_DSV_DIMENSION_TEXTURE2DMS;

    device->CreateDepthStencilView(resource, &depthStencilDesc, viewHandle);

    return resource;
}

void QSGD3D12EnginePrivate::setupDefaultRenderTargets()
{
    for (int i = 0; i < swapChainBufferCount; ++i) {
        if (FAILED(swapChain->GetBuffer(i, IID_PPV_ARGS(&backBufferRT[i])))) {
            qWarning("Failed to get buffer %d from swap chain", i);
            return;
        }
        defaultRTV[i] = cpuDescHeapManager.allocate(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        if (windowSamples == 1) {
            defaultRT[i] = backBufferRT[i];
            device->CreateRenderTargetView(defaultRT[i].Get(), nullptr, defaultRTV[i]);
        } else {
            const QSize size(windowSize.width() * windowDpr, windowSize.height() * windowDpr);
            // Not optimal if the user called setClearColor, but there's so
            // much we can do. The debug layer warning is suppressed so we're good to go.
            const QColor cc(Qt::white);
            const QVector4D clearColor(cc.redF(), cc.greenF(), cc.blueF(), cc.alphaF());
            ID3D12Resource *msaaRT = createColorBuffer(defaultRTV[i], size, clearColor, windowSamples);
            if (msaaRT)
                defaultRT[i].Attach(msaaRT);
        }
    }

    defaultDSV = cpuDescHeapManager.allocate(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    const QSize size(windowSize.width() * windowDpr, windowSize.height() * windowDpr);
    ID3D12Resource *ds = createDepthStencil(defaultDSV, size, windowSamples);
    if (ds)
        defaultDS.Attach(ds);

    presentFrameIndex = 0;
}

void QSGD3D12EnginePrivate::setWindowSize(const QSize &size, float dpr)
{
    if (!initialized || (windowSize == size && windowDpr == dpr))
        return;

    waitGPU();

    windowSize = size;
    windowDpr = dpr;

    if (Q_UNLIKELY(debug_render()))
        qDebug() << "resize" << size << dpr;

    // Clear these, otherwise resizing will fail.
    defaultDS = nullptr;
    cpuDescHeapManager.release(defaultDSV, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    for (int i = 0; i < swapChainBufferCount; ++i) {
        backBufferRT[i] = nullptr;
        defaultRT[i] = nullptr;
        cpuDescHeapManager.release(defaultRTV[i], D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    const int w = windowSize.width() * windowDpr;
    const int h = windowSize.height() * windowDpr;
    HRESULT hr = swapChain->ResizeBuffers(swapChainBufferCount, w, h, RT_COLOR_FORMAT,
                                          waitableSwapChainMaxLatency ? DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT : 0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        deviceManager()->deviceLossDetected();
        return;
    } else if (FAILED(hr)) {
        qWarning("Failed to resize buffers: %s", qPrintable(comErrorMessage(hr)));
        return;
    }

    setupDefaultRenderTargets();
}

void QSGD3D12EnginePrivate::deviceLost()
{
    qWarning("D3D device lost, will attempt to reinitialize");

    // Release all resources. This is important because otherwise reinitialization may fail.
    releaseResources();

    // Now in uninitialized state (but 'window' is still valid). Will recreate
    // all the resources on the next beginFrame().
}

QSGD3D12CPUWaitableFence *QSGD3D12EnginePrivate::createCPUWaitableFence() const
{
    QSGD3D12CPUWaitableFence *f = new QSGD3D12CPUWaitableFence;
    HRESULT hr = device->CreateFence(f->value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&f->fence));
    if (FAILED(hr)) {
        qWarning("Failed to create fence: %s", qPrintable(comErrorMessage(hr)));
        return f;
    }
    f->event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    return f;
}

void QSGD3D12EnginePrivate::waitForGPU(QSGD3D12CPUWaitableFence *f) const
{
    const UINT64 newValue = f->value.fetchAndAddAcquire(1) + 1;
    commandQueue->Signal(f->fence.Get(), newValue);
    if (f->fence->GetCompletedValue() < newValue) {
        HRESULT hr = f->fence->SetEventOnCompletion(newValue, f->event);
        if (FAILED(hr)) {
            qWarning("SetEventOnCompletion failed: %s", qPrintable(comErrorMessage(hr)));
            return;
        }
        WaitForSingleObject(f->event, INFINITE);
    }
}

void QSGD3D12EnginePrivate::transitionResource(ID3D12Resource *resource, ID3D12GraphicsCommandList *commandList,
                                               D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) const
{
    D3D12_RESOURCE_BARRIER barrier;
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    commandList->ResourceBarrier(1, &barrier);
}

void QSGD3D12EnginePrivate::resolveMultisampledTarget(ID3D12Resource *msaa,
                                                      ID3D12Resource *resolve,
                                                      D3D12_RESOURCE_STATES resolveUsage,
                                                      ID3D12GraphicsCommandList *commandList) const
{
    D3D12_RESOURCE_BARRIER barriers[2];
    for (int i = 0; i < _countof(barriers); ++i) {
        barriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[i].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barriers[i].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    }

    barriers[0].Transition.pResource = msaa;
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
    barriers[1].Transition.pResource = resolve;
    barriers[1].Transition.StateBefore = resolveUsage;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_RESOLVE_DEST;
    commandList->ResourceBarrier(2, barriers);

    commandList->ResolveSubresource(resolve, 0, msaa, 0, RT_COLOR_FORMAT);

    barriers[0].Transition.pResource = msaa;
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barriers[1].Transition.pResource = resolve;
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_RESOLVE_DEST;
    barriers[1].Transition.StateAfter = resolveUsage;
    commandList->ResourceBarrier(2, barriers);
}

void QSGD3D12EnginePrivate::uavBarrier(ID3D12Resource *resource, ID3D12GraphicsCommandList *commandList) const
{
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = resource;

    commandList->ResourceBarrier(1, &barrier);
}

ID3D12Resource *QSGD3D12EnginePrivate::createBuffer(int size)
{
    ID3D12Resource *buf;

    D3D12_HEAP_PROPERTIES uploadHeapProp = {};
    uploadHeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = size;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    HRESULT hr = device->CreateCommittedResource(&uploadHeapProp, D3D12_HEAP_FLAG_NONE, &bufDesc,
                                                 D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buf));
    if (FAILED(hr))
        qWarning("Failed to create buffer resource: %s", qPrintable(comErrorMessage(hr)));

    return buf;
}

void QSGD3D12EnginePrivate::ensureBuffer(Buffer *buf)
{
    Buffer::InFlightData &bfd(buf->d[currentPFrameIndex]);
    // Only enlarge, never shrink
    const bool newBufferNeeded = bfd.buffer ? (buf->cpuDataRef.size > bfd.resourceSize) : true;
    if (newBufferNeeded) {
        // Round it up and overallocate a little bit so that a subsequent
        // buffer contents rebuild with a slightly larger total size does
        // not lead to creating a new buffer.
        const quint32 sz = alignedSize(buf->cpuDataRef.size, 4096);
        if (Q_UNLIKELY(debug_buffer()))
            qDebug("new buffer[pf=%d] of size %d (actual data size %d)", currentPFrameIndex, sz, buf->cpuDataRef.size);
        bfd.buffer.Attach(createBuffer(sz));
        bfd.resourceSize = sz;
    }
    // Cache the actual data size in the per-in-flight-frame data as well.
    bfd.dataSize = buf->cpuDataRef.size;
}

void QSGD3D12EnginePrivate::updateBuffer(Buffer *buf)
{
    if (buf->cpuDataRef.dirty.isEmpty())
        return;

    Buffer::InFlightData &bfd(buf->d[currentPFrameIndex]);
    quint8 *p = nullptr;
    const D3D12_RANGE readRange = { 0, 0 };
    if (FAILED(bfd.buffer->Map(0, &readRange, reinterpret_cast<void **>(&p)))) {
        qWarning("Map failed for buffer of size %d", buf->cpuDataRef.size);
        return;
    }
    for (const auto &r : qAsConst(buf->cpuDataRef.dirty)) {
        if (Q_UNLIKELY(debug_buffer()))
            qDebug("%p o %d s %d", buf, r.first, r.second);
        memcpy(p + r.first, buf->cpuDataRef.p + r.first, r.second);
    }
    bfd.buffer->Unmap(0, nullptr);
    buf->cpuDataRef.dirty.clear();
}

void QSGD3D12EnginePrivate::ensureDevice()
{
    if (!initialized && window)
        initialize(window, windowSize, windowDpr, windowSamples, windowAlpha);
}

void QSGD3D12EnginePrivate::beginFrame()
{
    if (inFrame && !activeLayers)
        qFatal("beginFrame called again without an endFrame, frame index was %d", frameIndex);

    if (Q_UNLIKELY(debug_render()))
        qDebug() << "***** begin frame, logical" << frameIndex << "present" << presentFrameIndex << "layer" << activeLayers;

    if (inFrame && activeLayers) {
        if (Q_UNLIKELY(debug_render()))
            qDebug("frame %d already in progress", frameIndex);
        if (!currentLayerDepth) {
            // There are layers and the real frame preparation starts now. Prepare for present.
            beginFrameDraw();
        }
        return;
    }

    inFrame = true;

    // The device may have been lost. This is the point to attempt to start
    // again from scratch. Except when it is not. Operations that can happen
    // out of frame (e.g. textures, render targets) may trigger reinit earlier
    // than beginFrame.
    ensureDevice();

    // Wait for a buffer to be available for Present, if the waitable event is in use.
    if (waitableSwapChainMaxLatency)
        WaitForSingleObject(swapEvent, INFINITE);

    // Block if needed. With 2 frames in flight frame N waits for frame N - 2, but not N - 1, to finish.
    currentPFrameIndex = frameIndex % frameInFlightCount;
    if (frameIndex >= frameInFlightCount) {
        ID3D12Fence *fence = frameFence[currentPFrameIndex]->fence.Get();
        HANDLE event = frameFence[currentPFrameIndex]->event;
        // Frame fence values start from 1, hence the +1.
        const quint64 inFlightFenceValue = frameIndex - frameInFlightCount + 1;
        if (fence->GetCompletedValue() < inFlightFenceValue) {
            fence->SetEventOnCompletion(inFlightFenceValue, event);
            WaitForSingleObject(event, INFINITE);
        }
        frameCommandAllocator[currentPFrameIndex]->Reset();
    }

    PersistentFrameData &pfd(pframeData[currentPFrameIndex]);
    pfd.cbvSrvUavNextFreeDescriptorIndex = 0;

    for (Buffer &b : buffers) {
        if (b.entryInUse())
            b.d[currentPFrameIndex].dirty.clear();
    }

    if (frameIndex >= frameInFlightCount - 1) {
        // Now sync the buffer changes from the previous, potentially still in
        // flight, frames. This is done by taking the ranges dirtied in those
        // frames and adding them to the global CPU-side buffer's dirty list,
        // as if this frame changed those ranges. (however, dirty ranges
        // inherited this way are not added to this frame's persistent
        // per-frame dirty list because the next frame after this one should
        // inherit this frame's genuine changes only, the rest will come from
        // the earlier ones)
        for (int delta = frameInFlightCount - 1; delta >= 1; --delta) {
            const int prevPFrameIndex = (frameIndex - delta) % frameInFlightCount;
            PersistentFrameData &prevFrameData(pframeData[prevPFrameIndex]);
            for (uint id : qAsConst(prevFrameData.buffersUsedInFrame)) {
                Buffer &b(buffers[id - 1]);
                if (b.d[currentPFrameIndex].buffer && b.d[currentPFrameIndex].dataSize == b.cpuDataRef.size) {
                    if (Q_UNLIKELY(debug_buffer()))
                        qDebug() << "frame" << frameIndex << "takes dirty" << b.d[prevPFrameIndex].dirty
                                 << "from frame" << frameIndex - delta << "for buffer" << id;
                    for (const auto &range : qAsConst(b.d[prevPFrameIndex].dirty))
                        addDirtyRange(&b.cpuDataRef.dirty, range.first, range.second, b.cpuDataRef.size);
                } else {
                    if (Q_UNLIKELY(debug_buffer()))
                        qDebug() << "frame" << frameIndex << "makes all dirty from frame" << frameIndex - delta
                                 << "for buffer" << id;
                    addDirtyRange(&b.cpuDataRef.dirty, 0, b.cpuDataRef.size, b.cpuDataRef.size);
                }
            }
        }
    }

    if (frameIndex >= frameInFlightCount) {
        // Do some texture upload bookkeeping.
        const quint64 finishedFrameIndex = frameIndex - frameInFlightCount; // we know since we just blocked for this
        // pfd conveniently refers to the same slot that was used by that frame
        if (!pfd.pendingTextureUploads.isEmpty()) {
            if (Q_UNLIKELY(debug_texture()))
                qDebug("Removing texture upload data for frame %d", finishedFrameIndex);
            for (uint id : qAsConst(pfd.pendingTextureUploads)) {
                const int idx = id - 1;
                Texture &t(textures[idx]);
                // fenceValue is 0 when the previous frame cleared it, skip in
                // this case. Skip also when fenceValue > the value it was when
                // adding the last GPU wait - this is the case when more
                // uploads were queued for the same texture in the meantime.
                if (t.fenceValue && t.fenceValue == t.lastWaitFenceValue) {
                    t.fenceValue = 0;
                    t.lastWaitFenceValue = 0;
                    t.stagingBuffers.clear();
                    t.stagingHeaps.clear();
                    if (Q_UNLIKELY(debug_texture()))
                        qDebug("Cleaned staging data for texture %u", id);
                }
            }
            pfd.pendingTextureUploads.clear();
            if (!pfd.pendingTextureMipMap.isEmpty()) {
                if (Q_UNLIKELY(debug_texture()))
                    qDebug() << "cleaning mipmap generation data for " << pfd.pendingTextureMipMap;
                // no special cleanup is needed as mipmap generation uses the frame's resources
                pfd.pendingTextureMipMap.clear();
            }
            bool hasPending = false;
            for (int delta = 1; delta < frameInFlightCount; ++delta) {
                const PersistentFrameData &prevFrameData(pframeData[(frameIndex - delta) % frameInFlightCount]);
                if (!prevFrameData.pendingTextureUploads.isEmpty()) {
                    hasPending = true;
                    break;
                }
            }
            if (!hasPending) {
                if (Q_UNLIKELY(debug_texture()))
                    qDebug("no more pending textures");
                copyCommandAllocator->Reset();
            }
        }

        // Do the deferred deletes.
        if (!pfd.deleteQueue.isEmpty()) {
            for (PersistentFrameData::DeleteQueueEntry &e : pfd.deleteQueue) {
                e.res = nullptr;
                e.descHeap = nullptr;
                if (e.cpuDescriptorPtr) {
                    D3D12_CPU_DESCRIPTOR_HANDLE h = { e.cpuDescriptorPtr };
                    cpuDescHeapManager.release(h, e.descHeapType);
                }
            }
            pfd.deleteQueue.clear();
        }
        // Deferred deletes issued outside a begin-endFrame go to the next
        // frame's out-of-frame delete queue as these cannot be executed in the
        // next beginFrame, only in next + frameInFlightCount. Move to the
        // normal queue if this is the next beginFrame.
        if (!pfd.outOfFrameDeleteQueue.isEmpty()) {
            pfd.deleteQueue = pfd.outOfFrameDeleteQueue;
            pfd.outOfFrameDeleteQueue.clear();
        }

        // Mark released texture, buffer, etc. slots free.
        if (!pfd.pendingReleases.isEmpty()) {
            for (const auto &pr : qAsConst(pfd.pendingReleases)) {
                Q_ASSERT(pr.id);
                if (pr.type == PersistentFrameData::PendingRelease::TypeTexture) {
                    Texture &t(textures[pr.id - 1]);
                    Q_ASSERT(t.entryInUse());
                    t.flags &= ~RenderTarget::EntryInUse; // createTexture() can now reuse this entry
                    t.texture = nullptr;
                } else if (pr.type == PersistentFrameData::PendingRelease::TypeBuffer) {
                    Buffer &b(buffers[pr.id - 1]);
                    Q_ASSERT(b.entryInUse());
                    b.flags &= ~Buffer::EntryInUse;
                    for (int i = 0; i < frameInFlightCount; ++i)
                        b.d[i].buffer = nullptr;
                } else {
                    qFatal("Corrupt pending release list, type %d", pr.type);
                }
            }
            pfd.pendingReleases.clear();
        }
        if (!pfd.outOfFramePendingReleases.isEmpty()) {
            pfd.pendingReleases = pfd.outOfFramePendingReleases;
            pfd.outOfFramePendingReleases.clear();
        }
    }

    pfd.buffersUsedInFrame.clear();

    beginDrawCalls();

    // Prepare for present if this is a frame without layers.
    if (!activeLayers)
        beginFrameDraw();
}

void QSGD3D12EnginePrivate::beginDrawCalls()
{
    frameCommandList->Reset(frameCommandAllocator[frameIndex % frameInFlightCount].Get(), nullptr);
    commandList = frameCommandList.Get();
    invalidateCachedFrameState();
}

void QSGD3D12EnginePrivate::invalidateCachedFrameState()
{
    tframeData.drawingMode = QSGGeometry::DrawingMode(-1);
    tframeData.currentIndexBuffer = 0;
    tframeData.activeTextureCount = 0;
    tframeData.drawCount = 0;
    tframeData.lastPso = nullptr;
    tframeData.lastRootSig = nullptr;
    tframeData.descHeapSet = false;
}

void QSGD3D12EnginePrivate::restoreFrameState(bool minimal)
{
    queueSetRenderTarget(currentRenderTarget);
    if (!minimal) {
        queueViewport(tframeData.viewport);
        queueScissor(tframeData.scissor);
        queueSetBlendFactor(tframeData.blendFactor);
        queueSetStencilRef(tframeData.stencilRef);
    }
    finalizePipeline(tframeData.pipelineState);
}

void QSGD3D12EnginePrivate::beginFrameDraw()
{
    if (windowSamples == 1)
        transitionResource(defaultRT[presentFrameIndex % swapChainBufferCount].Get(), commandList,
                D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

void QSGD3D12EnginePrivate::endFrame()
{
    if (!inFrame)
        qFatal("endFrame called without beginFrame, frame index %d", frameIndex);

    if (Q_UNLIKELY(debug_render()))
        qDebug("***** end frame");

    endDrawCalls(true);

    commandQueue->Signal(frameFence[frameIndex % frameInFlightCount]->fence.Get(), frameIndex + 1);
    ++frameIndex;

    inFrame = false;
}

void QSGD3D12EnginePrivate::endDrawCalls(bool lastInFrame)
{
    PersistentFrameData &pfd(pframeData[currentPFrameIndex]);

    // Now is the time to sync all the changed areas in the buffers.
    if (Q_UNLIKELY(debug_buffer()))
        qDebug() << "buffers used in drawcall set" << pfd.buffersUsedInDrawCallSet;
    for (uint id : qAsConst(pfd.buffersUsedInDrawCallSet))
        updateBuffer(&buffers[id - 1]);

    pfd.buffersUsedInFrame += pfd.buffersUsedInDrawCallSet;
    pfd.buffersUsedInDrawCallSet.clear();

    // Add a wait on the 3D queue for the relevant texture uploads on the copy queue.
    if (!pfd.pendingTextureUploads.isEmpty()) {
        quint64 topFenceValue = 0;
        for (uint id : qAsConst(pfd.pendingTextureUploads)) {
            const int idx = id - 1;
            Texture &t(textures[idx]);
            Q_ASSERT(t.fenceValue);
            // skip if already added a Wait in the previous frame
            if (t.lastWaitFenceValue == t.fenceValue)
                continue;
            t.lastWaitFenceValue = t.fenceValue;
            if (t.fenceValue > topFenceValue)
                topFenceValue = t.fenceValue;
            if (t.mipmap())
                pfd.pendingTextureMipMap.insert(id);
        }
        if (topFenceValue) {
            if (Q_UNLIKELY(debug_texture()))
                qDebug("added wait for texture fence %llu", topFenceValue);
            commandQueue->Wait(textureUploadFence.Get(), topFenceValue);
            // Generate mipmaps after the wait, when necessary.
            if (!pfd.pendingTextureMipMap.isEmpty()) {
                if (Q_UNLIKELY(debug_texture()))
                    qDebug() << "starting mipmap generation for" << pfd.pendingTextureMipMap;
                for (uint id : qAsConst(pfd.pendingTextureMipMap))
                    mipmapper.queueGenerate(textures[id - 1]);
            }
        }
    }

    if (lastInFrame) {
        // Resolve and transition the backbuffer for present, if needed.
        const int idx = presentFrameIndex % swapChainBufferCount;
        if (windowSamples == 1) {
            transitionResource(defaultRT[idx].Get(), commandList,
                               D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        } else {
            if (Q_UNLIKELY(debug_render())) {
                const D3D12_RESOURCE_DESC desc = defaultRT[idx]->GetDesc();
                qDebug("added resolve for multisampled render target (count %d, quality %d)",
                       desc.SampleDesc.Count, desc.SampleDesc.Quality);
            }
            resolveMultisampledTarget(defaultRT[idx].Get(), backBufferRT[idx].Get(),
                                      D3D12_RESOURCE_STATE_PRESENT, commandList);
        }

        if (activeLayers) {
            if (Q_UNLIKELY(debug_render()))
                qDebug("this frame had %d layers", activeLayers);
            activeLayers = 0;
        }
    }

    // Go!
    HRESULT hr = frameCommandList->Close();
    if (FAILED(hr)) {
        qWarning("Failed to close command list: %s", qPrintable(comErrorMessage(hr)));
        if (hr == E_INVALIDARG)
            qWarning("Invalid arguments. Some of the commands in the list is invalid in some way.");
    }

    ID3D12CommandList *commandLists[] = { frameCommandList.Get() };
    commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

    commandList = nullptr;
}

void QSGD3D12EnginePrivate::beginLayer()
{
    if (inFrame && !activeLayers)
        qFatal("Layer rendering cannot be started while a frame is active");

    if (Q_UNLIKELY(debug_render()))
        qDebug("===== beginLayer active %d depth %d (inFrame=%d)", activeLayers, currentLayerDepth, inFrame);

    ++activeLayers;
    ++currentLayerDepth;

    // Do an early beginFrame. With multiple layers this results in
    // beginLayer - beginFrame - endLayer - beginLayer - beginFrame - endLayer - ... - (*) beginFrame - endFrame
    // where (*) denotes the start of the preparation of the actual, non-layer frame.

    if (activeLayers == 1)
        beginFrame();
}

void QSGD3D12EnginePrivate::endLayer()
{
    if (!inFrame || !activeLayers || !currentLayerDepth)
        qFatal("Mismatched endLayer");

    if (Q_UNLIKELY(debug_render()))
        qDebug("===== endLayer active %d depth %d", activeLayers, currentLayerDepth);

    --currentLayerDepth;

    // Do not touch activeLayers. It remains valid until endFrame.
}

// Root signature:
// [0] CBV - always present
// [1] table with one SRV per texture (must be a table since root descriptor SRVs cannot be textures) - optional
// one static sampler per texture - optional
//
// SRVs can be created freely via QSGD3D12CPUDescriptorHeapManager and stored
// in QSGD3D12TextureView. The engine will copy them onto a dedicated,
// shader-visible CBV-SRV-UAV heap in the correct order.

void QSGD3D12EnginePrivate::finalizePipeline(const QSGD3D12PipelineState &pipelineState)
{
    if (!inFrame) {
        qWarning("%s: Cannot be called outside begin/endFrame", __FUNCTION__);
        return;
    }

    tframeData.pipelineState = pipelineState;

    RootSigCacheEntry *cachedRootSig = rootSigCache[pipelineState.shaders.rootSig];
    if (!cachedRootSig) {
        if (Q_UNLIKELY(debug_render()))
            qDebug("NEW ROOTSIG");

        cachedRootSig = new RootSigCacheEntry;

        D3D12_ROOT_PARAMETER rootParams[4];
        int rootParamCount = 0;

        rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParams[0].Descriptor.ShaderRegister = 0; // b0
        rootParams[0].Descriptor.RegisterSpace = 0;
        ++rootParamCount;

        D3D12_DESCRIPTOR_RANGE tvDescRange;
        if (pipelineState.shaders.rootSig.textureViewCount > 0) {
            rootParams[rootParamCount].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            rootParams[rootParamCount].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
            rootParams[rootParamCount].DescriptorTable.NumDescriptorRanges = 1;
            tvDescRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            tvDescRange.NumDescriptors = pipelineState.shaders.rootSig.textureViewCount;
            tvDescRange.BaseShaderRegister = 0; // t0, t1, ...
            tvDescRange.RegisterSpace = 0;
            tvDescRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
            rootParams[rootParamCount].DescriptorTable.pDescriptorRanges = &tvDescRange;
            ++rootParamCount;
        }

        Q_ASSERT(rootParamCount <= _countof(rootParams));
        D3D12_ROOT_SIGNATURE_DESC desc;
        desc.NumParameters = rootParamCount;
        desc.pParameters = rootParams;
        // Mixing up samplers and resource views in QSGD3D12TextureView means
        // that the number of static samplers has to match the number of
        // textures. This is not really ideal in general but works for Quick's use cases.
        // The shaders can still choose to declare and use fewer samplers, if they want to.
        desc.NumStaticSamplers = pipelineState.shaders.rootSig.textureViewCount;
        D3D12_STATIC_SAMPLER_DESC staticSamplers[8];
        int sdIdx = 0;
        Q_ASSERT(pipelineState.shaders.rootSig.textureViewCount <= _countof(staticSamplers));
        for (int i = 0; i < pipelineState.shaders.rootSig.textureViewCount; ++i) {
            const QSGD3D12TextureView &tv(pipelineState.shaders.rootSig.textureViews[i]);
            D3D12_STATIC_SAMPLER_DESC sd = {};
            sd.Filter = D3D12_FILTER(tv.filter);
            sd.AddressU = D3D12_TEXTURE_ADDRESS_MODE(tv.addressModeHoriz);
            sd.AddressV = D3D12_TEXTURE_ADDRESS_MODE(tv.addressModeVert);
            sd.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            sd.MinLOD = 0.0f;
            sd.MaxLOD = D3D12_FLOAT32_MAX;
            sd.ShaderRegister = sdIdx; // t0, t1, ...
            sd.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
            staticSamplers[sdIdx++] = sd;
        }
        desc.pStaticSamplers = staticSamplers;
        desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error))) {
            QByteArray msg(static_cast<const char *>(error->GetBufferPointer()), error->GetBufferSize());
            qWarning("Failed to serialize root signature: %s", qPrintable(msg));
            return;
        }
        if (FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                               IID_PPV_ARGS(&cachedRootSig->rootSig)))) {
            qWarning("Failed to create root signature");
            return;
        }

        rootSigCache.insert(pipelineState.shaders.rootSig, cachedRootSig);
    }

    PSOCacheEntry *cachedPso = psoCache[pipelineState];
    if (!cachedPso) {
        if (Q_UNLIKELY(debug_render()))
            qDebug("NEW PSO");

        cachedPso = new PSOCacheEntry;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

        D3D12_INPUT_ELEMENT_DESC inputElements[QSGD3D12_MAX_INPUT_ELEMENTS];
        int ieIdx = 0;
        for (int i = 0; i < pipelineState.inputElementCount; ++i) {
            const QSGD3D12InputElement &ie(pipelineState.inputElements[i]);
            D3D12_INPUT_ELEMENT_DESC ieDesc = {};
            ieDesc.SemanticName = ie.semanticName;
            ieDesc.SemanticIndex = ie.semanticIndex;
            ieDesc.Format = DXGI_FORMAT(ie.format);
            ieDesc.InputSlot = ie.slot;
            ieDesc.AlignedByteOffset = ie.offset;
            ieDesc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            if (Q_UNLIKELY(debug_render()))
                qDebug("input [%d]: %s %d 0x%x %d", ieIdx, ie.semanticName, ie.offset, ie.format, ie.slot);
            inputElements[ieIdx++] = ieDesc;
        }

        psoDesc.InputLayout = { inputElements, UINT(ieIdx) };

        psoDesc.pRootSignature = cachedRootSig->rootSig.Get();

        D3D12_SHADER_BYTECODE vshader;
        vshader.pShaderBytecode = pipelineState.shaders.vs;
        vshader.BytecodeLength = pipelineState.shaders.vsSize;
        D3D12_SHADER_BYTECODE pshader;
        pshader.pShaderBytecode = pipelineState.shaders.ps;
        pshader.BytecodeLength = pipelineState.shaders.psSize;

        psoDesc.VS = vshader;
        psoDesc.PS = pshader;

        D3D12_RASTERIZER_DESC rastDesc = {};
        rastDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rastDesc.CullMode = D3D12_CULL_MODE(pipelineState.cullMode);
        rastDesc.FrontCounterClockwise = pipelineState.frontCCW;
        rastDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        rastDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        rastDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        rastDesc.DepthClipEnable = TRUE;

        psoDesc.RasterizerState = rastDesc;

        D3D12_BLEND_DESC blendDesc = {};
        if (pipelineState.blend == QSGD3D12PipelineState::BlendNone) {
            D3D12_RENDER_TARGET_BLEND_DESC noBlendDesc = {};
            noBlendDesc.RenderTargetWriteMask = pipelineState.colorWrite ? D3D12_COLOR_WRITE_ENABLE_ALL : 0;
            blendDesc.RenderTarget[0] = noBlendDesc;
        } else if (pipelineState.blend == QSGD3D12PipelineState::BlendPremul) {
            const D3D12_RENDER_TARGET_BLEND_DESC premulBlendDesc = {
                TRUE, FALSE,
                D3D12_BLEND_ONE, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD,
                D3D12_BLEND_ONE, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD,
                D3D12_LOGIC_OP_NOOP,
                UINT8(pipelineState.colorWrite ? D3D12_COLOR_WRITE_ENABLE_ALL : 0)
            };
            blendDesc.RenderTarget[0] = premulBlendDesc;
        } else if (pipelineState.blend == QSGD3D12PipelineState::BlendColor) {
            const D3D12_RENDER_TARGET_BLEND_DESC colorBlendDesc = {
                TRUE, FALSE,
                D3D12_BLEND_BLEND_FACTOR, D3D12_BLEND_INV_SRC_COLOR, D3D12_BLEND_OP_ADD,
                D3D12_BLEND_BLEND_FACTOR, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD,
                D3D12_LOGIC_OP_NOOP,
                UINT8(pipelineState.colorWrite ? D3D12_COLOR_WRITE_ENABLE_ALL : 0)
            };
            blendDesc.RenderTarget[0] = colorBlendDesc;
        }
        psoDesc.BlendState = blendDesc;

        psoDesc.DepthStencilState.DepthEnable = pipelineState.depthEnable;
        psoDesc.DepthStencilState.DepthWriteMask = pipelineState.depthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC(pipelineState.depthFunc);

        psoDesc.DepthStencilState.StencilEnable = pipelineState.stencilEnable;
        psoDesc.DepthStencilState.StencilReadMask = psoDesc.DepthStencilState.StencilWriteMask = 0xFF;
        D3D12_DEPTH_STENCILOP_DESC stencilOpDesc = {
            D3D12_STENCIL_OP(pipelineState.stencilFailOp),
            D3D12_STENCIL_OP(pipelineState.stencilDepthFailOp),
            D3D12_STENCIL_OP(pipelineState.stencilPassOp),
            D3D12_COMPARISON_FUNC(pipelineState.stencilFunc)
        };
        psoDesc.DepthStencilState.FrontFace = psoDesc.DepthStencilState.BackFace = stencilOpDesc;

        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE(pipelineState.topologyType);
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = RT_COLOR_FORMAT;
        psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        psoDesc.SampleDesc = defaultRT[0]->GetDesc().SampleDesc;

        HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&cachedPso->pso));
        if (FAILED(hr)) {
            qWarning("Failed to create graphics pipeline state: %s",
                     qPrintable(comErrorMessage(hr)));
            return;
        }

        psoCache.insert(pipelineState, cachedPso);
    }

    if (cachedPso->pso.Get() != tframeData.lastPso) {
        tframeData.lastPso = cachedPso->pso.Get();
        commandList->SetPipelineState(tframeData.lastPso);
    }

    if (cachedRootSig->rootSig.Get() != tframeData.lastRootSig) {
        tframeData.lastRootSig = cachedRootSig->rootSig.Get();
        commandList->SetGraphicsRootSignature(tframeData.lastRootSig);
    }

    if (pipelineState.shaders.rootSig.textureViewCount > 0)
        setDescriptorHeaps();
}

void QSGD3D12EnginePrivate::setDescriptorHeaps(bool force)
{
    if (force || !tframeData.descHeapSet) {
        tframeData.descHeapSet = true;
        ID3D12DescriptorHeap *heaps[] = { pframeData[currentPFrameIndex].gpuCbvSrvUavHeap.Get() };
        commandList->SetDescriptorHeaps(_countof(heaps), heaps);
    }
}

void QSGD3D12EnginePrivate::queueViewport(const QRect &rect)
{
    if (!inFrame) {
        qWarning("%s: Cannot be called outside begin/endFrame", __FUNCTION__);
        return;
    }

    tframeData.viewport = rect;
    const D3D12_VIEWPORT viewport = { float(rect.x()), float(rect.y()), float(rect.width()), float(rect.height()), 0, 1 };
    commandList->RSSetViewports(1, &viewport);
}

void QSGD3D12EnginePrivate::queueScissor(const QRect &rect)
{
    if (!inFrame) {
        qWarning("%s: Cannot be called outside begin/endFrame", __FUNCTION__);
        return;
    }

    tframeData.scissor = rect;
    const D3D12_RECT scissorRect = { rect.x(), rect.y(), rect.x() + rect.width(), rect.y() + rect.height() };
    commandList->RSSetScissorRects(1, &scissorRect);
}

void QSGD3D12EnginePrivate::queueSetRenderTarget(uint id)
{
    if (!inFrame) {
        qWarning("%s: Cannot be called outside begin/endFrame", __FUNCTION__);
        return;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle;

    if (!id) {
        rtvHandle = defaultRTV[presentFrameIndex % swapChainBufferCount];
        dsvHandle = defaultDSV;
    } else {
        const int idx = id - 1;
        Q_ASSERT(idx < renderTargets.count() && renderTargets[idx].entryInUse());
        RenderTarget &rt(renderTargets[idx]);
        rtvHandle = rt.rtv;
        dsvHandle = rt.dsv;
        if (!(rt.flags & RenderTarget::NeedsReadBarrier)) {
            rt.flags |= RenderTarget::NeedsReadBarrier;
            if (!(rt.flags & RenderTarget::Multisample))
                transitionResource(rt.color.Get(), commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                   D3D12_RESOURCE_STATE_RENDER_TARGET);
        }
    }

    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    currentRenderTarget = id;
}

void QSGD3D12EnginePrivate::queueClearRenderTarget(const QColor &color)
{
    if (!inFrame) {
        qWarning("%s: Cannot be called outside begin/endFrame", __FUNCTION__);
        return;
    }

    const float clearColor[] = { float(color.redF()), float(color.blueF()), float(color.greenF()), float(color.alphaF()) };
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = !currentRenderTarget
            ? defaultRTV[presentFrameIndex % swapChainBufferCount]
            : renderTargets[currentRenderTarget - 1].rtv;
    commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
}

void QSGD3D12EnginePrivate::queueClearDepthStencil(float depthValue, quint8 stencilValue, QSGD3D12Engine::ClearFlags which)
{
    if (!inFrame) {
        qWarning("%s: Cannot be called outside begin/endFrame", __FUNCTION__);
        return;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE dsv = !currentRenderTarget
            ? defaultDSV
            : renderTargets[currentRenderTarget - 1].dsv;
    commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAGS(int(which)), depthValue, stencilValue, 0, nullptr);
}

void QSGD3D12EnginePrivate::queueSetBlendFactor(const QVector4D &factor)
{
    if (!inFrame) {
        qWarning("%s: Cannot be called outside begin/endFrame", __FUNCTION__);
        return;
    }

    tframeData.blendFactor = factor;
    const float f[4] = { factor.x(), factor.y(), factor.z(), factor.w() };
    commandList->OMSetBlendFactor(f);
}

void QSGD3D12EnginePrivate::queueSetStencilRef(quint32 ref)
{
    if (!inFrame) {
        qWarning("%s: Cannot be called outside begin/endFrame", __FUNCTION__);
        return;
    }

    tframeData.stencilRef = ref;
    commandList->OMSetStencilRef(ref);
}

void QSGD3D12EnginePrivate::queueDraw(const QSGD3D12Engine::DrawParams &params)
{
    if (!inFrame) {
        qWarning("%s: Cannot be called outside begin/endFrame", __FUNCTION__);
        return;
    }

    const bool skip = tframeData.scissor.isEmpty();

    PersistentFrameData &pfd(pframeData[currentPFrameIndex]);

    pfd.buffersUsedInDrawCallSet.insert(params.vertexBuf);
    const int vertexBufIdx = params.vertexBuf - 1;
    Q_ASSERT(params.vertexBuf && vertexBufIdx < buffers.count() && buffers[vertexBufIdx].entryInUse());
    pfd.buffersUsedInDrawCallSet.insert(params.constantBuf);
    const int constantBufIdx = params.constantBuf - 1;
    Q_ASSERT(params.constantBuf && constantBufIdx < buffers.count() && buffers[constantBufIdx].entryInUse());
    int indexBufIdx = -1;
    if (params.indexBuf) {
        pfd.buffersUsedInDrawCallSet.insert(params.indexBuf);
        indexBufIdx = params.indexBuf - 1;
        Q_ASSERT(indexBufIdx < buffers.count() && buffers[indexBufIdx].entryInUse());
    }

    // Ensure buffers are created but do not copy the data here, leave that to endDrawCalls().
    ensureBuffer(&buffers[vertexBufIdx]);
    ensureBuffer(&buffers[constantBufIdx]);
    if (indexBufIdx >= 0)
        ensureBuffer(&buffers[indexBufIdx]);

    // Set the CBV.
    if (!skip && params.cboOffset >= 0) {
        ID3D12Resource *cbuf = buffers[constantBufIdx].d[currentPFrameIndex].buffer.Get();
        if (cbuf)
            commandList->SetGraphicsRootConstantBufferView(0, cbuf->GetGPUVirtualAddress() + params.cboOffset);
    }

    // Set up vertex and index buffers.
    ID3D12Resource *vbuf = buffers[vertexBufIdx].d[currentPFrameIndex].buffer.Get();
    ID3D12Resource *ibuf = indexBufIdx >= 0 && params.startIndexIndex >= 0
            ? buffers[indexBufIdx].d[currentPFrameIndex].buffer.Get() : nullptr;

    if (!skip && params.mode != tframeData.drawingMode) {
        D3D_PRIMITIVE_TOPOLOGY topology;
        switch (params.mode) {
        case QSGGeometry::DrawPoints:
            topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
            break;
        case QSGGeometry::DrawLines:
            topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
            break;
        case QSGGeometry::DrawLineStrip:
            topology = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
            break;
        case QSGGeometry::DrawTriangles:
            topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            break;
        case QSGGeometry::DrawTriangleStrip:
            topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
            break;
        default:
            qFatal("Unsupported drawing mode 0x%x", params.mode);
            break;
        }
        commandList->IASetPrimitiveTopology(topology);
        tframeData.drawingMode = params.mode;
    }

    if (!skip) {
        D3D12_VERTEX_BUFFER_VIEW vbv;
        vbv.BufferLocation = vbuf->GetGPUVirtualAddress() + params.vboOffset;
        vbv.SizeInBytes = params.vboSize;
        vbv.StrideInBytes = params.vboStride;

        // must be set after the topology
        commandList->IASetVertexBuffers(0, 1, &vbv);
    }

    if (!skip && params.startIndexIndex >= 0 && ibuf && tframeData.currentIndexBuffer != params.indexBuf) {
        tframeData.currentIndexBuffer = params.indexBuf;
        D3D12_INDEX_BUFFER_VIEW ibv;
        ibv.BufferLocation = ibuf->GetGPUVirtualAddress();
        ibv.SizeInBytes = buffers[indexBufIdx].cpuDataRef.size;
        ibv.Format = DXGI_FORMAT(params.indexFormat);
        commandList->IASetIndexBuffer(&ibv);
    }

    // Copy the SRVs to a drawcall-dedicated area of the shader-visible descriptor heap.
    Q_ASSERT(tframeData.activeTextureCount == tframeData.pipelineState.shaders.rootSig.textureViewCount);
    if (tframeData.activeTextureCount > 0) {
        if (!skip) {
            ensureGPUDescriptorHeap(tframeData.activeTextureCount);
            const uint stride = cpuDescHeapManager.handleSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            D3D12_CPU_DESCRIPTOR_HANDLE dst = pfd.gpuCbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
            dst.ptr += pfd.cbvSrvUavNextFreeDescriptorIndex * stride;
            for (int i = 0; i < tframeData.activeTextureCount; ++i) {
                const TransientFrameData::ActiveTexture &t(tframeData.activeTextures[i]);
                Q_ASSERT(t.id);
                const int idx = t.id - 1;
                const bool isTex = t.type == TransientFrameData::ActiveTexture::TypeTexture;
                device->CopyDescriptorsSimple(1, dst, isTex ? textures[idx].srv : renderTargets[idx].srv,
                                              D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                dst.ptr += stride;
            }

            D3D12_GPU_DESCRIPTOR_HANDLE gpuAddr = pfd.gpuCbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
            gpuAddr.ptr += pfd.cbvSrvUavNextFreeDescriptorIndex * stride;
            commandList->SetGraphicsRootDescriptorTable(1, gpuAddr);

            pfd.cbvSrvUavNextFreeDescriptorIndex += tframeData.activeTextureCount;
        }
        tframeData.activeTextureCount = 0;
    }

    // Add the draw call.
    if (!skip) {
        ++tframeData.drawCount;
        if (params.startIndexIndex >= 0)
            commandList->DrawIndexedInstanced(params.count, 1, params.startIndexIndex, 0, 0);
        else
            commandList->DrawInstanced(params.count, 1, 0, 0);
    }

    if (tframeData.drawCount == MAX_DRAW_CALLS_PER_LIST) {
        if (Q_UNLIKELY(debug_render()))
            qDebug("Limit of %d draw calls reached, executing command list", MAX_DRAW_CALLS_PER_LIST);
        // submit the command list
        endDrawCalls();
        // start a new one
        beginDrawCalls();
        // prepare for the upcoming drawcalls
        restoreFrameState();
    }
}

void QSGD3D12EnginePrivate::ensureGPUDescriptorHeap(int cbvSrvUavDescriptorCount)
{
    PersistentFrameData &pfd(pframeData[currentPFrameIndex]);
    int newSize = pfd.gpuCbvSrvUavHeapSize;
    while (pfd.cbvSrvUavNextFreeDescriptorIndex + cbvSrvUavDescriptorCount > newSize)
        newSize *= 2;
    if (newSize != pfd.gpuCbvSrvUavHeapSize) {
        if (Q_UNLIKELY(debug_descheap()))
            qDebug("Out of space for SRVs, creating new CBV-SRV-UAV descriptor heap with descriptor count %d", newSize);
        deferredDelete(pfd.gpuCbvSrvUavHeap);
        createCbvSrvUavHeap(currentPFrameIndex, newSize);
        setDescriptorHeaps(true);
        pfd.cbvSrvUavNextFreeDescriptorIndex = 0;
    }
}

void QSGD3D12EnginePrivate::present()
{
    if (!initialized)
        return;

    if (Q_UNLIKELY(debug_render()))
        qDebug("--- present with vsync ---");

    // This call will not block the CPU unless at least 3 buffers are queued,
    // unless the waitable frame latency event is enabled. Then the latency of
    // 3 is changed to whatever value desired, and blocking happens in
    // beginFrame. If none of these hold, the fence-based wait in beginFrame
    // throttles. Vsync (interval 1) is always enabled.
    HRESULT hr = swapChain->Present(1, 0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        deviceManager()->deviceLossDetected();
        return;
    } else if (FAILED(hr)) {
        qWarning("Present failed: %s", qPrintable(comErrorMessage(hr)));
        return;
    }

#ifndef Q_OS_WINRT
    if (dcompDevice)
        dcompDevice->Commit();
#endif

    ++presentFrameIndex;
}

void QSGD3D12EnginePrivate::waitGPU()
{
    if (!initialized)
        return;

    if (Q_UNLIKELY(debug_render()))
        qDebug("--- blocking wait for GPU ---");

    waitForGPU(presentFence);
}

template<class T> uint newId(T *tbl)
{
    uint id = 0;
    for (int i = 0; i < tbl->count(); ++i) {
        if (!(*tbl)[i].entryInUse()) {
            id = i + 1;
            break;
        }
    }

    if (!id) {
        tbl->resize(tbl->size() + 1);
        id = tbl->count();
    }

    (*tbl)[id - 1].flags = 0x01; // reset flags and set EntryInUse

    return id;
}

template<class T> void syncEntryFlags(T *e, int flag, bool b)
{
    if (b)
        e->flags |= flag;
    else
        e->flags &= ~flag;
}

uint QSGD3D12EnginePrivate::genBuffer()
{
    return newId(&buffers);
}

void QSGD3D12EnginePrivate::releaseBuffer(uint id)
{
    if (!id || !initialized)
        return;

    const int idx = id - 1;
    Q_ASSERT(idx < buffers.count());

    if (Q_UNLIKELY(debug_buffer()))
        qDebug("releasing buffer %u", id);

    Buffer &b(buffers[idx]);
    if (!b.entryInUse())
        return;

    // Do not null out and do not mark the entry reusable yet.
    // Do that only when the frames potentially in flight have finished for sure.

    for (int i = 0; i < frameInFlightCount; ++i) {
        if (b.d[i].buffer)
            deferredDelete(b.d[i].buffer);
    }

    QSet<PersistentFrameData::PendingRelease> *pendingReleasesSet = inFrame
            ? &pframeData[currentPFrameIndex].pendingReleases
            : &pframeData[(currentPFrameIndex + 1) % frameInFlightCount].outOfFramePendingReleases;

    pendingReleasesSet->insert(PersistentFrameData::PendingRelease(PersistentFrameData::PendingRelease::TypeBuffer, id));
}

void QSGD3D12EnginePrivate::resetBuffer(uint id, const quint8 *data, int size)
{
    if (!inFrame) {
        qWarning("%s: Cannot be called outside begin/endFrame", __FUNCTION__);
        return;
    }

    Q_ASSERT(id);
    const int idx = id - 1;
    Q_ASSERT(idx < buffers.count() && buffers[idx].entryInUse());
    Buffer &b(buffers[idx]);

    if (Q_UNLIKELY(debug_buffer()))
        qDebug("reset buffer %u, size %d", id, size);

    b.cpuDataRef.p = data;
    b.cpuDataRef.size = size;

    b.cpuDataRef.dirty.clear();
    b.d[currentPFrameIndex].dirty.clear();

    if (size > 0) {
        const QPair<int, int> range = qMakePair(0, size);
        b.cpuDataRef.dirty.append(range);
        b.d[currentPFrameIndex].dirty.append(range);
    }
}

void QSGD3D12EnginePrivate::addDirtyRange(DirtyList *dirty, int offset, int size, int bufferSize)
{
    // Bail out when the dirty list already spans the entire buffer.
    if (!dirty->isEmpty()) {
        if (dirty->at(0).first == 0 && dirty->at(0).second == bufferSize)
            return;
    }

    const QPair<int, int> range = qMakePair(offset, size);
    if (!dirty->contains(range))
        dirty->append(range);
}

void QSGD3D12EnginePrivate::markBufferDirty(uint id, int offset, int size)
{
    if (!inFrame) {
        qWarning("%s: Cannot be called outside begin/endFrame", __FUNCTION__);
        return;
    }

    Q_ASSERT(id);
    const int idx = id - 1;
    Q_ASSERT(idx < buffers.count() && buffers[idx].entryInUse());
    Buffer &b(buffers[idx]);

    addDirtyRange(&b.cpuDataRef.dirty, offset, size, b.cpuDataRef.size);
    addDirtyRange(&b.d[currentPFrameIndex].dirty, offset, size, b.cpuDataRef.size);
}

uint QSGD3D12EnginePrivate::genTexture()
{
    const uint id = newId(&textures);
    textures[id - 1].fenceValue = 0;
    return id;
}

static inline DXGI_FORMAT textureFormat(QImage::Format format, bool wantsAlpha, bool mipmap, bool force32bit,
                                        QImage::Format *imageFormat, int *bytesPerPixel)
{
    DXGI_FORMAT f = DXGI_FORMAT_R8G8B8A8_UNORM;
    QImage::Format convFormat = format;
    int bpp = 4;

    if (!mipmap) {
        switch (format) {
        case QImage::Format_Grayscale8:
        case QImage::Format_Indexed8:
        case QImage::Format_Alpha8:
            if (!force32bit) {
                f = DXGI_FORMAT_R8_UNORM;
                bpp = 1;
            } else {
                convFormat = QImage::Format_RGBA8888;
            }
            break;
        case QImage::Format_RGB32:
            f = DXGI_FORMAT_B8G8R8A8_UNORM;
            break;
        case QImage::Format_ARGB32:
            f = DXGI_FORMAT_B8G8R8A8_UNORM;
            convFormat = wantsAlpha ? QImage::Format_ARGB32_Premultiplied : QImage::Format_RGB32;
            break;
        case QImage::Format_ARGB32_Premultiplied:
            f = DXGI_FORMAT_B8G8R8A8_UNORM;
            convFormat = wantsAlpha ? format : QImage::Format_RGB32;
            break;
        default:
            convFormat = wantsAlpha ? QImage::Format_RGBA8888_Premultiplied : QImage::Format_RGBX8888;
            break;
        }
    } else {
        // Mipmap generation needs unordered access and BGRA is not an option for that. Stick to RGBA.
        convFormat = wantsAlpha ? QImage::Format_RGBA8888_Premultiplied : QImage::Format_RGBX8888;
    }

    if (imageFormat)
        *imageFormat = convFormat;

    if (bytesPerPixel)
        *bytesPerPixel = bpp;

    return f;
}

static inline QImage::Format imageFormatForTexture(DXGI_FORMAT format)
{
    QImage::Format f = QImage::Format_Invalid;

    switch (format) {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
        f = QImage::Format_RGBA8888_Premultiplied;
        break;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
        f = QImage::Format_ARGB32_Premultiplied;
        break;
    case DXGI_FORMAT_R8_UNORM:
        f = QImage::Format_Grayscale8;
        break;
    default:
        break;
    }

    return f;
}

void QSGD3D12EnginePrivate::createTexture(uint id, const QSize &size, QImage::Format format,
                                          QSGD3D12Engine::TextureCreateFlags createFlags)
{
    ensureDevice();

    Q_ASSERT(id);
    const int idx = id - 1;
    Q_ASSERT(idx < textures.count() && textures[idx].entryInUse());
    Texture &t(textures[idx]);

    syncEntryFlags(&t, Texture::Alpha, createFlags & QSGD3D12Engine::TextureWithAlpha);
    syncEntryFlags(&t, Texture::MipMap, createFlags & QSGD3D12Engine::TextureWithMipMaps);

    const QSize adjustedSize = !t.mipmap() ? size : QSGD3D12Engine::mipMapAdjustedSourceSize(size);

    D3D12_HEAP_PROPERTIES defaultHeapProp = {};
    defaultHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = adjustedSize.width();
    textureDesc.Height = adjustedSize.height();
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = !t.mipmap() ? 1 : QSGD3D12Engine::mipMapLevels(adjustedSize);
    textureDesc.Format = textureFormat(format, t.alpha(), t.mipmap(),
                                       createFlags.testFlag(QSGD3D12Engine::TextureAlways32Bit),
                                       nullptr, nullptr);
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    if (t.mipmap())
        textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    HRESULT hr = device->CreateCommittedResource(&defaultHeapProp, D3D12_HEAP_FLAG_NONE, &textureDesc,
                                                 D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&t.texture));
    if (FAILED(hr)) {
        qWarning("Failed to create texture resource: %s", qPrintable(comErrorMessage(hr)));
        return;
    }

    t.srv = cpuDescHeapManager.allocate(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = textureDesc.MipLevels;

    device->CreateShaderResourceView(t.texture.Get(), &srvDesc, t.srv);

    if (t.mipmap()) {
        // Mipmap generation will need an UAV for each level that needs to be generated.
        t.mipUAVs.clear();
        for (int level = 1; level < textureDesc.MipLevels; ++level) {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.Format = textureDesc.Format;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            uavDesc.Texture2D.MipSlice = level;
            D3D12_CPU_DESCRIPTOR_HANDLE h = cpuDescHeapManager.allocate(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            device->CreateUnorderedAccessView(t.texture.Get(), nullptr, &uavDesc, h);
            t.mipUAVs.append(h);
        }
    }

    if (Q_UNLIKELY(debug_texture()))
        qDebug("created texture %u, size %dx%d, miplevels %d", id, adjustedSize.width(), adjustedSize.height(), textureDesc.MipLevels);
}

void QSGD3D12EnginePrivate::queueTextureResize(uint id, const QSize &size)
{
    Q_ASSERT(id);
    const int idx = id - 1;
    Q_ASSERT(idx < textures.count() && textures[idx].entryInUse());
    Texture &t(textures[idx]);

    if (!t.texture) {
        qWarning("Cannot resize non-created texture %u", id);
        return;
    }

    if (t.mipmap()) {
        qWarning("Cannot resize mipmapped texture %u", id);
        return;
    }

    if (Q_UNLIKELY(debug_texture()))
        qDebug("resizing texture %u, size %dx%d", id, size.width(), size.height());

    D3D12_RESOURCE_DESC textureDesc = t.texture->GetDesc();
    textureDesc.Width = size.width();
    textureDesc.Height = size.height();

    D3D12_HEAP_PROPERTIES defaultHeapProp = {};
    defaultHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;

    ComPtr<ID3D12Resource> oldTexture = t.texture;
    deferredDelete(t.texture);

    HRESULT hr = device->CreateCommittedResource(&defaultHeapProp, D3D12_HEAP_FLAG_NONE, &textureDesc,
                                                 D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&t.texture));
    if (FAILED(hr)) {
        qWarning("Failed to create resized texture resource: %s",
                 qPrintable(comErrorMessage(hr)));
        return;
    }

    deferredDelete(t.srv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    t.srv = cpuDescHeapManager.allocate(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = textureDesc.MipLevels;

    device->CreateShaderResourceView(t.texture.Get(), &srvDesc, t.srv);

    D3D12_TEXTURE_COPY_LOCATION dstLoc;
    dstLoc.pResource = t.texture.Get();
    dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLoc.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION srcLoc;
    srcLoc.pResource = oldTexture.Get();
    srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    srcLoc.SubresourceIndex = 0;

    copyCommandList->Reset(copyCommandAllocator.Get(), nullptr);

    copyCommandList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

    copyCommandList->Close();
    ID3D12CommandList *commandLists[] = { copyCommandList.Get() };
    copyCommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

    t.fenceValue = nextTextureUploadFenceValue.fetchAndAddAcquire(1) + 1;
    copyCommandQueue->Signal(textureUploadFence.Get(), t.fenceValue);

    if (Q_UNLIKELY(debug_texture()))
        qDebug("submitted old content copy for texture %u on the copy queue, fence %llu", id, t.fenceValue);
}

void QSGD3D12EnginePrivate::queueTextureUpload(uint id, const QVector<QImage> &images, const QVector<QPoint> &dstPos,
                                               QSGD3D12Engine::TextureUploadFlags flags)
{
    Q_ASSERT(id);
    Q_ASSERT(images.count() == dstPos.count());
    if (images.isEmpty())
        return;

    const int idx = id - 1;
    Q_ASSERT(idx < textures.count() && textures[idx].entryInUse());
    Texture &t(textures[idx]);
    Q_ASSERT(t.texture);

    // When mipmapping is not in use, image can be smaller than the size passed
    // to createTexture() and dstPos can specify a non-zero destination position.

    if (t.mipmap() && (images.count() != 1 || dstPos.count() != 1 || !dstPos[0].isNull())) {
        qWarning("Mipmapped textures (%u) do not support partial uploads", id);
        return;
    }

    // Make life simpler by disallowing queuing a new mipmapped upload before the previous one finishes.
    if (t.mipmap() && t.fenceValue) {
        qWarning("Attempted to queue mipmapped texture upload (%u) while a previous upload is still in progress", id);
        return;
    }

    t.fenceValue = nextTextureUploadFenceValue.fetchAndAddAcquire(1) + 1;

    if (Q_UNLIKELY(debug_texture()))
        qDebug("adding upload for texture %u on the copy queue, fence %llu", id, t.fenceValue);

    D3D12_RESOURCE_DESC textureDesc = t.texture->GetDesc();
    const QSize adjustedTextureSize(textureDesc.Width, textureDesc.Height);

    int totalSize = 0;
    for (const QImage &image : images) {
        int bytesPerPixel;
        textureFormat(image.format(), t.alpha(), t.mipmap(),
                      flags.testFlag(QSGD3D12Engine::TextureUploadAlways32Bit),
                      nullptr, &bytesPerPixel);
        const int w = !t.mipmap() ? image.width() : adjustedTextureSize.width();
        const int h = !t.mipmap() ? image.height() : adjustedTextureSize.height();
        const int stride = alignedSize(w * bytesPerPixel, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
        totalSize += alignedSize(h * stride, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
    }

    if (Q_UNLIKELY(debug_texture()))
        qDebug("%d sub-uploads, heap size %d bytes", images.count(), totalSize);

    // Instead of individual committed resources for each upload buffer,
    // allocate only once and use placed resources.
    D3D12_HEAP_PROPERTIES uploadHeapProp = {};
    uploadHeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_HEAP_DESC uploadHeapDesc = {};
    uploadHeapDesc.SizeInBytes = totalSize;
    uploadHeapDesc.Properties = uploadHeapProp;
    uploadHeapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;

    Texture::StagingHeap sheap;
    if (FAILED(device->CreateHeap(&uploadHeapDesc, IID_PPV_ARGS(&sheap.heap)))) {
        qWarning("Failed to create texture upload heap of size %d", totalSize);
        return;
    }
    t.stagingHeaps.append(sheap);

    copyCommandList->Reset(copyCommandAllocator.Get(), nullptr);

    int placedOffset = 0;
    for (int i = 0; i < images.count(); ++i) {
        QImage::Format convFormat;
        int bytesPerPixel;
        textureFormat(images[i].format(), t.alpha(), t.mipmap(),
                      flags.testFlag(QSGD3D12Engine::TextureUploadAlways32Bit),
                      &convFormat, &bytesPerPixel);
        if (Q_UNLIKELY(debug_texture() && i == 0))
            qDebug("source image format %d, target format %d, bpp %d", images[i].format(), convFormat, bytesPerPixel);

        QImage convImage = images[i].format() == convFormat ? images[i] : images[i].convertToFormat(convFormat);

        if (t.mipmap() && adjustedTextureSize != convImage.size())
            convImage = convImage.scaled(adjustedTextureSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        const int stride = alignedSize(convImage.width() * bytesPerPixel, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

        D3D12_RESOURCE_DESC bufDesc = {};
        bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufDesc.Width = stride * convImage.height();
        bufDesc.Height = 1;
        bufDesc.DepthOrArraySize = 1;
        bufDesc.MipLevels = 1;
        bufDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufDesc.SampleDesc.Count = 1;
        bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        Texture::StagingBuffer sbuf;
        if (FAILED(device->CreatePlacedResource(sheap.heap.Get(), placedOffset,
                                                &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                                nullptr, IID_PPV_ARGS(&sbuf.buffer)))) {
            qWarning("Failed to create texture upload buffer");
            return;
        }

        quint8 *p = nullptr;
        const D3D12_RANGE readRange = { 0, 0 };
        if (FAILED(sbuf.buffer->Map(0, &readRange, reinterpret_cast<void **>(&p)))) {
            qWarning("Map failed (texture upload buffer)");
            return;
        }
        for (int y = 0, ye = convImage.height(); y < ye; ++y) {
            memcpy(p, convImage.constScanLine(y), convImage.width() * bytesPerPixel);
            p += stride;
        }
        sbuf.buffer->Unmap(0, nullptr);

        D3D12_TEXTURE_COPY_LOCATION dstLoc;
        dstLoc.pResource = t.texture.Get();
        dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLoc.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION srcLoc;
        srcLoc.pResource = sbuf.buffer.Get();
        srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLoc.PlacedFootprint.Offset = 0;
        srcLoc.PlacedFootprint.Footprint.Format = textureDesc.Format;
        srcLoc.PlacedFootprint.Footprint.Width = convImage.width();
        srcLoc.PlacedFootprint.Footprint.Height = convImage.height();
        srcLoc.PlacedFootprint.Footprint.Depth = 1;
        srcLoc.PlacedFootprint.Footprint.RowPitch = stride;

        copyCommandList->CopyTextureRegion(&dstLoc, dstPos[i].x(), dstPos[i].y(), 0, &srcLoc, nullptr);

        t.stagingBuffers.append(sbuf);
        placedOffset += alignedSize(bufDesc.Width, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
    }

    copyCommandList->Close();
    ID3D12CommandList *commandLists[] = { copyCommandList.Get() };
    copyCommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
    copyCommandQueue->Signal(textureUploadFence.Get(), t.fenceValue);
}

void QSGD3D12EnginePrivate::releaseTexture(uint id)
{
    if (!id || !initialized)
        return;

    const int idx = id - 1;
    Q_ASSERT(idx < textures.count());

    if (Q_UNLIKELY(debug_texture()))
        qDebug("releasing texture %d", id);

    Texture &t(textures[idx]);
    if (!t.entryInUse())
        return;

    if (t.texture) {
        deferredDelete(t.texture);
        deferredDelete(t.srv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        for (D3D12_CPU_DESCRIPTOR_HANDLE h : t.mipUAVs)
            deferredDelete(h, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    QSet<PersistentFrameData::PendingRelease> *pendingReleasesSet = inFrame
            ? &pframeData[currentPFrameIndex].pendingReleases
            : &pframeData[(currentPFrameIndex + 1) % frameInFlightCount].outOfFramePendingReleases;

    pendingReleasesSet->insert(PersistentFrameData::PendingRelease(PersistentFrameData::PendingRelease::TypeTexture, id));
}

void QSGD3D12EnginePrivate::useTexture(uint id)
{
    if (!inFrame) {
        qWarning("%s: Cannot be called outside begin/endFrame", __FUNCTION__);
        return;
    }

    Q_ASSERT(id);
    const int idx = id - 1;
    Q_ASSERT(idx < textures.count() && textures[idx].entryInUse());

    // Within one frame the order of calling this function determines the
    // texture register (0, 1, ...) so fill up activeTextures accordingly.
    tframeData.activeTextures[tframeData.activeTextureCount++]
            = TransientFrameData::ActiveTexture(TransientFrameData::ActiveTexture::TypeTexture, id);

    if (textures[idx].fenceValue)
        pframeData[currentPFrameIndex].pendingTextureUploads.insert(id);
}

bool QSGD3D12EnginePrivate::MipMapGen::initialize(QSGD3D12EnginePrivate *enginePriv)
{
    engine = enginePriv;

    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;

    D3D12_DESCRIPTOR_RANGE descRange[2];
    descRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descRange[0].NumDescriptors = 1;
    descRange[0].BaseShaderRegister = 0; // t0
    descRange[0].RegisterSpace = 0;
    descRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    descRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    descRange[1].NumDescriptors = 4;
    descRange[1].BaseShaderRegister = 0; // u0..u3
    descRange[1].RegisterSpace = 0;
    descRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // Split into two to allow switching between the first and second set of UAVs later.
    D3D12_ROOT_PARAMETER rootParameters[3];
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &descRange[0];

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &descRange[1];

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[2].Constants.Num32BitValues = 4; // uint2 mip1Size, uint sampleLevel, uint totalMips
    rootParameters[2].Constants.ShaderRegister = 0; // b0
    rootParameters[2].Constants.RegisterSpace = 0;

    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters = 3;
    desc.pParameters = rootParameters;
    desc.NumStaticSamplers = 1;
    desc.pStaticSamplers = &sampler;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error))) {
        QByteArray msg(static_cast<const char *>(error->GetBufferPointer()), error->GetBufferSize());
        qWarning("Failed to serialize compute root signature: %s", qPrintable(msg));
        return false;
    }
    if (FAILED(engine->device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                                   IID_PPV_ARGS(&rootSig)))) {
        qWarning("Failed to create compute root signature");
        return false;
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSig.Get();
    psoDesc.CS.pShaderBytecode = g_CS_Generate4MipMaps;
    psoDesc.CS.BytecodeLength = sizeof(g_CS_Generate4MipMaps);

    if (FAILED(engine->device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState)))) {
        qWarning("Failed to create compute pipeline state");
        return false;
    }

    return true;
}

void QSGD3D12EnginePrivate::MipMapGen::releaseResources()
{
    pipelineState = nullptr;
    rootSig = nullptr;
}

// The mipmap generator is used to insert commands on the main 3D queue. It is
// guaranteed that the queue has a wait for the base texture level upload
// before invoking queueGenerate(). There can be any number of invocations
// without waiting for earlier ones to finish. finished() is invoked when it is
// known for sure that frame containing the upload and mipmap generation has
// finished on the GPU.

void QSGD3D12EnginePrivate::MipMapGen::queueGenerate(const Texture &t)
{
    D3D12_RESOURCE_DESC textureDesc = t.texture->GetDesc();

    engine->commandList->SetPipelineState(pipelineState.Get());
    engine->commandList->SetComputeRootSignature(rootSig.Get());

    // 1 SRV + (miplevels - 1) UAVs
    const int descriptorCount = 1 + (textureDesc.MipLevels - 1);

    engine->ensureGPUDescriptorHeap(descriptorCount);

    // The descriptor heap is set on the command list either because the
    // ensure() call above resized, or, typically, due to a texture-dependent
    // draw call earlier.

    engine->transitionResource(t.texture.Get(), engine->commandList,
                               D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    QSGD3D12EnginePrivate::PersistentFrameData &pfd(engine->pframeData[engine->currentPFrameIndex]);

    const uint stride = engine->cpuDescHeapManager.handleSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_CPU_DESCRIPTOR_HANDLE h = pfd.gpuCbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
    h.ptr += pfd.cbvSrvUavNextFreeDescriptorIndex * stride;

    engine->device->CopyDescriptorsSimple(1, h, t.srv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    h.ptr += stride;

    for (int level = 1; level < textureDesc.MipLevels; ++level, h.ptr += stride)
        engine->device->CopyDescriptorsSimple(1, h, t.mipUAVs[level - 1], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_GPU_DESCRIPTOR_HANDLE gpuAddr = pfd.gpuCbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
    gpuAddr.ptr += pfd.cbvSrvUavNextFreeDescriptorIndex * stride;

    engine->commandList->SetComputeRootDescriptorTable(0, gpuAddr);
    gpuAddr.ptr += stride; // now points to the first UAV

    for (int level = 1; level < textureDesc.MipLevels; level += 4, gpuAddr.ptr += stride * 4) {
        engine->commandList->SetComputeRootDescriptorTable(1, gpuAddr);

        QSize sz(textureDesc.Width, textureDesc.Height);
        sz.setWidth(qMax(1, sz.width() >> level));
        sz.setHeight(qMax(1, sz.height() >> level));

        const quint32 constants[4] = { quint32(sz.width()), quint32(sz.height()),
                                       quint32(level - 1),
                                       quint32(textureDesc.MipLevels - 1) };

        engine->commandList->SetComputeRoot32BitConstants(2, 4, constants, 0);
        engine->commandList->Dispatch(sz.width(), sz.height(), 1);
        engine->uavBarrier(t.texture.Get(), engine->commandList);
    }

    engine->transitionResource(t.texture.Get(), engine->commandList,
                               D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    pfd.cbvSrvUavNextFreeDescriptorIndex += descriptorCount;
}

void QSGD3D12EnginePrivate::deferredDelete(ComPtr<ID3D12Resource> res)
{
    PersistentFrameData::DeleteQueueEntry e;
    e.res = res;
    QVector<PersistentFrameData::DeleteQueueEntry> *dq = inFrame
            ? &pframeData[currentPFrameIndex].deleteQueue
            : &pframeData[(currentPFrameIndex + 1) % frameInFlightCount].outOfFrameDeleteQueue;
    (*dq) << e;
}

void QSGD3D12EnginePrivate::deferredDelete(ComPtr<ID3D12DescriptorHeap> dh)
{
    PersistentFrameData::DeleteQueueEntry e;
    e.descHeap = dh;
    QVector<PersistentFrameData::DeleteQueueEntry> *dq = inFrame
            ? &pframeData[currentPFrameIndex].deleteQueue
            : &pframeData[(currentPFrameIndex + 1) % frameInFlightCount].outOfFrameDeleteQueue;
    (*dq) << e;
}

void QSGD3D12EnginePrivate::deferredDelete(D3D12_CPU_DESCRIPTOR_HANDLE h, D3D12_DESCRIPTOR_HEAP_TYPE type)
{
    PersistentFrameData::DeleteQueueEntry e;
    e.cpuDescriptorPtr = h.ptr;
    e.descHeapType = type;
    QVector<PersistentFrameData::DeleteQueueEntry> *dq = inFrame
            ? &pframeData[currentPFrameIndex].deleteQueue
            : &pframeData[(currentPFrameIndex + 1) % frameInFlightCount].outOfFrameDeleteQueue;
    (*dq) << e;
}

uint QSGD3D12EnginePrivate::genRenderTarget()
{
    return newId(&renderTargets);
}

void QSGD3D12EnginePrivate::createRenderTarget(uint id, const QSize &size, const QVector4D &clearColor, uint samples)
{
    ensureDevice();

    Q_ASSERT(id);
    const int idx = id - 1;
    Q_ASSERT(idx < renderTargets.count() && renderTargets[idx].entryInUse());
    RenderTarget &rt(renderTargets[idx]);

    rt.rtv = cpuDescHeapManager.allocate(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    rt.dsv = cpuDescHeapManager.allocate(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    rt.srv = cpuDescHeapManager.allocate(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    ID3D12Resource *res = createColorBuffer(rt.rtv, size, clearColor, samples);
    if (res)
        rt.color.Attach(res);

    ID3D12Resource *dsres = createDepthStencil(rt.dsv, size, samples);
    if (dsres)
        rt.ds.Attach(dsres);

    const bool multisample = rt.color->GetDesc().SampleDesc.Count > 1;
    syncEntryFlags(&rt, RenderTarget::Multisample, multisample);

    if (!multisample) {
        device->CreateShaderResourceView(rt.color.Get(), nullptr, rt.srv);
    } else {
        D3D12_HEAP_PROPERTIES defaultHeapProp = {};
        defaultHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC textureDesc = {};
        textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        textureDesc.Width = size.width();
        textureDesc.Height = size.height();
        textureDesc.DepthOrArraySize = 1;
        textureDesc.MipLevels = 1;
        textureDesc.Format = RT_COLOR_FORMAT;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

        HRESULT hr = device->CreateCommittedResource(&defaultHeapProp, D3D12_HEAP_FLAG_NONE, &textureDesc,
                                                     D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&rt.colorResolve));
        if (FAILED(hr)) {
            qWarning("Failed to create resolve buffer: %s",
                     qPrintable(comErrorMessage(hr)));
            return;
        }

        device->CreateShaderResourceView(rt.colorResolve.Get(), nullptr, rt.srv);
    }

    if (Q_UNLIKELY(debug_render()))
        qDebug("created new render target %u, size %dx%d, samples %d", id, size.width(), size.height(), samples);
}

void QSGD3D12EnginePrivate::releaseRenderTarget(uint id)
{
    if (!id || !initialized)
        return;

    const int idx = id - 1;
    Q_ASSERT(idx < renderTargets.count());
    RenderTarget &rt(renderTargets[idx]);
    if (!rt.entryInUse())
        return;

    if (Q_UNLIKELY(debug_render()))
        qDebug("releasing render target %u", id);

    if (rt.colorResolve) {
        deferredDelete(rt.colorResolve);
        rt.colorResolve = nullptr;
    }
    if (rt.color) {
        deferredDelete(rt.color);
        rt.color = nullptr;
        deferredDelete(rt.rtv, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        deferredDelete(rt.srv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }
    if (rt.ds) {
        deferredDelete(rt.ds);
        rt.ds = nullptr;
        deferredDelete(rt.dsv, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    }

    rt.flags &= ~RenderTarget::EntryInUse;
}

void QSGD3D12EnginePrivate::useRenderTargetAsTexture(uint id)
{
    if (!inFrame) {
        qWarning("%s: Cannot be called outside begin/endFrame", __FUNCTION__);
        return;
    }

    Q_ASSERT(id);
    const int idx = id - 1;
    Q_ASSERT(idx < renderTargets.count());
    RenderTarget &rt(renderTargets[idx]);
    Q_ASSERT(rt.entryInUse() && rt.color);

    if (rt.flags & RenderTarget::NeedsReadBarrier) {
        rt.flags &= ~RenderTarget::NeedsReadBarrier;
        if (rt.flags & RenderTarget::Multisample)
            resolveMultisampledTarget(rt.color.Get(), rt.colorResolve.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, commandList);
        else
            transitionResource(rt.color.Get(), commandList, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }

    tframeData.activeTextures[tframeData.activeTextureCount++] =
            TransientFrameData::ActiveTexture::ActiveTexture(TransientFrameData::ActiveTexture::TypeRenderTarget, id);
}

QImage QSGD3D12EnginePrivate::executeAndWaitReadbackRenderTarget(uint id)
{
    // Readback due to QQuickWindow::grabWindow() happens outside
    // begin-endFrame, but QQuickItemGrabResult leads to rendering a layer
    // without a real frame afterwards and triggering readback. This has to be
    // supported as well.
    if (inFrame && (!activeLayers || currentLayerDepth)) {
        qWarning("%s: Cannot be called while frame preparation is active", __FUNCTION__);
        return QImage();
    }

    // Due to the above we insert a fake "real" frame when a layer was just rendered into.
    if (inFrame) {
        beginFrame();
        endFrame();
    }

    frameCommandList->Reset(frameCommandAllocator[frameIndex % frameInFlightCount].Get(), nullptr);

    D3D12_RESOURCE_STATES bstate;
    bool needsBarrier = false;
    ID3D12Resource *rtRes;
    if (id == 0) {
        const int idx = presentFrameIndex % swapChainBufferCount;
        if (windowSamples > 1) {
            resolveMultisampledTarget(defaultRT[idx].Get(), backBufferRT[idx].Get(),
                                      D3D12_RESOURCE_STATE_COPY_SOURCE, frameCommandList.Get());
        } else {
            bstate = D3D12_RESOURCE_STATE_PRESENT;
            needsBarrier = true;
        }
        rtRes = backBufferRT[idx].Get();
    } else {
        const int idx = id - 1;
        Q_ASSERT(idx < renderTargets.count());
        RenderTarget &rt(renderTargets[idx]);
        Q_ASSERT(rt.entryInUse() && rt.color);

        if (rt.flags & RenderTarget::Multisample) {
            resolveMultisampledTarget(rt.color.Get(), rt.colorResolve.Get(),
                                      D3D12_RESOURCE_STATE_COPY_SOURCE, frameCommandList.Get());
            rtRes = rt.colorResolve.Get();
        } else {
            rtRes = rt.color.Get();
            bstate = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            needsBarrier = true;
        }
    }

    ComPtr<ID3D12Resource> readbackBuf;

    D3D12_RESOURCE_DESC rtDesc = rtRes->GetDesc();
    UINT64 textureByteSize = 0;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT textureLayout = {};
    device->GetCopyableFootprints(&rtDesc, 0, 1, 0, &textureLayout, nullptr, nullptr, &textureByteSize);

    D3D12_HEAP_PROPERTIES heapProp = {};
    heapProp.Type = D3D12_HEAP_TYPE_READBACK;

    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = textureByteSize;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    if (FAILED(device->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &bufDesc,
                                               D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readbackBuf)))) {
        qWarning("Failed to create committed resource (readback buffer)");
        return QImage();
    }

    D3D12_TEXTURE_COPY_LOCATION dstLoc;
    dstLoc.pResource = readbackBuf.Get();
    dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dstLoc.PlacedFootprint = textureLayout;
    D3D12_TEXTURE_COPY_LOCATION srcLoc;
    srcLoc.pResource = rtRes;
    srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    srcLoc.SubresourceIndex = 0;

    ID3D12GraphicsCommandList *cl = frameCommandList.Get();
    if (needsBarrier)
        transitionResource(rtRes, cl, bstate, D3D12_RESOURCE_STATE_COPY_SOURCE);
    cl->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
    if (needsBarrier)
        transitionResource(rtRes, cl, D3D12_RESOURCE_STATE_COPY_SOURCE, bstate);

    cl->Close();
    ID3D12CommandList *commandLists[] = { cl };
    commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

    QScopedPointer<QSGD3D12CPUWaitableFence> f(createCPUWaitableFence());
    waitForGPU(f.data()); // uh oh

    QImage::Format fmt = imageFormatForTexture(rtDesc.Format);
    if (fmt == QImage::Format_Invalid) {
        qWarning("Could not map render target format %d to a QImage format", rtDesc.Format);
        return QImage();
    }
    QImage img(rtDesc.Width, rtDesc.Height, fmt);
    quint8 *p = nullptr;
    const D3D12_RANGE readRange = { 0, 0 };
    if (FAILED(readbackBuf->Map(0, &readRange, reinterpret_cast<void **>(&p)))) {
        qWarning("Mapping the readback buffer failed");
        return QImage();
    }
    const int bpp = 4; // ###
    if (id == 0) {
        for (UINT y = 0; y < rtDesc.Height; ++y) {
            quint8 *dst = img.scanLine(y);
            memcpy(dst, p, rtDesc.Width * bpp);
            p += textureLayout.Footprint.RowPitch;
        }
    } else {
        for (int y = rtDesc.Height - 1; y >= 0; --y) {
            quint8 *dst = img.scanLine(y);
            memcpy(dst, p, rtDesc.Width * bpp);
            p += textureLayout.Footprint.RowPitch;
        }
    }
    readbackBuf->Unmap(0, nullptr);

    return img;
}

void QSGD3D12EnginePrivate::simulateDeviceLoss()
{
    qWarning("QSGD3D12Engine: Triggering device loss via TDR");
    devLossTest.killDevice();
}

bool QSGD3D12EnginePrivate::DeviceLossTester::initialize(QSGD3D12EnginePrivate *enginePriv)
{
    engine = enginePriv;

#ifdef DEVLOSS_TEST
    D3D12_DESCRIPTOR_RANGE descRange[2];
    descRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    descRange[0].NumDescriptors = 1;
    descRange[0].BaseShaderRegister = 0;
    descRange[0].RegisterSpace = 0;
    descRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    descRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    descRange[1].NumDescriptors = 1;
    descRange[1].BaseShaderRegister = 0;
    descRange[1].RegisterSpace = 0;
    descRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER param;
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    param.DescriptorTable.NumDescriptorRanges = 2;
    param.DescriptorTable.pDescriptorRanges = descRange;

    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters = 1;
    desc.pParameters = &param;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error))) {
        QByteArray msg(static_cast<const char *>(error->GetBufferPointer()), error->GetBufferSize());
        qWarning("Failed to serialize compute root signature: %s", qPrintable(msg));
        return false;
    }
    if (FAILED(engine->device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                        IID_PPV_ARGS(&computeRootSignature)))) {
        qWarning("Failed to create compute root signature");
        return false;
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = computeRootSignature.Get();
    psoDesc.CS.pShaderBytecode = g_timeout;
    psoDesc.CS.BytecodeLength = sizeof(g_timeout);

    if (FAILED(engine->device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&computeState)))) {
        qWarning("Failed to create compute pipeline state");
        return false;
    }
#endif

    return true;
}

void QSGD3D12EnginePrivate::DeviceLossTester::releaseResources()
{
    computeState = nullptr;
    computeRootSignature = nullptr;
}

void QSGD3D12EnginePrivate::DeviceLossTester::killDevice()
{
#ifdef DEVLOSS_TEST
    ID3D12CommandAllocator *ca = engine->frameCommandAllocator[engine->frameIndex % engine->frameInFlightCount].Get();
    ID3D12GraphicsCommandList *cl = engine->frameCommandList.Get();
    cl->Reset(ca, computeState.Get());

    cl->SetComputeRootSignature(computeRootSignature.Get());
    cl->Dispatch(256, 1, 1);

    cl->Close();
    ID3D12CommandList *commandLists[] = { cl };
    engine->commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

    engine->waitGPU();
#endif
}

void *QSGD3D12EnginePrivate::getResource(QSGRendererInterface::Resource resource) const
{
    switch (resource) {
    case QSGRendererInterface::DeviceResource:
        return device;
    case QSGRendererInterface::CommandQueueResource:
        return commandQueue.Get();
    case QSGRendererInterface::CommandListResource:
        return commandList;
    default:
        break;
    }
    return nullptr;
}

QT_END_NAMESPACE
