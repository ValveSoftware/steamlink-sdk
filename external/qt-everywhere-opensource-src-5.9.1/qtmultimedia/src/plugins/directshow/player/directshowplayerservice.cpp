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

#include <dshow.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include "directshowplayerservice.h"

#include "directshowaudioendpointcontrol.h"
#include "directshowmetadatacontrol.h"
#include "vmr9videowindowcontrol.h"
#include "directshowiosource.h"
#include "directshowplayercontrol.h"
#include "directshowvideorenderercontrol.h"
#include "directshowutils.h"
#include "directshowglobal.h"
#include "directshowaudioprobecontrol.h"
#include "directshowvideoprobecontrol.h"
#include "directshowsamplegrabber.h"

#if QT_CONFIG(evr)
#include "directshowevrvideowindowcontrol.h"
#endif

#include "qmediacontent.h"

#include <QtMultimedia/private/qtmultimedia-config_p.h>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qdir.h>
#include <QtCore/qthread.h>
#include <QtCore/qvarlengtharray.h>
#include <QtCore/qsize.h>

#include <QtMultimedia/qaudiobuffer.h>
#include <QtMultimedia/qvideoframe.h>
#include <QtMultimedia/private/qmemoryvideobuffer_p.h>

#if QT_CONFIG(wmsdk)
#  include <wmsdk.h>
#endif

#ifndef Q_CC_MINGW
#  include <comdef.h>
#endif

QT_BEGIN_NAMESPACE

Q_GLOBAL_STATIC(DirectShowEventLoop, qt_directShowEventLoop)

static QString comError(HRESULT hr)
{
#ifndef Q_CC_MINGW // MinGW 5.3 no longer has swprintf_s().
    _com_error error(hr);
    return QString::fromWCharArray(error.ErrorMessage());
#else
    Q_UNUSED(hr)
    return QString();
#endif
}

// QMediaPlayer uses millisecond time units, direct show uses 100 nanosecond units.
static const int qt_directShowTimeScale = 10000;

class DirectShowPlayerServiceThread : public QThread
{
public:
    DirectShowPlayerServiceThread(DirectShowPlayerService *service)
        : m_service(service)
    {
    }

protected:
    void run() { m_service->run(); }

private:
    DirectShowPlayerService *m_service;
};

DirectShowPlayerService::DirectShowPlayerService(QObject *parent)
    : QMediaService(parent)
    , m_playerControl(0)
    , m_metaDataControl(0)
    , m_videoRendererControl(0)
    , m_videoWindowControl(0)
    , m_audioEndpointControl(0)
    , m_audioProbeControl(nullptr)
    , m_videoProbeControl(nullptr)
    , m_audioSampleGrabber(nullptr)
    , m_videoSampleGrabber(nullptr)
    , m_taskThread(0)
    , m_loop(qt_directShowEventLoop())
    , m_pendingTasks(0)
    , m_executingTask(0)
    , m_executedTasks(0)
    , m_taskHandle(::CreateEvent(0, 0, 0, 0))
    , m_eventHandle(0)
    , m_graphStatus(NoMedia)
    , m_stream(0)
    , m_graph(0)
    , m_source(0)
    , m_audioOutput(0)
    , m_videoOutput(0)
    , m_rate(1.0)
    , m_position(0)
    , m_seekPosition(-1)
    , m_duration(0)
    , m_buffering(false)
    , m_seekable(false)
    , m_atEnd(false)
    , m_dontCacheNextSeekResult(false)
{
    m_playerControl = new DirectShowPlayerControl(this);
    m_metaDataControl = new DirectShowMetaDataControl(this);
    m_audioEndpointControl = new DirectShowAudioEndpointControl(this);

    m_taskThread = new DirectShowPlayerServiceThread(this);
    m_taskThread->start();
}

DirectShowPlayerService::~DirectShowPlayerService()
{
    {
        QMutexLocker locker(&m_mutex);

        releaseGraph();

        m_pendingTasks = Shutdown;
        ::SetEvent(m_taskHandle);
    }

    m_taskThread->wait();
    delete m_taskThread;

    if (m_audioOutput) {
        m_audioOutput->Release();
        m_audioOutput = 0;
    }

    if (m_videoOutput) {
        m_videoOutput->Release();
        m_videoOutput = 0;
    }

    delete m_playerControl;
    delete m_audioEndpointControl;
    delete m_metaDataControl;
    delete m_videoRendererControl;
    delete m_videoWindowControl;
    delete m_audioProbeControl;
    delete m_videoProbeControl;

    ::CloseHandle(m_taskHandle);
}

QMediaControl *DirectShowPlayerService::requestControl(const char *name)
{
    if (qstrcmp(name, QMediaPlayerControl_iid) == 0) {
        return m_playerControl;
    } else if (qstrcmp(name, QAudioOutputSelectorControl_iid) == 0) {
        return m_audioEndpointControl;
    } else if (qstrcmp(name, QMetaDataReaderControl_iid) == 0) {
        return m_metaDataControl;
    } else if (qstrcmp(name, QVideoRendererControl_iid) == 0) {
        if (!m_videoRendererControl && !m_videoWindowControl) {
            m_videoRendererControl = new DirectShowVideoRendererControl(m_loop);

            connect(m_videoRendererControl, SIGNAL(filterChanged()),
                    this, SLOT(videoOutputChanged()));

            return m_videoRendererControl;
        }
    } else if (qstrcmp(name, QVideoWindowControl_iid) == 0) {
        if (!m_videoRendererControl && !m_videoWindowControl) {
            IBaseFilter *filter;

#if QT_CONFIG(evr)
            DirectShowEvrVideoWindowControl *evrControl = new DirectShowEvrVideoWindowControl;
            if ((filter = evrControl->filter()))
                m_videoWindowControl = evrControl;
            else
                delete evrControl;
#endif
            // Fall back to the VMR9 if the EVR is not available
            if (!m_videoWindowControl) {
                Vmr9VideoWindowControl *vmr9Control = new Vmr9VideoWindowControl;
                filter = vmr9Control->filter();
                m_videoWindowControl = vmr9Control;
            }

            setVideoOutput(filter);

            return m_videoWindowControl;
        }
    } else if (qstrcmp(name, QMediaAudioProbeControl_iid) == 0) {
        if (!m_audioProbeControl)
            m_audioProbeControl = new DirectShowAudioProbeControl();
        m_audioProbeControl->ref();
        updateAudioProbe();
        return m_audioProbeControl;
    } else if (qstrcmp(name, QMediaVideoProbeControl_iid) == 0) {
        if (!m_videoProbeControl)
            m_videoProbeControl = new DirectShowVideoProbeControl();
        m_videoProbeControl->ref();
        updateVideoProbe();
        return m_videoProbeControl;
    }
    return 0;
}

void DirectShowPlayerService::releaseControl(QMediaControl *control)
{
    if (!control) {
        qWarning("QMediaService::releaseControl():"
                " Attempted release of null control");
    } else if (control == m_videoRendererControl) {
        setVideoOutput(0);

        delete m_videoRendererControl;

        m_videoRendererControl = 0;
    } else if (control == m_videoWindowControl) {
        setVideoOutput(0);

        delete m_videoWindowControl;

        m_videoWindowControl = 0;
    } else if (control == m_audioProbeControl) {
        if (!m_audioProbeControl->deref()) {
            DirectShowAudioProbeControl *old = m_audioProbeControl;
            m_audioProbeControl = nullptr;
            updateAudioProbe();
            delete old;
        }
    } else if (control == m_videoProbeControl) {
        if (!m_videoProbeControl->deref()) {
            DirectShowVideoProbeControl *old = m_videoProbeControl;
            m_videoProbeControl = nullptr;
            updateVideoProbe();
            delete old;
        }
    }
}

