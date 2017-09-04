// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_CLIENT_SERVICE_MAP_H_
#define GPU_COMMAND_BUFFER_SERVICE_CLIENT_SERVICE_MAP_H_

#include <limits>
#include <unordered_map>

namespace gpu {

namespace gles2 {

template <typename ClientType, typename ServiceType>
class ClientServiceMap {
 public:
  ClientServiceMap() : client_to_service_() {}

  void SetIDMapping(ClientType client_id, ServiceType service_id) {
    DCHECK(client_to_service_.find(client_id) == client_to_service_.end());
    DCHECK(service_id != invalid_service_id());
    client_to_service_[client_id] = service_id;
  }

  void RemoveClientID(ClientType client_id) {
    client_to_service_.erase(client_id);
  }

  void Clear() { client_to_service_.clear(); }

  bool GetServiceID(ClientType client_id, ServiceType* service_id) const {
    if (client_id == 0) {
      if (service_id) {
        *service_id = 0;
      }
      return true;
    }
    auto iter = client_to_service_.find(client_id);
    if (iter != client_to_service_.end()) {
      if (service_id) {
        *service_id = iter->second;
      }
      return true;
    }
    return false;
  }

  ServiceType GetServiceIDOrInvalid(ClientType client_id) {
    ServiceType service_id;
    if (GetServiceID(client_id, &service_id)) {
      return service_id;
    }
    return invalid_service_id();
  }

  ServiceType invalid_service_id() const {
    return std::numeric_limits<ServiceType>::max();
  }

  typedef typename std::unordered_map<ClientType, ServiceType>::const_iterator
      const_iterator;
  const_iterator begin() const { return client_to_service_.begin(); }
  const_iterator end() const { return client_to_service_.end(); }

 private:
  std::unordered_map<ClientType, ServiceType> client_to_service_;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_CLIENT_SERVICE_MAP_H_