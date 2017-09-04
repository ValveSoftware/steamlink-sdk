/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "core/inspector/InspectorApplicationCacheAgent.h"

#include "core/frame/LocalFrame.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectedFrames.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/page/NetworkStateNotifier.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

namespace ApplicationCacheAgentState {
static const char applicationCacheAgentEnabled[] =
    "applicationCacheAgentEnabled";
}

InspectorApplicationCacheAgent::InspectorApplicationCacheAgent(
    InspectedFrames* inspectedFrames)
    : m_inspectedFrames(inspectedFrames) {}

void InspectorApplicationCacheAgent::restore() {
  if (m_state->booleanProperty(
          ApplicationCacheAgentState::applicationCacheAgentEnabled, false)) {
    enable();
  }
}

Response InspectorApplicationCacheAgent::enable() {
  m_state->setBoolean(ApplicationCacheAgentState::applicationCacheAgentEnabled,
                      true);
  m_instrumentingAgents->addInspectorApplicationCacheAgent(this);
  frontend()->networkStateUpdated(networkStateNotifier().onLine());
  return Response::OK();
}

Response InspectorApplicationCacheAgent::disable() {
  m_state->setBoolean(ApplicationCacheAgentState::applicationCacheAgentEnabled,
                      false);
  m_instrumentingAgents->removeInspectorApplicationCacheAgent(this);
  return Response::OK();
}

void InspectorApplicationCacheAgent::updateApplicationCacheStatus(
    LocalFrame* frame) {
  DocumentLoader* documentLoader = frame->loader().documentLoader();
  if (!documentLoader)
    return;

  ApplicationCacheHost* host = documentLoader->applicationCacheHost();
  ApplicationCacheHost::Status status = host->getStatus();
  ApplicationCacheHost::CacheInfo info = host->applicationCacheInfo();

  String manifestURL = info.m_manifest.getString();
  String frameId = IdentifiersFactory::frameId(frame);
  frontend()->applicationCacheStatusUpdated(frameId, manifestURL,
                                            static_cast<int>(status));
}

void InspectorApplicationCacheAgent::networkStateChanged(LocalFrame* frame,
                                                         bool online) {
  if (frame == m_inspectedFrames->root())
    frontend()->networkStateUpdated(online);
}

Response InspectorApplicationCacheAgent::getFramesWithManifests(
    std::unique_ptr<
        protocol::Array<protocol::ApplicationCache::FrameWithManifest>>*
        result) {
  *result =
      protocol::Array<protocol::ApplicationCache::FrameWithManifest>::create();

  for (LocalFrame* frame : *m_inspectedFrames) {
    DocumentLoader* documentLoader = frame->loader().documentLoader();
    if (!documentLoader)
      continue;

    ApplicationCacheHost* host = documentLoader->applicationCacheHost();
    ApplicationCacheHost::CacheInfo info = host->applicationCacheInfo();
    String manifestURL = info.m_manifest.getString();
    if (!manifestURL.isEmpty()) {
      std::unique_ptr<protocol::ApplicationCache::FrameWithManifest> value =
          protocol::ApplicationCache::FrameWithManifest::create()
              .setFrameId(IdentifiersFactory::frameId(frame))
              .setManifestURL(manifestURL)
              .setStatus(static_cast<int>(host->getStatus()))
              .build();
      (*result)->addItem(std::move(value));
    }
  }
  return Response::OK();
}

Response InspectorApplicationCacheAgent::assertFrameWithDocumentLoader(
    String frameId,
    DocumentLoader*& result) {
  LocalFrame* frame = IdentifiersFactory::frameById(m_inspectedFrames, frameId);
  if (!frame)
    return Response::Error("No frame for given id found");

  result = frame->loader().documentLoader();
  if (!result)
    return Response::Error("No documentLoader for given frame found");
  return Response::OK();
}

