// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IntegrityMetadata_h
#define IntegrityMetadata_h

#include "core/CoreExport.h"
#include "platform/Crypto.h"
#include "wtf/HashSet.h"
#include "wtf/StringHasher.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

namespace blink {

class IntegrityMetadata;

using IntegrityMetadataPair = std::pair<WTF::String, HashAlgorithm>;
using IntegrityMetadataSet = WTF::HashSet<IntegrityMetadataPair>;

class CORE_EXPORT IntegrityMetadata {
public:
    IntegrityMetadata() {}
    IntegrityMetadata(WTF::String digest, HashAlgorithm);
    IntegrityMetadata(std::pair<WTF::String, HashAlgorithm>);

    WTF::String digest() const { return m_digest; }
    void setDigest(const WTF::String& digest) { m_digest = digest; }
    HashAlgorithm algorithm() const { return m_algorithm; }
    void setAlgorithm(HashAlgorithm algorithm) { m_algorithm = algorithm; }

    IntegrityMetadataPair toPair() const;

    static bool setsEqual(const IntegrityMetadataSet& set1, const IntegrityMetadataSet& set2);

private:
    WTF::String m_digest;
    HashAlgorithm m_algorithm;
};

} // namespace blink

#endif
