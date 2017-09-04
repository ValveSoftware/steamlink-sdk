// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/leak_detector/custom_allocator.h"

#include <stddef.h>

namespace metrics {
namespace leak_detector {

namespace {

// Wrappers around new and delete.
void* DefaultAlloc(size_t size) {
  return new char[size];
}
void DefaultFree(void* ptr, size_t /* size */) {
  delete[] reinterpret_cast<char*>(ptr);
}

CustomAllocator::AllocFunc g_alloc_func = nullptr;
CustomAllocator::FreeFunc g_free_func = nullptr;

}  // namespace

// static
void CustomAllocator::Initialize() {
  Initialize(&DefaultAlloc, &DefaultFree);
}

// static
void CustomAllocator::Initialize(AllocFunc alloc_func, FreeFunc free_func) {
  g_alloc_func = alloc_func;
  g_free_func = free_func;
}

// static
bool CustomAllocator::Shutdown() {
  g_alloc_func = nullptr;
  g_free_func = nullptr;
  return true;
}

// static
bool CustomAllocator::IsInitialized() {
  return g_alloc_func && g_free_func;
}

// static
void* CustomAllocator::Allocate(size_t size) {
  return g_alloc_func(size);
}

// static
void CustomAllocator::Free(void* ptr, size_t size) {
  g_free_func(ptr, size);
}

}  // namespace leak_detector
}  // namespace metrics
