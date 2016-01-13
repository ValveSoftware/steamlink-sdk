// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/tile_manager.h"

#include <algorithm>
#include <limits>
#include <string>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/debug/frame_viewer_instrumentation.h"
#include "cc/debug/traced_value.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/resources/raster_worker_pool.h"
#include "cc/resources/tile.h"
#include "skia/ext/paint_simplifier.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "ui/gfx/rect_conversions.h"

namespace cc {
namespace {

// Flag to indicate whether we should try and detect that
// a tile is of solid color.
const bool kUseColorEstimator = true;

class RasterTaskImpl : public RasterTask {
 public:
  RasterTaskImpl(
      const Resource* resource,
      PicturePileImpl* picture_pile,
      const gfx::Rect& content_rect,
      float contents_scale,
      RasterMode raster_mode,
      TileResolution tile_resolution,
      int layer_id,
      const void* tile_id,
      int source_frame_number,
      bool analyze_picture,
      RenderingStatsInstrumentation* rendering_stats,
      const base::Callback<void(const PicturePileImpl::Analysis&, bool)>& reply,
      ImageDecodeTask::Vector* dependencies)
      : RasterTask(resource, dependencies),
        picture_pile_(picture_pile),
        content_rect_(content_rect),
        contents_scale_(contents_scale),
        raster_mode_(raster_mode),
        tile_resolution_(tile_resolution),
        layer_id_(layer_id),
        tile_id_(tile_id),
        source_frame_number_(source_frame_number),
        analyze_picture_(analyze_picture),
        rendering_stats_(rendering_stats),
        reply_(reply),
        canvas_(NULL) {}

  // Overridden from Task:
  virtual void RunOnWorkerThread() OVERRIDE {
    TRACE_EVENT0("cc", "RasterizerTaskImpl::RunOnWorkerThread");

    DCHECK(picture_pile_);
    if (canvas_) {
      AnalyzeAndRaster(picture_pile_->GetCloneForDrawingOnThread(
          RasterWorkerPool::GetPictureCloneIndexForCurrentThread()));
    }
  }

  // Overridden from RasterizerTask:
  virtual void ScheduleOnOriginThread(RasterizerTaskClient* client) OVERRIDE {
    DCHECK(!canvas_);
    canvas_ = client->AcquireCanvasForRaster(this);
  }
  virtual void CompleteOnOriginThread(RasterizerTaskClient* client) OVERRIDE {
    canvas_ = NULL;
    client->ReleaseCanvasForRaster(this);
  }
  virtual void RunReplyOnOriginThread() OVERRIDE {
    DCHECK(!canvas_);
    reply_.Run(analysis_, !HasFinishedRunning());
  }

 protected:
  virtual ~RasterTaskImpl() { DCHECK(!canvas_); }

 private:
  void AnalyzeAndRaster(PicturePileImpl* picture_pile) {
    DCHECK(picture_pile);
    DCHECK(canvas_);

    if (analyze_picture_) {
      Analyze(picture_pile);
      if (analysis_.is_solid_color)
        return;
    }

    Raster(picture_pile);
  }

  void Analyze(PicturePileImpl* picture_pile) {
    frame_viewer_instrumentation::ScopedAnalyzeTask analyze_task(
        tile_id_, tile_resolution_, source_frame_number_, layer_id_);

    DCHECK(picture_pile);

    picture_pile->AnalyzeInRect(
        content_rect_, contents_scale_, &analysis_, rendering_stats_);

    // Record the solid color prediction.
    UMA_HISTOGRAM_BOOLEAN("Renderer4.SolidColorTilesAnalyzed",
                          analysis_.is_solid_color);

    // Clear the flag if we're not using the estimator.
    analysis_.is_solid_color &= kUseColorEstimator;
  }

  void Raster(PicturePileImpl* picture_pile) {
    frame_viewer_instrumentation::ScopedRasterTask raster_task(
        tile_id_,
        tile_resolution_,
        source_frame_number_,
        layer_id_,
        raster_mode_);
    devtools_instrumentation::ScopedLayerTask layer_task(
        devtools_instrumentation::kRasterTask, layer_id_);

    skia::RefPtr<SkDrawFilter> draw_filter;
    switch (raster_mode_) {
      case LOW_QUALITY_RASTER_MODE:
        draw_filter = skia::AdoptRef(new skia::PaintSimplifier);
        break;
      case HIGH_QUALITY_RASTER_MODE:
        break;
      case NUM_RASTER_MODES:
      default:
        NOTREACHED();
    }
    canvas_->setDrawFilter(draw_filter.get());

    base::TimeDelta prev_rasterize_time =
        rendering_stats_->impl_thread_rendering_stats().rasterize_time;

    // Only record rasterization time for highres tiles, because
    // lowres tiles are not required for activation and therefore
    // introduce noise in the measurement (sometimes they get rasterized
    // before we draw and sometimes they aren't)
    RenderingStatsInstrumentation* stats =
        tile_resolution_ == HIGH_RESOLUTION ? rendering_stats_ : NULL;
    DCHECK(picture_pile);
    picture_pile->RasterToBitmap(
        canvas_, content_rect_, contents_scale_, stats);

    if (rendering_stats_->record_rendering_stats()) {
      base::TimeDelta current_rasterize_time =
          rendering_stats_->impl_thread_rendering_stats().rasterize_time;
      HISTOGRAM_CUSTOM_COUNTS(
          "Renderer4.PictureRasterTimeUS",
          (current_rasterize_time - prev_rasterize_time).InMicroseconds(),
          0,
          100000,
          100);
    }
  }

  PicturePileImpl::Analysis analysis_;
  scoped_refptr<PicturePileImpl> picture_pile_;
  gfx::Rect content_rect_;
  float contents_scale_;
  RasterMode raster_mode_;
  TileResolution tile_resolution_;
  int layer_id_;
  const void* tile_id_;
  int source_frame_number_;
  bool analyze_picture_;
  RenderingStatsInstrumentation* rendering_stats_;
  const base::Callback<void(const PicturePileImpl::Analysis&, bool)> reply_;
  SkCanvas* canvas_;

  DISALLOW_COPY_AND_ASSIGN(RasterTaskImpl);
};

class ImageDecodeTaskImpl : public ImageDecodeTask {
 public:
  ImageDecodeTaskImpl(SkPixelRef* pixel_ref,
                      int layer_id,
                      RenderingStatsInstrumentation* rendering_stats,
                      const base::Callback<void(bool was_canceled)>& reply)
      : pixel_ref_(skia::SharePtr(pixel_ref)),
        layer_id_(layer_id),
        rendering_stats_(rendering_stats),
        reply_(reply) {}

  // Overridden from Task:
  virtual void RunOnWorkerThread() OVERRIDE {
    TRACE_EVENT0("cc", "ImageDecodeTaskImpl::RunOnWorkerThread");

    devtools_instrumentation::ScopedImageDecodeTask image_decode_task(
        pixel_ref_.get());
    // This will cause the image referred to by pixel ref to be decoded.
    pixel_ref_->lockPixels();
    pixel_ref_->unlockPixels();
  }

  // Overridden from RasterizerTask:
  virtual void ScheduleOnOriginThread(RasterizerTaskClient* client) OVERRIDE {}
  virtual void CompleteOnOriginThread(RasterizerTaskClient* client) OVERRIDE {}
  virtual void RunReplyOnOriginThread() OVERRIDE {
    reply_.Run(!HasFinishedRunning());
  }

 protected:
  virtual ~ImageDecodeTaskImpl() {}

 private:
  skia::RefPtr<SkPixelRef> pixel_ref_;
  int layer_id_;
  RenderingStatsInstrumentation* rendering_stats_;
  const base::Callback<void(bool was_canceled)> reply_;

