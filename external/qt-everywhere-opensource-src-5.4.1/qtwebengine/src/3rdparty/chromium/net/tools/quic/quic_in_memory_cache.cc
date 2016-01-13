// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_in_memory_cache.h"

#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "net/tools/balsa/balsa_headers.h"

using base::FilePath;
using base::StringPiece;
using std::string;

// Specifies the directory used during QuicInMemoryCache
// construction to seed the cache. Cache directory can be
// generated using `wget -p --save-headers <url>

namespace net {
namespace tools {

std::string FLAGS_quic_in_memory_cache_dir = "";

namespace {

// BalsaVisitor implementation (glue) which caches response bodies.
class CachingBalsaVisitor : public NoOpBalsaVisitor {
 public:
  CachingBalsaVisitor() : done_framing_(false) {}
  virtual void ProcessBodyData(const char* input, size_t size) OVERRIDE {
    AppendToBody(input, size);
  }
  virtual void MessageDone() OVERRIDE {
    done_framing_ = true;
  }
  virtual void HandleHeaderError(BalsaFrame* framer) OVERRIDE {
    UnhandledError();
  }
  virtual void HandleHeaderWarning(BalsaFrame* framer) OVERRIDE {
    UnhandledError();
  }
  virtual void HandleChunkingError(BalsaFrame* framer) OVERRIDE {
    UnhandledError();
  }
  virtual void HandleBodyError(BalsaFrame* framer) OVERRIDE {
    UnhandledError();
  }
  void UnhandledError() {
    LOG(DFATAL) << "Unhandled error framing HTTP.";
  }
  void AppendToBody(const char* input, size_t size) {
    body_.append(input, size);
  }
  bool done_framing() const { return done_framing_; }
  const string& body() const { return body_; }

