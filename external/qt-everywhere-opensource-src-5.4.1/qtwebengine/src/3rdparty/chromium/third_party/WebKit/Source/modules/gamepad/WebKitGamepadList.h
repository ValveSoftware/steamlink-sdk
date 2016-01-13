// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebKitGamepadList_h
#define WebKitGamepadList_h

#include "bindings/v8/ScriptWrappable.h"
#include "modules/gamepad/WebKitGamepad.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebGamepads.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"

namespace WebCore {

class WebKitGamepadList FINAL : public GarbageCollectedFinalized<WebKitGamepadList>, public ScriptWrappable {
public:
    static WebKitGamepadList* create()
    {
        return new WebKitGamepadList;
    }
    ~WebKitGamepadList();

    void set(unsigned index, WebKitGamepad*);
    WebKitGamepad* item(unsigned index);
    unsigned length() const { return blink::WebGamepads::itemsLengthCap; }

    void trace(Visitor*);

private:
    WebKitGamepadList();
    Member<WebKitGamepad> m_items[blink::WebGamepads::itemsLengthCap];
};

} // namespace WebCore

#endif // WebKitGamepadList_h
