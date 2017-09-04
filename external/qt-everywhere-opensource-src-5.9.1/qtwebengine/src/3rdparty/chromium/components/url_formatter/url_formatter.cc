// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_formatter/url_formatter.h"

#include <algorithm>
#include <utility>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_local_storage.h"
#include "third_party/icu/source/common/unicode/schriter.h"
#include "third_party/icu/source/common/unicode/uidna.h"
#include "third_party/icu/source/common/unicode/uniset.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "third_party/icu/source/common/unicode/uvernum.h"
#include "third_party/icu/source/i18n/unicode/regex.h"
#include "third_party/icu/source/i18n/unicode/uspoof.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"

namespace url_formatter {

namespace {

base::string16 IDNToUnicodeWithAdjustments(
    base::StringPiece host,
    base::OffsetAdjuster::Adjustments* adjustments);
bool IDNToUnicodeOneComponent(const base::char16* comp,
                              size_t comp_len,
                              bool is_tld_ascii,
                              base::string16* out);

class AppendComponentTransform {
 public:
  AppendComponentTransform() {}
  virtual ~AppendComponentTransform() {}

  virtual base::string16 Execute(
      const std::string& component_text,
      base::OffsetAdjuster::Adjustments* adjustments) const = 0;

  // NOTE: No DISALLOW_COPY_AND_ASSIGN here, since gcc < 4.3.0 requires an
  // accessible copy constructor in order to call AppendFormattedComponent()
  // with an inline temporary (see http://gcc.gnu.org/bugs/#cxx%5Frvalbind ).
};

class HostComponentTransform : public AppendComponentTransform {
 public:
  HostComponentTransform() {}

 private:
  base::string16 Execute(
      const std::string& component_text,
      base::OffsetAdjuster::Adjustments* adjustments) const override {
    return IDNToUnicodeWithAdjustments(component_text, adjustments);
  }
};

class NonHostComponentTransform : public AppendComponentTransform {
 public:
  explicit NonHostComponentTransform(net::UnescapeRule::Type unescape_rules)
      : unescape_rules_(unescape_rules) {}

 private:
  base::string16 Execute(
      const std::string& component_text,
      base::OffsetAdjuster::Adjustments* adjustments) const override {
    return (unescape_rules_ == net::UnescapeRule::NONE)
               ? base::UTF8ToUTF16WithAdjustments(component_text, adjustments)
               : net::UnescapeAndDecodeUTF8URLComponentWithAdjustments(
                     component_text, unescape_rules_, adjustments);
  }

