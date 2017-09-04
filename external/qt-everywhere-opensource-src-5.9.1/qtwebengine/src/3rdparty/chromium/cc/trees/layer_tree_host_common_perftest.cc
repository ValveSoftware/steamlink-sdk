// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <deque>
#include <memory>
#include <sstream>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "cc/debug/lap_timer.h"
#include "cc/layers/layer.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/layer_tree_json_parser.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/paths.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_impl.h"
#include "testing/perf/perf_test.h"

namespace cc {
namespace {

static const int kTimeLimitMillis = 2000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;

class LayerTreeHostCommonPerfTest : public LayerTreeTest {
 public:
  LayerTreeHostCommonPerfTest()
      : timer_(kWarmupRuns,
               base::TimeDelta::FromMilliseconds(kTimeLimitMillis),
               kTimeCheckInterval) {}

  void ReadTestFile(const std::string& name) {
    base::FilePath test_data_dir;
    ASSERT_TRUE(PathService::Get(CCPaths::DIR_TEST_DATA, &test_data_dir));
    base::FilePath json_file = test_data_dir.AppendASCII(name + ".json");
    ASSERT_TRUE(base::ReadFileToString(json_file, &json_));
  }

  void SetupTree() override {
    gfx::Size viewport = gfx::Size(720, 1038);
    layer_tree()->SetViewportSize(viewport);
    scoped_refptr<Layer> root =
        ParseTreeFromJson(json_, &content_layer_client_);
    ASSERT_TRUE(root.get());
    layer_tree()->SetRootLayer(root);
    content_layer_client_.set_bounds(viewport);
  }

  void SetTestName(const std::string& name) { test_name_ = name; }

  void AfterTest() override {
    CHECK(!test_name_.empty()) << "Must SetTestName() before TearDown().";
    perf_test::PrintResult("calc_draw_props_time",
                           "",
                           test_name_,
                           1000 * timer_.MsPerLap(),
                           "us",
                           true);
  }

 protected:
  FakeContentLayerClient content_layer_client_;
  LapTimer timer_;
  std::string test_name_;
  std::string json_;
};

class CalcDrawPropsTest : public LayerTreeHostCommonPerfTest {
 public:
  void RunCalcDrawProps() { RunTest(CompositorMode::SINGLE_THREADED); }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override {
    timer_.Reset();
    LayerTreeImpl* active_tree = host_impl->active_tree();

    do {
      bool can_render_to_separate_surface = true;
      int max_texture_size = 8096;
      DoCalcDrawPropertiesImpl(can_render_to_separate_surface,
                               max_texture_size,
                               active_tree,
                               host_impl);

      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    EndTest();
  }

  void DoCalcDrawPropertiesImpl(bool can_render_to_separate_surface,
                                int max_texture_size,
                                LayerTreeImpl* active_tree,
                                LayerTreeHostImpl* host_impl) {
    LayerImplList update_list;
    LayerTreeHostCommon::CalcDrawPropsImplInputs inputs(
        active_tree->root_layer_for_testing(), active_tree->DrawViewportSize(),
        host_impl->DrawTransform(), active_tree->device_scale_factor(),
        active_tree->current_page_scale_factor(),
        active_tree->InnerViewportContainerLayer(),
        active_tree->InnerViewportScrollLayer(),
        active_tree->OuterViewportScrollLayer(),
        active_tree->elastic_overscroll()->Current(active_tree->IsActiveTree()),
        active_tree->OverscrollElasticityLayer(), max_texture_size,
        can_render_to_separate_surface,
        host_impl->settings().layer_transforms_should_scale_layer_contents,
        false,  // do not verify_clip_tree_calculation for perf tests
        false,  // do not verify_visible_rect_calculation for perf tests
        &update_list, active_tree->property_trees());
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);
  }
};

TEST_F(CalcDrawPropsTest, TenTen) {
  SetTestName("10_10");
  ReadTestFile("10_10_layer_tree");
  RunCalcDrawProps();
}

TEST_F(CalcDrawPropsTest, HeavyPage) {
  SetTestName("heavy_page");
  ReadTestFile("heavy_layer_tree");
  RunCalcDrawProps();
}

TEST_F(CalcDrawPropsTest, TouchRegionLight) {
  SetTestName("touch_region_light");
  ReadTestFile("touch_region_light");
  RunCalcDrawProps();
}

TEST_F(CalcDrawPropsTest, TouchRegionHeavy) {
  SetTestName("touch_region_heavy");
  ReadTestFile("touch_region_heavy");
  RunCalcDrawProps();
}

}  // namespace
}  // namespace cc
