// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/scoped_resource.h"

#include "cc/output/renderer.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/tiled_layer_test_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(ScopedResourceTest, NewScopedResource) {
  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d());
  CHECK(output_surface->BindToClient(&output_surface_client));

  scoped_ptr<SharedBitmapManager> shared_bitmap_manager(
      new TestSharedBitmapManager());
  scoped_ptr<ResourceProvider> resource_provider(ResourceProvider::Create(
      output_surface.get(), shared_bitmap_manager.get(), 0, false, 1, false));
  scoped_ptr<ScopedResource> texture =
      ScopedResource::Create(resource_provider.get());

  // New scoped textures do not hold a texture yet.
  EXPECT_EQ(0u, texture->id());

  // New scoped textures do not have a size yet.
  EXPECT_EQ(gfx::Size(), texture->size());
  EXPECT_EQ(0u, texture->bytes());
}

TEST(ScopedResourceTest, CreateScopedResource) {
  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d());
  CHECK(output_surface->BindToClient(&output_surface_client));

  scoped_ptr<SharedBitmapManager> shared_bitmap_manager(
      new TestSharedBitmapManager());
  scoped_ptr<ResourceProvider> resource_provider(ResourceProvider::Create(
      output_surface.get(), shared_bitmap_manager.get(), 0, false, 1, false));
  scoped_ptr<ScopedResource> texture =
      ScopedResource::Create(resource_provider.get());
  texture->Allocate(gfx::Size(30, 30),
                    ResourceProvider::TextureUsageAny,
                    RGBA_8888);

  // The texture has an allocated byte-size now.
  size_t expected_bytes = 30 * 30 * 4;
  EXPECT_EQ(expected_bytes, texture->bytes());

  EXPECT_LT(0u, texture->id());
  EXPECT_EQ(static_cast<unsigned>(RGBA_8888), texture->format());
  EXPECT_EQ(gfx::Size(30, 30), texture->size());
}

TEST(ScopedResourceTest, ScopedResourceIsDeleted) {
  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d());
  CHECK(output_surface->BindToClient(&output_surface_client));

  scoped_ptr<SharedBitmapManager> shared_bitmap_manager(
      new TestSharedBitmapManager());
  scoped_ptr<ResourceProvider> resource_provider(ResourceProvider::Create(
      output_surface.get(), shared_bitmap_manager.get(), 0, false, 1, false));
  {
    scoped_ptr<ScopedResource> texture =
        ScopedResource::Create(resource_provider.get());

    EXPECT_EQ(0u, resource_provider->num_resources());
    texture->Allocate(gfx::Size(30, 30),
                      ResourceProvider::TextureUsageAny,
                      RGBA_8888);
    EXPECT_LT(0u, texture->id());
    EXPECT_EQ(1u, resource_provider->num_resources());
  }

  EXPECT_EQ(0u, resource_provider->num_resources());
  {
    scoped_ptr<ScopedResource> texture =
        ScopedResource::Create(resource_provider.get());
    EXPECT_EQ(0u, resource_provider->num_resources());
    texture->Allocate(gfx::Size(30, 30),
                      ResourceProvider::TextureUsageAny,
                      RGBA_8888);
    EXPECT_LT(0u, texture->id());
    EXPECT_EQ(1u, resource_provider->num_resources());
    texture->Free();
    EXPECT_EQ(0u, resource_provider->num_resources());
  }
}

TEST(ScopedResourceTest, LeakScopedResource) {
  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d());
  CHECK(output_surface->BindToClient(&output_surface_client));

  scoped_ptr<SharedBitmapManager> shared_bitmap_manager(
      new TestSharedBitmapManager());
  scoped_ptr<ResourceProvider> resource_provider(ResourceProvider::Create(
      output_surface.get(), shared_bitmap_manager.get(), 0, false, 1, false));
  {
    scoped_ptr<ScopedResource> texture =
        ScopedResource::Create(resource_provider.get());

    EXPECT_EQ(0u, resource_provider->num_resources());
    texture->Allocate(gfx::Size(30, 30),
                      ResourceProvider::TextureUsageAny,
                      RGBA_8888);
    EXPECT_LT(0u, texture->id());
    EXPECT_EQ(1u, resource_provider->num_resources());

    texture->Leak();
    EXPECT_EQ(0u, texture->id());
    EXPECT_EQ(1u, resource_provider->num_resources());

    texture->Free();
    EXPECT_EQ(0u, texture->id());
    EXPECT_EQ(1u, resource_provider->num_resources());
  }

  EXPECT_EQ(1u, resource_provider->num_resources());
}

}  // namespace
}  // namespace cc
