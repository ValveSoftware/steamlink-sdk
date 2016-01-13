// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_VIDEO_SOURCE_HANDLER_H_
#define CONTENT_RENDERER_MEDIA_VIDEO_SOURCE_HANDLER_H_

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "media/base/video_frame.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"

namespace content {

class MediaStreamRegistryInterface;
class MediaStreamVideoSink;
class PpFrameReceiver;

// Interface used by the effects pepper plugin to get captured frame
// from the video track.
class CONTENT_EXPORT FrameReaderInterface {
 public:
  // Got a new captured frame.
  virtual bool GotFrame(const scoped_refptr<media::VideoFrame>& frame) = 0;

 protected:
  virtual ~FrameReaderInterface() {}
};

// VideoSourceHandler is a glue class between MediaStreamVideoTrack and
// the effects pepper plugin host.
class CONTENT_EXPORT VideoSourceHandler {
 public:
  // |registry| is used to look up the media stream by url. If a NULL |registry|
  // is given, the global blink::WebMediaStreamRegistry will be used.
  explicit VideoSourceHandler(MediaStreamRegistryInterface* registry);
  virtual ~VideoSourceHandler();
  // Connects to the first video track in the MediaStream specified by |url| and
  // the received frames will be delivered via |reader|.
  // Returns true on success and false on failure.
  bool Open(const std::string& url, FrameReaderInterface* reader);
  // Closes |reader|'s connection with the video track, i.e. stops receiving
  // frames from the video track.
  // Returns true on success and false on failure.
  bool Close(FrameReaderInterface* reader);

 private:
  FRIEND_TEST_ALL_PREFIXES(VideoSourceHandlerTest, OpenClose);

  struct SourceInfo {
    SourceInfo(const blink::WebMediaStreamTrack& blink_track,
               FrameReaderInterface* reader);
    ~SourceInfo();

    scoped_ptr<PpFrameReceiver> receiver_;
  };

  typedef std::map<FrameReaderInterface*, SourceInfo*> SourceInfoMap;

  // Deliver VideoFrame to the MediaStreamVideoSink associated with
  // |reader|. For testing only.
  void DeliverFrameForTesting(FrameReaderInterface* reader,
                              const scoped_refptr<media::VideoFrame>& frame);

  blink::WebMediaStreamTrack GetFirstVideoTrack(const std::string& url);

  MediaStreamRegistryInterface* registry_;
  SourceInfoMap reader_to_receiver_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(VideoSourceHandler);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_VIDEO_SOURCE_HANDLER_H_
