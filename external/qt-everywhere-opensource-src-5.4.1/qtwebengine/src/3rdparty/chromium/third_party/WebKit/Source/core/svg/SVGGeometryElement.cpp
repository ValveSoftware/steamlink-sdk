/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/svg/SVGGeometryElement.h"

#include "core/SVGNames.h"
#include "core/rendering/HitTestRequest.h"
#include "core/rendering/PointerEventsHitRules.h"
#include "core/rendering/svg/RenderSVGShape.h"
#include "core/svg/SVGPointTearOff.h"

namespace WebCore {

SVGGeometryElement::SVGGeometryElement(const QualifiedName& tagName, Document& document, ConstructionType constructionType)
    : SVGGraphicsElement(tagName, document, constructionType)
{
}

bool SVGGeometryElement::isPointInFill(PassRefPtr<SVGPointTearOff> point) const
{
    document().updateLayoutIgnorePendingStylesheets();

    // FIXME: Eventually we should support isPointInFill for display:none elements.
    if (!renderer() || !renderer()->isSVGShape())
        return false;

    HitTestRequest request(HitTestRequest::ReadOnly);
    PointerEventsHitRules hitRules(PointerEventsHitRules::SVG_GEOMETRY_HITTESTING, request, renderer()->style()->pointerEvents());
    hitRules.canHitStroke = false;
    return toRenderSVGShape(renderer())->nodeAtFloatPointInternal(request, point->target()->value(), hitRules);
}

bool SVGGeometryElement::isPointInStroke(PassRefPtr<SVGPointTearOff> point) const
{
    document().updateLayoutIgnorePendingStylesheets();

    // FIXME: Eventually we should support isPointInStroke for display:none elements.
    if (!renderer() || !renderer()->isSVGShape())
        return false;

    HitTestRequest request(HitTestRequest::ReadOnly);
    PointerEventsHitRules hitRules(PointerEventsHitRules::SVG_GEOMETRY_HITTESTING, request, renderer()->style()->pointerEvents());
    hitRules.canHitFill = false;
    return toRenderSVGShape(renderer())->nodeAtFloatPointInternal(request, point->target()->value(), hitRules);
}

}
