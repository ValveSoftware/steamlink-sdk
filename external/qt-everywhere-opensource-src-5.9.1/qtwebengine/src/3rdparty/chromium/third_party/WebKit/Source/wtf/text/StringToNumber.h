// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WTF_StringToNumber_h
#define WTF_StringToNumber_h

#include "wtf/WTFExport.h"
#include "wtf/text/Unicode.h"

namespace WTF {

// string -> int.
WTF_EXPORT int charactersToIntStrict(const LChar*,
                                     size_t,
                                     bool* ok = 0,
                                     int base = 10);
WTF_EXPORT int charactersToIntStrict(const UChar*,
                                     size_t,
                                     bool* ok = 0,
                                     int base = 10);
WTF_EXPORT int charactersToInt(const LChar*,
                               size_t,
                               bool* ok = 0);  // ignores trailing garbage
WTF_EXPORT int charactersToInt(const UChar*,
                               size_t,
                               bool* ok = 0);  // ignores trailing garbage

// string -> unsigned.
WTF_EXPORT unsigned charactersToUIntStrict(const LChar*,
                                           size_t,
                                           bool* ok = 0,
                                           int base = 10);
WTF_EXPORT unsigned charactersToUIntStrict(const UChar*,
                                           size_t,
                                           bool* ok = 0,
                                           int base = 10);
WTF_EXPORT unsigned charactersToUInt(const LChar*,
                                     size_t,
                                     bool* ok = 0);  // ignores trailing garbage
WTF_EXPORT unsigned charactersToUInt(const UChar*,
                                     size_t,
                                     bool* ok = 0);  // ignores trailing garbage

// string -> int64_t.
WTF_EXPORT int64_t charactersToInt64Strict(const LChar*,
                                           size_t,
                                           bool* ok = 0,
                                           int base = 10);
WTF_EXPORT int64_t charactersToInt64Strict(const UChar*,
                                           size_t,
                                           bool* ok = 0,
                                           int base = 10);
WTF_EXPORT int64_t charactersToInt64(const LChar*,
                                     size_t,
                                     bool* ok = 0);  // ignores trailing garbage
WTF_EXPORT int64_t charactersToInt64(const UChar*,
                                     size_t,
                                     bool* ok = 0);  // ignores trailing garbage

// string -> uint64_t.
WTF_EXPORT uint64_t charactersToUInt64Strict(const LChar*,
                                             size_t,
                                             bool* ok = 0,
                                             int base = 10);
WTF_EXPORT uint64_t charactersToUInt64Strict(const UChar*,
                                             size_t,
                                             bool* ok = 0,
                                             int base = 10);
WTF_EXPORT uint64_t
charactersToUInt64(const LChar*,
                   size_t,
                   bool* ok = 0);  // ignores trailing garbage
WTF_EXPORT uint64_t
charactersToUInt64(const UChar*,
                   size_t,
                   bool* ok = 0);  // ignores trailing garbage

// FIXME: Like the strict functions above, these give false for "ok" when there
// is trailing garbage.  Like the non-strict functions above, these return the
// value when there is trailing garbage.  It would be better if these were more
// consistent with the above functions instead.
//
// string -> double.
WTF_EXPORT double charactersToDouble(const LChar*, size_t, bool* ok = 0);
WTF_EXPORT double charactersToDouble(const UChar*, size_t, bool* ok = 0);
WTF_EXPORT double charactersToDouble(const LChar*,
                                     size_t,
                                     size_t& parsedLength);
WTF_EXPORT double charactersToDouble(const UChar*,
                                     size_t,
                                     size_t& parsedLength);

// string -> float.
WTF_EXPORT float charactersToFloat(const LChar*, size_t, bool* ok = 0);
WTF_EXPORT float charactersToFloat(const UChar*, size_t, bool* ok = 0);
WTF_EXPORT float charactersToFloat(const LChar*, size_t, size_t& parsedLength);
WTF_EXPORT float charactersToFloat(const UChar*, size_t, size_t& parsedLength);

}  // namespace WTF

using WTF::charactersToIntStrict;
using WTF::charactersToUIntStrict;
using WTF::charactersToInt64Strict;
using WTF::charactersToUInt64Strict;
using WTF::charactersToInt;
using WTF::charactersToUInt;
using WTF::charactersToInt64;
using WTF::charactersToUInt64;
using WTF::charactersToDouble;
using WTF::charactersToFloat;

#endif
