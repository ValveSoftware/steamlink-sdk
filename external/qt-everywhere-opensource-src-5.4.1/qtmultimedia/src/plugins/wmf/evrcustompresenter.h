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

#ifndef EVRCUSTOMPRESENTER_H
#define EVRCUSTOMPRESENTER_H

#include <QObject>
#include <qmutex.h>
#include <qqueue.h>
#include <evr.h>
#include "mfactivate.h"

QT_BEGIN_NAMESPACE

class D3DPresentEngine;
class QAbstractVideoSurface;

class Scheduler
{
public:
    enum ScheduleEvent
    {
        Terminate =    WM_USER,
        Schedule =     WM_USER + 1,
        Flush =        WM_USER + 2
    };

    Scheduler();
    ~Scheduler();

    void setCallback(QObject *cb) {
        m_CB = cb;
    }

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

    // ThreadProc for the scheduler thread.
    static DWORD WINAPI schedulerThreadProc(LPVOID parameter);

private:
    DWORD schedulerThreadProcPrivate();

    QQueue<IMFSample*> m_scheduledSamples; // Samples waiting to be presented.

    IMFClock *m_clock; // Presentation clock. Can be NULL.
    QObject *m_CB; // Weak reference; do not delete.

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
    BOOL areSamplesPending();

private:
    QMutex m_mutex;
    QList<IMFSample*> m_videoSampleQueue;
    bool m_initialized;
    DWORD m_pending;
};

class EVRCustomPresenter
        : public QObject
        , public IMFVideoDeviceID
        , public IMFVideoPresenter // Inherits IMFClockStateSink
        , public IMFRateSupport
        , public IMFGetService
        , public IMFTopologyServiceLookupClient
{
    Q_OBJECT

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

    EVRCustomPresenter();
    ~EVRCustomPresenter();

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

private Q_SLOTS:
    void startSurface();
    void stopSurface();

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
    AsyncCallback<EVRCustomPresenter>   m_sampleFreeCB;

    // Holds information related to frame-stepping.
    struct FrameStep
    {
        FrameStep()
            : state(FrameStepNone)
            , steps(0)
            , sampleNoRef(NULL)
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

    D3DPresentEngine *m_D3DPresentEngine; // Rendering engine. (Never null if the constructor succeeds.)

    IMFClock *m_clock; // The EVR's clock.
    IMFTransform *m_mixer; // The EVR's mixer.
    IMediaEventSink *m_mediaEventSink; // The EVR's event-sink interface.
    IMFMediaType *m_mediaType; // Output media type

    QAbstractVideoSurface *m_surface;
    QList<DWORD> m_supportedGLFormats;
};

class EVRCustomPresenterActivate : public MFAbstractActivate
{
public:
    EVRCustomPresenterActivate();
    ~EVRCustomPresenterActivate()
    { }

    STDMETHODIMP ActivateObject(REFIID riid, void **ppv);
    STDMETHODIMP ShutdownObject();
    STDMETHODIMP DetachObject();

    void setSurface(QAbstractVideoSurface *surface);
    void supportedFormatsChanged();

private:
    EVRCustomPresenter *m_presenter;
    QAbstractVideoSurface *m_surface;
    QMutex m_mutex;
};

QT_END_NAMESPACE

#endif // EVRCUSTOMPRESENTER_H
