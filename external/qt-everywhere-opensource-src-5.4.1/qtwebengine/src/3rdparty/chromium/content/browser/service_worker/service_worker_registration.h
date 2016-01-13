// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_H_

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerRegistrationInfo;
class ServiceWorkerVersion;

// This class manages all persistence of service workers:
//  - Registrations
//  - Mapping of caches to registrations / versions
//
// This is the place where we manage simultaneous
// requests for the same registrations and caches, making sure that
// two pages that are registering the same pattern at the same time
// have their registrations coalesced rather than overwriting each
// other.
//
// This class also manages the state of the upgrade process, which
// includes managing which ServiceWorkerVersion is "active" vs "in
// waiting".
class CONTENT_EXPORT ServiceWorkerRegistration
    : NON_EXPORTED_BASE(public base::RefCounted<ServiceWorkerRegistration>) {
 public:
  ServiceWorkerRegistration(const GURL& pattern,
                            const GURL& script_url,
                            int64 registration_id,
                            base::WeakPtr<ServiceWorkerContextCore> context);

  int64 id() const { return registration_id_; }
  const GURL& script_url() const { return script_url_; }
  const GURL& pattern() const { return pattern_; }

  ServiceWorkerVersion* active_version() const {
    DCHECK(!is_shutdown_);
    return active_version_.get();
  }

  ServiceWorkerVersion* waiting_version() const {
    DCHECK(!is_shutdown_);
    return waiting_version_.get();
  }

  void set_active_version(ServiceWorkerVersion* version) {
    DCHECK(!is_shutdown_);
    active_version_ = version;
  }

  void set_waiting_version(ServiceWorkerVersion* version) {
    DCHECK(!is_shutdown_);
    waiting_version_ = version;
  }

  ServiceWorkerRegistrationInfo GetInfo();

  // Returns the active version, if it is not null; otherwise, returns the
  // waiting version.
  ServiceWorkerVersion* GetNewestVersion();

  // The final synchronous switchover after all events have been
  // fired, and the old "active version" is being shut down.
  void ActivateWaitingVersion();

 private:
  ~ServiceWorkerRegistration();
  friend class base::RefCounted<ServiceWorkerRegistration>;

  const GURL pattern_;
  const GURL script_url_;
  const int64 registration_id_;

  scoped_refptr<ServiceWorkerVersion> active_version_;
  scoped_refptr<ServiceWorkerVersion> waiting_version_;

  bool is_shutdown_;
  base::WeakPtr<ServiceWorkerContextCore> context_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRegistration);
};
}  // namespace content
#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_H_