  DISALLOW_COPY_AND_ASSIGN(ImageDecodeTaskImpl);
};

const size_t kScheduledRasterTasksLimit = 32u;

// Memory limit policy works by mapping some bin states to the NEVER bin.
const ManagedTileBin kBinPolicyMap[NUM_TILE_MEMORY_LIMIT_POLICIES][NUM_BINS] = {
    // [ALLOW_NOTHING]
    {NEVER_BIN,  // [NOW_AND_READY_TO_DRAW_BIN]
     NEVER_BIN,  // [NOW_BIN]
     NEVER_BIN,  // [SOON_BIN]
     NEVER_BIN,  // [EVENTUALLY_AND_ACTIVE_BIN]
     NEVER_BIN,  // [EVENTUALLY_BIN]
     NEVER_BIN,  // [AT_LAST_AND_ACTIVE_BIN]
     NEVER_BIN,  // [AT_LAST_BIN]
     NEVER_BIN   // [NEVER_BIN]
    },
    // [ALLOW_ABSOLUTE_MINIMUM]
    {NOW_AND_READY_TO_DRAW_BIN,  // [NOW_AND_READY_TO_DRAW_BIN]
     NOW_BIN,                    // [NOW_BIN]
     NEVER_BIN,                  // [SOON_BIN]
     NEVER_BIN,                  // [EVENTUALLY_AND_ACTIVE_BIN]
     NEVER_BIN,                  // [EVENTUALLY_BIN]
     NEVER_BIN,                  // [AT_LAST_AND_ACTIVE_BIN]
     NEVER_BIN,                  // [AT_LAST_BIN]
     NEVER_BIN                   // [NEVER_BIN]
    },
    // [ALLOW_PREPAINT_ONLY]
    {NOW_AND_READY_TO_DRAW_BIN,  // [NOW_AND_READY_TO_DRAW_BIN]
     NOW_BIN,                    // [NOW_BIN]
     SOON_BIN,                   // [SOON_BIN]
     NEVER_BIN,                  // [EVENTUALLY_AND_ACTIVE_BIN]
     NEVER_BIN,                  // [EVENTUALLY_BIN]
     NEVER_BIN,                  // [AT_LAST_AND_ACTIVE_BIN]
     NEVER_BIN,                  // [AT_LAST_BIN]
     NEVER_BIN                   // [NEVER_BIN]
    },
    // [ALLOW_ANYTHING]
    {NOW_AND_READY_TO_DRAW_BIN,  // [NOW_AND_READY_TO_DRAW_BIN]
     NOW_BIN,                    // [NOW_BIN]
     SOON_BIN,                   // [SOON_BIN]
     EVENTUALLY_AND_ACTIVE_BIN,  // [EVENTUALLY_AND_ACTIVE_BIN]
     EVENTUALLY_BIN,             // [EVENTUALLY_BIN]
     AT_LAST_AND_ACTIVE_BIN,     // [AT_LAST_AND_ACTIVE_BIN]
     AT_LAST_BIN,                // [AT_LAST_BIN]
     NEVER_BIN                   // [NEVER_BIN]
    }};

// Ready to draw works by mapping NOW_BIN to NOW_AND_READY_TO_DRAW_BIN.
const ManagedTileBin kBinReadyToDrawMap[2][NUM_BINS] = {
    // Not ready
    {NOW_AND_READY_TO_DRAW_BIN,  // [NOW_AND_READY_TO_DRAW_BIN]
     NOW_BIN,                    // [NOW_BIN]
     SOON_BIN,                   // [SOON_BIN]
     EVENTUALLY_AND_ACTIVE_BIN,  // [EVENTUALLY_AND_ACTIVE_BIN]
     EVENTUALLY_BIN,             // [EVENTUALLY_BIN]
     AT_LAST_AND_ACTIVE_BIN,     // [AT_LAST_AND_ACTIVE_BIN]
     AT_LAST_BIN,                // [AT_LAST_BIN]
     NEVER_BIN                   // [NEVER_BIN]
    },
    // Ready
    {NOW_AND_READY_TO_DRAW_BIN,  // [NOW_AND_READY_TO_DRAW_BIN]
     NOW_AND_READY_TO_DRAW_BIN,  // [NOW_BIN]
     SOON_BIN,                   // [SOON_BIN]
     EVENTUALLY_AND_ACTIVE_BIN,  // [EVENTUALLY_AND_ACTIVE_BIN]
     EVENTUALLY_BIN,             // [EVENTUALLY_BIN]
     AT_LAST_AND_ACTIVE_BIN,     // [AT_LAST_AND_ACTIVE_BIN]
     AT_LAST_BIN,                // [AT_LAST_BIN]
     NEVER_BIN                   // [NEVER_BIN]
    }};

// Active works by mapping some bin stats to equivalent _ACTIVE_BIN state.
const ManagedTileBin kBinIsActiveMap[2][NUM_BINS] = {
    // Inactive
    {NOW_AND_READY_TO_DRAW_BIN,  // [NOW_AND_READY_TO_DRAW_BIN]
     NOW_BIN,                    // [NOW_BIN]
     SOON_BIN,                   // [SOON_BIN]
     EVENTUALLY_AND_ACTIVE_BIN,  // [EVENTUALLY_AND_ACTIVE_BIN]
     EVENTUALLY_BIN,             // [EVENTUALLY_BIN]
     AT_LAST_AND_ACTIVE_BIN,     // [AT_LAST_AND_ACTIVE_BIN]
     AT_LAST_BIN,                // [AT_LAST_BIN]
     NEVER_BIN                   // [NEVER_BIN]
    },
    // Active
    {NOW_AND_READY_TO_DRAW_BIN,  // [NOW_AND_READY_TO_DRAW_BIN]
     NOW_BIN,                    // [NOW_BIN]
     SOON_BIN,                   // [SOON_BIN]
     EVENTUALLY_AND_ACTIVE_BIN,  // [EVENTUALLY_AND_ACTIVE_BIN]
     EVENTUALLY_AND_ACTIVE_BIN,  // [EVENTUALLY_BIN]
     AT_LAST_AND_ACTIVE_BIN,     // [AT_LAST_AND_ACTIVE_BIN]
     AT_LAST_AND_ACTIVE_BIN,     // [AT_LAST_BIN]
     NEVER_BIN                   // [NEVER_BIN]
    }};

// Determine bin based on three categories of tiles: things we need now,
// things we need soon, and eventually.
inline ManagedTileBin BinFromTilePriority(const TilePriority& prio) {
  if (prio.priority_bin == TilePriority::NOW)
    return NOW_BIN;

  if (prio.priority_bin == TilePriority::SOON)
    return SOON_BIN;

  if (prio.distance_to_visible == std::numeric_limits<float>::infinity())
    return NEVER_BIN;

  return EVENTUALLY_BIN;
}

}  // namespace

RasterTaskCompletionStats::RasterTaskCompletionStats()
    : completed_count(0u), canceled_count(0u) {}

scoped_ptr<base::Value> RasterTaskCompletionStatsAsValue(
    const RasterTaskCompletionStats& stats) {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());
  state->SetInteger("completed_count", stats.completed_count);
  state->SetInteger("canceled_count", stats.canceled_count);
  return state.PassAs<base::Value>();
}

// static
scoped_ptr<TileManager> TileManager::Create(
    TileManagerClient* client,
    base::SequencedTaskRunner* task_runner,
    ResourcePool* resource_pool,
    Rasterizer* rasterizer,
    RenderingStatsInstrumentation* rendering_stats_instrumentation) {
  return make_scoped_ptr(new TileManager(client,
                                         task_runner,
                                         resource_pool,
                                         rasterizer,
                                         rendering_stats_instrumentation));
}

