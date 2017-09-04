// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_instance.h"

#include "base/logging.h"

namespace content {

SharedWorkerInstance::SharedWorkerInstance(
    const GURL& url,
    const base::string16& name,
    const base::string16& content_security_policy,
    blink::WebContentSecurityPolicyType security_policy_type,
    blink::WebAddressSpace creation_address_space,
    ResourceContext* resource_context,
    const WorkerStoragePartitionId& partition_id,
    blink::WebSharedWorkerCreationContextType creation_context_type)
    : url_(url),
      name_(name),
      content_security_policy_(content_security_policy),
      security_policy_type_(security_policy_type),
      creation_address_space_(creation_address_space),
      resource_context_(resource_context),
      partition_id_(partition_id),
      creation_context_type_(creation_context_type) {
  DCHECK(resource_context_);
}

SharedWorkerInstance::SharedWorkerInstance(const SharedWorkerInstance& other)
    : url_(other.url_),
      name_(other.name_),
      content_security_policy_(other.content_security_policy_),
      security_policy_type_(other.security_policy_type_),
      creation_address_space_(other.creation_address_space_),
      resource_context_(other.resource_context_),
      partition_id_(other.partition_id_),
      creation_context_type_(other.creation_context_type_) {}

SharedWorkerInstance::~SharedWorkerInstance() {}

bool SharedWorkerInstance::Matches(const GURL& match_url,
                                   const base::string16& match_name,
                                   const WorkerStoragePartitionId& partition_id,
                                   ResourceContext* resource_context) const {
  // ResourceContext equivalence is being used as a proxy to ensure we only
  // matched shared workers within the same BrowserContext.
  if (resource_context_ != resource_context)
    return false;

  // We must be in the same storage partition otherwise sharing will violate
  // isolation.
  if (!partition_id_.Equals(partition_id))
    return false;

  if (url_.GetOrigin() != match_url.GetOrigin())
    return false;

  if (name_.empty() && match_name.empty())
    return url_ == match_url;

  return name_ == match_name;
}

bool SharedWorkerInstance::Matches(const SharedWorkerInstance& other) const {
  return Matches(other.url(),
                 other.name(),
                 other.partition_id(),
                 other.resource_context());
}

}  // namespace content
