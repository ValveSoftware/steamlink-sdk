// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "gpu/ipc/common/traits_test_service.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

namespace {

class StructTraitsTest : public testing::Test, public mojom::TraitsTestService {
 public:
  StructTraitsTest() {}

 protected:
  mojom::TraitsTestServicePtr GetTraitsTestProxy() {
    return traits_test_bindings_.CreateInterfacePtrAndBind(this);
  }

 private:
  // TraitsTestService:
  void EchoMailbox(const Mailbox& m,
                   const EchoMailboxCallback& callback) override {
    callback.Run(m);
  }

  void EchoMailboxHolder(const MailboxHolder& r,
                         const EchoMailboxHolderCallback& callback) override {
    callback.Run(r);
  }

  void EchoSyncToken(const SyncToken& s,
                     const EchoSyncTokenCallback& callback) override {
    callback.Run(s);
  }

  base::MessageLoop loop_;
  mojo::BindingSet<TraitsTestService> traits_test_bindings_;

  DISALLOW_COPY_AND_ASSIGN(StructTraitsTest);
};

}  // namespace

TEST_F(StructTraitsTest, Mailbox) {
  const int8_t mailbox_name[GL_MAILBOX_SIZE_CHROMIUM] = {
      0, 9, 8, 7, 6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2, 4, 6, 8, 0, 0, 9,
      8, 7, 6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2, 4, 6, 8, 0, 0, 9, 8, 7,
      6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2, 4, 6, 8, 0, 0, 9, 8, 7};
  gpu::Mailbox input;
  input.SetName(mailbox_name);
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  gpu::Mailbox output;
  proxy->EchoMailbox(input, &output);
  gpu::Mailbox test_mailbox;
  test_mailbox.SetName(mailbox_name);
  EXPECT_EQ(test_mailbox, output);
}

TEST_F(StructTraitsTest, MailboxHolder) {
  gpu::MailboxHolder input;
  const int8_t mailbox_name[GL_MAILBOX_SIZE_CHROMIUM] = {
      0, 9, 8, 7, 6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2, 4, 6, 8, 0, 0, 9,
      8, 7, 6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2, 4, 6, 8, 0, 0, 9, 8, 7,
      6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2, 4, 6, 8, 0, 0, 9, 8, 7};
  gpu::Mailbox mailbox;
  mailbox.SetName(mailbox_name);
  const gpu::CommandBufferNamespace namespace_id = gpu::IN_PROCESS;
  const int32_t extra_data_field = 0xbeefbeef;
  const gpu::CommandBufferId command_buffer_id(
      gpu::CommandBufferId::FromUnsafeValue(0xdeadbeef));
  const uint64_t release_count = 0xdeadbeefdeadL;
  const gpu::SyncToken sync_token(namespace_id, extra_data_field,
                                  command_buffer_id, release_count);
  const uint32_t texture_target = 1337;
  input.mailbox = mailbox;
  input.sync_token = sync_token;
  input.texture_target = texture_target;

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  gpu::MailboxHolder output;
  proxy->EchoMailboxHolder(input, &output);
  EXPECT_EQ(mailbox, output.mailbox);
  EXPECT_EQ(sync_token, output.sync_token);
  EXPECT_EQ(texture_target, output.texture_target);
}

TEST_F(StructTraitsTest, SyncToken) {
  const gpu::CommandBufferNamespace namespace_id = gpu::IN_PROCESS;
  const int32_t extra_data_field = 0xbeefbeef;
  const gpu::CommandBufferId command_buffer_id(
      gpu::CommandBufferId::FromUnsafeValue(0xdeadbeef));
  const uint64_t release_count = 0xdeadbeefdead;
  const bool verified_flush = false;
  gpu::SyncToken input(namespace_id, extra_data_field, command_buffer_id,
                       release_count);
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  gpu::SyncToken output;
  proxy->EchoSyncToken(input, &output);
  EXPECT_EQ(namespace_id, output.namespace_id());
  EXPECT_EQ(extra_data_field, output.extra_data_field());
  EXPECT_EQ(command_buffer_id, output.command_buffer_id());
  EXPECT_EQ(release_count, output.release_count());
  EXPECT_EQ(verified_flush, output.verified_flush());
}

}  // namespace gpu
