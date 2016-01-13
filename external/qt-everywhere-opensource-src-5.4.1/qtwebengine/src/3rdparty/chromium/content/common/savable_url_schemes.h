// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SAVABLE_URL_SCHEMES_H_
#define CONTENT_COMMON_SAVABLE_URL_SCHEMES_H_

namespace content {

// Null terminated list of schemes that are savable. This function can be
// invoked on any thread.
const char* const* GetSavableSchemesInternal();

// Intended for use only during initialization.
void SetSavableSchemes(const char* const* savable_schemes);

}  // namespace content

#endif  // CONTENT_COMMON_SAVABLE_URL_SCHEMES_H_
