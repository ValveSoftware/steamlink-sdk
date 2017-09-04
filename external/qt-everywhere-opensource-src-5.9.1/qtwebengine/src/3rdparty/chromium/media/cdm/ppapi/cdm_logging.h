// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines useful logging macros/methods for CDM adapter.

#ifndef MEDIA_CDM_PPAPI_CDM_LOGGING_H_
#define MEDIA_CDM_PPAPI_CDM_LOGGING_H_

#include <ostream>
#include <sstream>
#include <string>

namespace media {

namespace {

// The following classes/macros are adapted from base/logging.h.

// This class is used to explicitly ignore values in the conditional
// logging macros.  This avoids compiler warnings like "value computed
// is not used" and "statement has no effect".
class LogMessageVoidify {
 public:
  LogMessageVoidify() {}
  // This has to be an operator with a precedence lower than << but
  // higher than ?:
  void operator&(std::ostream&) {}
};

}  // namespace

// This class is used to avoid having to include <iostream> in this file.
class CdmLogStream {
 public:
  CdmLogStream() {}

  // Retrieves the stream that we write to. This header cannot depend on
  // <iostream> because that will add static initializers to all files that
  // include this header. See crbug.com/94794.
  std::ostream& stream();
};

// This class serves two purposes:
// (1) It adds common headers to the log message, e.g. timestamp, process ID.
// (2) It adds a line break at the end of the log message.
// This class is copied and modified from base/logging.* but is quite different
// in terms of how things work. This class is designed to work only with the
// CDM_DLOG() defined below and should not be used for other purposes.
class CdmLogMessage {
 public:
  CdmLogMessage(const char* file, int line);
  ~CdmLogMessage();

  std::string message() { return stream_.str(); }

 private:
  std::ostringstream stream_;
};

// Helper macro which avoids evaluating the arguments to a stream if
// the condition doesn't hold.
#define CDM_LAZY_STREAM(stream, condition) \
  !(condition) ? (void) 0 : LogMessageVoidify() & (stream)

#if defined(NDEBUG)
// Logging is disabled for the release builds, theoretically the compiler should
// take care of removing the references to CdmLogMessage but it's not always the
// case when some specific optimizations are turned on (like PGO). Update the
// macro to make sure that we don't try to do any logging or to refer to
// CdmLogMessage in release.
#define CDM_DLOG_IS_ON() false
#define CDM_DLOG() CDM_LAZY_STREAM(CdmLogStream().stream(), CDM_DLOG_IS_ON())
#else
#define CDM_DLOG_IS_ON() true
#define CDM_DLOG() CDM_LAZY_STREAM(CdmLogStream().stream(), CDM_DLOG_IS_ON()) \
    << CdmLogMessage(__FILE__, __LINE__).message()
#endif

}  // namespace media

#endif  // MEDIA_CDM_PPAPI_CDM_LOGGING_H_