TileManager::TileManager(
    TileManagerClient* client,
    base::SequencedTaskRunner* task_runner,
    ResourcePool* resource_pool,
    Rasterizer* rasterizer,
    RenderingStatsInstrumentation* rendering_stats_instrumentation)
    : client_(client),
      task_runner_(task_runner),
      resource_pool_(resource_pool),
      rasterizer_(rasterizer),
      prioritized_tiles_dirty_(false),
      all_tiles_that_need_to_be_rasterized_have_memory_(true),
      all_tiles_required_for_activation_have_memory_(true),
      bytes_releasable_(0),
      resources_releasable_(0),
      ever_exceeded_memory_budget_(false),
      rendering_stats_instrumentation_(rendering_stats_instrumentation),
      did_initialize_visible_tile_(false),
      did_check_for_completed_tasks_since_last_schedule_tasks_(true),
      ready_to_activate_check_notifier_(
          task_runner_,
          base::Bind(&TileManager::CheckIfReadyToActivate,
                     base::Unretained(this))) {
  rasterizer_->SetClient(this);
}

TileManager::~TileManager() {
  // Reset global state and manage. This should cause
  // our memory usage to drop to zero.
  global_state_ = GlobalStateThatImpactsTilePriority();

  CleanUpReleasedTiles();
  DCHECK_EQ(0u, tiles_.size());

  RasterTaskQueue empty;
  rasterizer_->ScheduleTasks(&empty);
  orphan_raster_tasks_.clear();

  // This should finish all pending tasks and release any uninitialized
  // resources.
  rasterizer_->Shutdown();
  rasterizer_->CheckForCompletedTasks();

  DCHECK_EQ(0u, bytes_releasable_);
  DCHECK_EQ(0u, resources_releasable_);
}

void TileManager::Release(Tile* tile) {
  prioritized_tiles_dirty_ = true;
  released_tiles_.push_back(tile);
}

void TileManager::DidChangeTilePriority(Tile* tile) {
  prioritized_tiles_dirty_ = true;
}

bool TileManager::ShouldForceTasksRequiredForActivationToComplete() const {
  return global_state_.tree_priority != SMOOTHNESS_TAKES_PRIORITY;
}

void TileManager::CleanUpReleasedTiles() {
  for (std::vector<Tile*>::iterator it = released_tiles_.begin();
       it != released_tiles_.end();
       ++it) {
    Tile* tile = *it;
    ManagedTileState& mts = tile->managed_state();

    for (int mode = 0; mode < NUM_RASTER_MODES; ++mode) {
      FreeResourceForTile(tile, static_cast<RasterMode>(mode));
      orphan_raster_tasks_.push_back(mts.tile_versions[mode].raster_task_);
    }

    DCHECK(tiles_.find(tile->id()) != tiles_.end());
    tiles_.erase(tile->id());

    LayerCountMap::iterator layer_it =
        used_layer_counts_.find(tile->layer_id());
    DCHECK_GT(layer_it->second, 0);
    if (--layer_it->second == 0) {
      used_layer_counts_.erase(layer_it);
      image_decode_tasks_.erase(tile->layer_id());
    }

    delete tile;
  }

  released_tiles_.clear();
}

void TileManager::UpdatePrioritizedTileSetIfNeeded() {
  if (!prioritized_tiles_dirty_)
    return;

  CleanUpReleasedTiles();

  prioritized_tiles_.Clear();
  GetTilesWithAssignedBins(&prioritized_tiles_);
  prioritized_tiles_dirty_ = false;
}

void TileManager::DidFinishRunningTasks() {
  TRACE_EVENT0("cc", "TileManager::DidFinishRunningTasks");

  bool memory_usage_above_limit = resource_pool_->total_memory_usage_bytes() >
                                  global_state_.soft_memory_limit_in_bytes;

  // When OOM, keep re-assigning memory until we reach a steady state
  // where top-priority tiles are initialized.
  if (all_tiles_that_need_to_be_rasterized_have_memory_ &&
      !memory_usage_above_limit)
    return;

  rasterizer_->CheckForCompletedTasks();
  did_check_for_completed_tasks_since_last_schedule_tasks_ = true;

  TileVector tiles_that_need_to_be_rasterized;
  AssignGpuMemoryToTiles(&prioritized_tiles_,
                         &tiles_that_need_to_be_rasterized);

  // |tiles_that_need_to_be_rasterized| will be empty when we reach a
  // steady memory state. Keep scheduling tasks until we reach this state.
  if (!tiles_that_need_to_be_rasterized.empty()) {
    ScheduleTasks(tiles_that_need_to_be_rasterized);
    return;
  }

  resource_pool_->ReduceResourceUsage();

  // We don't reserve memory for required-for-activation tiles during
  // accelerated gestures, so we just postpone activation when we don't
  // have these tiles, and activate after the accelerated gesture.
  bool allow_rasterize_on_demand =
      global_state_.tree_priority != SMOOTHNESS_TAKES_PRIORITY;

  // Use on-demand raster for any required-for-activation tiles that have not
  // been been assigned memory after reaching a steady memory state. This
  // ensures that we activate even when OOM.
  for (TileMap::iterator it = tiles_.begin(); it != tiles_.end(); ++it) {
    Tile* tile = it->second;
    ManagedTileState& mts = tile->managed_state();
    ManagedTileState::TileVersion& tile_version =
        mts.tile_versions[mts.raster_mode];

    if (tile->required_for_activation() && !tile_version.IsReadyToDraw()) {
      // If we can't raster on demand, give up early (and don't activate).
      if (!allow_rasterize_on_demand)
        return;

      tile_version.set_rasterize_on_demand();
      client_->NotifyTileStateChanged(tile);
    }
  }

  DCHECK(IsReadyToActivate());
  ready_to_activate_check_notifier_.Schedule();
}

void TileManager::DidFinishRunningTasksRequiredForActivation() {
  // This is only a true indication that all tiles required for
  // activation are initialized when no tiles are OOM. We need to
  // wait for DidFinishRunningTasks() to be called, try to re-assign
  // memory and in worst case use on-demand raster when tiles
  // required for activation are OOM.
  if (!all_tiles_required_for_activation_have_memory_)
    return;

  ready_to_activate_check_notifier_.Schedule();
}

