// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_IMAGE_DECODE_CONTROLLER_H_
#define CC_TILES_IMAGE_DECODE_CONTROLLER_H_

#include "base/memory/ref_counted.h"
#include "cc/playback/decoded_draw_image.h"
#include "cc/playback/draw_image.h"
#include "cc/tiles/tile_priority.h"

namespace cc {

class TileTask;

// ImageDecodeController is responsible for generating decode tasks, decoding
// images, storing images in cache, and being able to return the decoded images
// when requested.

// ImageDecodeController is responsible for the following things:
// 1. Given a DrawImage, it can return an TileTask which when run will
//    decode and cache the resulting image. If the image does not need a task to
//    be decoded, then nullptr will be returned. The return value of the
//    function indicates whether the image was or is going to be locked, so an
//    unlock will be required.
// 2. Given a cache key and a DrawImage, it can decode the image and store it in
//    the cache. Note that it is important that this function is only accessed
//    via an image decode task.
// 3. Given a DrawImage, it can return a DecodedDrawImage, which represented the
//    decoded version of the image. Note that if the image is not in the cache
//    and it needs to be scaled/decoded, then this decode will happen as part of
//    getting the image. As such, this should only be accessed from a raster
//    thread.
class CC_EXPORT ImageDecodeController {
 public:
  // This information should be used strictly in tracing, UMA, and any other
  // reporting systems.
  struct TracingInfo {
    TracingInfo(uint64_t prepare_tiles_id,
                TilePriority::PriorityBin requesting_tile_bin)
        : prepare_tiles_id(prepare_tiles_id),
          requesting_tile_bin(requesting_tile_bin) {}
    TracingInfo() : TracingInfo(0, TilePriority::NOW) {}

    // ID for the current prepare tiles call.
    const uint64_t prepare_tiles_id;

    // The bin of the tile that caused this image to be requested.
    const TilePriority::PriorityBin requesting_tile_bin;
  };

  virtual ~ImageDecodeController() {}

  // Fill in an TileTask which will decode the given image when run. In
  // case the image is already cached, fills in nullptr. Returns true if the
  // image needs to be unreffed when the caller is finished with it.
  //
  // This is called by the tile manager (on the compositor thread) when creating
  // a raster task.
  virtual bool GetTaskForImageAndRef(const DrawImage& image,
                                     const TracingInfo& tracing_info,
                                     scoped_refptr<TileTask>* task) = 0;
  // Unrefs an image. When the tile is finished, this should be called for every
  // GetTaskForImageAndRef call that returned true.
  virtual void UnrefImage(const DrawImage& image) = 0;

  // Returns a decoded draw image. This may cause a decode if the image was not
  // predecoded.
  //
  // This is called by a raster task (on a worker thread) when an image is
  // required.
  virtual DecodedDrawImage GetDecodedImageForDraw(const DrawImage& image) = 0;
  // Unrefs an image. This should be called for every GetDecodedImageForDraw
  // when the draw with the image is finished.
  virtual void DrawWithImageFinished(const DrawImage& image,
                                     const DecodedDrawImage& decoded_image) = 0;

  // This function informs the controller that now is a good time to clean up
  // memory. This is called periodically from the compositor thread.
  virtual void ReduceCacheUsage() = 0;

  // This function informs the controller that we are hidden and should not be
  // retaining cached resources longer than needed.
  virtual void SetShouldAggressivelyFreeResources(
      bool aggressively_free_resources) = 0;
};

}  // namespace cc

#endif  // CC_TILES_IMAGE_DECODE_CONTROLLER_H_
