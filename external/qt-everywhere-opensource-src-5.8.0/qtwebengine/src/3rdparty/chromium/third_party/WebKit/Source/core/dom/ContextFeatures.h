/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
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

#ifndef ContextFeatures_h
#define ContextFeatures_h

#include "core/CoreExport.h"
#include "core/page/Page.h"
#include "platform/heap/Handle.h"
#include <memory>

namespace blink {

class ContextFeaturesClient;
class Document;
class Page;

class ContextFeatures final : public GarbageCollectedFinalized<ContextFeatures>, public Supplement<Page> {
    USING_GARBAGE_COLLECTED_MIXIN(ContextFeatures);
public:
    enum FeatureType {
        PagePopup = 0,
        MutationEvents,
        FeatureTypeSize // Should be the last entry.
    };

    static const char* supplementName();
    static ContextFeatures& defaultSwitch();
    static ContextFeatures* create(std::unique_ptr<ContextFeaturesClient>);

    static bool pagePopupEnabled(Document*);
    static bool mutationEventsEnabled(Document*);

    bool isEnabled(Document*, FeatureType, bool) const;
    void urlDidChange(Document*);

private:
    explicit ContextFeatures(std::unique_ptr<ContextFeaturesClient> client)
        : m_client(std::move(client))
    { }

    std::unique_ptr<ContextFeaturesClient> m_client;
};

class ContextFeaturesClient {
    USING_FAST_MALLOC(ContextFeaturesClient);
public:
    static std::unique_ptr<ContextFeaturesClient> empty();

    virtual ~ContextFeaturesClient() { }
    virtual bool isEnabled(Document*, ContextFeatures::FeatureType, bool defaultValue) { return defaultValue; }
    virtual void urlDidChange(Document*) { }
};

CORE_EXPORT void provideContextFeaturesTo(Page&, std::unique_ptr<ContextFeaturesClient>);
void provideContextFeaturesToDocumentFrom(Document&, Page&);

inline ContextFeatures* ContextFeatures::create(std::unique_ptr<ContextFeaturesClient> client)
{
    return new ContextFeatures(std::move(client));
}

inline bool ContextFeatures::isEnabled(Document* document, FeatureType type, bool defaultValue) const
{
    if (!m_client)
        return defaultValue;
    return m_client->isEnabled(document, type, defaultValue);
}

inline void ContextFeatures::urlDidChange(Document* document)
{
    // FIXME: The original code, commented out below, is obviously
    // wrong, but the seemingly correct fix of negating the test to
    // the more logical 'if (!m_client)' crashes the renderer.
    // See issue 294180
    //
    // if (m_client)
    //     return;
    // m_client->urlDidChange(document);
}

} // namespace blink

#endif // ContextFeatures_h