void TileManager::GetTilesWithAssignedBins(PrioritizedTileSet* tiles) {
  TRACE_EVENT0("cc", "TileManager::GetTilesWithAssignedBins");

  const TileMemoryLimitPolicy memory_policy = global_state_.memory_limit_policy;
  const TreePriority tree_priority = global_state_.tree_priority;

  // For each tree, bin into different categories of tiles.
  for (TileMap::const_iterator it = tiles_.begin(); it != tiles_.end(); ++it) {
    Tile* tile = it->second;
    ManagedTileState& mts = tile->managed_state();

    const ManagedTileState::TileVersion& tile_version =
        tile->GetTileVersionForDrawing();
    bool tile_is_ready_to_draw = tile_version.IsReadyToDraw();
    bool tile_is_active = tile_is_ready_to_draw ||
                          mts.tile_versions[mts.raster_mode].raster_task_;

    // Get the active priority and bin.
    TilePriority active_priority = tile->priority(ACTIVE_TREE);
    ManagedTileBin active_bin = BinFromTilePriority(active_priority);

    // Get the pending priority and bin.
    TilePriority pending_priority = tile->priority(PENDING_TREE);
    ManagedTileBin pending_bin = BinFromTilePriority(pending_priority);

    bool pending_is_low_res = pending_priority.resolution == LOW_RESOLUTION;
    bool pending_is_non_ideal =
        pending_priority.resolution == NON_IDEAL_RESOLUTION;
    bool active_is_non_ideal =
        active_priority.resolution == NON_IDEAL_RESOLUTION;

    // Adjust bin state based on if ready to draw.
    active_bin = kBinReadyToDrawMap[tile_is_ready_to_draw][active_bin];
    pending_bin = kBinReadyToDrawMap[tile_is_ready_to_draw][pending_bin];

    // Adjust bin state based on if active.
    active_bin = kBinIsActiveMap[tile_is_active][active_bin];
    pending_bin = kBinIsActiveMap[tile_is_active][pending_bin];

    // We never want to paint new non-ideal tiles, as we always have
    // a high-res tile covering that content (paint that instead).
    if (!tile_is_ready_to_draw && active_is_non_ideal)
      active_bin = NEVER_BIN;
    if (!tile_is_ready_to_draw && pending_is_non_ideal)
      pending_bin = NEVER_BIN;

    ManagedTileBin tree_bin[NUM_TREES];
    tree_bin[ACTIVE_TREE] = kBinPolicyMap[memory_policy][active_bin];
    tree_bin[PENDING_TREE] = kBinPolicyMap[memory_policy][pending_bin];

    // Adjust pending bin state for low res tiles. This prevents pending tree
    // low-res tiles from being initialized before high-res tiles.
    if (pending_is_low_res)
      tree_bin[PENDING_TREE] = std::max(tree_bin[PENDING_TREE], EVENTUALLY_BIN);

    TilePriority tile_priority;
    switch (tree_priority) {
      case SAME_PRIORITY_FOR_BOTH_TREES:
        mts.bin = std::min(tree_bin[ACTIVE_TREE], tree_bin[PENDING_TREE]);
        tile_priority = tile->combined_priority();
        break;
      case SMOOTHNESS_TAKES_PRIORITY:
        mts.bin = tree_bin[ACTIVE_TREE];
        tile_priority = active_priority;
        break;
      case NEW_CONTENT_TAKES_PRIORITY:
        mts.bin = tree_bin[PENDING_TREE];
        tile_priority = pending_priority;
        break;
    }

    // Bump up the priority if we determined it's NEVER_BIN on one tree,
    // but is still required on the other tree.
    bool is_in_never_bin_on_both_trees = tree_bin[ACTIVE_TREE] == NEVER_BIN &&
                                         tree_bin[PENDING_TREE] == NEVER_BIN;

    if (mts.bin == NEVER_BIN && !is_in_never_bin_on_both_trees)
      mts.bin = tile_is_active ? AT_LAST_AND_ACTIVE_BIN : AT_LAST_BIN;

    mts.resolution = tile_priority.resolution;
    mts.priority_bin = tile_priority.priority_bin;
    mts.distance_to_visible = tile_priority.distance_to_visible;
    mts.required_for_activation = tile_priority.required_for_activation;

    mts.visible_and_ready_to_draw =
        tree_bin[ACTIVE_TREE] == NOW_AND_READY_TO_DRAW_BIN;

    // Tiles that are required for activation shouldn't be in NEVER_BIN unless
    // smoothness takes priority or memory policy allows nothing to be
    // initialized.
    DCHECK(!mts.required_for_activation || mts.bin != NEVER_BIN ||
           tree_priority == SMOOTHNESS_TAKES_PRIORITY ||
           memory_policy == ALLOW_NOTHING);

    // If the tile is in NEVER_BIN and it does not have an active task, then we
    // can release the resources early. If it does have the task however, we
    // should keep it in the prioritized tile set to ensure that AssignGpuMemory
    // can visit it.
    if (mts.bin == NEVER_BIN &&
        !mts.tile_versions[mts.raster_mode].raster_task_) {
      FreeResourcesForTileAndNotifyClientIfTileWasReadyToDraw(tile);
      continue;
    }

    // Insert the tile into a priority set.
    tiles->InsertTile(tile, mts.bin);
  }
}

void TileManager::ManageTiles(const GlobalStateThatImpactsTilePriority& state) {
  TRACE_EVENT0("cc", "TileManager::ManageTiles");

  // Update internal state.
  if (state != global_state_) {
    global_state_ = state;
    prioritized_tiles_dirty_ = true;
  }

  // We need to call CheckForCompletedTasks() once in-between each call
  // to ScheduleTasks() to prevent canceled tasks from being scheduled.
  if (!did_check_for_completed_tasks_since_last_schedule_tasks_) {
    rasterizer_->CheckForCompletedTasks();
    did_check_for_completed_tasks_since_last_schedule_tasks_ = true;
  }

  UpdatePrioritizedTileSetIfNeeded();

  TileVector tiles_that_need_to_be_rasterized;
  AssignGpuMemoryToTiles(&prioritized_tiles_,
                         &tiles_that_need_to_be_rasterized);

  // Finally, schedule rasterizer tasks.
  ScheduleTasks(tiles_that_need_to_be_rasterized);

  TRACE_EVENT_INSTANT1("cc",
                       "DidManage",
                       TRACE_EVENT_SCOPE_THREAD,
                       "state",
                       TracedValue::FromValue(BasicStateAsValue().release()));

  TRACE_COUNTER_ID1("cc",
                    "unused_memory_bytes",
                    this,
                    resource_pool_->total_memory_usage_bytes() -
                        resource_pool_->acquired_memory_usage_bytes());
}

bool TileManager::UpdateVisibleTiles() {
  TRACE_EVENT0("cc", "TileManager::UpdateVisibleTiles");

  rasterizer_->CheckForCompletedTasks();
  did_check_for_completed_tasks_since_last_schedule_tasks_ = true;

  TRACE_EVENT_INSTANT1(
      "cc",
      "DidUpdateVisibleTiles",
      TRACE_EVENT_SCOPE_THREAD,
      "stats",
      TracedValue::FromValue(RasterTaskCompletionStatsAsValue(
                                 update_visible_tiles_stats_).release()));
  update_visible_tiles_stats_ = RasterTaskCompletionStats();

  bool did_initialize_visible_tile = did_initialize_visible_tile_;
  did_initialize_visible_tile_ = false;
  return did_initialize_visible_tile;
}

scoped_ptr<base::Value> TileManager::BasicStateAsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());
  state->SetInteger("tile_count", tiles_.size());
  state->Set("global_state", global_state_.AsValue().release());
  return state.PassAs<base::Value>();
}

scoped_ptr<base::Value> TileManager::AllTilesAsValue() const {
  scoped_ptr<base::ListValue> state(new base::ListValue());
  for (TileMap::const_iterator it = tiles_.begin(); it != tiles_.end(); ++it)
    state->Append(it->second->AsValue().release());

  return state.PassAs<base::Value>();
}

