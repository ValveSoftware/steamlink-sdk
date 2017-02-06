// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plugins/renderer/mobile_youtube_plugin.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "content/public/common/content_constants.h"
#include "content/public/renderer/render_frame.h"
#include "gin/object_template_builder.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "ui/base/webui/jstemplate_builder.h"

using blink::WebFrame;
using blink::WebPlugin;
using blink::WebURLRequest;

const char* const kSlashVSlash = "/v/";
const char* const kSlashESlash = "/e/";

namespace {

std::string GetYoutubeVideoId(const blink::WebPluginParams& params) {
  GURL url(params.url);
  std::string video_id = url.path().substr(strlen(kSlashVSlash));

  // Extract just the video id
  size_t video_id_end = video_id.find('&');
  if (video_id_end != std::string::npos)
    video_id = video_id.substr(0, video_id_end);
  return video_id;
}

std::string HtmlData(const blink::WebPluginParams& params,
                     base::StringPiece template_html) {
  base::DictionaryValue values;
  values.SetString("video_id", GetYoutubeVideoId(params));
  return webui::GetI18nTemplateHtml(template_html, &values);
}

bool IsValidYouTubeVideo(const std::string& path) {
  unsigned len = strlen(kSlashVSlash);

  // check for more than just /v/ or /e/.
  if (path.length() <= len)
    return false;

  // Youtube flash url can start with /v/ or /e/.
  if (!base::StartsWith(path, kSlashVSlash,
                        base::CompareCase::INSENSITIVE_ASCII) &&
      !base::StartsWith(path, kSlashESlash,
                        base::CompareCase::INSENSITIVE_ASCII))
    return false;

  // Start after /v/
  for (unsigned i = len; i < path.length(); i++) {
    char c = path[i];
    if (isalpha(c) || isdigit(c) || c == '_' || c == '-')
      continue;
    // The url can have more parameters such as &hl=en after the video id.
    // Once we start seeing extra parameters we can return true.
    return c == '&' && i > len;
  }
  return true;
}

}  // namespace

namespace plugins {

gin::WrapperInfo MobileYouTubePlugin::kWrapperInfo = {gin::kEmbedderNativeGin};

MobileYouTubePlugin::MobileYouTubePlugin(content::RenderFrame* render_frame,
                                         blink::WebLocalFrame* frame,
                                         const blink::WebPluginParams& params,
                                         base::StringPiece& template_html)
    : PluginPlaceholderBase(render_frame,
                            frame,
                            params,
                            HtmlData(params, template_html)) {
}

MobileYouTubePlugin::~MobileYouTubePlugin() {}

// static
bool MobileYouTubePlugin::IsYouTubeURL(const GURL& url,
                                       const std::string& mime_type) {
  std::string host = url.host();
  bool is_youtube =
      base::EndsWith(host, "youtube.com", base::CompareCase::SENSITIVE) ||
      base::EndsWith(host, "youtube-nocookie.com",
                     base::CompareCase::SENSITIVE);

  return is_youtube && IsValidYouTubeVideo(url.path()) &&
         base::LowerCaseEqualsASCII(mime_type,
                                    content::kFlashPluginSwfMimeType);
}

void MobileYouTubePlugin::OpenYoutubeUrlCallback() {
  std::string youtube("vnd.youtube:");
  GURL url(youtube.append(GetYoutubeVideoId(GetPluginParams())));
  WebURLRequest request;
  request.initialize();
  request.setURL(url);
  render_frame()->LoadURLExternally(request,
                                    blink::WebNavigationPolicyNewForegroundTab);
}

v8::Local<v8::Value> MobileYouTubePlugin::GetV8Handle(v8::Isolate* isolate) {
  return gin::CreateHandle(isolate, this).ToV8();
}

gin::ObjectTemplateBuilder MobileYouTubePlugin::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<MobileYouTubePlugin>::GetObjectTemplateBuilder(isolate)
      .SetMethod("openYoutubeURL",
                 &MobileYouTubePlugin::OpenYoutubeUrlCallback);
}

}  // namespace plugins
