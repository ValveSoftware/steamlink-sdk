/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
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

#include "qwinrtmediaplayercontrol.h"
#include "qwinrtplayerrenderercontrol.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/qfunctions_winrt.h>
#include <QtCore/QSize>
#include <QtCore/QTimerEvent>
#include <QtCore/QUrl>
#include <QtMultimedia/QMediaPlaylist>
#include <QtConcurrent/QtConcurrentRun>

#include <dxgi.h>
#include <oleauto.h>
#include <mfapi.h>
#include <mfmediaengine.h>

#include <wrl.h>
using namespace Microsoft::WRL;

QT_BEGIN_NAMESPACE

#define QT_WINRT_MEDIAPLAYER_STREAM_ID "__qtmultimedia_winrt_player_stream"

class MediaEngineNotify;
class MediaEngineSources;
class MediaEngineByteStream;
class QWinRTMediaPlayerControlPrivate
{
public:
    QMediaPlayer::State state;
    QMediaPlayer::MediaStatus mediaStatus;
    qint64 duration;
    qint64 position;
    qreal playbackRate;
    int volume;
    bool muted;
    int bufferStatus;
    bool seekable;
    bool hasVideo;
    bool hasAudio;

    QMediaContent media;
    QScopedPointer<QIODevice, QWinRTMediaPlayerControlPrivate> stream;
    QWinRTPlayerRendererControl *videoRenderer;

    ComPtr<MediaEngineNotify> notifier;
    ComPtr<MediaEngineSources> sources;
    ComPtr<MediaEngineByteStream> streamProvider;
    ComPtr<IMFAttributes> configuration;
    ComPtr<IMFMediaEngineEx> engine;

    quint32 resetToken;
    ComPtr<IMFDXGIDeviceManager> manager;

    // Automatically delete streams created by the player
    static inline void cleanup(QIODevice *device)
    {
        if (device && device->property(QT_WINRT_MEDIAPLAYER_STREAM_ID).toBool())
            device->deleteLater();
    }

    // Allows for deferred cleanup of the engine, which tends to block on shutdown
    static void cleanup(QWinRTMediaPlayerControlPrivate *d);
    static void shutdown(QWinRTMediaPlayerControlPrivate *d);
};

class MediaEngineNotify : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IMFMediaEngineNotify>
{
public:
    MediaEngineNotify(QWinRTMediaPlayerControl *q_ptr, QWinRTMediaPlayerControlPrivate *d_ptr)
        : q(q_ptr), d(d_ptr)
    {
    }

