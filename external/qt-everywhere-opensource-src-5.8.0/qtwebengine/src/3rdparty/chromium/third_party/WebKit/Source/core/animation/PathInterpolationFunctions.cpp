// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/PathInterpolationFunctions.h"

#include "core/animation/InterpolatedSVGPathSource.h"
#include "core/animation/InterpolationEnvironment.h"
#include "core/animation/SVGPathSegInterpolationFunctions.h"
#include "core/css/CSSPathValue.h"
#include "core/svg/SVGPath.h"
#include "core/svg/SVGPathByteStreamBuilder.h"
#include "core/svg/SVGPathByteStreamSource.h"
#include "core/svg/SVGPathParser.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class SVGPathNonInterpolableValue : public NonInterpolableValue {
public:
    virtual ~SVGPathNonInterpolableValue() {}

    static PassRefPtr<SVGPathNonInterpolableValue> create(Vector<SVGPathSegType>& pathSegTypes)
    {
        return adoptRef(new SVGPathNonInterpolableValue(pathSegTypes));
    }

    const Vector<SVGPathSegType>& pathSegTypes() const { return m_pathSegTypes; }

    DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

private:
    SVGPathNonInterpolableValue(Vector<SVGPathSegType>& pathSegTypes)
    {
        m_pathSegTypes.swap(pathSegTypes);
    }

    Vector<SVGPathSegType> m_pathSegTypes;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(SVGPathNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(SVGPathNonInterpolableValue);

enum PathComponentIndex {
    PathArgsIndex,
    PathNeutralIndex,
    PathComponentIndexCount,
};

InterpolationValue PathInterpolationFunctions::convertValue(const SVGPathByteStream& byteStream)
{
    SVGPathByteStreamSource pathSource(byteStream);
    size_t length = 0;
    PathCoordinates currentCoordinates;
    Vector<std::unique_ptr<InterpolableValue>> interpolablePathSegs;
    Vector<SVGPathSegType> pathSegTypes;

    while (pathSource.hasMoreData()) {
        const PathSegmentData segment = pathSource.parseSegment();
        interpolablePathSegs.append(SVGPathSegInterpolationFunctions::consumePathSeg(segment, currentCoordinates));
        pathSegTypes.append(segment.command);
        length++;
    }

    std::unique_ptr<InterpolableList> pathArgs = InterpolableList::create(length);
    for (size_t i = 0; i < interpolablePathSegs.size(); i++)
        pathArgs->set(i, std::move(interpolablePathSegs[i]));

    std::unique_ptr<InterpolableList> result = InterpolableList::create(PathComponentIndexCount);
    result->set(PathArgsIndex, std::move(pathArgs));
    result->set(PathNeutralIndex, InterpolableNumber::create(0));

    return InterpolationValue(std::move(result), SVGPathNonInterpolableValue::create(pathSegTypes));
}

InterpolationValue PathInterpolationFunctions::convertValue(const StylePath* stylePath)
{
    if (stylePath)
        return convertValue(stylePath->byteStream());

    std::unique_ptr<SVGPathByteStream> emptyPath = SVGPathByteStream::create();
    return convertValue(*emptyPath);
}

class UnderlyingPathSegTypesChecker : public InterpolationType::ConversionChecker {
public:
    ~UnderlyingPathSegTypesChecker() final {}

    static std::unique_ptr<UnderlyingPathSegTypesChecker> create(const InterpolationValue& underlying)
    {
        return wrapUnique(new UnderlyingPathSegTypesChecker(getPathSegTypes(underlying)));
    }

private:
    UnderlyingPathSegTypesChecker(const Vector<SVGPathSegType>& pathSegTypes)
        : m_pathSegTypes(pathSegTypes)
    { }

    static const Vector<SVGPathSegType>& getPathSegTypes(const InterpolationValue& underlying)
    {
        return toSVGPathNonInterpolableValue(*underlying.nonInterpolableValue).pathSegTypes();
    }

    bool isValid(const InterpolationEnvironment&, const InterpolationValue& underlying) const final
    {
        return m_pathSegTypes == getPathSegTypes(underlying);
    }

    Vector<SVGPathSegType> m_pathSegTypes;
};

InterpolationValue PathInterpolationFunctions::maybeConvertNeutral(const InterpolationValue& underlying, InterpolationType::ConversionCheckers& conversionCheckers)
{
    conversionCheckers.append(UnderlyingPathSegTypesChecker::create(underlying));
    std::unique_ptr<InterpolableList> result = InterpolableList::create(PathComponentIndexCount);
    result->set(PathArgsIndex, toInterpolableList(*underlying.interpolableValue).get(PathArgsIndex)->cloneAndZero());
    result->set(PathNeutralIndex, InterpolableNumber::create(1));
    return InterpolationValue(std::move(result), underlying.nonInterpolableValue.get());
}

static bool pathSegTypesMatch(const Vector<SVGPathSegType>& a, const Vector<SVGPathSegType>& b)
{
    if (a.size() != b.size())
        return false;

    for (size_t i = 0; i < a.size(); i++) {
        if (toAbsolutePathSegType(a[i]) != toAbsolutePathSegType(b[i]))
            return false;
    }

    return true;
}

PairwiseInterpolationValue PathInterpolationFunctions::maybeMergeSingles(InterpolationValue&& start, InterpolationValue&& end)
{
    const Vector<SVGPathSegType>& startTypes = toSVGPathNonInterpolableValue(*start.nonInterpolableValue).pathSegTypes();
    const Vector<SVGPathSegType>& endTypes = toSVGPathNonInterpolableValue(*end.nonInterpolableValue).pathSegTypes();
    if (!pathSegTypesMatch(startTypes, endTypes))
        return nullptr;

    return PairwiseInterpolationValue(std::move(start.interpolableValue), std::move(end.interpolableValue), end.nonInterpolableValue.release());
}

void PathInterpolationFunctions::composite(UnderlyingValueOwner& underlyingValueOwner, double underlyingFraction, const InterpolationType& type, const InterpolationValue& value)
{
    const InterpolableList& list = toInterpolableList(*value.interpolableValue);
    double neutralComponent = toInterpolableNumber(list.get(PathNeutralIndex))->value();

    if (neutralComponent == 0) {
        underlyingValueOwner.set(type, value);
        return;
    }

    ASSERT(pathSegTypesMatch(
        toSVGPathNonInterpolableValue(*underlyingValueOwner.value().nonInterpolableValue).pathSegTypes(),
        toSVGPathNonInterpolableValue(*value.nonInterpolableValue).pathSegTypes()));
    underlyingValueOwner.mutableValue().interpolableValue->scaleAndAdd(neutralComponent, *value.interpolableValue);
    underlyingValueOwner.mutableValue().nonInterpolableValue = value.nonInterpolableValue.get();
}

std::unique_ptr<SVGPathByteStream> PathInterpolationFunctions::appliedValue(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue)
{
    std::unique_ptr<SVGPathByteStream> pathByteStream = SVGPathByteStream::create();
    InterpolatedSVGPathSource source(
        toInterpolableList(*toInterpolableList(interpolableValue).get(PathArgsIndex)),
        toSVGPathNonInterpolableValue(nonInterpolableValue)->pathSegTypes());
    SVGPathByteStreamBuilder builder(*pathByteStream);
    SVGPathParser::parsePath(source, builder);
    return pathByteStream;
}

} // namespace blink
