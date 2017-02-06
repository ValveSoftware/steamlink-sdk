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

#include "evrcustompresenter.h"

#include "evrd3dpresentengine.h"
#include "evrhelpers.h"

#include <QtCore/qmutex.h>
#include <QtCore/qvarlengtharray.h>
#include <QtCore/qrect.h>
#include <qabstractvideosurface.h>
#include <qthread.h>
#include <qcoreapplication.h>
#include <qmath.h>
#include <QtCore/qdebug.h>
#include <float.h>
#include <evcode.h>

const static MFRatio g_DefaultFrameRate = { 30, 1 };
static const DWORD SCHEDULER_TIMEOUT = 5000;
static const MFTIME ONE_SECOND = 10000000;
static const LONG   ONE_MSEC = 1000;

// Function declarations.
static HRESULT setDesiredSampleTime(IMFSample *sample, const LONGLONG& hnsSampleTime, const LONGLONG& hnsDuration);
static HRESULT clearDesiredSampleTime(IMFSample *sample);
static HRESULT setMixerSourceRect(IMFTransform *mixer, const MFVideoNormalizedRect& nrcSource);
static QVideoFrame::PixelFormat pixelFormatFromMediaType(IMFMediaType *type);

static inline LONG MFTimeToMsec(const LONGLONG& time)
{
    return (LONG)(time / (ONE_SECOND / ONE_MSEC));
}

bool qt_evr_setCustomPresenter(IUnknown *evr, EVRCustomPresenter *presenter)
{
    if (!evr || !presenter)
        return false;

    HRESULT result = E_FAIL;

    IMFVideoRenderer *renderer = NULL;
    if (SUCCEEDED(evr->QueryInterface(IID_PPV_ARGS(&renderer)))) {
        result = renderer->InitializeRenderer(NULL, presenter);
        renderer->Release();
    }

    return result == S_OK;
}

class PresentSampleEvent : public QEvent
{
public:
    PresentSampleEvent(IMFSample *sample)
        : QEvent(QEvent::Type(EVRCustomPresenter::PresentSample))
        , m_sample(sample)
    {
        if (m_sample)
            m_sample->AddRef();
    }

    ~PresentSampleEvent()
    {
        if (m_sample)
            m_sample->Release();
    }

    IMFSample *sample() const { return m_sample; }

private:
    IMFSample *m_sample;
};

Scheduler::Scheduler(EVRCustomPresenter *presenter)
    : m_presenter(presenter)
    , m_clock(NULL)
    , m_threadID(0)
    , m_schedulerThread(0)
    , m_threadReadyEvent(0)
    , m_flushEvent(0)
    , m_playbackRate(1.0f)
    , m_perFrameInterval(0)
    , m_perFrame_1_4th(0)
    , m_lastSampleTime(0)
{
}

Scheduler::~Scheduler()
{
    qt_evr_safe_release(&m_clock);
    for (int i = 0; i < m_scheduledSamples.size(); ++i)
        m_scheduledSamples[i]->Release();
    m_scheduledSamples.clear();
}

void Scheduler::setFrameRate(const MFRatio& fps)
{
    UINT64 AvgTimePerFrame = 0;

    // Convert to a duration.
    MFFrameRateToAverageTimePerFrame(fps.Numerator, fps.Denominator, &AvgTimePerFrame);

    m_perFrameInterval = (MFTIME)AvgTimePerFrame;

    // Calculate 1/4th of this value, because we use it frequently.
    m_perFrame_1_4th = m_perFrameInterval / 4;
}

HRESULT Scheduler::startScheduler(IMFClock *clock)
{
    if (m_schedulerThread)
        return E_UNEXPECTED;

    HRESULT hr = S_OK;
    DWORD dwID = 0;
    HANDLE hObjects[2];
    DWORD dwWait = 0;

    if (m_clock)
        m_clock->Release();
    m_clock = clock;
    if (m_clock)
        m_clock->AddRef();

    // Set a high the timer resolution (ie, short timer period).
    timeBeginPeriod(1);

    // Create an event to wait for the thread to start.
    m_threadReadyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!m_threadReadyEvent) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto done;
    }

    // Create an event to wait for flush commands to complete.
    m_flushEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!m_flushEvent) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto done;
    }

    // Create the scheduler thread.
    m_schedulerThread = CreateThread(NULL, 0, schedulerThreadProc, (LPVOID)this, 0, &dwID);
    if (!m_schedulerThread) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto done;
    }

    // Wait for the thread to signal the "thread ready" event.
    hObjects[0] = m_threadReadyEvent;
    hObjects[1] = m_schedulerThread;
    dwWait = WaitForMultipleObjects(2, hObjects, FALSE, INFINITE);  // Wait for EITHER of these handles.
    if (WAIT_OBJECT_0 != dwWait) {
        // The thread terminated early for some reason. This is an error condition.
        CloseHandle(m_schedulerThread);
        m_schedulerThread = NULL;

        hr = E_UNEXPECTED;
        goto done;
    }

    m_threadID = dwID;

done:
    // Regardless success/failure, we are done using the "thread ready" event.
    if (m_threadReadyEvent) {
        CloseHandle(m_threadReadyEvent);
        m_threadReadyEvent = NULL;
    }
    return hr;
}

HRESULT Scheduler::stopScheduler()
{
    if (!m_schedulerThread)
        return S_OK;

    // Ask the scheduler thread to exit.
    PostThreadMessage(m_threadID, Terminate, 0, 0);

    // Wait for the thread to exit.
    WaitForSingleObject(m_schedulerThread, INFINITE);

    // Close handles.
    CloseHandle(m_schedulerThread);
    m_schedulerThread = NULL;

    CloseHandle(m_flushEvent);
    m_flushEvent = NULL;

    // Discard samples.
    m_mutex.lock();
    for (int i = 0; i < m_scheduledSamples.size(); ++i)
        m_scheduledSamples[i]->Release();
    m_scheduledSamples.clear();
    m_mutex.unlock();

    // Restore the timer resolution.
    timeEndPeriod(1);

    return S_OK;
}

HRESULT Scheduler::flush()
{
    if (m_schedulerThread) {
        // Ask the scheduler thread to flush.
        PostThreadMessage(m_threadID, Flush, 0 , 0);

        // Wait for the scheduler thread to signal the flush event,
        // OR for the thread to terminate.
        HANDLE objects[] = { m_flushEvent, m_schedulerThread };

        WaitForMultipleObjects(ARRAYSIZE(objects), objects, FALSE, SCHEDULER_TIMEOUT);
    }

    return S_OK;
}

bool Scheduler::areSamplesScheduled()
{
    QMutexLocker locker(&m_mutex);
    return m_scheduledSamples.count() > 0;
}

HRESULT Scheduler::scheduleSample(IMFSample *sample, bool presentNow)
{
    if (!m_schedulerThread)
        return MF_E_NOT_INITIALIZED;

    HRESULT hr = S_OK;
    DWORD dwExitCode = 0;

    GetExitCodeThread(m_schedulerThread, &dwExitCode);
    if (dwExitCode != STILL_ACTIVE)
        return E_FAIL;

    if (presentNow || !m_clock) {
        m_presenter->presentSample(sample);
    } else {
        // Queue the sample and ask the scheduler thread to wake up.
        m_mutex.lock();
        sample->AddRef();
        m_scheduledSamples.enqueue(sample);
        m_mutex.unlock();

        if (SUCCEEDED(hr))
            PostThreadMessage(m_threadID, Schedule, 0, 0);
    }

    return hr;
}

