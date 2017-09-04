// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/tile_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <string>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/base/histograms.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/debug/frame_viewer_instrumentation.h"
#include "cc/debug/traced_value.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/raster/raster_buffer.h"
#include "cc/raster/task_category.h"
#include "cc/tiles/tile.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace cc {
namespace {

// Flag to indicate whether we should try and detect that
// a tile is of solid color.
const bool kUseColorEstimator = true;

// TODO(enne): remove this histogram and its monitoring in M58 once there is
// enough new data from the other two raster task timers.
DEFINE_SCOPED_UMA_HISTOGRAM_AREA_TIMER(
    ScopedGeneralRasterTaskTimer,
    "Compositing.%s.RasterTask.RasterUs",
    "Compositing.%s.RasterTask.RasterPixelsPerMs");

DEFINE_SCOPED_UMA_HISTOGRAM_AREA_TIMER(
    ScopedSoftwareRasterTaskTimer,
    "Compositing.%s.RasterTask.RasterUs.Software",
    "Compositing.%s.RasterTask.RasterPixelsPerMs.Software");

DEFINE_SCOPED_UMA_HISTOGRAM_AREA_TIMER(
    ScopedGpuRasterTaskTimer,
    "Compositing.%s.RasterTask.RasterUs.Gpu",
    "Compositing.%s.RasterTask.RasterPixelsPerMs.Gpu");

class ScopedRasterTaskTimer {
 public:
  explicit ScopedRasterTaskTimer(bool use_gpu_rasterization) {
    if (use_gpu_rasterization)
      gpu_timer_.emplace();
    else
      software_timer_.emplace();
  }

  void SetArea(int area) {
    general_timer_.SetArea(area);
    if (software_timer_)
      software_timer_->SetArea(area);
    if (gpu_timer_)
      gpu_timer_->SetArea(area);
  }

 private:
  ScopedGeneralRasterTaskTimer general_timer_;
  base::Optional<ScopedSoftwareRasterTaskTimer> software_timer_;
  base::Optional<ScopedGpuRasterTaskTimer> gpu_timer_;
};

class RasterTaskImpl : public TileTask {
 public:
  RasterTaskImpl(TileManager* tile_manager,
                 Tile* tile,
                 Resource* resource,
                 scoped_refptr<RasterSource> raster_source,
                 const RasterSource::PlaybackSettings& playback_settings,
                 TileResolution tile_resolution,
                 gfx::Rect invalidated_rect,
                 uint64_t source_prepare_tiles_id,
                 std::unique_ptr<RasterBuffer> raster_buffer,
                 TileTask::Vector* dependencies,
                 bool is_gpu_rasterization)
      : TileTask(!is_gpu_rasterization, dependencies),
        tile_manager_(tile_manager),
        tile_(tile),
        resource_(resource),
        raster_source_(std::move(raster_source)),
        content_rect_(tile->content_rect()),
        invalid_content_rect_(invalidated_rect),
        raster_scales_(tile->raster_scales()),
        playback_settings_(playback_settings),
        tile_resolution_(tile_resolution),
        layer_id_(tile->layer_id()),
        source_prepare_tiles_id_(source_prepare_tiles_id),
        tile_tracing_id_(static_cast<void*>(tile)),
        new_content_id_(tile->id()),
        source_frame_number_(tile->source_frame_number()),
        is_gpu_rasterization_(is_gpu_rasterization),
        raster_buffer_(std::move(raster_buffer)) {
    DCHECK(origin_thread_checker_.CalledOnValidThread());
  }

  // Overridden from Task:
  void RunOnWorkerThread() override {
    TRACE_EVENT1("cc", "RasterizerTaskImpl::RunOnWorkerThread",
                 "source_prepare_tiles_id", source_prepare_tiles_id_);

    DCHECK(raster_source_.get());
    DCHECK(raster_buffer_);

    frame_viewer_instrumentation::ScopedRasterTask raster_task(
        tile_tracing_id_, tile_resolution_, source_frame_number_, layer_id_);
    ScopedRasterTaskTimer timer(is_gpu_rasterization_);
    timer.SetArea(content_rect_.size().GetArea());

    DCHECK(raster_source_);

    raster_buffer_->Playback(raster_source_.get(), content_rect_,
                             invalid_content_rect_, new_content_id_,
                             raster_scales_, playback_settings_);
  }

  // Overridden from TileTask:
  void OnTaskCompleted() override {
    DCHECK(origin_thread_checker_.CalledOnValidThread());

    // Here calling state().IsCanceled() is thread-safe, because this task is
    // already concluded as FINISHED or CANCELLED and no longer will be worked
    // upon by task graph runner.
    tile_manager_->OnRasterTaskCompleted(std::move(raster_buffer_), tile_,
                                         resource_, state().IsCanceled());
  }

 protected:
  ~RasterTaskImpl() override {
    DCHECK(origin_thread_checker_.CalledOnValidThread());
    DCHECK(!raster_buffer_);
  }

 private:
  base::ThreadChecker origin_thread_checker_;

  // The following members are needed for processing completion of this task on
  // origin thread. These are not thread-safe and should be accessed only in
  // origin thread. Ensure their access by checking CalledOnValidThread().
  TileManager* tile_manager_;
  Tile* tile_;
  Resource* resource_;

  // The following members should be used for running the task.
  scoped_refptr<RasterSource> raster_source_;
  gfx::Rect content_rect_;
  gfx::Rect invalid_content_rect_;
  gfx::SizeF raster_scales_;
  RasterSource::PlaybackSettings playback_settings_;
  TileResolution tile_resolution_;
  int layer_id_;
  uint64_t source_prepare_tiles_id_;
  void* tile_tracing_id_;
  uint64_t new_content_id_;
  int source_frame_number_;
  bool is_gpu_rasterization_;
  std::unique_ptr<RasterBuffer> raster_buffer_;