void TileManager::AssignGpuMemoryToTiles(
    PrioritizedTileSet* tiles,
    TileVector* tiles_that_need_to_be_rasterized) {
  TRACE_EVENT0("cc", "TileManager::AssignGpuMemoryToTiles");

  // Maintain the list of released resources that can potentially be re-used
  // or deleted.
  // If this operation becomes expensive too, only do this after some
  // resource(s) was returned. Note that in that case, one also need to
  // invalidate when releasing some resource from the pool.
  resource_pool_->CheckBusyResources();

  // Now give memory out to the tiles until we're out, and build
  // the needs-to-be-rasterized queue.
  all_tiles_that_need_to_be_rasterized_have_memory_ = true;
  all_tiles_required_for_activation_have_memory_ = true;

  // Cast to prevent overflow.
  int64 soft_bytes_available =
      static_cast<int64>(bytes_releasable_) +
      static_cast<int64>(global_state_.soft_memory_limit_in_bytes) -
      static_cast<int64>(resource_pool_->acquired_memory_usage_bytes());
  int64 hard_bytes_available =
      static_cast<int64>(bytes_releasable_) +
      static_cast<int64>(global_state_.hard_memory_limit_in_bytes) -
      static_cast<int64>(resource_pool_->acquired_memory_usage_bytes());
  int resources_available = resources_releasable_ +
                            global_state_.num_resources_limit -
                            resource_pool_->acquired_resource_count();
  size_t soft_bytes_allocatable =
      std::max(static_cast<int64>(0), soft_bytes_available);
  size_t hard_bytes_allocatable =
      std::max(static_cast<int64>(0), hard_bytes_available);
  size_t resources_allocatable = std::max(0, resources_available);

  size_t bytes_that_exceeded_memory_budget = 0;
  size_t soft_bytes_left = soft_bytes_allocatable;
  size_t hard_bytes_left = hard_bytes_allocatable;

  size_t resources_left = resources_allocatable;
  bool oomed_soft = false;
  bool oomed_hard = false;
  bool have_hit_soft_memory = false;  // Soft memory comes after hard.

  unsigned schedule_priority = 1u;
  for (PrioritizedTileSet::Iterator it(tiles, true); it; ++it) {
    Tile* tile = *it;
    ManagedTileState& mts = tile->managed_state();

    mts.scheduled_priority = schedule_priority++;

    mts.raster_mode = tile->DetermineOverallRasterMode();

    ManagedTileState::TileVersion& tile_version =
        mts.tile_versions[mts.raster_mode];

    // If this tile doesn't need a resource, then nothing to do.
    if (!tile_version.requires_resource())
      continue;

    // If the tile is not needed, free it up.
    if (mts.bin == NEVER_BIN) {
      FreeResourcesForTileAndNotifyClientIfTileWasReadyToDraw(tile);
      continue;
    }

    const bool tile_uses_hard_limit = mts.bin <= NOW_BIN;
    const size_t bytes_if_allocated = BytesConsumedIfAllocated(tile);
    const size_t tile_bytes_left =
        (tile_uses_hard_limit) ? hard_bytes_left : soft_bytes_left;

    // Hard-limit is reserved for tiles that would cause a calamity
    // if they were to go away, so by definition they are the highest
    // priority memory, and must be at the front of the list.
    DCHECK(!(have_hit_soft_memory && tile_uses_hard_limit));
    have_hit_soft_memory |= !tile_uses_hard_limit;

    size_t tile_bytes = 0;
    size_t tile_resources = 0;

    // It costs to maintain a resource.
    for (int mode = 0; mode < NUM_RASTER_MODES; ++mode) {
      if (mts.tile_versions[mode].resource_) {
        tile_bytes += bytes_if_allocated;
        tile_resources++;
      }
    }

    // Allow lower priority tiles with initialized resources to keep
    // their memory by only assigning memory to new raster tasks if
    // they can be scheduled.
    bool reached_scheduled_raster_tasks_limit =
        tiles_that_need_to_be_rasterized->size() >= kScheduledRasterTasksLimit;
    if (!reached_scheduled_raster_tasks_limit) {
      // If we don't have the required version, and it's not in flight
      // then we'll have to pay to create a new task.
      if (!tile_version.resource_ && !tile_version.raster_task_) {
        tile_bytes += bytes_if_allocated;
        tile_resources++;
      }
    }

    // Tile is OOM.
    if (tile_bytes > tile_bytes_left || tile_resources > resources_left) {
      bool was_ready_to_draw = tile->IsReadyToDraw();

      FreeResourcesForTile(tile);

      // This tile was already on screen and now its resources have been
      // released. In order to prevent checkerboarding, set this tile as
      // rasterize on demand immediately.
      if (mts.visible_and_ready_to_draw)
        tile_version.set_rasterize_on_demand();

      if (was_ready_to_draw)
        client_->NotifyTileStateChanged(tile);

      oomed_soft = true;
      if (tile_uses_hard_limit) {
        oomed_hard = true;
        bytes_that_exceeded_memory_budget += tile_bytes;
      }
    } else {
      resources_left -= tile_resources;
      hard_bytes_left -= tile_bytes;
      soft_bytes_left =
          (soft_bytes_left > tile_bytes) ? soft_bytes_left - tile_bytes : 0;
      if (tile_version.resource_)
        continue;
    }

    DCHECK(!tile_version.resource_);

    // Tile shouldn't be rasterized if |tiles_that_need_to_be_rasterized|
    // has reached it's limit or we've failed to assign gpu memory to this
    // or any higher priority tile. Preventing tiles that fit into memory
    // budget to be rasterized when higher priority tile is oom is
    // important for two reasons:
    // 1. Tile size should not impact raster priority.
    // 2. Tiles with existing raster task could otherwise incorrectly
    //    be added as they are not affected by |bytes_allocatable|.
    bool can_schedule_tile =
        !oomed_soft && !reached_scheduled_raster_tasks_limit;

    if (!can_schedule_tile) {
      all_tiles_that_need_to_be_rasterized_have_memory_ = false;
      if (tile->required_for_activation())
        all_tiles_required_for_activation_have_memory_ = false;
      it.DisablePriorityOrdering();
      continue;
    }

    tiles_that_need_to_be_rasterized->push_back(tile);
  }

  // OOM reporting uses hard-limit, soft-OOM is normal depending on limit.
  ever_exceeded_memory_budget_ |= oomed_hard;
  if (ever_exceeded_memory_budget_) {
    TRACE_COUNTER_ID2("cc",
                      "over_memory_budget",
                      this,
                      "budget",
                      global_state_.hard_memory_limit_in_bytes,
                      "over",
                      bytes_that_exceeded_memory_budget);
  }
  memory_stats_from_last_assign_.total_budget_in_bytes =
      global_state_.hard_memory_limit_in_bytes;
  memory_stats_from_last_assign_.bytes_allocated =
      hard_bytes_allocatable - hard_bytes_left;
  memory_stats_from_last_assign_.bytes_unreleasable =
      resource_pool_->acquired_memory_usage_bytes() - bytes_releasable_;
  memory_stats_from_last_assign_.bytes_over = bytes_that_exceeded_memory_budget;
}

void TileManager::FreeResourceForTile(Tile* tile, RasterMode mode) {
  ManagedTileState& mts = tile->managed_state();
  if (mts.tile_versions[mode].resource_) {
    resource_pool_->ReleaseResource(mts.tile_versions[mode].resource_.Pass());

    DCHECK_GE(bytes_releasable_, BytesConsumedIfAllocated(tile));
    DCHECK_GE(resources_releasable_, 1u);

    bytes_releasable_ -= BytesConsumedIfAllocated(tile);
    --resources_releasable_;
  }
}

void TileManager::FreeResourcesForTile(Tile* tile) {
  for (int mode = 0; mode < NUM_RASTER_MODES; ++mode) {
    FreeResourceForTile(tile, static_cast<RasterMode>(mode));
  }
}

void TileManager::FreeUnusedResourcesForTile(Tile* tile) {
  DCHECK(tile->IsReadyToDraw());
  ManagedTileState& mts = tile->managed_state();
  RasterMode used_mode = LOW_QUALITY_RASTER_MODE;
  for (int mode = 0; mode < NUM_RASTER_MODES; ++mode) {
    if (mts.tile_versions[mode].IsReadyToDraw()) {
      used_mode = static_cast<RasterMode>(mode);
      break;
    }
  }

  for (int mode = 0; mode < NUM_RASTER_MODES; ++mode) {
    if (mode != used_mode)
      FreeResourceForTile(tile, static_cast<RasterMode>(mode));
  }
}

void TileManager::FreeResourcesForTileAndNotifyClientIfTileWasReadyToDraw(
    Tile* tile) {
  bool was_ready_to_draw = tile->IsReadyToDraw();
  FreeResourcesForTile(tile);
  if (was_ready_to_draw)
    client_->NotifyTileStateChanged(tile);
}

void TileManager::ScheduleTasks(
    const TileVector& tiles_that_need_to_be_rasterized) {
  TRACE_EVENT1("cc",
               "TileManager::ScheduleTasks",
               "count",
               tiles_that_need_to_be_rasterized.size());

  DCHECK(did_check_for_completed_tasks_since_last_schedule_tasks_);

  raster_queue_.Reset();

  // Build a new task queue containing all task currently needed. Tasks
  // are added in order of priority, highest priority task first.
  for (TileVector::const_iterator it = tiles_that_need_to_be_rasterized.begin();
       it != tiles_that_need_to_be_rasterized.end();
       ++it) {
    Tile* tile = *it;
    ManagedTileState& mts = tile->managed_state();
    ManagedTileState::TileVersion& tile_version =
        mts.tile_versions[mts.raster_mode];

    DCHECK(tile_version.requires_resource());
    DCHECK(!tile_version.resource_);

    if (!tile_version.raster_task_)
      tile_version.raster_task_ = CreateRasterTask(tile);

    raster_queue_.items.push_back(RasterTaskQueue::Item(
        tile_version.raster_task_.get(), tile->required_for_activation()));
    raster_queue_.required_for_activation_count +=
        tile->required_for_activation();
  }

  // We must reduce the amount of unused resoruces before calling
  // ScheduleTasks to prevent usage from rising above limits.
  resource_pool_->ReduceResourceUsage();

  // Schedule running of |raster_tasks_|. This replaces any previously
  // scheduled tasks and effectively cancels all tasks not present
  // in |raster_tasks_|.
  rasterizer_->ScheduleTasks(&raster_queue_);

  // It's now safe to clean up orphan tasks as raster worker pool is not
  // allowed to keep around unreferenced raster tasks after ScheduleTasks() has
  // been called.
  orphan_raster_tasks_.clear();

  did_check_for_completed_tasks_since_last_schedule_tasks_ = false;
}

