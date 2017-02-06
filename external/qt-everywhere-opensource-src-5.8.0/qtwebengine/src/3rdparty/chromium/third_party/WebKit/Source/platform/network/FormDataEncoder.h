/*
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef FormDataEncoder_h
#define FormDataEncoder_h

#include "platform/network/EncodedFormData.h"
#include "wtf/Allocator.h"
#include "wtf/Forward.h"

namespace WTF {
class TextEncoding;
}

namespace blink {

class PLATFORM_EXPORT FormDataEncoder {
    STATIC_ONLY(FormDataEncoder);
public:
    // Specifies how to handle CRs and LFs. When NormalizeCRLF is passed, the
    // method replaces the following characters with a CRLF pair:
    // - a CR not followed by an LF
    // - an LF not preceded by a CR
    enum Mode {
        NormalizeCRLF,
        DoNotNormalizeCRLF
    };

    static WTF::TextEncoding encodingFromAcceptCharset(const String& acceptCharset, const WTF::TextEncoding& fallbackEncoding);

    // Helper functions used by HTMLFormElement for multi-part form data
    static Vector<char> generateUniqueBoundaryString();
    static void beginMultiPartHeader(Vector<char>&, const CString& boundary, const CString& name);
    static void addBoundaryToMultiPartHeader(Vector<char>&, const CString& boundary, bool isLastBoundary = false);
    static void addFilenameToMultiPartHeader(Vector<char>&, const WTF::TextEncoding&, const String& filename);
    static void addContentTypeToMultiPartHeader(Vector<char>&, const CString& mimeType);
    static void finishMultiPartHeader(Vector<char>&);

    // Helper functions used by HTMLFormElement for non multi-part form data. Mode
    // argument is not used for TextPlain type.
    static void addKeyValuePairAsFormData(Vector<char>&, const CString& key, const CString& value, EncodedFormData::EncodingType = EncodedFormData::FormURLEncoded, Mode = NormalizeCRLF);
    static void encodeStringAsFormData(Vector<char>&, const CString&, Mode);
};

} // namespace blink

#endif