  DISALLOW_COPY_AND_ASSIGN(RasterTaskImpl);
};

TaskCategory TaskCategoryForTileTask(TileTask* task,
                                     bool use_foreground_category) {
  if (!task->supports_concurrent_execution())
    return TASK_CATEGORY_NONCONCURRENT_FOREGROUND;

  if (use_foreground_category)
    return TASK_CATEGORY_FOREGROUND;

  return TASK_CATEGORY_BACKGROUND;
}

bool IsForegroundCategory(uint16_t category) {
  TaskCategory enum_category = static_cast<TaskCategory>(category);
  switch (enum_category) {
    case TASK_CATEGORY_NONCONCURRENT_FOREGROUND:
    case TASK_CATEGORY_FOREGROUND:
      return true;
    case TASK_CATEGORY_BACKGROUND:
      return false;
  }

  DCHECK(false);
  return false;
}

// Task priorities that make sure that the task set done tasks run before any
// other remaining tasks.
const size_t kRequiredForActivationDoneTaskPriority = 1u;
const size_t kRequiredForDrawDoneTaskPriority = 2u;
const size_t kAllDoneTaskPriority = 3u;

// For correctness, |kTileTaskPriorityBase| must be greater than
// all task set done task priorities.
size_t kTileTaskPriorityBase = 10u;

void InsertNodeForTask(TaskGraph* graph,
                       TileTask* task,
                       uint16_t category,
                       uint16_t priority,
                       size_t dependencies) {
  DCHECK(std::find_if(graph->nodes.begin(), graph->nodes.end(),
                      [&task](const TaskGraph::Node& node) {
                        return node.task == task;
                      }) == graph->nodes.end());
  graph->nodes.emplace_back(task, category, priority, dependencies);
}

void InsertNodeForDecodeTask(TaskGraph* graph,
                             TileTask* task,
                             bool use_foreground_category,
                             uint16_t priority) {
  uint32_t dependency_count = 0u;
  if (task->dependencies().size()) {
    DCHECK_EQ(task->dependencies().size(), 1u);
    auto* dependency = task->dependencies()[0].get();
    if (!dependency->HasCompleted()) {
      InsertNodeForDecodeTask(graph, dependency, use_foreground_category,
                              priority);
      graph->edges.push_back(TaskGraph::Edge(dependency, task));
      dependency_count = 1u;
    }
  }
  InsertNodeForTask(graph, task,
                    TaskCategoryForTileTask(task, use_foreground_category),
                    priority, dependency_count);
}

void InsertNodesForRasterTask(TaskGraph* graph,
                              TileTask* raster_task,
                              const TileTask::Vector& decode_tasks,
                              size_t priority,
                              bool use_foreground_category) {
  size_t dependencies = 0u;

  // Insert image decode tasks.
  for (TileTask::Vector::const_iterator it = decode_tasks.begin();
       it != decode_tasks.end(); ++it) {
    TileTask* decode_task = it->get();

    // Skip if already decoded.
    if (decode_task->HasCompleted())
      continue;

    dependencies++;

    // Add decode task if it doesn't already exist in graph.
    TaskGraph::Node::Vector::iterator decode_it =
        std::find_if(graph->nodes.begin(), graph->nodes.end(),
                     [decode_task](const TaskGraph::Node& node) {
                       return node.task == decode_task;
                     });

    // In rare circumstances, a background category task may come in before a
    // foreground category task. In these cases, upgrade any background category
    // dependencies of the current task.
    // TODO(ericrk): Task iterators should be updated to avoid this.
    // crbug.com/594851
    // TODO(ericrk): This should handle dependencies recursively.
    // crbug.com/605234
    if (decode_it != graph->nodes.end() && use_foreground_category &&
        !IsForegroundCategory(decode_it->category)) {
      decode_it->category = TASK_CATEGORY_FOREGROUND;
    }

    if (decode_it == graph->nodes.end()) {
      InsertNodeForDecodeTask(graph, decode_task, use_foreground_category,
                              priority);
    }

    graph->edges.push_back(TaskGraph::Edge(decode_task, raster_task));
  }

  InsertNodeForTask(
      graph, raster_task,
      TaskCategoryForTileTask(raster_task, use_foreground_category), priority,
      dependencies);
}

class TaskSetFinishedTaskImpl : public TileTask {
 public:
  explicit TaskSetFinishedTaskImpl(
      base::SequencedTaskRunner* task_runner,
      const base::Closure& on_task_set_finished_callback)
      : TileTask(true),
        task_runner_(task_runner),
        on_task_set_finished_callback_(on_task_set_finished_callback) {}

  // Overridden from Task:
  void RunOnWorkerThread() override {
    TRACE_EVENT0("cc", "TaskSetFinishedTaskImpl::RunOnWorkerThread");
    TaskSetFinished();
  }

  // Overridden from TileTask:
  void OnTaskCompleted() override {}

 protected:
  ~TaskSetFinishedTaskImpl() override {}

  void TaskSetFinished() {
    task_runner_->PostTask(FROM_HERE, on_task_set_finished_callback_);
  }

 private:
  base::SequencedTaskRunner* task_runner_;
  const base::Closure on_task_set_finished_callback_;

  DISALLOW_COPY_AND_ASSIGN(TaskSetFinishedTaskImpl);
};

}  // namespace

RasterTaskCompletionStats::RasterTaskCompletionStats()
    : completed_count(0u), canceled_count(0u) {}

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
RasterTaskCompletionStatsAsValue(const RasterTaskCompletionStats& stats) {
  std::unique_ptr<base::trace_event::TracedValue> state(
      new base::trace_event::TracedValue());
  state->SetInteger("completed_count",
                    base::saturated_cast<int>(stats.completed_count));
  state->SetInteger("canceled_count",
                    base::saturated_cast<int>(stats.canceled_count));
  return std::move(state);
}

TileManager::TileManager(TileManagerClient* client,
                         base::SequencedTaskRunner* task_runner,
                         size_t scheduled_raster_task_limit,
                         bool use_partial_raster)
    : client_(client),
      task_runner_(task_runner),
      resource_pool_(nullptr),
      tile_task_manager_(nullptr),
      scheduled_raster_task_limit_(scheduled_raster_task_limit),
      use_partial_raster_(use_partial_raster),
      use_gpu_rasterization_(false),
      all_tiles_that_need_to_be_rasterized_are_scheduled_(true),
      did_check_for_completed_tasks_since_last_schedule_tasks_(true),
      did_oom_on_last_assign_(false),
      more_tiles_need_prepare_check_notifier_(
          task_runner_,
          base::Bind(&TileManager::CheckIfMoreTilesNeedToBePrepared,
                     base::Unretained(this))),
      signals_check_notifier_(task_runner_,
                              base::Bind(&TileManager::CheckAndIssueSignals,
                                         base::Unretained(this))),
      has_scheduled_tile_tasks_(false),
      prepare_tiles_count_(0u),
      next_tile_id_(0u),
      task_set_finished_weak_ptr_factory_(this) {}

TileManager::~TileManager() {
  FinishTasksAndCleanUp();
}

void TileManager::FinishTasksAndCleanUp() {
  if (!tile_task_manager_)
    return;

  global_state_ = GlobalStateThatImpactsTilePriority();

  // This cancels tasks if possible, finishes pending tasks, and release any
  // uninitialized resources.
  tile_task_manager_->Shutdown();

  raster_buffer_provider_->Shutdown();

  tile_task_manager_->CheckForCompletedTasks();

  // Now that all tasks have been finished, we can clear any |orphan_tasks_|.
  orphan_tasks_.clear();

  FreeResourcesForReleasedTiles();
  CleanUpReleasedTiles();

  tile_task_manager_ = nullptr;
  resource_pool_ = nullptr;
  more_tiles_need_prepare_check_notifier_.Cancel();
  signals_check_notifier_.Cancel();
  task_set_finished_weak_ptr_factory_.InvalidateWeakPtrs();

  image_manager_.SetImageDecodeController(nullptr);
  locked_image_tasks_.clear();
}

