// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/FilePathConversion.h"

#include "base/files/file_path.h"
#include "public/platform/WebString.h"
#include "wtf/text/StringUTF8Adaptor.h"
#include "wtf/text/WTFString.h"

namespace blink {

base::FilePath WebStringToFilePath(const WebString& webString)
{
    if (webString.isEmpty())
        return base::FilePath();

    String str = webString;
    if (!str.is8Bit()) {
        return base::FilePath::FromUTF16Unsafe(
            base::StringPiece16(str.characters16(), str.length()));
    }

#if OS(POSIX)
    StringUTF8Adaptor utf8(str);
    return base::FilePath::FromUTF8Unsafe(utf8.asStringPiece());
#else
    const LChar* data8 = str.characters8();
    return base::FilePath::FromUTF16Unsafe(
        base::string16(data8, data8 + str.length()));
#endif
}

} // namespace blink
