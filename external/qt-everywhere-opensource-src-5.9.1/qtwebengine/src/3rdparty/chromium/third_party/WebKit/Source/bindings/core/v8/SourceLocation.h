// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SourceLocation_h
#define SourceLocation_h

#include "core/CoreExport.h"
#include "platform/CrossThreadCopier.h"
#include "wtf/Forward.h"
#include "wtf/text/WTFString.h"
#include <memory>
#include <v8-inspector-protocol.h>

namespace blink {

class ExecutionContext;
class TracedValue;

class CORE_EXPORT SourceLocation {
 public:
  // Zero lineNumber and columnNumber mean unknown. Captures current stack
  // trace.
  static std::unique_ptr<SourceLocation> capture(const String& url,
                                                 unsigned lineNumber,
                                                 unsigned columnNumber);

  // Shortcut when location is unknown. Tries to capture call stack or parsing
  // location if available.
  static std::unique_ptr<SourceLocation> capture(ExecutionContext* = nullptr);

  static std::unique_ptr<SourceLocation> fromMessage(v8::Isolate*,
                                                     v8::Local<v8::Message>,
                                                     ExecutionContext*);

  static std::unique_ptr<SourceLocation> fromFunction(v8::Local<v8::Function>);

  // Forces full stack trace.
  static std::unique_ptr<SourceLocation> captureWithFullStackTrace();

  static std::unique_ptr<SourceLocation> create(
      const String& url,
      unsigned lineNumber,
      unsigned columnNumber,
      std::unique_ptr<v8_inspector::V8StackTrace>,
      int scriptId = 0);
  ~SourceLocation();

  bool isUnknown() const {
    return m_url.isNull() && !m_scriptId && !m_lineNumber;
  }
  const String& url() const { return m_url; }
  unsigned lineNumber() const { return m_lineNumber; }
  unsigned columnNumber() const { return m_columnNumber; }
  int scriptId() const { return m_scriptId; }
  std::unique_ptr<v8_inspector::V8StackTrace> takeStackTrace() {
    return std::move(m_stackTrace);
  }

  std::unique_ptr<SourceLocation> clone()
      const;  // Safe to pass between threads.

  // No-op when stack trace is unknown.
  void toTracedValue(TracedValue*, const char* name) const;

  // Could be null string when stack trace is unknown.
  String toString() const;

  // Could be null when stack trace is unknown.
  std::unique_ptr<v8_inspector::protocol::Runtime::API::StackTrace>
  buildInspectorObject() const;

 private:
  SourceLocation(const String& url,
                 unsigned lineNumber,
                 unsigned columnNumber,
                 std::unique_ptr<v8_inspector::V8StackTrace>,
                 int scriptId);
  static std::unique_ptr<SourceLocation> createFromNonEmptyV8StackTrace(
      std::unique_ptr<v8_inspector::V8StackTrace>,
      int scriptId);

  String m_url;
  unsigned m_lineNumber;
  unsigned m_columnNumber;
  std::unique_ptr<v8_inspector::V8StackTrace> m_stackTrace;
  int m_scriptId;
};

}  // namespace blink

#endif  // SourceLocation_h