void DirectShowPlayerService::load(const QMediaContent &media, QIODevice *stream)
{
    QMutexLocker locker(&m_mutex);

    m_pendingTasks = 0;

    if (m_graph)
        releaseGraph();

    m_resources = media.resources();
    m_stream = stream;
    m_error = QMediaPlayer::NoError;
    m_errorString = QString();
    m_position = 0;
    m_seekPosition = -1;
    m_duration = 0;
    m_streamTypes = 0;
    m_executedTasks = 0;
    m_buffering = false;
    m_seekable = false;
    m_atEnd = false;
    m_dontCacheNextSeekResult = false;
    m_metaDataControl->reset();

    if (m_resources.isEmpty() && !stream) {
        m_pendingTasks = 0;
        m_graphStatus = NoMedia;

        m_url.clear();
    } else if (stream && (!stream->isReadable() || stream->isSequential())) {
        m_pendingTasks = 0;
        m_graphStatus = InvalidMedia;
        m_error = QMediaPlayer::ResourceError;
    } else {
        // {36b73882-c2c8-11cf-8b46-00805f6cef60}
        static const GUID iid_IFilterGraph2 = {
            0x36b73882, 0xc2c8, 0x11cf, {0x8b, 0x46, 0x00, 0x80, 0x5f, 0x6c, 0xef, 0x60} };
        m_graphStatus = Loading;

        m_graph = com_new<IFilterGraph2>(CLSID_FilterGraph, iid_IFilterGraph2);

        if (stream)
            m_pendingTasks = SetStreamSource;
        else
            m_pendingTasks = SetUrlSource;

        ::SetEvent(m_taskHandle);
    }

    m_playerControl->updateError(m_error, m_errorString);
    m_playerControl->updateMediaInfo(m_duration, m_streamTypes, m_seekable);
    m_playerControl->updateState(QMediaPlayer::StoppedState);
    m_playerControl->updatePosition(m_position);
    updateStatus();
}

void DirectShowPlayerService::doSetUrlSource(QMutexLocker *locker)
{
    IBaseFilter *source = 0;

    QMediaResource resource = m_resources.takeFirst();
    m_url = resource.url();

    HRESULT hr = E_FAIL;
    if (m_url.scheme() == QLatin1String("http") || m_url.scheme() == QLatin1String("https")) {
        static const GUID clsid_WMAsfReader = {
            0x187463a0, 0x5bb7, 0x11d3, {0xac, 0xbe, 0x00, 0x80, 0xc7, 0x5e, 0x24, 0x6e} };

        // {56a868a6-0ad4-11ce-b03a-0020af0ba770}
        static const GUID iid_IFileSourceFilter = {
            0x56a868a6, 0x0ad4, 0x11ce, {0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70} };

        if (IFileSourceFilter *fileSource = com_new<IFileSourceFilter>(clsid_WMAsfReader, iid_IFileSourceFilter)) {
            locker->unlock();
            hr = fileSource->Load(reinterpret_cast<const OLECHAR *>(m_url.toString().utf16()), 0);

            if (SUCCEEDED(hr)) {
                source = com_cast<IBaseFilter>(fileSource, IID_IBaseFilter);

                if (!SUCCEEDED(hr = m_graph->AddFilter(source, L"Source")) && source) {
                    source->Release();
                    source = 0;
                }
            }
            fileSource->Release();
            locker->relock();
        }
    }

    if (!SUCCEEDED(hr)) {
        locker->unlock();
        const QString urlString = m_url.isLocalFile()
            ? QDir::toNativeSeparators(m_url.toLocalFile()) : m_url.toString();
        hr = m_graph->AddSourceFilter(
                reinterpret_cast<const OLECHAR *>(urlString.utf16()), L"Source", &source);
        locker->relock();
    }

    if (SUCCEEDED(hr)) {
        m_executedTasks = SetSource;
        m_pendingTasks |= Render;

        if (m_audioOutput)
            m_pendingTasks |= SetAudioOutput;
        if (m_videoOutput)
            m_pendingTasks |= SetVideoOutput;
        if (m_audioProbeControl)
            m_pendingTasks |= SetAudioProbe;
        if (m_videoProbeControl)
            m_pendingTasks |= SetVideoProbe;

        if (m_rate != 1.0)
            m_pendingTasks |= SetRate;

        m_source = source;
    } else if (!m_resources.isEmpty()) {
        m_pendingTasks |= SetUrlSource;
    } else {
        m_pendingTasks = 0;
        m_graphStatus = InvalidMedia;

        switch (hr) {
        case VFW_E_UNKNOWN_FILE_TYPE:
            m_error = QMediaPlayer::FormatError;
            m_errorString = QString();
            break;
        default:
            m_error = QMediaPlayer::ResourceError;
            m_errorString = QString();
            qWarning("DirectShowPlayerService::doSetUrlSource: Unresolved error code 0x%x (%s)",
                     uint(hr), qPrintable(comError(hr)));
            break;
        }

        QCoreApplication::postEvent(this, new QEvent(QEvent::Type(Error)));
    }
}

void DirectShowPlayerService::doSetStreamSource(QMutexLocker *locker)
{
    Q_UNUSED(locker)
    DirectShowIOSource *source = new DirectShowIOSource(m_loop);
    source->setDevice(m_stream);

    const HRESULT hr = m_graph->AddFilter(source, L"Source");
    if (SUCCEEDED(hr)) {
        m_executedTasks = SetSource;
        m_pendingTasks |= Render;

        if (m_audioOutput)
            m_pendingTasks |= SetAudioOutput;
        if (m_videoOutput)
            m_pendingTasks |= SetVideoOutput;

        if (m_rate != 1.0)
            m_pendingTasks |= SetRate;

        m_source = source;
    } else {
        source->Release();

        m_pendingTasks = 0;
        m_graphStatus = InvalidMedia;

        m_error = QMediaPlayer::ResourceError;
        m_errorString = QString();
        qWarning("DirectShowPlayerService::doPlay: Unresolved error code 0x%x (%s)",
                 uint(hr), qPrintable(comError(hr)));

        QCoreApplication::postEvent(this, new QEvent(QEvent::Type(Error)));
    }
}