HRESULT Scheduler::processSamplesInQueue(LONG *nextSleep)
{
    HRESULT hr = S_OK;
    LONG wait = 0;
    IMFSample *sample = NULL;

    // Process samples until the queue is empty or until the wait time > 0.
    while (!m_scheduledSamples.isEmpty()) {
        m_mutex.lock();
        sample = m_scheduledSamples.dequeue();
        m_mutex.unlock();

        // Process the next sample in the queue. If the sample is not ready
        // for presentation. the value returned in wait is > 0, which
        // means the scheduler should sleep for that amount of time.

        hr = processSample(sample, &wait);
        qt_evr_safe_release(&sample);

        if (FAILED(hr) || wait > 0)
            break;
    }

    // If the wait time is zero, it means we stopped because the queue is
    // empty (or an error occurred). Set the wait time to infinite; this will
    // make the scheduler thread sleep until it gets another thread message.
    if (wait == 0)
        wait = INFINITE;

    *nextSleep = wait;
    return hr;
}

HRESULT Scheduler::processSample(IMFSample *sample, LONG *pNextSleep)
{
    HRESULT hr = S_OK;

    LONGLONG hnsPresentationTime = 0;
    LONGLONG hnsTimeNow = 0;
    MFTIME   hnsSystemTime = 0;

    bool presentNow = true;
    LONG nextSleep = 0;

    if (m_clock) {
        // Get the sample's time stamp. It is valid for a sample to
        // have no time stamp.
        hr = sample->GetSampleTime(&hnsPresentationTime);

        // Get the clock time. (But if the sample does not have a time stamp,
        // we don't need the clock time.)
        if (SUCCEEDED(hr))
            hr = m_clock->GetCorrelatedTime(0, &hnsTimeNow, &hnsSystemTime);

        // Calculate the time until the sample's presentation time.
        // A negative value means the sample is late.
        LONGLONG hnsDelta = hnsPresentationTime - hnsTimeNow;
        if (m_playbackRate < 0) {
            // For reverse playback, the clock runs backward. Therefore, the
            // delta is reversed.
            hnsDelta = - hnsDelta;
        }

        if (hnsDelta < - m_perFrame_1_4th) {
            // This sample is late.
            presentNow = true;
        } else if (hnsDelta > (3 * m_perFrame_1_4th)) {
            // This sample is still too early. Go to sleep.
            nextSleep = MFTimeToMsec(hnsDelta - (3 * m_perFrame_1_4th));

            // Adjust the sleep time for the clock rate. (The presentation clock runs
            // at m_fRate, but sleeping uses the system clock.)
            if (m_playbackRate != 0)
                nextSleep = (LONG)(nextSleep / qFabs(m_playbackRate));

            // Don't present yet.
            presentNow = false;
        }
    }

    if (presentNow) {
        m_presenter->presentSample(sample);
    } else {
        // The sample is not ready yet. Return it to the queue.
        m_mutex.lock();
        sample->AddRef();
        m_scheduledSamples.prepend(sample);
        m_mutex.unlock();
    }

    *pNextSleep = nextSleep;

    return hr;
}

DWORD WINAPI Scheduler::schedulerThreadProc(LPVOID parameter)
{
    Scheduler* scheduler = reinterpret_cast<Scheduler*>(parameter);
    if (!scheduler)
        return -1;
    return scheduler->schedulerThreadProcPrivate();
}

DWORD Scheduler::schedulerThreadProcPrivate()
{
    HRESULT hr = S_OK;
    MSG msg;
    LONG wait = INFINITE;
    bool exitThread = false;

    // Force the system to create a message queue for this thread.
    // (See MSDN documentation for PostThreadMessage.)
    PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

    // Signal to the scheduler that the thread is ready.
    SetEvent(m_threadReadyEvent);

    while (!exitThread) {
        // Wait for a thread message OR until the wait time expires.
        DWORD result = MsgWaitForMultipleObjects(0, NULL, FALSE, wait, QS_POSTMESSAGE);

        if (result == WAIT_TIMEOUT) {
            // If we timed out, then process the samples in the queue
            hr = processSamplesInQueue(&wait);
            if (FAILED(hr))
                exitThread = true;
        }

        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            bool processSamples = true;

            switch (msg.message) {
            case Terminate:
                exitThread = true;
                break;
            case Flush:
                // Flushing: Clear the sample queue and set the event.
                m_mutex.lock();
                for (int i = 0; i < m_scheduledSamples.size(); ++i)
                    m_scheduledSamples[i]->Release();
                m_scheduledSamples.clear();
                m_mutex.unlock();
                wait = INFINITE;
                SetEvent(m_flushEvent);
                break;
            case Schedule:
                // Process as many samples as we can.
                if (processSamples) {
                    hr = processSamplesInQueue(&wait);
                    if (FAILED(hr))
                        exitThread = true;
                    processSamples = (wait != (LONG)INFINITE);
                }
                break;
            }
        }

    }

    return (SUCCEEDED(hr) ? 0 : 1);
}


SamplePool::SamplePool()
    : m_initialized(false)
{
}

SamplePool::~SamplePool()
{
    clear();
}

HRESULT SamplePool::getSample(IMFSample **sample)
{
    QMutexLocker locker(&m_mutex);

    if (!m_initialized)
        return MF_E_NOT_INITIALIZED;

    if (m_videoSampleQueue.isEmpty())
        return MF_E_SAMPLEALLOCATOR_EMPTY;

    // Get a sample from the allocated queue.

    // It doesn't matter if we pull them from the head or tail of the list,
    // but when we get it back, we want to re-insert it onto the opposite end.
    // (see ReturnSample)

    IMFSample *taken = m_videoSampleQueue.takeFirst();

    // Give the sample to the caller.
    *sample = taken;
    (*sample)->AddRef();

    taken->Release();

    return S_OK;
}

HRESULT SamplePool::returnSample(IMFSample *sample)
{
    QMutexLocker locker(&m_mutex);

    if (!m_initialized)
        return MF_E_NOT_INITIALIZED;

    m_videoSampleQueue.append(sample);
    sample->AddRef();

    return S_OK;
}

HRESULT SamplePool::initialize(QList<IMFSample*> &samples)
{
    QMutexLocker locker(&m_mutex);

    if (m_initialized)
        return MF_E_INVALIDREQUEST;

    IMFSample *sample = NULL;

    // Move these samples into our allocated queue.
    for (int i = 0; i < samples.size(); ++i) {
        sample = samples.at(i);
        sample->AddRef();
        m_videoSampleQueue.append(sample);
    }

    m_initialized = true;

    for (int i = 0; i < samples.size(); ++i)
        samples[i]->Release();
    samples.clear();
    return S_OK;
}

HRESULT SamplePool::clear()
{
    QMutexLocker locker(&m_mutex);

    for (int i = 0; i < m_videoSampleQueue.size(); ++i)
        m_videoSampleQueue[i]->Release();
    m_videoSampleQueue.clear();
    m_initialized = false;

    return S_OK;
}


EVRCustomPresenter::EVRCustomPresenter(QAbstractVideoSurface *surface)
    : QObject()
    , m_sampleFreeCB(this, &EVRCustomPresenter::onSampleFree)
    , m_refCount(1)
    , m_renderState(RenderShutdown)
    , m_mutex(QMutex::Recursive)
    , m_scheduler(this)
    , m_tokenCounter(0)
    , m_sampleNotify(false)
    , m_repaint(false)
    , m_prerolled(false)
    , m_endStreaming(false)
    , m_playbackRate(1.0f)
    , m_presentEngine(new D3DPresentEngine)
    , m_clock(0)
    , m_mixer(0)
    , m_mediaEventSink(0)
    , m_mediaType(0)
    , m_surface(0)
    , m_canRenderToSurface(false)
    , m_sampleToPresent(0)
{
    // Initial source rectangle = (0,0,1,1)
    m_sourceRect.top = 0;
    m_sourceRect.left = 0;
    m_sourceRect.bottom = 1;
    m_sourceRect.right = 1;

    setSurface(surface);
}

