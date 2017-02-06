/*
 * Copyright (C) 2006, 2008 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007-2008 Torch Mobile, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FontCache_h
#define FontCache_h

#include "platform/PlatformExport.h"
#include "platform/fonts/FallbackListCompositeKey.h"
#include "platform/fonts/FontCacheKey.h"
#include "platform/fonts/FontFaceCreationParams.h"
#include "platform/fonts/FontFallbackPriority.h"
#include "wtf/Allocator.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/text/CString.h"
#include "wtf/text/Unicode.h"
#include "wtf/text/WTFString.h"
#include <limits.h>
#include <memory>

#include "SkFontMgr.h"

class SkTypeface;

namespace base {
namespace trace_event {
class ProcessMemoryDump;
}
}

namespace blink {

class FontCacheClient;
class FontFaceCreationParams;
class FontPlatformData;
class FontDescription;
class OpenTypeVerticalData;
class ShapeCache;
class SimpleFontData;

enum ShouldRetain { Retain, DoNotRetain };
enum PurgeSeverity { PurgeIfNeeded, ForcePurge };

class PLATFORM_EXPORT FontCache {
    friend class FontCachePurgePreventer;

    WTF_MAKE_NONCOPYABLE(FontCache); USING_FAST_MALLOC(FontCache);
public:
    static FontCache* fontCache();

    void releaseFontData(const SimpleFontData*);

    // This method is implemented by the plaform and used by
    // FontFastPath to lookup the font for a given character.
    PassRefPtr<SimpleFontData> fallbackFontForCharacter(const FontDescription&,
        UChar32,
        const SimpleFontData* fontDataToSubstitute,
        FontFallbackPriority = FontFallbackPriority::Text);

    // Also implemented by the platform.
    void platformInit();

    PassRefPtr<SimpleFontData> getFontData(const FontDescription&, const AtomicString&, bool checkingAlternateName = false, ShouldRetain = Retain);
    PassRefPtr<SimpleFontData> getLastResortFallbackFont(const FontDescription&, ShouldRetain = Retain);
    SimpleFontData* getNonRetainedLastResortFallbackFont(const FontDescription&);
    bool isPlatformFontAvailable(const FontDescription&, const AtomicString&);

    // Returns the ShapeCache instance associated with the given cache key.
    // Creates a new instance as needed and as such is guaranteed not to return
    // a nullptr. Instances are managed by FontCache and are only guaranteed to
    // be valid for the duration of the current session, as controlled by
    // disable/enablePurging.
    ShapeCache* getShapeCache(const FallbackListCompositeKey&);

    void addClient(FontCacheClient*);

    unsigned short generation();
    void invalidate();

    SkFontMgr* fontManager() { return m_fontManager.get(); }
    static void setFontManager(const RefPtr<SkFontMgr>&);

#if OS(WIN)
    static bool antialiasedTextEnabled() { return s_antialiasedTextEnabled; }
    static bool lcdTextEnabled() { return s_lcdTextEnabled; }
    static float deviceScaleFactor() { return s_deviceScaleFactor; }
    static void setAntialiasedTextEnabled(bool enabled) { s_antialiasedTextEnabled = enabled; }
    static void setLCDTextEnabled(bool enabled) { s_lcdTextEnabled = enabled; }
    static void setDeviceScaleFactor(float deviceScaleFactor) { s_deviceScaleFactor = deviceScaleFactor; }
    static void addSideloadedFontForTesting(SkTypeface*);
    // Functions to cache and retrieve the system font metrics.
    static void setMenuFontMetrics(const wchar_t* familyName, int32_t fontHeight);
    static void setSmallCaptionFontMetrics(const wchar_t* familyName, int32_t fontHeight);
    static void setStatusFontMetrics(const wchar_t* familyName, int32_t fontHeight);
    static int32_t menuFontHeight() { return s_menuFontHeight; }
    static const AtomicString& menuFontFamily() { return *s_smallCaptionFontFamilyName; }
    static int32_t smallCaptionFontHeight() { return s_smallCaptionFontHeight; }
    static const AtomicString& smallCaptionFontFamily() { return *s_smallCaptionFontFamilyName; }
    static int32_t statusFontHeight() { return s_statusFontHeight; }
    static const AtomicString& statusFontFamily() { return *s_statusFontFamilyName; }
    static void setUseSkiaFontFallback(bool useSkiaFontFallback) { s_useSkiaFontFallback = useSkiaFontFallback; }
#endif

    typedef uint32_t FontFileKey;
    PassRefPtr<OpenTypeVerticalData> getVerticalData(const FontFileKey&, const FontPlatformData&);

    static void acceptLanguagesChanged(const String&);

#if OS(ANDROID)
    static AtomicString getGenericFamilyNameForScript(const AtomicString& familyName, const FontDescription&);
#else
    struct PlatformFallbackFont {
        String name;
        CString filename;
        int fontconfigInterfaceId;
        int ttcIndex;
        bool isBold;
        bool isItalic;
    };
    static void getFontForCharacter(UChar32, const char* preferredLocale, PlatformFallbackFont*);
#endif
    PassRefPtr<SimpleFontData> fontDataFromFontPlatformData(const FontPlatformData*, ShouldRetain = Retain);

    void invalidateShapeCache();

    // Memory reporting
    void dumpFontPlatformDataCache(base::trace_event::ProcessMemoryDump*);
    void dumpShapeResultCache(base::trace_event::ProcessMemoryDump*);

private:
    FontCache();
    ~FontCache();

    void purge(PurgeSeverity = PurgeIfNeeded);

    void disablePurging() { m_purgePreventCount++; }
    void enablePurging()
    {
        ASSERT(m_purgePreventCount);
        if (!--m_purgePreventCount)
            purge(PurgeIfNeeded);
    }

    // FIXME: This method should eventually be removed.
    FontPlatformData* getFontPlatformData(const FontDescription&, const FontFaceCreationParams&, bool checkingAlternateName = false);

    // These methods are implemented by each platform.
    std::unique_ptr<FontPlatformData> createFontPlatformData(const FontDescription&, const FontFaceCreationParams&, float fontSize);
    std::unique_ptr<FontPlatformData> scaleFontPlatformData(const FontPlatformData&, const FontDescription&, const FontFaceCreationParams&, float fontSize);

    // Implemented on skia platforms.
    PassRefPtr<SkTypeface> createTypeface(const FontDescription&, const FontFaceCreationParams&, CString& name);

#if OS(ANDROID) || OS(LINUX)
    static AtomicString getFamilyNameForCharacter(SkFontMgr*, UChar32, const FontDescription&, FontFallbackPriority);
#endif

    PassRefPtr<SimpleFontData> fallbackOnStandardFontStyle(const FontDescription&, UChar32);

    // Don't purge if this count is > 0;
    int m_purgePreventCount;

    RefPtr<SkFontMgr> m_fontManager;

    static SkFontMgr* s_fontManager;

#if OS(WIN)
    static bool s_antialiasedTextEnabled;
    static bool s_lcdTextEnabled;
    static float s_deviceScaleFactor;
    static HashMap<String, RefPtr<SkTypeface>>* s_sideloadedFonts;
    // The system font metrics cache.
    static AtomicString* s_menuFontFamilyName;
    static int32_t s_menuFontHeight;
    static AtomicString* s_smallCaptionFontFamilyName;
    static int32_t s_smallCaptionFontHeight;
    static AtomicString* s_statusFontFamilyName;
    static int32_t s_statusFontHeight;
    static bool s_useSkiaFontFallback;
#endif

    friend class SimpleFontData; // For fontDataFromFontPlatformData
    friend class FontFallbackList;
};

class PLATFORM_EXPORT FontCachePurgePreventer {
    USING_FAST_MALLOC(FontCachePurgePreventer);
    WTF_MAKE_NONCOPYABLE(FontCachePurgePreventer);
public:
    FontCachePurgePreventer() { FontCache::fontCache()->disablePurging(); }
    ~FontCachePurgePreventer() { FontCache::fontCache()->enablePurging(); }
};

CString toSkFontMgrLocale(const String& locale);

} // namespace blink

#endif
