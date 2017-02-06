/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebMediaPlayer_h
#define WebMediaPlayer_h

#include "WebCallbacks.h"
#include "WebCanvas.h"
#include "WebContentDecryptionModule.h"
#include "WebMediaSource.h"
#include "WebSetSinkIdCallbacks.h"
#include "WebString.h"
#include "WebTimeRange.h"
#include "third_party/skia/include/core/SkXfermode.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace blink {

class WebAudioSourceProvider;
class WebContentDecryptionModule;
class WebMediaPlayerSource;
class WebSecurityOrigin;
class WebString;
class WebURL;
struct WebRect;
struct WebSize;

class WebMediaPlayer {
public:
    enum NetworkState {
        NetworkStateEmpty,
        NetworkStateIdle,
        NetworkStateLoading,
        NetworkStateLoaded,
        NetworkStateFormatError,
        NetworkStateNetworkError,
        NetworkStateDecodeError,
    };

    enum ReadyState {
        ReadyStateHaveNothing,
        ReadyStateHaveMetadata,
        ReadyStateHaveCurrentData,
        ReadyStateHaveFutureData,
        ReadyStateHaveEnoughData,
    };

    enum Preload {
        PreloadNone,
        PreloadMetaData,
        PreloadAuto,
    };

    enum class BufferingStrategy {
        Normal,
        Aggressive,
    };

    enum CORSMode {
        CORSModeUnspecified,
        CORSModeAnonymous,
        CORSModeUseCredentials,
    };

    // Reported to UMA. Do not change existing values.
    enum LoadType {
        LoadTypeURL = 0,
        LoadTypeMediaSource = 1,
        LoadTypeMediaStream = 2,
        LoadTypeMax = LoadTypeMediaStream,
    };

    typedef WebString TrackId;
    enum TrackType { TextTrack, AudioTrack, VideoTrack };

    virtual ~WebMediaPlayer() { }

    virtual void load(LoadType, const WebMediaPlayerSource&, CORSMode) = 0;

    // Playback controls.
    virtual void play() = 0;
    virtual void pause() = 0;
    virtual bool supportsSave() const = 0;
    virtual void seek(double seconds) = 0;
    virtual void setRate(double) = 0;
    virtual void setVolume(double) = 0;

    virtual void requestRemotePlayback() { }
    virtual void requestRemotePlaybackControl() { }
    virtual void setPreload(Preload) { }
    virtual void setBufferingStrategy(BufferingStrategy) {}
    virtual WebTimeRanges buffered() const = 0;
    virtual WebTimeRanges seekable() const = 0;

    // Attempts to switch the audio output device.
    // Implementations of setSinkId take ownership of the WebSetSinkCallbacks
    // object.
    // Note also that setSinkId implementations must make sure that all
    // methods of the WebSetSinkCallbacks object, including constructors and
    // destructors, run in the same thread where the object is created
    // (i.e., the blink thread).
    virtual void setSinkId(const WebString& sinkId, const WebSecurityOrigin&, WebSetSinkIdCallbacks*) = 0;

    // True if the loaded media has a playable video/audio track.
    virtual bool hasVideo() const = 0;
    virtual bool hasAudio() const = 0;

    // True if the media is being played on a remote device.
    virtual bool isRemote() const { return false; }

    // Dimension of the video.
    virtual WebSize naturalSize() const = 0;

    // Getters of playback state.
    virtual bool paused() const = 0;
    virtual bool seeking() const = 0;
    virtual double duration() const = 0;
    virtual double currentTime() const = 0;

    // Internal states of loading and network.
    virtual NetworkState getNetworkState() const = 0;
    virtual ReadyState getReadyState() const = 0;

    virtual WebString getErrorMessage() = 0;
    virtual bool didLoadingProgress() = 0;

    virtual bool hasSingleSecurityOrigin() const = 0;
    virtual bool didPassCORSAccessCheck() const = 0;

    virtual double mediaTimeForTimeValue(double timeValue) const = 0;

    virtual unsigned decodedFrameCount() const = 0;
    virtual unsigned droppedFrameCount() const = 0;
    virtual unsigned corruptedFrameCount() const { return 0; }
    virtual size_t audioDecodedByteCount() const = 0;
    virtual size_t videoDecodedByteCount() const = 0;

    virtual void paint(WebCanvas*, const WebRect&, unsigned char alpha, SkXfermode::Mode) = 0;

    // TODO(dshwang): remove non-|target| version. crbug.com/349871
    virtual bool copyVideoTextureToPlatformTexture(gpu::gles2::GLES2Interface*, unsigned texture, unsigned internalFormat, unsigned type, bool premultiplyAlpha, bool flipY) { return false; }

    // Do a GPU-GPU textures copy. If the copy is impossible or fails, it returns false.
    virtual bool copyVideoTextureToPlatformTexture(gpu::gles2::GLES2Interface*, unsigned target,
        unsigned texture, unsigned internalFormat, unsigned type, int level,
        bool premultiplyAlpha, bool flipY) { return false; }
    // Copy sub video frame texture to |texture|. If the copy is impossible or fails, it returns false.
    virtual bool copyVideoSubTextureToPlatformTexture(gpu::gles2::GLES2Interface*, unsigned target,
        unsigned texture, int level, int xoffset, int yoffset, bool premultiplyAlpha,
        bool flipY) { return false; }

    virtual WebAudioSourceProvider* getAudioSourceProvider() { return nullptr; }

    virtual void setContentDecryptionModule(WebContentDecryptionModule* cdm, WebContentDecryptionModuleResult result) { result.completeWithError(WebContentDecryptionModuleExceptionNotSupportedError, 0, "ERROR"); }

    // Sets the poster image URL.
    virtual void setPoster(const WebURL& poster) { }

    // Whether the WebMediaPlayer supports overlay fullscreen video mode. When
    // this is true, the video layer will be removed from the layer tree when
    // entering fullscreen, and the WebMediaPlayer is responsible for displaying
    // the video in enteredFullscreen().
    virtual bool supportsOverlayFullscreenVideo() { return false; }
    // Inform WebMediaPlayer when the element has entered/exited fullscreen.
    virtual void enteredFullscreen() { }
    virtual void exitedFullscreen() { }

    virtual void enabledAudioTracksChanged(const WebVector<TrackId>& enabledTrackIds) { }
    // |selectedTrackId| is null if no track is selected.
    virtual void selectedVideoTrackChanged(TrackId* selectedTrackId) { }
};

} // namespace blink

#endif
