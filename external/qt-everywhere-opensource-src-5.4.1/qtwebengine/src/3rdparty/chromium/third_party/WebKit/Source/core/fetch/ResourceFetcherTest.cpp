/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
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

#include "config.h"
#include "core/fetch/ResourceFetcher.h"

#include <gtest/gtest.h>
#include "core/fetch/FetchInitiatorInfo.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/ResourcePtr.h"
#include "core/html/HTMLDocument.h"
#include "core/loader/DocumentLoader.h"
#include "platform/network/ResourceRequest.h"

using namespace WebCore;

namespace {

TEST(ResourceFetcherTest, StartLoadAfterFrameDetach)
{
    KURL testURL(ParsedURLString, "http://www.test.com/cancelTest.jpg");

    // Create a ResourceFetcher that has a real DocumentLoader and Document, but is not attached to a LocalFrame.
    // Technically, we're concerned about what happens after a LocalFrame is detached (rather than before
    // any attach occurs), but ResourceFetcher can't tell the difference.
    RefPtr<DocumentLoader> documentLoader = DocumentLoader::create(0, ResourceRequest(testURL), SubstituteData());
    RefPtrWillBeRawPtr<HTMLDocument> document = HTMLDocument::create();
    RefPtrWillBeRawPtr<ResourceFetcher> fetcher(documentLoader->fetcher());
    fetcher->setDocument(document.get());
    EXPECT_EQ(fetcher->frame(), static_cast<LocalFrame*>(0));

    // Try to request a url. The request should fail, no resource should be returned,
    // and no resource should be present in the cache.
    FetchRequest fetchRequest = FetchRequest(ResourceRequest(testURL), FetchInitiatorInfo());
    ResourcePtr<ImageResource> image = fetcher->fetchImage(fetchRequest);
    EXPECT_EQ(image.get(), static_cast<ImageResource*>(0));
    EXPECT_EQ(memoryCache()->resourceForURL(testURL), static_cast<Resource*>(0));
}

} // namespace