Response InspectorApplicationCacheAgent::getManifestForFrame(
    const String& frameId,
    String* manifestURL) {
  DocumentLoader* documentLoader = nullptr;
  Response response = assertFrameWithDocumentLoader(frameId, documentLoader);
  if (!response.isSuccess())
    return response;

  ApplicationCacheHost::CacheInfo info =
      documentLoader->applicationCacheHost()->applicationCacheInfo();
  *manifestURL = info.m_manifest.getString();
  return Response::OK();
}

Response InspectorApplicationCacheAgent::getApplicationCacheForFrame(
    const String& frameId,
    std::unique_ptr<protocol::ApplicationCache::ApplicationCache>*
        applicationCache) {
  DocumentLoader* documentLoader = nullptr;
  Response response = assertFrameWithDocumentLoader(frameId, documentLoader);
  if (!response.isSuccess())
    return response;

  ApplicationCacheHost* host = documentLoader->applicationCacheHost();
  ApplicationCacheHost::CacheInfo info = host->applicationCacheInfo();

  ApplicationCacheHost::ResourceInfoList resources;
  host->fillResourceList(&resources);

  *applicationCache = buildObjectForApplicationCache(resources, info);
  return Response::OK();
}

std::unique_ptr<protocol::ApplicationCache::ApplicationCache>
InspectorApplicationCacheAgent::buildObjectForApplicationCache(
    const ApplicationCacheHost::ResourceInfoList& applicationCacheResources,
    const ApplicationCacheHost::CacheInfo& applicationCacheInfo) {
  return protocol::ApplicationCache::ApplicationCache::create()
      .setManifestURL(applicationCacheInfo.m_manifest.getString())
      .setSize(applicationCacheInfo.m_size)
      .setCreationTime(applicationCacheInfo.m_creationTime)
      .setUpdateTime(applicationCacheInfo.m_updateTime)
      .setResources(
          buildArrayForApplicationCacheResources(applicationCacheResources))
      .build();
}

std::unique_ptr<
    protocol::Array<protocol::ApplicationCache::ApplicationCacheResource>>
InspectorApplicationCacheAgent::buildArrayForApplicationCacheResources(
    const ApplicationCacheHost::ResourceInfoList& applicationCacheResources) {
  std::unique_ptr<
      protocol::Array<protocol::ApplicationCache::ApplicationCacheResource>>
      resources = protocol::Array<
          protocol::ApplicationCache::ApplicationCacheResource>::create();

  ApplicationCacheHost::ResourceInfoList::const_iterator end =
      applicationCacheResources.end();
  ApplicationCacheHost::ResourceInfoList::const_iterator it =
      applicationCacheResources.begin();
  for (int i = 0; it != end; ++it, i++)
    resources->addItem(buildObjectForApplicationCacheResource(*it));

  return resources;
}

std::unique_ptr<protocol::ApplicationCache::ApplicationCacheResource>
InspectorApplicationCacheAgent::buildObjectForApplicationCacheResource(
    const ApplicationCacheHost::ResourceInfo& resourceInfo) {
  StringBuilder builder;
  if (resourceInfo.m_isMaster)
    builder.append("Master ");

  if (resourceInfo.m_isManifest)
    builder.append("Manifest ");

  if (resourceInfo.m_isFallback)
    builder.append("Fallback ");

  if (resourceInfo.m_isForeign)
    builder.append("Foreign ");

  if (resourceInfo.m_isExplicit)
    builder.append("Explicit ");

  std::unique_ptr<protocol::ApplicationCache::ApplicationCacheResource> value =
      protocol::ApplicationCache::ApplicationCacheResource::create()
          .setUrl(resourceInfo.m_resource.getString())
          .setSize(static_cast<int>(resourceInfo.m_size))
          .setType(builder.toString())
          .build();
  return value;
}

DEFINE_TRACE(InspectorApplicationCacheAgent) {
  visitor->trace(m_inspectedFrames);
  InspectorBaseAgent::trace(visitor);
}

}  // namespace blink
