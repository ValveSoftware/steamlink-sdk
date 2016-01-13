// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HeaderMapForEachCallback_h
#define HeaderMapForEachCallback_h

#include "bindings/v8/ScriptValue.h"

namespace WebCore {

class HeaderMap;

class HeaderMapForEachCallback {
public:
    virtual ~HeaderMapForEachCallback() { }
    virtual bool handleItem(ScriptValue thisValue, const String& value, const String& key, HeaderMap*) = 0;
    virtual bool handleItem(const String& value, const String& key, HeaderMap*) = 0;
};

} // namespace WebCore

#endif // HeaderMapForEachCallback_h