  const net::UnescapeRule::Type unescape_rules_;
};

// Transforms the portion of |spec| covered by |original_component| according to
// |transform|.  Appends the result to |output|.  If |output_component| is
// non-NULL, its start and length are set to the transformed component's new
// start and length.  If |adjustments| is non-NULL, appends adjustments (if
// any) that reflect the transformation the original component underwent to
// become the transformed value appended to |output|.
void AppendFormattedComponent(const std::string& spec,
                              const url::Component& original_component,
                              const AppendComponentTransform& transform,
                              base::string16* output,
                              url::Component* output_component,
                              base::OffsetAdjuster::Adjustments* adjustments) {
  DCHECK(output);
  if (original_component.is_nonempty()) {
    size_t original_component_begin =
        static_cast<size_t>(original_component.begin);
    size_t output_component_begin = output->length();
    std::string component_str(spec, original_component_begin,
                              static_cast<size_t>(original_component.len));

    // Transform |component_str| and modify |adjustments| appropriately.
    base::OffsetAdjuster::Adjustments component_transform_adjustments;
    output->append(
        transform.Execute(component_str, &component_transform_adjustments));

    // Shift all the adjustments made for this component so the offsets are
    // valid for the original string and add them to |adjustments|.
    for (base::OffsetAdjuster::Adjustments::iterator comp_iter =
             component_transform_adjustments.begin();
         comp_iter != component_transform_adjustments.end(); ++comp_iter)
      comp_iter->original_offset += original_component_begin;
    if (adjustments) {
      adjustments->insert(adjustments->end(),
                          component_transform_adjustments.begin(),
                          component_transform_adjustments.end());
    }

    // Set positions of the parsed component.
    if (output_component) {
      output_component->begin = static_cast<int>(output_component_begin);
      output_component->len =
          static_cast<int>(output->length() - output_component_begin);
    }
  } else if (output_component) {
    output_component->reset();
  }
}

// If |component| is valid, its begin is incremented by |delta|.
void AdjustComponent(int delta, url::Component* component) {
  if (!component->is_valid())
    return;

  DCHECK(delta >= 0 || component->begin >= -delta);
  component->begin += delta;
}

// Adjusts all the components of |parsed| by |delta|, except for the scheme.
void AdjustAllComponentsButScheme(int delta, url::Parsed* parsed) {
  AdjustComponent(delta, &(parsed->username));
  AdjustComponent(delta, &(parsed->password));
  AdjustComponent(delta, &(parsed->host));
  AdjustComponent(delta, &(parsed->port));
  AdjustComponent(delta, &(parsed->path));
  AdjustComponent(delta, &(parsed->query));
  AdjustComponent(delta, &(parsed->ref));
}

// Helper for FormatUrlWithOffsets().
base::string16 FormatViewSourceUrl(
    const GURL& url,
    FormatUrlTypes format_types,
    net::UnescapeRule::Type unescape_rules,
    url::Parsed* new_parsed,
    size_t* prefix_end,
    base::OffsetAdjuster::Adjustments* adjustments) {
  DCHECK(new_parsed);
  const char kViewSource[] = "view-source:";
  const size_t kViewSourceLength = arraysize(kViewSource) - 1;

  // Format the underlying URL and record adjustments.
  const std::string& url_str(url.possibly_invalid_spec());
  adjustments->clear();
  base::string16 result(
      base::ASCIIToUTF16(kViewSource) +
      FormatUrlWithAdjustments(GURL(url_str.substr(kViewSourceLength)),
                               format_types, unescape_rules, new_parsed,
                               prefix_end, adjustments));
  // Revise |adjustments| by shifting to the offsets to prefix that the above
  // call to FormatUrl didn't get to see.
  for (base::OffsetAdjuster::Adjustments::iterator it = adjustments->begin();
       it != adjustments->end(); ++it)
    it->original_offset += kViewSourceLength;

  // Adjust positions of the parsed components.
  if (new_parsed->scheme.is_nonempty()) {
    // Assume "view-source:real-scheme" as a scheme.
    new_parsed->scheme.len += kViewSourceLength;
  } else {
    new_parsed->scheme.begin = 0;
    new_parsed->scheme.len = kViewSourceLength - 1;
  }
  AdjustAllComponentsButScheme(kViewSourceLength, new_parsed);

  if (prefix_end)
    *prefix_end += kViewSourceLength;

  return result;
}

// TODO(brettw): We may want to skip this step in the case of file URLs to
// allow unicode UNC hostnames regardless of encodings.
base::string16 IDNToUnicodeWithAdjustments(
    base::StringPiece host, base::OffsetAdjuster::Adjustments* adjustments) {
  if (adjustments)
    adjustments->clear();
  // Convert the ASCII input to a base::string16 for ICU.
  base::string16 input16;
  input16.reserve(host.length());
  input16.insert(input16.end(), host.begin(), host.end());

  bool is_tld_ascii = true;
  size_t last_dot = host.rfind('.');
  if (last_dot != base::StringPiece::npos &&
      host.substr(last_dot).starts_with(".xn--")) {
    is_tld_ascii = false;
  }

  // Do each component of the host separately, since we enforce script matching
  // on a per-component basis.
  base::string16 out16;
  for (size_t component_start = 0, component_end;
       component_start < input16.length();
       component_start = component_end + 1) {
    // Find the end of the component.
    component_end = input16.find('.', component_start);
    if (component_end == base::string16::npos)
      component_end = input16.length();  // For getting the last component.
    size_t component_length = component_end - component_start;
    size_t new_component_start = out16.length();
    bool converted_idn = false;
    if (component_end > component_start) {
      // Add the substring that we just found.
      converted_idn =
          IDNToUnicodeOneComponent(input16.data() + component_start,
                                   component_length, is_tld_ascii, &out16);
    }
    size_t new_component_length = out16.length() - new_component_start;

    if (converted_idn && adjustments) {
      adjustments->push_back(base::OffsetAdjuster::Adjustment(
          component_start, component_length, new_component_length));
    }

    // Need to add the dot we just found (if we found one).
    if (component_end < input16.length())
      out16.push_back('.');
  }
  return out16;
}

// A helper class for IDN Spoof checking, used to ensure that no IDN input is
// spoofable per Chromium's standard of spoofability. For a more thorough
// explanation of how spoof checking works in Chromium, see
// http://dev.chromium.org/developers/design-documents/idn-in-google-chrome .
class IDNSpoofChecker {
 public:
  IDNSpoofChecker();

  // Returns true if |label| is safe to display as Unicode. When the TLD is
  // ASCII, check if a label is entirely made of Cyrillic letters that look like
  // Latin letters. In the event of library failure, all IDN inputs will be
  // treated as unsafe.
  bool Check(base::StringPiece16 label, bool is_tld_ascii);

 private:
  void SetAllowedUnicodeSet(UErrorCode* status);
  bool IsMadeOfLatinAlikeCyrillic(const icu::UnicodeString& label_string);

  USpoofChecker* checker_;
  icu::UnicodeSet deviation_characters_;
  icu::UnicodeSet non_ascii_latin_letters_;
  icu::UnicodeSet kana_letters_exceptions_;
  icu::UnicodeSet cyrillic_letters_;
  icu::UnicodeSet cyrillic_letters_latin_alike_;

