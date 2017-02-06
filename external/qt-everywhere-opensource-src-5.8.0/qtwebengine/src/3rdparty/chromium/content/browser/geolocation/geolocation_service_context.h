// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_SERVICE_CONTEXT_H_
#define CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_SERVICE_CONTEXT_H_

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "content/browser/geolocation/geolocation_service_impl.h"
#include "third_party/WebKit/public/platform/modules/geolocation/geolocation.mojom.h"

namespace content {

// Provides information to a set of GeolocationServiceImpl instances that are
// associated with a given context. Notably, allows pausing and resuming
// geolocation on these instances.
class GeolocationServiceContext {
 public:
  GeolocationServiceContext();
  virtual ~GeolocationServiceContext();

  // Creates a GeolocationServiceImpl that is weakly bound to |request|.
  // |update_callback| will be called when services send
  // location updates to their clients.
  void CreateService(
      const base::Closure& update_callback,
      mojo::InterfaceRequest<blink::mojom::GeolocationService> request);

  // Called when a service has a connection error. After this call, it is no
  // longer safe to access |service|.
  void ServiceHadConnectionError(GeolocationServiceImpl* service);

  // Pauses and resumes geolocation. Resuming when nothing is paused is a
  // no-op. If a service is added while geolocation is paused, that service
  // will not get geolocation updates until geolocation is resumed.
  void PauseUpdates();
  void ResumeUpdates();

  // Returns whether geolocation updates are currently paused.
  bool paused() { return paused_; }

  // Enables geolocation override. This method can be used to trigger possible
  // location-specific behavior in a particular context.
  void SetOverride(std::unique_ptr<Geoposition> geoposition);

  // Disables geolocation override.
  void ClearOverride();

 private:
  ScopedVector<GeolocationServiceImpl> services_;
  bool paused_;

  std::unique_ptr<Geoposition> geoposition_override_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationServiceContext);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_SERVICE_CONTEXT_H_
