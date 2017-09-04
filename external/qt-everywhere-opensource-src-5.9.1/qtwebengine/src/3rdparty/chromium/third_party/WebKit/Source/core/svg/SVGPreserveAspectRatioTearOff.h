/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#ifndef SVGPreserveAspectRatioTearOff_h
#define SVGPreserveAspectRatioTearOff_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/svg/SVGPreserveAspectRatio.h"
#include "core/svg/properties/SVGPropertyTearOff.h"
#include "platform/heap/Handle.h"

namespace blink {

class SVGPreserveAspectRatioTearOff final
    : public SVGPropertyTearOff<SVGPreserveAspectRatio>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum {
    kSvgPreserveaspectratioUnknown =
        SVGPreserveAspectRatio::kSvgPreserveaspectratioUnknown,
    kSvgPreserveaspectratioNone =
        SVGPreserveAspectRatio::kSvgPreserveaspectratioNone,
    kSvgPreserveaspectratioXminymin =
        SVGPreserveAspectRatio::kSvgPreserveaspectratioXminymin,
    kSvgPreserveaspectratioXmidymin =
        SVGPreserveAspectRatio::kSvgPreserveaspectratioXmidymin,
    kSvgPreserveaspectratioXmaxymin =
        SVGPreserveAspectRatio::kSvgPreserveaspectratioXmaxymin,
    kSvgPreserveaspectratioXminymid =
        SVGPreserveAspectRatio::kSvgPreserveaspectratioXminymid,
    kSvgPreserveaspectratioXmidymid =
        SVGPreserveAspectRatio::kSvgPreserveaspectratioXmidymid,
    kSvgPreserveaspectratioXmaxymid =
        SVGPreserveAspectRatio::kSvgPreserveaspectratioXmaxymid,
    kSvgPreserveaspectratioXminymax =
        SVGPreserveAspectRatio::kSvgPreserveaspectratioXminymax,
    kSvgPreserveaspectratioXmidymax =
        SVGPreserveAspectRatio::kSvgPreserveaspectratioXmidymax,
    kSvgPreserveaspectratioXmaxymax =
        SVGPreserveAspectRatio::kSvgPreserveaspectratioXmaxymax
  };

  enum {
    kSvgMeetorsliceUnknown = SVGPreserveAspectRatio::kSvgMeetorsliceUnknown,
    kSvgMeetorsliceMeet = SVGPreserveAspectRatio::kSvgMeetorsliceMeet,
    kSvgMeetorsliceSlice = SVGPreserveAspectRatio::kSvgMeetorsliceSlice
  };

  static SVGPreserveAspectRatioTearOff* create(
      SVGPreserveAspectRatio* target,
      SVGElement* contextElement,
      PropertyIsAnimValType propertyIsAnimVal,
      const QualifiedName& attributeName = QualifiedName::null()) {
    return new SVGPreserveAspectRatioTearOff(target, contextElement,
                                             propertyIsAnimVal, attributeName);
  }

  void setAlign(unsigned short, ExceptionState&);
  unsigned short align() { return target()->align(); }
  void setMeetOrSlice(unsigned short, ExceptionState&);
  unsigned short meetOrSlice() { return target()->meetOrSlice(); }

  DECLARE_VIRTUAL_TRACE_WRAPPERS();

 private:
  SVGPreserveAspectRatioTearOff(
      SVGPreserveAspectRatio*,
      SVGElement* contextElement,
      PropertyIsAnimValType,
      const QualifiedName& attributeName = QualifiedName::null());
};

}  // namespace blink

#endif  // SVGPreserveAspectRatioTearOff_h
