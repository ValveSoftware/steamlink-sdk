// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/style/StyleVariableData.h"

#include "core/style/DataEquivalency.h"

namespace blink {

bool StyleVariableData::operator==(const StyleVariableData& other) const
{
    // It's technically possible for divergent roots to be value-equal,
    // but unlikely. This equality operator is used for optimization purposes
    // so it's OK to be occasionally wrong.
    // TODO(shanestephens): Rename this to something that indicates it may not
    // always return equality.
    if (m_root != other.m_root)
        return false;

    if (m_data.size() != other.m_data.size())
        return false;

    for (const auto& iter : m_data) {
        RefPtr<CSSVariableData> otherData = other.m_data.get(iter.key);
        if (!dataEquivalent(iter.value, otherData))
            return false;
    }

    return true;
}

StyleVariableData::StyleVariableData(StyleVariableData& other)
{
    if (!other.m_root) {
        m_root = &other;
    } else {
        m_data = other.m_data;
        m_root = other.m_root;
    }
}

CSSVariableData* StyleVariableData::getVariable(const AtomicString& name) const
{
    auto result = m_data.find(name);
    if (result == m_data.end() && m_root)
        return m_root->getVariable(name);
    if (result == m_data.end())
        return nullptr;
    return result->value.get();
}

std::unique_ptr<HashMap<AtomicString, RefPtr<CSSVariableData>>> StyleVariableData::getVariables() const
{
    std::unique_ptr<HashMap<AtomicString, RefPtr<CSSVariableData>>> result;
    if (m_root) {
        result.reset(new HashMap<AtomicString, RefPtr<CSSVariableData>>(m_root->m_data));
        for (auto it = m_data.begin(); it != m_data.end(); ++it)
            result->set(it->key, it->value);
    } else {
        result.reset(new HashMap<AtomicString, RefPtr<CSSVariableData>>(m_data));
    }
    return result;
}

} // namespace blink
