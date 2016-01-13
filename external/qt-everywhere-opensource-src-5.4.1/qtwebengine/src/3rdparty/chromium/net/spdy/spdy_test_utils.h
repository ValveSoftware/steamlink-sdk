// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_TEST_UTILS_H_
#define NET_SPDY_TEST_UTILS_H_

#include <string>

#include "net/spdy/spdy_protocol.h"

namespace net {

namespace test {

std::string HexDumpWithMarks(const unsigned char* data, int length,
                             const bool* marks, int mark_length);

void CompareCharArraysWithHexError(
    const std::string& description,
    const unsigned char* actual,
    const int actual_len,
    const unsigned char* expected,
    const int expected_len);

void SetFrameFlags(SpdyFrame* frame,
                   uint8 flags,
                   SpdyMajorVersion spdy_version);

void SetFrameLength(SpdyFrame* frame,
                    size_t length,
                    SpdyMajorVersion spdy_version);

std::string a2b_hex(const char* hex_data);

}  // namespace test

}  // namespace net

#endif  // NET_SPDY_TEST_UTILS_H_
