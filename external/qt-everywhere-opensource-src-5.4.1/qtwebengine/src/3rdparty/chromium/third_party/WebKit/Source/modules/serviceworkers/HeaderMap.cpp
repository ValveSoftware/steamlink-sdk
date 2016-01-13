// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/serviceworkers/HeaderMap.h"

#include "bindings/v8/ExceptionState.h"
#include "modules/serviceworkers/HeaderMapForEachCallback.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

PassRefPtr<HeaderMap> HeaderMap::create()
{
    return adoptRef(new HeaderMap);
}

PassRefPtr<HeaderMap> HeaderMap::create(const HashMap<String, String>& headers)
{
    return adoptRef(new HeaderMap(headers));
}

unsigned long HeaderMap::size() const
{
    return m_headers.size();
}

void HeaderMap::clear()
{
    m_headers.clear();
}

bool HeaderMap::remove(const String& key)
{
    if (!has(key))
        return false;
    m_headers.remove(key);
    return true;
}

String HeaderMap::get(const String& key)
{
    return m_headers.get(key);
}

bool HeaderMap::has(const String& key)
{
    return m_headers.find(key) != m_headers.end();
}

void HeaderMap::set(const String& key, const String& value)
{
    m_headers.set(key, value);
}

void HeaderMap::forEach(PassOwnPtr<HeaderMapForEachCallback> callback, ScriptValue& thisArg)
{
    forEachInternal(callback, &thisArg);
}

void HeaderMap::forEach(PassOwnPtr<HeaderMapForEachCallback> callback)
{
    forEachInternal(callback, 0);
}

HeaderMap::HeaderMap()
{
    ScriptWrappable::init(this);
}

HeaderMap::HeaderMap(const HashMap<String, String>& headers)
    : m_headers(headers)
{
    ScriptWrappable::init(this);
}

void HeaderMap::forEachInternal(PassOwnPtr<HeaderMapForEachCallback> callback, ScriptValue* thisArg)
{
    TrackExceptionState exceptionState;
    for (HashMap<String, String>::const_iterator it = m_headers.begin(); it != m_headers.end(); ++it) {
        if (thisArg)
            callback->handleItem(*thisArg, it->value, it->key, this);
        else
            callback->handleItem(it->value, it->key, this);
        if (exceptionState.hadException())
            break;
    }
}

} // namespace WebCore
