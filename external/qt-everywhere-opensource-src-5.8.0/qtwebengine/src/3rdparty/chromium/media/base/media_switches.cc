// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "media/base/media_switches.h"

namespace switches {

// Allow users to specify a custom buffer size for debugging purpose.
const char kAudioBufferSize[] = "audio-buffer-size";

// Set number of threads to use for video decoding.
const char kVideoThreads[] = "video-threads";

// Suspend media pipeline on background tabs.
const char kEnableMediaSuspend[] = "enable-media-suspend";
const char kDisableMediaSuspend[] = "disable-media-suspend";

// Force to report VP9 as an unsupported MIME type.
const char kReportVp9AsAnUnsupportedMimeType[] =
    "report-vp9-as-an-unsupported-mime-type";

#if defined(OS_ANDROID)
// Sets the MediaSource player that uses UI thread for frame processing.
const char kDisableMediaThreadForMediaPlayback[] =
    "disable-media-thread-for-media-playback";

// Use WebMediaPlayerAndroid instead of WebMediaPlayerImpl. This is a temporary
// switch for holding back the new unified media pipeline.
const char kDisableUnifiedMediaPipeline[] = "disable-unified-media-pipeline";

// Sets the MediaSource player that uses the separate media thread
const char kEnableMediaThreadForMediaPlayback[] =
    "enable-media-thread-for-media-playback";
#endif

#if defined(OS_LINUX) || defined(OS_FREEBSD) || defined(OS_SOLARIS)
// The Alsa device to use when opening an audio input stream.
const char kAlsaInputDevice[] = "alsa-input-device";
// The Alsa device to use when opening an audio stream.
const char kAlsaOutputDevice[] = "alsa-output-device";
#endif

// Use GpuMemoryBuffers for Video Capture when this is an option for the device.
// Experimental, see http://crbug.com/503835 and http://crbug.com/440843.
const char kUseGpuMemoryBuffersForCapture[] =
    "use-gpu-memory-buffers-for-capture";

#if defined(OS_WIN)
// Use exclusive mode audio streaming for Windows Vista and higher.
// Leads to lower latencies for audio streams which uses the
// AudioParameters::AUDIO_PCM_LOW_LATENCY audio path.
// See http://msdn.microsoft.com/en-us/library/windows/desktop/dd370844.aspx
// for details.
const char kEnableExclusiveAudio[] = "enable-exclusive-audio";

// Force the use of MediaFoundation for video capture. This is only supported in
// Windows 7 and above. Used, like |kForceDirectShowVideoCapture|, to
// troubleshoot problems in Windows platforms.
const char kForceMediaFoundationVideoCapture[] = "force-mediafoundation";

// Use Windows WaveOut/In audio API even if Core Audio is supported.
const char kForceWaveAudio[] = "force-wave-audio";

// Instead of always using the hardware channel layout, check if a driver
// supports the source channel layout.  Avoids outputting empty channels and
// permits drivers to enable stereo to multichannel expansion.  Kept behind a
// flag since some drivers lie about supported layouts and hang when used.  See
// http://crbug.com/259165 for more details.
const char kTrySupportedChannelLayouts[] = "try-supported-channel-layouts";

// Number of buffers to use for WaveOut.
const char kWaveOutBuffers[] = "waveout-buffers";
#endif

#if defined(USE_CRAS)
// Use CRAS, the ChromeOS audio server.
const char kUseCras[] = "use-cras";
#endif

#if !defined(OS_ANDROID)
// Use a media session for each tabs in a way that two tabs can't play on top of
// each other. This is different from the Media Session API as it is enabling a
// default behaviour for the browser.
const char kEnableDefaultMediaSession[] = "enable-default-media-session";
#endif

// Use fake device for Media Stream to replace actual camera and microphone.
const char kUseFakeDeviceForMediaStream[] = "use-fake-device-for-media-stream";

// Use an .y4m file to play as the webcam. See the comments in
// media/capture/video/file_video_capture_device.h for more details.
const char kUseFileForFakeVideoCapture[] = "use-file-for-fake-video-capture";

// Play a .wav file as the microphone. Note that for WebRTC calls we'll treat
// the bits as if they came from the microphone, which means you should disable
// audio processing (lest your audio file will play back distorted). The input
// file is converted to suit Chrome's audio buses if necessary, so most sane
// .wav files should work. You can pass either <path> to play the file looping
// or <path>%noloop to stop after playing the file to completion.
const char kUseFileForFakeAudioCapture[] = "use-file-for-fake-audio-capture";

// Enables support for inband text tracks in media content.
const char kEnableInbandTextTracks[] = "enable-inband-text-tracks";

// When running tests on a system without the required hardware or libraries,
// this flag will cause the tests to fail. Otherwise, they silently succeed.
const char kRequireAudioHardwareForTesting[] =
    "require-audio-hardware-for-testing";

// Allows clients to override the threshold for when the media renderer will
// declare the underflow state for the video stream when audio is present.
// TODO(dalecurtis): Remove once experiments for http://crbug.com/470940 finish.
const char kVideoUnderflowThresholdMs[] = "video-underflow-threshold-ms";

// Disables the new rendering algorithm for webrtc, which is designed to improve
// the rendering smoothness.
const char kDisableRTCSmoothnessAlgorithm[] =
    "disable-rtc-smoothness-algorithm";

// Enables demuxing of vp9 in mp4. Note that this flag will not have any effect
// if MP4 demuxing is not enabled in the build.
const char kEnableVp9InMp4[] = "enable-vp9-in-mp4";

// Switches which method is used to compute the encoder utilization metric
// (e.g., --cast-encoder-util-heuristic=backlog).
//
// TODO(miu): This is temporary, for lab performance testing, until a
// good "works for all" solution is confirmed.
// https://code.google.com/p/chrome-os-partner/issues/detail?id=54806
const char kCastEncoderUtilHeuristic[] = "cast-encoder-util-heuristic";

}  // namespace switches

namespace media {

// Use new audio rendering mixer.
const base::Feature kNewAudioRenderingMixingStrategy{
    "NewAudioRenderingMixingStrategy", base::FEATURE_DISABLED_BY_DEFAULT};

// Use shared block-based buffering for media.
const base::Feature kUseNewMediaCache{"use-new-media-cache",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace media