void TileManager::SetResources(ResourcePool* resource_pool,
                               ImageDecodeController* image_decode_controller,
                               TaskGraphRunner* task_graph_runner,
                               RasterBufferProvider* raster_buffer_provider,
                               size_t scheduled_raster_task_limit,
                               bool use_gpu_rasterization) {
  DCHECK(!tile_task_manager_);
  DCHECK(task_graph_runner);

  use_gpu_rasterization_ = use_gpu_rasterization;
  scheduled_raster_task_limit_ = scheduled_raster_task_limit;
  resource_pool_ = resource_pool;
  image_manager_.SetImageDecodeController(image_decode_controller);
  tile_task_manager_ = TileTaskManagerImpl::Create(task_graph_runner);
  raster_buffer_provider_ = raster_buffer_provider;
}

void TileManager::Release(Tile* tile) {
  released_tiles_.push_back(tile);
}

void TileManager::FreeResourcesForReleasedTiles() {
  for (auto* tile : released_tiles_)
    FreeResourcesForTile(tile);
}

void TileManager::CleanUpReleasedTiles() {
  std::vector<Tile*> tiles_to_retain;
  for (auto* tile : released_tiles_) {
    if (tile->HasRasterTask()) {
      tiles_to_retain.push_back(tile);
      continue;
    }

    DCHECK(!tile->draw_info().has_resource());
    DCHECK(tiles_.find(tile->id()) != tiles_.end());
    tiles_.erase(tile->id());

    delete tile;
  }
  released_tiles_.swap(tiles_to_retain);
}

void TileManager::DidFinishRunningTileTasksRequiredForActivation() {
  TRACE_EVENT0("cc",
               "TileManager::DidFinishRunningTileTasksRequiredForActivation");
  TRACE_EVENT_ASYNC_STEP_INTO1("cc", "ScheduledTasks", this, "running", "state",
                               ScheduledTasksStateAsValue());
  // TODO(vmpstr): Temporary check to debug crbug.com/642927.
  CHECK(tile_task_manager_);
  signals_.ready_to_activate = true;
  signals_check_notifier_.Schedule();
}

void TileManager::DidFinishRunningTileTasksRequiredForDraw() {
  TRACE_EVENT0("cc", "TileManager::DidFinishRunningTileTasksRequiredForDraw");
  TRACE_EVENT_ASYNC_STEP_INTO1("cc", "ScheduledTasks", this, "running", "state",
                               ScheduledTasksStateAsValue());
  // TODO(vmpstr): Temporary check to debug crbug.com/642927.
  CHECK(tile_task_manager_);
  signals_.ready_to_draw = true;
  signals_check_notifier_.Schedule();
}

void TileManager::DidFinishRunningAllTileTasks() {
  TRACE_EVENT0("cc", "TileManager::DidFinishRunningAllTileTasks");
  TRACE_EVENT_ASYNC_END0("cc", "ScheduledTasks", this);
  DCHECK(resource_pool_);
  DCHECK(tile_task_manager_);

  has_scheduled_tile_tasks_ = false;

  if (all_tiles_that_need_to_be_rasterized_are_scheduled_ &&
      !resource_pool_->ResourceUsageTooHigh()) {
    // TODO(ericrk): We should find a better way to safely handle re-entrant
    // notifications than always having to schedule a new task.
    // http://crbug.com/498439
    // TODO(vmpstr): Temporary check to debug crbug.com/642927.
    CHECK(tile_task_manager_);
    signals_.all_tile_tasks_completed = true;
    signals_check_notifier_.Schedule();
    return;
  }

  more_tiles_need_prepare_check_notifier_.Schedule();
}

