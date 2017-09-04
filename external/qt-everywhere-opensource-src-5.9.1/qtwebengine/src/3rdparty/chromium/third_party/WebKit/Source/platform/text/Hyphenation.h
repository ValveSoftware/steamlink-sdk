// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Hyphenation_h
#define Hyphenation_h

#include "platform/PlatformExport.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/RefCounted.h"
#include "wtf/text/AtomicStringHash.h"
#include "wtf/text/StringHash.h"

namespace blink {

class Font;

class PLATFORM_EXPORT Hyphenation : public RefCounted<Hyphenation> {
 public:
  virtual ~Hyphenation() {}

  virtual size_t lastHyphenLocation(const StringView&,
                                    size_t beforeIndex) const = 0;
  virtual Vector<size_t, 8> hyphenLocations(const StringView&) const;

  static const unsigned minimumPrefixLength = 2;
  static const unsigned minimumSuffixLength = 2;
  static int minimumPrefixWidth(const Font&);

 private:
  friend class LayoutLocale;
  static PassRefPtr<Hyphenation> platformGetHyphenation(
      const AtomicString& locale);
};

}  // namespace blink

#endif  // Hyphenation_h
