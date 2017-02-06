// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleVariableData_h
#define StyleVariableData_h

#include "core/css/CSSVariableData.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/RefCounted.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

class StyleVariableData : public RefCounted<StyleVariableData> {
public:
    static PassRefPtr<StyleVariableData> create() { return adoptRef(new StyleVariableData()); }

    PassRefPtr<StyleVariableData> copy() { return adoptRef(new StyleVariableData(*this)); }

    bool operator==(const StyleVariableData& other) const;
    bool operator!=(const StyleVariableData& other) const { return !(*this == other); }

    void setVariable(const AtomicString& name, PassRefPtr<CSSVariableData> value) { m_data.set(name, value); }
    CSSVariableData* getVariable(const AtomicString& name) const;
    void removeVariable(const AtomicString& name) { return setVariable(name, nullptr); }

    // This map will contain null pointers if variables are invalid due to
    // cycles or referencing invalid variables without using a fallback.
    // Note that this method is slow as a new map is constructed.
    std::unique_ptr<HashMap<AtomicString, RefPtr<CSSVariableData>>> getVariables() const;
private:
    StyleVariableData()
        : m_root(nullptr)
        { }
    StyleVariableData(StyleVariableData& other);

    friend class CSSVariableResolver;

    HashMap<AtomicString, RefPtr<CSSVariableData>> m_data;
    RefPtr<StyleVariableData> m_root;
};

} // namespace blink

#endif // StyleVariableData_h
