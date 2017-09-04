// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_PHYSICAL_WEB_DATA_SOURCE_PHYSICAL_WEB_DATA_SOURCE_IMPL_H_
#define COMPONENTS_PHYSICAL_WEB_DATA_SOURCE_PHYSICAL_WEB_DATA_SOURCE_IMPL_H_

#include "base/observer_list.h"
#include "components/physical_web/data_source/physical_web_data_source.h"

class PhysicalWebListener;

class PhysicalWebDataSourceImpl : public PhysicalWebDataSource {
 public:
  PhysicalWebDataSourceImpl();
  ~PhysicalWebDataSourceImpl() override;

  // Register for changes to Physical Web URLs and associated page metadata.
  void RegisterListener(PhysicalWebListener* physical_web_listener) override;

  // Unregister for changes to Physical Web URLs and associated page metadata.
  void UnregisterListener(PhysicalWebListener* physical_web_listener) override;

  // Notify all registered listeners that a URL has been found.
  void NotifyOnFound(const std::string& url);

  // Notify all registered listeners that a URL has been lost.
  void NotifyOnLost(const std::string& url);

  // Notify all registered listeners that a distance has changed for a URL.
  void NotifyOnDistanceChanged(const std::string& url,
                               double distance_estimate);

 private:
  base::ObserverList<PhysicalWebListener> observer_list_;
};

#endif  // COMPONENTS_PHYSICAL_WEB_DATA_SOURCE_PHYSICAL_WEB_DATA_SOURCE_IMPL_H_