EVRCustomPresenter::~EVRCustomPresenter()
{
    m_scheduler.flush();
    m_scheduler.stopScheduler();
    m_samplePool.clear();

    qt_evr_safe_release(&m_clock);
    qt_evr_safe_release(&m_mixer);
    qt_evr_safe_release(&m_mediaEventSink);
    qt_evr_safe_release(&m_mediaType);

    delete m_presentEngine;
}

HRESULT EVRCustomPresenter::QueryInterface(REFIID riid, void ** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    if (riid == IID_IMFGetService) {
        *ppvObject = static_cast<IMFGetService*>(this);
    } else if (riid == IID_IMFTopologyServiceLookupClient) {
        *ppvObject = static_cast<IMFTopologyServiceLookupClient*>(this);
    } else if (riid == IID_IMFVideoDeviceID) {
        *ppvObject = static_cast<IMFVideoDeviceID*>(this);
    } else if (riid == IID_IMFVideoPresenter) {
        *ppvObject = static_cast<IMFVideoPresenter*>(this);
    } else if (riid == IID_IMFRateSupport) {
        *ppvObject = static_cast<IMFRateSupport*>(this);
    } else if (riid == IID_IUnknown) {
        *ppvObject = static_cast<IUnknown*>(static_cast<IMFGetService*>(this));
    } else if (riid == IID_IMFClockStateSink) {
        *ppvObject = static_cast<IMFClockStateSink*>(this);
    } else {
        *ppvObject =  NULL;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

ULONG EVRCustomPresenter::AddRef()
{
    return InterlockedIncrement(&m_refCount);
}

ULONG EVRCustomPresenter::Release()
{
    ULONG uCount = InterlockedDecrement(&m_refCount);
    if (uCount == 0)
        delete this;
    return uCount;
}

HRESULT EVRCustomPresenter::GetService(REFGUID guidService, REFIID riid, LPVOID *ppvObject)
{
    HRESULT hr = S_OK;

    if (!ppvObject)
        return E_POINTER;

    // The only service GUID that we support is MR_VIDEO_RENDER_SERVICE.
    if (guidService != mr_VIDEO_RENDER_SERVICE)
        return MF_E_UNSUPPORTED_SERVICE;

    // First try to get the service interface from the D3DPresentEngine object.
    hr = m_presentEngine->getService(guidService, riid, ppvObject);
    if (FAILED(hr))
        // Next, check if this object supports the interface.
        hr = QueryInterface(riid, ppvObject);

    return hr;
}

HRESULT EVRCustomPresenter::GetDeviceID(IID* deviceID)
{
    if (!deviceID)
        return E_POINTER;

    *deviceID = iid_IDirect3DDevice9;

    return S_OK;
}

HRESULT EVRCustomPresenter::InitServicePointers(IMFTopologyServiceLookup *lookup)
{
    if (!lookup)
        return E_POINTER;

    HRESULT hr = S_OK;
    DWORD objectCount = 0;

    QMutexLocker locker(&m_mutex);

    // Do not allow initializing when playing or paused.
    if (isActive())
        return MF_E_INVALIDREQUEST;

    qt_evr_safe_release(&m_clock);
    qt_evr_safe_release(&m_mixer);
    qt_evr_safe_release(&m_mediaEventSink);

    // Ask for the clock. Optional, because the EVR might not have a clock.
    objectCount = 1;

    lookup->LookupService(MF_SERVICE_LOOKUP_GLOBAL, 0,
                          mr_VIDEO_RENDER_SERVICE, IID_PPV_ARGS(&m_clock),
                          &objectCount
                          );

    // Ask for the mixer. (Required.)
    objectCount = 1;

    hr = lookup->LookupService(MF_SERVICE_LOOKUP_GLOBAL, 0,
                               mr_VIDEO_MIXER_SERVICE, IID_PPV_ARGS(&m_mixer),
                               &objectCount
                               );

    if (FAILED(hr))
        return hr;

    // Make sure that we can work with this mixer.
    hr = configureMixer(m_mixer);
    if (FAILED(hr))
        return hr;

    // Ask for the EVR's event-sink interface. (Required.)
    objectCount = 1;

    hr = lookup->LookupService(MF_SERVICE_LOOKUP_GLOBAL, 0,
                               mr_VIDEO_RENDER_SERVICE, IID_PPV_ARGS(&m_mediaEventSink),
                               &objectCount
                               );

    if (SUCCEEDED(hr))
        m_renderState = RenderStopped;

    return hr;
}

HRESULT EVRCustomPresenter::ReleaseServicePointers()
{
    // Enter the shut-down state.
    m_mutex.lock();

    m_renderState = RenderShutdown;

    m_mutex.unlock();

    // Flush any samples that were scheduled.
    flush();

    // Clear the media type and release related resources.
    setMediaType(NULL);

    // Release all services that were acquired from InitServicePointers.
    qt_evr_safe_release(&m_clock);
    qt_evr_safe_release(&m_mixer);
    qt_evr_safe_release(&m_mediaEventSink);

    return S_OK;
}

bool EVRCustomPresenter::isValid() const
{
    return m_presentEngine->isValid() && m_canRenderToSurface;
}

HRESULT EVRCustomPresenter::ProcessMessage(MFVP_MESSAGE_TYPE message, ULONG_PTR param)
{
    HRESULT hr = S_OK;

    QMutexLocker locker(&m_mutex);

    hr = checkShutdown();
    if (FAILED(hr))
        return hr;

    switch (message) {
    // Flush all pending samples.
    case MFVP_MESSAGE_FLUSH:
        hr = flush();
        break;

    // Renegotiate the media type with the mixer.
    case MFVP_MESSAGE_INVALIDATEMEDIATYPE:
        hr = renegotiateMediaType();
        break;

    // The mixer received a new input sample.
    case MFVP_MESSAGE_PROCESSINPUTNOTIFY:
        hr = processInputNotify();
        break;

    // Streaming is about to start.
    case MFVP_MESSAGE_BEGINSTREAMING:
        hr = beginStreaming();
        break;

    // Streaming has ended. (The EVR has stopped.)
    case MFVP_MESSAGE_ENDSTREAMING:
        hr = endStreaming();
        break;

    // All input streams have ended.
    case MFVP_MESSAGE_ENDOFSTREAM:
        // Set the EOS flag.
        m_endStreaming = true;
        // Check if it's time to send the EC_COMPLETE event to the EVR.
        hr = checkEndOfStream();
        break;

    // Frame-stepping is starting.
    case MFVP_MESSAGE_STEP:
        hr = prepareFrameStep(DWORD(param));
        break;

    // Cancels frame-stepping.
    case MFVP_MESSAGE_CANCELSTEP:
        hr = cancelFrameStep();
        break;

    default:
        hr = E_INVALIDARG; // Unknown message. This case should never occur.
        break;
    }

    return hr;
}

HRESULT EVRCustomPresenter::GetCurrentMediaType(IMFVideoMediaType **mediaType)
{
    HRESULT hr = S_OK;

    if (!mediaType)
        return E_POINTER;

    *mediaType = NULL;

    QMutexLocker locker(&m_mutex);

    hr = checkShutdown();
    if (FAILED(hr))
        return hr;

    if (!m_mediaType)
        return MF_E_NOT_INITIALIZED;

    return m_mediaType->QueryInterface(IID_PPV_ARGS(mediaType));
}

HRESULT EVRCustomPresenter::OnClockStart(MFTIME, LONGLONG clockStartOffset)
{
    QMutexLocker locker(&m_mutex);

    // We cannot start after shutdown.
    HRESULT hr = checkShutdown();
    if (FAILED(hr))
        return hr;

    // Check if the clock is already active (not stopped).
    if (isActive()) {
        m_renderState = RenderStarted;

        // If the clock position changes while the clock is active, it
        // is a seek request. We need to flush all pending samples.
        if (clockStartOffset != PRESENTATION_CURRENT_POSITION)
            flush();
    } else {
        m_renderState = RenderStarted;

        // The clock has started from the stopped state.

        // Possibly we are in the middle of frame-stepping OR have samples waiting
        // in the frame-step queue. Deal with these two cases first:
        hr = startFrameStep();
        if (FAILED(hr))
            return hr;
    }

    // Now try to get new output samples from the mixer.
    processOutputLoop();

    return hr;
}

HRESULT EVRCustomPresenter::OnClockRestart(MFTIME)
{
    QMutexLocker locker(&m_mutex);

    HRESULT hr = checkShutdown();
    if (FAILED(hr))
        return hr;

    // The EVR calls OnClockRestart only while paused.

    m_renderState = RenderStarted;

    // Possibly we are in the middle of frame-stepping OR we have samples waiting
    // in the frame-step queue. Deal with these two cases first:
    hr = startFrameStep();
    if (FAILED(hr))
        return hr;

    // Now resume the presentation loop.
    processOutputLoop();

    return hr;
}

HRESULT EVRCustomPresenter::OnClockStop(MFTIME)
{
    QMutexLocker locker(&m_mutex);

    HRESULT hr = checkShutdown();
    if (FAILED(hr))
        return hr;

    if (m_renderState != RenderStopped) {
        m_renderState = RenderStopped;
        flush();

        // If we are in the middle of frame-stepping, cancel it now.
        if (m_frameStep.state != FrameStepNone)
            cancelFrameStep();
    }

    return S_OK;
}

HRESULT EVRCustomPresenter::OnClockPause(MFTIME)
{
    QMutexLocker locker(&m_mutex);

    // We cannot pause the clock after shutdown.
    HRESULT hr = checkShutdown();

    if (SUCCEEDED(hr))
        m_renderState = RenderPaused;

    return hr;
}

HRESULT EVRCustomPresenter::OnClockSetRate(MFTIME, float rate)
{
    // Note:
    // The presenter reports its maximum rate through the IMFRateSupport interface.
    // Here, we assume that the EVR honors the maximum rate.

    QMutexLocker locker(&m_mutex);

    HRESULT hr = checkShutdown();
    if (FAILED(hr))
        return hr;

    // If the rate is changing from zero (scrubbing) to non-zero, cancel the
    // frame-step operation.
    if ((m_playbackRate == 0.0f) && (rate != 0.0f)) {
        cancelFrameStep();
        for (int i = 0; i < m_frameStep.samples.size(); ++i)
            m_frameStep.samples[i]->Release();
        m_frameStep.samples.clear();
    }

    m_playbackRate = rate;

    // Tell the scheduler about the new rate.
    m_scheduler.setClockRate(rate);

    return S_OK;
}

HRESULT EVRCustomPresenter::GetSlowestRate(MFRATE_DIRECTION, BOOL, float *rate)
{
    if (!rate)
        return E_POINTER;

    QMutexLocker locker(&m_mutex);

    HRESULT hr = checkShutdown();

    if (SUCCEEDED(hr)) {
        // There is no minimum playback rate, so the minimum is zero.
        *rate = 0;
    }

    return S_OK;
}

HRESULT EVRCustomPresenter::GetFastestRate(MFRATE_DIRECTION direction, BOOL thin, float *rate)
{
    if (!rate)
        return E_POINTER;

    QMutexLocker locker(&m_mutex);

    float maxRate = 0.0f;

    HRESULT hr = checkShutdown();
    if (FAILED(hr))
        return hr;

    // Get the maximum *forward* rate.
    maxRate = getMaxRate(thin);

    // For reverse playback, it's the negative of maxRate.
    if (direction == MFRATE_REVERSE)
        maxRate = -maxRate;

    *rate = maxRate;

    return S_OK;
}

HRESULT EVRCustomPresenter::IsRateSupported(BOOL thin, float rate, float *nearestSupportedRate)
{
    QMutexLocker locker(&m_mutex);

    float maxRate = 0.0f;
    float nearestRate = rate;  // If we support rate, that is the nearest.

    HRESULT hr = checkShutdown();
    if (FAILED(hr))
        return hr;

    // Find the maximum forward rate.
    // Note: We have no minimum rate (that is, we support anything down to 0).
    maxRate = getMaxRate(thin);

    if (qFabs(rate) > maxRate) {
        // The (absolute) requested rate exceeds the maximum rate.
        hr = MF_E_UNSUPPORTED_RATE;

        // The nearest supported rate is maxRate.
        nearestRate = maxRate;
        if (rate < 0) {
            // Negative for reverse playback.
            nearestRate = -nearestRate;
        }
    }

    // Return the nearest supported rate.
    if (nearestSupportedRate)
        *nearestSupportedRate = nearestRate;

    return hr;
}

void EVRCustomPresenter::supportedFormatsChanged()
{
    QMutexLocker locker(&m_mutex);

    m_canRenderToSurface = false;
    m_presentEngine->setHint(D3DPresentEngine::RenderToTexture, false);

    // check if we can render to the surface (compatible formats)
    if (m_surface) {
        QList<QVideoFrame::PixelFormat> formats = m_surface->supportedPixelFormats(QAbstractVideoBuffer::GLTextureHandle);
        if (m_presentEngine->supportsTextureRendering() && formats.contains(QVideoFrame::Format_RGB32)) {
            m_presentEngine->setHint(D3DPresentEngine::RenderToTexture, true);
            m_canRenderToSurface = true;
        } else {
            formats = m_surface->supportedPixelFormats(QAbstractVideoBuffer::NoHandle);
            Q_FOREACH (QVideoFrame::PixelFormat format, formats) {
                if (SUCCEEDED(m_presentEngine->checkFormat(qt_evr_D3DFormatFromPixelFormat(format)))) {
                    m_canRenderToSurface = true;
                    break;
                }
            }
        }
    }

    // TODO: if media type already set, renegotiate?
}

void EVRCustomPresenter::setSurface(QAbstractVideoSurface *surface)
{
    m_mutex.lock();

    if (m_surface) {
        disconnect(m_surface, &QAbstractVideoSurface::supportedFormatsChanged,
                   this, &EVRCustomPresenter::supportedFormatsChanged);
    }

    m_surface = surface;

    if (m_surface) {
        connect(m_surface, &QAbstractVideoSurface::supportedFormatsChanged,
                this, &EVRCustomPresenter::supportedFormatsChanged);
    }

    m_mutex.unlock();

    supportedFormatsChanged();
}

HRESULT EVRCustomPresenter::configureMixer(IMFTransform *mixer)
{
    // Set the zoom rectangle (ie, the source clipping rectangle).
    return setMixerSourceRect(mixer, m_sourceRect);
}

HRESULT EVRCustomPresenter::renegotiateMediaType()
{
    HRESULT hr = S_OK;
    bool foundMediaType = false;

    IMFMediaType *mixerType = NULL;
    IMFMediaType *optimalType = NULL;

    if (!m_mixer)
        return MF_E_INVALIDREQUEST;

    // Loop through all of the mixer's proposed output types.
    DWORD typeIndex = 0;
    while (!foundMediaType && (hr != MF_E_NO_MORE_TYPES)) {
        qt_evr_safe_release(&mixerType);
        qt_evr_safe_release(&optimalType);

        // Step 1. Get the next media type supported by mixer.
        hr = m_mixer->GetOutputAvailableType(0, typeIndex++, &mixerType);
        if (FAILED(hr))
            break;

        // From now on, if anything in this loop fails, try the next type,
        // until we succeed or the mixer runs out of types.

        // Step 2. Check if we support this media type.
        if (SUCCEEDED(hr))
            hr = isMediaTypeSupported(mixerType);

        // Step 3. Adjust the mixer's type to match our requirements.
        if (SUCCEEDED(hr))
            hr = createOptimalVideoType(mixerType, &optimalType);

        // Step 4. Check if the mixer will accept this media type.
        if (SUCCEEDED(hr))
            hr = m_mixer->SetOutputType(0, optimalType, MFT_SET_TYPE_TEST_ONLY);

        // Step 5. Try to set the media type on ourselves.
        if (SUCCEEDED(hr))
            hr = setMediaType(optimalType);

        // Step 6. Set output media type on mixer.
        if (SUCCEEDED(hr)) {
            hr = m_mixer->SetOutputType(0, optimalType, 0);

            // If something went wrong, clear the media type.
            if (FAILED(hr))
                setMediaType(NULL);
        }

        if (SUCCEEDED(hr))
            foundMediaType = true;
    }

    qt_evr_safe_release(&mixerType);
    qt_evr_safe_release(&optimalType);

    return hr;
}

HRESULT EVRCustomPresenter::flush()
{
    m_prerolled = false;

    // The scheduler might have samples that are waiting for
    // their presentation time. Tell the scheduler to flush.

    // This call blocks until the scheduler threads discards all scheduled samples.
    m_scheduler.flush();

    // Flush the frame-step queue.
    for (int i = 0; i < m_frameStep.samples.size(); ++i)
        m_frameStep.samples[i]->Release();
    m_frameStep.samples.clear();

    if (m_renderState == RenderStopped) {
        // Repaint with black.
        presentSample(NULL);
    }

    return S_OK;
}

HRESULT EVRCustomPresenter::processInputNotify()
{
    HRESULT hr = S_OK;

    // Set the flag that says the mixer has a new sample.
    m_sampleNotify = true;

    if (!m_mediaType) {
        // We don't have a valid media type yet.
        hr = MF_E_TRANSFORM_TYPE_NOT_SET;
    } else {
        // Try to process an output sample.
        processOutputLoop();
    }
    return hr;
}

HRESULT EVRCustomPresenter::beginStreaming()
{
    HRESULT hr = S_OK;

    // Start the scheduler thread.
    hr = m_scheduler.startScheduler(m_clock);

    return hr;
}

HRESULT EVRCustomPresenter::endStreaming()
{
    HRESULT hr = S_OK;

    // Stop the scheduler thread.
    hr = m_scheduler.stopScheduler();

    return hr;
}

HRESULT EVRCustomPresenter::checkEndOfStream()
{
    if (!m_endStreaming) {
        // The EVR did not send the MFVP_MESSAGE_ENDOFSTREAM message.
        return S_OK;
    }

    if (m_sampleNotify) {
        // The mixer still has input.
        return S_OK;
    }

    if (m_scheduler.areSamplesScheduled()) {
        // Samples are still scheduled for rendering.
        return S_OK;
    }

    // Everything is complete. Now we can tell the EVR that we are done.
    notifyEvent(EC_COMPLETE, (LONG_PTR)S_OK, 0);
    m_endStreaming = false;
    return S_OK;
}

HRESULT EVRCustomPresenter::prepareFrameStep(DWORD steps)
{
    HRESULT hr = S_OK;

    // Cache the step count.
    m_frameStep.steps += steps;

    // Set the frame-step state.
    m_frameStep.state = FrameStepWaitingStart;

    // If the clock is are already running, we can start frame-stepping now.
    // Otherwise, we will start when the clock starts.
    if (m_renderState == RenderStarted)
        hr = startFrameStep();

    return hr;
}

HRESULT EVRCustomPresenter::startFrameStep()
{
    HRESULT hr = S_OK;
    IMFSample *sample = NULL;

    if (m_frameStep.state == FrameStepWaitingStart) {
        // We have a frame-step request, and are waiting for the clock to start.
        // Set the state to "pending," which means we are waiting for samples.
        m_frameStep.state = FrameStepPending;

        // If the frame-step queue already has samples, process them now.
        while (!m_frameStep.samples.isEmpty() && (m_frameStep.state == FrameStepPending)) {
            sample = m_frameStep.samples.takeFirst();

            hr = deliverFrameStepSample(sample);
            if (FAILED(hr))
                goto done;

            qt_evr_safe_release(&sample);

            // We break from this loop when:
            //   (a) the frame-step queue is empty, or
            //   (b) the frame-step operation is complete.
        }
    } else if (m_frameStep.state == FrameStepNone) {
        // We are not frame stepping. Therefore, if the frame-step queue has samples,
        // we need to process them normally.
        while (!m_frameStep.samples.isEmpty()) {
            sample = m_frameStep.samples.takeFirst();

            hr = deliverSample(sample, false);
            if (FAILED(hr))
                goto done;

            qt_evr_safe_release(&sample);
        }
    }

done:
    qt_evr_safe_release(&sample);
    return hr;
}

HRESULT EVRCustomPresenter::completeFrameStep(IMFSample *sample)
{
    HRESULT hr = S_OK;
    MFTIME sampleTime = 0;
    MFTIME systemTime = 0;

    // Update our state.
    m_frameStep.state = FrameStepComplete;
    m_frameStep.sampleNoRef = 0;

    // Notify the EVR that the frame-step is complete.
    notifyEvent(EC_STEP_COMPLETE, FALSE, 0); // FALSE = completed (not cancelled)

    // If we are scrubbing (rate == 0), also send the "scrub time" event.
    if (isScrubbing()) {
        // Get the time stamp from the sample.
        hr = sample->GetSampleTime(&sampleTime);
        if (FAILED(hr)) {
            // No time stamp. Use the current presentation time.
            if (m_clock)
                m_clock->GetCorrelatedTime(0, &sampleTime, &systemTime);

            hr = S_OK; // (Not an error condition.)
        }

        notifyEvent(EC_SCRUB_TIME, DWORD(sampleTime), DWORD(((sampleTime) >> 32) & 0xffffffff));
    }
    return hr;
}

HRESULT EVRCustomPresenter::cancelFrameStep()
{
    FrameStepState oldState = m_frameStep.state;

    m_frameStep.state = FrameStepNone;
    m_frameStep.steps = 0;
    m_frameStep.sampleNoRef = 0;
    // Don't clear the frame-step queue yet, because we might frame step again.

    if (oldState > FrameStepNone && oldState < FrameStepComplete) {
        // We were in the middle of frame-stepping when it was cancelled.
        // Notify the EVR.
        notifyEvent(EC_STEP_COMPLETE, TRUE, 0); // TRUE = cancelled
    }
    return S_OK;
}

HRESULT EVRCustomPresenter::createOptimalVideoType(IMFMediaType *proposedType, IMFMediaType **optimalType)
{
    HRESULT hr = S_OK;

    RECT rcOutput;
    ZeroMemory(&rcOutput, sizeof(rcOutput));

    MFVideoArea displayArea;
    ZeroMemory(&displayArea, sizeof(displayArea));

    IMFMediaType *mtOptimal = NULL;

    UINT64 size;
    int width;
    int height;

    // Clone the proposed type.

    hr = MFCreateMediaType(&mtOptimal);
    if (FAILED(hr))
        goto done;

    hr = proposedType->CopyAllItems(mtOptimal);
    if (FAILED(hr))
        goto done;

    // Modify the new type.

    // Set the pixel aspect ratio (PAR) to 1:1 (see assumption #1, above)
    // The ratio is packed in a single UINT64. A helper function is normally available for
    // that (MFSetAttributeRatio) but it's not correctly defined in MinGW 4.9.1.
    hr = mtOptimal->SetUINT64(MF_MT_PIXEL_ASPECT_RATIO, (((UINT64) 1) << 32) | ((UINT64) 1));
    if (FAILED(hr))
        goto done;

    hr = proposedType->GetUINT64(MF_MT_FRAME_SIZE, &size);
    width = int(HI32(size));
    height = int(LO32(size));
    rcOutput.left = 0;
    rcOutput.top = 0;
    rcOutput.right = width;
    rcOutput.bottom = height;

    // Set the geometric aperture, and disable pan/scan.
    displayArea = qt_evr_makeMFArea(0, 0, rcOutput.right, rcOutput.bottom);

    hr = mtOptimal->SetUINT32(MF_MT_PAN_SCAN_ENABLED, FALSE);
    if (FAILED(hr))
        goto done;

    hr = mtOptimal->SetBlob(MF_MT_GEOMETRIC_APERTURE, (UINT8*)&displayArea, sizeof(displayArea));
    if (FAILED(hr))
        goto done;

    // Set the pan/scan aperture and the minimum display aperture. We don't care
    // about them per se, but the mixer will reject the type if these exceed the
    // frame dimentions.
    hr = mtOptimal->SetBlob(MF_MT_PAN_SCAN_APERTURE, (UINT8*)&displayArea, sizeof(displayArea));
    if (FAILED(hr))
        goto done;

    hr = mtOptimal->SetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE, (UINT8*)&displayArea, sizeof(displayArea));
    if (FAILED(hr))
        goto done;

    // Return the pointer to the caller.
    *optimalType = mtOptimal;
    (*optimalType)->AddRef();

done:
    qt_evr_safe_release(&mtOptimal);
    return hr;

}

HRESULT EVRCustomPresenter::setMediaType(IMFMediaType *mediaType)
{
    // Note: mediaType can be NULL (to clear the type)

    // Clearing the media type is allowed in any state (including shutdown).
    if (!mediaType) {
        stopSurface();
        qt_evr_safe_release(&m_mediaType);
        releaseResources();
        return S_OK;
    }

    MFRatio fps = { 0, 0 };
    QList<IMFSample*> sampleQueue;

    IMFSample *sample = NULL;

    // Cannot set the media type after shutdown.
    HRESULT hr = checkShutdown();
    if (FAILED(hr))
        goto done;

    // Check if the new type is actually different.
    // Note: This function safely handles NULL input parameters.
    if (qt_evr_areMediaTypesEqual(m_mediaType, mediaType))
        goto done; // Nothing more to do.

    // We're really changing the type. First get rid of the old type.
    qt_evr_safe_release(&m_mediaType);
    releaseResources();

    // Initialize the presenter engine with the new media type.
    // The presenter engine allocates the samples.

    hr = m_presentEngine->createVideoSamples(mediaType, sampleQueue);
    if (FAILED(hr))
        goto done;

    // Mark each sample with our token counter. If this batch of samples becomes
    // invalid, we increment the counter, so that we know they should be discarded.
    for (int i = 0; i < sampleQueue.size(); ++i) {
        sample = sampleQueue.at(i);

        hr = sample->SetUINT32(MFSamplePresenter_SampleCounter, m_tokenCounter);
        if (FAILED(hr))
            goto done;
    }

    // Add the samples to the sample pool.
    hr = m_samplePool.initialize(sampleQueue);
    if (FAILED(hr))
        goto done;

    // Set the frame rate on the scheduler.
    if (SUCCEEDED(qt_evr_getFrameRate(mediaType, &fps)) && (fps.Numerator != 0) && (fps.Denominator != 0)) {
        m_scheduler.setFrameRate(fps);
    } else {
        // NOTE: The mixer's proposed type might not have a frame rate, in which case
        // we'll use an arbitrary default. (Although it's unlikely the video source
        // does not have a frame rate.)
        m_scheduler.setFrameRate(g_DefaultFrameRate);
    }

    // Store the media type.
    m_mediaType = mediaType;
    m_mediaType->AddRef();

    startSurface();

done:
    if (FAILED(hr))
        releaseResources();
    return hr;
}

HRESULT EVRCustomPresenter::isMediaTypeSupported(IMFMediaType *proposed)
{
    D3DFORMAT d3dFormat = D3DFMT_UNKNOWN;
    BOOL compressed = FALSE;
    MFVideoInterlaceMode interlaceMode = MFVideoInterlace_Unknown;
    MFVideoArea videoCropArea;
    UINT32 width = 0, height = 0;

    // Validate the format.
    HRESULT hr = qt_evr_getFourCC(proposed, (DWORD*)&d3dFormat);
    if (FAILED(hr))
        return hr;

    QVideoFrame::PixelFormat pixelFormat = pixelFormatFromMediaType(proposed);
    if (pixelFormat == QVideoFrame::Format_Invalid)
        return MF_E_INVALIDMEDIATYPE;

    // When not rendering to texture, only accept pixel formats supported by the video surface
    if (!m_presentEngine->isTextureRenderingEnabled()
            && m_surface
            && !m_surface->supportedPixelFormats().contains(pixelFormat)) {
        return MF_E_INVALIDMEDIATYPE;
    }

    // Reject compressed media types.
    hr = proposed->IsCompressedFormat(&compressed);
    if (FAILED(hr))
        return hr;

    if (compressed)
        return MF_E_INVALIDMEDIATYPE;

    // The D3DPresentEngine checks whether surfaces can be created using this format
    hr = m_presentEngine->checkFormat(d3dFormat);
    if (FAILED(hr))
        return hr;

    // Reject interlaced formats.
    hr = proposed->GetUINT32(MF_MT_INTERLACE_MODE, (UINT32*)&interlaceMode);
    if (FAILED(hr))
        return hr;

    if (interlaceMode != MFVideoInterlace_Progressive)
        return MF_E_INVALIDMEDIATYPE;

    hr = MFGetAttributeSize(proposed, MF_MT_FRAME_SIZE, &width, &height);
    if (FAILED(hr))
        return hr;

    // Validate the various apertures (cropping regions) against the frame size.
    // Any of these apertures may be unspecified in the media type, in which case
    // we ignore it. We just want to reject invalid apertures.

    if (SUCCEEDED(proposed->GetBlob(MF_MT_PAN_SCAN_APERTURE, (UINT8*)&videoCropArea, sizeof(videoCropArea), NULL)))
        hr = qt_evr_validateVideoArea(videoCropArea, width, height);

    if (SUCCEEDED(proposed->GetBlob(MF_MT_GEOMETRIC_APERTURE, (UINT8*)&videoCropArea, sizeof(videoCropArea), NULL)))
        hr = qt_evr_validateVideoArea(videoCropArea, width, height);

    if (SUCCEEDED(proposed->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE, (UINT8*)&videoCropArea, sizeof(videoCropArea), NULL)))
        hr = qt_evr_validateVideoArea(videoCropArea, width, height);

    return hr;
}

