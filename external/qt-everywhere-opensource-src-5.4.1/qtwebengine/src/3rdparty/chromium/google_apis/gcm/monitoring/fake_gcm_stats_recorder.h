// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_MONITORING_FAKE_GCM_STATS_RECODER_H_
#define GOOGLE_APIS_GCM_MONITORING_FAKE_GCM_STATS_RECODER_H_

#include "google_apis/gcm/monitoring/gcm_stats_recorder.h"

namespace gcm {

// The fake version of GCMStatsRecorder that does nothing.
class FakeGCMStatsRecorder : public GCMStatsRecorder {
 public:
  FakeGCMStatsRecorder();
  virtual ~FakeGCMStatsRecorder();

  virtual void RecordCheckinInitiated(uint64 android_id) OVERRIDE;
  virtual void RecordCheckinDelayedDueToBackoff(int64 delay_msec) OVERRIDE;
  virtual void RecordCheckinSuccess() OVERRIDE;
  virtual void RecordCheckinFailure(std::string status,
                                    bool will_retry) OVERRIDE;
  virtual void RecordConnectionInitiated(const std::string& host) OVERRIDE;
  virtual void RecordConnectionDelayedDueToBackoff(int64 delay_msec) OVERRIDE;
  virtual void RecordConnectionSuccess() OVERRIDE;
  virtual void RecordConnectionFailure(int network_error) OVERRIDE;
  virtual void RecordConnectionResetSignaled(
      ConnectionFactory::ConnectionResetReason reason) OVERRIDE;
  virtual void RecordRegistrationSent(const std::string& app_id,
                                      const std::string& sender_ids) OVERRIDE;
  virtual void RecordRegistrationResponse(
      const std::string& app_id,
      const std::vector<std::string>& sender_ids,
      RegistrationRequest::Status status) OVERRIDE;
  virtual void RecordRegistrationRetryRequested(
      const std::string& app_id,
      const std::vector<std::string>& sender_ids,
      int retries_left) OVERRIDE;
  virtual void RecordUnregistrationSent(const std::string& app_id) OVERRIDE;
  virtual void RecordUnregistrationResponse(
      const std::string& app_id,
      UnregistrationRequest::Status status) OVERRIDE;
  virtual void RecordUnregistrationRetryDelayed(const std::string& app_id,
                                                int64 delay_msec) OVERRIDE;
  virtual void RecordDataMessageReceived(
      const std::string& app_id,
      const std::string& from,
      int message_byte_size,
      bool to_registered_app,
      ReceivedMessageType message_type) OVERRIDE;
  virtual void RecordDataSentToWire(const std::string& app_id,
                                    const std::string& receiver_id,
                                    const std::string& message_id,
                                    int queued) OVERRIDE;
  virtual void RecordNotifySendStatus(const std::string& app_id,
                                      const std::string& receiver_id,
                                      const std::string& message_id,
                                      MCSClient::MessageSendStatus status,
                                      int byte_size,
                                      int ttl) OVERRIDE;
  virtual void RecordIncomingSendError(const std::string& app_id,
                                       const std::string& receiver_id,
                                       const std::string& message_id) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeGCMStatsRecorder);
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_MONITORING_FAKE_GCM_STATS_RECODER_H_
