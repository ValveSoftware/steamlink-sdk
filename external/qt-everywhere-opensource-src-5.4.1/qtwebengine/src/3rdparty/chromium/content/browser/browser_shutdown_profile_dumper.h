// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_SHUTDOWN_PROFILE_DUMPER_H_
#define CONTENT_BROWSER_BROWSER_SHUTDOWN_PROFILE_DUMPER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "content/common/content_export.h"

namespace base {
class FilePath;
class WaitableEvent;
}

namespace content {

// This class is intended to dump the tracing results of the shutdown process
// to a file before the browser process exits.
// It will save the file either into the command line passed
// "--trace-shutdown-file=<name>" parameter - or - to "chrometrace.log" in the
// current directory.
// Use the class with a scoped_ptr to get files written in the destructor.
// Note that we cannot use the asynchronous file writer since the
// |SequencedWorkerPool| will get killed in the shutdown process.
class BrowserShutdownProfileDumper {
 public:
  BrowserShutdownProfileDumper();

  ~BrowserShutdownProfileDumper();

 private:
  // Writes all traces which happened to disk.
  void WriteTracesToDisc(const base::FilePath& file_name);

  void EndTraceAndFlush(base::WaitableEvent* flush_complete_event);

  // Returns the file name where we should save the trace dump to.
  base::FilePath GetFileName();

  // The callback for the |TraceLog::Flush| function. It saves all traces to
  // disc.
  void WriteTraceDataCollected(
      base::WaitableEvent* flush_complete_event,
      const scoped_refptr<base::RefCountedString>& events_str,
      bool has_more_events);

  // Returns true if the dump file is valid.
  bool IsFileValid();

  // Writes a string to the dump file.
  void WriteString(const std::string& string);

  // Write a buffer to the dump file.
  void WriteChars(const char* chars, size_t size);

  // Closes the dump file.
  void CloseFile();

  // The number of blocks we have already written.
  int blocks_;
  // For dumping the content to disc.
  FILE* dump_file_;

  DISALLOW_COPY_AND_ASSIGN(BrowserShutdownProfileDumper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_SHUTDOWN_PROFILE_DUMPER_H_