void EVRCustomPresenter::processOutputLoop()
{
    HRESULT hr = S_OK;

    // Process as many samples as possible.
    while (hr == S_OK) {
        // If the mixer doesn't have a new input sample, break from the loop.
        if (!m_sampleNotify) {
            hr = MF_E_TRANSFORM_NEED_MORE_INPUT;
            break;
        }

        // Try to process a sample.
        hr = processOutput();

        // NOTE: ProcessOutput can return S_FALSE to indicate it did not
        // process a sample. If so, break out of the loop.
    }

    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
        // The mixer has run out of input data. Check for end-of-stream.
        checkEndOfStream();
    }
}

HRESULT EVRCustomPresenter::processOutput()
{
    HRESULT hr = S_OK;
    DWORD status = 0;
    LONGLONG mixerStartTime = 0, mixerEndTime = 0;
    MFTIME systemTime = 0;
    BOOL repaint = m_repaint; // Temporarily store this state flag.

    MFT_OUTPUT_DATA_BUFFER dataBuffer;
    ZeroMemory(&dataBuffer, sizeof(dataBuffer));

    IMFSample *sample = NULL;

    // If the clock is not running, we present the first sample,
    // and then don't present any more until the clock starts.

    if ((m_renderState != RenderStarted) && !m_repaint && m_prerolled)
        return S_FALSE;

    // Make sure we have a pointer to the mixer.
    if (!m_mixer)
        return MF_E_INVALIDREQUEST;

    // Try to get a free sample from the video sample pool.
    hr = m_samplePool.getSample(&sample);
    if (hr == MF_E_SAMPLEALLOCATOR_EMPTY) {
        // No free samples. Try again when a sample is released.
        return S_FALSE;
    } else if (FAILED(hr)) {
        return hr;
    }

    // From now on, we have a valid video sample pointer, where the mixer will
    // write the video data.

    if (m_repaint) {
        // Repaint request. Ask the mixer for the most recent sample.
        setDesiredSampleTime(sample, m_scheduler.lastSampleTime(), m_scheduler.frameDuration());

        m_repaint = false; // OK to clear this flag now.
    } else {
        // Not a repaint request. Clear the desired sample time; the mixer will
        // give us the next frame in the stream.
        clearDesiredSampleTime(sample);

        if (m_clock) {
            // Latency: Record the starting time for ProcessOutput.
            m_clock->GetCorrelatedTime(0, &mixerStartTime, &systemTime);
        }
    }

    // Now we are ready to get an output sample from the mixer.
    dataBuffer.dwStreamID = 0;
    dataBuffer.pSample = sample;
    dataBuffer.dwStatus = 0;

    hr = m_mixer->ProcessOutput(0, 1, &dataBuffer, &status);

    if (FAILED(hr)) {
        // Return the sample to the pool.
        HRESULT hr2 = m_samplePool.returnSample(sample);
        if (FAILED(hr2)) {
            hr = hr2;
            goto done;
        }
        // Handle some known error codes from ProcessOutput.
        if (hr == MF_E_TRANSFORM_TYPE_NOT_SET) {
            // The mixer's format is not set. Negotiate a new format.
            hr = renegotiateMediaType();
        } else if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
            // There was a dynamic media type change. Clear our media type.
            setMediaType(NULL);
        } else if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
            // The mixer needs more input.
            // We have to wait for the mixer to get more input.
            m_sampleNotify = false;
        }
    } else {
        // We got an output sample from the mixer.

        if (m_clock && !repaint) {
            // Latency: Record the ending time for the ProcessOutput operation,
            // and notify the EVR of the latency.

            m_clock->GetCorrelatedTime(0, &mixerEndTime, &systemTime);

            LONGLONG latencyTime = mixerEndTime - mixerStartTime;
            notifyEvent(EC_PROCESSING_LATENCY, (LONG_PTR)&latencyTime, 0);
        }

        // Set up notification for when the sample is released.
        hr = trackSample(sample);
        if (FAILED(hr))
            goto done;

        // Schedule the sample.
        if ((m_frameStep.state == FrameStepNone) || repaint) {
            hr = deliverSample(sample, repaint);
            if (FAILED(hr))
                goto done;
        } else {
            // We are frame-stepping (and this is not a repaint request).
            hr = deliverFrameStepSample(sample);
            if (FAILED(hr))
                goto done;
        }

        m_prerolled = true; // We have presented at least one sample now.
    }

