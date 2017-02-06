// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/copresence/copresence_state_impl.h"
#include "components/copresence/proto/data.pb.h"
#include "components/copresence/public/copresence_observer.h"
#include "components/copresence/tokens.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::ElementsAre;
using testing::Key;
using testing::SizeIs;
using testing::UnorderedElementsAre;

// TODO(ckehoe): Test start and end time tracking.

namespace google {
namespace protobuf {

bool operator==(const MessageLite& A, const MessageLite& B) {
  std::string serializedA;
  CHECK(A.SerializeToString(&serializedA));

  std::string serializedB;
  CHECK(B.SerializeToString(&serializedB));

  return serializedA == serializedB;
}

}  // namespace protobuf
}  // namespace google

namespace copresence {

namespace {

const base::Time kStartTime = base::Time::FromDoubleT(10);
const base::Time kStopTime = base::Time::FromDoubleT(20);

Directive CreateDirective(const std::string& token, bool transmit) {
  Directive directive;
  TokenInstruction* instruction = directive.mutable_token_instruction();
  instruction->set_token_id(token);
  instruction->set_medium(AUDIO_ULTRASOUND_PASSBAND);
  if (transmit)
    instruction->set_token_instruction_type(TRANSMIT);
  return directive;
}

template<typename TokenType>
TokenType CreateToken(const std::string& id) {
  TokenType token;
  token.id = id;
  token.medium = AUDIO_ULTRASOUND_PASSBAND;
  token.start_time = kStartTime;
  return token;
}

}  // namespace

class CopresenceStateTest : public CopresenceObserver,
                                  public testing::Test {
 public:
  CopresenceStateTest() : directive_notifications_(0) {
    state_.AddObserver(this);
  }

 protected:
  CopresenceStateImpl state_;

  int directive_notifications_;
  std::vector<std::string> transmitted_updates_;
  std::vector<std::string> received_updates_;

 private:
  // CopresenceObserver implementation.
  void DirectivesUpdated() override {
    directive_notifications_++;
  }
  void TokenTransmitted(const TransmittedToken& token) override {
    transmitted_updates_.push_back(token.id);
  }
  void TokenReceived(const ReceivedToken& token) override {
    received_updates_.push_back(token.id);
  }
};

TEST_F(CopresenceStateTest, Directives) {
  std::vector<Directive> directives;
  directives.push_back(CreateDirective("transmit 1", true));
  directives.push_back(CreateDirective("transmit 2", true));
  directives.push_back(CreateDirective("receive", false));
  state_.UpdateDirectives(directives);

  EXPECT_EQ(1, directive_notifications_);
  EXPECT_EQ(directives, state_.active_directives());
  EXPECT_THAT(transmitted_updates_, ElementsAre("transmit 1", "transmit 2"));
  EXPECT_THAT(state_.transmitted_tokens(),
              UnorderedElementsAre(Key("transmit 1"), Key("transmit 2")));

  directives.clear();
  directives.push_back(CreateDirective("transmit 1", true));
  state_.UpdateDirectives(directives);
  EXPECT_EQ(2, directive_notifications_);
  EXPECT_EQ(directives, state_.active_directives());
  EXPECT_THAT(state_.transmitted_tokens(), SizeIs(2));
}

TEST_F(CopresenceStateTest, TransmittedTokens) {
  state_.UpdateTransmittedToken(CreateToken<TransmittedToken>("A"));
  state_.UpdateTransmittedToken(CreateToken<TransmittedToken>("B"));

  EXPECT_THAT(transmitted_updates_, ElementsAre("A", "B"));
  EXPECT_THAT(state_.transmitted_tokens(),
              UnorderedElementsAre(Key("A"), Key("B")));

  TransmittedToken tokenA = CreateToken<TransmittedToken>("A");
  tokenA.stop_time = kStopTime;
  state_.UpdateTransmittedToken(tokenA);

  EXPECT_THAT(transmitted_updates_, ElementsAre("A", "B", "A"));
  EXPECT_EQ(kStopTime, state_.transmitted_tokens().find("A")->second.stop_time);

  state_.UpdateReceivedToken(CreateToken<ReceivedToken>("B"));
  EXPECT_THAT(transmitted_updates_, ElementsAre("A", "B", "A", "B"));
  EXPECT_TRUE(state_.transmitted_tokens().find("B")
                  ->second.broadcast_confirmed);
}

TEST_F(CopresenceStateTest, ReceivedTokens) {
  state_.UpdateReceivedToken(CreateToken<ReceivedToken>("A"));
  state_.UpdateReceivedToken(CreateToken<ReceivedToken>("B"));

  EXPECT_THAT(received_updates_, ElementsAre("A", "B"));
  EXPECT_THAT(state_.received_tokens(),
              UnorderedElementsAre(Key("A"), Key("B")));

  state_.UpdateTokenStatus("A", copresence::VALID);
  EXPECT_THAT(received_updates_, ElementsAre("A", "B", "A"));
  EXPECT_EQ(ReceivedToken::VALID,
            state_.received_tokens().find("A")->second.valid);
}

}  // namespace copresence
