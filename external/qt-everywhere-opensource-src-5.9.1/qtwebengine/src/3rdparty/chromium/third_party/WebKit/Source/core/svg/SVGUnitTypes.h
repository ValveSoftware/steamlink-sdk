/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef SVGUnitTypes_h
#define SVGUnitTypes_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/svg/SVGEnumeration.h"
#include "platform/heap/Handle.h"

namespace blink {

class SVGUnitTypes final : public GarbageCollected<SVGUnitTypes>,
                           public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum SVGUnitType {
    kSvgUnitTypeUnknown = 0,
    kSvgUnitTypeUserspaceonuse = 1,
    kSvgUnitTypeObjectboundingbox = 2
  };

  DEFINE_INLINE_TRACE() {}

 private:
  SVGUnitTypes();  // No instantiation.
};

template <>
const SVGEnumerationStringEntries&
getStaticStringEntries<SVGUnitTypes::SVGUnitType>();

}  // namespace blink

#endif  // SVGUnitTypes_h
