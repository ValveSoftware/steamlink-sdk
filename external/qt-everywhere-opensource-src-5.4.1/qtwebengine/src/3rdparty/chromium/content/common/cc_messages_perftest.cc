// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cc_messages.h"

#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "cc/output/compositor_frame.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

using cc::CompositorFrame;
using cc::DelegatedFrameData;
using cc::DrawQuad;
using cc::PictureDrawQuad;
using cc::RenderPass;
using cc::SharedQuadState;

namespace content {
namespace {

static const int kTimeLimitMillis = 2000;
static const int kNumWarmupRuns = 20;
static const int kTimeCheckInterval = 10;

class CCMessagesPerfTest : public testing::Test {
 protected:
  static void RunTest(std::string test_name, const CompositorFrame& frame) {
    for (int i = 0; i < kNumWarmupRuns; ++i) {
      IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
      IPC::ParamTraits<CompositorFrame>::Write(&msg, frame);
    }

    base::TimeTicks start = base::TimeTicks::HighResNow();
    base::TimeTicks end =
        start + base::TimeDelta::FromMilliseconds(kTimeLimitMillis);
    base::TimeDelta min_time;
    int count = 0;
    while (start < end) {
      for (int i = 0; i < kTimeCheckInterval; ++i) {
        IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
        IPC::ParamTraits<CompositorFrame>::Write(&msg, frame);
        ++count;
      }

      base::TimeTicks now = base::TimeTicks::HighResNow();
      if (now - start < min_time || min_time == base::TimeDelta())
        min_time = now - start;
      start = base::TimeTicks::HighResNow();
    }

    perf_test::PrintResult(
        "min_frame_serialization_time",
        "",
        test_name,
        min_time.InMillisecondsF() / kTimeCheckInterval * 1000,
        "us",
        true);
  }
};

TEST_F(CCMessagesPerfTest, DelegatedFrame_ManyQuads_1_4000) {
  scoped_ptr<CompositorFrame> frame(new CompositorFrame);

  scoped_ptr<RenderPass> render_pass = RenderPass::Create();
  render_pass->CreateAndAppendSharedQuadState();
  for (int i = 0; i < 4000; ++i) {
    render_pass->quad_list.push_back(
        PictureDrawQuad::Create().PassAs<DrawQuad>());
    render_pass->quad_list.back()->shared_quad_state =
        render_pass->shared_quad_state_list.back();
  }

  frame->delegated_frame_data.reset(new DelegatedFrameData);
  frame->delegated_frame_data->render_pass_list.push_back(render_pass.Pass());

  RunTest("DelegatedFrame_ManyQuads_1_4000", *frame);
}

TEST_F(CCMessagesPerfTest, DelegatedFrame_ManyQuads_1_100000) {
  scoped_ptr<CompositorFrame> frame(new CompositorFrame);

  scoped_ptr<RenderPass> render_pass = RenderPass::Create();
  render_pass->CreateAndAppendSharedQuadState();
  for (int i = 0; i < 100000; ++i) {
    render_pass->quad_list.push_back(
        PictureDrawQuad::Create().PassAs<DrawQuad>());
    render_pass->quad_list.back()->shared_quad_state =
        render_pass->shared_quad_state_list.back();
  }

  frame->delegated_frame_data.reset(new DelegatedFrameData);
  frame->delegated_frame_data->render_pass_list.push_back(render_pass.Pass());

  RunTest("DelegatedFrame_ManyQuads_1_100000", *frame);
}

TEST_F(CCMessagesPerfTest, DelegatedFrame_ManyQuads_4000_4000) {
  scoped_ptr<CompositorFrame> frame(new CompositorFrame);

  scoped_ptr<RenderPass> render_pass = RenderPass::Create();
  for (int i = 0; i < 4000; ++i) {
    render_pass->CreateAndAppendSharedQuadState();
    render_pass->quad_list.push_back(
        PictureDrawQuad::Create().PassAs<DrawQuad>());
    render_pass->quad_list.back()->shared_quad_state =
        render_pass->shared_quad_state_list.back();
  }

  frame->delegated_frame_data.reset(new DelegatedFrameData);
  frame->delegated_frame_data->render_pass_list.push_back(render_pass.Pass());

  RunTest("DelegatedFrame_ManyQuads_4000_4000", *frame);
}

TEST_F(CCMessagesPerfTest, DelegatedFrame_ManyQuads_100000_100000) {
  scoped_ptr<CompositorFrame> frame(new CompositorFrame);

  scoped_ptr<RenderPass> render_pass = RenderPass::Create();
  for (int i = 0; i < 100000; ++i) {
    render_pass->CreateAndAppendSharedQuadState();
    render_pass->quad_list.push_back(
        PictureDrawQuad::Create().PassAs<DrawQuad>());
    render_pass->quad_list.back()->shared_quad_state =
        render_pass->shared_quad_state_list.back();
  }

  frame->delegated_frame_data.reset(new DelegatedFrameData);
  frame->delegated_frame_data->render_pass_list.push_back(render_pass.Pass());

  RunTest("DelegatedFrame_ManyQuads_100000_100000", *frame);
}

TEST_F(CCMessagesPerfTest,
       DelegatedFrame_ManyRenderPasses_10000_100) {
  scoped_ptr<CompositorFrame> frame(new CompositorFrame);
  frame->delegated_frame_data.reset(new DelegatedFrameData);

  for (int i = 0; i < 1000; ++i) {
    scoped_ptr<RenderPass> render_pass = RenderPass::Create();
    for (int j = 0; j < 100; ++j) {
      render_pass->CreateAndAppendSharedQuadState();
      render_pass->quad_list.push_back(
          PictureDrawQuad::Create().PassAs<DrawQuad>());
      render_pass->quad_list.back()->shared_quad_state =
          render_pass->shared_quad_state_list.back();
    }
    frame->delegated_frame_data->render_pass_list.push_back(render_pass.Pass());
  }

  RunTest("DelegatedFrame_ManyRenderPasses_10000_100", *frame);
}

}  // namespace
}  // namespace content