void DirectShowPlayerService::doRender(QMutexLocker *locker)
{
    m_pendingTasks |= m_executedTasks & (Play | Pause);

    if (IMediaControl *control = com_cast<IMediaControl>(m_graph, IID_IMediaControl)) {
        control->Stop();
        control->Release();
    }

    if (m_pendingTasks & SetAudioOutput) {
        m_graph->AddFilter(m_audioOutput, L"AudioOutput");

        m_pendingTasks ^= SetAudioOutput;
        m_executedTasks |= SetAudioOutput;
    }
    if (m_pendingTasks & SetVideoOutput) {
        m_graph->AddFilter(m_videoOutput, L"VideoOutput");

        m_pendingTasks ^= SetVideoOutput;
        m_executedTasks |= SetVideoOutput;
    }

    if (m_pendingTasks & SetAudioProbe) {
        doSetAudioProbe(locker);
        m_pendingTasks ^= SetAudioProbe;
        m_executedTasks |= SetAudioProbe;
    }

    if (m_pendingTasks & SetVideoProbe) {
        doSetVideoProbe(locker);
        m_pendingTasks ^= SetVideoProbe;
        m_executedTasks |= SetVideoProbe;
    }

    IFilterGraph2 *graph = m_graph;
    graph->AddRef();

    QVarLengthArray<IBaseFilter *, 16> filters;
    m_source->AddRef();
    filters.append(m_source);

    bool rendered = false;

    HRESULT renderHr = S_OK;

    while (!filters.isEmpty()) {
        IEnumPins *pins = 0;
        IBaseFilter *filter = filters[filters.size() - 1];
        filters.removeLast();

        if (!(m_pendingTasks & ReleaseFilters) && SUCCEEDED(filter->EnumPins(&pins))) {
            int outputs = 0;
            for (IPin *pin = 0; pins->Next(1, &pin, 0) == S_OK; pin->Release()) {
                PIN_DIRECTION direction;
                if (pin->QueryDirection(&direction) == S_OK && direction == PINDIR_OUTPUT) {
                    ++outputs;

                    IPin *peer = 0;
                    if (pin->ConnectedTo(&peer) == S_OK) {
                        PIN_INFO peerInfo;
                        if (SUCCEEDED(peer->QueryPinInfo(&peerInfo)))
                            filters.append(peerInfo.pFilter);
                        peer->Release();
                    } else {
                        locker->unlock();
                        HRESULT hr;
                        if (SUCCEEDED(hr = graph->RenderEx(
                                pin, /*AM_RENDEREX_RENDERTOEXISTINGRENDERERS*/ 1, 0))) {
                            rendered = true;
                        } else if (renderHr == S_OK || renderHr == VFW_E_NO_DECOMPRESSOR){
                            renderHr = hr;
                        }
                        locker->relock();
                    }
                }
            }

            pins->Release();

            if (outputs == 0)
                rendered = true;
        }
        filter->Release();
    }

    if (m_audioOutput && !isConnected(m_audioOutput, PINDIR_INPUT)) {
        graph->RemoveFilter(m_audioOutput);

        m_executedTasks &= ~SetAudioOutput;
    }

    if (m_videoOutput && !isConnected(m_videoOutput, PINDIR_INPUT)) {
        graph->RemoveFilter(m_videoOutput);

        m_executedTasks &= ~SetVideoOutput;
    }

    graph->Release();

    if (!(m_pendingTasks & ReleaseFilters)) {
        if (rendered) {
            if (!(m_executedTasks & FinalizeLoad))
                m_pendingTasks |= FinalizeLoad;
        } else {
            m_pendingTasks = 0;

            m_graphStatus = InvalidMedia;

            if (!m_audioOutput && !m_videoOutput) {
                m_error = QMediaPlayer::ResourceError;
                m_errorString = QString();
            } else {
                switch (renderHr) {
                case VFW_E_UNSUPPORTED_AUDIO:
                case VFW_E_UNSUPPORTED_VIDEO:
                case VFW_E_UNSUPPORTED_STREAM:
                    m_error = QMediaPlayer::FormatError;
                    m_errorString = QString();
                    break;
                default:
                    m_error = QMediaPlayer::ResourceError;
                    m_errorString = QString();
                    qWarning("DirectShowPlayerService::doRender: Unresolved error code 0x%x (%s)",
                             uint(renderHr), qPrintable(comError(renderHr)));
                }
            }

            QCoreApplication::postEvent(this, new QEvent(QEvent::Type(Error)));
        }

        m_executedTasks |= Render;
    }
}

void DirectShowPlayerService::doFinalizeLoad(QMutexLocker *locker)
{
    Q_UNUSED(locker)
    if (m_graphStatus != Loaded) {
        if (IMediaEvent *event = com_cast<IMediaEvent>(m_graph, IID_IMediaEvent)) {
            event->GetEventHandle(reinterpret_cast<OAEVENT *>(&m_eventHandle));
            event->Release();
        }
        if (IMediaSeeking *seeking = com_cast<IMediaSeeking>(m_graph, IID_IMediaSeeking)) {
            LONGLONG duration = 0;
            seeking->GetDuration(&duration);
            m_duration = duration / qt_directShowTimeScale;

            DWORD capabilities = 0;
            seeking->GetCapabilities(&capabilities);
            m_seekable = capabilities & AM_SEEKING_CanSeekAbsolute;

            seeking->Release();
        }
    }

    if ((m_executedTasks & SetOutputs) == SetOutputs) {
        m_streamTypes = AudioStream | VideoStream;
    } else {
        m_streamTypes = findStreamTypes(m_source);
    }

    m_executedTasks |= FinalizeLoad;

    m_graphStatus = Loaded;

    QCoreApplication::postEvent(this, new QEvent(QEvent::Type(FinalizedLoad)));
}

void DirectShowPlayerService::releaseGraph()
{
    if (m_graph) {
        if (m_executingTask != 0) {
            // {8E1C39A1-DE53-11cf-AA63-0080C744528D}
            static const GUID iid_IAMOpenProgress = {
                0x8E1C39A1, 0xDE53, 0x11cf, {0xAA, 0x63, 0x00, 0x80, 0xC7, 0x44, 0x52, 0x8D} };

            if (IAMOpenProgress *progress = com_cast<IAMOpenProgress>(
                    m_graph, iid_IAMOpenProgress)) {
                progress->AbortOperation();
                progress->Release();
            }
            m_graph->Abort();
        }

        m_pendingTasks = ReleaseGraph;

        ::SetEvent(m_taskHandle);

        m_loop->wait(&m_mutex);
    }
}

void DirectShowPlayerService::doReleaseGraph(QMutexLocker *locker)
{
    Q_UNUSED(locker);

    if (IMediaControl *control = com_cast<IMediaControl>(m_graph, IID_IMediaControl)) {
        control->Stop();
        control->Release();
    }

    doReleaseAudioProbe(locker);
    doReleaseVideoProbe(locker);

    if (m_source) {
        m_source->Release();
        m_source = 0;
    }

    m_eventHandle = 0;

    m_graph->Release();
    m_graph = 0;

    m_loop->wake();
}

