// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ENCODINGS_COMPACT_LANG_DET_WIN_CLD_UNICODETEXT_H_
#define ENCODINGS_COMPACT_LANG_DET_WIN_CLD_UNICODETEXT_H_

#include "languages/public/languages.h"
#include "unicode/utypes.h"

namespace CompactLangDet {
  struct DetectionTables;
}  // namespace CompactLangDet

// Detects a language of the UTF-16 encoded zero-terminated text.
// [in] detection_tables - internal CLD data tables (see compact_lang_det.h).
//     Can be NULL, in this case CLD will fall back to builtin static tables.
// [in] text - UTF-16 encoded text to detect a language of.
// [in] is_plain_text - true if plain text, false otherwise (e.g. HTML).
// [out] is_reliable - true, if returned language was detected reliably.
//     See compact_lang_det.h for details.
// [out] num_languages - set to the number of languages detected on the page.
//     Language counts only if it's detected in more than 20% of the text.
// [out, optional] error_code - set to 0 in case of success, Windows
//     GetLastError() code otherwise.  Pass NULL, if not interested in errors.
// See encodings/compact_lang_det/compact_lang_det.h,
//     CompactLangDet::DetectLanguage() description for other input parameters
//     description.
// Returns: Language enum.
//     Returns NUM_LANGUAGES in case of any error.
//     See googleclient/languages/internal/languages.cc
//     for details.
Language DetectLanguageOfUnicodeText(
    const CompactLangDet::DetectionTables* detection_tables,
    const UChar* text, bool is_plain_text,
    bool* is_reliable, int* num_languages,
    int* error_code, int* text_bytes);


#endif  // ENCODINGS_COMPACT_LANG_DET_WIN_CLD_UNICODETEXT_H_
