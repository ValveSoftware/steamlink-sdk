// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents_media_capture_id.h"

#include <tuple>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"

namespace content {
const char kWebContentsCaptureScheme[] = "web-contents-media-stream://";
static char kEnableThrottlingFlag[] = "?throttling=auto";

bool WebContentsMediaCaptureId::operator<(
    const WebContentsMediaCaptureId& other) const {
  return std::tie(render_process_id, main_render_frame_id,
                  enable_auto_throttling) <
         std::tie(other.render_process_id, other.main_render_frame_id,
                  other.enable_auto_throttling);
}

bool WebContentsMediaCaptureId::operator==(
    const WebContentsMediaCaptureId& other) const {
  return std::tie(render_process_id, main_render_frame_id,
                  enable_auto_throttling) ==
         std::tie(other.render_process_id, other.main_render_frame_id,
                  other.enable_auto_throttling);
}

bool WebContentsMediaCaptureId::is_null() const {
  return (render_process_id < 0) || (main_render_frame_id < 0);
}

std::string WebContentsMediaCaptureId::ToString() const {
  std::string s = kWebContentsCaptureScheme;
  s.append(base::Int64ToString(render_process_id));
  s.append(":");
  s.append(base::Int64ToString(main_render_frame_id));

  if (enable_auto_throttling)
    s.append(kEnableThrottlingFlag);

  return s;
}

// static
WebContentsMediaCaptureId WebContentsMediaCaptureId::Parse(
    const std::string& str) {
  int render_process_id;
  int main_render_frame_id;
  if (!ExtractTabCaptureTarget(str, &render_process_id, &main_render_frame_id))
    return WebContentsMediaCaptureId();

  return WebContentsMediaCaptureId(render_process_id, main_render_frame_id,
                                   IsAutoThrottlingOptionSet(str));
}

// static
bool WebContentsMediaCaptureId::IsWebContentsDeviceId(
    const std::string& device_id) {
  int ignored;
  return ExtractTabCaptureTarget(device_id, &ignored, &ignored);
}

// static
bool WebContentsMediaCaptureId::ExtractTabCaptureTarget(
    const std::string& device_id_param,
    int* render_process_id,
    int* main_render_frame_id) {
  const std::string device_scheme = kWebContentsCaptureScheme;
  if (!base::StartsWith(device_id_param, device_scheme,
                        base::CompareCase::SENSITIVE))
    return false;

  const std::string device_id = device_id_param.substr(device_scheme.size());

  const size_t sep_pos = device_id.find(':');
  if (sep_pos == std::string::npos)
    return false;

  const base::StringPiece component1(device_id.data(), sep_pos);
  size_t end_pos = device_id.find('?');
  if (end_pos == std::string::npos)
    end_pos = device_id.length();
  const base::StringPiece component2(device_id.data() + sep_pos + 1,
                                     end_pos - sep_pos - 1);

  return (base::StringToInt(component1, render_process_id) &&
          base::StringToInt(component2, main_render_frame_id));
}

// static
bool WebContentsMediaCaptureId::IsAutoThrottlingOptionSet(
    const std::string& device_id) {
  if (!IsWebContentsDeviceId(device_id))
    return false;

  // Find the option part of the string and just do a naive string compare since
  // there are no other options in the |device_id| to account for (at the time
  // of this writing).
  const size_t option_pos = device_id.find('?');
  if (option_pos == std::string::npos)
    return false;
  const base::StringPiece component(device_id.data() + option_pos,
                                    device_id.length() - option_pos);
  return component.compare(kEnableThrottlingFlag) == 0;
}

}  // namespace content
