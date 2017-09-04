// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AcceptLanguagesResolver_h
#define AcceptLanguagesResolver_h

#include "platform/PlatformExport.h"
#include "wtf/text/WTFString.h"

#include <unicode/uscript.h>

namespace blink {

class LayoutLocale;

class PLATFORM_EXPORT AcceptLanguagesResolver {
 public:
  static void acceptLanguagesChanged(const String&);

  static const LayoutLocale* localeForHan();
  static const LayoutLocale* localeForHanFromAcceptLanguages(const String&);
};

}  // namespace blink

#endif  // AcceptLanguagesResolver_h