scoped_refptr<ImageDecodeTask> TileManager::CreateImageDecodeTask(
    Tile* tile,
    SkPixelRef* pixel_ref) {
  return make_scoped_refptr(new ImageDecodeTaskImpl(
      pixel_ref,
      tile->layer_id(),
      rendering_stats_instrumentation_,
      base::Bind(&TileManager::OnImageDecodeTaskCompleted,
                 base::Unretained(this),
                 tile->layer_id(),
                 base::Unretained(pixel_ref))));
}

scoped_refptr<RasterTask> TileManager::CreateRasterTask(Tile* tile) {
  ManagedTileState& mts = tile->managed_state();

  scoped_ptr<ScopedResource> resource =
      resource_pool_->AcquireResource(tile->tile_size_.size());
  const ScopedResource* const_resource = resource.get();

  // Create and queue all image decode tasks that this tile depends on.
  ImageDecodeTask::Vector decode_tasks;
  PixelRefTaskMap& existing_pixel_refs = image_decode_tasks_[tile->layer_id()];
  for (PicturePileImpl::PixelRefIterator iter(
           tile->content_rect(), tile->contents_scale(), tile->picture_pile());
       iter;
       ++iter) {
    SkPixelRef* pixel_ref = *iter;
    uint32_t id = pixel_ref->getGenerationID();

    // Append existing image decode task if available.
    PixelRefTaskMap::iterator decode_task_it = existing_pixel_refs.find(id);
    if (decode_task_it != existing_pixel_refs.end()) {
      decode_tasks.push_back(decode_task_it->second);
      continue;
    }

    // Create and append new image decode task for this pixel ref.
    scoped_refptr<ImageDecodeTask> decode_task =
        CreateImageDecodeTask(tile, pixel_ref);
    decode_tasks.push_back(decode_task);
    existing_pixel_refs[id] = decode_task;
  }

  return make_scoped_refptr(
      new RasterTaskImpl(const_resource,
                         tile->picture_pile(),
                         tile->content_rect(),
                         tile->contents_scale(),
                         mts.raster_mode,
                         mts.resolution,
                         tile->layer_id(),
                         static_cast<const void*>(tile),
                         tile->source_frame_number(),
                         tile->use_picture_analysis(),
                         rendering_stats_instrumentation_,
                         base::Bind(&TileManager::OnRasterTaskCompleted,
                                    base::Unretained(this),
                                    tile->id(),
                                    base::Passed(&resource),
                                    mts.raster_mode),
                         &decode_tasks));
}

void TileManager::OnImageDecodeTaskCompleted(int layer_id,
                                             SkPixelRef* pixel_ref,
                                             bool was_canceled) {
  // If the task was canceled, we need to clean it up
  // from |image_decode_tasks_|.
  if (!was_canceled)
    return;

  LayerPixelRefTaskMap::iterator layer_it = image_decode_tasks_.find(layer_id);
  if (layer_it == image_decode_tasks_.end())
    return;

  PixelRefTaskMap& pixel_ref_tasks = layer_it->second;
  PixelRefTaskMap::iterator task_it =
      pixel_ref_tasks.find(pixel_ref->getGenerationID());

  if (task_it != pixel_ref_tasks.end())
    pixel_ref_tasks.erase(task_it);
}

void TileManager::OnRasterTaskCompleted(
    Tile::Id tile_id,
    scoped_ptr<ScopedResource> resource,
    RasterMode raster_mode,
    const PicturePileImpl::Analysis& analysis,
    bool was_canceled) {
  TileMap::iterator it = tiles_.find(tile_id);
  if (it == tiles_.end()) {
    ++update_visible_tiles_stats_.canceled_count;
    resource_pool_->ReleaseResource(resource.Pass());
    return;
  }

  Tile* tile = it->second;
  ManagedTileState& mts = tile->managed_state();
  ManagedTileState::TileVersion& tile_version = mts.tile_versions[raster_mode];
  DCHECK(tile_version.raster_task_);
  orphan_raster_tasks_.push_back(tile_version.raster_task_);
  tile_version.raster_task_ = NULL;

  if (was_canceled) {
    ++update_visible_tiles_stats_.canceled_count;
    resource_pool_->ReleaseResource(resource.Pass());
    return;
  }

  ++update_visible_tiles_stats_.completed_count;

  if (analysis.is_solid_color) {
    tile_version.set_solid_color(analysis.solid_color);
    resource_pool_->ReleaseResource(resource.Pass());
  } else {
    tile_version.set_use_resource();
    tile_version.resource_ = resource.Pass();

    bytes_releasable_ += BytesConsumedIfAllocated(tile);
    ++resources_releasable_;
  }

  FreeUnusedResourcesForTile(tile);
  if (tile->priority(ACTIVE_TREE).distance_to_visible == 0.f)
    did_initialize_visible_tile_ = true;

  client_->NotifyTileStateChanged(tile);
}

scoped_refptr<Tile> TileManager::CreateTile(PicturePileImpl* picture_pile,
                                            const gfx::Size& tile_size,
                                            const gfx::Rect& content_rect,
                                            const gfx::Rect& opaque_rect,
                                            float contents_scale,
                                            int layer_id,
                                            int source_frame_number,
                                            int flags) {
  scoped_refptr<Tile> tile = make_scoped_refptr(new Tile(this,
                                                         picture_pile,
                                                         tile_size,
                                                         content_rect,
                                                         opaque_rect,
                                                         contents_scale,
                                                         layer_id,
                                                         source_frame_number,
                                                         flags));
  DCHECK(tiles_.find(tile->id()) == tiles_.end());

  tiles_[tile->id()] = tile;
  used_layer_counts_[tile->layer_id()]++;
  prioritized_tiles_dirty_ = true;
  return tile;
}

void TileManager::GetPairedPictureLayers(
    std::vector<PairedPictureLayer>* paired_layers) const {
  const std::vector<PictureLayerImpl*>& layers = client_->GetPictureLayers();

  paired_layers->clear();
  // Reserve a maximum possible paired layers.
  paired_layers->reserve(layers.size());

  for (std::vector<PictureLayerImpl*>::const_iterator it = layers.begin();
       it != layers.end();
       ++it) {
    PictureLayerImpl* layer = *it;

    // TODO(vmpstr): Iterators and should handle this instead. crbug.com/381704
    if (!layer->HasValidTilePriorities())
      continue;

    PictureLayerImpl* twin_layer = layer->GetTwinLayer();

    // Ignore the twin layer when tile priorities are invalid.
    // TODO(vmpstr): Iterators should handle this instead. crbug.com/381704
    if (twin_layer && !twin_layer->HasValidTilePriorities())
      twin_layer = NULL;

    PairedPictureLayer paired_layer;
    WhichTree tree = layer->GetTree();

    // If the current tree is ACTIVE_TREE, then always generate a paired_layer.
    // If current tree is PENDING_TREE, then only generate a paired_layer if
    // there is no twin layer.
    if (tree == ACTIVE_TREE) {
      DCHECK(!twin_layer || twin_layer->GetTree() == PENDING_TREE);
      paired_layer.active_layer = layer;
      paired_layer.pending_layer = twin_layer;
      paired_layers->push_back(paired_layer);
    } else if (!twin_layer) {
      paired_layer.active_layer = NULL;
      paired_layer.pending_layer = layer;
      paired_layers->push_back(paired_layer);
    }
  }
}

