// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_latency.h"

#include <stdint.h>

#include <algorithm>

#include "base/logging.h"
#include "build/build_config.h"

namespace media {

namespace {
#if !defined(OS_WIN)
// Taken from "Bit Twiddling Hacks"
// http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
uint32_t RoundUpToPowerOfTwo(uint32_t v) {
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v++;
  return v;
}
#endif
}  // namespace

// static
int AudioLatency::GetHighLatencyBufferSize(int sample_rate,
                                           int preferred_buffer_size) {
  // Empirically, we consider 20ms of samples to be high latency.
  const double twenty_ms_size = 2.0 * sample_rate / 100;

#if defined(OS_WIN)
  preferred_buffer_size = std::max(preferred_buffer_size, 1);

  // Windows doesn't use power of two buffer sizes, so we should always round up
  // to the nearest multiple of the output buffer size.
  const int high_latency_buffer_size =
      std::ceil(twenty_ms_size / preferred_buffer_size) * preferred_buffer_size;
#else
  // On other platforms use the nearest higher power of two buffer size.  For a
  // given sample rate, this works out to:
  //
  //     <= 3200   : 64
  //     <= 6400   : 128
  //     <= 12800  : 256
  //     <= 25600  : 512
  //     <= 51200  : 1024
  //     <= 102400 : 2048
  //     <= 204800 : 4096
  //
  // On Linux, the minimum hardware buffer size is 512, so the lower calculated
  // values are unused.  OSX may have a value as low as 128.
  const int high_latency_buffer_size = RoundUpToPowerOfTwo(twenty_ms_size);
#endif  // defined(OS_WIN)

#if defined(OS_CHROMEOS)
  return high_latency_buffer_size;  // No preference.
#else
  return std::max(preferred_buffer_size, high_latency_buffer_size);
#endif  // defined(OS_CHROMEOS)
}

// static
int AudioLatency::GetRtcBufferSize(int sample_rate, int hardware_buffer_size) {
  // Use native hardware buffer size as default. On Windows, we strive to open
  // up using this native hardware buffer size to achieve best
  // possible performance and to ensure that no FIFO is needed on the browser
  // side to match the client request. That is why there is no #if case for
  // Windows below.
  int frames_per_buffer = hardware_buffer_size;

  // No |hardware_buffer_size| is specified, fall back to 10 ms buffer size.
  if (!frames_per_buffer) {
    frames_per_buffer = sample_rate / 100;
    DVLOG(1) << "Using 10 ms sink output buffer size: " << frames_per_buffer;
    return frames_per_buffer;
  }

#if defined(OS_LINUX) || defined(OS_MACOSX)
  // On Linux and MacOS, the low level IO implementations on the browser side
  // supports all buffer size the clients want. We use the native peer
  // connection buffer size (10ms) to achieve best possible performance.
  frames_per_buffer = sample_rate / 100;
#elif defined(OS_ANDROID)
  // TODO(olka/henrika): This settings are very old, need to be revisited.
  int frames_per_10ms = sample_rate / 100;
  if (frames_per_buffer < 2 * frames_per_10ms) {
    // Examples of low-latency frame sizes and the resulting |buffer_size|:
    //  Nexus 7     : 240 audio frames => 2*480 = 960
    //  Nexus 10    : 256              => 2*441 = 882
    //  Galaxy Nexus: 144              => 2*441 = 882
    frames_per_buffer = 2 * frames_per_10ms;
    DVLOG(1) << "Low-latency output detected on Android";
  }
#endif

  DVLOG(1) << "Using sink output buffer size: " << frames_per_buffer;
  return frames_per_buffer;
}

// static
int AudioLatency::GetInteractiveBufferSize(int hardware_buffer_size) {
#if defined(OS_ANDROID)
  // The optimum low-latency hardware buffer size is usually too small on
  // Android for WebAudio to render without glitching. So, if it is small, use
  // a larger size.
  //
  // Since WebAudio renders in 128-frame blocks, the small buffer sizes (144 for
  // a Galaxy Nexus), cause significant processing jitter. Sometimes multiple
  // blocks will processed, but other times will not be since the WebAudio can't
  // satisfy the request. By using a larger render buffer size, we smooth out
  // the jitter.
  const int kSmallBufferSize = 1024;
  const int kDefaultCallbackBufferSize = 2048;
  if (hardware_buffer_size <= kSmallBufferSize)
    return kDefaultCallbackBufferSize;
#endif

  return hardware_buffer_size;
}

}  // namespace media
