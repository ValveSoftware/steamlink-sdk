// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PseudoElementData_h
#define PseudoElementData_h

#include "platform/heap/Handle.h"

namespace blink {

class PseudoElementData final : public GarbageCollected<PseudoElementData> {
    WTF_MAKE_NONCOPYABLE(PseudoElementData);
public:
    static PseudoElementData* create();
    void setPseudoElement(PseudoId, PseudoElement*);
    PseudoElement* pseudoElement(PseudoId) const;
    bool hasPseudoElements() const;
    void clearPseudoElements();
    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_generatedBefore);
        visitor->trace(m_generatedAfter);
        visitor->trace(m_generatedFirstLetter);
        visitor->trace(m_backdrop);
    }
private:
    PseudoElementData() = default;
    Member<PseudoElement> m_generatedBefore;
    Member<PseudoElement> m_generatedAfter;
    Member<PseudoElement> m_generatedFirstLetter;
    Member<PseudoElement> m_backdrop;
};

inline PseudoElementData* PseudoElementData::create()
{
    return new PseudoElementData();
}

inline bool PseudoElementData::hasPseudoElements() const
{
    return m_generatedBefore || m_generatedAfter || m_backdrop || m_generatedFirstLetter;
}

inline void PseudoElementData::clearPseudoElements()
{
    setPseudoElement(PseudoIdBefore, nullptr);
    setPseudoElement(PseudoIdAfter, nullptr);
    setPseudoElement(PseudoIdBackdrop, nullptr);
    setPseudoElement(PseudoIdFirstLetter, nullptr);
}

inline void PseudoElementData::setPseudoElement(PseudoId pseudoId, PseudoElement* element)
{
    switch (pseudoId) {
    case PseudoIdBefore:
        if (m_generatedBefore)
            m_generatedBefore->dispose();
        m_generatedBefore = element;
        break;
    case PseudoIdAfter:
        if (m_generatedAfter)
            m_generatedAfter->dispose();
        m_generatedAfter = element;
        break;
    case PseudoIdBackdrop:
        if (m_backdrop)
            m_backdrop->dispose();
        m_backdrop = element;
        break;
    case PseudoIdFirstLetter:
        if (m_generatedFirstLetter)
            m_generatedFirstLetter->dispose();
        m_generatedFirstLetter = element;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

inline PseudoElement* PseudoElementData::pseudoElement(PseudoId pseudoId) const
{
    switch (pseudoId) {
    case PseudoIdBefore:
        return m_generatedBefore;
    case PseudoIdAfter:
        return m_generatedAfter;
    case PseudoIdBackdrop:
        return m_backdrop;
    case PseudoIdFirstLetter:
        return m_generatedFirstLetter;
    default:
        return nullptr;
    }
}

} // namespace blink

#endif // PseudoElementData_h
