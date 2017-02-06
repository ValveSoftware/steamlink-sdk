// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/network/NetworkUtils.h"

#include "net/base/ip_address.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/WTFString.h"

namespace blink {

TEST(NetworkUtilsTest, IsReservedIPAddress)
{
    // Unreserved IPv4 addresses (in various forms).
    EXPECT_FALSE(NetworkUtils::isReservedIPAddress("8.8.8.8"));
    EXPECT_FALSE(NetworkUtils::isReservedIPAddress("99.64.0.0"));
    EXPECT_FALSE(NetworkUtils::isReservedIPAddress("212.15.0.0"));
    EXPECT_FALSE(NetworkUtils::isReservedIPAddress("212.15"));
    EXPECT_FALSE(NetworkUtils::isReservedIPAddress("212.15.0"));
    EXPECT_FALSE(NetworkUtils::isReservedIPAddress("3557752832"));

    // Reserved IPv4 addresses (in various forms).
    EXPECT_TRUE(NetworkUtils::isReservedIPAddress("192.168.0.0"));
    EXPECT_TRUE(NetworkUtils::isReservedIPAddress("192.168.0.6"));
    EXPECT_TRUE(NetworkUtils::isReservedIPAddress("10.0.0.5"));
    EXPECT_TRUE(NetworkUtils::isReservedIPAddress("10.0.0"));
    EXPECT_TRUE(NetworkUtils::isReservedIPAddress("10.0"));
    EXPECT_TRUE(NetworkUtils::isReservedIPAddress("3232235526"));

    // Unreserved IPv6 addresses.
    EXPECT_FALSE(NetworkUtils::isReservedIPAddress(
        "[FFC0:ba98:7654:3210:FEDC:BA98:7654:3210]"));
    EXPECT_FALSE(NetworkUtils::isReservedIPAddress(
        "[2000:ba98:7654:2301:EFCD:BA98:7654:3210]"));

    // Reserved IPv6 addresses.
    EXPECT_TRUE(NetworkUtils::isReservedIPAddress("[::1]"));
    EXPECT_TRUE(NetworkUtils::isReservedIPAddress("[::192.9.5.5]"));
    EXPECT_TRUE(NetworkUtils::isReservedIPAddress("[FEED::BEEF]"));
    EXPECT_TRUE(NetworkUtils::isReservedIPAddress(
        "[FEC0:ba98:7654:3210:FEDC:BA98:7654:3210]"));

    // Not IP addresses at all.
    EXPECT_FALSE(NetworkUtils::isReservedIPAddress("example.com"));
    EXPECT_FALSE(NetworkUtils::isReservedIPAddress("127.0.0.1.example.com"));

    // Moar IPv4
    for (int i = 0; i < 256; i++) {
        net::IPAddress address(i, 0, 0, 1);
        std::string addressString = address.ToString();
        if (i == 0 || i == 10 || i == 127 || i > 223) {
            EXPECT_TRUE(NetworkUtils::isReservedIPAddress(
                String::fromUTF8(addressString.data(),
                    addressString.length())));
        } else {
            EXPECT_FALSE(NetworkUtils::isReservedIPAddress(
                String::fromUTF8(addressString.data(),
                    addressString.length())));
        }
    }
}

} // namespace blink
