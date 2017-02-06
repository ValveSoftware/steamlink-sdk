// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chromecast/crash/cast_crash_keys.h"

// TODO(kjoswiak): Potentially refactor chunk size info as well as non-cast
// specific keys out and make shared with chrome/common/crash_keys.cc.
namespace {

// A small crash key, guaranteed to never be split into multiple pieces.
const size_t kSmallSize = 63;

// A medium crash key, which will be chunked on certain platforms but not
// others. Guaranteed to never be more than four chunks.
const size_t kMediumSize = kSmallSize * 4;

// A large crash key, which will be chunked on all platforms. This should be
// used sparingly.
const size_t kLargeSize = kSmallSize * 16;

// The maximum lengths specified by breakpad include the trailing NULL, so
// the actual length of the string is one less.
static const size_t kChunkMaxLength = 63;

}  // namespace

namespace chromecast {
namespace crash_keys {

const char kLastApp[] = "last_app";
const char kCurrentApp[] = "current_app";
const char kPreviousApp[] = "previous_app";

size_t RegisterCastCrashKeys() {
  const base::debug::CrashKey fixed_keys[] = {
    { kLastApp, kSmallSize },
    { kCurrentApp, kSmallSize },
    { kPreviousApp, kSmallSize },
    // content/:
    { "discardable-memory-allocated", kSmallSize },
    { "discardable-memory-free", kSmallSize },
    { "ppapi_path", kMediumSize },
    { "subresource_url", kLargeSize },
    { "total-discardable-memory-allocated", kSmallSize },

    // gin/:
    { "v8-ignition", kSmallSize },

    // Copied from common/crash_keys. Remove when
    // http://crbug.com/598854 is resolved.

    // Keys for http://crbug.com/575245
    { "swapout_frame_id", kSmallSize },
    { "swapout_proxy_id", kSmallSize },
    { "swapout_view_id", kSmallSize },
    { "commit_frame_id", kSmallSize },
    { "commit_proxy_id", kSmallSize },
    { "commit_view_id", kSmallSize },
    { "commit_main_render_frame_id", kSmallSize },
    { "newproxy_proxy_id", kSmallSize },
    { "newproxy_view_id", kSmallSize },
    { "newproxy_opener_id", kSmallSize },
    { "newproxy_parent_id", kSmallSize },
    { "rvinit_view_id", kSmallSize },
    { "rvinit_proxy_id", kSmallSize },
    { "rvinit_main_frame_id", kSmallSize },
    { "initrf_frame_id", kSmallSize },
    { "initrf_proxy_id", kSmallSize },
    { "initrf_view_id", kSmallSize },
    { "initrf_main_frame_id", kSmallSize },
    { "initrf_view_is_live", kSmallSize },

    // Keys for https://crbug.com/591478
    { "initrf_parent_proxy_exists", kSmallSize },
    { "initrf_render_view_is_live", kSmallSize },
    { "initrf_parent_is_in_same_site_instance", kSmallSize},
    { "initrf_parent_process_is_live", kSmallSize},
    { "initrf_root_is_in_same_site_instance", kSmallSize},
    { "initrf_root_is_in_same_site_instance_as_parent", kSmallSize},
    { "initrf_root_process_is_live", kSmallSize},
    { "initrf_root_proxy_is_live", kSmallSize},
  };

  return base::debug::InitCrashKeys(fixed_keys, arraysize(fixed_keys),
                                    kChunkMaxLength);
}

}  // namespace crash_keys
}  // namespace chromecast
