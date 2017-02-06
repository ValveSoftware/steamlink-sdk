// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fetch/IntegrityMetadata.h"

namespace blink {

IntegrityMetadata::IntegrityMetadata(WTF::String digest, HashAlgorithm algorithm)
    : m_digest(digest), m_algorithm(algorithm)
{
}

IntegrityMetadata::IntegrityMetadata(std::pair<WTF::String, HashAlgorithm> pair)
    : m_digest(pair.first), m_algorithm(pair.second)
{
}

std::pair<WTF::String, HashAlgorithm> IntegrityMetadata::toPair() const
{
    return std::pair<WTF::String, HashAlgorithm>(m_digest, m_algorithm);
}

bool IntegrityMetadata::setsEqual(const IntegrityMetadataSet& set1, const IntegrityMetadataSet& set2)
{
    if (set1.size() != set2.size())
        return false;

    for (const IntegrityMetadataPair& metadata : set1) {
        if (!set2.contains(metadata))
            return false;
    }

    return true;
}

} // namespace blink