bool TileManager::PrepareTiles(
    const GlobalStateThatImpactsTilePriority& state) {
  ++prepare_tiles_count_;

  TRACE_EVENT1("cc", "TileManager::PrepareTiles", "prepare_tiles_id",
               prepare_tiles_count_);

  if (!tile_task_manager_) {
    TRACE_EVENT_INSTANT0("cc", "PrepareTiles aborted",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  signals_.reset();
  global_state_ = state;

  // We need to call CheckForCompletedTasks() once in-between each call
  // to ScheduleTasks() to prevent canceled tasks from being scheduled.
  if (!did_check_for_completed_tasks_since_last_schedule_tasks_) {
    tile_task_manager_->CheckForCompletedTasks();
    did_check_for_completed_tasks_since_last_schedule_tasks_ = true;
  }

  FreeResourcesForReleasedTiles();
  CleanUpReleasedTiles();

  PrioritizedWorkToSchedule prioritized_work = AssignGpuMemoryToTiles();

  // Inform the client that will likely require a draw if the highest priority
  // tile that will be rasterized is required for draw.
  client_->SetIsLikelyToRequireADraw(
      !prioritized_work.tiles_to_raster.empty() &&
      prioritized_work.tiles_to_raster.front().tile()->required_for_draw());

  // Schedule tile tasks.
  ScheduleTasks(prioritized_work);

  TRACE_EVENT_INSTANT1("cc", "DidPrepareTiles", TRACE_EVENT_SCOPE_THREAD,
                       "state", BasicStateAsValue());
  return true;
}

void TileManager::Flush() {
  TRACE_EVENT0("cc", "TileManager::Flush");

  if (!tile_task_manager_) {
    TRACE_EVENT_INSTANT0("cc", "Flush aborted", TRACE_EVENT_SCOPE_THREAD);
    return;
  }

  tile_task_manager_->CheckForCompletedTasks();
  did_check_for_completed_tasks_since_last_schedule_tasks_ = true;

  TRACE_EVENT_INSTANT1("cc", "DidFlush", TRACE_EVENT_SCOPE_THREAD, "stats",
                       RasterTaskCompletionStatsAsValue(flush_stats_));
  flush_stats_ = RasterTaskCompletionStats();
}

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
TileManager::BasicStateAsValue() const {
  std::unique_ptr<base::trace_event::TracedValue> value(
      new base::trace_event::TracedValue());
  BasicStateAsValueInto(value.get());
  return std::move(value);
}

void TileManager::BasicStateAsValueInto(
    base::trace_event::TracedValue* state) const {
  state->SetInteger("tile_count", base::saturated_cast<int>(tiles_.size()));
  state->SetBoolean("did_oom_on_last_assign", did_oom_on_last_assign_);
  state->BeginDictionary("global_state");
  global_state_.AsValueInto(state);
  state->EndDictionary();
}

std::unique_ptr<EvictionTilePriorityQueue>
TileManager::FreeTileResourcesUntilUsageIsWithinLimit(
    std::unique_ptr<EvictionTilePriorityQueue> eviction_priority_queue,
    const MemoryUsage& limit,
    MemoryUsage* usage) {
  while (usage->Exceeds(limit)) {
    if (!eviction_priority_queue) {
      eviction_priority_queue =
          client_->BuildEvictionQueue(global_state_.tree_priority);
    }
    if (eviction_priority_queue->IsEmpty())
      break;

    Tile* tile = eviction_priority_queue->Top().tile();
    *usage -= MemoryUsage::FromTile(tile);
    FreeResourcesForTileAndNotifyClientIfTileWasReadyToDraw(tile);
    eviction_priority_queue->Pop();
  }
  return eviction_priority_queue;
}

std::unique_ptr<EvictionTilePriorityQueue>
TileManager::FreeTileResourcesWithLowerPriorityUntilUsageIsWithinLimit(
    std::unique_ptr<EvictionTilePriorityQueue> eviction_priority_queue,
    const MemoryUsage& limit,
    const TilePriority& other_priority,
    MemoryUsage* usage) {
  while (usage->Exceeds(limit)) {
    if (!eviction_priority_queue) {
      eviction_priority_queue =
          client_->BuildEvictionQueue(global_state_.tree_priority);
    }
    if (eviction_priority_queue->IsEmpty())
      break;

    const PrioritizedTile& prioritized_tile = eviction_priority_queue->Top();
    if (!other_priority.IsHigherPriorityThan(prioritized_tile.priority()))
      break;

    Tile* tile = prioritized_tile.tile();
    *usage -= MemoryUsage::FromTile(tile);
    FreeResourcesForTileAndNotifyClientIfTileWasReadyToDraw(tile);
    eviction_priority_queue->Pop();
  }
  return eviction_priority_queue;
}

bool TileManager::TilePriorityViolatesMemoryPolicy(
    const TilePriority& priority) {
  switch (global_state_.memory_limit_policy) {
    case ALLOW_NOTHING:
      return true;
    case ALLOW_ABSOLUTE_MINIMUM:
      return priority.priority_bin > TilePriority::NOW;
    case ALLOW_PREPAINT_ONLY:
      return priority.priority_bin > TilePriority::SOON;
    case ALLOW_ANYTHING:
      return priority.distance_to_visible ==
             std::numeric_limits<float>::infinity();
  }
  NOTREACHED();
  return true;
}

TileManager::PrioritizedWorkToSchedule TileManager::AssignGpuMemoryToTiles() {
  TRACE_EVENT_BEGIN0("cc", "TileManager::AssignGpuMemoryToTiles");

  DCHECK(resource_pool_);
  DCHECK(tile_task_manager_);

  // Maintain the list of released resources that can potentially be re-used
  // or deleted. If this operation becomes expensive too, only do this after
  // some resource(s) was returned. Note that in that case, one also need to
  // invalidate when releasing some resource from the pool.
  resource_pool_->CheckBusyResources();

  // Now give memory out to the tiles until we're out, and build
  // the needs-to-be-rasterized queue.
  unsigned schedule_priority = 1u;
  all_tiles_that_need_to_be_rasterized_are_scheduled_ = true;
  bool had_enough_memory_to_schedule_tiles_needed_now = true;

  MemoryUsage hard_memory_limit(global_state_.hard_memory_limit_in_bytes,
                                global_state_.num_resources_limit);
  MemoryUsage soft_memory_limit(global_state_.soft_memory_limit_in_bytes,
                                global_state_.num_resources_limit);
  MemoryUsage memory_usage(resource_pool_->memory_usage_bytes(),
                           resource_pool_->resource_count());

  std::unique_ptr<RasterTilePriorityQueue> raster_priority_queue(
      client_->BuildRasterQueue(global_state_.tree_priority,
                                RasterTilePriorityQueue::Type::ALL));
  std::unique_ptr<EvictionTilePriorityQueue> eviction_priority_queue;
  PrioritizedWorkToSchedule work_to_schedule;
  for (; !raster_priority_queue->IsEmpty(); raster_priority_queue->Pop()) {
    const PrioritizedTile& prioritized_tile = raster_priority_queue->Top();
    Tile* tile = prioritized_tile.tile();
    TilePriority priority = prioritized_tile.priority();

    if (TilePriorityViolatesMemoryPolicy(priority)) {
      TRACE_EVENT_INSTANT0(
          "cc", "TileManager::AssignGpuMemory tile violates memory policy",
          TRACE_EVENT_SCOPE_THREAD);
      break;
    }

    bool tile_is_needed_now = priority.priority_bin == TilePriority::NOW;
    if (!tile->is_solid_color_analysis_performed() &&
        tile->use_picture_analysis() && kUseColorEstimator) {
      // We analyze for solid color here, to decide to continue
      // or drop the tile for scheduling and raster.
      // TODO(sohanjg): Check if we could use a shared analysis
      // canvas which is reset between tiles.
      tile->set_solid_color_analysis_performed(true);
      SkColor color = SK_ColorTRANSPARENT;
      bool is_solid_color =
          prioritized_tile.raster_source()->PerformSolidColorAnalysis(
              tile->content_rect(), tile->raster_scales(), &color);
      if (is_solid_color) {
        tile->draw_info().set_solid_color(color);
        tile->draw_info().set_was_ever_ready_to_draw();
        if (!tile_is_needed_now)
          tile->draw_info().set_was_a_prepaint_tile();
        client_->NotifyTileStateChanged(tile);
        continue;
      }
    }

    // Prepaint tiles that are far away are only processed for images.
    if (!tile->required_for_activation() && !tile->required_for_draw() &&
        prioritized_tile.is_process_for_images_only()) {
      work_to_schedule.tiles_to_process_for_images.push_back(prioritized_tile);
      continue;
    }

    // We won't be able to schedule this tile, so break out early.
    if (work_to_schedule.tiles_to_raster.size() >=
        scheduled_raster_task_limit_) {
      all_tiles_that_need_to_be_rasterized_are_scheduled_ = false;
      break;
    }

    tile->scheduled_priority_ = schedule_priority++;

    DCHECK(tile->draw_info().mode() == TileDrawInfo::OOM_MODE ||
           !tile->draw_info().IsReadyToDraw());

    // If the tile already has a raster_task, then the memory used by it is
    // already accounted for in memory_usage. Otherwise, we'll have to acquire
    // more memory to create a raster task.
    MemoryUsage memory_required_by_tile_to_be_scheduled;
    if (!tile->raster_task_.get()) {
      memory_required_by_tile_to_be_scheduled = MemoryUsage::FromConfig(
          tile->desired_texture_size(), DetermineResourceFormat(tile));
    }

    // This is the memory limit that will be used by this tile. Depending on
    // the tile priority, it will be one of hard_memory_limit or
    // soft_memory_limit.
    MemoryUsage& tile_memory_limit =
        tile_is_needed_now ? hard_memory_limit : soft_memory_limit;

    const MemoryUsage& scheduled_tile_memory_limit =
        tile_memory_limit - memory_required_by_tile_to_be_scheduled;
    eviction_priority_queue =
        FreeTileResourcesWithLowerPriorityUntilUsageIsWithinLimit(
            std::move(eviction_priority_queue), scheduled_tile_memory_limit,
            priority, &memory_usage);
    bool memory_usage_is_within_limit =
        !memory_usage.Exceeds(scheduled_tile_memory_limit);

    // If we couldn't fit the tile into our current memory limit, then we're
    // done.
    if (!memory_usage_is_within_limit) {
      if (tile_is_needed_now)
        had_enough_memory_to_schedule_tiles_needed_now = false;
      all_tiles_that_need_to_be_rasterized_are_scheduled_ = false;
      break;
    }

    memory_usage += memory_required_by_tile_to_be_scheduled;
    work_to_schedule.tiles_to_raster.push_back(prioritized_tile);

    // Since we scheduled the tile, set whether it was a prepaint or not
    // assuming that the tile will successfully finish running. We don't have
    // priority information at the time the tile completes, so it should be done
    // here.
    if (!tile_is_needed_now)
      tile->draw_info().set_was_a_prepaint_tile();
  }

  // Note that we should try and further reduce memory in case the above loop
  // didn't reduce memory. This ensures that we always release as many resources
  // as possible to stay within the memory limit.
  eviction_priority_queue = FreeTileResourcesUntilUsageIsWithinLimit(
      std::move(eviction_priority_queue), hard_memory_limit, &memory_usage);

  UMA_HISTOGRAM_BOOLEAN("TileManager.ExceededMemoryBudget",
                        !had_enough_memory_to_schedule_tiles_needed_now);
  did_oom_on_last_assign_ = !had_enough_memory_to_schedule_tiles_needed_now;

  memory_stats_from_last_assign_.total_budget_in_bytes =
      global_state_.hard_memory_limit_in_bytes;
  memory_stats_from_last_assign_.total_bytes_used = memory_usage.memory_bytes();
  DCHECK_GE(memory_stats_from_last_assign_.total_bytes_used, 0);
  memory_stats_from_last_assign_.had_enough_memory =
      had_enough_memory_to_schedule_tiles_needed_now;

  TRACE_EVENT_END2("cc", "TileManager::AssignGpuMemoryToTiles",
                   "all_tiles_that_need_to_be_rasterized_are_scheduled",
                   all_tiles_that_need_to_be_rasterized_are_scheduled_,
                   "had_enough_memory_to_schedule_tiles_needed_now",
                   had_enough_memory_to_schedule_tiles_needed_now);
  return work_to_schedule;
}

void TileManager::FreeResourcesForTile(Tile* tile) {
  TileDrawInfo& draw_info = tile->draw_info();
  if (draw_info.resource_) {
    resource_pool_->ReleaseResource(draw_info.resource_);
    draw_info.resource_ = nullptr;
  }
}

void TileManager::FreeResourcesForTileAndNotifyClientIfTileWasReadyToDraw(
    Tile* tile) {
  bool was_ready_to_draw = tile->draw_info().IsReadyToDraw();
  FreeResourcesForTile(tile);
  if (was_ready_to_draw)
    client_->NotifyTileStateChanged(tile);
}

void TileManager::ScheduleTasks(
    const PrioritizedWorkToSchedule& work_to_schedule) {
  const std::vector<PrioritizedTile>& tiles_that_need_to_be_rasterized =
      work_to_schedule.tiles_to_raster;
  TRACE_EVENT1("cc", "TileManager::ScheduleTasks", "count",
               tiles_that_need_to_be_rasterized.size());

  DCHECK(did_check_for_completed_tasks_since_last_schedule_tasks_);

  if (!has_scheduled_tile_tasks_) {
    TRACE_EVENT_ASYNC_BEGIN0("cc", "ScheduledTasks", this);
  }

  // Cancel existing OnTaskSetFinished callbacks.
  task_set_finished_weak_ptr_factory_.InvalidateWeakPtrs();

  // Even when scheduling an empty set of tiles, the TTWP does some work, and
  // will always trigger a DidFinishRunningTileTasks notification. Because of
  // this we unconditionally set |has_scheduled_tile_tasks_| to true.
  has_scheduled_tile_tasks_ = true;

  // Track the number of dependents for each *_done task.
  size_t required_for_activate_count = 0;
  size_t required_for_draw_count = 0;
  size_t all_count = 0;

  size_t priority = kTileTaskPriorityBase;

  graph_.Reset();

  gfx::ColorSpace color_space = client_->GetTileColorSpace();

  scoped_refptr<TileTask> required_for_activation_done_task =
      CreateTaskSetFinishedTask(
          &TileManager::DidFinishRunningTileTasksRequiredForActivation);
  scoped_refptr<TileTask> required_for_draw_done_task =
      CreateTaskSetFinishedTask(
          &TileManager::DidFinishRunningTileTasksRequiredForDraw);
  scoped_refptr<TileTask> all_done_task =
      CreateTaskSetFinishedTask(&TileManager::DidFinishRunningAllTileTasks);

  // Build a new task queue containing all task currently needed. Tasks
  // are added in order of priority, highest priority task first.
  for (auto& prioritized_tile : tiles_that_need_to_be_rasterized) {
    Tile* tile = prioritized_tile.tile();

    DCHECK(tile->draw_info().requires_resource());
    DCHECK(!tile->draw_info().resource_);

    if (!tile->raster_task_)
      tile->raster_task_ = CreateRasterTask(prioritized_tile, color_space);

    TileTask* task = tile->raster_task_.get();

    DCHECK(!task->HasCompleted());

    if (tile->required_for_activation()) {
      required_for_activate_count++;
      graph_.edges.push_back(
          TaskGraph::Edge(task, required_for_activation_done_task.get()));
    }
    if (tile->required_for_draw()) {
      required_for_draw_count++;
      graph_.edges.push_back(
          TaskGraph::Edge(task, required_for_draw_done_task.get()));
    }
    all_count++;
    graph_.edges.push_back(TaskGraph::Edge(task, all_done_task.get()));

    // A tile should use a foreground task cateogry if it is either blocking
    // future compositing (required for draw or required for activation), or if
    // it has a priority bin of NOW for another reason (low resolution tiles).
    bool use_foreground_category =
        tile->required_for_draw() || tile->required_for_activation() ||
        prioritized_tile.priority().priority_bin == TilePriority::NOW;
    InsertNodesForRasterTask(&graph_, task, task->dependencies(), priority++,
                             use_foreground_category);
  }

  const std::vector<PrioritizedTile>& tiles_to_process_for_images =
      work_to_schedule.tiles_to_process_for_images;
  std::vector<DrawImage> new_locked_images;
  for (const PrioritizedTile& prioritized_tile : tiles_to_process_for_images) {
    Tile* tile = prioritized_tile.tile();

    std::vector<DrawImage> images;
    prioritized_tile.raster_source()->GetDiscardableImagesInRect(
        tile->enclosing_layer_rect(), tile->raster_scales(), &images);
    new_locked_images.insert(new_locked_images.end(), images.begin(),
                             images.end());
  }

  // TODO(vmpstr): SOON is misleading here, but these images can come from
  // several diffent tiles. Rethink what we actually want to trace here. Note
  // that I'm using SOON, since it can't be NOW (these are prepaint).
  ImageDecodeController::TracingInfo tracing_info(prepare_tiles_count_,
                                                  TilePriority::SOON);
  std::vector<scoped_refptr<TileTask>> new_locked_image_tasks =
      image_manager_.SetPredecodeImages(std::move(new_locked_images),
                                        tracing_info);

  for (auto& task : new_locked_image_tasks) {
    auto decode_it = std::find_if(graph_.nodes.begin(), graph_.nodes.end(),
                                  [&task](const TaskGraph::Node& node) {
                                    return node.task == task.get();
                                  });
    // If this task is already in the graph, then we don't have to insert it.
    if (decode_it != graph_.nodes.end())
      continue;

    InsertNodeForDecodeTask(&graph_, task.get(), false, priority++);
    all_count++;
    graph_.edges.push_back(TaskGraph::Edge(task.get(), all_done_task.get()));
  }

  // The old locked images tasks have to stay around until past the
  // ScheduleTasks call below, so we do a swap instead of a move.
  // TODO(crbug.com/647402): Have the tile_task_manager keep a ref on the tasks,
  // since it makes it awkward for the callers to keep refs on tasks that only
  // exist within the task graph runner.
  locked_image_tasks_.swap(new_locked_image_tasks);

  // We must reduce the amount of unused resources before calling
  // ScheduleTasks to prevent usage from rising above limits.
  resource_pool_->ReduceResourceUsage();
  image_manager_.ReduceMemoryUsage();

  // Insert nodes for our task completion tasks. We enqueue these using
  // NONCONCURRENT_FOREGROUND category this is the highest prioirty category and
  // we'd like to run these tasks as soon as possible.
  InsertNodeForTask(&graph_, required_for_activation_done_task.get(),
                    TASK_CATEGORY_NONCONCURRENT_FOREGROUND,
                    kRequiredForActivationDoneTaskPriority,
                    required_for_activate_count);
  InsertNodeForTask(&graph_, required_for_draw_done_task.get(),
                    TASK_CATEGORY_NONCONCURRENT_FOREGROUND,
                    kRequiredForDrawDoneTaskPriority, required_for_draw_count);
  InsertNodeForTask(&graph_, all_done_task.get(),
                    TASK_CATEGORY_NONCONCURRENT_FOREGROUND,
                    kAllDoneTaskPriority, all_count);

  // Synchronize worker with compositor.
  raster_buffer_provider_->OrderingBarrier();

  // Schedule running of |raster_queue_|. This replaces any previously
  // scheduled tasks and effectively cancels all tasks not present
  // in |raster_queue_|.
  tile_task_manager_->ScheduleTasks(&graph_);

  // It's now safe to clean up orphan tasks as raster worker pool is not
  // allowed to keep around unreferenced raster tasks after ScheduleTasks() has
  // been called.
  orphan_tasks_.clear();

  // It's also now safe to replace our *_done_task_ tasks.
  required_for_activation_done_task_ =
      std::move(required_for_activation_done_task);
  required_for_draw_done_task_ = std::move(required_for_draw_done_task);
  all_done_task_ = std::move(all_done_task);

  did_check_for_completed_tasks_since_last_schedule_tasks_ = false;

  TRACE_EVENT_ASYNC_STEP_INTO1("cc", "ScheduledTasks", this, "running", "state",
                               ScheduledTasksStateAsValue());
}

scoped_refptr<TileTask> TileManager::CreateRasterTask(
    const PrioritizedTile& prioritized_tile,
    const gfx::ColorSpace& color_space) {
  Tile* tile = prioritized_tile.tile();

  // Get the resource.
  uint64_t resource_content_id = 0;
  Resource* resource = nullptr;
  gfx::Rect invalidated_rect = tile->invalidated_content_rect();
  if (UsePartialRaster() && tile->invalidated_id()) {
    resource = resource_pool_->TryAcquireResourceForPartialRaster(
        tile->id(), tile->invalidated_content_rect(), tile->invalidated_id(),
        &invalidated_rect);
  }

  if (resource) {
    resource_content_id = tile->invalidated_id();
    DCHECK_EQ(DetermineResourceFormat(tile), resource->format());
  } else {
    resource = resource_pool_->AcquireResource(tile->desired_texture_size(),
                                               DetermineResourceFormat(tile),
                                               color_space);
  }

  // For LOW_RESOLUTION tiles, we don't draw or predecode images.
  RasterSource::PlaybackSettings playback_settings;
  playback_settings.skip_images =
      prioritized_tile.priority().resolution == LOW_RESOLUTION;

  // Create and queue all image decode tasks that this tile depends on.
  TileTask::Vector decode_tasks;
  std::vector<DrawImage>& images = scheduled_draw_images_[tile->id()];
  images.clear();
  if (!playback_settings.skip_images) {
    prioritized_tile.raster_source()->GetDiscardableImagesInRect(
        tile->enclosing_layer_rect(), tile->raster_scales(), &images);
  }

  // We can skip the image hijack canvas if we have no images.
  playback_settings.use_image_hijack_canvas = !images.empty();

  // Get the tasks for the required images.
  ImageDecodeController::TracingInfo tracing_info(
      prepare_tiles_count_, prioritized_tile.priority().priority_bin);
  image_manager_.GetTasksForImagesAndRef(&images, &decode_tasks, tracing_info);

  std::unique_ptr<RasterBuffer> raster_buffer =
      raster_buffer_provider_->AcquireBufferForRaster(
          resource, resource_content_id, tile->invalidated_id());
  return make_scoped_refptr(new RasterTaskImpl(
      this, tile, resource, prioritized_tile.raster_source(), playback_settings,
      prioritized_tile.priority().resolution, invalidated_rect,
      prepare_tiles_count_, std::move(raster_buffer), &decode_tasks,
      use_gpu_rasterization_));
}

void TileManager::OnRasterTaskCompleted(
    std::unique_ptr<RasterBuffer> raster_buffer,
    Tile* tile,
    Resource* resource,
    bool was_canceled) {
  DCHECK(tile);
  DCHECK(tiles_.find(tile->id()) != tiles_.end());
  raster_buffer_provider_->ReleaseBufferForRaster(std::move(raster_buffer));

  TileDrawInfo& draw_info = tile->draw_info();
  DCHECK(tile->raster_task_.get());
  orphan_tasks_.push_back(tile->raster_task_);
  tile->raster_task_ = nullptr;

  // Unref all the images.
  auto images_it = scheduled_draw_images_.find(tile->id());
  image_manager_.UnrefImages(images_it->second);
  scheduled_draw_images_.erase(images_it);

  if (was_canceled) {
    ++flush_stats_.canceled_count;
    resource_pool_->ReleaseResource(resource);
    return;
  }

  resource_pool_->OnContentReplaced(resource->id(), tile->id());
  ++flush_stats_.completed_count;

  draw_info.set_use_resource();
  draw_info.resource_ = resource;
  draw_info.contents_swizzled_ = DetermineResourceRequiresSwizzle(tile);

  DCHECK(draw_info.IsReadyToDraw());
  draw_info.set_was_ever_ready_to_draw();

  client_->NotifyTileStateChanged(tile);
}

ScopedTilePtr TileManager::CreateTile(const Tile::CreateInfo& info,
                                      int layer_id,
                                      int source_frame_number,
                                      int flags) {
  // We need to have a tile task worker pool to do anything meaningful with
  // tiles.
  DCHECK(tile_task_manager_);
  ScopedTilePtr tile(
      new Tile(this, info, layer_id, source_frame_number, flags));
  DCHECK(tiles_.find(tile->id()) == tiles_.end());

  tiles_[tile->id()] = tile.get();
  return tile;
}

bool TileManager::AreRequiredTilesReadyToDraw(
    RasterTilePriorityQueue::Type type) const {
  std::unique_ptr<RasterTilePriorityQueue> raster_priority_queue(
      client_->BuildRasterQueue(global_state_.tree_priority, type));
  // It is insufficient to check whether the raster queue we constructed is
  // empty. The reason for this is that there are situations (rasterize on
  // demand) when the tile both needs raster and it's ready to draw. Hence, we
  // have to iterate the queue to check whether the required tiles are ready to
  // draw.
  for (; !raster_priority_queue->IsEmpty(); raster_priority_queue->Pop()) {
    if (!raster_priority_queue->Top().tile()->draw_info().IsReadyToDraw())
      return false;
  }

#if DCHECK_IS_ON()
  std::unique_ptr<RasterTilePriorityQueue> all_queue(
      client_->BuildRasterQueue(global_state_.tree_priority, type));
  for (; !all_queue->IsEmpty(); all_queue->Pop()) {
    Tile* tile = all_queue->Top().tile();
    DCHECK(!tile->required_for_activation() ||
           tile->draw_info().IsReadyToDraw());
  }
#endif
  return true;
}

bool TileManager::IsReadyToActivate() const {
  TRACE_EVENT0("cc", "TileManager::IsReadyToActivate");
  return AreRequiredTilesReadyToDraw(
      RasterTilePriorityQueue::Type::REQUIRED_FOR_ACTIVATION);
}

bool TileManager::IsReadyToDraw() const {
  TRACE_EVENT0("cc", "TileManager::IsReadyToDraw");
  return AreRequiredTilesReadyToDraw(
      RasterTilePriorityQueue::Type::REQUIRED_FOR_DRAW);
}

void TileManager::CheckAndIssueSignals() {
  TRACE_EVENT0("cc", "TileManager::CheckAndIssueSignals");
  tile_task_manager_->CheckForCompletedTasks();
  did_check_for_completed_tasks_since_last_schedule_tasks_ = true;

  // Ready to activate.
  if (signals_.ready_to_activate && !signals_.did_notify_ready_to_activate) {
    signals_.ready_to_activate = false;
    if (IsReadyToActivate()) {
      TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                   "TileManager::CheckAndIssueSignals - ready to activate");
      signals_.did_notify_ready_to_activate = true;
      client_->NotifyReadyToActivate();
    }
  }

  // Ready to draw.
  if (signals_.ready_to_draw && !signals_.did_notify_ready_to_draw) {
    signals_.ready_to_draw = false;
    if (IsReadyToDraw()) {
      TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                   "TileManager::CheckAndIssueSignals - ready to draw");
      signals_.did_notify_ready_to_draw = true;
      client_->NotifyReadyToDraw();
    }
  }

  // All tile tasks completed.
  if (signals_.all_tile_tasks_completed &&
      !signals_.did_notify_all_tile_tasks_completed) {
    signals_.all_tile_tasks_completed = false;
    if (!has_scheduled_tile_tasks_) {
      TRACE_EVENT0(
          TRACE_DISABLED_BY_DEFAULT("cc.debug"),
          "TileManager::CheckAndIssueSignals - all tile tasks completed");
      signals_.did_notify_all_tile_tasks_completed = true;
      client_->NotifyAllTileTasksCompleted();
    }
  }
}

