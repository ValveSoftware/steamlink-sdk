// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_APP_SWITCHES_H_
#define BLIMP_ENGINE_APP_SWITCHES_H_

namespace blimp {
namespace engine {

// The port to listen to for incoming TCP connections.
extern const char kEnginePort[];

// Flag enabling access by clients from non-localhost IP addresses.
extern const char kAllowNonLocalhost[];

// Transport type: tcp, grpc. If empty, tcp is assumed by default.
extern const char kEngineTransport[];

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_APP_SWITCHES_H_
