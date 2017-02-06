// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/URLConversion.h"

#include "public/platform/WebString.h"
#include "url/gurl.h"
#include "wtf/text/StringUTF8Adaptor.h"
#include "wtf/text/WTFString.h"

namespace blink {

GURL WebStringToGURL(const WebString& webString)
{
    if (webString.isEmpty())
        return GURL();

    String str = webString;
    if (str.is8Bit()) {
        // Ensure the (possibly Latin-1) 8-bit string is UTF-8 for GURL.
        StringUTF8Adaptor utf8(str);
        return GURL(utf8.asStringPiece());
    }

    // GURL can consume UTF-16 directly.
    return GURL(base::StringPiece16(str.characters16(), str.length()));
}

} // namespace blink
