// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/threaded_channel.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_mutator.h"
#include "cc/trees/mutator_host.h"

namespace cc {

std::unique_ptr<ThreadedChannel> ThreadedChannel::Create(
    ProxyMain* proxy_main,
    TaskRunnerProvider* task_runner_provider) {
  return base::WrapUnique(
      new ThreadedChannel(proxy_main, task_runner_provider));
}

ThreadedChannel::ThreadedChannel(ProxyMain* proxy_main,
                                 TaskRunnerProvider* task_runner_provider)
    : task_runner_provider_(task_runner_provider),
      main_thread_only_vars_unsafe_(proxy_main),
      compositor_thread_vars_unsafe_(
          main().proxy_main_weak_factory.GetWeakPtr()) {
  DCHECK(IsMainThread());
}

ThreadedChannel::~ThreadedChannel() {
  TRACE_EVENT0("cc", "ThreadChannel::~ThreadChannel");
  DCHECK(IsMainThread());
  DCHECK(!IsInitialized());
}

void ThreadedChannel::UpdateBrowserControlsStateOnImpl(
    BrowserControlsState constraints,
    BrowserControlsState current,
    bool animate) {
  DCHECK(IsMainThread());
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ProxyImpl::UpdateBrowserControlsStateOnImpl,
                 proxy_impl_weak_ptr_, constraints, current, animate));
}

void ThreadedChannel::InitializeCompositorFrameSinkOnImpl(
    CompositorFrameSink* output_surface) {
  DCHECK(IsMainThread());
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyImpl::InitializeCompositorFrameSinkOnImpl,
                            proxy_impl_weak_ptr_, output_surface));
}

void ThreadedChannel::InitializeMutatorOnImpl(
    std::unique_ptr<LayerTreeMutator> mutator) {
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ProxyImpl::InitializeMutatorOnImpl, proxy_impl_weak_ptr_,
                 base::Passed(std::move(mutator))));
}

void ThreadedChannel::MainThreadHasStoppedFlingingOnImpl() {
  DCHECK(IsMainThread());
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyImpl::MainThreadHasStoppedFlingingOnImpl,
                            proxy_impl_weak_ptr_));
}

void ThreadedChannel::SetInputThrottledUntilCommitOnImpl(bool is_throttled) {
  DCHECK(IsMainThread());
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyImpl::SetInputThrottledUntilCommitOnImpl,
                            proxy_impl_weak_ptr_, is_throttled));
}

void ThreadedChannel::SetDeferCommitsOnImpl(bool defer_commits) {
  DCHECK(IsMainThread());
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyImpl::SetDeferCommitsOnImpl,
                            proxy_impl_weak_ptr_, defer_commits));
}

void ThreadedChannel::SetNeedsCommitOnImpl() {
  DCHECK(IsMainThread());
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ProxyImpl::SetNeedsCommitOnImpl, proxy_impl_weak_ptr_));
}

void ThreadedChannel::BeginMainFrameAbortedOnImpl(
    CommitEarlyOutReason reason,
    base::TimeTicks main_thread_start_time,
    std::vector<std::unique_ptr<SwapPromise>> swap_promises) {
  DCHECK(IsMainThread());
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ProxyImpl::BeginMainFrameAbortedOnImpl, proxy_impl_weak_ptr_,
                 reason, main_thread_start_time, base::Passed(&swap_promises)));
}

void ThreadedChannel::SetNeedsRedrawOnImpl(const gfx::Rect& damage_rect) {
  DCHECK(IsMainThread());
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyImpl::SetNeedsRedrawOnImpl,
                            proxy_impl_weak_ptr_, damage_rect));
}

void ThreadedChannel::SetVisibleOnImpl(bool visible) {
  DCHECK(IsMainThread());
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ProxyImpl::SetVisibleOnImpl, proxy_impl_weak_ptr_, visible));
}

void ThreadedChannel::ReleaseCompositorFrameSinkOnImpl(
    CompletionEvent* completion) {
  DCHECK(IsMainThread());
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyImpl::ReleaseCompositorFrameSinkOnImpl,
                            proxy_impl_weak_ptr_, completion));
}

