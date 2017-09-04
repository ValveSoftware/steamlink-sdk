/*
 * Copyright (C) 2002, 2003 The Karbon Developers
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007, 2009 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef SVGPathBuilder_h
#define SVGPathBuilder_h

#include "core/svg/SVGPathConsumer.h"
#include "core/svg/SVGPathData.h"
#include "platform/geometry/FloatPoint.h"

namespace blink {

class FloatSize;
class Path;

class SVGPathBuilder final : public SVGPathConsumer {
 public:
  SVGPathBuilder(Path& path) : m_path(path), m_lastCommand(PathSegUnknown) {}

  void emitSegment(const PathSegmentData&) override;

 private:
  void emitClose();
  void emitMoveTo(const FloatPoint&);
  void emitLineTo(const FloatPoint&);
  void emitQuadTo(const FloatPoint&, const FloatPoint&);
  void emitSmoothQuadTo(const FloatPoint&);
  void emitCubicTo(const FloatPoint&, const FloatPoint&, const FloatPoint&);
  void emitSmoothCubicTo(const FloatPoint&, const FloatPoint&);
  void emitArcTo(const FloatPoint&,
                 const FloatSize&,
                 float,
                 bool largeArc,
                 bool sweep);

  FloatPoint smoothControl(bool isSmooth) const;

  Path& m_path;

  SVGPathSegType m_lastCommand;
  FloatPoint m_subpathPoint;
  FloatPoint m_currentPoint;
  FloatPoint m_lastControlPoint;
};

}  // namespace blink

#endif  // SVGPathBuilder_h
