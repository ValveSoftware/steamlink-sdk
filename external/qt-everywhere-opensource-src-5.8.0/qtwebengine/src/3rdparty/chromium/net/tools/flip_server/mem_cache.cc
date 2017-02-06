// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/flip_server/mem_cache.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <deque>
#include <map>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "net/tools/balsa/balsa_frame.h"
#include "net/tools/balsa/balsa_headers.h"
#include "net/tools/flip_server/url_to_filename_encoder.h"
#include "net/tools/flip_server/url_utilities.h"

namespace {
// The directory where cache locates);
const char FLAGS_cache_base_dir[] = ".";
}  // namespace

namespace net {

void StoreBodyAndHeadersVisitor::ProcessBodyData(const char* input,
                                                 size_t size) {
  body.append(input, size);
}

void StoreBodyAndHeadersVisitor::HandleHeaderError(BalsaFrame* framer) {
  HandleError();
}

void StoreBodyAndHeadersVisitor::HandleHeaderWarning(BalsaFrame* framer) {
  HandleError();
}

void StoreBodyAndHeadersVisitor::HandleChunkingError(BalsaFrame* framer) {
  HandleError();
}

void StoreBodyAndHeadersVisitor::HandleBodyError(BalsaFrame* framer) {
  HandleError();
}

FileData::FileData(const BalsaHeaders* headers,
                   const std::string& filename,
                   const std::string& body)
    : filename_(filename), body_(body) {
  if (headers) {
    headers_.reset(new BalsaHeaders);
    headers_->CopyFrom(*headers);
  }
}

FileData::FileData() {}

FileData::~FileData() {}

MemoryCache::MemoryCache() : cwd_(FLAGS_cache_base_dir) {}
MemoryCache::~MemoryCache() {}

void MemoryCache::AddFiles() {
  std::deque<std::string> paths;
  paths.push_back(cwd_ + "/GET_");
  DIR* current_dir = NULL;
  while (!paths.empty()) {
    while (current_dir == NULL && !paths.empty()) {
      std::string current_dir_name = paths.front();
      VLOG(1) << "Attempting to open dir: \"" << current_dir_name << "\"";
      current_dir = opendir(current_dir_name.c_str());
      paths.pop_front();

      if (current_dir == NULL) {
        perror("Unable to open directory. ");
        current_dir_name.clear();
        continue;
      }

      if (current_dir) {
        VLOG(1) << "Succeeded opening";
        for (struct dirent* dir_data = readdir(current_dir); dir_data != NULL;
             dir_data = readdir(current_dir)) {
          std::string current_entry_name =
              current_dir_name + "/" + dir_data->d_name;
          if (dir_data->d_type == DT_REG) {
            VLOG(1) << "Found file: " << current_entry_name;
            ReadAndStoreFileContents(current_entry_name.c_str());
          } else if (dir_data->d_type == DT_DIR) {
            VLOG(1) << "Found subdir: " << current_entry_name;
            if (std::string(dir_data->d_name) != "." &&
                std::string(dir_data->d_name) != "..") {
              VLOG(1) << "Adding to search path: " << current_entry_name;
              paths.push_front(current_entry_name);
            }
          }
        }
        VLOG(1) << "Oops, no data left. Closing dir.";
        closedir(current_dir);
        current_dir = NULL;
      }
    }
  }
}

void MemoryCache::ReadToString(const char* filename, std::string* output) {
  output->clear();
  int fd = open(filename, 0, "r");
  if (fd == -1)
    return;
  char buffer[4096];
  ssize_t read_status = read(fd, buffer, sizeof(buffer));
  while (read_status > 0) {
    output->append(buffer, static_cast<size_t>(read_status));
    do {
      read_status = read(fd, buffer, sizeof(buffer));
    } while (read_status <= 0 && errno == EINTR);
  }
  close(fd);
}

void MemoryCache::ReadAndStoreFileContents(const char* filename) {
  StoreBodyAndHeadersVisitor visitor;
  BalsaFrame framer;
  framer.set_balsa_visitor(&visitor);
  framer.set_balsa_headers(&(visitor.headers));
  std::string filename_contents;
  ReadToString(filename, &filename_contents);

  // Ugly hack to make everything look like 1.1.
  if (filename_contents.find("HTTP/1.0") == 0)
    filename_contents[7] = '1';

  size_t pos = 0;
  size_t old_pos = 0;
  while (true) {
    old_pos = pos;
    pos += framer.ProcessInput(filename_contents.data() + pos,
                               filename_contents.size() - pos);
    if (framer.Error() || pos == old_pos) {
      LOG(ERROR) << "Unable to make forward progress, or error"
                    " framing file: " << filename;
      if (framer.Error()) {
        LOG(INFO) << "********************************************ERROR!";
        return;
      }
      return;
    }
    if (framer.MessageFullyRead()) {
      // If no Content-Length or Transfer-Encoding was captured in the
      // file, then the rest of the data is the body.  Many of the captures
      // from within Chrome don't have content-lengths.
      if (!visitor.body.length())
        visitor.body = filename_contents.substr(pos);
      break;
    }
  }
  visitor.headers.RemoveAllOfHeader("content-length");
  visitor.headers.RemoveAllOfHeader("transfer-encoding");
  visitor.headers.RemoveAllOfHeader("connection");
  visitor.headers.AppendHeader("transfer-encoding", "chunked");
  visitor.headers.AppendHeader("connection", "keep-alive");

// Experiment with changing headers for forcing use of cached
// versions of content.
// TODO(mbelshe) REMOVE ME
#if 0
  // TODO(mbelshe) append current date.
  visitor.headers.RemoveAllOfHeader("date");
  if (visitor.headers.HasHeader("expires")) {
    visitor.headers.RemoveAllOfHeader("expires");
    visitor.headers.AppendHeader("expires",
                               "Fri, 30 Aug, 2019 12:00:00 GMT");
  }
#endif
  DCHECK_GE(std::string(filename).size(), cwd_.size() + 1);
  DCHECK_EQ(std::string(filename).substr(0, cwd_.size()), cwd_);
  DCHECK_EQ(filename[cwd_.size()], '/');
  std::string filename_stripped = std::string(filename).substr(cwd_.size() + 1);
  LOG(INFO) << "Adding file (" << visitor.body.length()
            << " bytes): " << filename_stripped;
  size_t slash_pos = filename_stripped.find('/');
  if (slash_pos == std::string::npos) {
    slash_pos = filename_stripped.size();
  }
  InsertFile(
      &visitor.headers, filename_stripped.substr(0, slash_pos), visitor.body);
}

FileData* MemoryCache::GetFileData(const std::string& filename) {
  Files::iterator fi = files_.end();
  if (base::EndsWith(filename, ".html", base::CompareCase::SENSITIVE)) {
    fi = files_.find(filename.substr(0, filename.size() - 5) + ".http");
  }
  if (fi == files_.end())
    fi = files_.find(filename);

  if (fi == files_.end()) {
    return NULL;
  }
  return fi->second.get();
}

bool MemoryCache::AssignFileData(const std::string& filename,
                                 MemCacheIter* mci) {
  mci->file_data = GetFileData(filename);
  if (mci->file_data == NULL) {
    LOG(ERROR) << "Could not find file data for " << filename;
    return false;
  }
  return true;
}

void MemoryCache::InsertFile(const BalsaHeaders* headers,
                             const std::string& filename,
                             const std::string& body) {
  InsertFile(new FileData(headers, filename, body));
}

void MemoryCache::InsertFile(FileData* file_data) {
  Files::iterator it = files_.find(file_data->filename());
  if (it != files_.end()) {
    it->second.reset(file_data);
  } else {
    files_.insert(
        std::make_pair(file_data->filename(), base::WrapUnique(file_data)));
  }
}

}  // namespace net