void TileManager::CheckIfMoreTilesNeedToBePrepared() {
  tile_task_manager_->CheckForCompletedTasks();
  did_check_for_completed_tasks_since_last_schedule_tasks_ = true;

  // When OOM, keep re-assigning memory until we reach a steady state
  // where top-priority tiles are initialized.
  PrioritizedWorkToSchedule work_to_schedule = AssignGpuMemoryToTiles();

  // Inform the client that will likely require a draw if the highest priority
  // tile that will be rasterized is required for draw.
  client_->SetIsLikelyToRequireADraw(
      !work_to_schedule.tiles_to_raster.empty() &&
      work_to_schedule.tiles_to_raster.front().tile()->required_for_draw());

  // |tiles_that_need_to_be_rasterized| will be empty when we reach a
  // steady memory state. Keep scheduling tasks until we reach this state.
  if (!work_to_schedule.tiles_to_raster.empty()) {
    ScheduleTasks(work_to_schedule);
    return;
  }

  // If we're not in SMOOTHNESS_TAKES_PRIORITY  mode, we should unlock all
  // images since we're technically going idle here at least for this frame.
  if (global_state_.tree_priority != SMOOTHNESS_TAKES_PRIORITY) {
    image_manager_.SetPredecodeImages(std::vector<DrawImage>(),
                                      ImageDecodeController::TracingInfo());
    locked_image_tasks_.clear();
  }

  FreeResourcesForReleasedTiles();

  resource_pool_->ReduceResourceUsage();
  image_manager_.ReduceMemoryUsage();

  // TODO(vmpstr): Temporary check to debug crbug.com/642927.
  CHECK(tile_task_manager_);
  signals_.all_tile_tasks_completed = true;
  signals_check_notifier_.Schedule();

  // We don't reserve memory for required-for-activation tiles during
  // accelerated gestures, so we just postpone activation when we don't
  // have these tiles, and activate after the accelerated gesture.
  // Likewise if we don't allow any tiles (as is the case when we're
  // invisible), if we have tiles that aren't ready, then we shouldn't
  // activate as activation can cause checkerboards.
  bool wait_for_all_required_tiles =
      global_state_.tree_priority == SMOOTHNESS_TAKES_PRIORITY ||
      global_state_.memory_limit_policy == ALLOW_NOTHING;

  // If we have tiles left to raster for activation, and we don't allow
  // activating without them, then skip activation and return early.
  if (wait_for_all_required_tiles)
    return;

  // Mark any required tiles that have not been been assigned memory after
  // reaching a steady memory state as OOM. This ensures that we activate/draw
  // even when OOM. Note that we can't reuse the queue we used for
  // AssignGpuMemoryToTiles, since the AssignGpuMemoryToTiles call could have
  // evicted some tiles that would not be picked up by the old raster queue.
  MarkTilesOutOfMemory(client_->BuildRasterQueue(
      global_state_.tree_priority,
      RasterTilePriorityQueue::Type::REQUIRED_FOR_ACTIVATION));
  MarkTilesOutOfMemory(client_->BuildRasterQueue(
      global_state_.tree_priority,
      RasterTilePriorityQueue::Type::REQUIRED_FOR_DRAW));

  // TODO(vmpstr): Temporary check to debug crbug.com/642927.
  CHECK(tile_task_manager_);

  DCHECK(IsReadyToActivate());
  DCHECK(IsReadyToDraw());
  signals_.ready_to_activate = true;
  signals_.ready_to_draw = true;
  // TODO(ericrk): Investigate why we need to schedule this (not just call it
  // inline). http://crbug.com/498439
  signals_check_notifier_.Schedule();
}

