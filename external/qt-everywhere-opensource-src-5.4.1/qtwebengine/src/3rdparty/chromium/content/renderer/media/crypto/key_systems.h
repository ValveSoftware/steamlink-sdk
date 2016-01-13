// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CRYPTO_KEY_SYSTEMS_H_
#define CONTENT_RENDERER_MEDIA_CRYPTO_KEY_SYSTEMS_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"

namespace content {

// Prefixed EME API only supports prefixed (webkit-) key system name for
// certain key systems. But internally only unprefixed key systems are
// supported. The following two functions help convert between prefixed and
// unprefixed key system names.

// Gets the unprefixed key system name for |key_system|.
std::string GetUnprefixedKeySystemName(const std::string& key_system);

// Gets the prefixed key system name for |key_system|.
std::string GetPrefixedKeySystemName(const std::string& key_system);

// Returns whether |key_system| is a real supported key system that can be
// instantiated.
// Abstract parent |key_system| strings will return false.
// Call IsSupportedKeySystemWithMediaMimeType() to determine whether a
// |key_system| supports a specific type of media or to check parent key
// systems.
CONTENT_EXPORT bool IsConcreteSupportedKeySystem(const std::string& key_system);

// Returns whether |key_sytem| supports the specified media type and codec(s).
CONTENT_EXPORT bool IsSupportedKeySystemWithMediaMimeType(
    const std::string& mime_type,
    const std::vector<std::string>& codecs,
    const std::string& key_system);

// Returns a name for |key_system| suitable to UMA logging.
CONTENT_EXPORT std::string KeySystemNameForUMA(const std::string& key_system);

// Returns whether AesDecryptor can be used for the given |concrete_key_system|.
CONTENT_EXPORT bool CanUseAesDecryptor(const std::string& concrete_key_system);

#if defined(ENABLE_PEPPER_CDMS)
// Returns the Pepper MIME type for |concrete_key_system|.
// Returns empty string if |concrete_key_system| is unknown or not Pepper-based.
CONTENT_EXPORT std::string GetPepperType(
    const std::string& concrete_key_system);
#endif

#if defined(UNIT_TEST)
// Helper functions to add container/codec types for testing purposes.
CONTENT_EXPORT void AddContainerMask(const std::string& container, uint32 mask);
CONTENT_EXPORT void AddCodecMask(const std::string& codec, uint32 mask);
#endif  // defined(UNIT_TEST)

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CRYPTO_KEY_SYSTEMS_H_
