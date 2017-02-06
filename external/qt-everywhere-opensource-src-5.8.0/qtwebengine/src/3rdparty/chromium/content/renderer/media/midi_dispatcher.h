// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MIDI_DISPATCHER_H_
#define CONTENT_RENDERER_MEDIA_MIDI_DISPATCHER_H_

#include "base/id_map.h"
#include "base/macros.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission.mojom.h"
#include "third_party/WebKit/public/web/modules/webmidi/WebMIDIClient.h"

namespace blink {
class WebMIDIPermissionRequest;
}

namespace content {

// MidiDispatcher implements WebMIDIClient to handle permissions for using
// system exclusive messages.
// It works as RenderFrameObserver to keep a 1:1 relationship with a RenderFrame
// and make sure it has the same life cycle.
class MidiDispatcher : public RenderFrameObserver,
                       public blink::WebMIDIClient {
 public:
  explicit MidiDispatcher(RenderFrame* render_frame);
  ~MidiDispatcher() override;

 private:
  // RenderFrameObserver implementation.
  void OnDestruct() override;

  // blink::WebMIDIClient implementation.
  void requestPermission(const blink::WebMIDIPermissionRequest& request,
                         const blink::WebMIDIOptions& options) override;
  void cancelPermissionRequest(
      const blink::WebMIDIPermissionRequest& request) override;

  // Permission for using MIDI system has been set.
  void OnPermissionSet(int request_id, blink::mojom::PermissionStatus status);

  // Each WebMIDIPermissionRequest object is valid until
  // cancelSysexPermissionRequest() is called with the object, or used to call
  // WebMIDIPermissionRequest::setIsAllowed().
  typedef IDMap<blink::WebMIDIPermissionRequest, IDMapOwnPointer> Requests;
  Requests requests_;

  blink::mojom::PermissionServicePtr permission_service_;

  DISALLOW_COPY_AND_ASSIGN(MidiDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MIDI_DISPATCHER_H_
