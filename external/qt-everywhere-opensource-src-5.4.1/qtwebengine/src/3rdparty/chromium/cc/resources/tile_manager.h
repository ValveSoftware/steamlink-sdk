// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_TILE_MANAGER_H_
#define CC_RESOURCES_TILE_MANAGER_H_

#include <deque>
#include <queue>
#include <set>
#include <utility>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "cc/base/ref_counted_managed.h"
#include "cc/base/unique_notifier.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/resources/managed_tile_state.h"
#include "cc/resources/memory_history.h"
#include "cc/resources/picture_pile_impl.h"
#include "cc/resources/prioritized_tile_set.h"
#include "cc/resources/rasterizer.h"
#include "cc/resources/resource_pool.h"
#include "cc/resources/tile.h"

namespace cc {
class ResourceProvider;

class CC_EXPORT TileManagerClient {
 public:
  // Returns the set of layers that the tile manager should consider for raster.
  virtual const std::vector<PictureLayerImpl*>& GetPictureLayers() = 0;

  // Called when all tiles marked as required for activation are ready to draw.
  virtual void NotifyReadyToActivate() = 0;

  // Called when the visible representation of a tile might have changed. Some
  // examples are:
  // - Tile version initialized.
  // - Tile resources freed.
  // - Tile marked for on-demand raster.
  virtual void NotifyTileStateChanged(const Tile* tile) = 0;

 protected:
  virtual ~TileManagerClient() {}
};

struct RasterTaskCompletionStats {
  RasterTaskCompletionStats();

