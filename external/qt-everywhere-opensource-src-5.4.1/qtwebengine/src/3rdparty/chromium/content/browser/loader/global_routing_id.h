// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_GLOBAL_ROUTING_ID_H_
#define CONTENT_PUBLIC_BROWSER_GLOBAL_ROUTING_ID_H_

namespace content {

// Uniquely identifies the route from which a net::URLRequest comes.
struct GlobalRoutingID {
  GlobalRoutingID() : child_id(-1), route_id(-1) {
  }

  GlobalRoutingID(int child_id, int route_id)
      : child_id(child_id),
        route_id(route_id) {
  }

  // The unique ID of the child process (different from OS's PID).
  int child_id;

  // The route ID (unique for each URLRequest source).
  int route_id;

  bool operator<(const GlobalRoutingID& other) const {
    if (child_id == other.child_id)
      return route_id < other.route_id;
    return child_id < other.child_id;
  }
  bool operator==(const GlobalRoutingID& other) const {
    return child_id == other.child_id &&
        route_id == other.route_id;
  }
  bool operator!=(const GlobalRoutingID& other) const {
    return !(*this == other);
  }
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_GLOBAL_ROUTING_ID_H_
