// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_LOG_H_
#define MEDIA_BASE_MEDIA_LOG_H_

#include <sstream>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"
#include "media/base/media_log_event.h"
#include "media/base/pipeline.h"
#include "media/base/pipeline_status.h"

namespace media {

// Indicates a string should be added to the log.
// First parameter - The string to add to the log.
typedef base::Callback<void(const std::string&)> LogCB;

// Helper class to make it easier to use log_cb like DVLOG().
class LogHelper {
 public:
  LogHelper(const LogCB& Log_cb);
  ~LogHelper();

  std::ostream& stream() { return stream_; }

 private:
  LogCB log_cb_;
  std::stringstream stream_;
};

#define MEDIA_LOG(log_cb) LogHelper(log_cb).stream()

class MEDIA_EXPORT MediaLog : public base::RefCountedThreadSafe<MediaLog> {
 public:
  // Convert various enums to strings.
  static const char* EventTypeToString(MediaLogEvent::Type type);
  static const char* PipelineStatusToString(PipelineStatus);

  MediaLog();

  // Add an event to this log. Overriden by inheritors to actually do something
  // with it.
  virtual void AddEvent(scoped_ptr<MediaLogEvent> event);

  // Helper methods to create events and their parameters.
  scoped_ptr<MediaLogEvent> CreateEvent(MediaLogEvent::Type type);
  scoped_ptr<MediaLogEvent> CreateBooleanEvent(
      MediaLogEvent::Type type, const char* property, bool value);
  scoped_ptr<MediaLogEvent> CreateStringEvent(
      MediaLogEvent::Type type, const char* property, const std::string& value);
  scoped_ptr<MediaLogEvent> CreateTimeEvent(
      MediaLogEvent::Type type, const char* property, base::TimeDelta value);
  scoped_ptr<MediaLogEvent> CreateLoadEvent(const std::string& url);
  scoped_ptr<MediaLogEvent> CreateSeekEvent(float seconds);
  scoped_ptr<MediaLogEvent> CreatePipelineStateChangedEvent(
      Pipeline::State state);
  scoped_ptr<MediaLogEvent> CreatePipelineErrorEvent(PipelineStatus error);
  scoped_ptr<MediaLogEvent> CreateVideoSizeSetEvent(
      size_t width, size_t height);
  scoped_ptr<MediaLogEvent> CreateBufferedExtentsChangedEvent(
      int64 start, int64 current, int64 end);
  scoped_ptr<MediaLogEvent> CreateMediaSourceErrorEvent(
      const std::string& error);

  // Report a property change without an accompanying event.
  void SetStringProperty(const char* key, const std::string& value);
  void SetIntegerProperty(const char* key, int value);
  void SetDoubleProperty(const char* key, double value);
  void SetBooleanProperty(const char* key, bool value);
  void SetTimeProperty(const char* key, base::TimeDelta value);

 protected:
  friend class base::RefCountedThreadSafe<MediaLog>;
  virtual ~MediaLog();

 private:
  // A unique (to this process) id for this MediaLog.
  int32 id_;

  DISALLOW_COPY_AND_ASSIGN(MediaLog);
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_LOG_H_
