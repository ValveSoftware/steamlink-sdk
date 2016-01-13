/*
 * Copyright (C) Research In Motion Limited 2010, 2012. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include "core/svg/SVGPathUtilities.h"

#include "core/svg/SVGPathBlender.h"
#include "core/svg/SVGPathBuilder.h"
#include "core/svg/SVGPathByteStreamBuilder.h"
#include "core/svg/SVGPathByteStreamSource.h"
#include "core/svg/SVGPathParser.h"
#include "core/svg/SVGPathSegListBuilder.h"
#include "core/svg/SVGPathSegListSource.h"
#include "core/svg/SVGPathStringBuilder.h"
#include "core/svg/SVGPathStringSource.h"
#include "core/svg/SVGPathTraversalStateBuilder.h"
#include "platform/graphics/PathTraversalState.h"

namespace WebCore {

static SVGPathBuilder* globalSVGPathBuilder(Path& result)
{
    static SVGPathBuilder* s_builder = 0;
    if (!s_builder)
        s_builder = new SVGPathBuilder;

    s_builder->setCurrentPath(&result);
    return s_builder;
}

static SVGPathByteStreamBuilder* globalSVGPathByteStreamBuilder(SVGPathByteStream* result)
{
    static SVGPathByteStreamBuilder* s_builder = 0;
    if (!s_builder)
        s_builder = new SVGPathByteStreamBuilder;

    s_builder->setCurrentByteStream(result);
    return s_builder;
}

static SVGPathStringBuilder* globalSVGPathStringBuilder()
{
    static SVGPathStringBuilder* s_builder = 0;
    if (!s_builder)
        s_builder = new SVGPathStringBuilder;

    return s_builder;
}

static SVGPathTraversalStateBuilder* globalSVGPathTraversalStateBuilder(PathTraversalState& traversalState, float length)
{
    static SVGPathTraversalStateBuilder* s_builder = 0;
    if (!s_builder)
        s_builder = new SVGPathTraversalStateBuilder;

    s_builder->setCurrentTraversalState(&traversalState);
    s_builder->setDesiredLength(length);
    return s_builder;
}

static SVGPathParser* globalSVGPathParser(SVGPathSource* source, SVGPathConsumer* consumer)
{
    static SVGPathParser* s_parser = 0;
    if (!s_parser)
        s_parser = new SVGPathParser;

    s_parser->setCurrentSource(source);
    s_parser->setCurrentConsumer(consumer);
    return s_parser;
}

static SVGPathBlender* globalSVGPathBlender()
{
    static SVGPathBlender* s_blender = 0;
    if (!s_blender)
        s_blender = new SVGPathBlender;

    return s_blender;
}

bool buildPathFromString(const String& d, Path& result)
{
    if (d.isEmpty())
        return false;

    SVGPathBuilder* builder = globalSVGPathBuilder(result);

    OwnPtr<SVGPathStringSource> source = SVGPathStringSource::create(d);
    SVGPathParser* parser = globalSVGPathParser(source.get(), builder);
    bool ok = parser->parsePathDataFromSource(NormalizedParsing);
    parser->cleanup();
    return ok;
}

bool buildPathFromByteStream(const SVGPathByteStream* stream, Path& result)
{
    ASSERT(stream);
    if (stream->isEmpty())
        return false;

    SVGPathBuilder* builder = globalSVGPathBuilder(result);

    SVGPathByteStreamSource source(stream);
    SVGPathParser* parser = globalSVGPathParser(&source, builder);
    bool ok = parser->parsePathDataFromSource(NormalizedParsing);
    parser->cleanup();
    return ok;
}

bool buildStringFromByteStream(const SVGPathByteStream* stream, String& result, PathParsingMode parsingMode)
{
    ASSERT(stream);
    if (stream->isEmpty())
        return false;

    SVGPathStringBuilder* builder = globalSVGPathStringBuilder();

    SVGPathByteStreamSource source(stream);
    SVGPathParser* parser = globalSVGPathParser(&source, builder);
    bool ok = parser->parsePathDataFromSource(parsingMode);
    result = builder->result();
    parser->cleanup();
    return ok;
}

bool buildSVGPathByteStreamFromString(const String& d, SVGPathByteStream* result, PathParsingMode parsingMode)
{
    ASSERT(result);
    result->clear();
    if (d.isEmpty())
        return false;

    // The string length is typically a minor overestimate of eventual byte stream size, so it avoids us a lot of reallocs.
    result->reserveInitialCapacity(d.length());

    SVGPathByteStreamBuilder* builder = globalSVGPathByteStreamBuilder(result);

    OwnPtr<SVGPathStringSource> source = SVGPathStringSource::create(d);
    SVGPathParser* parser = globalSVGPathParser(source.get(), builder);
    bool ok = parser->parsePathDataFromSource(parsingMode);
    parser->cleanup();

    result->shrinkToFit();

    return ok;
}

bool addToSVGPathByteStream(SVGPathByteStream* fromStream, const SVGPathByteStream* byStream, unsigned repeatCount)
{
    ASSERT(fromStream);
    ASSERT(byStream);
    if (fromStream->isEmpty() || byStream->isEmpty())
        return false;

    SVGPathByteStreamBuilder* builder = globalSVGPathByteStreamBuilder(fromStream);

    OwnPtr<SVGPathByteStream> fromStreamCopy = fromStream->copy();
    fromStream->clear();

    SVGPathByteStreamSource fromSource(fromStreamCopy.get());
    SVGPathByteStreamSource bySource(byStream);
    SVGPathBlender* blender = globalSVGPathBlender();
    bool ok = blender->addAnimatedPath(&fromSource, &bySource, builder, repeatCount);
    blender->cleanup();
    return ok;
}

bool getSVGPathSegAtLengthFromSVGPathByteStream(const SVGPathByteStream* stream, float length, unsigned& pathSeg)
{
    ASSERT(stream);
    if (stream->isEmpty())
        return false;

    PathTraversalState traversalState(PathTraversalState::TraversalSegmentAtLength);
    SVGPathTraversalStateBuilder* builder = globalSVGPathTraversalStateBuilder(traversalState, length);

    SVGPathByteStreamSource source(stream);
    SVGPathParser* parser = globalSVGPathParser(&source, builder);
    bool ok = parser->parsePathDataFromSource(NormalizedParsing);
    pathSeg = builder->pathSegmentIndex();
    parser->cleanup();
    return ok;
}

bool getTotalLengthOfSVGPathByteStream(const SVGPathByteStream* stream, float& totalLength)
{
    ASSERT(stream);
    if (stream->isEmpty())
        return false;

    PathTraversalState traversalState(PathTraversalState::TraversalTotalLength);
    SVGPathTraversalStateBuilder* builder = globalSVGPathTraversalStateBuilder(traversalState, 0);

    SVGPathByteStreamSource source(stream);
    SVGPathParser* parser = globalSVGPathParser(&source, builder);
    bool ok = parser->parsePathDataFromSource(NormalizedParsing);
    totalLength = builder->totalLength();
    parser->cleanup();
    return ok;
}

bool getPointAtLengthOfSVGPathByteStream(const SVGPathByteStream* stream, float length, FloatPoint& point)
{
    ASSERT(stream);
    if (stream->isEmpty())
        return false;

    PathTraversalState traversalState(PathTraversalState::TraversalPointAtLength);
    SVGPathTraversalStateBuilder* builder = globalSVGPathTraversalStateBuilder(traversalState, length);

    SVGPathByteStreamSource source(stream);
    SVGPathParser* parser = globalSVGPathParser(&source, builder);
    bool ok = parser->parsePathDataFromSource(NormalizedParsing);
    point = builder->currentPoint();
    parser->cleanup();
    return ok;
}

}
