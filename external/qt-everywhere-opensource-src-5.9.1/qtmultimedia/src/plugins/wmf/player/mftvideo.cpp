/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

#include "mftvideo.h"
#include "mfvideoprobecontrol.h"
#include <private/qmemoryvideobuffer_p.h>
#include <Mferror.h>
#include <strmif.h>
#include <uuids.h>
#include <InitGuid.h>
#include <d3d9.h>
#include <qdebug.h>

// This MFT sends all samples it processes to connected video probes.
// Sample is sent to probes in ProcessInput.
// In ProcessOutput this MFT simply returns the original sample.

// The implementation is based on a boilerplate from the MF SDK example.

MFTransform::MFTransform():
    m_cRef(1),
    m_inputType(0),
    m_outputType(0),
    m_sample(0),
    m_videoSinkTypeHandler(0),
    m_bytesPerLine(0)
{
}

MFTransform::~MFTransform()
{
    if (m_inputType)
        m_inputType->Release();

    if (m_outputType)
        m_outputType->Release();

    if (m_videoSinkTypeHandler)
        m_videoSinkTypeHandler->Release();
}

void MFTransform::addProbe(MFVideoProbeControl *probe)
{
    QMutexLocker locker(&m_videoProbeMutex);

    if (m_videoProbes.contains(probe))
        return;

    m_videoProbes.append(probe);
}

void MFTransform::removeProbe(MFVideoProbeControl *probe)
{
    QMutexLocker locker(&m_videoProbeMutex);
    m_videoProbes.removeOne(probe);
}

void MFTransform::setVideoSink(IUnknown *videoSink)
{
    // This transform supports the same input types as the video sink.
    // Store its type handler interface in order to report the correct supported types.

    if (m_videoSinkTypeHandler) {
        m_videoSinkTypeHandler->Release();
        m_videoSinkTypeHandler = NULL;
    }

    if (videoSink)
        videoSink->QueryInterface(IID_PPV_ARGS(&m_videoSinkTypeHandler));
}

STDMETHODIMP MFTransform::QueryInterface(REFIID riid, void** ppv)
{
    if (!ppv)
        return E_POINTER;
    if (riid == IID_IMFTransform) {
        *ppv = static_cast<IMFTransform*>(this);
    } else if (riid == IID_IUnknown) {
        *ppv = static_cast<IUnknown*>(this);
    } else {
        *ppv =  NULL;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) MFTransform::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) MFTransform::Release()
{
    ULONG cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0) {
        delete this;
    }
    return cRef;
}

STDMETHODIMP MFTransform::GetStreamLimits(DWORD *pdwInputMinimum, DWORD *pdwInputMaximum, DWORD *pdwOutputMinimum, DWORD *pdwOutputMaximum)
{
    if (!pdwInputMinimum || !pdwInputMaximum || !pdwOutputMinimum || !pdwOutputMaximum)
        return E_POINTER;
    *pdwInputMinimum = 1;
    *pdwInputMaximum = 1;
    *pdwOutputMinimum = 1;
    *pdwOutputMaximum = 1;
    return S_OK;
}

STDMETHODIMP MFTransform::GetStreamCount(DWORD *pcInputStreams, DWORD *pcOutputStreams)
{
    if (!pcInputStreams || !pcOutputStreams)
        return E_POINTER;

    *pcInputStreams = 1;
    *pcOutputStreams = 1;
    return S_OK;
}

STDMETHODIMP MFTransform::GetStreamIDs(DWORD dwInputIDArraySize, DWORD *pdwInputIDs, DWORD dwOutputIDArraySize, DWORD *pdwOutputIDs)
{
    // streams are numbered consecutively
    Q_UNUSED(dwInputIDArraySize);
    Q_UNUSED(pdwInputIDs);
    Q_UNUSED(dwOutputIDArraySize);
    Q_UNUSED(pdwOutputIDs);
    return E_NOTIMPL;
}

