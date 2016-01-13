// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_SOURCE_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_SOURCE_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"

namespace blink {
class WebMediaStreamTrack;
}  // namespace blink

namespace content {

class CONTENT_EXPORT MediaStreamSource
    : NON_EXPORTED_BASE(public blink::WebMediaStreamSource::ExtraData) {
 public:
  typedef base::Callback<void(const blink::WebMediaStreamSource& source)>
      SourceStoppedCallback;

  typedef base::Callback<void(MediaStreamSource* source,
                              bool success)> ConstraintsCallback;

  // Source constraints key for
  // http://dev.w3.org/2011/webrtc/editor/getusermedia.html.
  static const char kSourceId[];

  MediaStreamSource();
  virtual ~MediaStreamSource();

  // Returns device information about a source that has been created by a
  // JavaScript call to GetUserMedia, e.g., a camera or microphone.
  const StreamDeviceInfo& device_info() const {
    return device_info_;
  }

  // Stops the source (by calling DoStopSource()). This sets the
  // WebMediaStreamSource::readyState to ended, triggers the |stop_callback_|
  // if set. All pointers to this object are invalid after calling this.
  void StopSource();

  void ResetSourceStoppedCallback() {
    DCHECK(!stop_callback_.is_null());
    stop_callback_.Reset();
  }

 protected:
  // Called when StopSource is called. It allows derived classes to implement
  // its  own Stop method.
  virtual void DoStopSource() = 0;

  // Sets device information about a source that has been created by a
  // JavaScript call to GetUserMedia. F.E a camera or microphone.
  void SetDeviceInfo(const StreamDeviceInfo& device_info) {
    device_info_ = device_info;
  }

  // Sets a callback that will be triggered when StopSource is called.
  void SetStopCallback(const SourceStoppedCallback& stop_callback) {
    DCHECK(stop_callback_.is_null());
    stop_callback_ = stop_callback;
  }

 private:
  StreamDeviceInfo device_info_;
  SourceStoppedCallback stop_callback_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamSource);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_SOURCE_H_
