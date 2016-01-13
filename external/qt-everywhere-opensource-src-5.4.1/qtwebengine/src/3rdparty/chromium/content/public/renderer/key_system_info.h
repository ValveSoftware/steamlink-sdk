// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_KEY_SYSTEM_INFO_H_
#define CONTENT_PUBLIC_RENDERER_KEY_SYSTEM_INFO_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "content/common/content_export.h"
#include "content/public/common/eme_codec.h"

// Definitions:
// * Key system
//    https://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#key-system
// * Concrete key system
//    A key system string that can be instantiated, such as
//    via the MediaKeys constructor. Examples include "org.w3.clearkey" and
//    "com.widevine.alpha".
// * Abstract key system
//    A key system string that cannot be instantiated like a concrete key system
//    but is otherwise useful, such as in discovery using isTypeSupported().
// * Parent key system
//    A key system string that is one level up from the child key system. It may
//    be an abstract key system.
//    As an example, "com.example" is the parent of "com.example.foo".

namespace content {

// Contains information about an EME key system as well as how to instantiate
// the corresponding CDM.
struct CONTENT_EXPORT KeySystemInfo {
  explicit KeySystemInfo(const std::string& key_system);
  ~KeySystemInfo();

  std::string key_system;

  // Specifies codecs supported by |key_system|.
  SupportedCodecs supported_codecs;

  // A hierarchical parent for |key_system|. This value can be used to check
  // supported types but cannot be used to instantiate a MediaKeys object.
  // Only one parent key system is currently supported per concrete key system.
  std::string parent_key_system;

  // The following indicate how the corresponding CDM should be instantiated.
  bool use_aes_decryptor;
#if defined(ENABLE_PEPPER_CDMS)
  std::string pepper_type;
#endif
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_KEY_SYSTEM_INFO_H_