done:
    qt_evr_safe_release(&sample);

    // Important: Release any events returned from the ProcessOutput method.
    qt_evr_safe_release(&dataBuffer.pEvents);
    return hr;
}

HRESULT EVRCustomPresenter::deliverSample(IMFSample *sample, bool repaint)
{
    // If we are not actively playing, OR we are scrubbing (rate = 0) OR this is a
    // repaint request, then we need to present the sample immediately. Otherwise,
    // schedule it normally.

    bool presentNow = ((m_renderState != RenderStarted) ||  isScrubbing() || repaint);

    HRESULT hr = m_scheduler.scheduleSample(sample, presentNow);

    if (FAILED(hr)) {
        // Notify the EVR that we have failed during streaming. The EVR will notify the
        // pipeline.

        notifyEvent(EC_ERRORABORT, hr, 0);
    }

    return hr;
}

HRESULT EVRCustomPresenter::deliverFrameStepSample(IMFSample *sample)
{
    HRESULT hr = S_OK;
    IUnknown *unk = NULL;

    // For rate 0, discard any sample that ends earlier than the clock time.
    if (isScrubbing() && m_clock && qt_evr_isSampleTimePassed(m_clock, sample)) {
        // Discard this sample.
    } else if (m_frameStep.state >= FrameStepScheduled) {
        // A frame was already submitted. Put this sample on the frame-step queue,
        // in case we are asked to step to the next frame. If frame-stepping is
        // cancelled, this sample will be processed normally.
        sample->AddRef();
        m_frameStep.samples.append(sample);
    } else {
        // We're ready to frame-step.

        // Decrement the number of steps.
        if (m_frameStep.steps > 0)
            m_frameStep.steps--;

        if (m_frameStep.steps > 0) {
            // This is not the last step. Discard this sample.
        } else if (m_frameStep.state == FrameStepWaitingStart) {
            // This is the right frame, but the clock hasn't started yet. Put the
            // sample on the frame-step queue. When the clock starts, the sample
            // will be processed.
            sample->AddRef();
            m_frameStep.samples.append(sample);
        } else {
            // This is the right frame *and* the clock has started. Deliver this sample.
            hr = deliverSample(sample, false);
            if (FAILED(hr))
                goto done;

            // Query for IUnknown so that we can identify the sample later.
            // Per COM rules, an object always returns the same pointer when QI'ed for IUnknown.
            hr = sample->QueryInterface(IID_PPV_ARGS(&unk));
            if (FAILED(hr))
                goto done;

            m_frameStep.sampleNoRef = (DWORD_PTR)unk; // No add-ref.

            // NOTE: We do not AddRef the IUnknown pointer, because that would prevent the
            // sample from invoking the OnSampleFree callback after the sample is presented.
            // We use this IUnknown pointer purely to identify the sample later; we never
            // attempt to dereference the pointer.

            m_frameStep.state = FrameStepScheduled;
        }
    }
done:
    qt_evr_safe_release(&unk);
    return hr;
}

