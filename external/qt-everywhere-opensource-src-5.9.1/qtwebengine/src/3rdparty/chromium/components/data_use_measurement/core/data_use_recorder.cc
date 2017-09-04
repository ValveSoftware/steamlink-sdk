// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_use_measurement/core/data_use_recorder.h"

#include "net/url_request/url_request.h"

namespace data_use_measurement {

DataUseRecorder::DataUseRecorder() {}

DataUseRecorder::~DataUseRecorder() {}

bool DataUseRecorder::IsDataUseComplete() {
  return pending_url_requests_.empty() && pending_data_sources_.empty();
}

void DataUseRecorder::OnBeforeUrlRequest(net::URLRequest* request) {
  pending_url_requests_.insert(request);
}

void DataUseRecorder::OnUrlRequestDestroyed(net::URLRequest* request) {
  pending_url_requests_.erase(request);
}

void DataUseRecorder::OnNetworkBytesReceived(net::URLRequest* request,
                                             int64_t bytes_received) {
  data_use_.total_bytes_received_ += bytes_received;
}

void DataUseRecorder::OnNetworkBytesSent(net::URLRequest* request,
                                         int64_t bytes_sent) {
  data_use_.total_bytes_sent_ += bytes_sent;
}

void DataUseRecorder::AddPendingDataSource(void* source) {
  pending_data_sources_.insert(source);
}

bool DataUseRecorder::HasPendingDataSource(void* source) {
  return pending_data_sources_.find(source) != pending_data_sources_.end();
}

void DataUseRecorder::RemovePendingDataSource(void* source) {
  pending_data_sources_.erase(source);
}

bool DataUseRecorder::HasPendingURLRequest(const net::URLRequest* request) {
  return pending_url_requests_.find(request) != pending_url_requests_.end();
}

}  // namespace data_use_measurement
