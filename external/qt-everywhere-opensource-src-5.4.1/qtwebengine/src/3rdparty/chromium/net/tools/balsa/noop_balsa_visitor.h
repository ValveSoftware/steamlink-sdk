// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Provides empty BalsaVisitorInterface overrides for convenience.
// Intended to be used as a base class for BalsaVisitorInterface subclasses that
// only need to override a small number of methods.

#ifndef NET_TOOLS_BALSA_NOOP_BALSA_VISITOR_H_
#define NET_TOOLS_BALSA_NOOP_BALSA_VISITOR_H_

#include "net/tools/balsa/balsa_visitor_interface.h"

namespace net {

// See file comment above.
class NoOpBalsaVisitor : public BalsaVisitorInterface {
 public:
  NoOpBalsaVisitor() { }
  virtual ~NoOpBalsaVisitor() { }

  virtual void ProcessBodyInput(const char* input, size_t size) OVERRIDE { }
  virtual void ProcessBodyData(const char* input, size_t size) OVERRIDE { }
  virtual void ProcessHeaderInput(const char* input, size_t size) OVERRIDE { }
  virtual void ProcessTrailerInput(const char* input, size_t size) OVERRIDE { }
  virtual void ProcessHeaders(const BalsaHeaders& headers) OVERRIDE { }

  virtual void ProcessRequestFirstLine(const char* line_input,
                                       size_t line_length,
                                       const char* method_input,
                                       size_t method_length,
                                       const char* request_uri_input,
                                       size_t request_uri_length,
                                       const char* version_input,
                                       size_t version_length) OVERRIDE { }
  virtual void ProcessResponseFirstLine(const char* line_input,
                                        size_t line_length,
                                        const char* version_input,
                                        size_t version_length,
                                        const char* status_input,
                                        size_t status_length,
                                        const char* reason_input,
                                        size_t reason_length) OVERRIDE { }
  virtual void ProcessChunkLength(size_t chunk_length) OVERRIDE { }
  virtual void ProcessChunkExtensions(const char* input, size_t size) OVERRIDE {
  }
  virtual void HeaderDone() OVERRIDE { }
  virtual void MessageDone() OVERRIDE { }
  virtual void HandleHeaderError(BalsaFrame* framer) OVERRIDE { }
  virtual void HandleHeaderWarning(BalsaFrame* framer) OVERRIDE { }
  virtual void HandleChunkingError(BalsaFrame* framer) OVERRIDE { }
  virtual void HandleBodyError(BalsaFrame* framer) OVERRIDE { }

 private:
  DISALLOW_COPY_AND_ASSIGN(NoOpBalsaVisitor);
};

}  // namespace net

#endif  // NET_TOOLS_BALSA_NOOP_BALSA_VISITOR_H_
