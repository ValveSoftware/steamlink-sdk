// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fetch/CSSStyleSheetResource.h"

#include "core/css/CSSCrossfadeValue.h"
#include "core/css/CSSImageValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSProperty.h"
#include "core/css/CSSSelectorList.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/parser/CSSParserMode.h"
#include "core/css/parser/CSSParserSelector.h"
#include "core/dom/Document.h"
#include "core/fetch/FetchContext.h"
#include "core/fetch/FetchInitiatorTypeNames.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Heap.h"
#include "platform/network/ResourceRequest.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLResponse.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

class Document;

namespace {

class CSSStyleSheetResourceTest : public ::testing::Test {
protected:
    CSSStyleSheetResourceTest()
    {
        m_originalMemoryCache = replaceMemoryCacheForTesting(MemoryCache::create());
        m_page = DummyPageHolder::create();
        document()->setURL(KURL(KURL(), "https://localhost/"));
    }

    ~CSSStyleSheetResourceTest() override
    {
        replaceMemoryCacheForTesting(m_originalMemoryCache.release());
    }

    Document* document() { return &m_page->document(); }

    Persistent<MemoryCache> m_originalMemoryCache;
    std::unique_ptr<DummyPageHolder> m_page;
};

TEST_F(CSSStyleSheetResourceTest, PruneCanCauseEviction)
{
    {
        // We need this scope to remove all references.
        KURL imageURL(KURL(), "https://localhost/image");
        KURL cssURL(KURL(), "https://localhost/style.css");

        // We need to disable loading because we manually give a response to
        // the image resource.
        document()->fetcher()->setAutoLoadImages(false);

        CSSStyleSheetResource* cssResource = CSSStyleSheetResource::createForTest(ResourceRequest(cssURL), "utf-8");
        memoryCache()->add(cssResource);
        cssResource->responseReceived(ResourceResponse(cssURL, "style/css", 0, nullAtom, String()), nullptr);
        cssResource->finish();

        StyleSheetContents* contents = StyleSheetContents::create(CSSParserContext(HTMLStandardMode, nullptr));
        CSSStyleSheet* sheet = CSSStyleSheet::create(contents, document());
        EXPECT_TRUE(sheet);
        CSSCrossfadeValue* crossfade = CSSCrossfadeValue::create(
            CSSImageValue::create(String("image"), imageURL),
            CSSImageValue::create(String("image"), imageURL),
            CSSPrimitiveValue::create(1.0, CSSPrimitiveValue::UnitType::Number));
        Vector<std::unique_ptr<CSSParserSelector>> selectors;
        selectors.append(wrapUnique(new CSSParserSelector()));
        selectors[0]->setMatch(CSSSelector::Id);
        selectors[0]->setValue("foo");
        CSSProperty property(CSSPropertyBackground, *crossfade);
        contents->parserAppendRule(
            StyleRule::create(CSSSelectorList::adoptSelectorVector(selectors), ImmutableStylePropertySet::create(&property, 1, HTMLStandardMode)));

        crossfade->loadSubimages(document());
        Resource* imageResource = memoryCache()->resourceForURL(imageURL, MemoryCache::defaultCacheIdentifier());
        ASSERT_TRUE(imageResource);
        ResourceResponse imageResponse;
        imageResponse.setURL(imageURL);
        imageResponse.setHTTPHeaderField("cache-control", "no-store");
        imageResource->responseReceived(imageResponse, nullptr);

        contents->checkLoaded();
        cssResource->saveParsedStyleSheet(contents);

        memoryCache()->update(cssResource, cssResource->size(), cssResource->size(), false);
        memoryCache()->update(imageResource, imageResource->size(), imageResource->size(), false);
        if (!memoryCache()->isInSameLRUListForTest(cssResource, imageResource)) {
            // We assume that the LRU list is determined by |size / accessCount|.
            for (size_t i = 0; i < cssResource->size() + 1; ++i)
                memoryCache()->update(cssResource, cssResource->size(), cssResource->size(), true);
            for (size_t i = 0; i < imageResource->size() + 1; ++i)
                memoryCache()->update(imageResource, imageResource->size(), imageResource->size(), true);
        }
        ASSERT_TRUE(memoryCache()->isInSameLRUListForTest(cssResource, imageResource));
    }
    ThreadHeap::collectAllGarbage();
    // This operation should not lead to crash!
    memoryCache()->pruneAll();
}

TEST_F(CSSStyleSheetResourceTest, DuplicateResourceNotCached)
{
    const char url[] = "https://localhost/style.css";
    KURL imageURL(KURL(), url);
    KURL cssURL(KURL(), url);

    // Emulate using <img> to do async stylesheet preloads.

    Resource* imageResource = ImageResource::create(ResourceRequest(imageURL));
    ASSERT_TRUE(imageResource);
    memoryCache()->add(imageResource);
    ASSERT_TRUE(memoryCache()->contains(imageResource));

    CSSStyleSheetResource* cssResource = CSSStyleSheetResource::createForTest(ResourceRequest(cssURL), "utf-8");
    cssResource->responseReceived(ResourceResponse(cssURL, "style/css", 0, nullAtom, String()), nullptr);
    cssResource->finish();

    CSSParserContext parserContext(HTMLStandardMode, nullptr);
    StyleSheetContents* contents = StyleSheetContents::create(parserContext);
    CSSStyleSheet* sheet = CSSStyleSheet::create(contents, document());
    EXPECT_TRUE(sheet);

    contents->checkLoaded();
    cssResource->saveParsedStyleSheet(contents);

    // Verify that the cache will have a mapping for |imageResource| at |url|.
    // The underlying |contents| for the stylesheet resource must have a
    // matching reference status.
    EXPECT_TRUE(memoryCache()->contains(imageResource));
    EXPECT_FALSE(memoryCache()->contains(cssResource));
    EXPECT_FALSE(contents->isReferencedFromResource());
    EXPECT_FALSE(cssResource->restoreParsedStyleSheet(parserContext));
}

} // namespace
} // namespace blink
