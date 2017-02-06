// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_PERFTEST_SUPPORT_H_
#define IPC_IPC_PERFTEST_SUPPORT_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/test/test_io_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "ipc/ipc_test_base.h"

namespace IPC {
namespace test {

class ChannelReflectorListener;

class PingPongTestParams {
 public:
  PingPongTestParams(size_t size, int count)
      : message_size_(size), message_count_(count) {
  }

  size_t message_size() const { return message_size_; }
  int message_count() const { return message_count_; }

 private:
  size_t message_size_;
  int message_count_;
};

class IPCChannelPerfTestBase : public IPCTestBase {
 public:
  IPCChannelPerfTestBase();
  ~IPCChannelPerfTestBase() override;

  static std::vector<PingPongTestParams> GetDefaultTestParams();

  void RunTestChannelPingPong(
      const std::vector<PingPongTestParams>& params_list);
  void RunTestChannelProxyPingPong(
      const std::vector<PingPongTestParams>& params_list);

  scoped_refptr<base::TaskRunner> io_task_runner() {
    if (io_thread_)
      return io_thread_->task_runner();
    return base::ThreadTaskRunnerHandle::Get();
  }

 private:
  std::unique_ptr<base::TestIOThread> io_thread_;
};

class PingPongTestClient {
 public:
  PingPongTestClient();
  virtual ~PingPongTestClient();

  virtual std::unique_ptr<Channel> CreateChannel(Listener* listener);
  int RunMain();
  scoped_refptr<base::TaskRunner> task_runner();

 private:
  base::MessageLoopForIO main_message_loop_;
  std::unique_ptr<ChannelReflectorListener> listener_;
  std::unique_ptr<Channel> channel_;
};

// This class locks the current thread to a particular CPU core. This is
// important because otherwise the different threads and processes of these
// tests end up on different CPU cores which means that all of the cores are
// lightly loaded so the OS (Windows and Linux) fails to ramp up the CPU
// frequency, leading to unpredictable and often poor performance.
class LockThreadAffinity {
 public:
  explicit LockThreadAffinity(int cpu_number);
  ~LockThreadAffinity();

 private:
  bool affinity_set_ok_;
#if defined(OS_WIN)
  DWORD_PTR old_affinity_;
#elif defined(OS_LINUX)
  cpu_set_t old_cpuset_;
#endif

  DISALLOW_COPY_AND_ASSIGN(LockThreadAffinity);
};

}
}

#endif  // IPC_IPC_PERFTEST_SUPPORT_H_
