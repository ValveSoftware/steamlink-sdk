// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/delegating_renderer.h"
#include "cc/output/gl_renderer.h"
#include "cc/output/output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_renderer_client.h"
#include "cc/test/test_context_provider.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class MockContextProvider : public TestContextProvider {
 public:
  explicit MockContextProvider(scoped_ptr<TestWebGraphicsContext3D> context)
      : TestContextProvider(context.Pass()) {}
  MOCK_METHOD0(DeleteCachedResources, void());

 protected:
  ~MockContextProvider() {}
};

template <class T>
scoped_ptr<Renderer> CreateRenderer(RendererClient* client,
                                    const LayerTreeSettings* settings,
                                    OutputSurface* output_surface,
                                    ResourceProvider* resource_provider);

template <>
scoped_ptr<Renderer> CreateRenderer<DelegatingRenderer>(
    RendererClient* client,
    const LayerTreeSettings* settings,
    OutputSurface* output_surface,
    ResourceProvider* resource_provider) {
  return DelegatingRenderer::Create(
             client, settings, output_surface, resource_provider)
      .PassAs<Renderer>();
}

template <>
scoped_ptr<Renderer> CreateRenderer<GLRenderer>(
    RendererClient* client,
    const LayerTreeSettings* settings,
    OutputSurface* output_surface,
    ResourceProvider* resource_provider) {
  return GLRenderer::Create(
             client, settings, output_surface, resource_provider, NULL, 0)
      .PassAs<Renderer>();
}

template <typename T>
class RendererTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    context_provider_ =
        new MockContextProvider(TestWebGraphicsContext3D::Create());
    output_surface_.reset(new OutputSurface(context_provider_));
    output_surface_->BindToClient(&output_surface_client_);
    resource_provider_ =
        ResourceProvider::Create(output_surface_.get(), NULL, 0, false, 1,
        false);
    renderer_ = CreateRenderer<T>(&renderer_client_,
                                  &tree_settings_,
                                  output_surface_.get(),
                                  resource_provider_.get());
  }

  FakeRendererClient renderer_client_;
  LayerTreeSettings tree_settings_;
  FakeOutputSurfaceClient output_surface_client_;
  scoped_refptr<MockContextProvider> context_provider_;
  scoped_ptr<OutputSurface> output_surface_;
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<Renderer> renderer_;
};

typedef ::testing::Types<DelegatingRenderer, GLRenderer> RendererTypes;
TYPED_TEST_CASE(RendererTest, RendererTypes);

TYPED_TEST(RendererTest, ContextPurgedWhenRendererBecomesInvisible) {
  EXPECT_CALL(*(this->context_provider_), DeleteCachedResources()).Times(1);

  EXPECT_TRUE(this->renderer_->visible());
  this->renderer_->SetVisible(false);
  EXPECT_FALSE(this->renderer_->visible());
}

}  // namespace
}  // namespace cc
