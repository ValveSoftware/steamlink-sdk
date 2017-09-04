// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/session/connection_status.h"

#include "base/metrics/histogram_macros.h"

namespace blimp {
namespace client {

ConnectionStatus::ConnectionStatus()
    : is_connected_(false), weak_factory_(this) {}

ConnectionStatus::~ConnectionStatus() = default;

void ConnectionStatus::AddObserver(NetworkEventObserver* observer) {
  connection_observers_.AddObserver(observer);
}

void ConnectionStatus::RemoveObserver(NetworkEventObserver* observer) {
  connection_observers_.RemoveObserver(observer);
}

void ConnectionStatus::OnAssignmentResult(int result,
                                          const Assignment& assignment) {
  if (result == AssignmentRequestResult::ASSIGNMENT_REQUEST_RESULT_OK) {
    engine_endpoint_ = assignment.engine_endpoint;
  }
}

base::WeakPtr<ConnectionStatus> ConnectionStatus::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void ConnectionStatus::OnConnected() {
  is_connected_ = true;
  UMA_HISTOGRAM_BOOLEAN("Blimp.Connected", true);
  for (auto& observer : connection_observers_)
    observer.OnConnected();
}

void ConnectionStatus::OnDisconnected(int result) {
  is_connected_ = false;
  UMA_HISTOGRAM_BOOLEAN("Blimp.Connected", false);
  for (auto& observer : connection_observers_)
    observer.OnDisconnected(result);
}

}  // namespace client
}  // namespace blimp
