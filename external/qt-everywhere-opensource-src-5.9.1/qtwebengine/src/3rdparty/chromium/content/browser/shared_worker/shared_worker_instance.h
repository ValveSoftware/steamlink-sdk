// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_INSTANCE_H_
#define CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_INSTANCE_H_

#include <string>

#include "content/browser/shared_worker/worker_storage_partition.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebAddressSpace.h"
#include "third_party/WebKit/public/web/WebContentSecurityPolicy.h"
#include "third_party/WebKit/public/web/WebSharedWorkerCreationContextType.h"
#include "url/gurl.h"

namespace content {
class ResourceContext;

// SharedWorkerInstance is copyable value-type data type. It could be passed to
// the UI thread and be used for comparison in SharedWorkerDevToolsManager.
class CONTENT_EXPORT SharedWorkerInstance {
 public:
  SharedWorkerInstance(
      const GURL& url,
      const base::string16& name,
      const base::string16& content_security_policy,
      blink::WebContentSecurityPolicyType security_policy_type,
      blink::WebAddressSpace creation_address_space,
      ResourceContext* resource_context,
      const WorkerStoragePartitionId& partition_id,
      blink::WebSharedWorkerCreationContextType creation_context_type);
  SharedWorkerInstance(const SharedWorkerInstance& other);
  ~SharedWorkerInstance();

  // Checks if this SharedWorkerInstance matches the passed url/name params
  // based on the algorithm in the WebWorkers spec - an instance matches if the
  // origins of the URLs match, and:
  // a) the names are non-empty and equal.
  // -or-
  // b) the names are both empty, and the urls are equal.
  bool Matches(const GURL& url,
               const base::string16& name,
               const WorkerStoragePartitionId& partition,
               ResourceContext* resource_context) const;
  bool Matches(const SharedWorkerInstance& other) const;

  // Accessors.
  const GURL& url() const { return url_; }
  const base::string16 name() const { return name_; }
  const base::string16 content_security_policy() const {
    return content_security_policy_;
  }
  blink::WebContentSecurityPolicyType security_policy_type() const {
    return security_policy_type_;
  }
  blink::WebAddressSpace creation_address_space() const {
    return creation_address_space_;
  }
  ResourceContext* resource_context() const {
    return resource_context_;
  }
  const WorkerStoragePartitionId& partition_id() const { return partition_id_; }
  blink::WebSharedWorkerCreationContextType creation_context_type() const {
    return creation_context_type_;
  }

 private:
  const GURL url_;
  const base::string16 name_;
  const base::string16 content_security_policy_;
  const blink::WebContentSecurityPolicyType security_policy_type_;
  const blink::WebAddressSpace creation_address_space_;
  ResourceContext* const resource_context_;
  const WorkerStoragePartitionId partition_id_;
  const blink::WebSharedWorkerCreationContextType creation_context_type_;
};

}  // namespace content


#endif  // CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_INSTANCE_H_
