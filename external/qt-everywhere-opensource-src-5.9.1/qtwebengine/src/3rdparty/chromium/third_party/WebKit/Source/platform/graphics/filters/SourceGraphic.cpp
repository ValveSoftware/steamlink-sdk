/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "platform/graphics/filters/SourceGraphic.h"

#include "platform/graphics/filters/Filter.h"
#include "platform/text/TextStream.h"

namespace blink {

SourceGraphic::SourceGraphic(Filter* filter) : FilterEffect(filter) {
  setOperatingColorSpace(ColorSpaceDeviceRGB);
}

SourceGraphic::~SourceGraphic() {}

SourceGraphic* SourceGraphic::create(Filter* filter) {
  return new SourceGraphic(filter);
}

FloatRect SourceGraphic::mapInputs(const FloatRect& rect) const {
  return !m_sourceRect.isEmpty() ? m_sourceRect : rect;
}

void SourceGraphic::setSourceRect(const IntRect& sourceRect) {
  m_sourceRect = sourceRect;
}

TextStream& SourceGraphic::externalRepresentation(TextStream& ts,
                                                  int indent) const {
  writeIndent(ts, indent);
  ts << "[SourceGraphic]\n";
  return ts;
}

}  // namespace blink