    HRESULT __stdcall EventNotify(DWORD event, DWORD_PTR param1, DWORD param2)
    {
        QMediaPlayer::State newState = d->state;
        QMediaPlayer::MediaStatus newStatus = d->mediaStatus;

        switch (event) {
        // Media change events
        case MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA: {
            const bool hasAudio = d->engine->HasAudio();
            if (d->hasAudio != hasAudio) {
                d->hasAudio = hasAudio;
                emit q->audioAvailableChanged(d->hasAudio);
            }

            const bool hasVideo = d->engine->HasVideo();
            if (d->hasVideo != hasVideo) {
                d->hasVideo = hasVideo;
                emit q->audioAvailableChanged(d->hasAudio);
            }

            if (hasVideo) {
                HRESULT hr;
                DWORD width, height;
                hr = d->engine->GetNativeVideoSize(&width, &height);
                if (FAILED(hr))
                    break;
                d->videoRenderer->setSize(QSize(width, height));
            }

            newStatus = QMediaPlayer::LoadedMedia;
            break;
        }
        case MF_MEDIA_ENGINE_EVENT_LOADSTART:
        case MF_MEDIA_ENGINE_EVENT_PROGRESS: {
            newStatus = QMediaPlayer::LoadingMedia;
            break;
        }
        case MF_MEDIA_ENGINE_EVENT_CANPLAY:
            d->bufferStatus = 100; // Fired when buffering is not used
            newStatus = d->state == QMediaPlayer::StoppedState ? QMediaPlayer::LoadedMedia
                                                               : QMediaPlayer::BufferedMedia;
            break;
        case MF_MEDIA_ENGINE_EVENT_BUFFERINGSTARTED:
        case MF_MEDIA_ENGINE_EVENT_BUFFERINGENDED: {
            PROPVARIANT stat;
            HRESULT hr = d->engine->GetStatistics(MF_MEDIA_ENGINE_STATISTIC_BUFFER_PROGRESS, &stat);
            if (SUCCEEDED(hr)) {
                d->bufferStatus = stat.lVal;
                PropVariantClear(&stat);
            }
            newStatus = d->state == QMediaPlayer::StoppedState ? QMediaPlayer::LoadedMedia
                    : (d->bufferStatus == 100 ? QMediaPlayer::BufferedMedia : QMediaPlayer::BufferingMedia);
            break;
        }
        //case MF_MEDIA_ENGINE_EVENT_SUSPEND: ???
        //case MF_MEDIA_ENGINE_EVENT_ABORT: ???
        case MF_MEDIA_ENGINE_EVENT_EMPTIED: {
            newState = QMediaPlayer::StoppedState;
            break;
        }
        // Transport controls
        case MF_MEDIA_ENGINE_EVENT_PLAY: {
            // If the media is already loaded, the playing event may not occur after stop
            if (d->mediaStatus != QMediaPlayer::LoadedMedia)
                break;
            // fall through
        }
        case MF_MEDIA_ENGINE_EVENT_PLAYING: {
            newState = QMediaPlayer::PlayingState;
            newStatus = d->bufferStatus < 100 ? QMediaPlayer::BufferingMedia
                                              : QMediaPlayer::BufferedMedia;
            break;
        }
        case MF_MEDIA_ENGINE_EVENT_PAUSE: {
            newState = QMediaPlayer::PausedState;
            break;
        }
        case MF_MEDIA_ENGINE_EVENT_STALLED: {
            newStatus = QMediaPlayer::StalledMedia;
            break;
        }
        case MF_MEDIA_ENGINE_EVENT_WAITING: {
            newStatus = QMediaPlayer::StalledMedia;
            break;
        }
        case MF_MEDIA_ENGINE_EVENT_ENDED: {
            newState = QMediaPlayer::StoppedState;
            newStatus = QMediaPlayer::EndOfMedia;
            break;
        }
        // Media attributes
        case MF_MEDIA_ENGINE_EVENT_DURATIONCHANGE: {
            double duration = d->engine->GetDuration() * 1000;
            if (!qFuzzyCompare(d->duration, duration)) {
                d->duration = duration;
                emit q->durationChanged(d->duration);
            }
            break;
        }
        case MF_MEDIA_ENGINE_EVENT_TIMEUPDATE: {
            double position = d->engine->GetCurrentTime() * 1000;
            if (!qFuzzyCompare(d->position, position)) {
                d->position = position;
                emit q->positionChanged(d->position);
            }
            // Stopped state: paused at beginning
            if (qFuzzyIsNull(position) && d->state == QMediaPlayer::PausedState)
                newState = QMediaPlayer::StoppedState;
            break;
        }
        case MF_MEDIA_ENGINE_EVENT_RATECHANGE: {
            double playbackRate = d->engine->GetPlaybackRate();
            if (!qFuzzyCompare(d->playbackRate, playbackRate)) {
                d->playbackRate = playbackRate;
                emit q->playbackRateChanged(d->playbackRate);
            }
            break;
        }
        // Error handling
        case MF_MEDIA_ENGINE_EVENT_ERROR: {
            newState = QMediaPlayer::StoppedState;
            newStatus = QMediaPlayer::InvalidMedia;
            switch (param1) {
            default:
            case MF_MEDIA_ENGINE_ERR_NOERROR:
                newStatus = QMediaPlayer::UnknownMediaStatus;
                emit q->error(QMediaPlayer::ResourceError, qt_error_string(param2));
                break;
            case MF_MEDIA_ENGINE_ERR_ABORTED:
                if (d->mediaStatus == QMediaPlayer::StalledMedia || d->mediaStatus == QMediaPlayer::BufferingMedia)
                    d->mediaStatus = QMediaPlayer::LoadedMedia;
                emit q->error(QMediaPlayer::ResourceError, QStringLiteral("The process of fetching the media resource was stopped at the user's request."));
                break;
            case MF_MEDIA_ENGINE_ERR_NETWORK:
                if (d->mediaStatus == QMediaPlayer::StalledMedia || d->mediaStatus == QMediaPlayer::BufferingMedia)
                    d->mediaStatus = QMediaPlayer::LoadedMedia;
                emit q->error(QMediaPlayer::NetworkError, QStringLiteral("A network error occurred while fetching the media resource."));
                break;
            case MF_MEDIA_ENGINE_ERR_DECODE:
                emit q->error(QMediaPlayer::FormatError, QStringLiteral("An error occurred while decoding the media resource."));
                break;
            case MF_MEDIA_ENGINE_ERR_SRC_NOT_SUPPORTED:
                emit q->error(QMediaPlayer::FormatError, QStringLiteral("The media resource is not supported."));
                break;
            case MF_MEDIA_ENGINE_ERR_ENCRYPTED:
                emit q->error(QMediaPlayer::FormatError, QStringLiteral("An error occurred while encrypting the media resource."));
                break;
            }
            break;
        }
        default:
            break;
        }

        if (d->videoRenderer)
            d->videoRenderer->setActive(d->state == QMediaPlayer::PlayingState);

        const QMediaPlayer::MediaStatus oldMediaStatus = d->mediaStatus;
        const QMediaPlayer::State oldState = d->state;
        d->mediaStatus = newStatus;
        d->state = newState;

        if (d->mediaStatus != oldMediaStatus)
            emit q->mediaStatusChanged(d->mediaStatus);

        if (d->state != oldState)
            emit q->stateChanged(d->state);

        return S_OK;
    }

private:
    QWinRTMediaPlayerControl *q;
    QWinRTMediaPlayerControlPrivate *d;
};

