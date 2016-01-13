// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_LOCALIZED_ERROR_H_
#define CHROME_COMMON_LOCALIZED_ERROR_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace extensions {
class Extension;
}

namespace blink {
struct WebURLError;
}

class LocalizedError {
 public:
  // Optional parameters that affect the display of an error page.
  struct ErrorPageParams {
    ErrorPageParams();
    ~ErrorPageParams();

    // Overrides whether reloading is suggested.
    bool suggest_reload;
    int reload_tracking_id;

    // Overrides default suggestions.  Each entry must contain a header and may
    // optionally contain a body as well.  Must not be NULL.
    scoped_ptr<base::ListValue> override_suggestions;

    // Prefix to prepend to search terms.  Search box is only shown if this is
    // a valid url.  The search terms will be appended to the end of this URL to
    // conduct a search.
    GURL search_url;
    // Default search terms.  Ignored if |search_url| is invalid.
    std::string search_terms;
    int search_tracking_id;
  };

  // Fills |error_strings| with values to be used to build an error page used
  // on HTTP errors, like 404 or connection reset.
  static void GetStrings(int error_code,
                         const std::string& error_domain,
                         const GURL& failed_url,
                         bool is_post,
                         bool show_stale_load_button,
                         const std::string& locale,
                         const std::string& accept_languages,
                         scoped_ptr<ErrorPageParams> params,
                         base::DictionaryValue* strings);

  // Returns a description of the encountered error.
  static base::string16 GetErrorDetails(const blink::WebURLError& error,
                                        bool is_post);

  // Returns true if an error page exists for the specified parameters.
  static bool HasStrings(const std::string& error_domain, int error_code);

  // Fills |error_strings| with values to be used to build an error page used
  // on HTTP errors, like 404 or connection reset, but using information from
  // the associated |app| in order to make the error page look like it's more
  // part of the app.
#if !defined(TOOLKIT_QT)
  static void GetAppErrorStrings(const GURL& display_url,
                                 const extensions::Extension* app,
                                 base::DictionaryValue* error_strings);
#endif // !defined(TOOLKIT_QT)

  static const char kHttpErrorDomain[];

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(LocalizedError);
};

#endif  // CHROME_COMMON_LOCALIZED_ERROR_H_
