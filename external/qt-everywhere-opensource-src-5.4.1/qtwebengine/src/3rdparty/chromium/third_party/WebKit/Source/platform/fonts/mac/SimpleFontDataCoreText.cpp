/*
 * Copyright (C) 2005, 2006, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "platform/fonts/SimpleFontData.h"

#include "platform/fonts/Character.h"
#include "platform/fonts/Font.h"
#include "platform/fonts/GlyphPage.h"
#include <ApplicationServices/ApplicationServices.h>

// Forward declare Mac SPIs.
// Request for public API: rdar://13787589
extern "C" {
void CGFontGetGlyphsForUnichars(CGFontRef, const UniChar chars[], CGGlyph glyphs[], size_t length);
}

namespace WebCore {

CFDictionaryRef SimpleFontData::getCFStringAttributes(TypesettingFeatures typesettingFeatures, FontOrientation orientation) const
{
    unsigned key = typesettingFeatures + 1;
    HashMap<unsigned, RetainPtr<CFDictionaryRef> >::AddResult addResult = m_CFStringAttributes.add(key, RetainPtr<CFDictionaryRef>());
    RetainPtr<CFDictionaryRef>& attributesDictionary = addResult.storedValue->value;
    if (!addResult.isNewEntry)
        return attributesDictionary.get();

    attributesDictionary.adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 4, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    CFMutableDictionaryRef mutableAttributes = (CFMutableDictionaryRef)attributesDictionary.get();

    CFDictionarySetValue(mutableAttributes, kCTFontAttributeName, platformData().ctFont());

    if (!(typesettingFeatures & Kerning)) {
        const float zero = 0;
        static CFNumberRef zeroKerningValue = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &zero);
        CFDictionarySetValue(mutableAttributes, kCTKernAttributeName, zeroKerningValue);
    }

    bool allowLigatures = (orientation == Horizontal && platformData().allowsLigatures()) || (typesettingFeatures & Ligatures);
    if (!allowLigatures) {
        const int zero = 0;
        static CFNumberRef essentialLigaturesValue = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &zero);
        CFDictionarySetValue(mutableAttributes, kCTLigatureAttributeName, essentialLigaturesValue);
    }

    if (orientation == Vertical)
        CFDictionarySetValue(mutableAttributes, kCTVerticalFormsAttributeName, kCFBooleanTrue);

    return attributesDictionary.get();
}

static bool shouldUseCoreText(UChar* buffer, unsigned bufferLength, const SimpleFontData* fontData)
{
    if (fontData->platformData().isCompositeFontReference())
        return true;

    // CoreText doesn't have vertical glyphs of surrogate pair characters.
    // Therefore, we should not use CoreText, but this always returns horizontal glyphs.
    // FIXME: We should use vertical glyphs. https://code.google.com/p/chromium/issues/detail?id=340173
    if (bufferLength >= 2 && U_IS_SURROGATE(buffer[0]) && fontData->hasVerticalGlyphs()) {
        ASSERT(U_IS_SURROGATE_LEAD(buffer[0]));
        ASSERT(U_IS_TRAIL(buffer[1]));
        return false;
    }

    if (fontData->platformData().widthVariant() != RegularWidth || fontData->hasVerticalGlyphs()) {
        // Ideographs don't have a vertical variant or width variants.
        for (unsigned i = 0; i < bufferLength; ++i) {
            if (!Character::isCJKIdeograph(buffer[i]))
                return true;
        }
    }

    return false;
}

bool SimpleFontData::fillGlyphPage(GlyphPage* pageToFill, unsigned offset, unsigned length, UChar* buffer, unsigned bufferLength) const
{
    bool haveGlyphs = false;

    Vector<CGGlyph, 512> glyphs(bufferLength);
    if (!shouldUseCoreText(buffer, bufferLength, this)) {
        CGFontGetGlyphsForUnichars(platformData().cgFont(), buffer, glyphs.data(), bufferLength);
        for (unsigned i = 0; i < length; ++i) {
            if (glyphs[i]) {
                pageToFill->setGlyphDataForIndex(offset + i, glyphs[i], this);
                haveGlyphs = true;
            }
        }
    } else if (!platformData().isCompositeFontReference() && platformData().widthVariant() != RegularWidth
        && CTFontGetGlyphsForCharacters(platformData().ctFont(), buffer, glyphs.data(), bufferLength)) {
        // When buffer consists of surrogate pairs, CTFontGetGlyphsForCharacters
        // places the glyphs at indices corresponding to the first character of each pair.
        unsigned glyphStep = bufferLength / length;
        for (unsigned i = 0; i < length; ++i) {
            if (glyphs[i * glyphStep]) {
                pageToFill->setGlyphDataForIndex(offset + i, glyphs[i * glyphStep], this);
                haveGlyphs = true;
            }
        }
    } else {
        // We ask CoreText for possible vertical variant glyphs
        RetainPtr<CFStringRef> string(AdoptCF, CFStringCreateWithCharactersNoCopy(kCFAllocatorDefault, buffer, bufferLength, kCFAllocatorNull));
        RetainPtr<CFAttributedStringRef> attributedString(AdoptCF, CFAttributedStringCreate(kCFAllocatorDefault, string.get(), getCFStringAttributes(0, hasVerticalGlyphs() ? Vertical : Horizontal)));
        RetainPtr<CTLineRef> line(AdoptCF, CTLineCreateWithAttributedString(attributedString.get()));

        CFArrayRef runArray = CTLineGetGlyphRuns(line.get());
        CFIndex runCount = CFArrayGetCount(runArray);

        Vector<CGGlyph, 512> glyphVector;
        Vector<CFIndex, 512> indexVector;
        bool done = false;

        // For the CGFont comparison in the loop, use the CGFont that Core Text assigns to the CTFont. This may
        // be non-CFEqual to platformData().cgFont().
        RetainPtr<CGFontRef> cgFont(AdoptCF, CTFontCopyGraphicsFont(platformData().ctFont(), 0));

        for (CFIndex r = 0; r < runCount && !done ; ++r) {
            // CTLine could map characters over multiple fonts using its own font fallback list.
            // We need to pick runs that use the exact font we need, i.e., platformData().ctFont().
            CTRunRef ctRun = static_cast<CTRunRef>(CFArrayGetValueAtIndex(runArray, r));
            ASSERT(CFGetTypeID(ctRun) == CTRunGetTypeID());

            CFDictionaryRef attributes = CTRunGetAttributes(ctRun);
            CTFontRef runFont = static_cast<CTFontRef>(CFDictionaryGetValue(attributes, kCTFontAttributeName));
            RetainPtr<CGFontRef> runCGFont(AdoptCF, CTFontCopyGraphicsFont(runFont, 0));
            // Use CGFont here as CFEqual for CTFont counts all attributes for font.
            bool gotBaseFont = CFEqual(cgFont.get(), runCGFont.get());
            if (gotBaseFont || platformData().isCompositeFontReference()) {
                // This run uses the font we want. Extract glyphs.
                CFIndex glyphCount = CTRunGetGlyphCount(ctRun);
                const CGGlyph* glyphs = CTRunGetGlyphsPtr(ctRun);
                if (!glyphs) {
                    glyphVector.resize(glyphCount);
                    CTRunGetGlyphs(ctRun, CFRangeMake(0, 0), glyphVector.data());
                    glyphs = glyphVector.data();
                }
                const CFIndex* stringIndices = CTRunGetStringIndicesPtr(ctRun);
                if (!stringIndices) {
                    indexVector.resize(glyphCount);
                    CTRunGetStringIndices(ctRun, CFRangeMake(0, 0), indexVector.data());
                    stringIndices = indexVector.data();
                }

                if (gotBaseFont) {
                    for (CFIndex i = 0; i < glyphCount; ++i) {
                        if (stringIndices[i] >= static_cast<CFIndex>(length)) {
                            done = true;
                            break;
                        }
                        if (glyphs[i]) {
                            pageToFill->setGlyphDataForIndex(offset + stringIndices[i], glyphs[i], this);
                            haveGlyphs = true;
                        }
                    }
                } else {
                    const SimpleFontData* runSimple = getCompositeFontReferenceFontData((NSFont *)runFont);
                    if (runSimple) {
                        for (CFIndex i = 0; i < glyphCount; ++i) {
                            if (stringIndices[i] >= static_cast<CFIndex>(length)) {
                                done = true;
                                break;
                            }
                            if (glyphs[i]) {
                                pageToFill->setGlyphDataForIndex(offset + stringIndices[i], glyphs[i], runSimple);
                                haveGlyphs = true;
                            }
                        }
                    }
                }
            }
        }
    }

    return haveGlyphs;
}

} // namespace WebCore
