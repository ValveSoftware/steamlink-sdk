// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/send_ping_task.h"

#include "base/base64.h"
#include "base/memory/scoped_ptr.h"
#include "jingle/notifier/listener/xml_element_util.h"
#include "talk/xmpp/jid.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace buzz {
class XmlElement;
}

namespace notifier {

class SendPingTaskTest : public testing::Test {
 public:
  SendPingTaskTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SendPingTaskTest);
};

TEST_F(SendPingTaskTest, MakePingStanza) {
  std::string task_id = "42";

  scoped_ptr<buzz::XmlElement> message(SendPingTask::MakePingStanza(task_id));

  std::string expected_xml_string("<cli:iq type=\"get\" id=\"");
  expected_xml_string += task_id;
  expected_xml_string +=
      "\" xmlns:cli=\"jabber:client\">"
      "<ping:ping xmlns:ping=\"urn:xmpp:ping\"/></cli:iq>";

  EXPECT_EQ(expected_xml_string, XmlElementToString(*message));
}

}  // namespace notifier