void TileManager::MarkTilesOutOfMemory(
    std::unique_ptr<RasterTilePriorityQueue> queue) const {
  // Mark required tiles as OOM so that we can activate/draw without them.
  for (; !queue->IsEmpty(); queue->Pop()) {
    Tile* tile = queue->Top().tile();
    if (tile->draw_info().IsReadyToDraw())
      continue;
    tile->draw_info().set_oom();
    client_->NotifyTileStateChanged(tile);
  }
}

ResourceFormat TileManager::DetermineResourceFormat(const Tile* tile) const {
  return raster_buffer_provider_->GetResourceFormat(!tile->is_opaque());
}

bool TileManager::DetermineResourceRequiresSwizzle(const Tile* tile) const {
  return raster_buffer_provider_->IsResourceSwizzleRequired(!tile->is_opaque());
}

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
TileManager::ScheduledTasksStateAsValue() const {
  std::unique_ptr<base::trace_event::TracedValue> state(
      new base::trace_event::TracedValue());
  state->BeginDictionary("tasks_pending");
  state->SetBoolean("ready_to_activate", signals_.ready_to_activate);
  state->SetBoolean("ready_to_draw", signals_.ready_to_draw);
  state->SetBoolean("all_tile_tasks_completed",
                    signals_.all_tile_tasks_completed);
  state->EndDictionary();
  return std::move(state);
}