class MediaEngineSources : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IMFMediaEngineSrcElements>
{
public:
    MediaEngineSources(QWinRTMediaPlayerControlPrivate *d_ptr)
        : d(d_ptr)
    {
    }

    DWORD __stdcall GetLength()
    {
        return d->media.resources().length();
    }

    HRESULT __stdcall GetURL(DWORD index, BSTR *url)
    {
        const QString resourceUrl = d->media.resources().value(index).url().toString();
        *url = SysAllocString((const OLECHAR *)resourceUrl.utf16());
        return S_OK;
    }

    HRESULT __stdcall GetType(DWORD index, BSTR *type)
    {
        const QString resourceType = d->media.resources().value(index).mimeType();
        *type = SysAllocString((const OLECHAR *)resourceType.utf16());
        return S_OK;
    }

    HRESULT __stdcall GetMedia(DWORD index, BSTR *media)
    {
        Q_UNUSED(index);
        *media = NULL;
        return S_OK;
    }

    HRESULT __stdcall AddElement(BSTR url, BSTR type, BSTR media)
    {
        Q_UNUSED(url);
        Q_UNUSED(type);
        Q_UNUSED(media);
        return E_NOTIMPL;
    }

    HRESULT __stdcall RemoveAllElements()
    {
        return E_NOTIMPL;
    }

private:
    QWinRTMediaPlayerControlPrivate *d;
};

class MediaEngineReadResult : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IUnknown>
{
public:
    MediaEngineReadResult(BYTE *bytes, ULONG maxLength)
        : bytes(bytes), maxLength(maxLength) { }

    void read(QIODevice *device)
    {
        bytesRead = device->read(reinterpret_cast<char *>(bytes), maxLength);
    }

    BYTE *bytes;
    ULONG maxLength;
    ULONG bytesRead;
};

class MediaEngineByteStream : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IMFByteStream>
{
public:
    MediaEngineByteStream(QWinRTMediaPlayerControl *q_ptr, QWinRTMediaPlayerControlPrivate *d_ptr)
        : q(q_ptr), d(d_ptr)
    {
    }

    HRESULT __stdcall GetCapabilities(DWORD *capabilities)
    {
        *capabilities |= MFBYTESTREAM_IS_READABLE;
        if (!d->stream->isSequential())
            *capabilities |= MFBYTESTREAM_IS_SEEKABLE;
        return S_OK;
    }

    HRESULT __stdcall GetLength(QWORD *length)
    {
        *length = QWORD(d->stream->size());
        return S_OK;
    }

