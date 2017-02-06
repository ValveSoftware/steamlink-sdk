// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FontVariantNumeric_h
#define FontVariantNumeric_h

#include "wtf/Allocator.h"

namespace blink {

class FontVariantNumeric {
    STACK_ALLOCATED();

public:

    enum NumericFigure {
        NormalFigure = 0,
        LiningNums,
        OldstyleNums
    };

    enum NumericSpacing {
        NormalSpacing = 0,
        ProportionalNums,
        TabularNums
    };

    enum NumericFraction {
        NormalFraction = 0,
        DiagonalFractions,
        StackedFractions
    };

    enum Ordinal {
        OrdinalOff = 0,
        OrdinalOn
    };

    enum SlashedZero {
        SlashedZeroOff = 0,
        SlashedZeroOn
    };

    FontVariantNumeric() : m_fieldsAsUnsigned(0) { }

    static FontVariantNumeric initializeFromUnsigned(unsigned initValue)
    {
        return FontVariantNumeric(initValue);
    }

    void setNumericFigure(NumericFigure figure) { m_fields.m_numericFigure = figure; };
    void setNumericSpacing(NumericSpacing spacing) { m_fields.m_numericSpacing = spacing; };
    void setNumericFraction(NumericFraction fraction) { m_fields.m_numericFraction = fraction; };
    void setOrdinal(Ordinal ordinal) { m_fields.m_ordinal = ordinal; };
    void setSlashedZero(SlashedZero slashedZero) { m_fields.m_slashedZero = slashedZero; };

    NumericFigure numericFigureValue() const { return static_cast<NumericFigure>(m_fields.m_numericFigure); }
    NumericSpacing numericSpacingValue() const { return static_cast<NumericSpacing>(m_fields.m_numericSpacing); }
    NumericFraction numericFractionValue() const { return static_cast<NumericFraction>(m_fields.m_numericFraction); }
    Ordinal ordinalValue() const { return static_cast<Ordinal>(m_fields.m_ordinal); };
    SlashedZero slashedZeroValue() const { return static_cast<SlashedZero>(m_fields.m_slashedZero); }

    bool isAllNormal() { return !m_fieldsAsUnsigned; }

    bool operator==(const FontVariantNumeric& other) const
    {
        return m_fieldsAsUnsigned == other.m_fieldsAsUnsigned;
    }

private:
    FontVariantNumeric(unsigned initValue) : m_fieldsAsUnsigned(initValue) { }

    struct BitFields {
        unsigned m_numericFigure : 2;
        unsigned m_numericSpacing : 2;
        unsigned m_numericFraction : 2;
        unsigned m_ordinal : 1;
        unsigned m_slashedZero : 1;
    };


    union {
        BitFields m_fields;
        unsigned m_fieldsAsUnsigned;
    };
    static_assert(sizeof(BitFields) == sizeof(unsigned), "Mapped union types must match in size.");

    // Used in setVariant to store the value in m_fields.m_variantNumeric;
    friend class FontDescription;
};

}

#endif // FontVariantNumeric_h