HRESULT EVRCustomPresenter::trackSample(IMFSample *sample)
{
    IMFTrackedSample *tracked = NULL;

    HRESULT hr = sample->QueryInterface(IID_PPV_ARGS(&tracked));

    if (SUCCEEDED(hr))
        hr = tracked->SetAllocator(&m_sampleFreeCB, NULL);

    qt_evr_safe_release(&tracked);
    return hr;
}

void EVRCustomPresenter::releaseResources()
{
    // Increment the token counter to indicate that all existing video samples
    // are "stale." As these samples get released, we'll dispose of them.
    //
    // Note: The token counter is required because the samples are shared
    // between more than one thread, and they are returned to the presenter
    // through an asynchronous callback (onSampleFree). Without the token, we
    // might accidentally re-use a stale sample after the ReleaseResources
    // method returns.

    m_tokenCounter++;

    flush();

    m_samplePool.clear();

    m_presentEngine->releaseResources();
}

HRESULT EVRCustomPresenter::onSampleFree(IMFAsyncResult *result)
{
    IUnknown *object = NULL;
    IMFSample *sample = NULL;
    IUnknown *unk = NULL;
    UINT32 token;

    // Get the sample from the async result object.
    HRESULT hr = result->GetObject(&object);
    if (FAILED(hr))
        goto done;

    hr = object->QueryInterface(IID_PPV_ARGS(&sample));
    if (FAILED(hr))
        goto done;

    // If this sample was submitted for a frame-step, the frame step operation
    // is complete.

    if (m_frameStep.state == FrameStepScheduled) {
        // Query the sample for IUnknown and compare it to our cached value.
        hr = sample->QueryInterface(IID_PPV_ARGS(&unk));
        if (FAILED(hr))
            goto done;

        if (m_frameStep.sampleNoRef == (DWORD_PTR)unk) {
            // Notify the EVR.
            hr = completeFrameStep(sample);
            if (FAILED(hr))
                goto done;
        }

        // Note: Although object is also an IUnknown pointer, it is not
        // guaranteed to be the exact pointer value returned through
        // QueryInterface. Therefore, the second QueryInterface call is
        // required.
    }

    m_mutex.lock();

    token = MFGetAttributeUINT32(sample, MFSamplePresenter_SampleCounter, (UINT32)-1);

    if (token == m_tokenCounter) {
        // Return the sample to the sample pool.
        hr = m_samplePool.returnSample(sample);
        if (SUCCEEDED(hr)) {
            // A free sample is available. Process more data if possible.
            processOutputLoop();
        }
    }

    m_mutex.unlock();

done:
    if (FAILED(hr))
        notifyEvent(EC_ERRORABORT, hr, 0);
    qt_evr_safe_release(&object);
    qt_evr_safe_release(&sample);
    qt_evr_safe_release(&unk);
    return hr;
}

