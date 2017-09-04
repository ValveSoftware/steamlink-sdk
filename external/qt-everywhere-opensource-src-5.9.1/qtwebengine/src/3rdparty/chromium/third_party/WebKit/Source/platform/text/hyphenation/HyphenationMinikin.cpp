// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/text/Hyphenation.h"

#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/metrics/histogram.h"
#include "base/timer/elapsed_timer.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "platform/LayoutLocale.h"
#include "platform/text/hyphenation/HyphenatorAOSP.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/hyphenation/hyphenation.mojom-blink.h"

namespace blink {

using Hyphenator = android::Hyphenator;

class HyphenationMinikin : public Hyphenation {
 public:
  bool openDictionary(const AtomicString& locale);

  size_t lastHyphenLocation(const StringView& text,
                            size_t beforeIndex) const override;
  Vector<size_t, 8> hyphenLocations(const StringView&) const override;

 private:
  static base::PlatformFile openDictionaryFile(const AtomicString& locale);

  std::vector<uint8_t> hyphenate(const StringView&) const;

  base::MemoryMappedFile m_file;
  std::unique_ptr<Hyphenator> m_hyphenator;
};

static mojom::blink::HyphenationPtr connectToRemoteService() {
  mojom::blink::HyphenationPtr service;
  Platform::current()->interfaceProvider()->getInterface(
      mojo::GetProxy(&service));
  return service;
}

static const mojom::blink::HyphenationPtr& getService() {
  DEFINE_STATIC_LOCAL(mojom::blink::HyphenationPtr, service,
                      (connectToRemoteService()));
  return service;
}

base::PlatformFile HyphenationMinikin::openDictionaryFile(
    const AtomicString& locale) {
  const mojom::blink::HyphenationPtr& service = getService();
  mojo::ScopedHandle handle;
  base::ElapsedTimer timer;
  service->OpenDictionary(locale, &handle);
  UMA_HISTOGRAM_TIMES("Hyphenation.Open", timer.Elapsed());
  if (!handle.is_valid())
    return base::kInvalidPlatformFile;

  base::PlatformFile file;
  MojoResult result = mojo::UnwrapPlatformFile(std::move(handle), &file);
  if (result != MOJO_RESULT_OK) {
    DLOG(ERROR) << "UnwrapPlatformFile failed";
    return base::kInvalidPlatformFile;
  }
  return file;
}

bool HyphenationMinikin::openDictionary(const AtomicString& locale) {
  base::PlatformFile file = openDictionaryFile(locale);
  if (file == base::kInvalidPlatformFile)
    return false;
  if (!m_file.Initialize(base::File(file))) {
    DLOG(ERROR) << "mmap failed";
    return false;
  }

  m_hyphenator = wrapUnique(Hyphenator::loadBinary(m_file.data()));

  return true;
}

std::vector<uint8_t> HyphenationMinikin::hyphenate(
    const StringView& text) const {
  std::vector<uint8_t> result;
  if (text.is8Bit()) {
    String text16Bit = text.toString();
    text16Bit.ensure16Bit();
    m_hyphenator->hyphenate(&result, text16Bit.characters16(),
                            text16Bit.length());
  } else {
    m_hyphenator->hyphenate(&result, text.characters16(), text.length());
  }
  return result;
}

size_t HyphenationMinikin::lastHyphenLocation(const StringView& text,
                                              size_t beforeIndex) const {
  if (text.length() < minimumPrefixLength + minimumSuffixLength)
    return 0;

  std::vector<uint8_t> result = hyphenate(text);
  static_assert(minimumPrefixLength >= 1,
                "Change the 'if' above if this fails");
  for (size_t i = text.length() - minimumSuffixLength - 1;
       i >= minimumPrefixLength; i--) {
    if (result[i])
      return i;
  }
  return 0;
}

Vector<size_t, 8> HyphenationMinikin::hyphenLocations(
    const StringView& text) const {
  Vector<size_t, 8> hyphenLocations;
  if (text.length() < minimumPrefixLength + minimumSuffixLength)
    return hyphenLocations;

  std::vector<uint8_t> result = hyphenate(text);
  static_assert(minimumPrefixLength >= 1,
                "Change the 'if' above if this fails");
  for (size_t i = text.length() - minimumSuffixLength - 1;
       i >= minimumPrefixLength; i--) {
    if (result[i])
      hyphenLocations.append(i);
  }
  return hyphenLocations;
}

using LocaleMap = HashMap<AtomicString, AtomicString, CaseFoldingHash>;

static LocaleMap createLocaleFallbackMap() {
  // This data is from CLDR, compiled by AOSP.
  // https://android.googlesource.com/platform/frameworks/base/+/master/core/java/android/text/Hyphenator.java
  using LocaleFallback = const char * [2];
  static LocaleFallback localeFallbackData[] = {
      {"en-AS", "en-us"},  // English (American Samoa)
      {"en-GU", "en-us"},  // English (Guam)
      {"en-MH", "en-us"},  // English (Marshall Islands)
      {"en-MP", "en-us"},  // English (Northern Mariana Islands)
      {"en-PR", "en-us"},  // English (Puerto Rico)
      {"en-UM", "en-us"},  // English (United States Minor Outlying Islands)
      {"en-VI", "en-us"},  // English (Virgin Islands)
      // All English locales other than those falling back to en-US are mapped
      // to en-GB.
      {"en", "en-gb"},
      // For German, we're assuming the 1996 (and later) orthography by default.
      {"de", "de-1996"},
      // Liechtenstein uses the Swiss hyphenation rules for the 1901
      // orthography.
      {"de-LI-1901", "de-ch-1901"},
      // Norwegian is very probably Norwegian Bokmål.
      {"no", "nb"},
      {"mn", "mn-cyrl"},    // Mongolian
      {"am", "und-ethi"},   // Amharic
      {"byn", "und-ethi"},  // Blin
      {"gez", "und-ethi"},  // Geʻez
      {"ti", "und-ethi"},   // Tigrinya
      {"wal", "und-ethi"},  // Wolaytta
  };
  LocaleMap map;
  for (const auto& it : localeFallbackData)
    map.add(it[0], it[1]);
  return map;
}

PassRefPtr<Hyphenation> Hyphenation::platformGetHyphenation(
    const AtomicString& locale) {
  RefPtr<HyphenationMinikin> hyphenation(adoptRef(new HyphenationMinikin));
  if (hyphenation->openDictionary(locale.lowerASCII()))
    return hyphenation.release();
  hyphenation.clear();

  DEFINE_STATIC_LOCAL(LocaleMap, localeFallback, (createLocaleFallbackMap()));
  const auto& it = localeFallback.find(locale);
  if (it != localeFallback.end())
    return LayoutLocale::get(it->value)->getHyphenation();

  return nullptr;
}

}  // namespace blink
