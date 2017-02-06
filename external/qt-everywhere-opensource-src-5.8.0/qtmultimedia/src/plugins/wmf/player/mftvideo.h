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

#ifndef MFTRANSFORM_H
#define MFTRANSFORM_H

#include <mfapi.h>
#include <mfidl.h>
#include <QtCore/qlist.h>
#include <QtCore/qmutex.h>
#include <QtMultimedia/qvideosurfaceformat.h>

QT_USE_NAMESPACE

class MFVideoProbeControl;

class MFTransform: public IMFTransform
{
public:
    MFTransform();
    ~MFTransform();

    void addProbe(MFVideoProbeControl* probe);
    void removeProbe(MFVideoProbeControl* probe);

    void setVideoSink(IUnknown *videoSink);

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID iid, void** ppv);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IMFTransform methods
    STDMETHODIMP GetStreamLimits(DWORD *pdwInputMinimum, DWORD *pdwInputMaximum, DWORD *pdwOutputMinimum, DWORD *pdwOutputMaximum);
    STDMETHODIMP GetStreamCount(DWORD *pcInputStreams, DWORD *pcOutputStreams);
    STDMETHODIMP GetStreamIDs(DWORD dwInputIDArraySize, DWORD *pdwInputIDs, DWORD dwOutputIDArraySize, DWORD *pdwOutputIDs);
    STDMETHODIMP GetInputStreamInfo(DWORD dwInputStreamID, MFT_INPUT_STREAM_INFO *pStreamInfo);
    STDMETHODIMP GetOutputStreamInfo(DWORD dwOutputStreamID, MFT_OUTPUT_STREAM_INFO *pStreamInfo);
    STDMETHODIMP GetAttributes(IMFAttributes **pAttributes);
    STDMETHODIMP GetInputStreamAttributes(DWORD dwInputStreamID, IMFAttributes **pAttributes);
    STDMETHODIMP GetOutputStreamAttributes(DWORD dwOutputStreamID, IMFAttributes **pAttributes);
    STDMETHODIMP DeleteInputStream(DWORD dwStreamID);
    STDMETHODIMP AddInputStreams(DWORD cStreams, DWORD *adwStreamIDs);
    STDMETHODIMP GetInputAvailableType(DWORD dwInputStreamID, DWORD dwTypeIndex, IMFMediaType **ppType);
    STDMETHODIMP GetOutputAvailableType(DWORD dwOutputStreamID,DWORD dwTypeIndex, IMFMediaType **ppType);
    STDMETHODIMP SetInputType(DWORD dwInputStreamID, IMFMediaType *pType, DWORD dwFlags);
    STDMETHODIMP SetOutputType(DWORD dwOutputStreamID, IMFMediaType *pType, DWORD dwFlags);
    STDMETHODIMP GetInputCurrentType(DWORD dwInputStreamID, IMFMediaType **ppType);
    STDMETHODIMP GetOutputCurrentType(DWORD dwOutputStreamID, IMFMediaType **ppType);
    STDMETHODIMP GetInputStatus(DWORD dwInputStreamID, DWORD *pdwFlags);
    STDMETHODIMP GetOutputStatus(DWORD *pdwFlags);
    STDMETHODIMP SetOutputBounds(LONGLONG hnsLowerBound, LONGLONG hnsUpperBound);
    STDMETHODIMP ProcessEvent(DWORD dwInputStreamID, IMFMediaEvent *pEvent);
    STDMETHODIMP ProcessMessage(MFT_MESSAGE_TYPE eMessage, ULONG_PTR ulParam);
    STDMETHODIMP ProcessInput(DWORD dwInputStreamID, IMFSample *pSample, DWORD dwFlags);
    STDMETHODIMP ProcessOutput(DWORD dwFlags, DWORD cOutputBufferCount, MFT_OUTPUT_DATA_BUFFER *pOutputSamples, DWORD *pdwStatus);

private:
    HRESULT OnFlush();
    static QVideoFrame::PixelFormat formatFromSubtype(const GUID& subtype);
    static QVideoSurfaceFormat videoFormatForMFMediaType(IMFMediaType *mediaType, int *bytesPerLine);
    QVideoFrame makeVideoFrame();
    QByteArray dataFromBuffer(IMFMediaBuffer *buffer, int height, int *bytesPerLine);
    bool isMediaTypeSupported(IMFMediaType *type);

    long m_cRef;
    IMFMediaType *m_inputType;
    IMFMediaType *m_outputType;
    IMFSample *m_sample;
    QMutex m_mutex;

    IMFMediaTypeHandler *m_videoSinkTypeHandler;

    QList<MFVideoProbeControl*> m_videoProbes;
    QMutex m_videoProbeMutex;

    QVideoSurfaceFormat m_format;
    int m_bytesPerLine;
};

#endif
