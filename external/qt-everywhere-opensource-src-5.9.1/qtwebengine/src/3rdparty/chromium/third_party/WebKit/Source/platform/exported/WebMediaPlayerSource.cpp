// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebMediaPlayerSource.h"

namespace blink {

WebMediaPlayerSource::WebMediaPlayerSource() {}

WebMediaPlayerSource::WebMediaPlayerSource(const WebURL& url) : m_url(url) {}

WebMediaPlayerSource::WebMediaPlayerSource(const WebMediaStream& mediaStream)
    : m_mediaStream(mediaStream) {}

WebMediaPlayerSource::~WebMediaPlayerSource() {
  m_mediaStream.reset();
}

bool WebMediaPlayerSource::isURL() const {
  return !m_url.isEmpty();
}

WebURL WebMediaPlayerSource::getAsURL() const {
  return m_url;
}

bool WebMediaPlayerSource::isMediaStream() const {
  return !m_mediaStream.isNull();
}

WebMediaStream WebMediaPlayerSource::getAsMediaStream() const {
  return m_mediaStream;
}

}  // namespace blink
