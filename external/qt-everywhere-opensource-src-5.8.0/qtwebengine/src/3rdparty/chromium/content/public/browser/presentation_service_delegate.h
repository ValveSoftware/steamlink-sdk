// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRESENTATION_SERVICE_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_PRESENTATION_SERVICE_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_vector.h"
#include "content/common/content_export.h"
#include "content/public/browser/presentation_session.h"
#include "content/public/browser/presentation_session_message.h"

namespace content {

class PresentationScreenAvailabilityListener;

using PresentationSessionStartedCallback =
    base::Callback<void(const PresentationSessionInfo&)>;
using PresentationSessionErrorCallback =
    base::Callback<void(const PresentationError&)>;

// Param #0: a vector of messages that are received.
// Param #1: tells the callback handler that it may reuse strings or buffers
//           in the messages contained within param #0.
using PresentationSessionMessageCallback = base::Callback<
    void(const ScopedVector<content::PresentationSessionMessage>&, bool)>;

struct PresentationConnectionStateChangeInfo {
  explicit PresentationConnectionStateChangeInfo(
      PresentationConnectionState state)
      : state(state),
        close_reason(PRESENTATION_CONNECTION_CLOSE_REASON_CONNECTION_ERROR) {}
  ~PresentationConnectionStateChangeInfo() = default;

  PresentationConnectionState state;

  // |close_reason| and |messsage| are only used for state change to CLOSED.
  PresentationConnectionCloseReason close_reason;
  std::string message;
};

using PresentationConnectionStateChangedCallback =
    base::Callback<void(const PresentationConnectionStateChangeInfo&)>;

// An interface implemented by embedders to handle presentation API calls
// forwarded from PresentationServiceImpl.
class CONTENT_EXPORT PresentationServiceDelegate {
 public:
  // Observer interface to listen for changes to PresentationServiceDelegate.
  class CONTENT_EXPORT Observer {
   public:
    // Called when the PresentationServiceDelegate is being destroyed.
    virtual void OnDelegateDestroyed() = 0;

   protected:
    virtual ~Observer() {}
  };

  using SendMessageCallback = base::Callback<void(bool)>;

  virtual ~PresentationServiceDelegate() {}

  // Registers an observer associated with frame with |render_process_id|
  // and |render_frame_id| with this class to listen for updates.
  // This class does not own the observer.
  // It is an error to add an observer if there is already an observer for that
  // frame.
  virtual void AddObserver(int render_process_id,
                           int render_frame_id,
                           Observer* observer) = 0;

  // Unregisters the observer associated with the frame with |render_process_id|
  // and |render_frame_id|.
  // The observer will no longer receive updates.
  virtual void RemoveObserver(int render_process_id, int render_frame_id) = 0;

  // Registers |listener| to continuously listen for
  // availability updates for a presentation URL, originated from the frame
  // given by |render_process_id| and |render_frame_id|.
  // This class does not own |listener|.
  // Returns true on success.
  // This call will return false if a listener with the same presentation URL
  // from the same frame is already registered.
  virtual bool AddScreenAvailabilityListener(
      int render_process_id,
      int render_frame_id,
      PresentationScreenAvailabilityListener* listener) = 0;

  // Unregisters |listener| originated from the frame given by
  // |render_process_id| and |render_frame_id| from this class. The listener
  // will no longer receive availability updates.
  virtual void RemoveScreenAvailabilityListener(
      int render_process_id,
      int render_frame_id,
      PresentationScreenAvailabilityListener* listener) = 0;

  // Resets the presentation state for the frame given by |render_process_id|
  // and |render_frame_id|.
  // This unregisters all listeners associated with the given frame, and clears
  // the default presentation URL and ID set for the frame.
  virtual void Reset(
      int render_process_id,
      int render_frame_id) = 0;

