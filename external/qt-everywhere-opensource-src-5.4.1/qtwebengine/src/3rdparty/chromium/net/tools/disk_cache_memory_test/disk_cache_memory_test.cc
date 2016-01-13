// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "net/base/cache_type.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/simple/simple_backend_impl.h"
#include "net/disk_cache/simple/simple_index.h"

namespace disk_cache {
namespace {

const char kBlockFileBackendType[] = "block_file";
const char kSimpleBackendType[] = "simple";

const char kDiskCacheType[] = "disk_cache";
const char kAppCacheType[] = "app_cache";

const char kPrivateDirty[] = "Private_Dirty:";
const char kReadWrite[] = "rw-";
const char kHeap[] = "[heap]";
const char kKb[] = "kB";

struct CacheSpec {
 public:
  static scoped_ptr<CacheSpec> Parse(const std::string& spec_string) {
    std::vector<std::string> tokens;
    base::SplitString(spec_string, ':', &tokens);
    if (tokens.size() != 3)
      return scoped_ptr<CacheSpec>();
    if (tokens[0] != kBlockFileBackendType && tokens[0] != kSimpleBackendType)
      return scoped_ptr<CacheSpec>();
    if (tokens[1] != kDiskCacheType && tokens[1] != kAppCacheType)
      return scoped_ptr<CacheSpec>();
    return scoped_ptr<CacheSpec>(new CacheSpec(
        tokens[0] == kBlockFileBackendType ? net::CACHE_BACKEND_BLOCKFILE
                                           : net::CACHE_BACKEND_SIMPLE,
        tokens[1] == kDiskCacheType ? net::DISK_CACHE : net::APP_CACHE,
        base::FilePath(tokens[2])));
  }

  const net::BackendType backend_type;
  const net::CacheType cache_type;
  const base::FilePath path;