float EVRCustomPresenter::getMaxRate(bool thin)
{
    // Non-thinned:
    // If we have a valid frame rate and a monitor refresh rate, the maximum
    // playback rate is equal to the refresh rate. Otherwise, the maximum rate
    // is unbounded (FLT_MAX).

    // Thinned: The maximum rate is unbounded.

    float maxRate = FLT_MAX;
    MFRatio fps = { 0, 0 };
    UINT monitorRateHz = 0;

    if (!thin && m_mediaType) {
        qt_evr_getFrameRate(m_mediaType, &fps);
        monitorRateHz = m_presentEngine->refreshRate();

        if (fps.Denominator && fps.Numerator && monitorRateHz) {
            // Max Rate = Refresh Rate / Frame Rate
            maxRate = (float)MulDiv(monitorRateHz, fps.Denominator, fps.Numerator);
        }
    }

    return maxRate;
}

bool EVRCustomPresenter::event(QEvent *e)
{
    switch (int(e->type())) {
    case StartSurface:
        startSurface();
        return true;
    case StopSurface:
        stopSurface();
        return true;
    case PresentSample:
        presentSample(static_cast<PresentSampleEvent *>(e)->sample());
        return true;
    default:
        break;
    }
    return QObject::event(e);
}

void EVRCustomPresenter::startSurface()
{
    if (thread() != QThread::currentThread()) {
        QCoreApplication::postEvent(this, new QEvent(QEvent::Type(StartSurface)));
        return;
    }

    if (!m_surface || m_surface->isActive())
        return;

    QVideoSurfaceFormat format = m_presentEngine->videoSurfaceFormat();
    if (!format.isValid())
        return;

    m_surface->start(format);
}

