// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaSessionError_h
#define MediaSessionError_h

#include "platform/heap/Handle.h"
#include <v8/include/v8.h>

namespace blink {

class DOMException;
class ScriptPromiseResolver;
enum class WebMediaSessionError;

class MediaSessionError {
    STATIC_ONLY(MediaSessionError);
public:
    // For CallbackPromiseAdapter
    using WebType = const WebMediaSessionError&;
    static DOMException* take(ScriptPromiseResolver*, const WebMediaSessionError& webError);
};

} // namespace blink

#endif // MediaSessionError_h
