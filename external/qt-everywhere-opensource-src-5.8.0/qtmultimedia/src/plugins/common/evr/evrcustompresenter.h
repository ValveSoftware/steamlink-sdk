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

#ifndef EVRCUSTOMPRESENTER_H
#define EVRCUSTOMPRESENTER_H

#include <QObject>
#include <qmutex.h>
#include <qqueue.h>
#include <qevent.h>
#include <qvideosurfaceformat.h>

#include "evrdefs.h"

QT_BEGIN_NAMESPACE
class QAbstractVideoSurface;
QT_END_NAMESPACE

QT_USE_NAMESPACE

class EVRCustomPresenter;
class D3DPresentEngine;

template<class T>
class AsyncCallback : public IMFAsyncCallback
{
public:
    typedef HRESULT (T::*InvokeFn)(IMFAsyncResult *asyncResult);

    AsyncCallback(T *parent, InvokeFn fn) : m_parent(parent), m_invokeFn(fn)
    {
    }

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void** ppv)
    {
        if (!ppv)
            return E_POINTER;

        if (iid == __uuidof(IUnknown)) {
            *ppv = static_cast<IUnknown*>(static_cast<IMFAsyncCallback*>(this));
        } else if (iid == __uuidof(IMFAsyncCallback)) {
            *ppv = static_cast<IMFAsyncCallback*>(this);
        } else {
            *ppv = NULL;
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }

    STDMETHODIMP_(ULONG) AddRef() {
        // Delegate to parent class.
        return m_parent->AddRef();
    }
    STDMETHODIMP_(ULONG) Release() {
        // Delegate to parent class.
        return m_parent->Release();
    }

    // IMFAsyncCallback methods
    STDMETHODIMP GetParameters(DWORD*, DWORD*)
    {
        // Implementation of this method is optional.
        return E_NOTIMPL;
    }

    STDMETHODIMP Invoke(IMFAsyncResult* asyncResult)
    {
        return (m_parent->*m_invokeFn)(asyncResult);
    }

    T *m_parent;
    InvokeFn m_invokeFn;
};

class Scheduler
{
public:
    enum ScheduleEvent
    {
        Terminate =    WM_USER,
        Schedule =     WM_USER + 1,
        Flush =        WM_USER + 2
    };

    Scheduler(EVRCustomPresenter *presenter);
    ~Scheduler();

    void setFrameRate(const MFRatio &fps);
    void setClockRate(float rate) { m_playbackRate = rate; }

    const LONGLONG &lastSampleTime() const { return m_lastSampleTime; }
    const LONGLONG &frameDuration() const { return m_perFrameInterval; }

    HRESULT startScheduler(IMFClock *clock);
    HRESULT stopScheduler();

    HRESULT scheduleSample(IMFSample *sample, bool presentNow);
    HRESULT processSamplesInQueue(LONG *nextSleep);
    HRESULT processSample(IMFSample *sample, LONG *nextSleep);
    HRESULT flush();

    bool areSamplesScheduled();

    // ThreadProc for the scheduler thread.
    static DWORD WINAPI schedulerThreadProc(LPVOID parameter);

private:
    DWORD schedulerThreadProcPrivate();

    EVRCustomPresenter *m_presenter;

    QQueue<IMFSample*> m_scheduledSamples; // Samples waiting to be presented.

    IMFClock *m_clock; // Presentation clock. Can be NULL.

    DWORD m_threadID;
    HANDLE m_schedulerThread;
    HANDLE m_threadReadyEvent;
    HANDLE m_flushEvent;

    float m_playbackRate;
    MFTIME m_perFrameInterval; // Duration of each frame.
    LONGLONG m_perFrame_1_4th; // 1/4th of the frame duration.
    MFTIME m_lastSampleTime; // Most recent sample time.

    QMutex m_mutex;
};

class SamplePool
{
public:
    SamplePool();
    ~SamplePool();

    HRESULT initialize(QList<IMFSample*> &samples);
    HRESULT clear();

    HRESULT getSample(IMFSample **sample);
    HRESULT returnSample(IMFSample *sample);

private:
    QMutex m_mutex;
    QList<IMFSample*> m_videoSampleQueue;
    bool m_initialized;
};