  DISALLOW_COPY_AND_ASSIGN(IDNSpoofChecker);
};

base::LazyInstance<IDNSpoofChecker>::Leaky g_idn_spoof_checker =
    LAZY_INSTANCE_INITIALIZER;
base::ThreadLocalStorage::StaticSlot tls_index = TLS_INITIALIZER;

void OnThreadTermination(void* regex_matcher) {
  delete reinterpret_cast<icu::RegexMatcher*>(regex_matcher);
}

IDNSpoofChecker::IDNSpoofChecker() {
  UErrorCode status = U_ZERO_ERROR;
  checker_ = uspoof_open(&status);
  if (U_FAILURE(status)) {
    checker_ = nullptr;
    return;
  }

  // At this point, USpoofChecker has all the checks enabled except
  // for USPOOF_CHAR_LIMIT (USPOOF_{RESTRICTION_LEVEL, INVISIBLE,
  // MIXED_SCRIPT_CONFUSABLE, WHOLE_SCRIPT_CONFUSABLE, MIXED_NUMBERS, ANY_CASE})
  // This default configuration is adjusted below as necessary.

  // Set the restriction level to moderate. It allows mixing Latin with another
  // script (+ COMMON and INHERITED). Except for Chinese(Han + Bopomofo),
  // Japanese(Hiragana + Katakana + Han), and Korean(Hangul + Han), only one
  // script other than Common and Inherited can be mixed with Latin. Cyrillic
  // and Greek are not allowed to mix with Latin.
  // See http://www.unicode.org/reports/tr39/#Restriction_Level_Detection
  uspoof_setRestrictionLevel(checker_, USPOOF_MODERATELY_RESTRICTIVE);

  // Restrict allowed characters in IDN labels and turn on USPOOF_CHAR_LIMIT.
  SetAllowedUnicodeSet(&status);

  // Enable the return of auxillary (non-error) information.
  // We used to disable WHOLE_SCRIPT_CONFUSABLE check explicitly, but as of
  // ICU 58.1, WSC is a no-op in a single string check API.
  int32_t checks = uspoof_getChecks(checker_, &status) | USPOOF_AUX_INFO;
  uspoof_setChecks(checker_, checks, &status);

  // Four characters handled differently by IDNA 2003 and IDNA 2008. UTS46
  // transitional processing treats them as IDNA 2003 does; maps U+00DF and
  // U+03C2 and drops U+200[CD].
  deviation_characters_ =
      icu::UnicodeSet(UNICODE_STRING_SIMPLE("[\\u00df\\u03c2\\u200c\\u200d]"),
                      status);
  deviation_characters_.freeze();

  // Latin letters outside ASCII. 'Script_Extensions=Latin' is not necessary
  // because additional characters pulled in with scx=Latn are not included in
  // the allowed set.
  non_ascii_latin_letters_ = icu::UnicodeSet(
      UNICODE_STRING_SIMPLE("[[:Latin:] - [a-zA-Z]]"), status);
  non_ascii_latin_letters_.freeze();

  // These letters are parts of |dangerous_patterns_|.
  kana_letters_exceptions_ = icu::UnicodeSet(UNICODE_STRING_SIMPLE(
      "[\\u3078-\\u307a\\u30d8-\\u30da\\u30fb\\u30fc]"), status);
  kana_letters_exceptions_.freeze();

  // These Cyrillic letters look like Latin. A domain label entirely made of
  // these letters is blocked as a simpliified whole-script-spoofable.
  cyrillic_letters_latin_alike_ =
      icu::UnicodeSet(icu::UnicodeString("[асԁеһіјӏорԛѕԝхуъЬҽпгѵѡ]"), status);
  cyrillic_letters_latin_alike_.freeze();

  cyrillic_letters_ =
      icu::UnicodeSet(UNICODE_STRING_SIMPLE("[[:Cyrl:]]"), status);
  cyrillic_letters_.freeze();

  DCHECK(U_SUCCESS(status));
}

bool IDNSpoofChecker::Check(base::StringPiece16 label, bool is_tld_ascii) {
  UErrorCode status = U_ZERO_ERROR;
  int32_t result = uspoof_check(checker_, (const UChar*)label.data(),
                                base::checked_cast<int32_t>(label.size()),
                                NULL, &status);
  // If uspoof_check fails (due to library failure), or if any of the checks
  // fail, treat the IDN as unsafe.
  if (U_FAILURE(status) || (result & USPOOF_ALL_CHECKS))
    return false;

  icu::UnicodeString label_string(FALSE, (const UChar*)label.data(),
                                  base::checked_cast<int32_t>(label.size()));

  // A punycode label with 'xn--' prefix is not subject to the URL
  // canonicalization and is stored as it is in GURL. If it encodes a deviation
  // character (UTS 46; e.g. U+00DF/sharp-s), it should be still shown in
  // punycode instead of Unicode. Without this check, xn--fu-hia for
  // 'fu<sharp-s>' would be converted to 'fu<sharp-s>' for display because
  // "UTS 46 section 4 Processing step 4" applies validity criteria for
  // non-transitional processing (i.e. do not map deviation characters) to any
  // punycode labels regardless of whether transitional or non-transitional is
  // chosen. On the other hand, 'fu<sharp-s>' typed or copy and pasted
  // as Unicode would be canonicalized to 'fuss' by GURL and is displayed as
  // such. See http://crbug.com/595263 .
  if (deviation_characters_.containsSome(label_string))
    return false;

  // If there's no script mixing, the input is regarded as safe without any
  // extra check unless it contains Kana letter exceptions or it's made entirely
  // of Cyrillic letters that look like Latin letters. Note that the following
  // combinations of scripts are treated as a 'logical' single script.
  //  - Chinese: Han, Bopomofo, Common
  //  - Japanese: Han, Hiragana, Katakana, Common
  //  - Korean: Hangul, Han, Common
  result &= USPOOF_RESTRICTION_LEVEL_MASK;
  if (result == USPOOF_ASCII) return true;
  if (result == USPOOF_SINGLE_SCRIPT_RESTRICTIVE &&
      kana_letters_exceptions_.containsNone(label_string)) {
    // Check Cyrillic confusable only for ASCII TLDs.
    return !is_tld_ascii || !IsMadeOfLatinAlikeCyrillic(label_string);
  }

  // Additional checks for |label| with multiple scripts, one of which is Latin.
  // Disallow non-ASCII Latin letters to mix with a non-Latin script.
  if (non_ascii_latin_letters_.containsSome(label_string))
    return false;

  if (!tls_index.initialized())
    tls_index.Initialize(&OnThreadTermination);
  icu::RegexMatcher* dangerous_pattern =
      reinterpret_cast<icu::RegexMatcher*>(tls_index.Get());
  if (!dangerous_pattern) {
    // Disallow the katakana no, so, zo, or n, as they may be mistaken for
    // slashes when they're surrounded by non-Japanese scripts (i.e. scripts
    // other than Katakana, Hiragana or Han). If {no, so, zo, n} next to a
    // non-Japanese script on either side is disallowed, legitimate cases like
    // '{vitamin in Katakana}b6' are blocked. Note that trying to block those
    // characters when used alone as a label is futile because those cases
    // would not reach here.
    // Also disallow what used to be blocked by mixed-script-confusable (MSC)
    // detection. ICU 58 does not detect MSC any more for a single input string.
    // See http://bugs.icu-project.org/trac/ticket/12823 .
    // TODO(jshin): adjust the pattern once the above ICU bug is fixed.
    // - Disallow U+30FB (Katakana Middle Dot) and U+30FC (Hiragana-Katakana
    //   Prolonged Sound) used out-of-context.
    // - Disallow three Hiragana letters (U+307[8-A]) or Katakana letters
    //   (U+30D[8-A]) that look exactly like each other when they're used in a
    //   label otherwise entirely in Katakna or Hiragana.
    // - Disallow U+0585 (Armenian Small Letter Oh) and U+0581 (Armenian Small
    //   Letter Co) to be next to Latin.
    // - Disallow Latin 'o' and 'g' next to Armenian.
    // - Disalow mixing of Latin and Canadian Syllabary.
    dangerous_pattern = new icu::RegexMatcher(
        icu::UnicodeString(
            "[^\\p{scx=kana}\\p{scx=hira}\\p{scx=hani}]"
            "[\\u30ce\\u30f3\\u30bd\\u30be]"
            "[^\\p{scx=kana}\\p{scx=hira}\\p{scx=hani}]|"
            "[^\\p{scx=kana}\\p{scx=hira}]\\u30fc|"
            "\\u30fc[^\\p{scx=kana}\\p{scx=hira}]|"
            "^[\\p{scx=kana}]+[\\u3078-\\u307a][\\p{scx=kana}]+$|"
            "^[\\p{scx=hira}]+[\\u30d8-\\u30da][\\p{scx=hira}]+$|"
            "[a-z]\\u30fb|\\u30fb[a-z]|"
            "^[\\u0585\\u0581]+[a-z]|[a-z][\\u0585\\u0581]+$|"
            "[a-z][\\u0585\\u0581]+[a-z]|"
            "^[og]+[\\p{scx=armn}]|[\\p{scx=armn}][og]+$|"
            "[\\p{scx=armn}][og]+[\\p{scx=armn}]|"
            "[\\p{sc=cans}].*[a-z]|[a-z].*[\\p{sc=cans}]",
            -1, US_INV),
        0, status);
    tls_index.Set(dangerous_pattern);
  }
  dangerous_pattern->reset(label_string);
  return !dangerous_pattern->find();
}

bool IDNSpoofChecker::IsMadeOfLatinAlikeCyrillic(
    const icu::UnicodeString& label_string) {
  // Collect all the Cyrillic letters in |label_string| and see if they're
  // a subset of |cyrillic_letters_latin_alike_|.
  // A shortcut of defining cyrillic_letters_latin_alike_ to include [0-9] and
  // [_-] and checking if the set contains all letters of |label_string|
  // would work in most cases, but not if a label has non-letters outside
  // ASCII.
  icu::UnicodeSet cyrillic_in_label;
  icu::StringCharacterIterator it(label_string);
  for (it.setToStart(); it.hasNext();) {
    const UChar32 c = it.next32PostInc();
    if (cyrillic_letters_.contains(c))
      cyrillic_in_label.add(c);
  }
  return !cyrillic_in_label.isEmpty() &&
         cyrillic_letters_latin_alike_.containsAll(cyrillic_in_label);
}

void IDNSpoofChecker::SetAllowedUnicodeSet(UErrorCode* status) {
  if (U_FAILURE(*status))
    return;

  // The recommended set is a set of characters for identifiers in a
  // security-sensitive environment taken from UTR 39
  // (http://unicode.org/reports/tr39/) and
  // http://www.unicode.org/Public/security/latest/xidmodifications.txt .
  // The inclusion set comes from "Candidate Characters for Inclusion
  // in idenfiers" of UTR 31 (http://www.unicode.org/reports/tr31). The list
  // may change over the time and will be updated whenever the version of ICU
  // used in Chromium is updated.
  const icu::UnicodeSet* recommended_set =
      uspoof_getRecommendedUnicodeSet(status);
  icu::UnicodeSet allowed_set;
  allowed_set.addAll(*recommended_set);
  const icu::UnicodeSet* inclusion_set = uspoof_getInclusionUnicodeSet(status);
  allowed_set.addAll(*inclusion_set);

  // Five aspirational scripts are taken from UTR 31 Table 6 at
  // http://www.unicode.org/reports/tr31/#Aspirational_Use_Scripts .
  // Not all the characters of aspirational scripts are suitable for
  // identifiers. Therefore, only characters belonging to
  // [:Identifier_Type=Aspirational:] (listed in 'Status/Type=Aspirational'
  // section at
  // http://www.unicode.org/Public/security/latest/xidmodifications.txt) are
  // are added to the allowed set. The list has to be updated when a new
  // version of Unicode is released. The current version is 9.0.0 and ICU 60
  // will have Unicode 10.0 data.
#if U_ICU_VERSION_MAJOR_NUM < 60
  const icu::UnicodeSet aspirational_scripts(
      icu::UnicodeString(
          // Unified Canadian Syllabics
          "[\\u1401-\\u166C\\u166F-\\u167F"
          // Mongolian
          "\\u1810-\\u1819\\u1820-\\u1877\\u1880-\\u18AA"
          // Unified Canadian Syllabics
          "\\u18B0-\\u18F5"
          // Tifinagh
          "\\u2D30-\\u2D67\\u2D7F"
          // Yi
          "\\uA000-\\uA48C"
          // Miao
          "\\U00016F00-\\U00016F44\\U00016F50-\\U00016F7E"
          "\\U00016F8F-\\U00016F9F]",
          -1, US_INV),
      *status);
  allowed_set.addAll(aspirational_scripts);
#else
#error "Update aspirational_scripts per Unicode 10.0"
#endif

  // U+0338 is included in the recommended set, while U+05F4 and U+2027 are in
  // the inclusion set. However, they are blacklisted as a part of Mozilla's
  // IDN blacklist (http://kb.mozillazine.org/Network.IDN.blacklist_chars).
  // U+2010 is in the inclusion set, but we drop it because it can be confused
  // with an ASCII U+002D (Hyphen-Minus).
  // U+0338 and U+2027 are dropped; the former can look like a slash when
  // rendered with a broken font, and the latter can be confused with U+30FB
  // (Katakana Middle Dot). U+05F4 (Hebrew Punctuation Gershayim) is kept,
  // even though it can look like a double quotation mark. Using it in Hebrew
  // should be safe. When used with a non-Hebrew script, it'd be filtered by
  // other checks in place.
  allowed_set.remove(0x338u);   // Combining Long Solidus Overlay
  allowed_set.remove(0x2010u);  // Hyphen
  allowed_set.remove(0x2027u);  // Hyphenation Point

#if defined(OS_MACOSX)
  // The following characters are reported as present in the default macOS
  // system UI font, but they render as blank. Remove them from the allowed
  // set to prevent spoofing.
  // Tibetan characters used for transliteration of ancient texts:
  allowed_set.remove(0x0F8Cu);
  allowed_set.remove(0x0F8Du);
  allowed_set.remove(0x0F8Eu);
  allowed_set.remove(0x0F8Fu);
#endif

  uspoof_setAllowedUnicodeSet(checker_, &allowed_set, status);
}

// Returns true if the given Unicode host component is safe to display to the
// user. Note that this function does not deal with pure ASCII domain labels at
// all even though it's possible to make up look-alike labels with ASCII
// characters alone.
bool IsIDNComponentSafe(base::StringPiece16 label, bool is_tld_ascii) {
  return g_idn_spoof_checker.Get().Check(label, is_tld_ascii);
}

// A wrapper to use LazyInstance<>::Leaky with ICU's UIDNA, a C pointer to
// a UTS46/IDNA 2008 handling object opened with uidna_openUTS46().
//
// We use UTS46 with BiDiCheck to migrate from IDNA 2003 to IDNA 2008 with the
// backward compatibility in mind. What it does:
//
// 1. Use the up-to-date Unicode data.
// 2. Define a case folding/mapping with the up-to-date Unicode data as in
//    IDNA 2003.
// 3. Use transitional mechanism for 4 deviation characters (sharp-s,
//    final sigma, ZWJ and ZWNJ) for now.
// 4. Continue to allow symbols and punctuations.
// 5. Apply new BiDi check rules more permissive than the IDNA 2003 BiDI rules.
// 6. Do not apply STD3 rules
// 7. Do not allow unassigned code points.
//
// It also closely matches what IE 10 does except for the BiDi check (
// http://goo.gl/3XBhqw ).
// See http://http://unicode.org/reports/tr46/ and references therein/ for more
// details.
struct UIDNAWrapper {
  UIDNAWrapper() {
    UErrorCode err = U_ZERO_ERROR;
    // TODO(jungshik): Change options as different parties (browsers,
    // registrars, search engines) converge toward a consensus.
    value = uidna_openUTS46(UIDNA_CHECK_BIDI, &err);
    if (U_FAILURE(err))
      value = NULL;
  }

