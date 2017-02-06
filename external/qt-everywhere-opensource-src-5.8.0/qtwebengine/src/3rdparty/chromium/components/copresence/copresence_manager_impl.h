// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_COPRESENCE_MANAGER_IMPL_H_
#define COMPONENTS_COPRESENCE_COPRESENCE_MANAGER_IMPL_H_

#include <google/protobuf/repeated_field.h>

#include <memory>
#include <string>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "components/copresence/copresence_state_impl.h"
#include "components/copresence/public/copresence_manager.h"
#include "components/copresence/timed_map.h"

namespace audio_modem {
struct AudioToken;
}

namespace base {
class Timer;
}

namespace net {
class URLContextGetter;
}

namespace copresence {

class DirectiveHandler;
class GCMHandler;
class ReportRequest;
class RpcHandler;
class SubscribedMessage;

// The implementation for CopresenceManager. Responsible primarily for
// client-side initialization. The RpcHandler handles all the details
// of interacting with the server.
// TODO(ckehoe, rkc): Add tests for this class.
class CopresenceManagerImpl : public CopresenceManager {
 public:
  // The delegate is owned by the caller, and must outlive the manager.
  explicit CopresenceManagerImpl(CopresenceDelegate* delegate);

  ~CopresenceManagerImpl() override;

  // CopresenceManager overrides.
  CopresenceState* state() override;
  void ExecuteReportRequest(const ReportRequest& request,
                            const std::string& app_id,
                            const std::string& auth_token,
                            const StatusCallback& callback) override;

 private:
  // Complete initialization when Whispernet is available.
  void WhispernetInitComplete(bool success);

  // Handle tokens decoded by Whispernet.
  // TODO(ckehoe): Replace AudioToken with ReceivedToken.
  void ReceivedTokens(const std::vector<audio_modem::AudioToken>& tokens);

  // Verifies that we can hear the audio we're playing.
  // This gets called every kAudioCheckIntervalMs milliseconds.
  void AudioCheck();

  // This gets called every kPollTimerIntervalMs milliseconds
  // to poll the server for new messages.
  void PollForMessages();

  // Send SubscribedMessages to the appropriate clients.
  void DispatchMessages(
      const google::protobuf::RepeatedPtrField<SubscribedMessage>&
      subscribed_messages);

  // Belongs to the caller.
  CopresenceDelegate* const delegate_;

  // We use a CancelableCallback here because Whispernet
  // does not provide a way to unregister its init callback.
  base::CancelableCallback<void(bool)> whispernet_init_callback_;

  bool init_failed_;

  // The RpcHandler makes calls to the other objects here, so it must come last.
  std::unique_ptr<CopresenceStateImpl> state_;
  std::unique_ptr<DirectiveHandler> directive_handler_;
  std::unique_ptr<GCMHandler> gcm_handler_;
  std::unique_ptr<RpcHandler> rpc_handler_;

  std::unique_ptr<base::Timer> poll_timer_;
  std::unique_ptr<base::Timer> audio_check_timer_;

  TimedMap<std::string, google::protobuf::RepeatedPtrField<SubscribedMessage>>
  queued_messages_by_token_;

  DISALLOW_COPY_AND_ASSIGN(CopresenceManagerImpl);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_COPRESENCE_MANAGER_IMPL_H_
