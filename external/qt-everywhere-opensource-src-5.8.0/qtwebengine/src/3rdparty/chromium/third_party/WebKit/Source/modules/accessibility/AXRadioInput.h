// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef AXRadioInput_h
#define AXRadioInput_h

#include "modules/accessibility/AXLayoutObject.h"

namespace blink {

class AXObjectCacheImpl;
class HTMLInputElement;

class AXRadioInput final : public AXLayoutObject {
    WTF_MAKE_NONCOPYABLE(AXRadioInput);

public:
    static AXRadioInput* create(LayoutObject*, AXObjectCacheImpl&);
    ~AXRadioInput() override { }

    bool isAXRadioInput() const override { return true; }
    void updatePosAndSetSize(int position = 0);
    void requestUpdateToNextNode(bool forward);
    HTMLInputElement* findFirstRadioButtonInGroup(HTMLInputElement* current) const;

    int posInSet() const final;
    int setSize() const final;

private:
    AXRadioInput(LayoutObject*, AXObjectCacheImpl&);
    bool calculatePosInSet();
    int countFromFirstElement() const;
    HTMLInputElement* element() const;
    int sizeOfRadioGroup() const;

    int m_posInSet;
    int m_setSize;
};

DEFINE_AX_OBJECT_TYPE_CASTS(AXRadioInput, isAXRadioInput());

} // namespace blink

#endif // AXRadioInput_h

