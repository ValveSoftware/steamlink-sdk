// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ListInterpolationFunctions_h
#define ListInterpolationFunctions_h

#include "core/animation/InterpolationValue.h"
#include "core/animation/PairwiseInterpolationValue.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

class CSSValueList;
class UnderlyingValueOwner;
class InterpolationType;

class ListInterpolationFunctions {
public:
    template <typename CreateItemCallback>
    static InterpolationValue createList(size_t length, CreateItemCallback);
    static InterpolationValue createEmptyList() { return InterpolationValue(InterpolableList::create(0)); }

    using MergeSingleItemConversionsCallback = PairwiseInterpolationValue (*)(InterpolationValue&& start, InterpolationValue&& end);
    static PairwiseInterpolationValue maybeMergeSingles(InterpolationValue&& start, InterpolationValue&& end, MergeSingleItemConversionsCallback);

    using EqualNonInterpolableValuesCallback = bool (*)(const NonInterpolableValue*, const NonInterpolableValue*);
    static bool equalValues(const InterpolationValue&, const InterpolationValue&, EqualNonInterpolableValuesCallback);

    using NonInterpolableValuesAreCompatibleCallback = bool (*)(const NonInterpolableValue*, const NonInterpolableValue*);
    using CompositeItemCallback = void (*)(std::unique_ptr<InterpolableValue>&, RefPtr<NonInterpolableValue>&, double underlyingFraction, const InterpolableValue&, const NonInterpolableValue*);
    static void composite(UnderlyingValueOwner&, double underlyingFraction, const InterpolationType&, const InterpolationValue&, NonInterpolableValuesAreCompatibleCallback, CompositeItemCallback);
};

class NonInterpolableList : public NonInterpolableValue {
public:
    ~NonInterpolableList() final { }

    static PassRefPtr<NonInterpolableList> create()
    {
        return adoptRef(new NonInterpolableList());
    }
    static PassRefPtr<NonInterpolableList> create(Vector<RefPtr<NonInterpolableValue>>&& list)
    {
        return adoptRef(new NonInterpolableList(std::move(list)));
    }

    size_t length() const { return m_list.size(); }
    const NonInterpolableValue* get(size_t index) const { return m_list[index].get(); }
    NonInterpolableValue* get(size_t index) { return m_list[index].get(); }
    RefPtr<NonInterpolableValue>& getMutable(size_t index) { return m_list[index]; }

    DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

private:
    NonInterpolableList()
    { }
    NonInterpolableList(Vector<RefPtr<NonInterpolableValue>>&& list)
        : m_list(list)
    { }

    Vector<RefPtr<NonInterpolableValue>> m_list;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(NonInterpolableList);

template <typename CreateItemCallback>
InterpolationValue ListInterpolationFunctions::createList(size_t length, CreateItemCallback createItem)
{
    if (length == 0)
        return createEmptyList();
    std::unique_ptr<InterpolableList> interpolableList = InterpolableList::create(length);
    Vector<RefPtr<NonInterpolableValue>> nonInterpolableValues(length);
    for (size_t i = 0; i < length; i++) {
        InterpolationValue item = createItem(i);
        if (!item)
            return nullptr;
        interpolableList->set(i, std::move(item.interpolableValue));
        nonInterpolableValues[i] = item.nonInterpolableValue.release();
    }
    return InterpolationValue(std::move(interpolableList), NonInterpolableList::create(std::move(nonInterpolableValues)));
}

} // namespace blink

#endif // ListInterpolationFunctions_h