void DirectShowPlayerService::doSetVideoProbe(QMutexLocker *locker)
{
    Q_UNUSED(locker);

    if (!m_graph) {
        qCWarning(qtDirectShowPlugin, "Attempting to set a video probe without a valid graph!");
        return;
    }

    // Create the sample grabber, if necessary.
    if (!m_videoSampleGrabber) {
        m_videoSampleGrabber = new DirectShowSampleGrabber;
        connect(m_videoSampleGrabber, &DirectShowSampleGrabber::bufferAvailable, this, &DirectShowPlayerService::onVideoBufferAvailable);
    }

    if (FAILED(m_graph->AddFilter(m_videoSampleGrabber->filter(), L"Video Sample Grabber"))) {
        qCWarning(qtDirectShowPlugin, "Failed to add the video sample grabber into the graph!");
        return;
    }

    // TODO: Make util function for getting this, so it's easy to keep it in sync.
    static const GUID subtypes[] = { MEDIASUBTYPE_ARGB32,
                                     MEDIASUBTYPE_RGB32,
                                     MEDIASUBTYPE_RGB24,
                                     MEDIASUBTYPE_RGB565,
                                     MEDIASUBTYPE_RGB555,
                                     MEDIASUBTYPE_AYUV,
                                     MEDIASUBTYPE_I420,
                                     MEDIASUBTYPE_IYUV,
                                     MEDIASUBTYPE_YV12,
                                     MEDIASUBTYPE_UYVY,
                                     MEDIASUBTYPE_YUYV,
                                     MEDIASUBTYPE_YUY2,
                                     MEDIASUBTYPE_NV12,
                                     MEDIASUBTYPE_MJPG,
                                     MEDIASUBTYPE_IMC1,
                                     MEDIASUBTYPE_IMC2,
                                     MEDIASUBTYPE_IMC3,
                                     MEDIASUBTYPE_IMC4 };

    // Negotiate the subtype
    DirectShowMediaType mediaType(AM_MEDIA_TYPE { MEDIATYPE_Video });
    const int items = (sizeof subtypes / sizeof(GUID));
    bool connected = false;
    for (int i = 0; i != items; ++i) {
        mediaType->subtype = subtypes[i];
        m_videoSampleGrabber->setMediaType(&mediaType);
        if (SUCCEEDED(DirectShowUtils::connectFilters(m_graph, m_source, m_videoSampleGrabber->filter(), true)))
            connected = true;
            break;
    }

    if (!connected) {
        qCWarning(qtDirectShowPlugin, "Unable to connect the video probe!");
        return;
    }

    m_videoSampleGrabber->start(DirectShowSampleGrabber::CallbackMethod::BufferCB);
}

void DirectShowPlayerService::doSetAudioProbe(QMutexLocker *locker)
{
    Q_UNUSED(locker);

    if (!m_graph) {
        qCWarning(qtDirectShowPlugin, "Attempting to set an audio probe without a valid graph!");
        return;
    }

    // Create the sample grabber, if necessary.
    if (!m_audioSampleGrabber) {
        m_audioSampleGrabber = new DirectShowSampleGrabber;
        connect(m_audioSampleGrabber, &DirectShowSampleGrabber::bufferAvailable, this, &DirectShowPlayerService::onAudioBufferAvailable);
    }

    static const AM_MEDIA_TYPE mediaType { MEDIATYPE_Audio, MEDIASUBTYPE_PCM };
    m_audioSampleGrabber->setMediaType(&mediaType);

    if (FAILED(m_graph->AddFilter(m_audioSampleGrabber->filter(), L"Audio Sample Grabber"))) {
        qCWarning(qtDirectShowPlugin, "Failed to add the audio sample grabber into the graph!");
        return;
    }

    if (FAILED(DirectShowUtils::connectFilters(m_graph, m_source, m_audioSampleGrabber->filter(), true))) {
        qCWarning(qtDirectShowPlugin, "Failed to connect the audio sample grabber");
        return;
    }

    m_audioSampleGrabber->start(DirectShowSampleGrabber::CallbackMethod::BufferCB);
}

void DirectShowPlayerService::doReleaseVideoProbe(QMutexLocker *locker)
{
    Q_UNUSED(locker);

    if (!m_graph)
        return;

    if (!m_videoSampleGrabber)
        return;

    m_videoSampleGrabber->stop();
    HRESULT hr = m_graph->RemoveFilter(m_videoSampleGrabber->filter());
    if (FAILED(hr)) {
        qCWarning(qtDirectShowPlugin, "Failed to remove the video sample grabber!");
        return;
    }

    m_videoSampleGrabber->deleteLater();
    m_videoSampleGrabber = nullptr;
}

void DirectShowPlayerService::doReleaseAudioProbe(QMutexLocker *locker)
{
    Q_UNUSED(locker);

    if (!m_graph)
        return;

    if (!m_audioSampleGrabber)
        return;

    m_audioSampleGrabber->stop();
    HRESULT hr = m_graph->RemoveFilter(m_audioSampleGrabber->filter());
    if (FAILED(hr)) {
        qCWarning(qtDirectShowPlugin, "Failed to remove the audio sample grabber!");
        return;
    }

    m_audioSampleGrabber->deleteLater();
    m_audioSampleGrabber = nullptr;
}

int DirectShowPlayerService::findStreamTypes(IBaseFilter *source) const
{
    QVarLengthArray<IBaseFilter *, 16> filters;
    source->AddRef();
    filters.append(source);

    int streamTypes = 0;

    while (!filters.isEmpty()) {
        IEnumPins *pins = 0;
        IBaseFilter *filter = filters[filters.size() - 1];
        filters.removeLast();

        if (SUCCEEDED(filter->EnumPins(&pins))) {
            for (IPin *pin = 0; pins->Next(1, &pin, 0) == S_OK; pin->Release()) {
                PIN_DIRECTION direction;
                if (pin->QueryDirection(&direction) == S_OK && direction == PINDIR_OUTPUT) {
                    DirectShowMediaType connectionType;
                    if (SUCCEEDED(pin->ConnectionMediaType(&connectionType))) {
                        IPin *peer = 0;

                        if (connectionType->majortype == MEDIATYPE_Audio) {
                            streamTypes |= AudioStream;
                        } else if (connectionType->majortype == MEDIATYPE_Video) {
                            streamTypes |= VideoStream;
                        } else if (SUCCEEDED(pin->ConnectedTo(&peer))) {
                            PIN_INFO peerInfo;
                            if (SUCCEEDED(peer->QueryPinInfo(&peerInfo)))
                                filters.append(peerInfo.pFilter);
                            peer->Release();
                        }
                    } else {
                        streamTypes |= findStreamType(pin);
                    }
                }
            }
            pins->Release();
        }
        filter->Release();
    }
    return streamTypes;
}

int DirectShowPlayerService::findStreamType(IPin *pin) const
{
    IEnumMediaTypes *types;

    if (SUCCEEDED(pin->EnumMediaTypes(&types))) {
        bool video = false;
        bool audio = false;
        bool other = false;

        for (AM_MEDIA_TYPE *type = 0;
                types->Next(1, &type, 0) == S_OK;
                DirectShowMediaType::deleteType(type)) {
            if (type->majortype == MEDIATYPE_Audio)
                audio = true;
            else if (type->majortype == MEDIATYPE_Video)
                video = true;
            else
                other = true;
        }
        types->Release();

        if (other)
            return 0;
        else if (audio && !video)
            return AudioStream;
        else if (!audio && video)
            return VideoStream;
        else
            return 0;
    } else {
        return 0;
    }
}

void DirectShowPlayerService::play()
{
    QMutexLocker locker(&m_mutex);

    m_pendingTasks &= ~Pause;
    m_pendingTasks |= Play;

    if (m_executedTasks & Render) {
        if (m_executedTasks & Stop) {
            m_atEnd = false;
            if (m_seekPosition == -1) {
                m_dontCacheNextSeekResult = true;
                m_seekPosition = 0;
                m_position = 0;
                m_pendingTasks |= Seek;
            }
            m_executedTasks ^= Stop;
        }

        ::SetEvent(m_taskHandle);
    }

    updateStatus();
}

