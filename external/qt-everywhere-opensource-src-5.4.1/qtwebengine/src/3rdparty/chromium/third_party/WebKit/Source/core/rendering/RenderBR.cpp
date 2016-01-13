/**
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2006 Apple Computer, Inc.
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
 *
 */

#include "config.h"
#include "core/rendering/RenderBR.h"

#include "core/dom/Document.h"
#include "core/rendering/RenderView.h"

namespace WebCore {

static PassRefPtr<StringImpl> newlineString()
{
    DEFINE_STATIC_LOCAL(const String, string, ("\n"));
    return string.impl();
}

RenderBR::RenderBR(Node* node)
    : RenderText(node, newlineString())
{
}

RenderBR::~RenderBR()
{
}

int RenderBR::lineHeight(bool firstLine) const
{
    RenderStyle* s = style(firstLine && document().styleEngine()->usesFirstLineRules());
    return s->computedLineHeight();
}

void RenderBR::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderText::styleDidChange(diff, oldStyle);
}

int RenderBR::caretMinOffset() const
{
    return 0;
}

int RenderBR::caretMaxOffset() const
{
    return 1;
}

PositionWithAffinity RenderBR::positionForPoint(const LayoutPoint&)
{
    return createPositionWithAffinity(0, DOWNSTREAM);
}

} // namespace WebCore