    HRESULT __stdcall SetLength(QWORD length)
    {
        Q_UNUSED(length);
        return E_NOTIMPL;
    }

    HRESULT __stdcall GetCurrentPosition(QWORD *position)
    {
        *position = QWORD(d->stream->pos());
        return S_OK;
    }

    HRESULT __stdcall SetCurrentPosition(QWORD position)
    {
        qint64 pos(position);
        if (pos >= d->stream->size()) {
            // MSDN states we should return E_INVALIDARG, but that immediately
            // stops playback and does not play remaining buffers in the queue.
            // For some formats this can cause losing up to 5 seconds of the
            // end of the stream.
            return S_FALSE;
        }

        const bool ok = d->stream->seek(pos);
        return ok ? S_OK : S_FALSE;
    }

    HRESULT __stdcall IsEndOfStream(BOOL *endOfStream)
    {
        *endOfStream = d->stream->atEnd() ? TRUE : FALSE;
        return S_OK;
    }

    HRESULT __stdcall Read(BYTE *bytes, ULONG maxlen, ULONG *bytesRead)
    {
        *bytesRead = d->stream->read(reinterpret_cast<char *>(bytes), maxlen);
        return S_OK;
    }

    HRESULT __stdcall BeginRead(BYTE *bytes, ULONG maxLength, IMFAsyncCallback *callback, IUnknown *state)
    {
        ComPtr<MediaEngineReadResult> readResult = Make<MediaEngineReadResult>(bytes, maxLength);
        HRESULT hr;
        hr = MFCreateAsyncResult(readResult.Get(), callback, state, &asyncResult);
        RETURN_HR_IF_FAILED("Failed to create read callback result");
        QMetaObject::invokeMethod(q, "finishRead", Qt::QueuedConnection);
        return S_OK;
    }

    HRESULT __stdcall EndRead(IMFAsyncResult *result, ULONG *bytesRead)
    {
        HRESULT hr;
        ComPtr<MediaEngineReadResult> readResult;
        hr = result->GetObject(&readResult);
        RETURN_HR_IF_FAILED("Failed to get read result");

        *bytesRead = readResult->bytesRead;
        return S_OK;
    }

    HRESULT __stdcall Write(const BYTE *bytes, ULONG maxlen, ULONG *bytesWritten)
    {
        Q_UNUSED(bytes);
        Q_UNUSED(maxlen);
        Q_UNUSED(bytesWritten);
        return E_NOTIMPL;
    }

    HRESULT __stdcall BeginWrite(const BYTE *bytes, ULONG maxlen, IMFAsyncCallback *callback, IUnknown *state)
    {
        Q_UNUSED(bytes);
        Q_UNUSED(maxlen);
        Q_UNUSED(callback);
        Q_UNUSED(state);
        return E_NOTIMPL;
    }

    HRESULT __stdcall EndWrite(IMFAsyncResult *result, ULONG *bytesWritten)
    {
        Q_UNUSED(result);
        Q_UNUSED(bytesWritten);
        return E_NOTIMPL;
    }

    HRESULT __stdcall Seek(MFBYTESTREAM_SEEK_ORIGIN origin, LONGLONG offset, DWORD flags, QWORD *position)
    {
        Q_UNUSED(flags);

        qint64 pos = offset;
        if (origin == msoCurrent)
            pos += d->stream->pos();

        const bool ok = d->stream->seek(pos);
        *position = QWORD(d->stream->pos());

        return ok ? S_OK : E_FAIL;
    }

    HRESULT __stdcall Flush()
    {
        return E_NOTIMPL;
    }

    HRESULT __stdcall Close()
    {
        if (asyncResult)
            finishRead();

        if (d->stream->property(QT_WINRT_MEDIAPLAYER_STREAM_ID).toBool())
            d->stream->close();

        return S_OK;
    }

    void finishRead()
    {
        if (!asyncResult)
            return;

        HRESULT hr;
        ComPtr<MediaEngineReadResult> readResult;
        hr = asyncResult->GetObject(&readResult);
        RETURN_VOID_IF_FAILED("Failed to get read result object");
        readResult->read(d->stream.data());
        hr = MFInvokeCallback(asyncResult.Get());
        RETURN_VOID_IF_FAILED("Failed to invoke read callback");
        asyncResult.Reset();
    }

private:
    QWinRTMediaPlayerControl *q;
    QWinRTMediaPlayerControlPrivate *d;