void ThreadedChannel::MainFrameWillHappenOnImplForTesting(
    CompletionEvent* completion,
    bool* main_frame_will_happen) {
  DCHECK(IsMainThread());
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ProxyImpl::MainFrameWillHappenOnImplForTesting,
                 proxy_impl_weak_ptr_, completion, main_frame_will_happen));
}

void ThreadedChannel::NotifyReadyToCommitOnImpl(
    CompletionEvent* completion,
    LayerTreeHostInProcess* layer_tree_host,
    base::TimeTicks main_thread_start_time,
    bool hold_commit_for_activation) {
  DCHECK(IsMainThread());
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ProxyImpl::NotifyReadyToCommitOnImpl, proxy_impl_weak_ptr_,
                 completion, layer_tree_host, main_thread_start_time,
                 hold_commit_for_activation));
}

void ThreadedChannel::SynchronouslyInitializeImpl(
    LayerTreeHostInProcess* layer_tree_host) {
  TRACE_EVENT0("cc", "ThreadChannel::SynchronouslyInitializeImpl");
  DCHECK(IsMainThread());
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(task_runner_provider_);
    CompletionEvent completion;
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ThreadedChannel::InitializeImplOnImpl,
                   base::Unretained(this), &completion, layer_tree_host));
    completion.Wait();
  }
  main().initialized = true;
}

void ThreadedChannel::SynchronouslyCloseImpl() {
  TRACE_EVENT0("cc", "ThreadChannel::~SynchronouslyCloseImpl");
  DCHECK(IsMainThread());

  // Synchronously finishes pending GL operations and deletes the impl.
  // The two steps are done as separate post tasks, so that tasks posted
  // by the GL implementation due to the Finish can be executed by the
  // renderer before shutting it down.
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(task_runner_provider_);
    CompletionEvent completion;
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE, base::Bind(&ProxyImpl::FinishGLOnImpl, proxy_impl_weak_ptr_,
                              &completion));
    completion.Wait();
  }
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(task_runner_provider_);
    CompletionEvent completion;
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE, base::Bind(&ThreadedChannel::CloseImplOnImpl,
                              base::Unretained(this), &completion));
    completion.Wait();
  }
  main().proxy_main_weak_factory.InvalidateWeakPtrs();
  main().initialized = false;
}

void ThreadedChannel::DidReceiveCompositorFrameAck() {
  DCHECK(IsImplThread());
  MainThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyMain::DidReceiveCompositorFrameAck,
                            impl().proxy_main_weak_ptr));
}

void ThreadedChannel::BeginMainFrameNotExpectedSoon() {
  DCHECK(IsImplThread());
  MainThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyMain::BeginMainFrameNotExpectedSoon,
                            impl().proxy_main_weak_ptr));
}

void ThreadedChannel::DidCommitAndDrawFrame() {
  DCHECK(IsImplThread());
  MainThreadTaskRunner()->PostTask(FROM_HERE,
                                   base::Bind(&ProxyMain::DidCommitAndDrawFrame,
                                              impl().proxy_main_weak_ptr));
}

void ThreadedChannel::SetAnimationEvents(
    std::unique_ptr<MutatorEvents> events) {
  DCHECK(IsImplThread());
  MainThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyMain::SetAnimationEvents,
                            impl().proxy_main_weak_ptr, base::Passed(&events)));
}

void ThreadedChannel::DidLoseCompositorFrameSink() {
  DCHECK(IsImplThread());
  MainThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyMain::DidLoseCompositorFrameSink,
                            impl().proxy_main_weak_ptr));
}

void ThreadedChannel::RequestNewCompositorFrameSink() {
  DCHECK(IsImplThread());
  MainThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyMain::RequestNewCompositorFrameSink,
                            impl().proxy_main_weak_ptr));
}

void ThreadedChannel::DidInitializeCompositorFrameSink(bool success) {
  DCHECK(IsImplThread());
  MainThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyMain::DidInitializeCompositorFrameSink,
                            impl().proxy_main_weak_ptr, success));
}

