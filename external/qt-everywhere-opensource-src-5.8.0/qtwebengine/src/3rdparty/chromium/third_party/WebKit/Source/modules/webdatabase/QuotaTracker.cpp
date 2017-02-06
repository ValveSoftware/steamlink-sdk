/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/webdatabase/QuotaTracker.h"

#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebSecurityOrigin.h"
#include "wtf/StdLibExtras.h"
#include "wtf/Threading.h"

namespace blink {

QuotaTracker& QuotaTracker::instance()
{
    DEFINE_THREAD_SAFE_STATIC_LOCAL(QuotaTracker, tracker, new QuotaTracker);
    return tracker;
}

void QuotaTracker::getDatabaseSizeAndSpaceAvailableToOrigin(
    SecurityOrigin* origin, const String& databaseName,
    unsigned long long* databaseSize, unsigned long long* spaceAvailable)
{
    // Extra scope to unlock prior to potentially calling Platform.
    {
        MutexLocker lockData(m_dataGuard);
        ASSERT(m_databaseSizes.contains(origin->toRawString()));
        HashMap<String, SizeMap>::const_iterator it = m_databaseSizes.find(origin->toRawString());
        ASSERT(it->value.contains(databaseName));
        *databaseSize = it->value.get(databaseName);

        if (m_spaceAvailableToOrigins.contains(origin->toRawString())) {
            *spaceAvailable = m_spaceAvailableToOrigins.get(origin->toRawString());
            return;
        }
    }

    // The embedder hasn't pushed this value to us, so we pull it as needed.
    *spaceAvailable = Platform::current()->databaseGetSpaceAvailableForOrigin(WebSecurityOrigin(origin));
}

void QuotaTracker::updateDatabaseSize(
    SecurityOrigin* origin, const String& databaseName,
    unsigned long long databaseSize)
{
    MutexLocker lockData(m_dataGuard);
    HashMap<String, SizeMap>::ValueType* it = m_databaseSizes.add(origin->toRawString(), SizeMap()).storedValue;
    it->value.set(databaseName, databaseSize);
}

void QuotaTracker::updateSpaceAvailableToOrigin(SecurityOrigin* origin, unsigned long long spaceAvailable)
{
    MutexLocker lockData(m_dataGuard);
    m_spaceAvailableToOrigins.set(origin->toRawString(), spaceAvailable);
}

void QuotaTracker::resetSpaceAvailableToOrigin(SecurityOrigin* origin)
{
    MutexLocker lockData(m_dataGuard);
    m_spaceAvailableToOrigins.remove(origin->toRawString());
}

} // namespace blink