    ComPtr<IMFAsyncResult> asyncResult;
};

void QWinRTMediaPlayerControlPrivate::cleanup(QWinRTMediaPlayerControlPrivate *d)
{
    QtConcurrent::run(&QWinRTMediaPlayerControlPrivate::shutdown, d);
}

void QWinRTMediaPlayerControlPrivate::shutdown(QWinRTMediaPlayerControlPrivate *d)
{
    d->engine->Shutdown();
    delete d;
}

QWinRTMediaPlayerControl::QWinRTMediaPlayerControl(IMFMediaEngineClassFactory *factory, QObject *parent)
    : QMediaPlayerControl(parent), d_ptr(new QWinRTMediaPlayerControlPrivate)
{
    Q_D(QWinRTMediaPlayerControl);

    d->state = QMediaPlayer::StoppedState;
    d->mediaStatus = QMediaPlayer::NoMedia;
    d->duration = 0;
    d->position = 0;
    d->playbackRate = 1.0;
    d->volume = 100;
    d->bufferStatus = 0;
    d->muted = false;
    d->seekable = false;
    d->hasAudio = false;
    d->hasVideo = false;
    d->videoRenderer = Q_NULLPTR;
    d->notifier = Make<MediaEngineNotify>(this, d);

    HRESULT hr;
    hr = MFCreateDXGIDeviceManager(&d->resetToken, &d->manager);
    RETURN_VOID_IF_FAILED("Failed to create DXGI device manager");

    hr = MFCreateAttributes(&d->configuration, 1);
    Q_ASSERT_SUCCEEDED(hr);
    hr = d->configuration->SetUnknown(MF_MEDIA_ENGINE_CALLBACK, d->notifier.Get());
    Q_ASSERT_SUCCEEDED(hr);
    hr = d->configuration->SetUnknown(MF_MEDIA_ENGINE_DXGI_MANAGER, d->manager.Get());
    Q_ASSERT_SUCCEEDED(hr);
    hr = d->configuration->SetUINT32(MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT, DXGI_FORMAT_B8G8R8A8_UNORM);
    Q_ASSERT_SUCCEEDED(hr);

    ComPtr<IMFMediaEngine> engine;
    hr = factory->CreateInstance(0, d->configuration.Get(), &engine);
    Q_ASSERT_SUCCEEDED(hr);
    hr = engine.As(&d->engine);
    Q_ASSERT_SUCCEEDED(hr);

    hr = d->engine->SetVolume(1.0);
    Q_ASSERT_SUCCEEDED(hr);

    d->sources = Make<MediaEngineSources>(d);
    hr = d->engine->SetSourceElements(d->sources.Get());
    Q_ASSERT_SUCCEEDED(hr);

    d->streamProvider = Make<MediaEngineByteStream>(this, d);
}

QWinRTMediaPlayerControl::~QWinRTMediaPlayerControl()
{
}

QMediaPlayer::State QWinRTMediaPlayerControl::state() const
{
    Q_D(const QWinRTMediaPlayerControl);
    return d->state;
}

QMediaPlayer::MediaStatus QWinRTMediaPlayerControl::mediaStatus() const
{
    Q_D(const QWinRTMediaPlayerControl);
    return d->mediaStatus;
}

qint64 QWinRTMediaPlayerControl::duration() const
{
    Q_D(const QWinRTMediaPlayerControl);
    return d->duration;
}

qint64 QWinRTMediaPlayerControl::position() const
{
    Q_D(const QWinRTMediaPlayerControl);
    return d->position;
}

void QWinRTMediaPlayerControl::setPosition(qint64 position)
{
    Q_D(QWinRTMediaPlayerControl);

    if (d->position == position)
        return;

    HRESULT hr;
    hr = d->engine->SetCurrentTime(double(position)/1000);
    RETURN_VOID_IF_FAILED("Failed to seek to new position");

    d->position = position;
    emit positionChanged(d->position);

    if (d->mediaStatus == QMediaPlayer::EndOfMedia) {
        d->mediaStatus = QMediaPlayer::LoadedMedia;
        emit mediaStatusChanged(d->mediaStatus);
    }
}

