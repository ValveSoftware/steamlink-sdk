// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/logger.h"

#include <stdint.h>

#include <string>
#include <utility>

#include "base/strings/string_util.h"
#include "base/time/clock.h"
#include "extensions/browser/api/cast_channel/cast_auth_util.h"
#include "extensions/browser/api/cast_channel/cast_socket.h"
#include "extensions/browser/api/cast_channel/logger_util.h"
#include "net/base/net_errors.h"
#include "third_party/zlib/zlib.h"

namespace extensions {
namespace api {
namespace cast_channel {

using net::IPEndPoint;
using proto::AggregatedSocketEvent;
using proto::EventType;
using proto::Log;
using proto::SocketEvent;

namespace {

const char* kInternalNamespacePrefix = "com.google.cast";

proto::ChallengeReplyErrorType ChallegeReplyErrorToProto(
    AuthResult::ErrorType error_type) {
  switch (error_type) {
    case AuthResult::ERROR_NONE:
      return proto::CHALLENGE_REPLY_ERROR_NONE;
    case AuthResult::ERROR_PEER_CERT_EMPTY:
      return proto::CHALLENGE_REPLY_ERROR_PEER_CERT_EMPTY;
    case AuthResult::ERROR_WRONG_PAYLOAD_TYPE:
      return proto::CHALLENGE_REPLY_ERROR_WRONG_PAYLOAD_TYPE;
    case AuthResult::ERROR_NO_PAYLOAD:
      return proto::CHALLENGE_REPLY_ERROR_NO_PAYLOAD;
    case AuthResult::ERROR_PAYLOAD_PARSING_FAILED:
      return proto::CHALLENGE_REPLY_ERROR_PAYLOAD_PARSING_FAILED;
    case AuthResult::ERROR_MESSAGE_ERROR:
      return proto::CHALLENGE_REPLY_ERROR_MESSAGE_ERROR;
    case AuthResult::ERROR_NO_RESPONSE:
      return proto::CHALLENGE_REPLY_ERROR_NO_RESPONSE;
    case AuthResult::ERROR_FINGERPRINT_NOT_FOUND:
      return proto::CHALLENGE_REPLY_ERROR_FINGERPRINT_NOT_FOUND;
    case AuthResult::ERROR_CERT_PARSING_FAILED:
      return proto::CHALLENGE_REPLY_ERROR_CERT_PARSING_FAILED;
    case AuthResult::ERROR_CERT_NOT_SIGNED_BY_TRUSTED_CA:
      return proto::CHALLENGE_REPLY_ERROR_CERT_NOT_SIGNED_BY_TRUSTED_CA;
    case AuthResult::ERROR_CANNOT_EXTRACT_PUBLIC_KEY:
      return proto::CHALLENGE_REPLY_ERROR_CANNOT_EXTRACT_PUBLIC_KEY;
    case AuthResult::ERROR_SIGNED_BLOBS_MISMATCH:
      return proto::CHALLENGE_REPLY_ERROR_SIGNED_BLOBS_MISMATCH;
    default:
      NOTREACHED();
      return proto::CHALLENGE_REPLY_ERROR_NONE;
  }
}

std::unique_ptr<char[]> Compress(const std::string& input, size_t* length) {
  *length = 0;
  z_stream stream = {0};
  int result = deflateInit2(&stream,
                            Z_DEFAULT_COMPRESSION,
                            Z_DEFLATED,
                            // 16 is added to produce a gzip header + trailer.
                            MAX_WBITS + 16,
                            8,  // memLevel = 8 is default.
                            Z_DEFAULT_STRATEGY);
  DCHECK_EQ(Z_OK, result);

  size_t out_size = deflateBound(&stream, input.size());
  std::unique_ptr<char[]> out(new char[out_size]);

  stream.next_in = reinterpret_cast<uint8_t*>(const_cast<char*>(input.data()));
  stream.avail_in = input.size();
  stream.next_out = reinterpret_cast<uint8_t*>(out.get());
  stream.avail_out = out_size;

  // Do a one-shot compression. This will return Z_STREAM_END only if |output|
  // is large enough to hold all compressed data.
  result = deflate(&stream, Z_FINISH);

  bool success = (result == Z_STREAM_END);

  if (!success)
    VLOG(2) << "deflate() failed. Result: " << result;

  result = deflateEnd(&stream);
  DCHECK(result == Z_OK || result == Z_DATA_ERROR);

  if (success)
    *length = out_size - stream.avail_out;

  return out;
}

// Propagate any error fields set in |event| to |last_errors|.  If any error
// field in |event| is set, then also set |last_errors->event_type|.
void MaybeSetLastErrors(const SocketEvent& event, LastErrors* last_errors) {
  if (event.has_net_return_value() &&
      event.net_return_value() < net::ERR_IO_PENDING) {
    last_errors->net_return_value = event.net_return_value();
    last_errors->event_type = event.type();
  }
  if (event.has_challenge_reply_error_type()) {
    last_errors->challenge_reply_error_type =
        event.challenge_reply_error_type();
    last_errors->event_type = event.type();
  }
}

}  // namespace

Logger::AggregatedSocketEventLog::AggregatedSocketEventLog() {
}

Logger::AggregatedSocketEventLog::~AggregatedSocketEventLog() {
}

Logger::Logger(std::unique_ptr<base::Clock> clock, base::Time unix_epoch_time)
    : clock_(std::move(clock)), unix_epoch_time_(unix_epoch_time) {
  DCHECK(clock_);

  // Logger may not be necessarily be created on the IO thread, but logging
  // happens exclusively there.
  thread_checker_.DetachFromThread();
}

Logger::~Logger() {
}

void Logger::LogNewSocketEvent(const CastSocket& cast_socket) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SocketEvent event = CreateEvent(proto::CAST_SOCKET_CREATED);
  AggregatedSocketEvent& aggregated_socket_event =
      LogSocketEvent(cast_socket.id(), event);

