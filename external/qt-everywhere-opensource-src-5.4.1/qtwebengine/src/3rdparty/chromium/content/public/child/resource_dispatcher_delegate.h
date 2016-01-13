// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_CHILD_RESOURCE_DISPATCHER_DELEGATE_H_
#define CONTENT_PUBLIC_CHILD_RESOURCE_DISPATCHER_DELEGATE_H_

#include <string>

#include "content/common/content_export.h"
#include "webkit/common/resource_type.h"

class GURL;

namespace content {

class RequestPeer;

// Interface that allows observing request events and optionally replacing the
// peer.
class CONTENT_EXPORT ResourceDispatcherDelegate {
 public:
  virtual ~ResourceDispatcherDelegate() {}

  virtual RequestPeer* OnRequestComplete(
      RequestPeer* current_peer,
      ResourceType::Type resource_type,
      int error_code) = 0;

  virtual RequestPeer* OnReceivedResponse(
      RequestPeer* current_peer,
      const std::string& mime_type,
      const GURL& url) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_CHILD_RESOURCE_DISPATCHER_DELEGATE_H_