int QWinRTMediaPlayerControl::volume() const
{
    Q_D(const QWinRTMediaPlayerControl);
    return d->volume;
}

void QWinRTMediaPlayerControl::setVolume(int volume)
{
    Q_D(QWinRTMediaPlayerControl);

    if (d->volume == volume)
        return;

    HRESULT hr;
    hr = d->engine->SetVolume(double(volume)/100);
    RETURN_VOID_IF_FAILED("Failed to set volume");

    d->volume = volume;
    emit volumeChanged(d->volume);
}

bool QWinRTMediaPlayerControl::isMuted() const
{
    Q_D(const QWinRTMediaPlayerControl);
    return d->muted;
}

void QWinRTMediaPlayerControl::setMuted(bool muted)
{
    Q_D(QWinRTMediaPlayerControl);

    if (d->muted == muted)
        return;

    HRESULT hr;
    hr = d->engine->SetMuted(muted);
    RETURN_VOID_IF_FAILED("Failed to mute volume");

    d->muted = muted;
    emit mutedChanged(d->muted);
}

int QWinRTMediaPlayerControl::bufferStatus() const
{
    Q_D(const QWinRTMediaPlayerControl);
    return d->bufferStatus;
}

bool QWinRTMediaPlayerControl::isAudioAvailable() const
{
    Q_D(const QWinRTMediaPlayerControl);
    return d->hasAudio;
}

bool QWinRTMediaPlayerControl::isVideoAvailable() const
{
    Q_D(const QWinRTMediaPlayerControl);
    return d->hasVideo;
}

bool QWinRTMediaPlayerControl::isSeekable() const
{
    Q_D(const QWinRTMediaPlayerControl);
    return d->seekable;
}

QMediaTimeRange QWinRTMediaPlayerControl::availablePlaybackRanges() const
{
    Q_D(const QWinRTMediaPlayerControl);
    return QMediaTimeRange(0, d->duration);
}

qreal QWinRTMediaPlayerControl::playbackRate() const
{
    Q_D(const QWinRTMediaPlayerControl);
    return d->playbackRate;
}

void QWinRTMediaPlayerControl::setPlaybackRate(qreal rate)
{
    Q_D(QWinRTMediaPlayerControl);

    if (qFuzzyCompare(d->playbackRate, rate))
        return;

    HRESULT hr;
    hr = d->engine->SetPlaybackRate(rate);
    RETURN_VOID_IF_FAILED("Failed to set playback rate");
}

QMediaContent QWinRTMediaPlayerControl::media() const
{
    Q_D(const QWinRTMediaPlayerControl);
    return d->media;
}

const QIODevice *QWinRTMediaPlayerControl::mediaStream() const
{
    Q_D(const QWinRTMediaPlayerControl);
    return d->stream.data();
}

