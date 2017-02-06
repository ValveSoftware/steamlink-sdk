/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/graphics/filters/FilterOperations.h"

#include "platform/LengthFunctions.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/filters/FEGaussianBlur.h"
#include <numeric>

namespace blink {

FilterOperations::FilterOperations()
{
}

DEFINE_TRACE(FilterOperations)
{
    visitor->trace(m_operations);
}

FilterOperations& FilterOperations::operator=(const FilterOperations& other)
{
    m_operations = other.m_operations;
    return *this;
}

bool FilterOperations::operator==(const FilterOperations& o) const
{
    if (m_operations.size() != o.m_operations.size())
        return false;

    unsigned s = m_operations.size();
    for (unsigned i = 0; i < s; i++) {
        if (*m_operations[i] != *o.m_operations[i])
            return false;
    }

    return true;
}

bool FilterOperations::canInterpolateWith(const FilterOperations& other) const
{
    for (size_t i = 0; i < operations().size(); ++i) {
        if (!FilterOperation::canInterpolate(operations()[i]->type()))
            return false;
    }

    for (size_t i = 0; i < other.operations().size(); ++i) {
        if (!FilterOperation::canInterpolate(other.operations()[i]->type()))
            return false;
    }

    size_t commonSize = std::min(operations().size(), other.operations().size());
    for (size_t i = 0; i < commonSize; ++i) {
        if (!operations()[i]->isSameType(*other.operations()[i]))
            return false;
    }
    return true;
}

bool FilterOperations::hasReferenceFilter() const
{
    for (size_t i = 0; i < m_operations.size(); ++i) {
        if (m_operations.at(i)->type() == FilterOperation::REFERENCE)
            return true;
    }
    return false;
}

FloatRect FilterOperations::mapRect(const FloatRect& rect) const
{
    auto accumulateMappedRect = [](const FloatRect& rect, const Member<FilterOperation>& op)
    {
        return op->mapRect(rect);
    };
    return std::accumulate(m_operations.begin(), m_operations.end(), rect, accumulateMappedRect);
}

bool FilterOperations::hasFilterThatAffectsOpacity() const
{
    for (size_t i = 0; i < m_operations.size(); ++i)
        if (m_operations[i]->affectsOpacity())
            return true;
    return false;
}

bool FilterOperations::hasFilterThatMovesPixels() const
{
    for (size_t i = 0; i < m_operations.size(); ++i)
        if (m_operations[i]->movesPixels())
            return true;
    return false;
}

} // namespace blink
