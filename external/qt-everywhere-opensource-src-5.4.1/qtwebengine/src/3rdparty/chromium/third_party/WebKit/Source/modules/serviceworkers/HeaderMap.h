// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HeaderMap_h
#define HeaderMap_h

#include "bindings/v8/ScriptWrappable.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/RefCounted.h"
#include "wtf/text/StringHash.h"

namespace WebCore {

class HeaderMapForEachCallback;
class ScriptValue;

class HeaderMap FINAL : public ScriptWrappable, public RefCounted<HeaderMap> {
public:
    static PassRefPtr<HeaderMap> create();
    static PassRefPtr<HeaderMap> create(const HashMap<String, String>& headers);

    // HeaderMap.idl implementation.
    unsigned long size() const;
    void clear();
    bool remove(const String& key);
    String get(const String& key);
    bool has(const String& key);
    void set(const String& key, const String& value);
    void forEach(PassOwnPtr<HeaderMapForEachCallback>, ScriptValue&);
    void forEach(PassOwnPtr<HeaderMapForEachCallback>);

    const HashMap<String, String>& headerMap() const { return m_headers; }

private:
    HeaderMap();
    explicit HeaderMap(const HashMap<String, String>& headers);
    void forEachInternal(PassOwnPtr<HeaderMapForEachCallback>, ScriptValue* thisArg);

    // FIXME: this doesn't preserve ordering while ES6 Map type requires it.
    HashMap<String, String> m_headers;
};

} // namespace WebCore

#endif // HeaderMap_h
