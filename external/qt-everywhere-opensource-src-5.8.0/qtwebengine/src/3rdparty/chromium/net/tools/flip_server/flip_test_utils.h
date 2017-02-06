// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_FLIP_SERVER_FLIP_TEST_UTILS_H_
#define NET_TOOLS_FLIP_SERVER_FLIP_TEST_UTILS_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "net/tools/flip_server/sm_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class MockSMInterface : public SMInterface {
 public:
  MockSMInterface();
  virtual ~MockSMInterface();

  MOCK_METHOD2(InitSMInterface, void(SMInterface*, int32_t));
  MOCK_METHOD8(InitSMConnection,
               void(SMConnectionPoolInterface*,
                    SMInterface*,
                    EpollServer*,
                    int,
                    std::string,
                    std::string,
                    std::string,
                    bool));
  MOCK_METHOD2(ProcessReadInput, size_t(const char*, size_t));
  MOCK_METHOD2(ProcessWriteInput, size_t(const char*, size_t));
  MOCK_METHOD1(SetStreamID, void(uint32_t stream_id));
  MOCK_CONST_METHOD0(MessageFullyRead, bool());
  MOCK_CONST_METHOD0(Error, bool());
  MOCK_CONST_METHOD0(ErrorAsString, const char*());
  MOCK_METHOD0(Reset, void());
  MOCK_METHOD1(ResetForNewInterface, void(int32_t server_idx));
  MOCK_METHOD0(ResetForNewConnection, void());
  MOCK_METHOD0(Cleanup, void());
  MOCK_METHOD0(PostAcceptHook, int());
  MOCK_METHOD3(NewStream, void(uint32_t, uint32_t, const std::string&));
  MOCK_METHOD1(SendEOF, void(uint32_t stream_id));
  MOCK_METHOD1(SendErrorNotFound, void(uint32_t stream_id));
  MOCK_METHOD2(SendSynStream, size_t(uint32_t, const BalsaHeaders&));
  MOCK_METHOD2(SendSynReply, size_t(uint32_t, const BalsaHeaders&));
  MOCK_METHOD5(SendDataFrame,
               void(uint32_t, const char*, int64_t, uint32_t, bool));
  MOCK_METHOD0(GetOutput, void());
  MOCK_METHOD0(set_is_request, void());
};

}  // namespace net

#endif  // NET_TOOLS_FLIP_SERVER_FLIP_TEST_UTILS_H_
