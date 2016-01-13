// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GamepadButton_h
#define GamepadButton_h

#include "bindings/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"

namespace WebCore {

class GamepadButton FINAL : public GarbageCollectedFinalized<GamepadButton>, public ScriptWrappable {
public:
    static GamepadButton* create();
    ~GamepadButton();

    double value() const { return m_value; }
    void setValue(double val) { m_value = val; }

    bool pressed() const { return m_pressed; }
    void setPressed(bool val) { m_pressed = val; }

    void trace(Visitor*);

private:
    GamepadButton();
    double m_value;
    bool m_pressed;
};

typedef HeapVector<Member<GamepadButton> > GamepadButtonVector;

} // namespace WebCore

#endif // GamepadButton_h