void EVRCustomPresenter::stopSurface()
{
    if (thread() != QThread::currentThread()) {
        QCoreApplication::postEvent(this, new QEvent(QEvent::Type(StopSurface)));
        return;
    }

    if (!m_surface || !m_surface->isActive())
        return;

    m_surface->stop();
}

void EVRCustomPresenter::presentSample(IMFSample *sample)
{
    if (thread() != QThread::currentThread()) {
        QCoreApplication::postEvent(this, new PresentSampleEvent(sample));
        return;
    }

    if (!m_surface || !m_surface->isActive() || !m_presentEngine->videoSurfaceFormat().isValid())
        return;

    QVideoFrame frame = m_presentEngine->makeVideoFrame(sample);

    if (m_surface->isActive() && m_surface->surfaceFormat() != m_presentEngine->videoSurfaceFormat()) {
        m_surface->stop();
        if (!m_surface->start(m_presentEngine->videoSurfaceFormat()))
            return;
    }

    m_surface->present(frame);
}

HRESULT setDesiredSampleTime(IMFSample *sample, const LONGLONG &sampleTime, const LONGLONG &duration)
{
    if (!sample)
        return E_POINTER;

    HRESULT hr = S_OK;
    IMFDesiredSample *desired = NULL;

    hr = sample->QueryInterface(IID_PPV_ARGS(&desired));
    if (SUCCEEDED(hr))
        desired->SetDesiredSampleTimeAndDuration(sampleTime, duration);

    qt_evr_safe_release(&desired);
    return hr;
}

HRESULT clearDesiredSampleTime(IMFSample *sample)
{
    if (!sample)
        return E_POINTER;

    HRESULT hr = S_OK;

    IMFDesiredSample *desired = NULL;
    IUnknown *unkSwapChain = NULL;

    // We store some custom attributes on the sample, so we need to cache them
    // and reset them.
    //
    // This works around the fact that IMFDesiredSample::Clear() removes all of the
    // attributes from the sample.

    UINT32 counter = MFGetAttributeUINT32(sample, MFSamplePresenter_SampleCounter, (UINT32)-1);

    hr = sample->QueryInterface(IID_PPV_ARGS(&desired));
    if (SUCCEEDED(hr)) {
        desired->Clear();

        hr = sample->SetUINT32(MFSamplePresenter_SampleCounter, counter);
        if (FAILED(hr))
            goto done;
    }

done:
    qt_evr_safe_release(&unkSwapChain);
    qt_evr_safe_release(&desired);
    return hr;
}

HRESULT setMixerSourceRect(IMFTransform *mixer, const MFVideoNormalizedRect &sourceRect)
{
    if (!mixer)
        return E_POINTER;

    IMFAttributes *attributes = NULL;

    HRESULT hr = mixer->GetAttributes(&attributes);
    if (SUCCEEDED(hr)) {
        hr = attributes->SetBlob(video_ZOOM_RECT, (const UINT8*)&sourceRect, sizeof(sourceRect));
        attributes->Release();
    }
    return hr;
}

static QVideoFrame::PixelFormat pixelFormatFromMediaType(IMFMediaType *type)
{
    GUID majorType;
    if (FAILED(type->GetMajorType(&majorType)))
        return QVideoFrame::Format_Invalid;
    if (majorType != MFMediaType_Video)
        return QVideoFrame::Format_Invalid;

    GUID subtype;
    if (FAILED(type->GetGUID(MF_MT_SUBTYPE, &subtype)))
        return QVideoFrame::Format_Invalid;

    if (subtype == MFVideoFormat_RGB32)
        return QVideoFrame::Format_RGB32;
    else if (subtype == MFVideoFormat_ARGB32)
        return QVideoFrame::Format_ARGB32;
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