 private:
  bool done_framing_;
  string body_;
};

}  // namespace

// static
QuicInMemoryCache* QuicInMemoryCache::GetInstance() {
  return Singleton<QuicInMemoryCache>::get();
}

const QuicInMemoryCache::Response* QuicInMemoryCache::GetResponse(
    const BalsaHeaders& request_headers) const {
  ResponseMap::const_iterator it = responses_.find(GetKey(request_headers));
  if (it == responses_.end()) {
    return NULL;
  }
  return it->second;
}

void QuicInMemoryCache::AddSimpleResponse(StringPiece method,
                                          StringPiece path,
                                          StringPiece version,
                                          StringPiece response_code,
                                          StringPiece response_detail,
                                          StringPiece body) {
  BalsaHeaders request_headers, response_headers;
  request_headers.SetRequestFirstlineFromStringPieces(method,
                                                      path,
                                                      version);
  response_headers.SetRequestFirstlineFromStringPieces(version,
                                                       response_code,
                                                       response_detail);
  response_headers.AppendHeader("content-length",
                                base::IntToString(body.length()));

  AddResponse(request_headers, response_headers, body);
}

void QuicInMemoryCache::AddResponse(const BalsaHeaders& request_headers,
                                    const BalsaHeaders& response_headers,
                                    StringPiece response_body) {
  VLOG(1) << "Adding response for: " << GetKey(request_headers);
  if (ContainsKey(responses_, GetKey(request_headers))) {
    LOG(DFATAL) << "Response for given request already exists!";
    return;
  }
  Response* new_response = new Response();
  new_response->set_headers(response_headers);
  new_response->set_body(response_body);
  responses_[GetKey(request_headers)] = new_response;
}

void QuicInMemoryCache::AddSpecialResponse(StringPiece method,
                                           StringPiece path,
                                           StringPiece version,
                                           SpecialResponseType response_type) {
  BalsaHeaders request_headers, response_headers;
  request_headers.SetRequestFirstlineFromStringPieces(method,
                                                      path,
                                                      version);
  AddResponse(request_headers, response_headers, "");
  responses_[GetKey(request_headers)]->response_type_ = response_type;
}

QuicInMemoryCache::QuicInMemoryCache() {
  Initialize();
}

void QuicInMemoryCache::ResetForTests() {
  STLDeleteValues(&responses_);
  Initialize();
}

void QuicInMemoryCache::Initialize() {
  // If there's no defined cache dir, we have no initialization to do.
  if (FLAGS_quic_in_memory_cache_dir.empty()) {
    VLOG(1) << "No cache directory found. Skipping initialization.";
    return;
  }
  VLOG(1) << "Attempting to initialize QuicInMemoryCache from directory: "
          << FLAGS_quic_in_memory_cache_dir;

  FilePath directory(FLAGS_quic_in_memory_cache_dir);
  base::FileEnumerator file_list(directory,
                                 true,
                                 base::FileEnumerator::FILES);

  FilePath file = file_list.Next();
  while (!file.empty()) {
    // Need to skip files in .svn directories
    if (file.value().find("/.svn/") != std::string::npos) {
      file = file_list.Next();
      continue;
    }

    BalsaHeaders request_headers, response_headers;

    string file_contents;
    base::ReadFileToString(file, &file_contents);

    // Frame HTTP.
    CachingBalsaVisitor caching_visitor;
    BalsaFrame framer;
    framer.set_balsa_headers(&response_headers);
    framer.set_balsa_visitor(&caching_visitor);
    size_t processed = 0;
    while (processed < file_contents.length() &&
           !caching_visitor.done_framing()) {
      processed += framer.ProcessInput(file_contents.c_str() + processed,
                                       file_contents.length() - processed);
    }

    string response_headers_str;
    response_headers.DumpToString(&response_headers_str);
    if (!caching_visitor.done_framing()) {
      LOG(DFATAL) << "Did not frame entire message from file: " << file.value()
                  << " (" << processed << " of " << file_contents.length()
                  << " bytes).";
    }
    if (processed < file_contents.length()) {
      // Didn't frame whole file. Assume remainder is body.
      // This sometimes happens as a result of incompatibilities between
      // BalsaFramer and wget's serialization of HTTP sans content-length.
      caching_visitor.AppendToBody(file_contents.c_str() + processed,
                                   file_contents.length() - processed);
      processed += file_contents.length();
    }

    StringPiece base = file.value();
    if (response_headers.HasHeader("X-Original-Url")) {
      base = response_headers.GetHeader("X-Original-Url");
      response_headers.RemoveAllOfHeader("X-Original-Url");
      // Remove the protocol so that the string is of the form host + path,
      // which is parsed properly below.
      if (StringPieceUtils::StartsWithIgnoreCase(base, "https://")) {
        base.remove_prefix(8);
      } else if (StringPieceUtils::StartsWithIgnoreCase(base, "http://")) {
        base.remove_prefix(7);
      }
    }
    int path_start = base.find_first_of('/');
    DCHECK_LT(0, path_start);
    StringPiece host(base.substr(0, path_start));
    StringPiece path(base.substr(path_start));
    if (path[path.length() - 1] == ',') {
      path.remove_suffix(1);
    }
    // Set up request headers. Assume method is GET and protocol is HTTP/1.1.
    request_headers.SetRequestFirstlineFromStringPieces("GET",
                                                        path,
                                                        "HTTP/1.1");
    request_headers.ReplaceOrAppendHeader("host", host);

    VLOG(1) << "Inserting 'http://" << GetKey(request_headers)
            << "' into QuicInMemoryCache.";

    AddResponse(request_headers, response_headers, caching_visitor.body());

    file = file_list.Next();
  }
}

QuicInMemoryCache::~QuicInMemoryCache() {
  STLDeleteValues(&responses_);
}

string QuicInMemoryCache::GetKey(const BalsaHeaders& request_headers) const {
  StringPiece uri = request_headers.request_uri();
  if (uri.size() == 0) {
    return "";
  }
  StringPiece host;
  if (uri[0] == '/') {
    host = request_headers.GetHeader("host");
  } else if (StringPieceUtils::StartsWithIgnoreCase(uri, "https://")) {
    uri.remove_prefix(8);
  } else if (StringPieceUtils::StartsWithIgnoreCase(uri, "http://")) {
    uri.remove_prefix(7);
  }
  return host.as_string() + uri.as_string();
}

}  // namespace tools
}  // namespace net
