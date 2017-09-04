// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/image_manager.h"

namespace cc {

ImageManager::ImageManager() = default;
ImageManager::~ImageManager() = default;

void ImageManager::SetImageDecodeController(ImageDecodeController* controller) {
  // We can only switch from null to non-null and back.
  // CHECK to debug crbug.com/650234.
  CHECK(controller || controller_);
  CHECK(!controller || !controller_);

  if (!controller) {
    SetPredecodeImages(std::vector<DrawImage>(),
                       ImageDecodeController::TracingInfo());
  }
  controller_ = controller;
  // Debugging information for crbug.com/650234.
  ++num_times_controller_was_set_;
}

void ImageManager::GetTasksForImagesAndRef(
    std::vector<DrawImage>* images,
    std::vector<scoped_refptr<TileTask>>* tasks,
    const ImageDecodeController::TracingInfo& tracing_info) {
  DCHECK(controller_);
  for (auto it = images->begin(); it != images->end();) {
    scoped_refptr<TileTask> task;
    bool need_to_unref_when_finished =
        controller_->GetTaskForImageAndRef(*it, tracing_info, &task);
    if (task)
      tasks->push_back(std::move(task));

    if (need_to_unref_when_finished)
      ++it;
    else
      it = images->erase(it);
  }
}

void ImageManager::UnrefImages(const std::vector<DrawImage>& images) {
  // Debugging information for crbug.com/650234.
  CHECK(controller_) << num_times_controller_was_set_;
  for (auto image : images)
    controller_->UnrefImage(image);
}

void ImageManager::ReduceMemoryUsage() {
  DCHECK(controller_);
  controller_->ReduceCacheUsage();
}

std::vector<scoped_refptr<TileTask>> ImageManager::SetPredecodeImages(
    std::vector<DrawImage> images,
    const ImageDecodeController::TracingInfo& tracing_info) {
  std::vector<scoped_refptr<TileTask>> new_tasks;
  GetTasksForImagesAndRef(&images, &new_tasks, tracing_info);
  UnrefImages(predecode_locked_images_);
  predecode_locked_images_ = std::move(images);
  return new_tasks;
}

}  // namespace cc
