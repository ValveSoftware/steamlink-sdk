// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/handlers/audio/audio_directive_list.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop/message_loop.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::IsNull;

namespace copresence {

static const int64_t kTtl = 10;

const Directive CreateDirective(int64_t ttl) {
  Directive directive;
  directive.set_ttl_millis(ttl);
  return directive;
}

class AudioDirectiveListTest : public testing::Test {
 public:
  AudioDirectiveListTest() : directive_list_(new AudioDirectiveList) {}

 protected:
  base::MessageLoop message_loop_;
  std::unique_ptr<AudioDirectiveList> directive_list_;
};

TEST_F(AudioDirectiveListTest, Basic) {
  EXPECT_THAT(directive_list_->GetActiveDirective(), IsNull());

  directive_list_->AddDirective("op_id1", CreateDirective(kTtl));
  directive_list_->AddDirective("op_id2", CreateDirective(kTtl * 3));
  directive_list_->AddDirective("op_id3", CreateDirective(kTtl * 2));
  EXPECT_EQ("op_id2", directive_list_->GetActiveDirective()->op_id);

  directive_list_->RemoveDirective("op_id2");
  EXPECT_EQ("op_id3", directive_list_->GetActiveDirective()->op_id);
}

TEST_F(AudioDirectiveListTest, AddDirectiveMultiple) {
  directive_list_->AddDirective("op_id1", CreateDirective(kTtl));
  directive_list_->AddDirective("op_id2", CreateDirective(kTtl * 2));
  directive_list_->AddDirective("op_id3", CreateDirective(kTtl * 3 * 2));
  directive_list_->AddDirective("op_id3", CreateDirective(kTtl * 3 * 3));
  directive_list_->AddDirective("op_id4", CreateDirective(kTtl * 4));

  EXPECT_EQ("op_id3", directive_list_->GetActiveDirective()->op_id);
  directive_list_->RemoveDirective("op_id3");
  EXPECT_EQ("op_id4", directive_list_->GetActiveDirective()->op_id);
  directive_list_->RemoveDirective("op_id4");
  EXPECT_EQ("op_id2", directive_list_->GetActiveDirective()->op_id);
  directive_list_->RemoveDirective("op_id2");
  EXPECT_EQ("op_id1", directive_list_->GetActiveDirective()->op_id);
  directive_list_->RemoveDirective("op_id1");
  EXPECT_THAT(directive_list_->GetActiveDirective(), IsNull());
}

TEST_F(AudioDirectiveListTest, RemoveDirectiveMultiple) {
  directive_list_->AddDirective("op_id1", CreateDirective(kTtl));
  directive_list_->AddDirective("op_id2", CreateDirective(kTtl * 2));
  directive_list_->AddDirective("op_id3", CreateDirective(kTtl * 3));
  directive_list_->AddDirective("op_id4", CreateDirective(kTtl * 4));

  EXPECT_EQ("op_id4", directive_list_->GetActiveDirective()->op_id);
  directive_list_->RemoveDirective("op_id4");
  EXPECT_EQ("op_id3", directive_list_->GetActiveDirective()->op_id);
  directive_list_->RemoveDirective("op_id3");
  directive_list_->RemoveDirective("op_id3");
  directive_list_->RemoveDirective("op_id3");
  EXPECT_EQ("op_id2", directive_list_->GetActiveDirective()->op_id);
  directive_list_->RemoveDirective("op_id2");
  EXPECT_EQ("op_id1", directive_list_->GetActiveDirective()->op_id);
  directive_list_->RemoveDirective("op_id1");
  EXPECT_THAT(directive_list_->GetActiveDirective(), IsNull());
}

}  // namespace copresence