void ThreadedChannel::DidCompletePageScaleAnimation() {
  DCHECK(IsImplThread());
  MainThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyMain::DidCompletePageScaleAnimation,
                            impl().proxy_main_weak_ptr));
}

void ThreadedChannel::BeginMainFrame(
    std::unique_ptr<BeginMainFrameAndCommitState> begin_main_frame_state) {
  DCHECK(IsImplThread());
  MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ProxyMain::BeginMainFrame, impl().proxy_main_weak_ptr,
                 base::Passed(&begin_main_frame_state)));
}

std::unique_ptr<ProxyImpl> ThreadedChannel::CreateProxyImpl(
    ChannelImpl* channel_impl,
    LayerTreeHostInProcess* layer_tree_host,
    TaskRunnerProvider* task_runner_provider) {
  DCHECK(IsImplThread());
  return base::MakeUnique<ProxyImpl>(channel_impl, layer_tree_host,
                                     task_runner_provider);
}

void ThreadedChannel::InitializeImplOnImpl(
    CompletionEvent* completion,
    LayerTreeHostInProcess* layer_tree_host) {
  DCHECK(IsImplThread());
  impl().proxy_impl =
      CreateProxyImpl(this, layer_tree_host, task_runner_provider_);
  impl().proxy_impl_weak_factory =
      base::MakeUnique<base::WeakPtrFactory<ProxyImpl>>(
          impl().proxy_impl.get());
  proxy_impl_weak_ptr_ = impl().proxy_impl_weak_factory->GetWeakPtr();
  completion->Signal();
}

void ThreadedChannel::CloseImplOnImpl(CompletionEvent* completion) {
  DCHECK(IsImplThread());

  // We must destroy the factory and ensure that the ProxyImpl weak pointers are
  // invalidated before destroying proxy_impl.
  impl().proxy_impl_weak_factory.reset();

  impl().proxy_impl.reset();
  completion->Signal();
}

bool ThreadedChannel::IsInitialized() const {
  return main().initialized;
}

base::SingleThreadTaskRunner* ThreadedChannel::MainThreadTaskRunner() const {
  return task_runner_provider_->MainThreadTaskRunner();
}

base::SingleThreadTaskRunner* ThreadedChannel::ImplThreadTaskRunner() const {
  return task_runner_provider_->ImplThreadTaskRunner();
}

bool ThreadedChannel::IsMainThread() const {
  return task_runner_provider_->IsMainThread();
}

bool ThreadedChannel::IsImplThread() const {
  return task_runner_provider_->IsImplThread();
}

ThreadedChannel::MainThreadOnly& ThreadedChannel::main() {
  DCHECK(task_runner_provider_->IsMainThread());
  return main_thread_only_vars_unsafe_;
}

const ThreadedChannel::MainThreadOnly& ThreadedChannel::main() const {
  DCHECK(task_runner_provider_->IsMainThread());
  return main_thread_only_vars_unsafe_;
}

ThreadedChannel::CompositorThreadOnly& ThreadedChannel::impl() {
  DCHECK(task_runner_provider_->IsImplThread());
  return compositor_thread_vars_unsafe_;
}

const ThreadedChannel::CompositorThreadOnly& ThreadedChannel::impl() const {
  DCHECK(task_runner_provider_->IsImplThread());
  return compositor_thread_vars_unsafe_;
}

ThreadedChannel::MainThreadOnly::MainThreadOnly(ProxyMain* proxy_main)
    : proxy_main_weak_factory(proxy_main), initialized(false) {}

ThreadedChannel::MainThreadOnly::~MainThreadOnly() {}

ThreadedChannel::CompositorThreadOnly::CompositorThreadOnly(
    base::WeakPtr<ProxyMain> proxy_main_weak_ptr)
    : proxy_main_weak_ptr(proxy_main_weak_ptr) {}

ThreadedChannel::CompositorThreadOnly::~CompositorThreadOnly() {}

}  // namespace cc
