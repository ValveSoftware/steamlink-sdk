/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "bindings/core/v8/V8SVGPathSeg.h"

#include "bindings/core/v8/V8SVGPathSegArcAbs.h"
#include "bindings/core/v8/V8SVGPathSegArcRel.h"
#include "bindings/core/v8/V8SVGPathSegClosePath.h"
#include "bindings/core/v8/V8SVGPathSegCurvetoCubicAbs.h"
#include "bindings/core/v8/V8SVGPathSegCurvetoCubicRel.h"
#include "bindings/core/v8/V8SVGPathSegCurvetoCubicSmoothAbs.h"
#include "bindings/core/v8/V8SVGPathSegCurvetoCubicSmoothRel.h"
#include "bindings/core/v8/V8SVGPathSegCurvetoQuadraticAbs.h"
#include "bindings/core/v8/V8SVGPathSegCurvetoQuadraticRel.h"
#include "bindings/core/v8/V8SVGPathSegCurvetoQuadraticSmoothAbs.h"
#include "bindings/core/v8/V8SVGPathSegCurvetoQuadraticSmoothRel.h"
#include "bindings/core/v8/V8SVGPathSegLinetoAbs.h"
#include "bindings/core/v8/V8SVGPathSegLinetoHorizontalAbs.h"
#include "bindings/core/v8/V8SVGPathSegLinetoHorizontalRel.h"
#include "bindings/core/v8/V8SVGPathSegLinetoRel.h"
#include "bindings/core/v8/V8SVGPathSegLinetoVerticalAbs.h"
#include "bindings/core/v8/V8SVGPathSegLinetoVerticalRel.h"
#include "bindings/core/v8/V8SVGPathSegMovetoAbs.h"
#include "bindings/core/v8/V8SVGPathSegMovetoRel.h"
#include "bindings/core/v8/V8Window.h"
#include "bindings/v8/V8DOMWrapper.h"

namespace WebCore {

v8::Handle<v8::Object> wrap(SVGPathSeg* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(impl);
    switch (impl->pathSegType()) {
    case SVGPathSeg::PATHSEG_CLOSEPATH:
        return wrap(static_cast<SVGPathSegClosePath*>(impl), creationContext, isolate);
    case SVGPathSeg::PATHSEG_MOVETO_ABS:
        return wrap(static_cast<SVGPathSegMovetoAbs*>(impl), creationContext, isolate);
    case SVGPathSeg::PATHSEG_MOVETO_REL:
        return wrap(static_cast<SVGPathSegMovetoRel*>(impl), creationContext, isolate);
    case SVGPathSeg::PATHSEG_LINETO_ABS:
        return wrap(static_cast<SVGPathSegLinetoAbs*>(impl), creationContext, isolate);
    case SVGPathSeg::PATHSEG_LINETO_REL:
        return wrap(static_cast<SVGPathSegLinetoRel*>(impl), creationContext, isolate);
    case SVGPathSeg::PATHSEG_CURVETO_CUBIC_ABS:
        return wrap(static_cast<SVGPathSegCurvetoCubicAbs*>(impl), creationContext, isolate);
    case SVGPathSeg::PATHSEG_CURVETO_CUBIC_REL:
        return wrap(static_cast<SVGPathSegCurvetoCubicRel*>(impl), creationContext, isolate);
    case SVGPathSeg::PATHSEG_CURVETO_QUADRATIC_ABS:
        return wrap(static_cast<SVGPathSegCurvetoQuadraticAbs*>(impl), creationContext, isolate);
    case SVGPathSeg::PATHSEG_CURVETO_QUADRATIC_REL:
        return wrap(static_cast<SVGPathSegCurvetoQuadraticRel*>(impl), creationContext, isolate);
    case SVGPathSeg::PATHSEG_ARC_ABS:
        return wrap(static_cast<SVGPathSegArcAbs*>(impl), creationContext, isolate);
    case SVGPathSeg::PATHSEG_ARC_REL:
        return wrap(static_cast<SVGPathSegArcRel*>(impl), creationContext, isolate);
    case SVGPathSeg::PATHSEG_LINETO_HORIZONTAL_ABS:
        return wrap(static_cast<SVGPathSegLinetoHorizontalAbs*>(impl), creationContext, isolate);
    case SVGPathSeg::PATHSEG_LINETO_HORIZONTAL_REL:
        return wrap(static_cast<SVGPathSegLinetoHorizontalRel*>(impl), creationContext, isolate);
    case SVGPathSeg::PATHSEG_LINETO_VERTICAL_ABS:
        return wrap(static_cast<SVGPathSegLinetoVerticalAbs*>(impl), creationContext, isolate);
    case SVGPathSeg::PATHSEG_LINETO_VERTICAL_REL:
        return wrap(static_cast<SVGPathSegLinetoVerticalRel*>(impl), creationContext, isolate);
    case SVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_ABS:
        return wrap(static_cast<SVGPathSegCurvetoCubicSmoothAbs*>(impl), creationContext, isolate);
    case SVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_REL:
        return wrap(static_cast<SVGPathSegCurvetoCubicSmoothRel*>(impl), creationContext, isolate);
    case SVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS:
        return wrap(static_cast<SVGPathSegCurvetoQuadraticSmoothAbs*>(impl), creationContext, isolate);
    case SVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL:
        return wrap(static_cast<SVGPathSegCurvetoQuadraticSmoothRel*>(impl), creationContext, isolate);
    }
    ASSERT_NOT_REACHED();
    return V8SVGPathSeg::createWrapper(impl, creationContext, isolate);
}

} // namespace WebCore