bool TileManager::UsePartialRaster() const {
  return use_partial_raster_ &&
         raster_buffer_provider_->CanPartialRasterIntoProvidedResource();
}

// Utility function that can be used to create a "Task set finished" task that
// posts |callback| to |task_runner| when run.
scoped_refptr<TileTask> TileManager::CreateTaskSetFinishedTask(
    void (TileManager::*callback)()) {
  return make_scoped_refptr(new TaskSetFinishedTaskImpl(
      task_runner_,
      base::Bind(callback, task_set_finished_weak_ptr_factory_.GetWeakPtr())));
}

TileManager::MemoryUsage::MemoryUsage()
    : memory_bytes_(0), resource_count_(0) {}

TileManager::MemoryUsage::MemoryUsage(size_t memory_bytes,
                                      size_t resource_count)
    : memory_bytes_(static_cast<int64_t>(memory_bytes)),
      resource_count_(static_cast<int>(resource_count)) {
  // MemoryUsage is constructed using size_ts, since it deals with memory and
  // the inputs are typically size_t. However, during the course of usage (in
  // particular operator-=) can cause internal values to become negative. Thus,
  // member variables are signed.
  DCHECK_LE(memory_bytes,
            static_cast<size_t>(std::numeric_limits<int64_t>::max()));
  DCHECK_LE(resource_count,
            static_cast<size_t>(std::numeric_limits<int>::max()));
}

