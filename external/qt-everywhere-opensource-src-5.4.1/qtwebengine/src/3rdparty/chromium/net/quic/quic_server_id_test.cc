// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_server_id.h"

#include "testing/gtest/include/gtest/gtest.h"

using std::string;

namespace net {

namespace {

TEST(QuicServerIdTest, ToString) {
  HostPortPair google_host_port_pair("google.com", 10);

  QuicServerId google_http_server_id(google_host_port_pair, false,
                                     PRIVACY_MODE_DISABLED);
  string google_http_server_id_str = google_http_server_id.ToString();
  EXPECT_EQ("http://google.com:10", google_http_server_id_str);

  QuicServerId google_https_server_id(google_host_port_pair, true,
                                      PRIVACY_MODE_DISABLED);
  string google_https_server_id_str = google_https_server_id.ToString();
  EXPECT_EQ("https://google.com:10", google_https_server_id_str);

  QuicServerId private_http_server_id(google_host_port_pair, false,
                                      PRIVACY_MODE_ENABLED);
  string private_http_server_id_str = private_http_server_id.ToString();
  EXPECT_EQ("http://google.com:10/private", private_http_server_id_str);

  QuicServerId private_https_server_id(google_host_port_pair, true,
                                       PRIVACY_MODE_ENABLED);
  string private_https_server_id_str = private_https_server_id.ToString();
  EXPECT_EQ("https://google.com:10/private", private_https_server_id_str);
}

TEST(QuicServerIdTest, LessThan) {
  QuicServerId a_10_http(HostPortPair("a.com", 10), false,
                         PRIVACY_MODE_DISABLED);
  QuicServerId a_10_https(HostPortPair("a.com", 10), true,
                          PRIVACY_MODE_DISABLED);
  QuicServerId a_11_http(HostPortPair("a.com", 11), false,
                         PRIVACY_MODE_DISABLED);
  QuicServerId a_11_https(HostPortPair("a.com", 11), true,
                          PRIVACY_MODE_DISABLED);
  QuicServerId b_10_http(HostPortPair("b.com", 10), false,
                         PRIVACY_MODE_DISABLED);
  QuicServerId b_10_https(HostPortPair("b.com", 10), true,
                          PRIVACY_MODE_DISABLED);
  QuicServerId b_11_http(HostPortPair("b.com", 11), false,
                         PRIVACY_MODE_DISABLED);
  QuicServerId b_11_https(HostPortPair("b.com", 11), true,
                          PRIVACY_MODE_DISABLED);

  QuicServerId a_10_http_private(HostPortPair("a.com", 10), false,
                                 PRIVACY_MODE_ENABLED);
  QuicServerId a_10_https_private(HostPortPair("a.com", 10), true,
                                  PRIVACY_MODE_ENABLED);
  QuicServerId a_11_http_private(HostPortPair("a.com", 11), false,
                                 PRIVACY_MODE_ENABLED);
  QuicServerId a_11_https_private(HostPortPair("a.com", 11), true,
                                  PRIVACY_MODE_ENABLED);
  QuicServerId b_10_http_private(HostPortPair("b.com", 10), false,
                                 PRIVACY_MODE_ENABLED);
  QuicServerId b_10_https_private(HostPortPair("b.com", 10), true,
                                  PRIVACY_MODE_ENABLED);
  QuicServerId b_11_http_private(HostPortPair("b.com", 11), false,
                                 PRIVACY_MODE_ENABLED);
  QuicServerId b_11_https_private(HostPortPair("b.com", 11), true,
                                  PRIVACY_MODE_ENABLED);

  // Test combinations of host, port, https and privacy being same on left and
  // right side of less than.
  EXPECT_FALSE(a_10_http  < a_10_http);
  EXPECT_TRUE(a_10_http   < a_10_https);
  EXPECT_FALSE(a_10_https < a_10_http);
  EXPECT_FALSE(a_10_https < a_10_https);

  EXPECT_TRUE(a_10_http   < a_10_http_private);
  EXPECT_TRUE(a_10_http   < a_10_https_private);
  EXPECT_FALSE(a_10_https < a_10_http_private);
  EXPECT_TRUE(a_10_https  < a_10_https_private);

  EXPECT_FALSE(a_10_http_private  < a_10_http);
  EXPECT_TRUE(a_10_http_private   < a_10_https);
  EXPECT_FALSE(a_10_https_private < a_10_http);
  EXPECT_FALSE(a_10_https_private < a_10_https);

  EXPECT_FALSE(a_10_http_private  < a_10_http_private);
  EXPECT_TRUE(a_10_http_private   < a_10_https_private);
  EXPECT_FALSE(a_10_https_private < a_10_http_private);
  EXPECT_FALSE(a_10_https_private < a_10_https_private);

  // Test with either host, port or https being different on left and right side
  // of less than.
  PrivacyMode left_privacy;
  PrivacyMode right_privacy;
  for (int i = 0; i < 4; i++) {
    switch (i) {
      case 0:
        left_privacy = PRIVACY_MODE_DISABLED;
        right_privacy = PRIVACY_MODE_DISABLED;
        break;
      case 1:
        left_privacy = PRIVACY_MODE_DISABLED;
        right_privacy = PRIVACY_MODE_ENABLED;
        break;
      case 2:
        left_privacy = PRIVACY_MODE_ENABLED;
        right_privacy = PRIVACY_MODE_DISABLED;
        break;
      case 3:
        left_privacy = PRIVACY_MODE_ENABLED;
        right_privacy = PRIVACY_MODE_ENABLED;
        break;
    }
    QuicServerId a_10_http_left_private(HostPortPair("a.com", 10), false,
                                        left_privacy);
    QuicServerId a_10_http_right_private(HostPortPair("a.com", 10), false,
                                         right_privacy);
    QuicServerId a_10_https_left_private(HostPortPair("a.com", 10), true,
                                         left_privacy);
    QuicServerId a_10_https_right_private(HostPortPair("a.com", 10), true,
                                          right_privacy);
    QuicServerId a_11_http_left_private(HostPortPair("a.com", 11), false,
                                        left_privacy);
    QuicServerId a_11_http_right_private(HostPortPair("a.com", 11), false,
                                         right_privacy);
    QuicServerId a_11_https_left_private(HostPortPair("a.com", 11), true,
                                         left_privacy);
    QuicServerId a_11_https_right_private(HostPortPair("a.com", 11), true,
                                          right_privacy);

    QuicServerId b_10_http_left_private(HostPortPair("b.com", 10), false,
                                        left_privacy);
    QuicServerId b_10_http_right_private(HostPortPair("b.com", 10), false,
                                         right_privacy);
    QuicServerId b_10_https_left_private(HostPortPair("b.com", 10), true,
                                         left_privacy);
    QuicServerId b_10_https_right_private(HostPortPair("b.com", 10), true,
                                          right_privacy);
    QuicServerId b_11_http_left_private(HostPortPair("b.com", 11), false,
                                        left_privacy);
    QuicServerId b_11_http_right_private(HostPortPair("b.com", 11), false,
                                         right_privacy);
    QuicServerId b_11_https_left_private(HostPortPair("b.com", 11), true,
                                         left_privacy);
    QuicServerId b_11_https_right_private(HostPortPair("b.com", 11), true,
                                          right_privacy);

    EXPECT_TRUE(a_10_http_left_private  < a_11_http_right_private);
    EXPECT_TRUE(a_10_http_left_private  < a_11_https_right_private);
    EXPECT_TRUE(a_10_https_left_private < a_11_http_right_private);
    EXPECT_TRUE(a_10_https_left_private < a_11_https_right_private);

    EXPECT_TRUE(a_10_http_left_private  < b_10_http_right_private);
    EXPECT_TRUE(a_10_http_left_private  < b_10_https_right_private);
    EXPECT_TRUE(a_10_https_left_private < b_10_http_right_private);
    EXPECT_TRUE(a_10_https_left_private < b_10_https_right_private);

    EXPECT_TRUE(a_10_http_left_private  < b_11_http_right_private);
    EXPECT_TRUE(a_10_http_left_private  < b_11_https_right_private);
    EXPECT_TRUE(a_10_https_left_private < b_11_http_right_private);
    EXPECT_TRUE(a_10_https_left_private < b_11_https_right_private);

    EXPECT_FALSE(a_11_http_left_private  < a_10_http_right_private);
    EXPECT_FALSE(a_11_http_left_private  < a_10_https_right_private);
    EXPECT_FALSE(a_11_https_left_private < a_10_http_right_private);
    EXPECT_FALSE(a_11_https_left_private < a_10_https_right_private);

    EXPECT_FALSE(a_11_http_left_private  < b_10_http_right_private);
    EXPECT_FALSE(a_11_http_left_private  < b_10_https_right_private);
    EXPECT_FALSE(a_11_https_left_private < b_10_http_right_private);
    EXPECT_FALSE(a_11_https_left_private < b_10_https_right_private);

    EXPECT_TRUE(a_11_http_left_private  < b_11_http_right_private);
    EXPECT_TRUE(a_11_http_left_private  < b_11_https_right_private);
    EXPECT_TRUE(a_11_https_left_private < b_11_http_right_private);
    EXPECT_TRUE(a_11_https_left_private < b_11_https_right_private);

    EXPECT_FALSE(b_10_http_left_private  < a_10_http_right_private);
    EXPECT_FALSE(b_10_http_left_private  < a_10_https_right_private);
    EXPECT_FALSE(b_10_https_left_private < a_10_http_right_private);
    EXPECT_FALSE(b_10_https_left_private < a_10_https_right_private);

    EXPECT_TRUE(b_10_http_left_private  < a_11_http_right_private);
    EXPECT_TRUE(b_10_http_left_private  < a_11_https_right_private);
    EXPECT_TRUE(b_10_https_left_private < a_11_http_right_private);
    EXPECT_TRUE(b_10_https_left_private < a_11_https_right_private);

    EXPECT_TRUE(b_10_http_left_private  < b_11_http_right_private);
    EXPECT_TRUE(b_10_http_left_private  < b_11_https_right_private);
    EXPECT_TRUE(b_10_https_left_private < b_11_http_right_private);
    EXPECT_TRUE(b_10_https_left_private < b_11_https_right_private);

    EXPECT_FALSE(b_11_http_left_private  < a_10_http_right_private);
    EXPECT_FALSE(b_11_http_left_private  < a_10_https_right_private);
    EXPECT_FALSE(b_11_https_left_private < a_10_http_right_private);
    EXPECT_FALSE(b_11_https_left_private < a_10_https_right_private);

    EXPECT_FALSE(b_11_http_left_private  < a_11_http_right_private);
    EXPECT_FALSE(b_11_http_left_private  < a_11_https_right_private);
    EXPECT_FALSE(b_11_https_left_private < a_11_http_right_private);
    EXPECT_FALSE(b_11_https_left_private < a_11_https_right_private);

    EXPECT_FALSE(b_11_http_left_private  < b_10_http_right_private);
    EXPECT_FALSE(b_11_http_left_private  < b_10_https_right_private);
    EXPECT_FALSE(b_11_https_left_private < b_10_http_right_private);
    EXPECT_FALSE(b_11_https_left_private < b_10_https_right_private);
  }
}

TEST(QuicServerIdTest, Equals) {
  PrivacyMode left_privacy;
  PrivacyMode right_privacy;
  for (int i = 0; i < 2; i++) {
    switch (i) {
      case 0:
        left_privacy = PRIVACY_MODE_DISABLED;
        right_privacy = PRIVACY_MODE_DISABLED;
        break;
      case 1:
        left_privacy = PRIVACY_MODE_ENABLED;
        right_privacy = PRIVACY_MODE_ENABLED;
        break;
    }
    QuicServerId a_10_http_right_private(HostPortPair("a.com", 10), false,
                                         right_privacy);
    QuicServerId a_10_https_right_private(HostPortPair("a.com", 10), true,
                                          right_privacy);
    QuicServerId a_11_http_right_private(HostPortPair("a.com", 11), false,
                                         right_privacy);
    QuicServerId a_11_https_right_private(HostPortPair("a.com", 11), true,
                                          right_privacy);
    QuicServerId b_10_http_right_private(HostPortPair("b.com", 10), false,
                                         right_privacy);
    QuicServerId b_10_https_right_private(HostPortPair("b.com", 10), true,
                                          right_privacy);
    QuicServerId b_11_http_right_private(HostPortPair("b.com", 11), false,
                                         right_privacy);
    QuicServerId b_11_https_right_private(HostPortPair("b.com", 11), true,
                                          right_privacy);

    QuicServerId new_a_10_http_left_private(HostPortPair("a.com", 10), false,
                                            left_privacy);
    QuicServerId new_a_10_https_left_private(HostPortPair("a.com", 10), true,
                                             left_privacy);
    QuicServerId new_a_11_http_left_private(HostPortPair("a.com", 11), false,
                                            left_privacy);
    QuicServerId new_a_11_https_left_private(HostPortPair("a.com", 11), true,
                                             left_privacy);
    QuicServerId new_b_10_http_left_private(HostPortPair("b.com", 10), false,
                                            left_privacy);
    QuicServerId new_b_10_https_left_private(HostPortPair("b.com", 10), true,
                                             left_privacy);
    QuicServerId new_b_11_http_left_private(HostPortPair("b.com", 11), false,
                                            left_privacy);
    QuicServerId new_b_11_https_left_private(HostPortPair("b.com", 11), true,
                                             left_privacy);

    EXPECT_EQ(new_a_10_http_left_private,  a_10_http_right_private);
    EXPECT_EQ(new_a_10_https_left_private, a_10_https_right_private);
    EXPECT_EQ(new_a_11_http_left_private,  a_11_http_right_private);
    EXPECT_EQ(new_a_11_https_left_private, a_11_https_right_private);
    EXPECT_EQ(new_b_10_http_left_private,  b_10_http_right_private);
    EXPECT_EQ(new_b_10_https_left_private, b_10_https_right_private);
    EXPECT_EQ(new_b_11_http_left_private,  b_11_http_right_private);
    EXPECT_EQ(new_b_11_https_left_private, b_11_https_right_private);
  }

  for (int i = 0; i < 2; i++) {
    switch (i) {
      case 0:
        right_privacy = PRIVACY_MODE_DISABLED;
        break;
      case 1:
        right_privacy = PRIVACY_MODE_ENABLED;
        break;
    }
    QuicServerId a_10_http_right_private(HostPortPair("a.com", 10), false,
                                         right_privacy);
    QuicServerId a_10_https_right_private(HostPortPair("a.com", 10), true,
                                          right_privacy);
    QuicServerId a_11_http_right_private(HostPortPair("a.com", 11), false,
                                         right_privacy);
    QuicServerId a_11_https_right_private(HostPortPair("a.com", 11), true,
                                          right_privacy);
    QuicServerId b_10_http_right_private(HostPortPair("b.com", 10), false,
                                         right_privacy);
    QuicServerId b_10_https_right_private(HostPortPair("b.com", 10), true,
                                          right_privacy);
    QuicServerId b_11_http_right_private(HostPortPair("b.com", 11), false,
                                         right_privacy);
    QuicServerId b_11_https_right_private(HostPortPair("b.com", 11), true,
                                          right_privacy);

    QuicServerId new_a_10_http_left_private(HostPortPair("a.com", 10), false,
                                            PRIVACY_MODE_DISABLED);

    EXPECT_FALSE(new_a_10_http_left_private == a_10_https_right_private);
    EXPECT_FALSE(new_a_10_http_left_private == a_11_http_right_private);
    EXPECT_FALSE(new_a_10_http_left_private == b_10_http_right_private);
    EXPECT_FALSE(new_a_10_http_left_private == a_11_https_right_private);
    EXPECT_FALSE(new_a_10_http_left_private == b_10_https_right_private);
    EXPECT_FALSE(new_a_10_http_left_private == b_11_http_right_private);
    EXPECT_FALSE(new_a_10_http_left_private == b_11_https_right_private);
  }
  QuicServerId a_10_http_private(HostPortPair("a.com", 10), false,
                                 PRIVACY_MODE_ENABLED);
  QuicServerId new_a_10_http_no_private(HostPortPair("a.com", 10), false,
                                        PRIVACY_MODE_DISABLED);
  EXPECT_FALSE(new_a_10_http_no_private == a_10_http_private);
}

}  // namespace

}  // namespace net
