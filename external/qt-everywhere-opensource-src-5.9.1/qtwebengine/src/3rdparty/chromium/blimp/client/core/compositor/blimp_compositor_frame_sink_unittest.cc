// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/compositor/blimp_compositor_frame_sink.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/output/compositor_frame.h"
#include "cc/test/fake_compositor_frame_sink_client.h"
#include "cc/test/test_context_provider.h"
#include "cc/test/test_context_support.h"
#include "cc/test/test_gles2_interface.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace client {
namespace {

class FakeBlimpCompositorFrameSinkProxy : public BlimpCompositorFrameSinkProxy {
 public:
  FakeBlimpCompositorFrameSinkProxy(
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner)
      : compositor_task_runner_(compositor_task_runner),
        proxy_client_(nullptr),
        submit_count_(0),
        weak_factory_(this) {
    DCHECK(thread_checker_.CalledOnValidThread());
  }

  ~FakeBlimpCompositorFrameSinkProxy() override {
    DCHECK(thread_checker_.CalledOnValidThread());
  }

  base::WeakPtr<FakeBlimpCompositorFrameSinkProxy> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void BindToProxyClient(base::WeakPtr<BlimpCompositorFrameSinkProxyClient>
                             proxy_client) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    EXPECT_EQ(nullptr, proxy_client_);
    proxy_client_ = proxy_client;
    bound_ = true;
  }

  void SubmitCompositorFrame(cc::CompositorFrame frame) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    submit_count_++;
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(
            &BlimpCompositorFrameSinkProxyClient::SubmitCompositorFrameAck,
            proxy_client_));
  }

  void UnbindProxyClient() override {
    DCHECK(bound_);
    bound_ = false;
    DCHECK(thread_checker_.CalledOnValidThread());
    proxy_client_ = nullptr;
  }

  int submit_count() const { return submit_count_; }
  bool bound() const { return bound_; }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  bool bound_ = false;
  base::WeakPtr<BlimpCompositorFrameSinkProxyClient> proxy_client_;
  int submit_count_;
  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<FakeBlimpCompositorFrameSinkProxy> weak_factory_;
};

class TestContextProvider : public cc::TestContextProvider {
 public:
  static scoped_refptr<TestContextProvider> Create(bool bind_should_fail) {
    return new TestContextProvider(bind_should_fail);
  }

 protected:
  explicit TestContextProvider(bool bind_should_fail)
      : cc::TestContextProvider(base::MakeUnique<cc::TestContextSupport>(),
                                base::MakeUnique<cc::TestGLES2Interface>(),
                                cc::TestWebGraphicsContext3D::Create()),
        bind_should_fail_(bind_should_fail) {}

  ~TestContextProvider() override {}

  bool BindToCurrentThread() override {
    if (bind_should_fail_)
      return false;
    return cc::TestContextProvider::BindToCurrentThread();
  }

 private:
  const bool bind_should_fail_;
};

class BlimpCompositorFrameSinkTest : public testing::Test {
 public:
  BlimpCompositorFrameSinkTest() {}

  void SetUpTest(bool bind_should_fail) {
    main_task_runner_ = base::ThreadTaskRunnerHandle::Get();
    compositor_thread_ = base::MakeUnique<base::Thread>("Compositor");
    ASSERT_TRUE(compositor_thread_->Start());
    compositor_task_runner_ = compositor_thread_->task_runner();
    main_thread_proxy_ = base::MakeUnique<FakeBlimpCompositorFrameSinkProxy>(
        compositor_task_runner_);
    compositor_frame_sink_ = base::MakeUnique<BlimpCompositorFrameSink>(
        TestContextProvider::Create(bind_should_fail), nullptr, nullptr,
        nullptr, main_task_runner_, main_thread_proxy_->GetWeakPtr());

    base::WaitableEvent init_event(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&BlimpCompositorFrameSinkTest::InitOnCompositorThread,
                   base::Unretained(this), &init_event));
    init_event.Wait();

    // Run all tasks so the registration of the BlimpCompositorFrameSink on the
    // main thread completes.
    base::RunLoop().RunUntilIdle();
  }

  void DoSubmitCompositorFrame() {
    base::WaitableEvent submit_event(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    compositor_task_runner_->PostTask(
        FROM_HERE, base::Bind(&BlimpCompositorFrameSinkTest::
                                  DoSubmitCompositorFrameOnCompositorThread,
                              base::Unretained(this), &submit_event));
    submit_event.Wait();
  }

  void TearDown() override {
    EXPECT_EQ(main_thread_proxy_->submit_count(),
              compositor_frame_sink_client_.ack_count());
  }

  void EndTest() {
    base::WaitableEvent shutdown_event(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&BlimpCompositorFrameSinkTest::ShutdownOnCompositorThread,
                   base::Unretained(this), &shutdown_event));
    shutdown_event.Wait();
    compositor_thread_->Stop();

    // Run all tasks so the unregistration of the BlimpCompositorFrameSink on
    // the main thread completes.
    base::RunLoop().RunUntilIdle();
  }

  void InitOnCompositorThread(base::WaitableEvent* event) {
    bound_ =
        compositor_frame_sink_->BindToClient(&compositor_frame_sink_client_);
    event->Signal();
  }

  void DoSubmitCompositorFrameOnCompositorThread(base::WaitableEvent* event) {
    compositor_frame_sink_->SubmitCompositorFrame(cc::CompositorFrame());
    event->Signal();
  }

  void ShutdownOnCompositorThread(base::WaitableEvent* event) {
    base::RunLoop().RunUntilIdle();
    if (bound_) {
      compositor_frame_sink_->DetachFromClient();
      bound_ = false;
    }
    compositor_frame_sink_.reset();
    event->Signal();
  }

  base::MessageLoop loop_;
  std::unique_ptr<base::Thread> compositor_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  std::unique_ptr<BlimpCompositorFrameSink> compositor_frame_sink_;
  std::unique_ptr<FakeBlimpCompositorFrameSinkProxy> main_thread_proxy_;

  bool bound_ = false;
  cc::FakeCompositorFrameSinkClient compositor_frame_sink_client_;
};

TEST_F(BlimpCompositorFrameSinkTest, BindFails) {
  SetUpTest(true);
  EXPECT_FALSE(main_thread_proxy_->bound());
  EndTest();
}

TEST_F(BlimpCompositorFrameSinkTest, BindSucceedsSubmitCompositorFrame) {
  SetUpTest(false);
  EXPECT_TRUE(main_thread_proxy_->bound());

  DoSubmitCompositorFrame();
  DoSubmitCompositorFrame();
  DoSubmitCompositorFrame();

  // Run all tasks so the swap buffer calls to the BlimpCompositorFrameSink on
  // the main thread complete.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, main_thread_proxy_->submit_count());

  EndTest();
}

}  // namespace
}  // namespace client
}  // namespace blimp
