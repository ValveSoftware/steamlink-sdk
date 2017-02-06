// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/session/test_client_session.h"

namespace blimp {
namespace client {
namespace {
// Assigner is not actually used in this test.
const char kAssignerUrl[] = "http://www.assigner.test/";
}  // namespace

TestClientSession::TestClientSession()
    : client::BlimpClientSession(GURL(kAssignerUrl)) {}

TestClientSession::~TestClientSession() {}

}  // namespace client
}  // namespace blimp