STDMETHODIMP MFTransform::GetInputStreamInfo(DWORD dwInputStreamID, MFT_INPUT_STREAM_INFO *pStreamInfo)
{
    QMutexLocker locker(&m_mutex);

    if (dwInputStreamID > 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (!pStreamInfo)
        return E_POINTER;

    pStreamInfo->cbSize = 0;
    pStreamInfo->hnsMaxLatency = 0;
    pStreamInfo->cbMaxLookahead = 0;
    pStreamInfo->cbAlignment = 0;
    pStreamInfo->dwFlags = MFT_INPUT_STREAM_WHOLE_SAMPLES
                            | MFT_INPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER
                            | MFT_INPUT_STREAM_PROCESSES_IN_PLACE;

    return S_OK;
}

STDMETHODIMP MFTransform::GetOutputStreamInfo(DWORD dwOutputStreamID, MFT_OUTPUT_STREAM_INFO *pStreamInfo)
{
    QMutexLocker locker(&m_mutex);

    if (dwOutputStreamID > 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (!pStreamInfo)
        return E_POINTER;

    pStreamInfo->cbSize = 0;
    pStreamInfo->cbAlignment = 0;
    pStreamInfo->dwFlags = MFT_OUTPUT_STREAM_WHOLE_SAMPLES
                            | MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER
                            | MFT_OUTPUT_STREAM_PROVIDES_SAMPLES
                            | MFT_OUTPUT_STREAM_DISCARDABLE;

    return S_OK;
}

STDMETHODIMP MFTransform::GetAttributes(IMFAttributes **pAttributes)
{
    // This MFT does not support attributes.
    Q_UNUSED(pAttributes);
    return E_NOTIMPL;
}

STDMETHODIMP MFTransform::GetInputStreamAttributes(DWORD dwInputStreamID, IMFAttributes **pAttributes)
{
    // This MFT does not support input stream attributes.
    Q_UNUSED(dwInputStreamID);
    Q_UNUSED(pAttributes);
    return E_NOTIMPL;
}

STDMETHODIMP MFTransform::GetOutputStreamAttributes(DWORD dwOutputStreamID, IMFAttributes **pAttributes)
{
    // This MFT does not support output stream attributes.
    Q_UNUSED(dwOutputStreamID);
    Q_UNUSED(pAttributes);
    return E_NOTIMPL;
}

STDMETHODIMP MFTransform::DeleteInputStream(DWORD dwStreamID)
{
    // This MFT has a fixed number of input streams.
    Q_UNUSED(dwStreamID);
    return E_NOTIMPL;
}

STDMETHODIMP MFTransform::AddInputStreams(DWORD cStreams, DWORD *adwStreamIDs)
{
    // This MFT has a fixed number of input streams.
    Q_UNUSED(cStreams);
    Q_UNUSED(adwStreamIDs);
    return E_NOTIMPL;
}

STDMETHODIMP MFTransform::GetInputAvailableType(DWORD dwInputStreamID, DWORD dwTypeIndex, IMFMediaType **ppType)
{
    // We support the same input types as the video sink
    if (!m_videoSinkTypeHandler)
        return E_NOTIMPL;

    if (dwInputStreamID > 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (!ppType)
        return E_POINTER;

    return m_videoSinkTypeHandler->GetMediaTypeByIndex(dwTypeIndex, ppType);
}

STDMETHODIMP MFTransform::GetOutputAvailableType(DWORD dwOutputStreamID, DWORD dwTypeIndex, IMFMediaType **ppType)
{
    // Since we don't modify the samples, the output type must be the same as the input type.
    // Report our input type as the only available output type.

    if (dwOutputStreamID > 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (!ppType)
        return E_POINTER;

    // Input type must be set first
    if (!m_inputType)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (dwTypeIndex > 0)
        return MF_E_NO_MORE_TYPES;

    // Return a copy to make sure our type is not modified
    if (FAILED(MFCreateMediaType(ppType)))
        return E_OUTOFMEMORY;

    return m_inputType->CopyAllItems(*ppType);
}

STDMETHODIMP MFTransform::SetInputType(DWORD dwInputStreamID, IMFMediaType *pType, DWORD dwFlags)
{
    if (dwInputStreamID > 0)
        return MF_E_INVALIDSTREAMNUMBER;

    QMutexLocker locker(&m_mutex);

    if (m_sample)
        return MF_E_TRANSFORM_CANNOT_CHANGE_MEDIATYPE_WHILE_PROCESSING;

    if (!isMediaTypeSupported(pType))
        return MF_E_INVALIDMEDIATYPE;

    if (dwFlags == MFT_SET_TYPE_TEST_ONLY)
        return pType ? S_OK : E_POINTER;

    if (m_inputType) {
        m_inputType->Release();
        // Input type has changed, discard output type (if it's set) so it's reset later on
        DWORD flags = 0;
        if (m_outputType && m_outputType->IsEqual(pType, &flags) != S_OK) {
            m_outputType->Release();
            m_outputType = 0;
        }
    }

    m_inputType = pType;

    if (m_inputType)
        m_inputType->AddRef();

    return S_OK;
}

STDMETHODIMP MFTransform::SetOutputType(DWORD dwOutputStreamID, IMFMediaType *pType, DWORD dwFlags)
{
    if (dwOutputStreamID > 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (dwFlags == MFT_SET_TYPE_TEST_ONLY && !pType)
        return E_POINTER;

    QMutexLocker locker(&m_mutex);

    // Input type must be set first
    if (!m_inputType)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (m_sample)
        return MF_E_TRANSFORM_CANNOT_CHANGE_MEDIATYPE_WHILE_PROCESSING;

    DWORD flags = 0;
    if (pType && m_inputType->IsEqual(pType, &flags) != S_OK)
        return MF_E_INVALIDMEDIATYPE;

    if (dwFlags == MFT_SET_TYPE_TEST_ONLY)
        return pType ? S_OK : E_POINTER;

    if (m_outputType)
        m_outputType->Release();

    m_outputType = pType;

    if (m_outputType) {
        m_outputType->AddRef();
        m_format = videoFormatForMFMediaType(m_outputType, &m_bytesPerLine);
    }

    return S_OK;
}

STDMETHODIMP MFTransform::GetInputCurrentType(DWORD dwInputStreamID, IMFMediaType **ppType)
{
    if (dwInputStreamID > 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (ppType == NULL)
        return E_POINTER;

    QMutexLocker locker(&m_mutex);

    if (!m_inputType)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    // Return a copy to make sure our type is not modified
    if (FAILED(MFCreateMediaType(ppType)))
        return E_OUTOFMEMORY;

    return m_inputType->CopyAllItems(*ppType);
}

STDMETHODIMP MFTransform::GetOutputCurrentType(DWORD dwOutputStreamID, IMFMediaType **ppType)
{
    if (dwOutputStreamID > 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (ppType == NULL)
        return E_POINTER;

    QMutexLocker locker(&m_mutex);

    if (!m_outputType)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    // Return a copy to make sure our type is not modified
    if (FAILED(MFCreateMediaType(ppType)))
        return E_OUTOFMEMORY;

    return m_outputType->CopyAllItems(*ppType);
}

STDMETHODIMP MFTransform::GetInputStatus(DWORD dwInputStreamID, DWORD *pdwFlags)
{
    if (dwInputStreamID > 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (!pdwFlags)
        return E_POINTER;

    QMutexLocker locker(&m_mutex);

    if (!m_inputType || !m_outputType)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (m_sample)
        *pdwFlags = 0;
    else
        *pdwFlags = MFT_INPUT_STATUS_ACCEPT_DATA;

    return S_OK;
}

STDMETHODIMP MFTransform::GetOutputStatus(DWORD *pdwFlags)
{
    if (!pdwFlags)
        return E_POINTER;

    QMutexLocker locker(&m_mutex);

    if (!m_inputType || !m_outputType)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (m_sample)
        *pdwFlags = MFT_OUTPUT_STATUS_SAMPLE_READY;
    else
        *pdwFlags = 0;

    return S_OK;
}

STDMETHODIMP MFTransform::SetOutputBounds(LONGLONG hnsLowerBound, LONGLONG hnsUpperBound)
{
    Q_UNUSED(hnsLowerBound);
    Q_UNUSED(hnsUpperBound);
    return E_NOTIMPL;
}

STDMETHODIMP MFTransform::ProcessEvent(DWORD dwInputStreamID, IMFMediaEvent *pEvent)
{
    // This MFT ignores all events, and the pipeline should send all events downstream.
    Q_UNUSED(dwInputStreamID);
    Q_UNUSED(pEvent);
    return E_NOTIMPL;
}

STDMETHODIMP MFTransform::ProcessMessage(MFT_MESSAGE_TYPE eMessage, ULONG_PTR ulParam)
{
    Q_UNUSED(ulParam);

    HRESULT hr = S_OK;

    switch (eMessage)
    {
    case MFT_MESSAGE_COMMAND_FLUSH:
        hr = OnFlush();
        break;

    case MFT_MESSAGE_COMMAND_DRAIN:
        // Drain: Tells the MFT not to accept any more input until
        // all of the pending output has been processed. That is our
        // default behevior already, so there is nothing to do.
        break;

    case MFT_MESSAGE_SET_D3D_MANAGER:
        // The pipeline should never send this message unless the MFT
        // has the MF_SA_D3D_AWARE attribute set to TRUE. However, if we
        // do get this message, it's invalid and we don't implement it.
        hr = E_NOTIMPL;
        break;

    // The remaining messages do not require any action from this MFT.
    case MFT_MESSAGE_NOTIFY_BEGIN_STREAMING:
    case MFT_MESSAGE_NOTIFY_END_STREAMING:
    case MFT_MESSAGE_NOTIFY_END_OF_STREAM:
    case MFT_MESSAGE_NOTIFY_START_OF_STREAM:
        break;
    }

    return hr;
}

STDMETHODIMP MFTransform::ProcessInput(DWORD dwInputStreamID, IMFSample *pSample, DWORD dwFlags)
{
    if (dwInputStreamID > 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (dwFlags != 0)
        return E_INVALIDARG; // dwFlags is reserved and must be zero.

    QMutexLocker locker(&m_mutex);

    if (!m_inputType)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (m_sample)
        return MF_E_NOTACCEPTING;

    // Validate the number of buffers. There should only be a single buffer to hold the video frame.
    DWORD dwBufferCount = 0;
    HRESULT hr = pSample->GetBufferCount(&dwBufferCount);
    if (FAILED(hr))
        return hr;

    if (dwBufferCount == 0)
        return E_FAIL;

    if (dwBufferCount > 1)
        return MF_E_SAMPLE_HAS_TOO_MANY_BUFFERS;

    m_sample = pSample;
    m_sample->AddRef();

    QMutexLocker lockerProbe(&m_videoProbeMutex);

    if (!m_videoProbes.isEmpty()) {
        QVideoFrame frame = makeVideoFrame();

        for (MFVideoProbeControl* probe : qAsConst(m_videoProbes))
            probe->bufferProbed(frame);
    }

    return S_OK;
}

STDMETHODIMP MFTransform::ProcessOutput(DWORD dwFlags, DWORD cOutputBufferCount, MFT_OUTPUT_DATA_BUFFER *pOutputSamples, DWORD *pdwStatus)
{
    if (pOutputSamples == NULL || pdwStatus == NULL)
        return E_POINTER;

    if (cOutputBufferCount != 1)
        return E_INVALIDARG;

    QMutexLocker locker(&m_mutex);

    if (!m_inputType)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (!m_outputType) {
        pOutputSamples[0].dwStatus = MFT_OUTPUT_DATA_BUFFER_FORMAT_CHANGE;
        return MF_E_TRANSFORM_STREAM_CHANGE;
    }

    IMFMediaBuffer *input = NULL;
    IMFMediaBuffer *output = NULL;

    if (dwFlags == MFT_PROCESS_OUTPUT_DISCARD_WHEN_NO_BUFFER)
        goto done;
    else if (dwFlags != 0)
        return E_INVALIDARG;

    if (!m_sample)
        return MF_E_TRANSFORM_NEED_MORE_INPUT;

    // Since the MFT_OUTPUT_STREAM_PROVIDES_SAMPLES flag is set, the client
    // should not be providing samples here
    if (pOutputSamples[0].pSample != NULL)
        return E_INVALIDARG;

    pOutputSamples[0].pSample = m_sample;
    pOutputSamples[0].pSample->AddRef();

    // Send video frame to probes
    // We do it here (instead of inside ProcessInput) to make sure samples discarded by the renderer
    // are not sent.
    m_videoProbeMutex.lock();
    if (!m_videoProbes.isEmpty()) {
        QVideoFrame frame = makeVideoFrame();

        foreach (MFVideoProbeControl* probe, m_videoProbes)
            probe->bufferProbed(frame);
    }
    m_videoProbeMutex.unlock();

done:
    pOutputSamples[0].dwStatus = 0;
    *pdwStatus = 0;

    m_sample->Release();
    m_sample = 0;

    if (input)
        input->Release();
    if (output)
        output->Release();

    return S_OK;
}

HRESULT MFTransform::OnFlush()
{
    QMutexLocker locker(&m_mutex);

    if (m_sample) {
        m_sample->Release();
        m_sample = 0;
    }
    return S_OK;
}

QVideoFrame::PixelFormat MFTransform::formatFromSubtype(const GUID& subtype)
{
    if (subtype == MFVideoFormat_ARGB32)
        return QVideoFrame::Format_ARGB32;
    else if (subtype == MFVideoFormat_RGB32)
        return QVideoFrame::Format_RGB32;
    else if (subtype == MFVideoFormat_RGB24)
        return QVideoFrame::Format_RGB24;
    else if (subtype == MFVideoFormat_RGB565)
        return QVideoFrame::Format_RGB565;
    else if (subtype == MFVideoFormat_RGB555)
        return QVideoFrame::Format_RGB555;
    else if (subtype == MFVideoFormat_AYUV)
        return QVideoFrame::Format_AYUV444;
    else if (subtype == MFVideoFormat_I420)
        return QVideoFrame::Format_YUV420P;
    else if (subtype == MFVideoFormat_UYVY)
        return QVideoFrame::Format_UYVY;
    else if (subtype == MFVideoFormat_YV12)
        return QVideoFrame::Format_YV12;
    else if (subtype == MFVideoFormat_NV12)
        return QVideoFrame::Format_NV12;

    return QVideoFrame::Format_Invalid;
}

QVideoSurfaceFormat MFTransform::videoFormatForMFMediaType(IMFMediaType *mediaType, int *bytesPerLine)
{
    UINT32 stride;
    if (FAILED(mediaType->GetUINT32(MF_MT_DEFAULT_STRIDE, &stride))) {
        *bytesPerLine = 0;
        return QVideoSurfaceFormat();
    }

    *bytesPerLine = (int)stride;

    QSize size;
    UINT32 width, height;
    if (FAILED(MFGetAttributeSize(mediaType, MF_MT_FRAME_SIZE, &width, &height)))
        return QVideoSurfaceFormat();

    size.setWidth(width);
    size.setHeight(height);

    GUID subtype = GUID_NULL;
    if (FAILED(mediaType->GetGUID(MF_MT_SUBTYPE, &subtype)))
        return QVideoSurfaceFormat();

    QVideoFrame::PixelFormat pixelFormat = formatFromSubtype(subtype);
    QVideoSurfaceFormat format(size, pixelFormat);

    UINT32 num, den;
    if (SUCCEEDED(MFGetAttributeRatio(mediaType, MF_MT_PIXEL_ASPECT_RATIO, &num, &den))) {
        format.setPixelAspectRatio(num, den);
    }
    if (SUCCEEDED(MFGetAttributeRatio(mediaType, MF_MT_FRAME_RATE, &num, &den))) {
        format.setFrameRate(qreal(num)/den);
    }

    return format;
}

QVideoFrame MFTransform::makeVideoFrame()
{
    QVideoFrame frame;

    if (!m_format.isValid())
        return frame;

    IMFMediaBuffer *buffer = 0;

    do {
        if (FAILED(m_sample->ConvertToContiguousBuffer(&buffer)))
            break;

        QByteArray array = dataFromBuffer(buffer, m_format.frameHeight(), &m_bytesPerLine);
        if (array.isEmpty())
            break;

        // Wrapping IMFSample or IMFMediaBuffer in a QVideoFrame is not possible because we cannot hold
        // IMFSample for a "long" time without affecting the rest of the topology.
        // If IMFSample is held for more than 5 frames decoder starts to reuse it even though it hasn't been released it yet.
        // That is why we copy data from IMFMediaBuffer here.
        frame = QVideoFrame(new QMemoryVideoBuffer(array, m_bytesPerLine), m_format.frameSize(), m_format.pixelFormat());

        // WMF uses 100-nanosecond units, Qt uses microseconds
        LONGLONG startTime = -1;
        if (SUCCEEDED(m_sample->GetSampleTime(&startTime))) {
            frame.setStartTime(startTime * 0.1);

            LONGLONG duration = -1;
            if (SUCCEEDED(m_sample->GetSampleDuration(&duration)))
                frame.setEndTime((startTime + duration) * 0.1);
        }
    } while (false);

    if (buffer)
        buffer->Release();

    return frame;
}

QByteArray MFTransform::dataFromBuffer(IMFMediaBuffer *buffer, int height, int *bytesPerLine)
{
    QByteArray array;
    BYTE *bytes;
    DWORD length;
    HRESULT hr = buffer->Lock(&bytes, NULL, &length);
    if (SUCCEEDED(hr)) {
        array = QByteArray((const char *)bytes, (int)length);
        buffer->Unlock();
    } else {
        // try to lock as Direct3DSurface
        IDirect3DSurface9 *surface = 0;
        do {
            if (FAILED(MFGetService(buffer, MR_BUFFER_SERVICE, IID_IDirect3DSurface9, (void**)&surface)))
                break;

            D3DLOCKED_RECT rect;
            if (FAILED(surface->LockRect(&rect, NULL, D3DLOCK_READONLY)))
                break;

            if (bytesPerLine)
                *bytesPerLine = (int)rect.Pitch;

            array = QByteArray((const char *)rect.pBits, rect.Pitch * height);
            surface->UnlockRect();
        } while (false);

        if (surface) {
            surface->Release();
            surface = 0;
        }
    }

    return array;
}

bool MFTransform::isMediaTypeSupported(IMFMediaType *type)
{
    // If we don't have the video sink's type handler,
    // assume it supports anything...
    if (!m_videoSinkTypeHandler || !type)
        return true;

    return m_videoSinkTypeHandler->IsMediaTypeSupported(type, NULL) == S_OK;
}
