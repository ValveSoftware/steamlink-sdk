// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OpenTypeCapsSupport_h
#define OpenTypeCapsSupport_h

#include "platform/fonts/FontDescription.h"
#include "platform/fonts/SmallCapsIterator.h"
#include "platform/fonts/opentype/OpenTypeCapsSupport.h"
#include "platform/fonts/shaping/CaseMappingHarfBuzzBufferFiller.h"
#include "platform/fonts/shaping/HarfBuzzFace.h"

#include <hb.h>

namespace blink {

class PLATFORM_EXPORT OpenTypeCapsSupport {
 public:
  OpenTypeCapsSupport();
  OpenTypeCapsSupport(const HarfBuzzFace*,
                      FontDescription::FontVariantCaps requestedCaps,
                      hb_script_t);

  bool needsRunCaseSplitting();
  bool needsSyntheticFont(SmallCapsIterator::SmallCapsBehavior runCase);
  FontDescription::FontVariantCaps fontFeatureToUse(
      SmallCapsIterator::SmallCapsBehavior runCase);
  CaseMapIntend needsCaseChange(SmallCapsIterator::SmallCapsBehavior runCase);

 private:
  void determineFontSupport(hb_script_t);
  bool supportsOpenTypeFeature(hb_script_t, uint32_t tag) const;

  const HarfBuzzFace* m_harfBuzzFace;
  FontDescription::FontVariantCaps m_requestedCaps;
  SmallCapsIterator::SmallCapsBehavior m_runCase;

  enum class FontSupport {
    Full,
    Fallback,  // Fall back to 'smcp' or 'smcp' + 'c2sc'
    None
  };

  enum class CapsSynthesis {
    None,
    LowerToSmallCaps,
    UpperToSmallCaps,
    BothToSmallCaps
  };

  FontSupport m_fontSupport;
  CapsSynthesis m_capsSynthesis;
};

};  // namespace blink

#endif
