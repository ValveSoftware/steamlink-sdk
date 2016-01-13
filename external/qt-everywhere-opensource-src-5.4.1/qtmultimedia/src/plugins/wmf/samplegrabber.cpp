/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "samplegrabber.h"
#include "mfaudioprobecontrol.h"

STDMETHODIMP SampleGrabberCallback::QueryInterface(REFIID riid, void** ppv)
{
    if (!ppv)
        return E_POINTER;
    if (riid == IID_IMFSampleGrabberSinkCallback) {
        *ppv = static_cast<IMFSampleGrabberSinkCallback*>(this);
    } else if (riid == IID_IMFClockStateSink) {
        *ppv = static_cast<IMFClockStateSink*>(this);
    } else if (riid == IID_IUnknown) {
        *ppv = static_cast<IUnknown*>(this);
    } else {
        *ppv =  NULL;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) SampleGrabberCallback::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) SampleGrabberCallback::Release()
{
    ULONG cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0) {
        delete this;
    }
    return cRef;

}

// IMFClockStateSink methods.

STDMETHODIMP SampleGrabberCallback::OnClockStart(MFTIME hnsSystemTime, LONGLONG llClockStartOffset)
{
    Q_UNUSED(hnsSystemTime);
    Q_UNUSED(llClockStartOffset);
    return S_OK;
}

STDMETHODIMP SampleGrabberCallback::OnClockStop(MFTIME hnsSystemTime)
{
    Q_UNUSED(hnsSystemTime);
    return S_OK;
}

STDMETHODIMP SampleGrabberCallback::OnClockPause(MFTIME hnsSystemTime)
{
    Q_UNUSED(hnsSystemTime);
    return S_OK;
}

STDMETHODIMP SampleGrabberCallback::OnClockRestart(MFTIME hnsSystemTime)
{
    Q_UNUSED(hnsSystemTime);
    return S_OK;
}

STDMETHODIMP SampleGrabberCallback::OnClockSetRate(MFTIME hnsSystemTime, float flRate)
{
    Q_UNUSED(hnsSystemTime);
    Q_UNUSED(flRate);
    return S_OK;
}

// IMFSampleGrabberSink methods.

STDMETHODIMP SampleGrabberCallback::OnSetPresentationClock(IMFPresentationClock* pClock)
{
    Q_UNUSED(pClock);
    return S_OK;
}

STDMETHODIMP SampleGrabberCallback::OnShutdown()
{
    return S_OK;
}

void AudioSampleGrabberCallback::addProbe(MFAudioProbeControl* probe)
{
    QMutexLocker locker(&m_audioProbeMutex);

    if (m_audioProbes.contains(probe))
        return;

    m_audioProbes.append(probe);
}

void AudioSampleGrabberCallback::removeProbe(MFAudioProbeControl* probe)
{
    QMutexLocker locker(&m_audioProbeMutex);
    m_audioProbes.removeOne(probe);
}

void AudioSampleGrabberCallback::setFormat(const QAudioFormat& format)
{
    m_format = format;
}

STDMETHODIMP AudioSampleGrabberCallback::OnProcessSample(REFGUID guidMajorMediaType, DWORD dwSampleFlags,
    LONGLONG llSampleTime, LONGLONG llSampleDuration, const BYTE * pSampleBuffer,
    DWORD dwSampleSize)
{
    Q_UNUSED(dwSampleFlags);
    Q_UNUSED(llSampleTime);
    Q_UNUSED(llSampleDuration);

    if (guidMajorMediaType != GUID_NULL && guidMajorMediaType != MFMediaType_Audio)
        return S_OK;

    QMutexLocker locker(&m_audioProbeMutex);

    if (m_audioProbes.isEmpty())
        return S_OK;

    // Check if sample has a presentation time
    if (llSampleTime == _I64_MAX) {
        // Set default QAudioBuffer start time
        llSampleTime = -1;
    } else {
        // WMF uses 100-nanosecond units, Qt uses microseconds
        llSampleTime /= 10;
    }

    foreach (MFAudioProbeControl* probe, m_audioProbes)
        probe->bufferProbed((const char*)pSampleBuffer, dwSampleSize, m_format, llSampleTime);

    return S_OK;
}