 private:
  CacheSpec(net::BackendType backend_type,
            net::CacheType cache_type,
            const base::FilePath& path)
      : backend_type(backend_type),
        cache_type(cache_type),
        path(path) {
  }
};

void SetSuccessCodeOnCompletion(base::RunLoop* run_loop,
                                bool* succeeded,
                                int net_error) {
  if (net_error == net::OK) {
    *succeeded = true;
  } else {
    *succeeded = false;
  }
  run_loop->Quit();
}

scoped_ptr<Backend> CreateAndInitBackend(const CacheSpec& spec) {
  scoped_ptr<Backend> result;
  scoped_ptr<Backend> backend;
  bool succeeded = false;
  base::RunLoop run_loop;
  const net::CompletionCallback callback = base::Bind(
      &SetSuccessCodeOnCompletion,
      base::Unretained(&run_loop),
      base::Unretained(&succeeded));
  const int net_error = CreateCacheBackend(
      spec.cache_type, spec.backend_type, spec.path, 0, false,
      base::MessageLoopProxy::current(), NULL, &backend, callback);
  if (net_error == net::OK)
    callback.Run(net::OK);
  else
    run_loop.Run();
  if (!succeeded) {
    LOG(ERROR) << "Could not initialize backend in "
               << spec.path.LossyDisplayName();
    return result.Pass();
  }
  // For the simple cache, the index may not be initialized yet.
  if (spec.backend_type == net::CACHE_BACKEND_SIMPLE) {
    base::RunLoop index_run_loop;
    const net::CompletionCallback index_callback = base::Bind(
        &SetSuccessCodeOnCompletion,
        base::Unretained(&index_run_loop),
        base::Unretained(&succeeded));
    SimpleBackendImpl* simple_backend =
        static_cast<SimpleBackendImpl*>(backend.get());
    const int index_net_error =
        simple_backend->index()->ExecuteWhenReady(index_callback);
    if (index_net_error == net::OK)
      index_callback.Run(net::OK);
    else
      index_run_loop.Run();
    if (!succeeded) {
      LOG(ERROR) << "Could not initialize Simple Cache in "
                 << spec.path.LossyDisplayName();
      return result.Pass();
    }
  }
  DCHECK(backend);
  result.swap(backend);
  return result.Pass();
}

// Parses range lines from /proc/<PID>/smaps, e.g. (anonymous read write):
// 7f819d88b000-7f819d890000 rw-p 00000000 00:00 0
bool ParseRangeLine(const std::string& line,
                    std::vector<std::string>* tokens,
                    bool* is_anonymous_read_write) {
  tokens->clear();
  base::SplitStringAlongWhitespace(line, tokens);
  if (tokens->size() == 5) {
    const std::string& mode = (*tokens)[1];
    *is_anonymous_read_write = !mode.compare(0, 3, kReadWrite);
    return true;
  }
  // On Android, most of the memory is allocated in the heap, instead of being
  // mapped.
  if (tokens->size() == 6) {
    const std::string& type = (*tokens)[5];
    *is_anonymous_read_write = (type == kHeap);
    return true;
  }
  return false;
}

// Parses range property lines from /proc/<PID>/smaps, e.g.:
// Private_Dirty:        16 kB
//
// Returns |false| iff it recognizes a new range line. Outputs non-zero |size|
// only if parsing succeeded.
bool ParseRangeProperty(const std::string& line,
                        std::vector<std::string>* tokens,
                        uint64* size,
                        bool* is_private_dirty) {
  tokens->clear();
  base::SplitStringAlongWhitespace(line, tokens);

  // If the line is long, attempt to parse new range outside of this scope.
  if (tokens->size() > 3)
    return false;

  // Skip the line on other parsing error occasions.
  if (tokens->size() < 3)
    return true;
  const std::string& type = (*tokens)[0];
  if (type != kPrivateDirty)
    return true;
  const std::string& unit = (*tokens)[2];
  if (unit != kKb) {
    LOG(WARNING) << "Discarding value not in kB: " << line;
    return true;
  }
  const std::string& size_str = (*tokens)[1];
  uint64 map_size = 0;
  if (!base::StringToUint64(size_str, &map_size))
    return true;
  *is_private_dirty = true;
  *size = map_size;
  return true;
}

uint64 GetMemoryConsumption() {
  std::ifstream maps_file(
      base::StringPrintf("/proc/%d/smaps", getpid()).c_str());
  if (!maps_file.good()) {
    LOG(ERROR) << "Could not open smaps file.";
    return false;
  }
  std::string line;
  std::vector<std::string> tokens;
  uint64 total_size = 0;
  if (!std::getline(maps_file, line) || line.empty())
    return total_size;
  while (true) {
    bool is_anonymous_read_write = false;
    if (!ParseRangeLine(line, &tokens, &is_anonymous_read_write)) {
      LOG(WARNING) << "Parsing smaps - did not expect line: " << line;
    }
    if (!std::getline(maps_file, line) || line.empty())
      return total_size;
    bool is_private_dirty = false;
    uint64 size = 0;
    while (ParseRangeProperty(line, &tokens, &size, &is_private_dirty)) {
      if (is_anonymous_read_write && is_private_dirty) {
        total_size += size;
        is_private_dirty = false;
      }
      if (!std::getline(maps_file, line) || line.empty())
        return total_size;
    }
  }
  return total_size;
}

bool CacheMemTest(const ScopedVector<CacheSpec>& specs) {
  ScopedVector<Backend> backends;
  ScopedVector<CacheSpec>::const_iterator it;
  for (it = specs.begin(); it != specs.end(); ++it) {
    scoped_ptr<Backend> backend = CreateAndInitBackend(**it);
    if (!backend)
      return false;
    std::cout << "Number of entries in " << (*it)->path.LossyDisplayName()
              << " : " << backend->GetEntryCount() << std::endl;
    backends.push_back(backend.release());
  }
  const uint64 memory_consumption = GetMemoryConsumption();
  std::cout << "Private dirty memory: " << memory_consumption << " kB"
            << std::endl;
  return true;
}

void PrintUsage(std::ostream* stream) {
  *stream << "Usage: disk_cache_mem_test "
          << "--spec-1=<spec> "
          << "[--spec-2=<spec>]"
          << std::endl
          << "  with <cache_spec>=<backend_type>:<cache_type>:<cache_path>"
          << std::endl
          << "       <backend_type>='block_file'|'simple'" << std::endl
          << "       <cache_type>='disk_cache'|'app_cache'" << std::endl
          << "       <cache_path>=file system path" << std::endl;
}

bool ParseAndStoreSpec(const std::string& spec_str,
                       ScopedVector<CacheSpec>* specs) {
  scoped_ptr<CacheSpec> spec = CacheSpec::Parse(spec_str);
  if (!spec) {
    PrintUsage(&std::cerr);
    return false;
  }
  specs->push_back(spec.release());
  return true;
}

bool Main(int argc, char** argv) {
  base::AtExitManager at_exit_manager;
  base::MessageLoopForIO message_loop;
  base::CommandLine::Init(argc, argv);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch("help")) {
    PrintUsage(&std::cout);
    return true;
  }
  if ((command_line.GetSwitches().size() != 1 &&
       command_line.GetSwitches().size() != 2) ||
      !command_line.HasSwitch("spec-1") ||
      (command_line.GetSwitches().size() == 2 &&
       !command_line.HasSwitch("spec-2"))) {
    PrintUsage(&std::cerr);
    return false;
  }
  ScopedVector<CacheSpec> specs;
  const std::string spec_str_1 = command_line.GetSwitchValueASCII("spec-1");
  if (!ParseAndStoreSpec(spec_str_1, &specs))
    return false;
  if (command_line.HasSwitch("spec-2")) {
    const std::string spec_str_2 = command_line.GetSwitchValueASCII("spec-2");
    if (!ParseAndStoreSpec(spec_str_2, &specs))
      return false;
  }
  return CacheMemTest(specs);
}

}  // namespace
}  // namespace disk_cache

int main(int argc, char** argv) {
  return !disk_cache::Main(argc, argv);
}