void DirectShowPlayerService::doPlay(QMutexLocker *locker)
{
    if (IMediaControl *control = com_cast<IMediaControl>(m_graph, IID_IMediaControl)) {
        locker->unlock();
        HRESULT hr = control->Run();
        locker->relock();

        control->Release();

        if (SUCCEEDED(hr)) {
            m_executedTasks |= Play;

            QCoreApplication::postEvent(this, new QEvent(QEvent::Type(StatusChange)));
        } else {
            m_error = QMediaPlayer::ResourceError;
            m_errorString = QString();
            qWarning("DirectShowPlayerService::doPlay: Unresolved error code 0x%x (%s)",
                     uint(hr), qPrintable(comError(hr)));

            QCoreApplication::postEvent(this, new QEvent(QEvent::Type(Error)));
        }
    }
}

void DirectShowPlayerService::pause()
{
    QMutexLocker locker(&m_mutex);

    m_pendingTasks &= ~Play;
    m_pendingTasks |= Pause;

    if (m_executedTasks & Render) {
        if (m_executedTasks & Stop) {
            m_atEnd = false;
            if (m_seekPosition == -1) {
                m_dontCacheNextSeekResult = true;
                m_seekPosition = 0;
                m_position = 0;
                m_pendingTasks |= Seek;
            }
            m_executedTasks ^= Stop;
        }

        ::SetEvent(m_taskHandle);
    }

    updateStatus();
}

void DirectShowPlayerService::doPause(QMutexLocker *locker)
{
    if (IMediaControl *control = com_cast<IMediaControl>(m_graph, IID_IMediaControl)) {
        locker->unlock();
        HRESULT hr = control->Pause();
        locker->relock();

        control->Release();

        if (SUCCEEDED(hr)) {
            if (IMediaSeeking *seeking = com_cast<IMediaSeeking>(m_graph, IID_IMediaSeeking)) {
                LONGLONG position = 0;

                seeking->GetCurrentPosition(&position);
                seeking->Release();

                m_position = position / qt_directShowTimeScale;
            } else {
                m_position = 0;
            }

            m_executedTasks |= Pause;

            QCoreApplication::postEvent(this, new QEvent(QEvent::Type(StatusChange)));
        } else {
            m_error = QMediaPlayer::ResourceError;
            m_errorString = QString();
            qWarning("DirectShowPlayerService::doPause: Unresolved error code 0x%x (%s)",
                     uint(hr), qPrintable(comError(hr)));

            QCoreApplication::postEvent(this, new QEvent(QEvent::Type(Error)));
        }
    }
}

void DirectShowPlayerService::stop()
{
    QMutexLocker locker(&m_mutex);

    m_pendingTasks &= ~(Play | Pause | Seek);

    if ((m_executingTask | m_executedTasks) & (Play | Pause | Seek)) {
        m_pendingTasks |= Stop;

        ::SetEvent(m_taskHandle);

        m_loop->wait(&m_mutex);
    }

    updateStatus();
}

void DirectShowPlayerService::doStop(QMutexLocker *locker)
{
    Q_UNUSED(locker)
    if (m_executedTasks & (Play | Pause)) {
        if (IMediaControl *control = com_cast<IMediaControl>(m_graph, IID_IMediaControl)) {
            control->Stop();
            control->Release();
        }

        m_seekPosition = 0;
        m_position = 0;
        m_dontCacheNextSeekResult = true;
        m_pendingTasks |= Seek;

        m_executedTasks &= ~(Play | Pause);

        QCoreApplication::postEvent(this, new QEvent(QEvent::Type(StatusChange)));
    }

    m_executedTasks |= Stop;

    m_loop->wake();
}

void DirectShowPlayerService::setRate(qreal rate)
{
    QMutexLocker locker(&m_mutex);

    m_rate = rate;

    m_pendingTasks |= SetRate;

    if (m_executedTasks & FinalizeLoad)
        ::SetEvent(m_taskHandle);
}

void DirectShowPlayerService::doSetRate(QMutexLocker *locker)
{
    if (IMediaSeeking *seeking = com_cast<IMediaSeeking>(m_graph, IID_IMediaSeeking)) {
        // Cache current values as we can't query IMediaSeeking during a seek due to the
        // possibility of a deadlock when flushing the VideoSurfaceFilter.
        LONGLONG currentPosition = 0;
        seeking->GetCurrentPosition(&currentPosition);
        m_position = currentPosition / qt_directShowTimeScale;

        LONGLONG minimum = 0;
        LONGLONG maximum = 0;
        m_playbackRange = SUCCEEDED(seeking->GetAvailable(&minimum, &maximum))
                ? QMediaTimeRange(minimum / qt_directShowTimeScale, maximum / qt_directShowTimeScale)
                : QMediaTimeRange();

        locker->unlock();
        HRESULT hr = seeking->SetRate(m_rate);
        locker->relock();

        if (!SUCCEEDED(hr)) {
            double rate = 0.0;
            m_rate = seeking->GetRate(&rate)
                    ? rate
                    : 1.0;
        }

        seeking->Release();
    } else if (m_rate != 1.0) {
        m_rate = 1.0;
    }
    QCoreApplication::postEvent(this, new QEvent(QEvent::Type(RateChange)));
}

qint64 DirectShowPlayerService::position() const
{
    QMutexLocker locker(const_cast<QMutex *>(&m_mutex));

    if (m_graphStatus == Loaded) {
        if (m_executingTask == Seek || m_executingTask == SetRate || (m_pendingTasks & Seek)) {
            return m_position;
        } else if (IMediaSeeking *seeking = com_cast<IMediaSeeking>(m_graph, IID_IMediaSeeking)) {
            LONGLONG position = 0;

            seeking->GetCurrentPosition(&position);
            seeking->Release();

            const_cast<qint64 &>(m_position) = position / qt_directShowTimeScale;

            return m_position;
        }
    }
    return 0;
}

QMediaTimeRange DirectShowPlayerService::availablePlaybackRanges() const
{
    QMutexLocker locker(const_cast<QMutex *>(&m_mutex));

    if (m_graphStatus == Loaded) {
        if (m_executingTask == Seek || m_executingTask == SetRate || (m_pendingTasks & Seek)) {
            return m_playbackRange;
        } else if (IMediaSeeking *seeking = com_cast<IMediaSeeking>(m_graph, IID_IMediaSeeking)) {
            LONGLONG minimum = 0;
            LONGLONG maximum = 0;

            HRESULT hr = seeking->GetAvailable(&minimum, &maximum);
            seeking->Release();

            if (SUCCEEDED(hr))
                return QMediaTimeRange(minimum, maximum);
        }
    }
    return QMediaTimeRange();
}

void DirectShowPlayerService::seek(qint64 position)
{
    QMutexLocker locker(&m_mutex);

    m_seekPosition = position;

    m_pendingTasks |= Seek;

    if (m_executedTasks & FinalizeLoad)
        ::SetEvent(m_taskHandle);
}

