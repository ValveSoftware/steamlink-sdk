// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/bootstrap_sandbox.h"

#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/mach_logging.h"
#include "base/mac/scoped_mach_port.h"
#include "base/mac/scoped_nsobject.h"
#include "base/process/kill.h"
#include "base/strings/stringprintf.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "sandbox/mac/pre_exec_delegate.h"
#include "sandbox/mac/xpc.h"
#import "testing/gtest_mac.h"
#include "testing/multiprocess_func_list.h"

NSString* const kTestNotification = @"org.chromium.bootstrap_sandbox_test";

@interface DistributedNotificationObserver : NSObject {
 @private
  int receivedCount_;
  base::scoped_nsobject<NSString> object_;
}
- (int)receivedCount;
- (NSString*)object;
- (void)waitForNotification;
@end

@implementation DistributedNotificationObserver
- (id)init {
  if ((self = [super init])) {
    [[NSDistributedNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(observeNotification:)
               name:kTestNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  [[NSDistributedNotificationCenter defaultCenter]
      removeObserver:self
                name:kTestNotification
              object:nil];
  [super dealloc];
}

- (int)receivedCount {
  return receivedCount_;
}

- (NSString*)object {
  return object_.get();
}

- (void)waitForNotification {
  object_.reset();
  CFRunLoopRunInMode(kCFRunLoopDefaultMode,
      TestTimeouts::action_timeout().InSeconds(), false);
}

- (void)observeNotification:(NSNotification*)notification {
  ++receivedCount_;
  object_.reset([[notification object] copy]);
  CFRunLoopStop(CFRunLoopGetCurrent());
}
@end

////////////////////////////////////////////////////////////////////////////////

namespace sandbox {

class BootstrapSandboxTest : public base::MultiProcessTest {
 public:
  void SetUp() override {
    base::MultiProcessTest::SetUp();

    sandbox_ = BootstrapSandbox::Create();
    ASSERT_TRUE(sandbox_.get());
  }

  BootstrapSandboxPolicy BaselinePolicy() {
    BootstrapSandboxPolicy policy;
    policy.rules["com.apple.cfprefsd.daemon"] = Rule(POLICY_ALLOW);
    return policy;
  }

  void RunChildWithPolicy(int policy_id,
                          const char* child_name,
                          base::ProcessHandle* out_pid) {
    std::unique_ptr<PreExecDelegate> pre_exec_delegate(
        sandbox_->NewClient(policy_id));

    base::LaunchOptions options;
    options.pre_exec_delegate = pre_exec_delegate.get();

    base::Process process = SpawnChildWithOptions(child_name, options);
    ASSERT_TRUE(process.IsValid());
    int code = 0;
    EXPECT_TRUE(process.WaitForExit(&code));
    EXPECT_EQ(0, code);
    if (out_pid)
      *out_pid = process.Pid();
  }

 protected:
  std::unique_ptr<BootstrapSandbox> sandbox_;
};

const char kNotificationTestMain[] = "PostNotification";

// Run the test without the sandbox.
TEST_F(BootstrapSandboxTest, DistributedNotifications_Unsandboxed) {
  base::scoped_nsobject<DistributedNotificationObserver> observer(
      [[DistributedNotificationObserver alloc] init]);

  base::Process process = SpawnChild(kNotificationTestMain);
  ASSERT_TRUE(process.IsValid());
  int code = 0;
  EXPECT_TRUE(process.WaitForExit(&code));
  EXPECT_EQ(0, code);

  [observer waitForNotification];
  EXPECT_EQ(1, [observer receivedCount]);
  EXPECT_EQ(process.Pid(), [[observer object] intValue]);
}

// Run the test with the sandbox enabled without notifications on the policy
// whitelist.
TEST_F(BootstrapSandboxTest, DistributedNotifications_SandboxDeny) {
  base::scoped_nsobject<DistributedNotificationObserver> observer(
      [[DistributedNotificationObserver alloc] init]);

  sandbox_->RegisterSandboxPolicy(1, BaselinePolicy());
  RunChildWithPolicy(1, kNotificationTestMain, NULL);

  [observer waitForNotification];
  EXPECT_EQ(0, [observer receivedCount]);
  EXPECT_EQ(nil, [observer object]);
}

// Run the test with notifications permitted.
TEST_F(BootstrapSandboxTest, DistributedNotifications_SandboxAllow) {
  base::scoped_nsobject<DistributedNotificationObserver> observer(
      [[DistributedNotificationObserver alloc] init]);

  BootstrapSandboxPolicy policy(BaselinePolicy());
  // 10.9:
  policy.rules["com.apple.distributed_notifications@Uv3"] = Rule(POLICY_ALLOW);
  policy.rules["com.apple.distributed_notifications@1v3"] = Rule(POLICY_ALLOW);
  // 10.6:
  policy.rules["com.apple.system.notification_center"] = Rule(POLICY_ALLOW);
  policy.rules["com.apple.distributed_notifications.2"] = Rule(POLICY_ALLOW);
  sandbox_->RegisterSandboxPolicy(2, policy);

  base::ProcessHandle pid;
  RunChildWithPolicy(2, kNotificationTestMain, &pid);

  [observer waitForNotification];
  EXPECT_EQ(1, [observer receivedCount]);
  EXPECT_EQ(pid, [[observer object] intValue]);
}

MULTIPROCESS_TEST_MAIN(PostNotification) {
  [[NSDistributedNotificationCenter defaultCenter]
      postNotificationName:kTestNotification
                    object:[NSString stringWithFormat:@"%d", getpid()]];
  return 0;
}

const char kTestServer[] = "org.chromium.test_bootstrap_server";

TEST_F(BootstrapSandboxTest, PolicyDenyError) {
  BootstrapSandboxPolicy policy(BaselinePolicy());
  policy.rules[kTestServer] = Rule(POLICY_DENY_ERROR);
  sandbox_->RegisterSandboxPolicy(1, policy);

  RunChildWithPolicy(1, "PolicyDenyError", NULL);
}

MULTIPROCESS_TEST_MAIN(PolicyDenyError) {
  mach_port_t port = MACH_PORT_NULL;
  kern_return_t kr = bootstrap_look_up(bootstrap_port, kTestServer,
      &port);
  CHECK_EQ(BOOTSTRAP_UNKNOWN_SERVICE, kr);
  CHECK(port == MACH_PORT_NULL);

  kr = bootstrap_look_up(bootstrap_port, "org.chromium.some_other_server",
      &port);
  CHECK_EQ(BOOTSTRAP_UNKNOWN_SERVICE, kr);
  CHECK(port == MACH_PORT_NULL);

  return 0;
}

TEST_F(BootstrapSandboxTest, PolicyDenyDummyPort) {
  BootstrapSandboxPolicy policy(BaselinePolicy());
  policy.rules[kTestServer] = Rule(POLICY_DENY_DUMMY_PORT);
  sandbox_->RegisterSandboxPolicy(1, policy);

  RunChildWithPolicy(1, "PolicyDenyDummyPort", NULL);
}

MULTIPROCESS_TEST_MAIN(PolicyDenyDummyPort) {
  mach_port_t port = MACH_PORT_NULL;
  kern_return_t kr = bootstrap_look_up(bootstrap_port, kTestServer,
      &port);
  CHECK_EQ(KERN_SUCCESS, kr);
  CHECK(port != MACH_PORT_NULL);
  return 0;
}

struct SubstitutePortAckSend {
  mach_msg_header_t header;
  char buf[32];
};

struct SubstitutePortAckRecv : public SubstitutePortAckSend {
  mach_msg_trailer_t trailer;
};

const char kSubstituteAck[] = "Hello, this is doge!";

TEST_F(BootstrapSandboxTest, PolicySubstitutePort) {
  mach_port_t task = mach_task_self();

  mach_port_t port;
  ASSERT_EQ(KERN_SUCCESS, mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE,
      &port));
  base::mac::ScopedMachReceiveRight scoped_port(port);

  mach_port_urefs_t send_rights = 0;
  ASSERT_EQ(KERN_SUCCESS, mach_port_get_refs(task, port, MACH_PORT_RIGHT_SEND,
      &send_rights));
  EXPECT_EQ(0u, send_rights);

  ASSERT_EQ(KERN_SUCCESS, mach_port_insert_right(task, port, port,
      MACH_MSG_TYPE_MAKE_SEND));
  base::mac::ScopedMachSendRight scoped_port_send(port);

  send_rights = 0;
  ASSERT_EQ(KERN_SUCCESS, mach_port_get_refs(task, port, MACH_PORT_RIGHT_SEND,
      &send_rights));
  EXPECT_EQ(1u, send_rights);

  BootstrapSandboxPolicy policy(BaselinePolicy());
  policy.rules[kTestServer] = Rule(port);
  sandbox_->RegisterSandboxPolicy(1, policy);

  RunChildWithPolicy(1, "PolicySubstitutePort", NULL);

  struct SubstitutePortAckRecv msg;
  bzero(&msg, sizeof(msg));
  msg.header.msgh_size = sizeof(msg);
  msg.header.msgh_local_port = port;
  kern_return_t kr = mach_msg(&msg.header, MACH_RCV_MSG, 0,
      msg.header.msgh_size, port,
      TestTimeouts::tiny_timeout().InMilliseconds(), MACH_PORT_NULL);
  EXPECT_EQ(KERN_SUCCESS, kr);

  send_rights = 0;
  ASSERT_EQ(KERN_SUCCESS, mach_port_get_refs(task, port, MACH_PORT_RIGHT_SEND,
      &send_rights));
  EXPECT_EQ(1u, send_rights);

  EXPECT_EQ(0, strncmp(kSubstituteAck, msg.buf, sizeof(msg.buf)));
}

MULTIPROCESS_TEST_MAIN(PolicySubstitutePort) {
  mach_port_t port = MACH_PORT_NULL;
  kern_return_t kr = bootstrap_look_up(bootstrap_port, kTestServer, &port);
  CHECK_EQ(KERN_SUCCESS, kr);
  CHECK(port != MACH_PORT_NULL);

  struct SubstitutePortAckSend msg;
  bzero(&msg, sizeof(msg));
  msg.header.msgh_size = sizeof(msg);
  msg.header.msgh_remote_port = port;
  msg.header.msgh_bits = MACH_MSGH_BITS_REMOTE(MACH_MSG_TYPE_MOVE_SEND);
  strncpy(msg.buf, kSubstituteAck, sizeof(msg.buf));

  CHECK_EQ(KERN_SUCCESS, mach_msg_send(&msg.header));

  return 0;
}

TEST_F(BootstrapSandboxTest, ForwardMessageInProcess) {
  mach_port_t task = mach_task_self();

  mach_port_t port;
  ASSERT_EQ(KERN_SUCCESS, mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE,
      &port));
  base::mac::ScopedMachReceiveRight scoped_port_recv(port);

  mach_port_urefs_t send_rights = 0;
  ASSERT_EQ(KERN_SUCCESS, mach_port_get_refs(task, port, MACH_PORT_RIGHT_SEND,
      &send_rights));
  EXPECT_EQ(0u, send_rights);

  ASSERT_EQ(KERN_SUCCESS, mach_port_insert_right(task, port, port,
      MACH_MSG_TYPE_MAKE_SEND));
  base::mac::ScopedMachSendRight scoped_port_send(port);

  send_rights = 0;
  ASSERT_EQ(KERN_SUCCESS, mach_port_get_refs(task, port, MACH_PORT_RIGHT_SEND,
      &send_rights));
  EXPECT_EQ(1u, send_rights);

  mach_port_t bp;
  ASSERT_EQ(KERN_SUCCESS, task_get_bootstrap_port(task, &bp));
  base::mac::ScopedMachSendRight scoped_bp(bp);

  char service_name[] = "org.chromium.sandbox.test.ForwardMessageInProcess";
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  kern_return_t kr = bootstrap_register(bp, service_name, port);
#pragma GCC diagnostic pop
  EXPECT_EQ(KERN_SUCCESS, kr);

  send_rights = 0;
  ASSERT_EQ(KERN_SUCCESS, mach_port_get_refs(task, port, MACH_PORT_RIGHT_SEND,
      &send_rights));
  EXPECT_EQ(1u, send_rights);

  mach_port_t service_port;
  EXPECT_EQ(KERN_SUCCESS, bootstrap_look_up(bp, service_name, &service_port));
  base::mac::ScopedMachSendRight scoped_service_port(service_port);

  send_rights = 0;
  ASSERT_EQ(KERN_SUCCESS, mach_port_get_refs(task, port, MACH_PORT_RIGHT_SEND,
      &send_rights));
  EXPECT_EQ(2u, send_rights);
}

const char kDefaultRuleTestAllow[] =
    "org.chromium.sandbox.test.DefaultRuleAllow";
const char kDefaultRuleTestDeny[] =
    "org.chromium.sandbox.test.DefaultRuleAllow.Deny";

TEST_F(BootstrapSandboxTest, DefaultRuleAllow) {
  mach_port_t task = mach_task_self();

  mach_port_t port;
  ASSERT_EQ(KERN_SUCCESS, mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE,
      &port));
  base::mac::ScopedMachReceiveRight scoped_port_recv(port);

  ASSERT_EQ(KERN_SUCCESS, mach_port_insert_right(task, port, port,
      MACH_MSG_TYPE_MAKE_SEND));
  base::mac::ScopedMachSendRight scoped_port_send(port);

  BootstrapSandboxPolicy policy;
  policy.default_rule = Rule(POLICY_ALLOW);
  policy.rules[kDefaultRuleTestAllow] = Rule(port);
  policy.rules[kDefaultRuleTestDeny] = Rule(POLICY_DENY_ERROR);
  sandbox_->RegisterSandboxPolicy(3, policy);

  base::scoped_nsobject<DistributedNotificationObserver> observer(
      [[DistributedNotificationObserver alloc] init]);

  int pid = 0;
  RunChildWithPolicy(3, "DefaultRuleAllow", &pid);
  EXPECT_GT(pid, 0);

  [observer waitForNotification];
  EXPECT_EQ(1, [observer receivedCount]);
  EXPECT_EQ(pid, [[observer object] intValue]);

  struct SubstitutePortAckRecv msg;
  bzero(&msg, sizeof(msg));
  msg.header.msgh_size = sizeof(msg);
  msg.header.msgh_local_port = port;
  kern_return_t kr = mach_msg(&msg.header, MACH_RCV_MSG, 0,
      msg.header.msgh_size, port,
      TestTimeouts::tiny_timeout().InMilliseconds(), MACH_PORT_NULL);
  EXPECT_EQ(KERN_SUCCESS, kr);

  EXPECT_EQ(0, strncmp(kSubstituteAck, msg.buf, sizeof(msg.buf)));
}

MULTIPROCESS_TEST_MAIN(DefaultRuleAllow) {
  [[NSDistributedNotificationCenter defaultCenter]
      postNotificationName:kTestNotification
                    object:[NSString stringWithFormat:@"%d", getpid()]];

  mach_port_t port = MACH_PORT_NULL;
  CHECK_EQ(BOOTSTRAP_UNKNOWN_SERVICE, bootstrap_look_up(bootstrap_port,
      const_cast<char*>(kDefaultRuleTestDeny), &port));
  CHECK(port == MACH_PORT_NULL);

  CHECK_EQ(KERN_SUCCESS, bootstrap_look_up(bootstrap_port,
      const_cast<char*>(kDefaultRuleTestAllow), &port));
  CHECK(port != MACH_PORT_NULL);

  struct SubstitutePortAckSend msg;
  bzero(&msg, sizeof(msg));
  msg.header.msgh_size = sizeof(msg);
  msg.header.msgh_remote_port = port;
  msg.header.msgh_bits = MACH_MSGH_BITS_REMOTE(MACH_MSG_TYPE_MOVE_SEND);
  strncpy(msg.buf, kSubstituteAck, sizeof(msg.buf));

  CHECK_EQ(KERN_SUCCESS, mach_msg_send(&msg.header));

  return 0;
}

TEST_F(BootstrapSandboxTest, ChildOutliveSandbox) {
  const int kTestPolicyId = 1;
  mach_port_t task = mach_task_self();

  // Create a server port.
  mach_port_t port;
  ASSERT_EQ(KERN_SUCCESS, mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE,
      &port));
  base::mac::ScopedMachReceiveRight scoped_port_recv(port);

  ASSERT_EQ(KERN_SUCCESS, mach_port_insert_right(task, port, port,
      MACH_MSG_TYPE_MAKE_SEND));
  base::mac::ScopedMachSendRight scoped_port_send(port);

  // Set up the policy and register the port.
  BootstrapSandboxPolicy policy(BaselinePolicy());
  policy.rules["sync"] = Rule(port);
  sandbox_->RegisterSandboxPolicy(kTestPolicyId, policy);

  // Launch the child.
  std::unique_ptr<PreExecDelegate> pre_exec_delegate(
      sandbox_->NewClient(kTestPolicyId));
  base::LaunchOptions options;
  options.pre_exec_delegate = pre_exec_delegate.get();
  base::Process process = SpawnChildWithOptions("ChildOutliveSandbox", options);
  ASSERT_TRUE(process.IsValid());

  // Synchronize with the child.
  mach_msg_empty_rcv_t rcv_msg;
  bzero(&rcv_msg, sizeof(rcv_msg));
  kern_return_t kr = mach_msg(&rcv_msg.header, MACH_RCV_MSG, 0,
      sizeof(rcv_msg), port,
      TestTimeouts::tiny_timeout().InMilliseconds(), MACH_PORT_NULL);
  ASSERT_EQ(KERN_SUCCESS, kr) << mach_error_string(kr);

  // Destroy the sandbox.
  sandbox_.reset();

  // Synchronize again with the child.
  mach_msg_empty_send_t send_msg;
  bzero(&send_msg, sizeof(send_msg));
  send_msg.header.msgh_size = sizeof(send_msg);
  send_msg.header.msgh_remote_port = rcv_msg.header.msgh_remote_port;
  send_msg.header.msgh_bits =
      MACH_MSGH_BITS_REMOTE(MACH_MSG_TYPE_MOVE_SEND_ONCE);
  kr = mach_msg(&send_msg.header, MACH_SEND_MSG, send_msg.header.msgh_size, 0,
      MACH_PORT_NULL, TestTimeouts::tiny_timeout().InMilliseconds(),
      MACH_PORT_NULL);
  EXPECT_EQ(KERN_SUCCESS, kr) << mach_error_string(kr);

  int code = 0;
  EXPECT_TRUE(process.WaitForExit(&code));
  EXPECT_EQ(0, code);
}

