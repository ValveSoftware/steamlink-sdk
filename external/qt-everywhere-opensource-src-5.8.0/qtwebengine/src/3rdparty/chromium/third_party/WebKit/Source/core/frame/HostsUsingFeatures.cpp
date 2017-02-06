// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/HostsUsingFeatures.h"

#include "bindings/core/v8/ScriptState.h"
#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/page/Page.h"
#include "public/platform/Platform.h"

namespace blink {

HostsUsingFeatures::~HostsUsingFeatures()
{
    updateMeasurementsAndClear();
}

HostsUsingFeatures::Value::Value()
    : m_countBits(0)
{
}

void HostsUsingFeatures::countAnyWorld(Document& document, Feature feature)
{
    document.HostsUsingFeaturesValue().count(feature);
}

void HostsUsingFeatures::countMainWorldOnly(const ScriptState* scriptState, Document& document, Feature feature)
{
    if (!scriptState || !scriptState->world().isMainWorld())
        return;
    countAnyWorld(document, feature);
}

static Document* documentFromEventTarget(EventTarget& target)
{
    ExecutionContext* executionContext = target.getExecutionContext();
    if (!executionContext)
        return nullptr;
    if (executionContext->isDocument())
        return toDocument(executionContext);
    if (LocalDOMWindow* executingWindow = executionContext->executingWindow())
        return executingWindow->document();
    return nullptr;
}

void HostsUsingFeatures::countHostOrIsolatedWorldHumanReadableName(const ScriptState* scriptState, EventTarget& target, Feature feature)
{
    if (!scriptState)
        return;
    Document* document = documentFromEventTarget(target);
    if (!document)
        return;
    if (scriptState->world().isMainWorld()) {
        document->HostsUsingFeaturesValue().count(feature);
        return;
    }
    if (Page* page = document->page())
        page->hostsUsingFeatures().countName(feature, scriptState->world().isolatedWorldHumanReadableName());
}

void HostsUsingFeatures::Value::count(Feature feature)
{
    DCHECK(feature < Feature::NumberOfFeatures);
    m_countBits |= 1 << static_cast<unsigned>(feature);
}

void HostsUsingFeatures::countName(Feature feature, const String& name)
{
    auto result = m_valueByName.add(name, Value());
    result.storedValue->value.count(feature);
}

void HostsUsingFeatures::clear()
{
    m_valueByName.clear();
    m_urlAndValues.clear();
}

void HostsUsingFeatures::documentDetached(Document& document)
{
    HostsUsingFeatures::Value counter = document.HostsUsingFeaturesValue();
    if (counter.isEmpty())
        return;

    const KURL& url = document.url();
    if (!url.protocolIsInHTTPFamily())
        return;

    m_urlAndValues.append(std::make_pair(url, counter));
    document.HostsUsingFeaturesValue().clear();
    DCHECK(document.HostsUsingFeaturesValue().isEmpty());
}

void HostsUsingFeatures::updateMeasurementsAndClear()
{
    if (!m_urlAndValues.isEmpty()) {
        recordHostToRappor();
        recordETLDPlus1ToRappor();
        m_urlAndValues.clear();
    }
    if (!m_valueByName.isEmpty())
        recordNamesToRappor();
}

void HostsUsingFeatures::recordHostToRappor()
{
    DCHECK(!m_urlAndValues.isEmpty());

    // Aggregate values by hosts.
    HashMap<String, HostsUsingFeatures::Value> aggregatedByHost;
    for (const auto& urlAndValue : m_urlAndValues) {
        DCHECK(!urlAndValue.first.isEmpty());
        auto result = aggregatedByHost.add(urlAndValue.first.host(), urlAndValue.second);
        if (!result.isNewEntry)
            result.storedValue->value.aggregate(urlAndValue.second);
    }

    // Report to RAPPOR.
    for (auto& hostAndValue : aggregatedByHost)
        hostAndValue.value.recordHostToRappor(hostAndValue.key);
}

void HostsUsingFeatures::recordETLDPlus1ToRappor()
{
    DCHECK(!m_urlAndValues.isEmpty());

    // Aggregate values by URL.
    HashMap<String, HostsUsingFeatures::Value> aggregatedByURL;
    for (const auto& urlAndValue : m_urlAndValues) {
        DCHECK(!urlAndValue.first.isEmpty());
        auto result = aggregatedByURL.add(urlAndValue.first, urlAndValue.second);
        if (!result.isNewEntry)
            result.storedValue->value.aggregate(urlAndValue.second);
    }

    // Report to RAPPOR.
    for (auto& urlAndValue : aggregatedByURL)
        urlAndValue.value.recordETLDPlus1ToRappor(KURL(ParsedURLString, urlAndValue.key));
}

void HostsUsingFeatures::recordNamesToRappor()
{
    DCHECK(!m_valueByName.isEmpty());

    for (auto& nameAndValue : m_valueByName)
        nameAndValue.value.recordNameToRappor(nameAndValue.key);

    m_valueByName.clear();
}

void HostsUsingFeatures::Value::aggregate(HostsUsingFeatures::Value other)
{
    m_countBits |= other.m_countBits;
}

void HostsUsingFeatures::Value::recordHostToRappor(const String& host)
{
    if (get(Feature::ElementCreateShadowRoot))
        Platform::current()->recordRappor("WebComponents.ElementCreateShadowRoot", host);
    if (get(Feature::ElementAttachShadow))
        Platform::current()->recordRappor("WebComponents.ElementAttachShadow", host);
    if (get(Feature::DocumentRegisterElement))
        Platform::current()->recordRappor("WebComponents.DocumentRegisterElement", host);
    if (get(Feature::EventPath))
        Platform::current()->recordRappor("WebComponents.EventPath", host);
    if (get(Feature::DeviceMotionInsecureHost))
        Platform::current()->recordRappor("PowerfulFeatureUse.Host.DeviceMotion.Insecure", host);
    if (get(Feature::DeviceOrientationInsecureHost))
        Platform::current()->recordRappor("PowerfulFeatureUse.Host.DeviceOrientation.Insecure", host);
    if (get(Feature::FullscreenInsecureHost))
        Platform::current()->recordRappor("PowerfulFeatureUse.Host.Fullscreen.Insecure", host);
    if (get(Feature::GeolocationInsecureHost))
        Platform::current()->recordRappor("PowerfulFeatureUse.Host.Geolocation.Insecure", host);
    if (get(Feature::ApplicationCacheManifestSelectInsecureHost))
        Platform::current()->recordRappor("PowerfulFeatureUse.Host.ApplicationCacheManifestSelect.Insecure", host);
    if (get(Feature::ApplicationCacheAPIInsecureHost))
        Platform::current()->recordRappor("PowerfulFeatureUse.Host.ApplicationCacheAPI.Insecure", host);
}

void HostsUsingFeatures::Value::recordNameToRappor(const String& name)
{
    if (get(Feature::EventPath))
        Platform::current()->recordRappor("WebComponents.EventPath.Extensions", name);
}

void HostsUsingFeatures::Value::recordETLDPlus1ToRappor(const KURL& url)
{
    if (get(Feature::GetUserMediaInsecureHost))
        Platform::current()->recordRapporURL("PowerfulFeatureUse.ETLDPlus1.GetUserMedia.Insecure", WebURL(url));
    if (get(Feature::GetUserMediaSecureHost))
        Platform::current()->recordRapporURL("PowerfulFeatureUse.ETLDPlus1.GetUserMedia.Secure", WebURL(url));
    if (get(Feature::RTCPeerConnectionAudio))
        Platform::current()->recordRapporURL("RTCPeerConnection.Audio", WebURL(url));
    if (get(Feature::RTCPeerConnectionVideo))
        Platform::current()->recordRapporURL("RTCPeerConnection.Video", WebURL(url));
    if (get(Feature::RTCPeerConnectionDataChannel))
        Platform::current()->recordRapporURL("RTCPeerConnection.DataChannel", WebURL(url));
}

} // namespace blink