TileManager::PairedPictureLayer::PairedPictureLayer()
    : active_layer(NULL), pending_layer(NULL) {}

TileManager::PairedPictureLayer::~PairedPictureLayer() {}

TileManager::RasterTileIterator::RasterTileIterator(TileManager* tile_manager,
                                                    TreePriority tree_priority)
    : tree_priority_(tree_priority), comparator_(tree_priority) {
  std::vector<TileManager::PairedPictureLayer> paired_layers;
  tile_manager->GetPairedPictureLayers(&paired_layers);
  bool prioritize_low_res = tree_priority_ == SMOOTHNESS_TAKES_PRIORITY;

  paired_iterators_.reserve(paired_layers.size());
  iterator_heap_.reserve(paired_layers.size());
  for (std::vector<TileManager::PairedPictureLayer>::iterator it =
           paired_layers.begin();
       it != paired_layers.end();
       ++it) {
    PairedPictureLayerIterator paired_iterator;
    if (it->active_layer) {
      paired_iterator.active_iterator =
          PictureLayerImpl::LayerRasterTileIterator(it->active_layer,
                                                    prioritize_low_res);
    }

    if (it->pending_layer) {
      paired_iterator.pending_iterator =
          PictureLayerImpl::LayerRasterTileIterator(it->pending_layer,
                                                    prioritize_low_res);
    }

    if (paired_iterator.PeekTile(tree_priority_) != NULL) {
      paired_iterators_.push_back(paired_iterator);
      iterator_heap_.push_back(&paired_iterators_.back());
    }
  }

  std::make_heap(iterator_heap_.begin(), iterator_heap_.end(), comparator_);
}

TileManager::RasterTileIterator::~RasterTileIterator() {}

TileManager::RasterTileIterator& TileManager::RasterTileIterator::operator++() {
  DCHECK(*this);

  std::pop_heap(iterator_heap_.begin(), iterator_heap_.end(), comparator_);
  PairedPictureLayerIterator* paired_iterator = iterator_heap_.back();
  iterator_heap_.pop_back();

  paired_iterator->PopTile(tree_priority_);
  if (paired_iterator->PeekTile(tree_priority_) != NULL) {
    iterator_heap_.push_back(paired_iterator);
    std::push_heap(iterator_heap_.begin(), iterator_heap_.end(), comparator_);
  }
  return *this;
}

TileManager::RasterTileIterator::operator bool() const {
  return !iterator_heap_.empty();
}

Tile* TileManager::RasterTileIterator::operator*() {
  DCHECK(*this);
  return iterator_heap_.front()->PeekTile(tree_priority_);
}

TileManager::RasterTileIterator::PairedPictureLayerIterator::
    PairedPictureLayerIterator() {}

TileManager::RasterTileIterator::PairedPictureLayerIterator::
    ~PairedPictureLayerIterator() {}

Tile* TileManager::RasterTileIterator::PairedPictureLayerIterator::PeekTile(
    TreePriority tree_priority) {
  PictureLayerImpl::LayerRasterTileIterator* next_iterator =
      NextTileIterator(tree_priority).first;
  if (!next_iterator)
    return NULL;

  DCHECK(*next_iterator);
  DCHECK(std::find(returned_shared_tiles.begin(),
                   returned_shared_tiles.end(),
                   **next_iterator) == returned_shared_tiles.end());
  return **next_iterator;
}

void TileManager::RasterTileIterator::PairedPictureLayerIterator::PopTile(
    TreePriority tree_priority) {
  PictureLayerImpl::LayerRasterTileIterator* next_iterator =
      NextTileIterator(tree_priority).first;
  DCHECK(next_iterator);
  DCHECK(*next_iterator);
  returned_shared_tiles.push_back(**next_iterator);
  ++(*next_iterator);

  next_iterator = NextTileIterator(tree_priority).first;
  while (next_iterator &&
         std::find(returned_shared_tiles.begin(),
                   returned_shared_tiles.end(),
                   **next_iterator) != returned_shared_tiles.end()) {
    ++(*next_iterator);
    next_iterator = NextTileIterator(tree_priority).first;
  }
}

std::pair<PictureLayerImpl::LayerRasterTileIterator*, WhichTree>
TileManager::RasterTileIterator::PairedPictureLayerIterator::NextTileIterator(
    TreePriority tree_priority) {
  // If both iterators are out of tiles, return NULL.
  if (!active_iterator && !pending_iterator) {
    return std::pair<PictureLayerImpl::LayerRasterTileIterator*, WhichTree>(
        NULL, ACTIVE_TREE);
  }

  // If we only have one iterator with tiles, return it.
  if (!active_iterator)
    return std::make_pair(&pending_iterator, PENDING_TREE);
  if (!pending_iterator)
    return std::make_pair(&active_iterator, ACTIVE_TREE);

  // Now both iterators have tiles, so we have to decide based on tree priority.
  switch (tree_priority) {
    case SMOOTHNESS_TAKES_PRIORITY:
      return std::make_pair(&active_iterator, ACTIVE_TREE);
    case NEW_CONTENT_TAKES_PRIORITY:
      return std::make_pair(&pending_iterator, ACTIVE_TREE);
    case SAME_PRIORITY_FOR_BOTH_TREES: {
      Tile* active_tile = *active_iterator;
      Tile* pending_tile = *pending_iterator;
      if (active_tile == pending_tile)
        return std::make_pair(&active_iterator, ACTIVE_TREE);

      const TilePriority& active_priority = active_tile->priority(ACTIVE_TREE);
      const TilePriority& pending_priority =
          pending_tile->priority(PENDING_TREE);

      if (active_priority.IsHigherPriorityThan(pending_priority))
        return std::make_pair(&active_iterator, ACTIVE_TREE);
      return std::make_pair(&pending_iterator, PENDING_TREE);
    }
  }

  NOTREACHED();
  // Keep the compiler happy.
  return std::pair<PictureLayerImpl::LayerRasterTileIterator*, WhichTree>(
      NULL, ACTIVE_TREE);
}

TileManager::RasterTileIterator::RasterOrderComparator::RasterOrderComparator(
    TreePriority tree_priority)
    : tree_priority_(tree_priority) {}

bool TileManager::RasterTileIterator::RasterOrderComparator::operator()(
    PairedPictureLayerIterator* a,
    PairedPictureLayerIterator* b) const {
  std::pair<PictureLayerImpl::LayerRasterTileIterator*, WhichTree> a_pair =
      a->NextTileIterator(tree_priority_);
  DCHECK(a_pair.first);
  DCHECK(*a_pair.first);

  std::pair<PictureLayerImpl::LayerRasterTileIterator*, WhichTree> b_pair =
      b->NextTileIterator(tree_priority_);
  DCHECK(b_pair.first);
  DCHECK(*b_pair.first);

  Tile* a_tile = **a_pair.first;
  Tile* b_tile = **b_pair.first;

  const TilePriority& a_priority =
      a_tile->priority_for_tree_priority(tree_priority_);
  const TilePriority& b_priority =
      b_tile->priority_for_tree_priority(tree_priority_);
  bool prioritize_low_res = tree_priority_ == SMOOTHNESS_TAKES_PRIORITY;

  // Now we have to return true iff b is higher priority than a.

  // If the bin is the same but the resolution is not, then the order will be
  // determined by whether we prioritize low res or not.
  // TODO(vmpstr): Remove this when TilePriority is no longer a member of Tile
  // class but instead produced by the iterators.
  if (b_priority.priority_bin == a_priority.priority_bin &&
      b_priority.resolution != a_priority.resolution) {
    // Non ideal resolution should be sorted lower than other resolutions.
    if (a_priority.resolution == NON_IDEAL_RESOLUTION)
      return true;

    if (b_priority.resolution == NON_IDEAL_RESOLUTION)
      return false;

    if (prioritize_low_res)
      return b_priority.resolution == LOW_RESOLUTION;

    return b_priority.resolution == HIGH_RESOLUTION;
  }

  return b_priority.IsHigherPriorityThan(a_priority);
}

