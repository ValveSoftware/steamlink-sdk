// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/notifications/NotificationResourcesLoader.h"

#include "platform/Histogram.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/modules/notifications/WebNotificationData.h"
#include "public/platform/modules/notifications/WebNotificationResources.h"
#include "wtf/CurrentTime.h"
#include "wtf/Threading.h"
#include <cmath>

namespace blink {

NotificationResourcesLoader::NotificationResourcesLoader(
    std::unique_ptr<CompletionCallback> completionCallback)
    : m_started(false),
      m_completionCallback(std::move(completionCallback)),
      m_pendingRequestCount(0) {
  ThreadState::current()->registerPreFinalizer(this);
  DCHECK(m_completionCallback);
}

NotificationResourcesLoader::~NotificationResourcesLoader() {}

void NotificationResourcesLoader::start(
    ExecutionContext* executionContext,
    const WebNotificationData& notificationData) {
  DCHECK(!m_started);
  m_started = true;

  size_t numActions = notificationData.actions.size();
  m_pendingRequestCount = 3 /* image, icon, badge */ + numActions;

  // TODO(johnme): ensure image is not loaded when it will not be used.
  // TODO(mvanouwerkerk): ensure no badge is loaded when it will not be used.
  loadImage(executionContext, NotificationImageLoader::Type::Image,
            notificationData.image,
            WTF::bind(&NotificationResourcesLoader::didLoadImage,
                      wrapWeakPersistent(this)));
  loadImage(executionContext, NotificationImageLoader::Type::Icon,
            notificationData.icon,
            WTF::bind(&NotificationResourcesLoader::didLoadIcon,
                      wrapWeakPersistent(this)));
  loadImage(executionContext, NotificationImageLoader::Type::Badge,
            notificationData.badge,
            WTF::bind(&NotificationResourcesLoader::didLoadBadge,
                      wrapWeakPersistent(this)));

  m_actionIcons.resize(numActions);
  for (size_t i = 0; i < numActions; i++)
    loadImage(executionContext, NotificationImageLoader::Type::ActionIcon,
              notificationData.actions[i].icon,
              WTF::bind(&NotificationResourcesLoader::didLoadActionIcon,
                        wrapWeakPersistent(this), i));
}

std::unique_ptr<WebNotificationResources>
NotificationResourcesLoader::getResources() const {
  std::unique_ptr<WebNotificationResources> resources(
      new WebNotificationResources());
  resources->image = m_image;
  resources->icon = m_icon;
  resources->badge = m_badge;
  resources->actionIcons = m_actionIcons;
  return resources;
}

void NotificationResourcesLoader::stop() {
  for (auto imageLoader : m_imageLoaders)
    imageLoader->stop();
}

DEFINE_TRACE(NotificationResourcesLoader) {
  visitor->trace(m_imageLoaders);
}

void NotificationResourcesLoader::loadImage(
    ExecutionContext* executionContext,
    NotificationImageLoader::Type type,
    const KURL& url,
    std::unique_ptr<NotificationImageLoader::ImageCallback> imageCallback) {
  if (url.isNull() || url.isEmpty() || !url.isValid()) {
    didFinishRequest();
    return;
  }

  NotificationImageLoader* imageLoader = new NotificationImageLoader(type);
  m_imageLoaders.append(imageLoader);
  imageLoader->start(executionContext, url, std::move(imageCallback));
}

void NotificationResourcesLoader::didLoadImage(const SkBitmap& image) {
  m_image = NotificationImageLoader::scaleDownIfNeeded(
      image, NotificationImageLoader::Type::Image);
  didFinishRequest();
}

void NotificationResourcesLoader::didLoadIcon(const SkBitmap& image) {
  m_icon = NotificationImageLoader::scaleDownIfNeeded(
      image, NotificationImageLoader::Type::Icon);
  didFinishRequest();
}

void NotificationResourcesLoader::didLoadBadge(const SkBitmap& image) {
  m_badge = NotificationImageLoader::scaleDownIfNeeded(
      image, NotificationImageLoader::Type::Badge);
  didFinishRequest();
}

void NotificationResourcesLoader::didLoadActionIcon(size_t actionIndex,
                                                    const SkBitmap& image) {
  DCHECK_LT(actionIndex, m_actionIcons.size());

  m_actionIcons[actionIndex] = NotificationImageLoader::scaleDownIfNeeded(
      image, NotificationImageLoader::Type::ActionIcon);
  didFinishRequest();
}

void NotificationResourcesLoader::didFinishRequest() {
  DCHECK_GT(m_pendingRequestCount, 0);
  m_pendingRequestCount--;
  if (!m_pendingRequestCount) {
    stop();
    (*m_completionCallback)(this);
    // The |this| pointer may have been deleted now.
  }
}

}  // namespace blink
