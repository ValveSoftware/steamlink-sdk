// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/opentype/OpenTypeCapsSupport.h"

namespace blink {

OpenTypeCapsSupport::OpenTypeCapsSupport()
    : m_harfBuzzFace(nullptr),
      m_requestedCaps(FontDescription::CapsNormal),
      m_fontSupport(FontSupport::Full),
      m_capsSynthesis(CapsSynthesis::None) {}

OpenTypeCapsSupport::OpenTypeCapsSupport(
    const HarfBuzzFace* harfBuzzFace,
    FontDescription::FontVariantCaps requestedCaps,
    hb_script_t script)
    : m_harfBuzzFace(harfBuzzFace),
      m_requestedCaps(requestedCaps),
      m_fontSupport(FontSupport::Full),
      m_capsSynthesis(CapsSynthesis::None) {
  if (requestedCaps != FontDescription::CapsNormal)
    determineFontSupport(script);
}

FontDescription::FontVariantCaps OpenTypeCapsSupport::fontFeatureToUse(
    SmallCapsIterator::SmallCapsBehavior sourceTextCase) {
  if (m_fontSupport == FontSupport::Full)
    return m_requestedCaps;

  if (m_fontSupport == FontSupport::Fallback) {
    if (m_requestedCaps == FontDescription::FontVariantCaps::AllPetiteCaps)
      return FontDescription::FontVariantCaps::AllSmallCaps;

    if (m_requestedCaps == FontDescription::FontVariantCaps::PetiteCaps ||
        (m_requestedCaps == FontDescription::FontVariantCaps::Unicase &&
         sourceTextCase == SmallCapsIterator::SmallCapsSameCase))
      return FontDescription::FontVariantCaps::SmallCaps;
  }

  return FontDescription::FontVariantCaps::CapsNormal;
}

bool OpenTypeCapsSupport::needsRunCaseSplitting() {
  // Lack of titling case support is ignored, titling case is not synthesized.
  return m_fontSupport != FontSupport::Full &&
         m_requestedCaps != FontDescription::TitlingCaps;
}

bool OpenTypeCapsSupport::needsSyntheticFont(
    SmallCapsIterator::SmallCapsBehavior runCase) {
  if (m_fontSupport == FontSupport::Full)
    return false;

  if (m_requestedCaps == FontDescription::TitlingCaps)
    return false;

  if (m_fontSupport == FontSupport::None) {
    if (runCase == SmallCapsIterator::SmallCapsUppercaseNeeded &&
        (m_capsSynthesis == CapsSynthesis::LowerToSmallCaps ||
         m_capsSynthesis == CapsSynthesis::BothToSmallCaps))
      return true;

    if (runCase == SmallCapsIterator::SmallCapsSameCase &&
        (m_capsSynthesis == CapsSynthesis::UpperToSmallCaps ||
         m_capsSynthesis == CapsSynthesis::BothToSmallCaps)) {
      return true;
    }
  }

  return false;
}

CaseMapIntend OpenTypeCapsSupport::needsCaseChange(
    SmallCapsIterator::SmallCapsBehavior runCase) {
  CaseMapIntend caseMapIntend = CaseMapIntend::KeepSameCase;

  if (m_fontSupport == FontSupport::Full)
    return caseMapIntend;

  switch (runCase) {
    case SmallCapsIterator::SmallCapsSameCase:
      caseMapIntend =
          m_fontSupport == FontSupport::Fallback &&
                  (m_capsSynthesis == CapsSynthesis::BothToSmallCaps ||
                   m_capsSynthesis == CapsSynthesis::UpperToSmallCaps)
              ? CaseMapIntend::LowerCase
              : CaseMapIntend::KeepSameCase;
      break;
    case SmallCapsIterator::SmallCapsUppercaseNeeded:
      caseMapIntend =
          m_fontSupport != FontSupport::Fallback &&
                  (m_capsSynthesis == CapsSynthesis::LowerToSmallCaps ||
                   m_capsSynthesis == CapsSynthesis::BothToSmallCaps)
              ? CaseMapIntend::UpperCase
              : CaseMapIntend::KeepSameCase;
      break;
    default:
      break;
  }
  return caseMapIntend;
}

void OpenTypeCapsSupport::determineFontSupport(hb_script_t script) {
  switch (m_requestedCaps) {
    case FontDescription::SmallCaps:
      if (!supportsOpenTypeFeature(script, HB_TAG('s', 'm', 'c', 'p'))) {
        m_fontSupport = FontSupport::None;
        m_capsSynthesis = CapsSynthesis::LowerToSmallCaps;
      }
      break;
    case FontDescription::AllSmallCaps:
      if (!(supportsOpenTypeFeature(script, HB_TAG('s', 'm', 'c', 'p')) &&
            supportsOpenTypeFeature(script, HB_TAG('c', '2', 's', 'c')))) {
        m_fontSupport = FontSupport::None;
        m_capsSynthesis = CapsSynthesis::BothToSmallCaps;
      }
      break;
    case FontDescription::PetiteCaps:
      if (!supportsOpenTypeFeature(script, HB_TAG('p', 'c', 'a', 'p'))) {
        if (supportsOpenTypeFeature(script, HB_TAG('s', 'm', 'c', 'p'))) {
          m_fontSupport = FontSupport::Fallback;
        } else {
          m_fontSupport = FontSupport::None;
          m_capsSynthesis = CapsSynthesis::LowerToSmallCaps;
        }
      }
      break;
    case FontDescription::AllPetiteCaps:
      if (!(supportsOpenTypeFeature(script, HB_TAG('p', 'c', 'a', 'p')) &&
            supportsOpenTypeFeature(script, HB_TAG('c', '2', 'p', 'c')))) {
        if (supportsOpenTypeFeature(script, HB_TAG('s', 'm', 'c', 'p')) &&
            supportsOpenTypeFeature(script, HB_TAG('c', '2', 's', 'c'))) {
          m_fontSupport = FontSupport::Fallback;
        } else {
          m_fontSupport = FontSupport::None;
          m_capsSynthesis = CapsSynthesis::BothToSmallCaps;
        }
      }
      break;
    case FontDescription::Unicase:
      if (!supportsOpenTypeFeature(script, HB_TAG('u', 'n', 'i', 'c'))) {
        m_capsSynthesis = CapsSynthesis::UpperToSmallCaps;
        if (supportsOpenTypeFeature(script, HB_TAG('s', 'm', 'c', 'p'))) {
          m_fontSupport = FontSupport::Fallback;
        } else {
          m_fontSupport = FontSupport::None;
        }
      }
      break;
    case FontDescription::TitlingCaps:
      if (!supportsOpenTypeFeature(script, HB_TAG('t', 'i', 't', 'l'))) {
        m_fontSupport = FontSupport::None;
      }
      break;
    default:
      ASSERT_NOT_REACHED();
  }
}

}  // namespace blink