// static
TileManager::MemoryUsage TileManager::MemoryUsage::FromConfig(
    const gfx::Size& size,
    ResourceFormat format) {
  // We can use UncheckedSizeInBytes here since this is used with a tile
  // size which is determined by the compositor (it's at most max texture size).
  return MemoryUsage(ResourceUtil::UncheckedSizeInBytes<size_t>(size, format),
                     1);
}

// static
TileManager::MemoryUsage TileManager::MemoryUsage::FromTile(const Tile* tile) {
  const TileDrawInfo& draw_info = tile->draw_info();
  if (draw_info.resource_) {
    return MemoryUsage::FromConfig(draw_info.resource_->size(),
                                   draw_info.resource_->format());
  }
  return MemoryUsage();
}

TileManager::MemoryUsage& TileManager::MemoryUsage::operator+=(
    const MemoryUsage& other) {
  memory_bytes_ += other.memory_bytes_;
  resource_count_ += other.resource_count_;
  return *this;
}

TileManager::MemoryUsage& TileManager::MemoryUsage::operator-=(
    const MemoryUsage& other) {
  memory_bytes_ -= other.memory_bytes_;
  resource_count_ -= other.resource_count_;
  return *this;
}

TileManager::MemoryUsage TileManager::MemoryUsage::operator-(
    const MemoryUsage& other) {
  MemoryUsage result = *this;
  result -= other;
  return result;
}

bool TileManager::MemoryUsage::Exceeds(const MemoryUsage& limit) const {
  return memory_bytes_ > limit.memory_bytes_ ||
         resource_count_ > limit.resource_count_;
}

TileManager::Signals::Signals() {
  reset();
}

void TileManager::Signals::reset() {
  ready_to_activate = false;
  did_notify_ready_to_activate = false;
  ready_to_draw = false;
  did_notify_ready_to_draw = false;
  all_tile_tasks_completed = false;
  did_notify_all_tile_tasks_completed = false;
}

TileManager::PrioritizedWorkToSchedule::PrioritizedWorkToSchedule() = default;
TileManager::PrioritizedWorkToSchedule::PrioritizedWorkToSchedule(
    PrioritizedWorkToSchedule&& other) = default;
TileManager::PrioritizedWorkToSchedule::~PrioritizedWorkToSchedule() = default;

}  // namespace cc