void DirectShowPlayerService::doSeek(QMutexLocker *locker)
{
    if (m_seekPosition == -1)
        return;

    if (IMediaSeeking *seeking = com_cast<IMediaSeeking>(m_graph, IID_IMediaSeeking)) {
        LONGLONG seekPosition = LONGLONG(m_seekPosition) * qt_directShowTimeScale;

        // Cache current values as we can't query IMediaSeeking during a seek due to the
        // possibility of a deadlock when flushing the VideoSurfaceFilter.
        LONGLONG currentPosition = 0;
        if (!m_dontCacheNextSeekResult) {
            seeking->GetCurrentPosition(&currentPosition);
            m_position = currentPosition / qt_directShowTimeScale;
        }

        LONGLONG minimum = 0;
        LONGLONG maximum = 0;
        m_playbackRange = SUCCEEDED(seeking->GetAvailable(&minimum, &maximum))
                ? QMediaTimeRange(
                        minimum / qt_directShowTimeScale, maximum / qt_directShowTimeScale)
                : QMediaTimeRange();

        locker->unlock();
        seeking->SetPositions(
                &seekPosition, AM_SEEKING_AbsolutePositioning, 0, AM_SEEKING_NoPositioning);
        locker->relock();

        if (!m_dontCacheNextSeekResult) {
            seeking->GetCurrentPosition(&currentPosition);
            m_position = currentPosition / qt_directShowTimeScale;
        }

        seeking->Release();

        QCoreApplication::postEvent(this, new QEvent(QEvent::Type(PositionChange)));
    }

    m_seekPosition = -1;
    m_dontCacheNextSeekResult = false;
}

int DirectShowPlayerService::bufferStatus() const
{
#if QT_CONFIG(wmsdk)
    QMutexLocker locker(const_cast<QMutex *>(&m_mutex));

    if (IWMReaderAdvanced2 *reader = com_cast<IWMReaderAdvanced2>(
            m_source, IID_IWMReaderAdvanced2)) {
        DWORD percentage = 0;

        reader->GetBufferProgress(&percentage, 0);
        reader->Release();

        return percentage;
    } else {
        return 0;
    }
#else
    return 0;
#endif
}

void DirectShowPlayerService::setAudioOutput(IBaseFilter *filter)
{
    QMutexLocker locker(&m_mutex);

    if (m_graph) {
        if (m_audioOutput) {
            if (m_executedTasks & SetAudioOutput) {
                m_pendingTasks |= ReleaseAudioOutput;

                ::SetEvent(m_taskHandle);

                m_loop->wait(&m_mutex);
            }
            m_audioOutput->Release();
        }

        m_audioOutput = filter;

        if (m_audioOutput) {
            m_audioOutput->AddRef();

            m_pendingTasks |= SetAudioOutput;

            if (m_executedTasks & SetSource) {
                m_pendingTasks |= Render;

                ::SetEvent(m_taskHandle);
            }
        } else {
            m_pendingTasks &= ~ SetAudioOutput;
        }
    } else {
        if (m_audioOutput)
            m_audioOutput->Release();

        m_audioOutput = filter;

        if (m_audioOutput)
            m_audioOutput->AddRef();
    }

    m_playerControl->updateAudioOutput(m_audioOutput);
}

void DirectShowPlayerService::doReleaseAudioOutput(QMutexLocker *locker)
{
    Q_UNUSED(locker)
    m_pendingTasks |= m_executedTasks & (Play | Pause);

    if (IMediaControl *control = com_cast<IMediaControl>(m_graph, IID_IMediaControl)) {
        control->Stop();
        control->Release();
    }

    IBaseFilter *decoder = getConnected(m_audioOutput, PINDIR_INPUT);
    if (!decoder) {
        decoder = m_audioOutput;
        decoder->AddRef();
    }

    // {DCFBDCF6-0DC2-45f5-9AB2-7C330EA09C29}
    static const GUID iid_IFilterChain = {
        0xDCFBDCF6, 0x0DC2, 0x45f5, {0x9A, 0xB2, 0x7C, 0x33, 0x0E, 0xA0, 0x9C, 0x29} };

    if (IFilterChain *chain = com_cast<IFilterChain>(m_graph, iid_IFilterChain)) {
        chain->RemoveChain(decoder, m_audioOutput);
        chain->Release();
    } else {
        m_graph->RemoveFilter(m_audioOutput);
    }

    decoder->Release();

    m_executedTasks &= ~SetAudioOutput;

    m_loop->wake();
}

void DirectShowPlayerService::setVideoOutput(IBaseFilter *filter)
{
    QMutexLocker locker(&m_mutex);

    if (m_graph) {
        if (m_videoOutput) {
            if (m_executedTasks & SetVideoOutput) {
                m_pendingTasks |= ReleaseVideoOutput;

                ::SetEvent(m_taskHandle);

                m_loop->wait(&m_mutex);
            }
            m_videoOutput->Release();
        }

        m_videoOutput = filter;

        if (m_videoOutput) {
            m_videoOutput->AddRef();

            m_pendingTasks |= SetVideoOutput;

            if (m_executedTasks & SetSource) {
                m_pendingTasks |= Render;

                ::SetEvent(m_taskHandle);
            }
        }
    } else {
        if (m_videoOutput)
            m_videoOutput->Release();

        m_videoOutput = filter;

        if (m_videoOutput)
            m_videoOutput->AddRef();
    }
}

void DirectShowPlayerService::updateAudioProbe()
{
    QMutexLocker locker(&m_mutex);

    // Set/Activate the audio probe.
    if (m_graph) {
        // If we don't have a audio probe, then stop and release the audio sample grabber
        if (!m_audioProbeControl && (m_executedTasks & SetAudioProbe)) {
            m_pendingTasks |= ReleaseAudioProbe;
            ::SetEvent(m_taskHandle);
            m_loop->wait(&m_mutex);
        } else if (m_audioProbeControl) {
            m_pendingTasks |= SetAudioProbe;
        }
    }
}

void DirectShowPlayerService::updateVideoProbe()
{
    QMutexLocker locker(&m_mutex);

    // Set/Activate the video probe.
    if (m_graph) {
        // If we don't have a video probe, then stop and release the video sample grabber
        if (!m_videoProbeControl && (m_executedTasks & SetVideoProbe)) {
            m_pendingTasks |= ReleaseVideoProbe;
            ::SetEvent(m_taskHandle);
            m_loop->wait(&m_mutex);
        } else if (m_videoProbeControl){
            m_pendingTasks |= SetVideoProbe;
        }
    }
}

void DirectShowPlayerService::doReleaseVideoOutput(QMutexLocker *locker)
{
    Q_UNUSED(locker)
    m_pendingTasks |= m_executedTasks & (Play | Pause);

    if (IMediaControl *control = com_cast<IMediaControl>(m_graph, IID_IMediaControl)) {
        control->Stop();
        control->Release();
    }

    IBaseFilter *intermediate = 0;
    if (!SUCCEEDED(m_graph->FindFilterByName(L"Color Space Converter", &intermediate))) {
        intermediate = m_videoOutput;
        intermediate->AddRef();
    }

    IBaseFilter *decoder = getConnected(intermediate, PINDIR_INPUT);
    if (!decoder) {
        decoder = intermediate;
        decoder->AddRef();
    }

    // {DCFBDCF6-0DC2-45f5-9AB2-7C330EA09C29}
    static const GUID iid_IFilterChain = {
        0xDCFBDCF6, 0x0DC2, 0x45f5, {0x9A, 0xB2, 0x7C, 0x33, 0x0E, 0xA0, 0x9C, 0x29} };

    if (IFilterChain *chain = com_cast<IFilterChain>(m_graph, iid_IFilterChain)) {
        chain->RemoveChain(decoder, m_videoOutput);
        chain->Release();
    } else {
        m_graph->RemoveFilter(m_videoOutput);
    }

    intermediate->Release();
    decoder->Release();

    m_executedTasks &= ~SetVideoOutput;

    m_loop->wake();
}

