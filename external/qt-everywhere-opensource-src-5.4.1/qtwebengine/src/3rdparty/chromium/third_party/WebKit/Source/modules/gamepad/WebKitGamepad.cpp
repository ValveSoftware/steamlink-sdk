// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/gamepad/WebKitGamepad.h"

namespace WebCore {

WebKitGamepad::WebKitGamepad()
{
    ScriptWrappable::init(this);
}

WebKitGamepad::~WebKitGamepad()
{
}

void WebKitGamepad::setButtons(unsigned count, const blink::WebGamepadButton* data)
{
    m_buttons.resize(count);
    for (unsigned i = 0; i < count; ++i)
        m_buttons[i] = data[i].value;
}

} // namespace WebCore