  const net::IPAddress& ip = cast_socket.ip_endpoint().address();
  DCHECK(ip.IsValid());
  aggregated_socket_event.set_endpoint_id(ip.bytes().back());
  aggregated_socket_event.set_channel_auth_type(cast_socket.channel_auth() ==
                                                        CHANNEL_AUTH_TYPE_SSL
                                                    ? proto::SSL
                                                    : proto::SSL_VERIFIED);
}

void Logger::LogSocketEvent(int channel_id, EventType event_type) {
  DCHECK(thread_checker_.CalledOnValidThread());

  LogSocketEventWithDetails(channel_id, event_type, std::string());
}

void Logger::LogSocketEventWithDetails(int channel_id,
                                       EventType event_type,
                                       const std::string& details) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SocketEvent event = CreateEvent(event_type);
  if (!details.empty())
    event.set_details(details);

  LogSocketEvent(channel_id, event);
}

void Logger::LogSocketEventWithRv(int channel_id,
                                  EventType event_type,
                                  int rv) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SocketEvent event = CreateEvent(event_type);
  event.set_net_return_value(rv);

  AggregatedSocketEvent& aggregated_socket_event =
      LogSocketEvent(channel_id, event);

  if ((event_type == proto::SOCKET_READ || event_type == proto::SOCKET_WRITE) &&
      rv > 0) {
    if (event_type == proto::SOCKET_READ) {
      aggregated_socket_event.set_bytes_read(
          aggregated_socket_event.bytes_read() + rv);
    } else {
      aggregated_socket_event.set_bytes_written(
          aggregated_socket_event.bytes_written() + rv);
    }
  }
}

void Logger::LogSocketReadyState(int channel_id, proto::ReadyState new_state) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SocketEvent event = CreateEvent(proto::READY_STATE_CHANGED);
  event.set_ready_state(new_state);

  LogSocketEvent(channel_id, event);
}

void Logger::LogSocketConnectState(int channel_id,
                                   proto::ConnectionState new_state) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SocketEvent event = CreateEvent(proto::CONNECTION_STATE_CHANGED);
  event.set_connection_state(new_state);

  LogSocketEvent(channel_id, event);
}

void Logger::LogSocketReadState(int channel_id, proto::ReadState new_state) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SocketEvent event = CreateEvent(proto::READ_STATE_CHANGED);
  event.set_read_state(new_state);

  LogSocketEvent(channel_id, event);
}

void Logger::LogSocketWriteState(int channel_id, proto::WriteState new_state) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SocketEvent event = CreateEvent(proto::WRITE_STATE_CHANGED);
  event.set_write_state(new_state);

  LogSocketEvent(channel_id, event);
}

