// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SSL_STATUS_SERIALIZATION_H_
#define CONTENT_COMMON_SSL_STATUS_SERIALIZATION_H_

#include <string>

#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "content/public/common/ssl_status.h"

namespace content {

// Serializes the given security info. Does NOT include
// |ssl_status.content_status| in the serialized info.
CONTENT_EXPORT std::string SerializeSecurityInfo(const SSLStatus& ssl_status);

// Deserializes the given security info into |ssl_status|. Note that
// this returns the |content_status| field with its default
// value. Returns true on success and false if the state couldn't be
// deserialized. If false, all fields in |ssl_status| will be set to
// their default values. Note that this function does not validate that
// the deserialized SSLStatus is internally consistent (e.g. that the
// |security_style| matches up with the rest of the fields).
bool CONTENT_EXPORT
DeserializeSecurityInfo(const std::string& state,
                        SSLStatus* ssl_status) WARN_UNUSED_RESULT;

}  // namespace content

#endif  // CONTENT_COMMON_SSL_STATUS_SERIALIZATION_H_
