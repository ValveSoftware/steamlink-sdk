/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SegmentedFontData_h
#define SegmentedFontData_h

#include "platform/PlatformExport.h"
#include "platform/fonts/FontData.h"
#include "platform/fonts/FontDataForRangeSet.h"
#include "platform/fonts/SimpleFontData.h"

namespace blink {

class PLATFORM_EXPORT SegmentedFontData : public FontData {
public:
    static PassRefPtr<SegmentedFontData> create() { return adoptRef(new SegmentedFontData); }

    ~SegmentedFontData() override;

    void appendFace(const PassRefPtr<FontDataForRangeSet> fontDataForRangeSet) { m_faces.append(fontDataForRangeSet); }
    unsigned numFaces() const { return m_faces.size(); }
    const PassRefPtr<FontDataForRangeSet> faceAt(unsigned i) const { return m_faces[i]; }
    bool containsCharacter(UChar32) const;

private:
    SegmentedFontData() { }

    const SimpleFontData* fontDataForCharacter(UChar32) const override;

    bool isCustomFont() const override;
    bool isLoading() const override;
    bool isLoadingFallback() const override;
    bool isSegmented() const override;
    bool shouldSkipDrawing() const override;

    Vector<RefPtr<FontDataForRangeSet>, 1> m_faces;
};

DEFINE_FONT_DATA_TYPE_CASTS(SegmentedFontData, true);

} // namespace blink

#endif // SegmentedFontData_h
