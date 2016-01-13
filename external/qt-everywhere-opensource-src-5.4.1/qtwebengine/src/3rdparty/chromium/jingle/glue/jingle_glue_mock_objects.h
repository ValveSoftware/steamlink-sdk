// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_GLUE_JINGLE_GLUE_MOCK_OBJECTS_H_
#define JINGLE_GLUE_JINGLE_GLUE_MOCK_OBJECTS_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/libjingle/source/talk/base/stream.h"

namespace jingle_glue {

class MockStream : public talk_base::StreamInterface {
 public:
  MockStream();
  virtual ~MockStream();

  MOCK_CONST_METHOD0(GetState, talk_base::StreamState());

  MOCK_METHOD4(Read, talk_base::StreamResult(void*, size_t, size_t*, int*));
  MOCK_METHOD4(Write, talk_base::StreamResult(const void*, size_t,
                                              size_t*, int*));
  MOCK_CONST_METHOD1(GetAvailable, bool(size_t*));
  MOCK_METHOD0(Close, void());

  MOCK_METHOD3(PostEvent, void(talk_base::Thread*, int, int));
  MOCK_METHOD2(PostEvent, void(int, int));
};

}  // namespace jingle_glue

#endif  // JINGLE_GLUE_JINGLE_GLUE_MOCK_OBJECTS_H_