TileManager::EvictionTileIterator::EvictionTileIterator()
    : comparator_(SAME_PRIORITY_FOR_BOTH_TREES) {}

TileManager::EvictionTileIterator::EvictionTileIterator(
    TileManager* tile_manager,
    TreePriority tree_priority)
    : tree_priority_(tree_priority), comparator_(tree_priority) {
  std::vector<TileManager::PairedPictureLayer> paired_layers;

  tile_manager->GetPairedPictureLayers(&paired_layers);

  paired_iterators_.reserve(paired_layers.size());
  iterator_heap_.reserve(paired_layers.size());
  for (std::vector<TileManager::PairedPictureLayer>::iterator it =
           paired_layers.begin();
       it != paired_layers.end();
       ++it) {
    PairedPictureLayerIterator paired_iterator;
    if (it->active_layer) {
      paired_iterator.active_iterator =
          PictureLayerImpl::LayerEvictionTileIterator(it->active_layer,
                                                      tree_priority_);
    }

    if (it->pending_layer) {
      paired_iterator.pending_iterator =
          PictureLayerImpl::LayerEvictionTileIterator(it->pending_layer,
                                                      tree_priority_);
    }

    if (paired_iterator.PeekTile(tree_priority_) != NULL) {
      paired_iterators_.push_back(paired_iterator);
      iterator_heap_.push_back(&paired_iterators_.back());
    }
  }

  std::make_heap(iterator_heap_.begin(), iterator_heap_.end(), comparator_);
}

TileManager::EvictionTileIterator::~EvictionTileIterator() {}

TileManager::EvictionTileIterator& TileManager::EvictionTileIterator::
operator++() {
  std::pop_heap(iterator_heap_.begin(), iterator_heap_.end(), comparator_);
  PairedPictureLayerIterator* paired_iterator = iterator_heap_.back();
  iterator_heap_.pop_back();

  paired_iterator->PopTile(tree_priority_);
  if (paired_iterator->PeekTile(tree_priority_) != NULL) {
    iterator_heap_.push_back(paired_iterator);
    std::push_heap(iterator_heap_.begin(), iterator_heap_.end(), comparator_);
  }
  return *this;
}

TileManager::EvictionTileIterator::operator bool() const {
  return !iterator_heap_.empty();
}

Tile* TileManager::EvictionTileIterator::operator*() {
  DCHECK(*this);
  return iterator_heap_.front()->PeekTile(tree_priority_);
}

TileManager::EvictionTileIterator::PairedPictureLayerIterator::
    PairedPictureLayerIterator() {}

TileManager::EvictionTileIterator::PairedPictureLayerIterator::
    ~PairedPictureLayerIterator() {}

Tile* TileManager::EvictionTileIterator::PairedPictureLayerIterator::PeekTile(
    TreePriority tree_priority) {
  PictureLayerImpl::LayerEvictionTileIterator* next_iterator =
      NextTileIterator(tree_priority);
  if (!next_iterator)
    return NULL;

  DCHECK(*next_iterator);
  DCHECK(std::find(returned_shared_tiles.begin(),
                   returned_shared_tiles.end(),
                   **next_iterator) == returned_shared_tiles.end());
  return **next_iterator;
}

void TileManager::EvictionTileIterator::PairedPictureLayerIterator::PopTile(
    TreePriority tree_priority) {
  PictureLayerImpl::LayerEvictionTileIterator* next_iterator =
      NextTileIterator(tree_priority);
  DCHECK(next_iterator);
  DCHECK(*next_iterator);
  returned_shared_tiles.push_back(**next_iterator);
  ++(*next_iterator);

  next_iterator = NextTileIterator(tree_priority);
  while (next_iterator &&
         std::find(returned_shared_tiles.begin(),
                   returned_shared_tiles.end(),
                   **next_iterator) != returned_shared_tiles.end()) {
    ++(*next_iterator);
    next_iterator = NextTileIterator(tree_priority);
  }
}

PictureLayerImpl::LayerEvictionTileIterator*
TileManager::EvictionTileIterator::PairedPictureLayerIterator::NextTileIterator(
    TreePriority tree_priority) {
  // If both iterators are out of tiles, return NULL.
  if (!active_iterator && !pending_iterator)
    return NULL;

  // If we only have one iterator with tiles, return it.
  if (!active_iterator)
    return &pending_iterator;
  if (!pending_iterator)
    return &active_iterator;

  Tile* active_tile = *active_iterator;
  Tile* pending_tile = *pending_iterator;
  if (active_tile == pending_tile)
    return &active_iterator;

  const TilePriority& active_priority =
      active_tile->priority_for_tree_priority(tree_priority);
  const TilePriority& pending_priority =
      pending_tile->priority_for_tree_priority(tree_priority);

  if (pending_priority.IsHigherPriorityThan(active_priority))
    return &active_iterator;
  return &pending_iterator;
}

TileManager::EvictionTileIterator::EvictionOrderComparator::
    EvictionOrderComparator(TreePriority tree_priority)
    : tree_priority_(tree_priority) {}

bool TileManager::EvictionTileIterator::EvictionOrderComparator::operator()(
    PairedPictureLayerIterator* a,
    PairedPictureLayerIterator* b) const {
  PictureLayerImpl::LayerEvictionTileIterator* a_iterator =
      a->NextTileIterator(tree_priority_);
  DCHECK(a_iterator);
  DCHECK(*a_iterator);

  PictureLayerImpl::LayerEvictionTileIterator* b_iterator =
      b->NextTileIterator(tree_priority_);
  DCHECK(b_iterator);
  DCHECK(*b_iterator);

  Tile* a_tile = **a_iterator;
  Tile* b_tile = **b_iterator;

  const TilePriority& a_priority =
      a_tile->priority_for_tree_priority(tree_priority_);
  const TilePriority& b_priority =
      b_tile->priority_for_tree_priority(tree_priority_);
  bool prioritize_low_res = tree_priority_ == SMOOTHNESS_TAKES_PRIORITY;

  // Now we have to return true iff b is lower priority than a.

  // If the bin is the same but the resolution is not, then the order will be
  // determined by whether we prioritize low res or not.
  // TODO(vmpstr): Remove this when TilePriority is no longer a member of Tile
  // class but instead produced by the iterators.
  if (b_priority.priority_bin == a_priority.priority_bin &&
      b_priority.resolution != a_priority.resolution) {
    // Non ideal resolution should be sorted higher than other resolutions.
    if (a_priority.resolution == NON_IDEAL_RESOLUTION)
      return false;

    if (b_priority.resolution == NON_IDEAL_RESOLUTION)
      return true;

    if (prioritize_low_res)
      return a_priority.resolution == LOW_RESOLUTION;

    return a_priority.resolution == HIGH_RESOLUTION;
  }
  return a_priority.IsHigherPriorityThan(b_priority);
}

void TileManager::SetRasterizerForTesting(Rasterizer* rasterizer) {
  rasterizer_ = rasterizer;
  rasterizer_->SetClient(this);
}

bool TileManager::IsReadyToActivate() const {
  const std::vector<PictureLayerImpl*>& layers = client_->GetPictureLayers();

  for (std::vector<PictureLayerImpl*>::const_iterator it = layers.begin();
       it != layers.end();
       ++it) {
    if (!(*it)->AllTilesRequiredForActivationAreReadyToDraw())
      return false;
  }

  return true;
}

void TileManager::CheckIfReadyToActivate() {
  TRACE_EVENT0("cc", "TileManager::CheckIfReadyToActivate");

  rasterizer_->CheckForCompletedTasks();
  did_check_for_completed_tasks_since_last_schedule_tasks_ = true;

  if (IsReadyToActivate())
    client_->NotifyReadyToActivate();
}

}  // namespace cc