  UIDNA* value;
};

base::LazyInstance<UIDNAWrapper>::Leaky g_uidna = LAZY_INSTANCE_INITIALIZER;

// Converts one component (label) of a host (between dots) to Unicode if safe.
// The result will be APPENDED to the given output string and will be the
// same as the input if it is not IDN in ACE/punycode or the IDN is unsafe to
// display.
// Returns whether any conversion was performed.
bool IDNToUnicodeOneComponent(const base::char16* comp,
                              size_t comp_len,
                              bool is_tld_ascii,
                              base::string16* out) {
  DCHECK(out);
  if (comp_len == 0)
    return false;

  // Only transform if the input can be an IDN component.
  static const base::char16 kIdnPrefix[] = {'x', 'n', '-', '-'};
  if ((comp_len > arraysize(kIdnPrefix)) &&
      !memcmp(comp, kIdnPrefix, sizeof(kIdnPrefix))) {
    UIDNA* uidna = g_uidna.Get().value;
    DCHECK(uidna != NULL);
    size_t original_length = out->length();
    int32_t output_length = 64;
    UIDNAInfo info = UIDNA_INFO_INITIALIZER;
    UErrorCode status;
    do {
      out->resize(original_length + output_length);
      status = U_ZERO_ERROR;
      // This returns the actual length required. If this is more than 64
      // code units, |status| will be U_BUFFER_OVERFLOW_ERROR and we'll try
      // the conversion again, but with a sufficiently large buffer.
      output_length = uidna_labelToUnicode(
          uidna, (const UChar*)comp, static_cast<int32_t>(comp_len), (UChar*)&(*out)[original_length],
          output_length, &info, &status);
    } while ((status == U_BUFFER_OVERFLOW_ERROR && info.errors == 0));

    if (U_SUCCESS(status) && info.errors == 0) {
      // Converted successfully. Ensure that the converted component
      // can be safely displayed to the user.
      out->resize(original_length + output_length);
      if (IsIDNComponentSafe(
              base::StringPiece16(out->data() + original_length,
                                  base::checked_cast<size_t>(output_length)),
              is_tld_ascii))
        return true;
    }

    // Something went wrong. Revert to original string.
    out->resize(original_length);
  }

  // We get here with no IDN or on error, in which case we just append the
  // literal input.
  out->append(comp, comp_len);
  return false;
}

}  // namespace

const FormatUrlType kFormatUrlOmitNothing = 0;
const FormatUrlType kFormatUrlOmitUsernamePassword = 1 << 0;
const FormatUrlType kFormatUrlOmitHTTP = 1 << 1;
const FormatUrlType kFormatUrlOmitTrailingSlashOnBareHostname = 1 << 2;
const FormatUrlType kFormatUrlOmitAll =
    kFormatUrlOmitUsernamePassword | kFormatUrlOmitHTTP |
    kFormatUrlOmitTrailingSlashOnBareHostname;

base::string16 FormatUrl(const GURL& url,
                         FormatUrlTypes format_types,
                         net::UnescapeRule::Type unescape_rules,
                         url::Parsed* new_parsed,
                         size_t* prefix_end,
                         size_t* offset_for_adjustment) {
  std::vector<size_t> offsets;
  if (offset_for_adjustment)
    offsets.push_back(*offset_for_adjustment);
  base::string16 result =
      FormatUrlWithOffsets(url, format_types, unescape_rules, new_parsed,
                           prefix_end, &offsets);
  if (offset_for_adjustment)
    *offset_for_adjustment = offsets[0];
  return result;
}

base::string16 FormatUrlWithOffsets(
    const GURL& url,
    FormatUrlTypes format_types,
    net::UnescapeRule::Type unescape_rules,
    url::Parsed* new_parsed,
    size_t* prefix_end,
    std::vector<size_t>* offsets_for_adjustment) {
  base::OffsetAdjuster::Adjustments adjustments;
  const base::string16& format_url_return_value =
      FormatUrlWithAdjustments(url, format_types, unescape_rules, new_parsed,
                               prefix_end, &adjustments);
  base::OffsetAdjuster::AdjustOffsets(adjustments, offsets_for_adjustment);
  if (offsets_for_adjustment) {
    std::for_each(
        offsets_for_adjustment->begin(), offsets_for_adjustment->end(),
        base::LimitOffset<std::string>(format_url_return_value.length()));
  }
  return format_url_return_value;
}

base::string16 FormatUrlWithAdjustments(
    const GURL& url,
    FormatUrlTypes format_types,
    net::UnescapeRule::Type unescape_rules,
    url::Parsed* new_parsed,
    size_t* prefix_end,
    base::OffsetAdjuster::Adjustments* adjustments) {
  DCHECK(adjustments != NULL);
  adjustments->clear();
  url::Parsed parsed_temp;
  if (!new_parsed)
    new_parsed = &parsed_temp;
  else
    *new_parsed = url::Parsed();

  // Special handling for view-source:.  Don't use content::kViewSourceScheme
  // because this library shouldn't depend on chrome.
  const char kViewSource[] = "view-source";
  // Reject "view-source:view-source:..." to avoid deep recursion.
  const char kViewSourceTwice[] = "view-source:view-source:";
  if (url.SchemeIs(kViewSource) &&
      !base::StartsWith(url.possibly_invalid_spec(), kViewSourceTwice,
                        base::CompareCase::INSENSITIVE_ASCII)) {
    return FormatViewSourceUrl(url, format_types, unescape_rules,
                               new_parsed, prefix_end, adjustments);
  }

  // We handle both valid and invalid URLs (this will give us the spec
  // regardless of validity).
  const std::string& spec = url.possibly_invalid_spec();
  const url::Parsed& parsed = url.parsed_for_possibly_invalid_spec();

  // Scheme & separators.  These are ASCII.
  base::string16 url_string;
  url_string.insert(
      url_string.end(), spec.begin(),
      spec.begin() + parsed.CountCharactersBefore(url::Parsed::USERNAME, true));
  const char kHTTP[] = "http://";
  const char kFTP[] = "ftp.";
  // url_formatter::FixupURL() treats "ftp.foo.com" as ftp://ftp.foo.com.  This
  // means that if we trim "http://" off a URL whose host starts with "ftp." and
  // the user inputs this into any field subject to fixup (which is basically
  // all input fields), the meaning would be changed.  (In fact, often the
  // formatted URL is directly pre-filled into an input field.)  For this reason
  // we avoid stripping "http://" in this case.
  bool omit_http =
      (format_types & kFormatUrlOmitHTTP) &&
      base::EqualsASCII(url_string, kHTTP) &&
      !base::StartsWith(url.host(), kFTP, base::CompareCase::SENSITIVE);
  new_parsed->scheme = parsed.scheme;

  // Username & password.
  if ((format_types & kFormatUrlOmitUsernamePassword) != 0) {
    // Remove the username and password fields. We don't want to display those
    // to the user since they can be used for attacks,
    // e.g. "http://google.com:search@evil.ru/"
    new_parsed->username.reset();
    new_parsed->password.reset();
    // Update the adjustments based on removed username and/or password.
    if (parsed.username.is_nonempty() || parsed.password.is_nonempty()) {
      if (parsed.username.is_nonempty() && parsed.password.is_nonempty()) {
        // The seeming off-by-two is to account for the ':' after the username
        // and '@' after the password.
        adjustments->push_back(base::OffsetAdjuster::Adjustment(
            static_cast<size_t>(parsed.username.begin),
            static_cast<size_t>(parsed.username.len + parsed.password.len + 2),
            0));
      } else {
        const url::Component* nonempty_component =
            parsed.username.is_nonempty() ? &parsed.username : &parsed.password;
        // The seeming off-by-one is to account for the '@' after the
        // username/password.
        adjustments->push_back(base::OffsetAdjuster::Adjustment(
            static_cast<size_t>(nonempty_component->begin),
            static_cast<size_t>(nonempty_component->len + 1), 0));
      }
    }
  } else {
    AppendFormattedComponent(spec, parsed.username,
                             NonHostComponentTransform(unescape_rules),
                             &url_string, &new_parsed->username, adjustments);
    if (parsed.password.is_valid())
      url_string.push_back(':');
    AppendFormattedComponent(spec, parsed.password,
                             NonHostComponentTransform(unescape_rules),
                             &url_string, &new_parsed->password, adjustments);
    if (parsed.username.is_valid() || parsed.password.is_valid())
      url_string.push_back('@');
  }
  if (prefix_end)
    *prefix_end = static_cast<size_t>(url_string.length());

  // Host.
  AppendFormattedComponent(spec, parsed.host, HostComponentTransform(),
                           &url_string, &new_parsed->host, adjustments);

  // Port.
  if (parsed.port.is_nonempty()) {
    url_string.push_back(':');
    new_parsed->port.begin = url_string.length();
    url_string.insert(url_string.end(), spec.begin() + parsed.port.begin,
                      spec.begin() + parsed.port.end());
    new_parsed->port.len = url_string.length() - new_parsed->port.begin;
  } else {
    new_parsed->port.reset();
  }

  // Path & query.  Both get the same general unescape & convert treatment.
  if (!(format_types & kFormatUrlOmitTrailingSlashOnBareHostname) ||
      !CanStripTrailingSlash(url)) {
    AppendFormattedComponent(spec, parsed.path,
                             NonHostComponentTransform(unescape_rules),
                             &url_string, &new_parsed->path, adjustments);
  } else {
    if (parsed.path.len > 0) {
      adjustments->push_back(base::OffsetAdjuster::Adjustment(
          parsed.path.begin, parsed.path.len, 0));
    }
  }
  if (parsed.query.is_valid())
    url_string.push_back('?');
  AppendFormattedComponent(spec, parsed.query,
                           NonHostComponentTransform(unescape_rules),
                           &url_string, &new_parsed->query, adjustments);

  // Ref.  This is valid, unescaped UTF-8, so we can just convert.
  if (parsed.ref.is_valid())
    url_string.push_back('#');
  AppendFormattedComponent(spec, parsed.ref,
                           NonHostComponentTransform(net::UnescapeRule::NONE),
                           &url_string, &new_parsed->ref, adjustments);

  // If we need to strip out http do it after the fact.
  if (omit_http && base::StartsWith(url_string, base::ASCIIToUTF16(kHTTP),
                                    base::CompareCase::SENSITIVE)) {
    const size_t kHTTPSize = arraysize(kHTTP) - 1;
    url_string = url_string.substr(kHTTPSize);
    // Because offsets in the |adjustments| are already calculated with respect
    // to the string with the http:// prefix in it, those offsets remain correct
    // after stripping the prefix.  The only thing necessary is to add an
    // adjustment to reflect the stripped prefix.
    adjustments->insert(adjustments->begin(),
                        base::OffsetAdjuster::Adjustment(0, kHTTPSize, 0));

    if (prefix_end)
      *prefix_end -= kHTTPSize;

    // Adjust new_parsed.
    DCHECK(new_parsed->scheme.is_valid());
    int delta = -(new_parsed->scheme.len + 3);  // +3 for ://.
    new_parsed->scheme.reset();
    AdjustAllComponentsButScheme(delta, new_parsed);
  }

  return url_string;
}

bool CanStripTrailingSlash(const GURL& url) {
  // Omit the path only for standard, non-file URLs with nothing but "/" after
  // the hostname.
  return url.IsStandard() && !url.SchemeIsFile() && !url.SchemeIsFileSystem() &&
         !url.has_query() && !url.has_ref() && url.path_piece() == "/";
}

void AppendFormattedHost(const GURL& url, base::string16* output) {
  AppendFormattedComponent(
      url.possibly_invalid_spec(), url.parsed_for_possibly_invalid_spec().host,
      HostComponentTransform(), output, NULL, NULL);
}

base::string16 IDNToUnicode(base::StringPiece host) {
  return IDNToUnicodeWithAdjustments(host, nullptr);
}

base::string16 StripWWW(const base::string16& text) {
  const base::string16 www(base::ASCIIToUTF16("www."));
  return base::StartsWith(text, www, base::CompareCase::SENSITIVE)
      ? text.substr(www.length()) : text;
}

base::string16 StripWWWFromHost(const GURL& url) {
  DCHECK(url.is_valid());
  return StripWWW(base::ASCIIToUTF16(url.host_piece()));
}

}  // namespace url_formatter