  // Sets the default presentation URL for frame given by |render_process_id|
  // and |render_frame_id|. When the default presentation is started on this
  // frame, |callback| will be invoked with the corresponding
  // PresentationSessionInfo object.
  // If |default_presentation_url| is empty, the default presentation URL will
  // be cleared and the previously registered callback (if any) will be removed.
  virtual void SetDefaultPresentationUrl(
      int render_process_id,
      int render_frame_id,
      const std::string& default_presentation_url,
      const PresentationSessionStartedCallback& callback) = 0;

  // Starts a new presentation session. The presentation id of the session will
  // be the default presentation ID if any or a generated one otherwise.
  // Typically, the embedder will allow the user to select a screen to show
  // |presentation_url|.
  // |render_process_id|, |render_frame_id|: ID of originating frame.
  // |presentation_url|: URL of the presentation.
  // |success_cb|: Invoked with session info, if presentation session started
  // successfully.
  // |error_cb|: Invoked with error reason, if presentation session did not
  // start.
  virtual void StartSession(
      int render_process_id,
      int render_frame_id,
      const std::string& presentation_url,
      const PresentationSessionStartedCallback& success_cb,
      const PresentationSessionErrorCallback& error_cb) = 0;

  // Joins an existing presentation session. Unlike StartSession(), this
  // does not bring a screen list UI.
  // |render_process_id|, |render_frame_id|: ID for originating frame.
  // |presentation_url|: URL of the presentation.
  // |presentation_id|: The ID of the presentation to join.
  // |success_cb|: Invoked with session info, if presentation session joined
  // successfully.
  // |error_cb|: Invoked with error reason, if joining failed.
  virtual void JoinSession(
      int render_process_id,
      int render_frame_id,
      const std::string& presentation_url,
      const std::string& presentation_id,
      const PresentationSessionStartedCallback& success_cb,
      const PresentationSessionErrorCallback& error_cb) = 0;

  // Closes an existing presentation connection.
  // |render_process_id|, |render_frame_id|: ID for originating frame.
  // |presentation_id|: The ID of the presentation to close.
  virtual void CloseConnection(int render_process_id,
                               int render_frame_id,
                               const std::string& presentation_id) = 0;

  // Terminates an existing presentation.
  // |render_process_id|, |render_frame_id|: ID for originating frame.
  // |presentation_id|: The ID of the presentation to terminate.
  virtual void Terminate(int render_process_id,
                         int render_frame_id,
                         const std::string& presentation_id) = 0;

  // Listens for messages for a presentation session.
  // |render_process_id|, |render_frame_id|: ID for originating frame.
  // |session|: URL and ID of presentation session to listen for messages.
  // |message_cb|: Invoked with a non-empty list of messages whenever there are
  // messages.
  virtual void ListenForSessionMessages(
      int render_process_id,
      int render_frame_id,
      const content::PresentationSessionInfo& session,
      const PresentationSessionMessageCallback& message_cb) = 0;

  // Sends a message (string or binary data) to a presentation session.
  // |render_process_id|, |render_frame_id|: ID of originating frame.
  // |session|: The presentation session to send the message to.
  // |message|: The message to send. The embedder takes ownership of |message|.
  //            Must not be null.
  // |send_message_cb|: Invoked after handling the send message request.
  virtual void SendMessage(int render_process_id,
                           int render_frame_id,
                           const content::PresentationSessionInfo& session,
                           std::unique_ptr<PresentationSessionMessage> message,
                           const SendMessageCallback& send_message_cb) = 0;

  // Continuously listen for state changes for a PresentationConnection in a
  // frame.
  // |render_process_id|, |render_frame_id|: ID of frame.
  // |connection|: PresentationConnection to listen for state changes.
  // |state_changed_cb|: Invoked with the PresentationConnection and its new
  // state whenever there is a state change.
  virtual void ListenForConnectionStateChange(
      int render_process_id,
      int render_frame_id,
      const PresentationSessionInfo& connection,
      const PresentationConnectionStateChangedCallback& state_changed_cb) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRESENTATION_SERVICE_DELEGATE_H_
