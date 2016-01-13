// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_log.h"

#include <string>

#include "base/atomic_sequence_num.h"
#include "base/logging.h"
#include "base/values.h"

namespace media {

// A count of all MediaLogs created in the current process. Used to generate
// unique IDs.
static base::StaticAtomicSequenceNumber g_media_log_count;

const char* MediaLog::EventTypeToString(MediaLogEvent::Type type) {
  switch (type) {
    case MediaLogEvent::WEBMEDIAPLAYER_CREATED:
      return "WEBMEDIAPLAYER_CREATED";
    case MediaLogEvent::WEBMEDIAPLAYER_DESTROYED:
      return "WEBMEDIAPLAYER_DESTROYED";
    case MediaLogEvent::PIPELINE_CREATED:
      return "PIPELINE_CREATED";
    case MediaLogEvent::PIPELINE_DESTROYED:
      return "PIPELINE_DESTROYED";
    case MediaLogEvent::LOAD:
      return "LOAD";
    case MediaLogEvent::SEEK:
      return "SEEK";
    case MediaLogEvent::PLAY:
      return "PLAY";
    case MediaLogEvent::PAUSE:
      return "PAUSE";
    case MediaLogEvent::PIPELINE_STATE_CHANGED:
      return "PIPELINE_STATE_CHANGED";
    case MediaLogEvent::PIPELINE_ERROR:
      return "PIPELINE_ERROR";
    case MediaLogEvent::VIDEO_SIZE_SET:
      return "VIDEO_SIZE_SET";
    case MediaLogEvent::DURATION_SET:
      return "DURATION_SET";
    case MediaLogEvent::TOTAL_BYTES_SET:
      return "TOTAL_BYTES_SET";
    case MediaLogEvent::NETWORK_ACTIVITY_SET:
      return "NETWORK_ACTIVITY_SET";
    case MediaLogEvent::AUDIO_ENDED:
      return "AUDIO_ENDED";
    case MediaLogEvent::VIDEO_ENDED:
      return "VIDEO_ENDED";
    case MediaLogEvent::TEXT_ENDED:
      return "TEXT_ENDED";
    case MediaLogEvent::BUFFERED_EXTENTS_CHANGED:
      return "BUFFERED_EXTENTS_CHANGED";
    case MediaLogEvent::MEDIA_SOURCE_ERROR:
      return "MEDIA_SOURCE_ERROR";
    case MediaLogEvent::PROPERTY_CHANGE:
      return "PROPERTY_CHANGE";
  }
  NOTREACHED();
  return NULL;
}

const char* MediaLog::PipelineStatusToString(PipelineStatus status) {
  switch (status) {
    case PIPELINE_OK:
      return "pipeline: ok";
    case PIPELINE_ERROR_URL_NOT_FOUND:
      return "pipeline: url not found";
    case PIPELINE_ERROR_NETWORK:
      return "pipeline: network error";
    case PIPELINE_ERROR_DECODE:
      return "pipeline: decode error";
    case PIPELINE_ERROR_DECRYPT:
      return "pipeline: decrypt error";
    case PIPELINE_ERROR_ABORT:
      return "pipeline: abort";
    case PIPELINE_ERROR_INITIALIZATION_FAILED:
      return "pipeline: initialization failed";
    case PIPELINE_ERROR_COULD_NOT_RENDER:
      return "pipeline: could not render";
    case PIPELINE_ERROR_READ:
      return "pipeline: read error";
    case PIPELINE_ERROR_OPERATION_PENDING:
      return "pipeline: operation pending";
    case PIPELINE_ERROR_INVALID_STATE:
      return "pipeline: invalid state";
    case DEMUXER_ERROR_COULD_NOT_OPEN:
      return "demuxer: could not open";
    case DEMUXER_ERROR_COULD_NOT_PARSE:
      return "dumuxer: could not parse";
    case DEMUXER_ERROR_NO_SUPPORTED_STREAMS:
      return "demuxer: no supported streams";
    case DECODER_ERROR_NOT_SUPPORTED:
      return "decoder: not supported";
  }
  NOTREACHED();
  return NULL;
}

LogHelper::LogHelper(const LogCB& log_cb) : log_cb_(log_cb) {}

LogHelper::~LogHelper() {
  if (log_cb_.is_null())
    return;
  log_cb_.Run(stream_.str());
}

MediaLog::MediaLog() : id_(g_media_log_count.GetNext()) {}

MediaLog::~MediaLog() {}

void MediaLog::AddEvent(scoped_ptr<MediaLogEvent> event) {}

scoped_ptr<MediaLogEvent> MediaLog::CreateEvent(MediaLogEvent::Type type) {
  scoped_ptr<MediaLogEvent> event(new MediaLogEvent);
  event->id = id_;
  event->type = type;
  event->time = base::TimeTicks::Now();
  return event.Pass();
}

scoped_ptr<MediaLogEvent> MediaLog::CreateBooleanEvent(
    MediaLogEvent::Type type, const char* property, bool value) {
  scoped_ptr<MediaLogEvent> event(CreateEvent(type));
  event->params.SetBoolean(property, value);
  return event.Pass();
}

scoped_ptr<MediaLogEvent> MediaLog::CreateStringEvent(
    MediaLogEvent::Type type, const char* property, const std::string& value) {
  scoped_ptr<MediaLogEvent> event(CreateEvent(type));
  event->params.SetString(property, value);
  return event.Pass();
}

scoped_ptr<MediaLogEvent> MediaLog::CreateTimeEvent(
    MediaLogEvent::Type type, const char* property, base::TimeDelta value) {
  scoped_ptr<MediaLogEvent> event(CreateEvent(type));
  if (value.is_max())
    event->params.SetString(property, "unknown");
  else
    event->params.SetDouble(property, value.InSecondsF());
  return event.Pass();
}

scoped_ptr<MediaLogEvent> MediaLog::CreateLoadEvent(const std::string& url) {
  scoped_ptr<MediaLogEvent> event(CreateEvent(MediaLogEvent::LOAD));
  event->params.SetString("url", url);
  return event.Pass();
}

scoped_ptr<MediaLogEvent> MediaLog::CreateSeekEvent(float seconds) {
  scoped_ptr<MediaLogEvent> event(CreateEvent(MediaLogEvent::SEEK));
  event->params.SetDouble("seek_target", seconds);
  return event.Pass();
}

scoped_ptr<MediaLogEvent> MediaLog::CreatePipelineStateChangedEvent(
    Pipeline::State state) {
  scoped_ptr<MediaLogEvent> event(
      CreateEvent(MediaLogEvent::PIPELINE_STATE_CHANGED));
  event->params.SetString("pipeline_state", Pipeline::GetStateString(state));
  return event.Pass();
}

scoped_ptr<MediaLogEvent> MediaLog::CreatePipelineErrorEvent(
    PipelineStatus error) {
  scoped_ptr<MediaLogEvent> event(CreateEvent(MediaLogEvent::PIPELINE_ERROR));
  event->params.SetString("pipeline_error", PipelineStatusToString(error));
  return event.Pass();
}

scoped_ptr<MediaLogEvent> MediaLog::CreateVideoSizeSetEvent(
    size_t width, size_t height) {
  scoped_ptr<MediaLogEvent> event(CreateEvent(MediaLogEvent::VIDEO_SIZE_SET));
  event->params.SetInteger("width", width);
  event->params.SetInteger("height", height);
  return event.Pass();
}

scoped_ptr<MediaLogEvent> MediaLog::CreateBufferedExtentsChangedEvent(
    int64 start, int64 current, int64 end) {
  scoped_ptr<MediaLogEvent> event(
      CreateEvent(MediaLogEvent::BUFFERED_EXTENTS_CHANGED));
  // These values are headed to JS where there is no int64 so we use a double
  // and accept loss of precision above 2^53 bytes (8 Exabytes).
  event->params.SetDouble("buffer_start", start);
  event->params.SetDouble("buffer_current", current);
  event->params.SetDouble("buffer_end", end);
  return event.Pass();
}

scoped_ptr<MediaLogEvent> MediaLog::CreateMediaSourceErrorEvent(
    const std::string& error) {
  scoped_ptr<MediaLogEvent> event(
      CreateEvent(MediaLogEvent::MEDIA_SOURCE_ERROR));
  event->params.SetString("error", error);
  return event.Pass();
}

void MediaLog::SetStringProperty(
    const char* key, const std::string& value) {
  scoped_ptr<MediaLogEvent> event(CreateEvent(MediaLogEvent::PROPERTY_CHANGE));
  event->params.SetString(key, value);
  AddEvent(event.Pass());
}

void MediaLog::SetIntegerProperty(
    const char* key, int value) {
  scoped_ptr<MediaLogEvent> event(CreateEvent(MediaLogEvent::PROPERTY_CHANGE));
  event->params.SetInteger(key, value);
  AddEvent(event.Pass());
}

void MediaLog::SetDoubleProperty(
    const char* key, double value) {
  scoped_ptr<MediaLogEvent> event(CreateEvent(MediaLogEvent::PROPERTY_CHANGE));
  event->params.SetDouble(key, value);
  AddEvent(event.Pass());
}

void MediaLog::SetBooleanProperty(
    const char* key, bool value) {
  scoped_ptr<MediaLogEvent> event(CreateEvent(MediaLogEvent::PROPERTY_CHANGE));
  event->params.SetBoolean(key, value);
  AddEvent(event.Pass());
}

void MediaLog::SetTimeProperty(
    const char* key, base::TimeDelta value) {
  scoped_ptr<MediaLogEvent> event(CreateEvent(MediaLogEvent::PROPERTY_CHANGE));
  if (value.is_max())
    event->params.SetString(key, "unknown");
  else
    event->params.SetDouble(key, value.InSecondsF());
  AddEvent(event.Pass());
}

}  //namespace media
