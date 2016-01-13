// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/web_contents_capture_util.h"

#include "base/basictypes.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"

namespace {

const char kVirtualDeviceScheme[] = "virtual-media-stream://";

}  // namespace

namespace content {

std::string WebContentsCaptureUtil::AppendWebContentsDeviceScheme(
    const std::string& device_id) {
  return kVirtualDeviceScheme + device_id;
}

std::string WebContentsCaptureUtil::StripWebContentsDeviceScheme(
    const std::string& device_id) {
  return (IsWebContentsDeviceId(device_id) ?
              device_id.substr(arraysize(kVirtualDeviceScheme) - 1) :
              device_id);
}

bool WebContentsCaptureUtil::IsWebContentsDeviceId(
    const std::string& device_id) {
  return StartsWithASCII(device_id, kVirtualDeviceScheme, true);
}

bool WebContentsCaptureUtil::ExtractTabCaptureTarget(
    const std::string& device_id_param,
    int* render_process_id,
    int* render_view_id) {
  if (!IsWebContentsDeviceId(device_id_param))
    return false;

  const std::string device_id = device_id_param.substr(
      arraysize(kVirtualDeviceScheme) - 1);

  const size_t sep_pos = device_id.find(':');
  if (sep_pos == std::string::npos)
    return false;

  const base::StringPiece component1(device_id.data(), sep_pos);
  const base::StringPiece component2(device_id.data() + sep_pos + 1,
                                     device_id.length() - sep_pos - 1);

  return (base::StringToInt(component1, render_process_id) &&
          base::StringToInt(component2, render_view_id));
}

}  // namespace content
