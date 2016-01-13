// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/monitoring/fake_gcm_stats_recorder.h"

namespace gcm {

FakeGCMStatsRecorder::FakeGCMStatsRecorder() {
}

FakeGCMStatsRecorder::~FakeGCMStatsRecorder() {
}

void FakeGCMStatsRecorder::RecordCheckinInitiated(uint64 android_id) {
}

void FakeGCMStatsRecorder::RecordCheckinDelayedDueToBackoff(int64 delay_msec) {
}

void FakeGCMStatsRecorder::RecordCheckinSuccess() {
}

void FakeGCMStatsRecorder::RecordCheckinFailure(std::string status,
                                            bool will_retry) {
}

void FakeGCMStatsRecorder::RecordConnectionInitiated(const std::string& host) {
}

void FakeGCMStatsRecorder::RecordConnectionDelayedDueToBackoff(
    int64 delay_msec) {
}

void FakeGCMStatsRecorder::RecordConnectionSuccess() {
}

void FakeGCMStatsRecorder::RecordConnectionFailure(int network_error) {
}

void FakeGCMStatsRecorder::RecordConnectionResetSignaled(
      ConnectionFactory::ConnectionResetReason reason) {
}

void FakeGCMStatsRecorder::RecordRegistrationSent(
    const std::string& app_id,
    const std::string& sender_ids) {
}

void FakeGCMStatsRecorder::RecordRegistrationResponse(
    const std::string& app_id,
    const std::vector<std::string>& sender_ids,
    RegistrationRequest::Status status) {
}

void FakeGCMStatsRecorder::RecordRegistrationRetryRequested(
    const std::string& app_id,
    const std::vector<std::string>& sender_ids,
    int retries_left) {
}

void FakeGCMStatsRecorder::RecordUnregistrationSent(
    const std::string& app_id) {
}

void FakeGCMStatsRecorder::RecordUnregistrationResponse(
    const std::string& app_id,
    UnregistrationRequest::Status status) {
}

void FakeGCMStatsRecorder::RecordUnregistrationRetryDelayed(
    const std::string& app_id,
    int64 delay_msec) {
}

void FakeGCMStatsRecorder::RecordDataMessageReceived(
    const std::string& app_id,
    const std::string& from,
    int message_byte_size,
    bool to_registered_app,
    ReceivedMessageType message_type) {
}

void FakeGCMStatsRecorder::RecordDataSentToWire(
    const std::string& app_id,
    const std::string& receiver_id,
    const std::string& message_id,
    int queued) {
}

void FakeGCMStatsRecorder::RecordNotifySendStatus(
    const std::string& app_id,
    const std::string& receiver_id,
    const std::string& message_id,
    gcm::MCSClient::MessageSendStatus status,
    int byte_size,
    int ttl) {
}

void FakeGCMStatsRecorder::RecordIncomingSendError(
    const std::string& app_id,
    const std::string& receiver_id,
    const std::string& message_id) {
}


}  // namespace gcm