void QWinRTMediaPlayerControl::setMedia(const QMediaContent &media, QIODevice *stream)
{
    Q_D(QWinRTMediaPlayerControl);

    if (d->media == media)
        return;

    d->media = media;
    d->stream.reset(stream);
    if (d->hasAudio != false) {
        d->hasAudio = false;
        emit audioAvailableChanged(d->hasAudio);
    }
    if (d->hasVideo != false) {
        d->hasVideo = false;
        emit videoAvailableChanged(d->hasVideo);
    }
    if (d->seekable != false) {
        d->seekable = false;
        emit seekableChanged(d->seekable);
    }
    if (d->bufferStatus != 0) {
        d->bufferStatus = 0;
        emit bufferStatusChanged(d->bufferStatus);
    }
    if (d->position != 0) {
        d->position = 0;
        emit positionChanged(d->position);
    }
    if (d->duration != 0) {
        d->duration = 0;
        emit durationChanged(d->duration);
    }
    QMediaPlayer::MediaStatus mediaStatus = media.isNull() ? QMediaPlayer::NoMedia
                                                           : QMediaPlayer::LoadingMedia;
    if (d->mediaStatus != mediaStatus) {
        d->mediaStatus = mediaStatus;
        emit mediaStatusChanged(d->mediaStatus);
    }
    emit mediaChanged(media);

    QString urlString = media.canonicalUrl().toString();
    if (!d->stream) {
        // If we can read the file via Qt, use the byte stream approach
        const auto resources = media.resources();
        for (const QMediaResource &resource : resources) {
            const QUrl url = resource.url();
            if (url.isLocalFile()) {
                urlString = url.toLocalFile();
                QScopedPointer<QFile> file(new QFile(urlString));
                if (file->open(QFile::ReadOnly)) {
                    file->setProperty(QT_WINRT_MEDIAPLAYER_STREAM_ID, true);
                    d->stream.reset(file.take());
                    break;
                }
            }
        }
    }

    HRESULT hr;
    if (d->stream) {
        hr = d->engine->SetSourceFromByteStream(d->streamProvider.Get(),
                                                reinterpret_cast<BSTR>(urlString.data()));
        if (FAILED(hr))
            emit error(QMediaPlayer::ResourceError, qt_error_string(hr));
        return;
    }

    // Let Windows handle all other URLs
    hr = d->engine->SetSource(Q_NULLPTR); // Resets the byte stream
    Q_ASSERT_SUCCEEDED(hr);
    hr = d->engine->Load();
    if (FAILED(hr))
        emit error(QMediaPlayer::ResourceError, qt_error_string(hr));

    if (d->media.isNull() && d->stream.isNull())
        return;

    // Resume play/pause/stop
    switch (d->state) {
    case QMediaPlayer::StoppedState:
        stop();
        break;
    case QMediaPlayer::PlayingState:
        play();
        break;
    case QMediaPlayer::PausedState:
        pause();
        break;
    }
}

void QWinRTMediaPlayerControl::play()
{
    Q_D(QWinRTMediaPlayerControl);

    if (d->state != QMediaPlayer::PlayingState) {
        d->state = QMediaPlayer::PlayingState;
        emit stateChanged(d->state);
    }

    if (d->media.isNull() && d->stream.isNull())
        return;

    if (d->videoRenderer)
        d->videoRenderer->ensureReady();

    HRESULT hr = d->engine->Play();
    if (FAILED(hr)) {
        emit error(QMediaPlayer::ResourceError, qt_error_string(hr));
        return;
    }
}

void QWinRTMediaPlayerControl::pause()
{
    Q_D(QWinRTMediaPlayerControl);

    if (d->state != QMediaPlayer::PausedState) {
        d->state = QMediaPlayer::PausedState;
        emit stateChanged(d->state);
    }

    if (d->media.isNull() && d->stream.isNull())
        return;

    HRESULT hr;
    hr = d->engine->Pause();
    if (FAILED(hr)) {
        emit error(QMediaPlayer::ResourceError, qt_error_string(hr));
        return;
    }
}

void QWinRTMediaPlayerControl::stop()
{
    Q_D(QWinRTMediaPlayerControl);

    const QMediaPlayer::MediaStatus oldMediaStatus = d->mediaStatus;
    const QMediaPlayer::State oldState = d->state;

    d->state = QMediaPlayer::StoppedState;

    if (d->mediaStatus == QMediaPlayer::BufferedMedia
            || d->mediaStatus == QMediaPlayer::BufferingMedia) {
        d->mediaStatus = QMediaPlayer::LoadedMedia;
    }

    if (d->mediaStatus != oldMediaStatus)
        emit mediaStatusChanged(d->mediaStatus);

    if (d->state != oldState)
        emit stateChanged(d->state);

    if (d->media.isNull() && d->stream.isNull())
        return;

    setPosition(0);

    HRESULT hr;
    hr = d->engine->Pause();
    if (FAILED(hr)) {
        emit error(QMediaPlayer::ResourceError, qt_error_string(hr));
        return;
    }
}

QVideoRendererControl *QWinRTMediaPlayerControl::videoRendererControl()
{
    Q_D(QWinRTMediaPlayerControl);

    if (!d->videoRenderer) {
        d->videoRenderer = new QWinRTPlayerRendererControl(d->engine.Get(), d->manager.Get(),
                                                          d->resetToken, this);
    }

    return d->videoRenderer;
}

void QWinRTMediaPlayerControl::finishRead()
{
    Q_D(QWinRTMediaPlayerControl);
    d->streamProvider->finishRead();
}

QT_END_NAMESPACE
