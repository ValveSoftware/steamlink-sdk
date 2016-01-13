// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utility functions for dealing with localized
// content.

#ifndef UI_BASE_L10N_L10N_UTIL_H_
#define UI_BASE_L10N_L10N_UTIL_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "ui/base/ui_base_export.h"

#if defined(OS_MACOSX)
#include "ui/base/l10n/l10n_util_mac.h"
#endif  // OS_MACOSX

namespace l10n_util {

// The same as base::i18n::GetCanonicalLocale(const char*), but takes
// std::string as an argument.
UI_BASE_EXPORT std::string GetCanonicalLocale(const std::string& locale);

// This method translates a generic locale name to one of the locally defined
// ones. This method returns true if it succeeds.
UI_BASE_EXPORT bool CheckAndResolveLocale(const std::string& locale,
                                          std::string* resolved_locale);

// This method is responsible for determining the locale as defined below. In
// nearly all cases you shouldn't call this, rather use GetApplicationLocale
// defined on browser_process.
//
// Returns the locale used by the Application.  First we use the value from the
// command line (--lang), second we try the value in the prefs file (passed in
// as |pref_locale|), finally, we fall back on the system locale. We only return
// a value if there's a corresponding resource DLL for the locale.  Otherwise,
// we fall back to en-us.
UI_BASE_EXPORT std::string GetApplicationLocale(const std::string& pref_locale);

// Returns true if a display name for |locale| is available in the locale
// |display_locale|.
UI_BASE_EXPORT bool IsLocaleNameTranslated(const char* locale,
                                           const std::string& display_locale);

// Given a locale code, return true if the OS is capable of supporting it.
// For instance, Oriya is not well supported on Windows XP and we return
// false for "or".
bool IsLocaleSupportedByOS(const std::string& locale);

// This method returns the display name of the locale code in |display_locale|.

// For example, for |locale| = "fr" and |display_locale| = "en",
// it returns "French". To get the display name of
// |locale| in the UI language of Chrome, |display_locale| can be
// set to the return value of g_browser_process->GetApplicationLocale()
// in the UI thread.
// If |is_for_ui| is true, U+200F is appended so that it can be
// rendered properly in a RTL Chrome.
UI_BASE_EXPORT base::string16 GetDisplayNameForLocale(
    const std::string& locale,
    const std::string& display_locale,
    bool is_for_ui);

// Returns the display name of the |country_code| in |display_locale|.
UI_BASE_EXPORT base::string16 GetDisplayNameForCountry(
    const std::string& country_code,
    const std::string& display_locale);

// Converts all - into _, to be consistent with ICU and file system names.
UI_BASE_EXPORT std::string NormalizeLocale(const std::string& locale);

// Produce a vector of parent locales for given locale.
// It includes the current locale in the result.
// sr_Cyrl_RS generates sr_Cyrl_RS, sr_Cyrl and sr.
UI_BASE_EXPORT void GetParentLocales(const std::string& current_locale,
                                     std::vector<std::string>* parent_locales);

// Checks if a string is plausibly a syntactically-valid locale string,
// for cases where we want the valid input to be a locale string such as
// 'en', 'pt-BR', 'fil', 'es-419', 'zh-Hans-CN', 'i-klingon' or
// 'de_DE@collation=phonebook', but we don't want to limit it to
// locales that Chrome actually knows about, so 'xx-YY' should be
// accepted, but 'z', 'German', 'en-$1', or 'abcd-1234' should not.
// Case-insensitive. Based on BCP 47, see:
//   http://unicode.org/reports/tr35/#Unicode_Language_and_Locale_Identifiers
UI_BASE_EXPORT bool IsValidLocaleSyntax(const std::string& locale);

//
// Mac Note: See l10n_util_mac.h for some NSString versions and other support.
//

// Pulls resource string from the string bundle and returns it.
UI_BASE_EXPORT std::string GetStringUTF8(int message_id);
UI_BASE_EXPORT base::string16 GetStringUTF16(int message_id);

// Get a resource string and replace $i with replacements[i] for all
// i < replacements.size(). Additionally, $$ is replaced by $.
// If non-NULL |offsets| will be replaced with the start points of the replaced
// strings.
UI_BASE_EXPORT base::string16 GetStringFUTF16(
    int message_id,
    const std::vector<base::string16>& replacements,
    std::vector<size_t>* offsets);

// Convenience wrappers for the above.
UI_BASE_EXPORT base::string16 GetStringFUTF16(int message_id,
                                              const base::string16& a);
UI_BASE_EXPORT base::string16 GetStringFUTF16(int message_id,
                                              const base::string16& a,
                                              const base::string16& b);
UI_BASE_EXPORT base::string16 GetStringFUTF16(int message_id,
                                              const base::string16& a,
                                              const base::string16& b,
                                              const base::string16& c);
UI_BASE_EXPORT base::string16 GetStringFUTF16(int message_id,
                                              const base::string16& a,
                                              const base::string16& b,
                                              const base::string16& c,
                                              const base::string16& d);
UI_BASE_EXPORT base::string16 GetStringFUTF16(int message_id,
                                              const base::string16& a,
                                              const base::string16& b,
                                              const base::string16& c,
                                              const base::string16& d,
                                              const base::string16& e);
UI_BASE_EXPORT std::string GetStringFUTF8(int message_id,
                                          const base::string16& a);
UI_BASE_EXPORT std::string GetStringFUTF8(int message_id,
                                          const base::string16& a,
                                          const base::string16& b);
UI_BASE_EXPORT std::string GetStringFUTF8(int message_id,
                                          const base::string16& a,
                                          const base::string16& b,
                                          const base::string16& c);
UI_BASE_EXPORT std::string GetStringFUTF8(int message_id,
                                          const base::string16& a,
                                          const base::string16& b,
                                          const base::string16& c,
                                          const base::string16& d);

// Variants that return the offset(s) of the replaced parameters. The
// vector based version returns offsets ordered by parameter. For example if
// invoked with a and b offsets[0] gives the offset for a and offsets[1] the
// offset of b regardless of where the parameters end up in the string.
UI_BASE_EXPORT base::string16 GetStringFUTF16(int message_id,
                                              const base::string16& a,
                                              size_t* offset);
UI_BASE_EXPORT base::string16 GetStringFUTF16(int message_id,
                                              const base::string16& a,
                                              const base::string16& b,
                                              std::vector<size_t>* offsets);

// Convenience functions to get a string with a single number as a parameter.
UI_BASE_EXPORT base::string16 GetStringFUTF16Int(int message_id, int a);
base::string16 GetStringFUTF16Int(int message_id, int64 a);

// Get a resource string using |number| to decide which of |message_ids| should
// be used. |message_ids| must be size 6 and in order: default, singular, zero,
// two, few, many.
UI_BASE_EXPORT base::string16 GetPluralStringFUTF16(
    const std::vector<int>& message_ids,
    int number);
UI_BASE_EXPORT std::string GetPluralStringFUTF8(
    const std::vector<int>& message_ids,
    int number);

// In place sorting of base::string16 strings using collation rules for
// |locale|.
UI_BASE_EXPORT void SortStrings16(const std::string& locale,
                                  std::vector<base::string16>* strings);

// Returns a vector of available locale codes. E.g., a vector containing
// en-US, es, fr, fi, pt-PT, pt-BR, etc.
UI_BASE_EXPORT const std::vector<std::string>& GetAvailableLocales();

// Returns a vector of locale codes usable for accept-languages.
UI_BASE_EXPORT void GetAcceptLanguagesForLocale(
    const std::string& display_locale,
    std::vector<std::string>* locale_codes);

// Returns the preferred size of the contents view of a window based on
// designer given constraints which might dependent on the language used.
UI_BASE_EXPORT int GetLocalizedContentsWidthInPixels(int pixel_resource_id);

}  // namespace l10n_util

#endif  // UI_BASE_L10N_L10N_UTIL_H_
