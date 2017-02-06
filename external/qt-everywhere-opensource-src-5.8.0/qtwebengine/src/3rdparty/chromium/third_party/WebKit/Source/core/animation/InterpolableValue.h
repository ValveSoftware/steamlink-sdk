// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterpolableValue_h
#define InterpolableValue_h

#include "core/CoreExport.h"
#include "core/animation/animatable/AnimatableValue.h"
#include "platform/heap/Handle.h"
#include "wtf/PtrUtil.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

// Represents the components of a PropertySpecificKeyframe's value that change smoothly as it interpolates to an adjacent value.
class CORE_EXPORT InterpolableValue {
    USING_FAST_MALLOC(InterpolableValue);
public:
    virtual ~InterpolableValue() { }

    virtual bool isNumber() const { return false; }
    virtual bool isBool() const { return false; }
    virtual bool isList() const { return false; }
    virtual bool isAnimatableValue() const { return false; }

    virtual bool equals(const InterpolableValue&) const = 0;
    virtual std::unique_ptr<InterpolableValue> clone() const = 0;
    virtual std::unique_ptr<InterpolableValue> cloneAndZero() const = 0;
    virtual void scale(double scale) = 0;
    virtual void scaleAndAdd(double scale, const InterpolableValue& other) = 0;

private:
    virtual void interpolate(const InterpolableValue& to, const double progress, InterpolableValue& result) const = 0;

    friend class Interpolation;
    friend class PairwisePrimitiveInterpolation;

    // Keep interpolate private, but allow calls within the hierarchy without
    // knowledge of type.
    friend class DeferredLegacyStyleInterpolation;
    friend class InterpolableNumber;
    friend class InterpolableBool;
    friend class InterpolableList;

    friend class AnimationInterpolableValueTest;
};

class CORE_EXPORT InterpolableNumber final : public InterpolableValue {
public:
    static std::unique_ptr<InterpolableNumber> create(double value)
    {
        return wrapUnique(new InterpolableNumber(value));
    }

    bool isNumber() const final { return true; }
    double value() const { return m_value; }
    bool equals(const InterpolableValue& other) const final;
    std::unique_ptr<InterpolableValue> clone() const final { return create(m_value); }
    std::unique_ptr<InterpolableValue> cloneAndZero() const final { return create(0); }
    void scale(double scale) final;
    void scaleAndAdd(double scale, const InterpolableValue& other) final;
    void set(double value) { m_value = value; }

private:
    void interpolate(const InterpolableValue& to, const double progress, InterpolableValue& result) const final;
    double m_value;

    explicit InterpolableNumber(double value)
        : m_value(value)
    {
    }

};

class CORE_EXPORT InterpolableBool final : public InterpolableValue {
public:
    static std::unique_ptr<InterpolableBool> create(bool value)
    {
        return wrapUnique(new InterpolableBool(value));
    }

    bool isBool() const final { return true; }
    bool value() const { return m_value; }
    bool equals(const InterpolableValue&) const final { NOTREACHED(); return false; }
    std::unique_ptr<InterpolableValue> clone() const final { return create(m_value); }
    std::unique_ptr<InterpolableValue> cloneAndZero() const final { NOTREACHED(); return nullptr; }
    void scale(double scale) final { NOTREACHED(); }
    void scaleAndAdd(double scale, const InterpolableValue& other) final { NOTREACHED(); }

private:
    void interpolate(const InterpolableValue& to, const double progress, InterpolableValue& result) const final;
    bool m_value;

    explicit InterpolableBool(bool value)
        : m_value(value)
    {
    }

};

class CORE_EXPORT InterpolableList : public InterpolableValue {
public:
    // Explicitly delete operator= because MSVC automatically generate
    // copy constructors and operator= for dll-exported classes.
    // Since InterpolableList is not copyable, automatically generated
    // operator= causes MSVC compiler error.
    // However, we cannot use WTF_MAKE_NONCOPYABLE because InterpolableList
    // has its own copy constructor. So just delete operator= here.
    InterpolableList& operator=(const InterpolableList&) = delete;

    static std::unique_ptr<InterpolableList> create(const InterpolableList &other)
    {
        return wrapUnique(new InterpolableList(other));
    }

    static std::unique_ptr<InterpolableList> create(size_t size)
    {
        return wrapUnique(new InterpolableList(size));
    }

    bool isList() const final { return true; }
    void set(size_t position, std::unique_ptr<InterpolableValue> value)
    {
        ASSERT(position < m_size);
        m_values[position] = std::move(value);
    }
    const InterpolableValue* get(size_t position) const
    {
        ASSERT(position < m_size);
        return m_values[position].get();
    }
    std::unique_ptr<InterpolableValue>& getMutable(size_t position)
    {
        ASSERT(position < m_size);
        return m_values[position];
    }
    size_t length() const { return m_size; }
    bool equals(const InterpolableValue& other) const final;
    std::unique_ptr<InterpolableValue> clone() const final { return create(*this); }
    std::unique_ptr<InterpolableValue> cloneAndZero() const final;
    void scale(double scale) final;
    void scaleAndAdd(double scale, const InterpolableValue& other) final;

private:
    void interpolate(const InterpolableValue& to, const double progress, InterpolableValue& result) const final;
    explicit InterpolableList(size_t size)
        : m_size(size)
        , m_values(m_size)
    {
    }

    InterpolableList(const InterpolableList& other)
        : m_size(other.m_size)
        , m_values(m_size)
    {
        for (size_t i = 0; i < m_size; i++)
            set(i, other.m_values[i]->clone());
    }

    size_t m_size;
    Vector<std::unique_ptr<InterpolableValue>> m_values;
};

// FIXME: Remove this when we can.
class InterpolableAnimatableValue : public InterpolableValue {
public:
    static std::unique_ptr<InterpolableAnimatableValue> create(PassRefPtr<AnimatableValue> value)
    {
        return wrapUnique(new InterpolableAnimatableValue(value));
    }

    bool isAnimatableValue() const final { return true; }
    AnimatableValue* value() const { return m_value.get(); }
    bool equals(const InterpolableValue&) const final { NOTREACHED(); return false; }
    std::unique_ptr<InterpolableValue> clone() const final { return create(m_value); }
    std::unique_ptr<InterpolableValue> cloneAndZero() const final { NOTREACHED(); return nullptr; }
    void scale(double scale) final { NOTREACHED(); }
    void scaleAndAdd(double scale, const InterpolableValue& other) final { NOTREACHED(); }

private:
    void interpolate(const InterpolableValue &to, const double progress, InterpolableValue& result) const final;
    RefPtr<AnimatableValue> m_value;

    InterpolableAnimatableValue(PassRefPtr<AnimatableValue> value)
        : m_value(value)
    {
    }
};

DEFINE_TYPE_CASTS(InterpolableNumber, InterpolableValue, value, value->isNumber(), value.isNumber());
DEFINE_TYPE_CASTS(InterpolableBool, InterpolableValue, value, value->isBool(), value.isBool());
DEFINE_TYPE_CASTS(InterpolableList, InterpolableValue, value, value->isList(), value.isList());
DEFINE_TYPE_CASTS(InterpolableAnimatableValue, InterpolableValue, value, value->isAnimatableValue(), value.isAnimatableValue());

} // namespace blink

#endif