void DirectShowPlayerService::customEvent(QEvent *event)
{
    if (event->type() == QEvent::Type(FinalizedLoad)) {
        QMutexLocker locker(&m_mutex);

        m_playerControl->updateMediaInfo(m_duration, m_streamTypes, m_seekable);
        m_metaDataControl->updateMetadata(m_graph, m_source, m_url.toString());

        updateStatus();
    } else if (event->type() == QEvent::Type(Error)) {
        QMutexLocker locker(&m_mutex);

        if (m_error != QMediaPlayer::NoError) {
            m_playerControl->updateError(m_error, m_errorString);
            m_playerControl->updateMediaInfo(m_duration, m_streamTypes, m_seekable);
            m_playerControl->updateState(QMediaPlayer::StoppedState);
            updateStatus();
        }
    } else if (event->type() == QEvent::Type(RateChange)) {
        QMutexLocker locker(&m_mutex);

        m_playerControl->updatePlaybackRate(m_rate);
    } else if (event->type() == QEvent::Type(StatusChange)) {
        QMutexLocker locker(&m_mutex);

        updateStatus();
        m_playerControl->updatePosition(m_position);
    } else if (event->type() == QEvent::Type(DurationChange)) {
        QMutexLocker locker(&m_mutex);

        m_playerControl->updateMediaInfo(m_duration, m_streamTypes, m_seekable);
    } else if (event->type() == QEvent::Type(EndOfMedia)) {
        QMutexLocker locker(&m_mutex);

        if (m_atEnd) {
            m_playerControl->updateState(QMediaPlayer::StoppedState);
            m_playerControl->updateStatus(QMediaPlayer::EndOfMedia);
            m_playerControl->updatePosition(m_position);
        }
    } else if (event->type() == QEvent::Type(PositionChange)) {
        QMutexLocker locker(&m_mutex);

        if (m_playerControl->mediaStatus() == QMediaPlayer::EndOfMedia)
            m_playerControl->updateStatus(QMediaPlayer::LoadedMedia);
        m_playerControl->updatePosition(m_position);
    } else {
        QMediaService::customEvent(event);
    }
}

void DirectShowPlayerService::videoOutputChanged()
{
    setVideoOutput(m_videoRendererControl->filter());
}

void DirectShowPlayerService::onAudioBufferAvailable(double time, quint8 *buffer, long len)
{
    QMutexLocker locker(&m_mutex);
    if (!m_audioProbeControl || !m_audioSampleGrabber)
        return;

    DirectShowMediaType mt(AM_MEDIA_TYPE { GUID_NULL });
    const bool ok = m_audioSampleGrabber->getConnectedMediaType(&mt);
    if (!ok)
        return;

    if (mt->majortype != MEDIATYPE_Audio)
        return;

    if (mt->subtype != MEDIASUBTYPE_PCM)
        return;

    const bool isWfx = ((mt->formattype == FORMAT_WaveFormatEx) && (mt->cbFormat >= sizeof(WAVEFORMATEX)));
    WAVEFORMATEX *wfx = isWfx ? reinterpret_cast<WAVEFORMATEX *>(mt->pbFormat) : nullptr;

    if (!wfx)
        return;

    if (wfx->wFormatTag != WAVE_FORMAT_PCM && wfx->wFormatTag != WAVE_FORMAT_EXTENSIBLE)
        return;

    if ((wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) && (wfx->cbSize >= sizeof(WAVEFORMATEXTENSIBLE))) {
        WAVEFORMATEXTENSIBLE *wfxe = reinterpret_cast<WAVEFORMATEXTENSIBLE *>(wfx);
        if (wfxe->SubFormat != KSDATAFORMAT_SUBTYPE_PCM)
            return;
    }

    QAudioFormat format;
    format.setSampleRate(wfx->nSamplesPerSec);
    format.setChannelCount(wfx->nChannels);
    format.setSampleSize(wfx->wBitsPerSample);
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    if (format.sampleSize() == 8)
        format.setSampleType(QAudioFormat::UnSignedInt);
    else
        format.setSampleType(QAudioFormat::SignedInt);

    const quint64 startTime = quint64(time * 1000.);
    QAudioBuffer audioBuffer(QByteArray(reinterpret_cast<const char *>(buffer), len),
                             format,
                             startTime);

    Q_EMIT m_audioProbeControl->audioBufferProbed(audioBuffer);
}

void DirectShowPlayerService::onVideoBufferAvailable(double time, quint8 *buffer, long len)
{
    Q_UNUSED(time);
    Q_UNUSED(buffer);
    Q_UNUSED(len);

    QMutexLocker locker(&m_mutex);
    if (!m_videoProbeControl || !m_videoSampleGrabber)
        return;

    DirectShowMediaType mt(AM_MEDIA_TYPE { GUID_NULL });
    const bool ok = m_videoSampleGrabber->getConnectedMediaType(&mt);
    if (!ok)
        return;

    if (mt->majortype != MEDIATYPE_Video)
        return;

    QVideoFrame::PixelFormat format = DirectShowMediaType::pixelFormatFromType(&mt);
    if (format == QVideoFrame::Format_Invalid) {
        qCWarning(qtDirectShowPlugin, "Invalid format, stopping video probes!");
        m_videoSampleGrabber->stop();
        return;
    }

    const QVideoSurfaceFormat &videoFormat = DirectShowMediaType::videoFormatFromType(&mt);
    if (!videoFormat.isValid())
        return;

    const QSize &size = videoFormat.frameSize();

    const int bytesPerLine = DirectShowMediaType::bytesPerLine(videoFormat);
    QByteArray data(reinterpret_cast<const char *>(buffer), len);
    QVideoFrame frame(new QMemoryVideoBuffer(data, bytesPerLine),
                      size,
                      format);

    Q_EMIT m_videoProbeControl->videoFrameProbed(frame);
}

void DirectShowPlayerService::graphEvent(QMutexLocker *locker)
{
    Q_UNUSED(locker)
    if (IMediaEvent *event = com_cast<IMediaEvent>(m_graph, IID_IMediaEvent)) {
        long eventCode;
        LONG_PTR param1;
        LONG_PTR param2;

        while (event->GetEvent(&eventCode, &param1, &param2, 0) == S_OK) {
            switch (eventCode) {
            case EC_BUFFERING_DATA:
                m_buffering = param1;

                QCoreApplication::postEvent(this, new QEvent(QEvent::Type(StatusChange)));
                break;
            case EC_COMPLETE:
                m_executedTasks &= ~(Play | Pause);
                m_executedTasks |= Stop;

                m_buffering = false;
                m_atEnd = true;

                if (IMediaSeeking *seeking = com_cast<IMediaSeeking>(m_graph, IID_IMediaSeeking)) {
                    LONGLONG position = 0;

                    seeking->GetCurrentPosition(&position);
                    seeking->Release();

                    m_position = position / qt_directShowTimeScale;
                }

                QCoreApplication::postEvent(this, new QEvent(QEvent::Type(EndOfMedia)));
                break;
            case EC_LENGTH_CHANGED:
                if (IMediaSeeking *seeking = com_cast<IMediaSeeking>(m_graph, IID_IMediaSeeking)) {
                    LONGLONG duration = 0;
                    seeking->GetDuration(&duration);
                    m_duration = duration / qt_directShowTimeScale;

                    DWORD capabilities = 0;
                    seeking->GetCapabilities(&capabilities);
                    m_seekable = capabilities & AM_SEEKING_CanSeekAbsolute;

                    seeking->Release();

                    QCoreApplication::postEvent(this, new QEvent(QEvent::Type(DurationChange)));
                }
                break;
            default:
                break;
            }

            event->FreeEventParams(eventCode, param1, param2);
        }
        event->Release();
    }
}