  size_t completed_count;
  size_t canceled_count;
};
scoped_ptr<base::Value> RasterTaskCompletionStatsAsValue(
    const RasterTaskCompletionStats& stats);

// This class manages tiles, deciding which should get rasterized and which
// should no longer have any memory assigned to them. Tile objects are "owned"
// by layers; they automatically register with the manager when they are
// created, and unregister from the manager when they are deleted.
class CC_EXPORT TileManager : public RasterizerClient,
                              public RefCountedManager<Tile> {
 public:
  struct CC_EXPORT PairedPictureLayer {
    PairedPictureLayer();
    ~PairedPictureLayer();

    PictureLayerImpl* active_layer;
    PictureLayerImpl* pending_layer;
  };

  class CC_EXPORT RasterTileIterator {
   public:
    RasterTileIterator(TileManager* tile_manager, TreePriority tree_priority);
    ~RasterTileIterator();

    RasterTileIterator& operator++();
    operator bool() const;
    Tile* operator*();

   private:
    struct PairedPictureLayerIterator {
      PairedPictureLayerIterator();
      ~PairedPictureLayerIterator();

      Tile* PeekTile(TreePriority tree_priority);
      void PopTile(TreePriority tree_priority);

      std::pair<PictureLayerImpl::LayerRasterTileIterator*, WhichTree>
          NextTileIterator(TreePriority tree_priority);

      PictureLayerImpl::LayerRasterTileIterator active_iterator;
      PictureLayerImpl::LayerRasterTileIterator pending_iterator;

      std::vector<Tile*> returned_shared_tiles;
    };

    class RasterOrderComparator {
     public:
      explicit RasterOrderComparator(TreePriority tree_priority);

      bool operator()(PairedPictureLayerIterator* a,
                      PairedPictureLayerIterator* b) const;

     private:
      TreePriority tree_priority_;
    };

    std::vector<PairedPictureLayerIterator> paired_iterators_;
    std::vector<PairedPictureLayerIterator*> iterator_heap_;
    TreePriority tree_priority_;
    RasterOrderComparator comparator_;

    DISALLOW_COPY_AND_ASSIGN(RasterTileIterator);
  };

  struct CC_EXPORT EvictionTileIterator {
   public:
    EvictionTileIterator();
    EvictionTileIterator(TileManager* tile_manager, TreePriority tree_priority);
    ~EvictionTileIterator();

    EvictionTileIterator& operator++();
    operator bool() const;
    Tile* operator*();

   private:
    struct PairedPictureLayerIterator {
      PairedPictureLayerIterator();
      ~PairedPictureLayerIterator();

      Tile* PeekTile(TreePriority tree_priority);
      void PopTile(TreePriority tree_priority);

      PictureLayerImpl::LayerEvictionTileIterator* NextTileIterator(
          TreePriority tree_priority);

      PictureLayerImpl::LayerEvictionTileIterator active_iterator;
      PictureLayerImpl::LayerEvictionTileIterator pending_iterator;

      std::vector<Tile*> returned_shared_tiles;
    };

    class EvictionOrderComparator {
     public:
      explicit EvictionOrderComparator(TreePriority tree_priority);

      bool operator()(PairedPictureLayerIterator* a,
                      PairedPictureLayerIterator* b) const;

     private:
      TreePriority tree_priority_;
    };

    std::vector<PairedPictureLayerIterator> paired_iterators_;
    std::vector<PairedPictureLayerIterator*> iterator_heap_;
    TreePriority tree_priority_;
    EvictionOrderComparator comparator_;

    DISALLOW_COPY_AND_ASSIGN(EvictionTileIterator);
  };

  static scoped_ptr<TileManager> Create(
      TileManagerClient* client,
      base::SequencedTaskRunner* task_runner,
      ResourcePool* resource_pool,
      Rasterizer* rasterizer,
      RenderingStatsInstrumentation* rendering_stats_instrumentation);
  virtual ~TileManager();

  void ManageTiles(const GlobalStateThatImpactsTilePriority& state);

  // Returns true when visible tiles have been initialized.
  bool UpdateVisibleTiles();

  scoped_refptr<Tile> CreateTile(PicturePileImpl* picture_pile,
                                 const gfx::Size& tile_size,
                                 const gfx::Rect& content_rect,
                                 const gfx::Rect& opaque_rect,
                                 float contents_scale,
                                 int layer_id,
                                 int source_frame_number,
                                 int flags);

  scoped_ptr<base::Value> BasicStateAsValue() const;
  scoped_ptr<base::Value> AllTilesAsValue() const;
  const MemoryHistory::Entry& memory_stats_from_last_assign() const {
    return memory_stats_from_last_assign_;
  }

  void GetPairedPictureLayers(std::vector<PairedPictureLayer>* layers) const;

  void InitializeTilesWithResourcesForTesting(const std::vector<Tile*>& tiles) {
    for (size_t i = 0; i < tiles.size(); ++i) {
      ManagedTileState& mts = tiles[i]->managed_state();
      ManagedTileState::TileVersion& tile_version =
          mts.tile_versions[HIGH_QUALITY_RASTER_MODE];

      tile_version.resource_ = resource_pool_->AcquireResource(gfx::Size(1, 1));

      bytes_releasable_ += BytesConsumedIfAllocated(tiles[i]);
      ++resources_releasable_;
    }
  }

  void ReleaseTileResourcesForTesting(const std::vector<Tile*>& tiles) {
    for (size_t i = 0; i < tiles.size(); ++i) {
      Tile* tile = tiles[i];
      for (int mode = 0; mode < NUM_RASTER_MODES; ++mode) {
        FreeResourceForTile(tile, static_cast<RasterMode>(mode));
      }
    }
  }

  void SetGlobalStateForTesting(
      const GlobalStateThatImpactsTilePriority& state) {
    // Soft limit is used for resource pool such that
    // memory returns to soft limit after going over.
    if (state != global_state_) {
      global_state_ = state;
      prioritized_tiles_dirty_ = true;
    }
  }

  void SetRasterizerForTesting(Rasterizer* rasterizer);

  void CleanUpReleasedTilesForTesting() { CleanUpReleasedTiles(); }

 protected:
  TileManager(TileManagerClient* client,
              base::SequencedTaskRunner* task_runner,
              ResourcePool* resource_pool,
              Rasterizer* rasterizer,
              RenderingStatsInstrumentation* rendering_stats_instrumentation);

  // Methods called by Tile
  friend class Tile;
  void DidChangeTilePriority(Tile* tile);

  void CleanUpReleasedTiles();

  // Overriden from RefCountedManager<Tile>:
  virtual void Release(Tile* tile) OVERRIDE;

  // Overriden from RasterizerClient:
  virtual bool ShouldForceTasksRequiredForActivationToComplete() const OVERRIDE;
  virtual void DidFinishRunningTasks() OVERRIDE;
  virtual void DidFinishRunningTasksRequiredForActivation() OVERRIDE;

  typedef std::vector<Tile*> TileVector;
  typedef std::set<Tile*> TileSet;

  // Virtual for test
  virtual void ScheduleTasks(
      const TileVector& tiles_that_need_to_be_rasterized);

  void AssignGpuMemoryToTiles(PrioritizedTileSet* tiles,
                              TileVector* tiles_that_need_to_be_rasterized);
  void GetTilesWithAssignedBins(PrioritizedTileSet* tiles);

 private:
  void OnImageDecodeTaskCompleted(int layer_id,
                                  SkPixelRef* pixel_ref,
                                  bool was_canceled);
  void OnRasterTaskCompleted(Tile::Id tile,
                             scoped_ptr<ScopedResource> resource,
                             RasterMode raster_mode,
                             const PicturePileImpl::Analysis& analysis,
                             bool was_canceled);

  inline size_t BytesConsumedIfAllocated(const Tile* tile) const {
    return Resource::MemorySizeBytes(tile->size(),
                                     resource_pool_->resource_format());
  }

  void FreeResourceForTile(Tile* tile, RasterMode mode);
  void FreeResourcesForTile(Tile* tile);
  void FreeUnusedResourcesForTile(Tile* tile);
  void FreeResourcesForTileAndNotifyClientIfTileWasReadyToDraw(Tile* tile);
  scoped_refptr<ImageDecodeTask> CreateImageDecodeTask(Tile* tile,
                                                       SkPixelRef* pixel_ref);
  scoped_refptr<RasterTask> CreateRasterTask(Tile* tile);
  void UpdatePrioritizedTileSetIfNeeded();

  bool IsReadyToActivate() const;
  void CheckIfReadyToActivate();

  TileManagerClient* client_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  ResourcePool* resource_pool_;
  Rasterizer* rasterizer_;
  GlobalStateThatImpactsTilePriority global_state_;

  typedef base::hash_map<Tile::Id, Tile*> TileMap;
  TileMap tiles_;

  PrioritizedTileSet prioritized_tiles_;
  bool prioritized_tiles_dirty_;

  bool all_tiles_that_need_to_be_rasterized_have_memory_;
  bool all_tiles_required_for_activation_have_memory_;

  size_t bytes_releasable_;
  size_t resources_releasable_;

  bool ever_exceeded_memory_budget_;
  MemoryHistory::Entry memory_stats_from_last_assign_;

  RenderingStatsInstrumentation* rendering_stats_instrumentation_;

  bool did_initialize_visible_tile_;
  bool did_check_for_completed_tasks_since_last_schedule_tasks_;

  typedef base::hash_map<uint32_t, scoped_refptr<ImageDecodeTask> >
      PixelRefTaskMap;
  typedef base::hash_map<int, PixelRefTaskMap> LayerPixelRefTaskMap;
  LayerPixelRefTaskMap image_decode_tasks_;

  typedef base::hash_map<int, int> LayerCountMap;
  LayerCountMap used_layer_counts_;

  RasterTaskCompletionStats update_visible_tiles_stats_;

  std::vector<Tile*> released_tiles_;

  ResourceFormat resource_format_;

  // Queue used when scheduling raster tasks.
  RasterTaskQueue raster_queue_;

  std::vector<scoped_refptr<RasterTask> > orphan_raster_tasks_;

  UniqueNotifier ready_to_activate_check_notifier_;

  DISALLOW_COPY_AND_ASSIGN(TileManager);
};

}  // namespace cc

#endif  // CC_RESOURCES_TILE_MANAGER_H_
