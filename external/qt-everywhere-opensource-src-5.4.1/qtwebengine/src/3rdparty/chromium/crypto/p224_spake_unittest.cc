// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <crypto/p224_spake.h>

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crypto {

namespace {

bool RunExchange(P224EncryptedKeyExchange* client,
                 P224EncryptedKeyExchange* server) {

  for (;;) {
    std::string client_message, server_message;
    client_message = client->GetMessage();
    server_message = server->GetMessage();

    P224EncryptedKeyExchange::Result client_result, server_result;
    client_result = client->ProcessMessage(server_message);
    server_result = server->ProcessMessage(client_message);

    // Check that we never hit the case where only one succeeds.
    if ((client_result == P224EncryptedKeyExchange::kResultSuccess) ^
        (server_result == P224EncryptedKeyExchange::kResultSuccess)) {
      CHECK(false) << "Parties differ on whether authentication was successful";
    }

    if (client_result == P224EncryptedKeyExchange::kResultFailed ||
        server_result == P224EncryptedKeyExchange::kResultFailed) {
      return false;
    }

    if (client_result == P224EncryptedKeyExchange::kResultSuccess &&
        server_result == P224EncryptedKeyExchange::kResultSuccess) {
      return true;
    }

    CHECK_EQ(P224EncryptedKeyExchange::kResultPending, client_result);
    CHECK_EQ(P224EncryptedKeyExchange::kResultPending, server_result);
  }
}

const char kPassword[] = "foo";

}  // namespace

TEST(MutualAuth, CorrectAuth) {
  P224EncryptedKeyExchange client(
      P224EncryptedKeyExchange::kPeerTypeClient, kPassword);
  P224EncryptedKeyExchange server(
      P224EncryptedKeyExchange::kPeerTypeServer, kPassword);

  EXPECT_TRUE(RunExchange(&client, &server));
  EXPECT_EQ(client.GetKey(), server.GetKey());
}

TEST(MutualAuth, IncorrectPassword) {
  P224EncryptedKeyExchange client(
      P224EncryptedKeyExchange::kPeerTypeClient,
      kPassword);
  P224EncryptedKeyExchange server(
      P224EncryptedKeyExchange::kPeerTypeServer,
      "wrongpassword");

  EXPECT_FALSE(RunExchange(&client, &server));
}

TEST(MutualAuth, Fuzz) {
  static const unsigned kIterations = 40;

  for (unsigned i = 0; i < kIterations; i++) {
    P224EncryptedKeyExchange client(
        P224EncryptedKeyExchange::kPeerTypeClient, kPassword);
    P224EncryptedKeyExchange server(
        P224EncryptedKeyExchange::kPeerTypeServer, kPassword);

    // We'll only be testing small values of i, but we don't want that to bias
    // the test coverage. So we disperse the value of i by multiplying by the
    // FNV, 32-bit prime, producing a poor-man's PRNG.
    const uint32 rand = i * 16777619;

    for (unsigned round = 0;; round++) {
      std::string client_message, server_message;
      client_message = client.GetMessage();
      server_message = server.GetMessage();

      if ((rand & 1) == round) {
        const bool server_or_client = rand & 2;
        std::string* m = server_or_client ? &server_message : &client_message;
        if (rand & 4) {
          // Truncate
          *m = m->substr(0, (i >> 3) % m->size());
        } else {
          // Corrupt
          const size_t bits = m->size() * 8;
          const size_t bit_to_corrupt = (rand >> 3) % bits;
          const_cast<char*>(m->data())[bit_to_corrupt / 8] ^=
              1 << (bit_to_corrupt % 8);
        }
      }

      P224EncryptedKeyExchange::Result client_result, server_result;
      client_result = client.ProcessMessage(server_message);
      server_result = server.ProcessMessage(client_message);

      // If we have corrupted anything, we expect the authentication to fail,
      // although one side can succeed if we happen to corrupt the second round
      // message to the other.
      ASSERT_FALSE(
          client_result == P224EncryptedKeyExchange::kResultSuccess &&
          server_result == P224EncryptedKeyExchange::kResultSuccess);

      if (client_result == P224EncryptedKeyExchange::kResultFailed ||
          server_result == P224EncryptedKeyExchange::kResultFailed) {
        break;
      }

      ASSERT_EQ(P224EncryptedKeyExchange::kResultPending,
                client_result);
      ASSERT_EQ(P224EncryptedKeyExchange::kResultPending,
                server_result);
    }
  }
}

}  // namespace crypto