class EVRCustomPresenter
        : public QObject
        , public IMFVideoDeviceID
        , public IMFVideoPresenter // Inherits IMFClockStateSink
        , public IMFRateSupport
        , public IMFGetService
        , public IMFTopologyServiceLookupClient
{
public:
    // Defines the state of the presenter.
    enum RenderState
    {
        RenderStarted = 1,
        RenderStopped,
        RenderPaused,
        RenderShutdown  // Initial state.
    };

    // Defines the presenter's state with respect to frame-stepping.
    enum FrameStepState
    {
        FrameStepNone,             // Not frame stepping.
        FrameStepWaitingStart,     // Frame stepping, but the clock is not started.
        FrameStepPending,          // Clock is started. Waiting for samples.
        FrameStepScheduled,        // Submitted a sample for rendering.
        FrameStepComplete          // Sample was rendered.
    };

    enum PresenterEvents
    {
        StartSurface = QEvent::User,
        StopSurface = QEvent::User + 1,
        PresentSample = QEvent::User + 2
    };

    EVRCustomPresenter(QAbstractVideoSurface *surface = 0);
    ~EVRCustomPresenter();

    bool isValid() const;

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void ** ppv);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IMFGetService methods
    STDMETHODIMP GetService(REFGUID guidService, REFIID riid, LPVOID *ppvObject);

    // IMFVideoPresenter methods
    STDMETHODIMP ProcessMessage(MFVP_MESSAGE_TYPE message, ULONG_PTR param);
    STDMETHODIMP GetCurrentMediaType(IMFVideoMediaType** mediaType);

    // IMFClockStateSink methods
    STDMETHODIMP OnClockStart(MFTIME systemTime, LONGLONG clockStartOffset);
    STDMETHODIMP OnClockStop(MFTIME systemTime);
    STDMETHODIMP OnClockPause(MFTIME systemTime);
    STDMETHODIMP OnClockRestart(MFTIME systemTime);
    STDMETHODIMP OnClockSetRate(MFTIME systemTime, float rate);

    // IMFRateSupport methods
    STDMETHODIMP GetSlowestRate(MFRATE_DIRECTION direction, BOOL thin, float *rate);
    STDMETHODIMP GetFastestRate(MFRATE_DIRECTION direction, BOOL thin, float *rate);
    STDMETHODIMP IsRateSupported(BOOL thin, float rate, float *nearestSupportedRate);

    // IMFVideoDeviceID methods
    STDMETHODIMP GetDeviceID(IID* deviceID);

    // IMFTopologyServiceLookupClient methods
    STDMETHODIMP InitServicePointers(IMFTopologyServiceLookup *lookup);
    STDMETHODIMP ReleaseServicePointers();

    void supportedFormatsChanged();
    void setSurface(QAbstractVideoSurface *surface);

    void startSurface();
    void stopSurface();
    void presentSample(IMFSample *sample);

    bool event(QEvent *);

private:
    HRESULT checkShutdown() const
    {
        if (m_renderState == RenderShutdown)
            return MF_E_SHUTDOWN;
        else
            return S_OK;
    }

    // The "active" state is started or paused.
    inline bool isActive() const
    {
        return ((m_renderState == RenderStarted) || (m_renderState == RenderPaused));
    }

    // Scrubbing occurs when the frame rate is 0.
    inline bool isScrubbing() const { return m_playbackRate == 0.0f; }

    // Send an event to the EVR through its IMediaEventSink interface.
    void notifyEvent(long eventCode, LONG_PTR param1, LONG_PTR param2)
    {
        if (m_mediaEventSink)
            m_mediaEventSink->Notify(eventCode, param1, param2);
    }

    float getMaxRate(bool thin);

    // Mixer operations
    HRESULT configureMixer(IMFTransform *mixer);

    // Formats
    HRESULT createOptimalVideoType(IMFMediaType* proposed, IMFMediaType **optimal);
    HRESULT setMediaType(IMFMediaType *mediaType);
    HRESULT isMediaTypeSupported(IMFMediaType *mediaType);

    // Message handlers
    HRESULT flush();
    HRESULT renegotiateMediaType();
    HRESULT processInputNotify();
    HRESULT beginStreaming();
    HRESULT endStreaming();
    HRESULT checkEndOfStream();

    // Managing samples
    void processOutputLoop();
    HRESULT processOutput();
    HRESULT deliverSample(IMFSample *sample, bool repaint);
    HRESULT trackSample(IMFSample *sample);
    void releaseResources();

    // Frame-stepping
    HRESULT prepareFrameStep(DWORD steps);
    HRESULT startFrameStep();
    HRESULT deliverFrameStepSample(IMFSample *sample);
    HRESULT completeFrameStep(IMFSample *sample);
    HRESULT cancelFrameStep();

    // Callback when a video sample is released.
    HRESULT onSampleFree(IMFAsyncResult *result);
    AsyncCallback<EVRCustomPresenter> m_sampleFreeCB;

    // Holds information related to frame-stepping.
    struct FrameStep
    {
        FrameStep()
            : state(FrameStepNone)
            , steps(0)
            , sampleNoRef(0)
        {
        }

        FrameStepState state;
        QList<IMFSample*> samples;
        DWORD steps;
        DWORD_PTR sampleNoRef;
    };

    long m_refCount;

    RenderState m_renderState;
    FrameStep m_frameStep;

    QMutex m_mutex;

    // Samples and scheduling
    Scheduler m_scheduler; // Manages scheduling of samples.
    SamplePool m_samplePool; // Pool of allocated samples.
    DWORD m_tokenCounter; // Counter. Incremented whenever we create new samples.

    // Rendering state
    bool m_sampleNotify; // Did the mixer signal it has an input sample?
    bool m_repaint; // Do we need to repaint the last sample?
    bool m_prerolled; // Have we presented at least one sample?
    bool m_endStreaming; // Did we reach the end of the stream (EOS)?

    MFVideoNormalizedRect m_sourceRect;
    float m_playbackRate;

    D3DPresentEngine *m_presentEngine; // Rendering engine. (Never null if the constructor succeeds.)

    IMFClock *m_clock; // The EVR's clock.
    IMFTransform *m_mixer; // The EVR's mixer.
    IMediaEventSink *m_mediaEventSink; // The EVR's event-sink interface.
    IMFMediaType *m_mediaType; // Output media type

    QAbstractVideoSurface *m_surface;
    bool m_canRenderToSurface;

    IMFSample *m_sampleToPresent;
};

bool qt_evr_setCustomPresenter(IUnknown *evr, EVRCustomPresenter *presenter);

#endif // EVRCUSTOMPRESENTER_H
