/*
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
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

#ifndef HTTPParsers_h
#define HTTPParsers_h

#include "platform/PlatformExport.h"
#include "platform/json/JSONValues.h"
#include "wtf/Allocator.h"
#include "wtf/Forward.h"
#include "wtf/HashSet.h"
#include "wtf/Vector.h"
#include "wtf/text/StringHash.h"

#include <memory>

namespace blink {

class Suborigin;
class ResourceResponse;

typedef enum {
  ContentDispositionNone,
  ContentDispositionInline,
  ContentDispositionAttachment,
  ContentDispositionOther
} ContentDispositionType;

enum ContentTypeOptionsDisposition {
  ContentTypeOptionsNone,
  ContentTypeOptionsNosniff
};

enum XFrameOptionsDisposition {
  XFrameOptionsInvalid,
  XFrameOptionsDeny,
  XFrameOptionsSameOrigin,
  XFrameOptionsAllowAll,
  XFrameOptionsConflict
};

// Be sure to update the behavior of
// XSSAuditor::combineXSSProtectionHeaderAndCSP whenever you change this enum's
// content or ordering.
enum ReflectedXSSDisposition {
  ReflectedXSSUnset = 0,
  AllowReflectedXSS,
  ReflectedXSSInvalid,
  FilterReflectedXSS,
  BlockReflectedXSS
};

using CommaDelimitedHeaderSet = HashSet<String, CaseFoldingHash>;

struct CacheControlHeader {
  DISALLOW_NEW();
  bool parsed : 1;
  bool containsNoCache : 1;
  bool containsNoStore : 1;
  bool containsMustRevalidate : 1;
  double maxAge;
  double staleWhileRevalidate;

  CacheControlHeader()
      : parsed(false),
        containsNoCache(false),
        containsNoStore(false),
        containsMustRevalidate(false),
        maxAge(0.0),
        staleWhileRevalidate(0.0) {}
};

PLATFORM_EXPORT ContentDispositionType getContentDispositionType(const String&);
PLATFORM_EXPORT bool isValidHTTPHeaderValue(const String&);
PLATFORM_EXPORT bool isValidHTTPFieldContentRFC7230(const String&);
// Checks whether the given string conforms to the |token| ABNF production
// defined in the RFC 7230 or not.
//
// The ABNF is for validating octets, but this method takes a String instance
// for convenience which consists of Unicode code points. When this method sees
// non-ASCII characters, it just returns false.
PLATFORM_EXPORT bool isValidHTTPToken(const String&);
// |matcher| specifies a function to check a whitespace character. if |nullptr|
// is specified, ' ' and '\t' are treated as whitespace characters.
PLATFORM_EXPORT bool parseHTTPRefresh(const String& refresh,
                                      WTF::CharacterMatchFunctionPtr matcher,
                                      double& delay,
                                      String& url);
PLATFORM_EXPORT double parseDate(const String&);

// Given a Media Type (like "foo/bar; baz=gazonk" - usually from the
// 'Content-Type' HTTP header), extract and return the "type/subtype" portion
// ("foo/bar").
//
// Note:
// - This function does not in any way check that the "type/subtype" pair
//   is well-formed.
// - OWSes at the head and the tail of the region before the first semicolon
//   are trimmed.
PLATFORM_EXPORT AtomicString extractMIMETypeFromMediaType(const AtomicString&);
PLATFORM_EXPORT String extractCharsetFromMediaType(const String&);
PLATFORM_EXPORT void findCharsetInMediaType(const String& mediaType,
                                            unsigned& charsetPos,
                                            unsigned& charsetLen,
                                            unsigned start = 0);
PLATFORM_EXPORT ReflectedXSSDisposition
parseXSSProtectionHeader(const String& header,
                         String& failureReason,
                         unsigned& failurePosition,
                         String& reportURL);
PLATFORM_EXPORT XFrameOptionsDisposition
parseXFrameOptionsHeader(const String&);
PLATFORM_EXPORT CacheControlHeader
parseCacheControlDirectives(const AtomicString& cacheControlHeader,
                            const AtomicString& pragmaHeader);
PLATFORM_EXPORT void parseCommaDelimitedHeader(const String& headerValue,
                                               CommaDelimitedHeaderSet&);
// Returns true on success, otherwise false. The Suborigin argument must be a
// non-null return argument. |messages| is a list of messages based on any
// parse warnings or errors. Even if parseSuboriginHeader returns true, there
// may be Strings in |messages|.
PLATFORM_EXPORT bool parseSuboriginHeader(const String& header,
                                          Suborigin*,
                                          WTF::Vector<String>& messages);

PLATFORM_EXPORT ContentTypeOptionsDisposition
parseContentTypeOptionsHeader(const String& header);

// Returns true and stores the position of the end of the headers to |*end|
// if the headers part ends in |bytes[0..size]|. Returns false otherwise.
PLATFORM_EXPORT bool parseMultipartHeadersFromBody(const char* bytes,
                                                   size_t,
                                                   ResourceResponse*,
                                                   size_t* end);

// Parses a header value containing JSON data, according to
// https://tools.ietf.org/html/draft-ietf-httpbis-jfv-01
// Returns an empty unique_ptr if the header cannot be parsed as JSON.
PLATFORM_EXPORT std::unique_ptr<JSONArray> parseJSONHeader(
    const String& header);

}  // namespace blink

#endif