void Logger::LogSocketErrorState(int channel_id, proto::ErrorState new_state) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SocketEvent event = CreateEvent(proto::ERROR_STATE_CHANGED);
  event.set_error_state(new_state);

  LogSocketEvent(channel_id, event);
}

void Logger::LogSocketEventForMessage(int channel_id,
                                      EventType event_type,
                                      const std::string& message_namespace,
                                      const std::string& details) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SocketEvent event = CreateEvent(event_type);
  if (base::StartsWith(message_namespace, kInternalNamespacePrefix,
                       base::CompareCase::INSENSITIVE_ASCII))
    event.set_message_namespace(message_namespace);
  event.set_details(details);

  LogSocketEvent(channel_id, event);
}

void Logger::LogSocketChallengeReplyEvent(int channel_id,
                                          const AuthResult& auth_result) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SocketEvent event = CreateEvent(proto::AUTH_CHALLENGE_REPLY);
  event.set_challenge_reply_error_type(
      ChallegeReplyErrorToProto(auth_result.error_type));

  LogSocketEvent(channel_id, event);
}

SocketEvent Logger::CreateEvent(EventType event_type) {
  SocketEvent event;
  event.set_type(event_type);
  event.set_timestamp_micros(
      (clock_->Now() - unix_epoch_time_).InMicroseconds());
  return event;
}

AggregatedSocketEvent& Logger::LogSocketEvent(int channel_id,
                                              const SocketEvent& socket_event) {
  AggregatedSocketEventLogMap::iterator it =
      aggregated_socket_events_.find(channel_id);
  if (it == aggregated_socket_events_.end()) {
    if (aggregated_socket_events_.size() >= kMaxSocketsToLog) {
      AggregatedSocketEventLogMap::iterator erase_it =
          aggregated_socket_events_.begin();

      log_.set_num_evicted_aggregated_socket_events(
          log_.num_evicted_aggregated_socket_events() + 1);
      log_.set_num_evicted_socket_events(
          log_.num_evicted_socket_events() +
          erase_it->second->socket_events.size());

      aggregated_socket_events_.erase(erase_it);
    }

    it = aggregated_socket_events_
             .insert(std::make_pair(
                 channel_id, make_linked_ptr(new AggregatedSocketEventLog)))
             .first;
    it->second->aggregated_socket_event.set_id(channel_id);
  }

  std::deque<proto::SocketEvent>& socket_events = it->second->socket_events;
  if (socket_events.size() >= kMaxEventsPerSocket) {
    socket_events.pop_front();
    log_.set_num_evicted_socket_events(log_.num_evicted_socket_events() + 1);
  }
  socket_events.push_back(socket_event);

  MaybeSetLastErrors(socket_event, &(it->second->last_errors));

  return it->second->aggregated_socket_event;
}

std::unique_ptr<char[]> Logger::GetLogs(size_t* length) const {
  *length = 0;

  Log log;
  // Copy "global" values from |log_|. Don't use |log_| directly since this
  // function is const.
  log.CopyFrom(log_);

  for (AggregatedSocketEventLogMap::const_iterator it =
           aggregated_socket_events_.begin();
       it != aggregated_socket_events_.end();
       ++it) {
    AggregatedSocketEvent* new_aggregated_socket_event =
        log.add_aggregated_socket_event();
    new_aggregated_socket_event->CopyFrom(it->second->aggregated_socket_event);

    const std::deque<SocketEvent>& socket_events = it->second->socket_events;
    for (std::deque<SocketEvent>::const_iterator socket_event_it =
             socket_events.begin();
         socket_event_it != socket_events.end();
         ++socket_event_it) {
      SocketEvent* socket_event =
          new_aggregated_socket_event->add_socket_event();
      socket_event->CopyFrom(*socket_event_it);
    }
  }

  std::string serialized;
  if (!log.SerializeToString(&serialized)) {
    VLOG(2) << "Failed to serialized proto to string.";
    return std::unique_ptr<char[]>();
  }

  return Compress(serialized, length);
}

void Logger::Reset() {
  aggregated_socket_events_.clear();
  log_.Clear();
}

LastErrors Logger::GetLastErrors(int channel_id) const {
  AggregatedSocketEventLogMap::const_iterator it =
      aggregated_socket_events_.find(channel_id);
  if (it != aggregated_socket_events_.end()) {
    return it->second->last_errors;
  } else {
    return LastErrors();
  }
}

}  // namespace cast_channel
}  // namespace api
}  // namespace extensions
