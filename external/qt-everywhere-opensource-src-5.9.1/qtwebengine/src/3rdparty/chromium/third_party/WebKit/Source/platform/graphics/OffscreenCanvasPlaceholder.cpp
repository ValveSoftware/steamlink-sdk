// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/OffscreenCanvasPlaceholder.h"

#include "platform/CrossThreadFunctional.h"
#include "platform/WebTaskRunner.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/OffscreenCanvasFrameDispatcher.h"
#include "wtf/HashMap.h"

namespace {

typedef HashMap<int, blink::OffscreenCanvasPlaceholder*> PlaceholderIdMap;

PlaceholderIdMap& placeholderRegistry() {
  DCHECK(isMainThread());
  DEFINE_STATIC_LOCAL(PlaceholderIdMap, s_placeholderRegistry, ());
  return s_placeholderRegistry;
}

void releaseFrameToDispatcher(
    WeakPtr<blink::OffscreenCanvasFrameDispatcher> dispatcher,
    RefPtr<blink::Image> oldImage,
    unsigned resourceId) {
  oldImage = nullptr;  // Needed to unref'ed on the right thread
  if (dispatcher) {
    dispatcher->reclaimResource(resourceId);
  }
}

}  // unnamed namespace

namespace blink {

OffscreenCanvasPlaceholder::~OffscreenCanvasPlaceholder() {
  unregisterPlaceholder();
}

OffscreenCanvasPlaceholder* OffscreenCanvasPlaceholder::getPlaceholderById(
    unsigned placeholderId) {
  PlaceholderIdMap::iterator it = placeholderRegistry().find(placeholderId);
  if (it == placeholderRegistry().end())
    return nullptr;
  return it->value;
}

void OffscreenCanvasPlaceholder::registerPlaceholder(unsigned placeholderId) {
  DCHECK(!placeholderRegistry().contains(placeholderId));
  DCHECK(!isPlaceholderRegistered());
  placeholderRegistry().add(placeholderId, this);
  m_placeholderId = placeholderId;
}

void OffscreenCanvasPlaceholder::unregisterPlaceholder() {
  if (!isPlaceholderRegistered())
    return;
  DCHECK(placeholderRegistry().find(m_placeholderId)->value == this);
  placeholderRegistry().remove(m_placeholderId);
  m_placeholderId = kNoPlaceholderId;
}

void OffscreenCanvasPlaceholder::setPlaceholderFrame(
    RefPtr<Image> newFrame,
    WeakPtr<OffscreenCanvasFrameDispatcher> dispatcher,
    std::unique_ptr<WebTaskRunner> taskRunner,
    unsigned resourceId) {
  DCHECK(isPlaceholderRegistered());
  DCHECK(newFrame);
  releasePlaceholderFrame();
  m_placeholderFrame = std::move(newFrame);
  m_frameDispatcher = std::move(dispatcher);
  m_frameDispatcherTaskRunner = std::move(taskRunner);
  m_placeholderFrameResourceId = resourceId;
  setSize(m_placeholderFrame->size());
}

void OffscreenCanvasPlaceholder::releasePlaceholderFrame() {
  DCHECK(isPlaceholderRegistered());
  if (m_placeholderFrame) {
    m_placeholderFrame->transfer();
    m_frameDispatcherTaskRunner->postTask(
        BLINK_FROM_HERE,
        crossThreadBind(releaseFrameToDispatcher, std::move(m_frameDispatcher),
                        std::move(m_placeholderFrame),
                        m_placeholderFrameResourceId));
  }
}

}  // blink
