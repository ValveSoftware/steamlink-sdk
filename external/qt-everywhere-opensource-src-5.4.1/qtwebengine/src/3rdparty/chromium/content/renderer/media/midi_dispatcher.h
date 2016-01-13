// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MIDI_DISPATCHER_H_
#define CONTENT_RENDERER_MEDIA_MIDI_DISPATCHER_H_

#include "base/id_map.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/WebKit/public/web/WebMIDIClient.h"

namespace blink {
class WebMIDIPermissionRequest;
}

namespace content {

// MidiDispatcher implements WebMIDIClient to handle permissions for using
// system exclusive messages.
// It works as RenderFrameObserver to handle IPC messages between
// MidiDispatcherHost owned by WebContents since permissions are managed in
// the browser process.
class MidiDispatcher : public RenderFrameObserver,
                       public blink::WebMIDIClient {
 public:
  explicit MidiDispatcher(RenderFrame* render_frame);
  virtual ~MidiDispatcher();

 private:
  // RenderFrameObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // blink::WebMIDIClient implementation.
  virtual void requestSysexPermission(
      const blink::WebMIDIPermissionRequest& request);
  virtual void cancelSysexPermissionRequest(
      const blink::WebMIDIPermissionRequest& request);

  // Permission for using system exclusive messages has been set.
  void OnSysExPermissionApproved(int client_id, bool is_allowed);

  // Each WebMIDIPermissionRequest object is valid until
  // cancelSysexPermissionRequest() is called with the object, or used to call
  // WebMIDIPermissionRequest::setIsAllowed().
  typedef IDMap<blink::WebMIDIPermissionRequest, IDMapOwnPointer> Requests;
  Requests requests_;

  DISALLOW_COPY_AND_ASSIGN(MidiDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MIDI_DISPATCHER_H_
