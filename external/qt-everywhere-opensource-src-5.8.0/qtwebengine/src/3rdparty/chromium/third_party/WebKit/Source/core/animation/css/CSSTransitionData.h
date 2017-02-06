// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSTransitionData_h
#define CSSTransitionData_h

#include "core/CSSPropertyNames.h"
#include "core/animation/css/CSSTimingData.h"
#include "wtf/PtrUtil.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

class CSSTransitionData final : public CSSTimingData {
public:
    enum TransitionPropertyType {
        TransitionNone,
        TransitionKnownProperty,
        TransitionUnknownProperty,
    };

    // FIXME: We shouldn't allow 'none' to be used alongside other properties.
    struct TransitionProperty {
        DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
        TransitionProperty(CSSPropertyID id)
            : propertyType(TransitionKnownProperty)
            , unresolvedProperty(id)
        {
            ASSERT(id != CSSPropertyInvalid);
        }

        TransitionProperty(const String& string)
            : propertyType(TransitionUnknownProperty)
            , unresolvedProperty(CSSPropertyInvalid)
            , propertyString(string)
        {
        }

        TransitionProperty(TransitionPropertyType type)
            : propertyType(type)
            , unresolvedProperty(CSSPropertyInvalid)
        {
            ASSERT(type == TransitionNone);
        }

        bool operator==(const TransitionProperty& other) const { return propertyType == other.propertyType && unresolvedProperty == other.unresolvedProperty && propertyString == other.propertyString; }

        TransitionPropertyType propertyType;
        CSSPropertyID unresolvedProperty;
        String propertyString;
    };

    static std::unique_ptr<CSSTransitionData> create()
    {
        return wrapUnique(new CSSTransitionData);
    }

    static std::unique_ptr<CSSTransitionData> create(const CSSTransitionData& transitionData)
    {
        return wrapUnique(new CSSTransitionData(transitionData));
    }

    bool transitionsMatchForStyleRecalc(const CSSTransitionData& other) const;

    Timing convertToTiming(size_t index) const;

    const Vector<TransitionProperty>& propertyList() const { return m_propertyList; }
    Vector<TransitionProperty>& propertyList() { return m_propertyList; }

    static TransitionProperty initialProperty() { return TransitionProperty(CSSPropertyAll); }

private:
    CSSTransitionData();
    explicit CSSTransitionData(const CSSTransitionData&);

    Vector<TransitionProperty> m_propertyList;
};

} // namespace blink

#endif // CSSTransitionData_h