MULTIPROCESS_TEST_MAIN(ChildOutliveSandbox) {
  // Get the synchronization channel.
  mach_port_t port = MACH_PORT_NULL;
  CHECK_EQ(KERN_SUCCESS, bootstrap_look_up(bootstrap_port, "sync", &port));

  // Create a reply port.
  mach_port_t reply_port;
  CHECK_EQ(KERN_SUCCESS, mach_port_allocate(mach_task_self(),
      MACH_PORT_RIGHT_RECEIVE, &reply_port));
  base::mac::ScopedMachReceiveRight scoped_reply_port(reply_port);

  // Send a message to shutdown the sandbox.
  mach_msg_empty_send_t send_msg;
  bzero(&send_msg, sizeof(send_msg));
  send_msg.header.msgh_size = sizeof(send_msg);
  send_msg.header.msgh_local_port = reply_port;
  send_msg.header.msgh_remote_port = port;
  send_msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND,
                                             MACH_MSG_TYPE_MAKE_SEND_ONCE);
  kern_return_t kr = mach_msg_send(&send_msg.header);
  MACH_CHECK(kr == KERN_SUCCESS, kr) << "mach_msg_send";

  // Flood the server's message queue with messages. There should be some
  // pending when the sandbox is destroyed.
  for (int i = 0; i < 20; ++i) {
    mach_port_t tmp = MACH_PORT_NULL;
    std::string name = base::StringPrintf("test.%d", i);
    bootstrap_look_up(bootstrap_port, const_cast<char*>(name.c_str()), &tmp);
  }

  // Ack that the sandbox has been shutdown.
  mach_msg_empty_rcv_t rcv_msg;
  bzero(&rcv_msg, sizeof(rcv_msg));
  rcv_msg.header.msgh_size = sizeof(rcv_msg);
  rcv_msg.header.msgh_local_port = reply_port;
  kr = mach_msg_receive(&rcv_msg.header);
  MACH_CHECK(kr == KERN_SUCCESS, kr) << "mach_msg_receive";

  // Try to message the sandbox.
  bootstrap_look_up(bootstrap_port, "test", &port);

  return 0;
}

}  // namespace sandbox
