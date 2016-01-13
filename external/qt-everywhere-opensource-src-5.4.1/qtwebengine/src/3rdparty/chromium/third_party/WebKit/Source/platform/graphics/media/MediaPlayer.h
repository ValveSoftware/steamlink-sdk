/*
 * Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaPlayer_h
#define MediaPlayer_h

#include "platform/PlatformExport.h"
#include "platform/graphics/GraphicsTypes3D.h"
#include "public/platform/WebMediaPlayer.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"

namespace blink {
class WebGraphicsContext3D;
class WebContentDecryptionModule;
class WebInbandTextTrack;
class WebLayer;
class WebMediaSource;
}

namespace WebCore {

class AudioSourceProvider;
class GraphicsContext;
class IntRect;
class KURL;
class MediaPlayer;
class TimeRanges;

// GL types as defined in OpenGL ES 2.0 header file gl2.h from khronos.org.
// That header cannot be included directly due to a conflict with NPAPI headers.
// See crbug.com/328085.
typedef unsigned GC3Denum;
typedef int GC3Dint;

class MediaPlayerClient {
public:
    virtual ~MediaPlayerClient() { }

    // the network state has changed
    virtual void mediaPlayerNetworkStateChanged() = 0;

    // the ready state has changed
    virtual void mediaPlayerReadyStateChanged() = 0;

    // time has jumped, eg. not as a result of normal playback
    virtual void mediaPlayerTimeChanged() = 0;

    // the media file duration has changed, or is now known
    virtual void mediaPlayerDurationChanged() = 0;

    // the play/pause status changed
    virtual void mediaPlayerPlaybackStateChanged() = 0;

    virtual void mediaPlayerRequestFullscreen() = 0;

    virtual void mediaPlayerRequestSeek(double) = 0;

    // The URL for video poster image.
    // FIXME: Remove this when WebMediaPlayerClientImpl::loadInternal does not depend on it.
    virtual KURL mediaPlayerPosterURL() = 0;

// Presentation-related methods
    // a new frame of video is available
    virtual void mediaPlayerRepaint() = 0;

    // the movie size has changed
    virtual void mediaPlayerSizeChanged() = 0;

    virtual void mediaPlayerSetWebLayer(blink::WebLayer*) = 0;

    virtual void mediaPlayerDidAddTextTrack(blink::WebInbandTextTrack*) = 0;
    virtual void mediaPlayerDidRemoveTextTrack(blink::WebInbandTextTrack*) = 0;

    virtual void mediaPlayerMediaSourceOpened(blink::WebMediaSource*) = 0;
};

typedef PassOwnPtr<MediaPlayer> (*CreateMediaEnginePlayer)(MediaPlayerClient*);

class PLATFORM_EXPORT MediaPlayer {
    WTF_MAKE_NONCOPYABLE(MediaPlayer);
public:
    static PassOwnPtr<MediaPlayer> create(MediaPlayerClient*);
    static void setMediaEngineCreateFunction(CreateMediaEnginePlayer);

    static double invalidTime() { return -1.0; }

    MediaPlayer() { }
    virtual ~MediaPlayer() { }

    virtual void load(blink::WebMediaPlayer::LoadType, const String& url, blink::WebMediaPlayer::CORSMode) = 0;

    virtual void play() = 0;
    virtual void pause() = 0;

    virtual bool supportsSave() const = 0;

    virtual double duration() const = 0;

    virtual double currentTime() const = 0;

    virtual void seek(double) = 0;

    virtual bool seeking() const = 0;

    virtual double rate() const = 0;
    virtual void setRate(double) = 0;

    virtual bool paused() const = 0;

    virtual void setPoster(const KURL&) = 0;

    enum NetworkState { Empty, Idle, Loading, Loaded, FormatError, NetworkError, DecodeError };
    virtual NetworkState networkState() const = 0;

    virtual double maxTimeSeekable() const = 0;
    virtual PassRefPtr<TimeRanges> buffered() const = 0;

    virtual bool didLoadingProgress() const = 0;

    virtual void paint(GraphicsContext*, const IntRect&) = 0;
    virtual bool copyVideoTextureToPlatformTexture(blink::WebGraphicsContext3D*, Platform3DObject, GC3Dint, GC3Denum, GC3Denum, bool, bool) = 0;

    enum Preload { None, MetaData, Auto };
    virtual void setPreload(Preload) = 0;

    virtual bool hasSingleSecurityOrigin() const = 0;

    // Time value in the movie's time scale. It is only necessary to override this if the media
    // engine uses rational numbers to represent media time.
    virtual double mediaTimeForTimeValue(double timeValue) const = 0;

#if ENABLE(WEB_AUDIO)
    virtual AudioSourceProvider* audioSourceProvider() = 0;
#endif
    virtual blink::WebMediaPlayer* webMediaPlayer() const = 0;
};

}

#endif // MediaPlayer_h
