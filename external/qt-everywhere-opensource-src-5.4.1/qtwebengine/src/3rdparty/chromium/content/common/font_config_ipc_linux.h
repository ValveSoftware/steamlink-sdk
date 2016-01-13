// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FONT_CONFIG_IPC_LINUX_H_
#define CONTENT_COMMON_FONT_CONFIG_IPC_LINUX_H_

#include "base/compiler_specific.h"
#include "third_party/skia/include/ports/SkFontConfigInterface.h"

#include <string>

namespace content {

// FontConfig implementation for Skia that proxies out of process to get out
// of the sandbox. See http://code.google.com/p/chromium/wiki/LinuxSandboxIPC
class FontConfigIPC : public SkFontConfigInterface {
 public:
  explicit FontConfigIPC(int fd);
  virtual ~FontConfigIPC();

  virtual bool matchFamilyName(const char familyName[],
                               SkTypeface::Style requested,
                               FontIdentity* outFontIdentifier,
                               SkString* outFamilyName,
                               SkTypeface::Style* outStyle) OVERRIDE;

  virtual SkStream* openStream(const FontIdentity&) OVERRIDE;

  enum Method {
    METHOD_MATCH = 0,
    METHOD_OPEN = 1,
  };

  enum {
    kMaxFontFamilyLength = 2048
  };

 private:
  const int fd_;
};

}  // namespace content

#endif  // CONTENT_COMMON_FONT_CONFIG_IPC_LINUX_H_
