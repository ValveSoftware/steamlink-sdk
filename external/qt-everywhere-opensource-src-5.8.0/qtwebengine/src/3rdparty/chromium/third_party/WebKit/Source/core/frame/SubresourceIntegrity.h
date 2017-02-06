// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SubresourceIntegrity_h
#define SubresourceIntegrity_h

#include "base/gtest_prod_util.h"
#include "core/CoreExport.h"
#include "core/fetch/IntegrityMetadata.h"
#include "platform/Crypto.h"
#include "wtf/Allocator.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Document;
class Element;
class KURL;
class Resource;

class CORE_EXPORT SubresourceIntegrity {
    STATIC_ONLY(SubresourceIntegrity);
public:
    enum IntegrityParseResult {
        IntegrityParseValidResult,
        IntegrityParseNoValidResult
    };

    // The versions with the IntegrityMetadataSet passed as the first argument
    // assume that the integrity attribute has already been parsed, and the
    // IntegrityMetadataSet represents the result of that parsing.
    static bool CheckSubresourceIntegrity(const Element&, const char* content, size_t, const KURL& resourceUrl, const Resource&);
    static bool CheckSubresourceIntegrity(const IntegrityMetadataSet&, const Element&, const char* content, size_t, const KURL& resourceUrl, const Resource&);
    static bool CheckSubresourceIntegrity(const String&, const char*, size_t, const KURL& resourceUrl, Document&, WTF::String&);
    static bool CheckSubresourceIntegrity(const IntegrityMetadataSet&, const char*, size_t, const KURL& resourceUrl, Document&, WTF::String&);

    // The IntegrityMetadataSet arguments are out parameters which contain the
    // set of all valid, parsed metadata from |attribute|.
    static IntegrityParseResult parseIntegrityAttribute(const WTF::String& attribute, IntegrityMetadataSet&);
    static IntegrityParseResult parseIntegrityAttribute(const WTF::String& attribute, IntegrityMetadataSet&, Document*);

private:
    friend class SubresourceIntegrityTest;
    FRIEND_TEST_ALL_PREFIXES(SubresourceIntegrityTest, Parsing);
    FRIEND_TEST_ALL_PREFIXES(SubresourceIntegrityTest, ParseAlgorithm);
    FRIEND_TEST_ALL_PREFIXES(SubresourceIntegrityTest, Prioritization);

    enum AlgorithmParseResult {
        AlgorithmValid,
        AlgorithmUnparsable,
        AlgorithmUnknown
    };

    static HashAlgorithm getPrioritizedHashFunction(HashAlgorithm, HashAlgorithm);
    static AlgorithmParseResult parseAlgorithm(const UChar*& begin, const UChar* end, HashAlgorithm&);
    static bool parseDigest(const UChar*& begin, const UChar* end, String& digest);
};

} // namespace blink

#endif
