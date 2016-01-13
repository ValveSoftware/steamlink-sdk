// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printed_document.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/i18n/file_util_icu.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "printing/page_number.h"
#include "printing/print_settings_conversion.h"
#include "printing/printed_page.h"
#include "printing/printed_pages_source.h"
#include "printing/units.h"
#include "skia/ext/platform_device.h"
#include "ui/gfx/font.h"
#include "ui/gfx/text_elider.h"

namespace printing {

namespace {

base::LazyInstance<base::FilePath> g_debug_dump_info =
    LAZY_INSTANCE_INITIALIZER;

void DebugDumpPageTask(const base::string16& doc_name,
                       const PrintedPage* page) {
  if (g_debug_dump_info.Get().empty())
    return;

  base::string16 filename = doc_name;
  filename +=
      base::ASCIIToUTF16(base::StringPrintf("_%04d", page->page_number()));
#if defined(OS_WIN)
  page->metafile()->SaveTo(PrintedDocument::CreateDebugDumpPath(
      filename, FILE_PATH_LITERAL(".emf")));
#else   // OS_WIN
  page->metafile()->SaveTo(PrintedDocument::CreateDebugDumpPath(
      filename, FILE_PATH_LITERAL(".pdf")));
#endif  // OS_WIN
}

void DebugDumpDataTask(const base::string16& doc_name,
                       const base::FilePath::StringType& extension,
                       const base::RefCountedMemory* data) {
  base::FilePath path =
      PrintedDocument::CreateDebugDumpPath(doc_name, extension);
  if (path.empty())
    return;
  base::WriteFile(path,
                  reinterpret_cast<const char*>(data->front()),
                  base::checked_cast<int>(data->size()));
}

void DebugDumpSettings(const base::string16& doc_name,
                       const PrintSettings& settings,
                       base::TaskRunner* blocking_runner) {
  base::DictionaryValue job_settings;
  PrintSettingsToJobSettingsDebug(settings, &job_settings);
  std::string settings_str;
  base::JSONWriter::WriteWithOptions(
      &job_settings, base::JSONWriter::OPTIONS_PRETTY_PRINT, &settings_str);
  scoped_refptr<base::RefCountedMemory> data =
      base::RefCountedString::TakeString(&settings_str);
  blocking_runner->PostTask(
      FROM_HERE,
      base::Bind(
          &DebugDumpDataTask, doc_name, FILE_PATH_LITERAL(".json"), data));
}

}  // namespace

PrintedDocument::PrintedDocument(const PrintSettings& settings,
                                 PrintedPagesSource* source,
                                 int cookie,
                                 base::TaskRunner* blocking_runner)
    : mutable_(source), immutable_(settings, source, cookie, blocking_runner) {
  // Records the expected page count if a range is setup.
  if (!settings.ranges().empty()) {
    // If there is a range, set the number of page
    for (unsigned i = 0; i < settings.ranges().size(); ++i) {
      const PageRange& range = settings.ranges()[i];
      mutable_.expected_page_count_ += range.to - range.from + 1;
    }
  }

  if (!g_debug_dump_info.Get().empty())
    DebugDumpSettings(name(), settings, blocking_runner);
}

PrintedDocument::~PrintedDocument() {
}

void PrintedDocument::SetPage(int page_number,
                              Metafile* metafile,
#if defined(OS_WIN)
                              double shrink,
#endif  // OS_WIN
                              const gfx::Size& paper_size,
                              const gfx::Rect& page_rect) {
  // Notice the page_number + 1, the reason is that this is the value that will
  // be shown. Users dislike 0-based counting.
  scoped_refptr<PrintedPage> page(
      new PrintedPage(page_number + 1, metafile, paper_size, page_rect));
#if defined(OS_WIN)
  page->set_shrink_factor(shrink);
#endif  // OS_WIN
  {
    base::AutoLock lock(lock_);
    mutable_.pages_[page_number] = page;

#if defined(OS_POSIX) && !defined(OS_MACOSX)
    if (page_number < mutable_.first_page)
      mutable_.first_page = page_number;
#endif
  }

  if (!g_debug_dump_info.Get().empty()) {
    immutable_.blocking_runner_->PostTask(
        FROM_HERE, base::Bind(&DebugDumpPageTask, name(), page));
  }
}

scoped_refptr<PrintedPage> PrintedDocument::GetPage(int page_number) {
  scoped_refptr<PrintedPage> page;
  {
    base::AutoLock lock(lock_);
    PrintedPages::const_iterator itr = mutable_.pages_.find(page_number);
    if (itr != mutable_.pages_.end())
      page = itr->second;
  }
  return page;
}

bool PrintedDocument::IsComplete() const {
  base::AutoLock lock(lock_);
  if (!mutable_.page_count_)
    return false;
  PageNumber page(immutable_.settings_, mutable_.page_count_);
  if (page == PageNumber::npos())
    return false;

  for (; page != PageNumber::npos(); ++page) {
#if defined(OS_WIN) || defined(OS_MACOSX)
    const bool metafile_must_be_valid = true;
#elif defined(OS_POSIX)
    const bool metafile_must_be_valid = (page.ToInt() == mutable_.first_page);
#endif
    PrintedPages::const_iterator itr = mutable_.pages_.find(page.ToInt());
    if (itr == mutable_.pages_.end() || !itr->second.get())
      return false;
    if (metafile_must_be_valid && !itr->second->metafile())
      return false;
  }
  return true;
}

void PrintedDocument::DisconnectSource() {
  base::AutoLock lock(lock_);
  mutable_.source_ = NULL;
}

uint32 PrintedDocument::MemoryUsage() const {
  std::vector< scoped_refptr<PrintedPage> > pages_copy;
  {
    base::AutoLock lock(lock_);
    pages_copy.reserve(mutable_.pages_.size());
    PrintedPages::const_iterator end = mutable_.pages_.end();
    for (PrintedPages::const_iterator itr = mutable_.pages_.begin();
         itr != end; ++itr) {
      if (itr->second.get()) {
        pages_copy.push_back(itr->second);
      }
    }
  }
  uint32 total = 0;
  for (size_t i = 0; i < pages_copy.size(); ++i) {
    total += pages_copy[i]->metafile()->GetDataSize();
  }
  return total;
}

void PrintedDocument::set_page_count(int max_page) {
  base::AutoLock lock(lock_);
  DCHECK_EQ(0, mutable_.page_count_);
  mutable_.page_count_ = max_page;
  if (immutable_.settings_.ranges().empty()) {
    mutable_.expected_page_count_ = max_page;
  } else {
    // If there is a range, don't bother since expected_page_count_ is already
    // initialized.
    DCHECK_NE(mutable_.expected_page_count_, 0);
  }
}

int PrintedDocument::page_count() const {
  base::AutoLock lock(lock_);
  return mutable_.page_count_;
}

int PrintedDocument::expected_page_count() const {
  base::AutoLock lock(lock_);
  return mutable_.expected_page_count_;
}

void PrintedDocument::set_debug_dump_path(
    const base::FilePath& debug_dump_path) {
  g_debug_dump_info.Get() = debug_dump_path;
}

base::FilePath PrintedDocument::CreateDebugDumpPath(
    const base::string16& document_name,
    const base::FilePath::StringType& extension) {
  if (g_debug_dump_info.Get().empty())
    return base::FilePath();
  // Create a filename.
  base::string16 filename;
  base::Time now(base::Time::Now());
  filename = base::TimeFormatShortDateAndTime(now);
  filename += base::ASCIIToUTF16("_");
  filename += document_name;
  base::FilePath::StringType system_filename;
#if defined(OS_WIN)
  system_filename = filename;
#else   // OS_WIN
  system_filename = base::UTF16ToUTF8(filename);
#endif  // OS_WIN
  file_util::ReplaceIllegalCharactersInPath(&system_filename, '_');
  return g_debug_dump_info.Get().Append(system_filename).AddExtension(
      extension);
}

void PrintedDocument::DebugDumpData(
    const base::RefCountedMemory* data,
    const base::FilePath::StringType& extension) {
  if (g_debug_dump_info.Get().empty())
    return;
  immutable_.blocking_runner_->PostTask(
      FROM_HERE, base::Bind(&DebugDumpDataTask, name(), extension, data));
}

PrintedDocument::Mutable::Mutable(PrintedPagesSource* source)
    : source_(source),
      expected_page_count_(0),
      page_count_(0) {
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  first_page = INT_MAX;
#endif
}

PrintedDocument::Mutable::~Mutable() {
}

PrintedDocument::Immutable::Immutable(const PrintSettings& settings,
                                      PrintedPagesSource* source,
                                      int cookie,
                                      base::TaskRunner* blocking_runner)
    : settings_(settings),
      name_(source->RenderSourceName()),
      cookie_(cookie),
      blocking_runner_(blocking_runner) {
}

PrintedDocument::Immutable::~Immutable() {
}

#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
// This function is not used on aura linux/chromeos or android.
void PrintedDocument::RenderPrintedPage(const PrintedPage& page,
                                        PrintingContext* context) const {
}
#endif

}  // namespace printing