void DirectShowPlayerService::updateStatus()
{
    switch (m_graphStatus) {
    case NoMedia:
        m_playerControl->updateStatus(QMediaPlayer::NoMedia);
        break;
    case Loading:
        m_playerControl->updateStatus(QMediaPlayer::LoadingMedia);
        break;
    case Loaded:
        if ((m_pendingTasks | m_executingTask | m_executedTasks) & (Play | Pause)) {
            if (m_buffering)
                m_playerControl->updateStatus(QMediaPlayer::BufferingMedia);
            else
                m_playerControl->updateStatus(QMediaPlayer::BufferedMedia);
        } else {
            m_playerControl->updateStatus(QMediaPlayer::LoadedMedia);
        }
        break;
    case InvalidMedia:
        m_playerControl->updateStatus(QMediaPlayer::InvalidMedia);
        break;
    default:
        m_playerControl->updateStatus(QMediaPlayer::UnknownMediaStatus);
    }
}

bool DirectShowPlayerService::isConnected(IBaseFilter *filter, PIN_DIRECTION direction) const
{
    bool connected = false;

    IEnumPins *pins = 0;

    if (SUCCEEDED(filter->EnumPins(&pins))) {
        for (IPin *pin = 0; pins->Next(1, &pin, 0) == S_OK; pin->Release()) {
            PIN_DIRECTION dir;
            if (SUCCEEDED(pin->QueryDirection(&dir)) && dir == direction) {
                IPin *peer = 0;
                if (SUCCEEDED(pin->ConnectedTo(&peer))) {
                    connected = true;

                    peer->Release();
                }
            }
        }
        pins->Release();
    }
    return connected;
}

IBaseFilter *DirectShowPlayerService::getConnected(
        IBaseFilter *filter, PIN_DIRECTION direction) const
{
    IBaseFilter *connected = 0;

    IEnumPins *pins = 0;

    if (SUCCEEDED(filter->EnumPins(&pins))) {
        for (IPin *pin = 0; pins->Next(1, &pin, 0) == S_OK; pin->Release()) {
            PIN_DIRECTION dir;
            if (SUCCEEDED(pin->QueryDirection(&dir)) && dir == direction) {
                IPin *peer = 0;
                if (SUCCEEDED(pin->ConnectedTo(&peer))) {
                    PIN_INFO info;

                    if (SUCCEEDED(peer->QueryPinInfo(&info))) {
                        if (connected) {
                            qWarning("DirectShowPlayerService::getConnected: "
                                "Multiple connected filters");
                            connected->Release();
                        }
                        connected = info.pFilter;
                    }
                    peer->Release();
                }
            }
        }
        pins->Release();
    }
    return connected;
}

void DirectShowPlayerService::run()
{
    QMutexLocker locker(&m_mutex);

    for (;;) {
        ::ResetEvent(m_taskHandle);

        while (m_pendingTasks == 0) {
            DWORD result = 0;

            locker.unlock();
            if (m_eventHandle) {
                HANDLE handles[] = { m_taskHandle, m_eventHandle };

                result = ::WaitForMultipleObjects(2, handles, false, INFINITE);
            } else {
                result = ::WaitForSingleObject(m_taskHandle, INFINITE);
            }
            locker.relock();

            if (result == WAIT_OBJECT_0 + 1) {
                graphEvent(&locker);
            }
        }

        if (m_pendingTasks & ReleaseGraph) {
            m_pendingTasks ^= ReleaseGraph;
            m_executingTask = ReleaseGraph;

            doReleaseGraph(&locker);
            //if the graph is released, we should not process other operations later
            if (m_pendingTasks & Shutdown) {
                m_pendingTasks = 0;
                return;
            }
            m_pendingTasks = 0;
        } else if (m_pendingTasks & Shutdown) {
            return;
        } else if (m_pendingTasks & ReleaseAudioOutput) {
            m_pendingTasks ^= ReleaseAudioOutput;
            m_executingTask = ReleaseAudioOutput;

            doReleaseAudioOutput(&locker);
        } else if (m_pendingTasks & ReleaseVideoOutput) {
            m_pendingTasks ^= ReleaseVideoOutput;
            m_executingTask = ReleaseVideoOutput;

            doReleaseVideoOutput(&locker);
        } else if (m_pendingTasks & ReleaseAudioProbe) {
            m_pendingTasks ^= ReleaseAudioProbe;
            m_executingTask = ReleaseAudioProbe;

            doReleaseAudioProbe(&locker);
        } else if (m_pendingTasks & ReleaseVideoProbe) {
            m_pendingTasks ^= ReleaseVideoProbe;
            m_executingTask = ReleaseVideoProbe;

            doReleaseVideoProbe(&locker);
        } else if (m_pendingTasks & SetUrlSource) {
            m_pendingTasks ^= SetUrlSource;
            m_executingTask = SetUrlSource;

            doSetUrlSource(&locker);
        } else if (m_pendingTasks & SetStreamSource) {
            m_pendingTasks ^= SetStreamSource;
            m_executingTask = SetStreamSource;

            doSetStreamSource(&locker);
        } else if (m_pendingTasks & SetAudioProbe) {
            m_pendingTasks ^= SetAudioProbe;
            m_executingTask = SetAudioProbe;

            doSetAudioProbe(&locker);
        } else if (m_pendingTasks & SetVideoProbe) {
            m_pendingTasks ^= SetVideoProbe;
            m_executingTask = SetVideoProbe;

            doSetVideoProbe(&locker);
        } else if (m_pendingTasks & Render) {
            m_pendingTasks ^= Render;
            m_executingTask = Render;

            doRender(&locker);
        } else if (!(m_executedTasks & Render)) {
            m_pendingTasks &= ~(FinalizeLoad | SetRate | Stop | Pause | Seek | Play);
        } else if (m_pendingTasks & FinalizeLoad) {
            m_pendingTasks ^= FinalizeLoad;
            m_executingTask = FinalizeLoad;

            doFinalizeLoad(&locker);
        } else if (m_pendingTasks & Stop) {
            m_pendingTasks ^= Stop;
            m_executingTask = Stop;

            doStop(&locker);
        } else if (m_pendingTasks & SetRate) {
            m_pendingTasks ^= SetRate;
            m_executingTask = SetRate;

            doSetRate(&locker);
        } else if (m_pendingTasks & Pause) {
            m_pendingTasks ^= Pause;
            m_executingTask = Pause;

            doPause(&locker);
        } else if (m_pendingTasks & Seek) {
            m_pendingTasks ^= Seek;
            m_executingTask = Seek;

            doSeek(&locker);
        } else if (m_pendingTasks & Play) {
            m_pendingTasks ^= Play;
            m_executingTask = Play;

            doPlay(&locker);
        }
        m_executingTask = 0;
    }
}

QT_END_NAMESPACE
