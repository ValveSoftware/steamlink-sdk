// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_shutdown_profile_dumper.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/debug/trace_event_impl.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/common/content_switches.h"

namespace content {

BrowserShutdownProfileDumper::BrowserShutdownProfileDumper()
    : blocks_(0),
      dump_file_(NULL) {
}

BrowserShutdownProfileDumper::~BrowserShutdownProfileDumper() {
  WriteTracesToDisc(GetFileName());
}

void BrowserShutdownProfileDumper::WriteTracesToDisc(
    const base::FilePath& file_name) {
  // Note: I have seen a usage of 0.000xx% when dumping - which fits easily.
  // Since the tracer stops when the trace buffer is filled, we'd rather save
  // what we have than nothing since we might see from the amount of events
  // that caused the problem.
  DVLOG(1) << "Flushing shutdown traces to disc. The buffer is %" <<
      base::debug::TraceLog::GetInstance()->GetBufferPercentFull() <<
      " full.";
  DCHECK(!dump_file_);
  dump_file_ = base::OpenFile(file_name, "w+");
  if (!IsFileValid()) {
    LOG(ERROR) << "Failed to open performance trace file: " <<
        file_name.value();
    return;
  }
  WriteString("{\"traceEvents\":");
  WriteString("[");

  // TraceLog::Flush() requires the calling thread to have a message loop.
  // As the message loop of the current thread may have quit, start another
  // thread for flushing the trace.
  base::WaitableEvent flush_complete_event(false, false);
  base::Thread flush_thread("browser_shutdown_trace_event_flush");
  flush_thread.Start();
  flush_thread.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&BrowserShutdownProfileDumper::EndTraceAndFlush,
                 base::Unretained(this),
                 base::Unretained(&flush_complete_event)));

  bool original_wait_allowed = base::ThreadRestrictions::SetWaitAllowed(true);
  flush_complete_event.Wait();
  base::ThreadRestrictions::SetWaitAllowed(original_wait_allowed);
}

void BrowserShutdownProfileDumper::EndTraceAndFlush(
    base::WaitableEvent* flush_complete_event) {
  base::debug::TraceLog::GetInstance()->SetDisabled();
  base::debug::TraceLog::GetInstance()->Flush(
      base::Bind(&BrowserShutdownProfileDumper::WriteTraceDataCollected,
                 base::Unretained(this),
                 base::Unretained(flush_complete_event)));
}

base::FilePath BrowserShutdownProfileDumper::GetFileName() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  base::FilePath trace_file =
      command_line.GetSwitchValuePath(switches::kTraceShutdownFile);

  if (!trace_file.empty())
    return trace_file;

  // Default to saving the startup trace into the current dir.
  return base::FilePath().AppendASCII("chrometrace.log");
}

void BrowserShutdownProfileDumper::WriteTraceDataCollected(
    base::WaitableEvent* flush_complete_event,
    const scoped_refptr<base::RefCountedString>& events_str,
    bool has_more_events) {
  if (!IsFileValid()) {
    flush_complete_event->Signal();
    return;
  }
  if (blocks_) {
    // Blocks are not comma separated. Beginning with the second block we
    // start therefore to add one in front of the previous block.
    WriteString(",");
  }
  ++blocks_;
  WriteString(events_str->data());

  if (!has_more_events) {
    WriteString("]");
    WriteString("}");
    CloseFile();
    flush_complete_event->Signal();
  }
}

bool BrowserShutdownProfileDumper::IsFileValid() {
  return dump_file_ && (ferror(dump_file_) == 0);
}

void BrowserShutdownProfileDumper::WriteString(const std::string& string) {
  WriteChars(string.data(), string.size());
}

void BrowserShutdownProfileDumper::WriteChars(const char* chars, size_t size) {
  if (!IsFileValid())
    return;

  size_t written = fwrite(chars, 1, size, dump_file_);
  if (written != size) {
    LOG(ERROR) << "Error " << ferror(dump_file_) <<
        " in fwrite() to trace file";
    CloseFile();
  }
}

void BrowserShutdownProfileDumper::CloseFile() {
  if (!dump_file_)
    return;
  base::CloseFile(dump_file_);
  dump_file_ = NULL;
}

}  // namespace content
