// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CompositorMutableState.h"

#include "base/message_loop/message_loop.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "platform/graphics/CompositorElementId.h"
#include "platform/graphics/CompositorMutableProperties.h"
#include "platform/graphics/CompositorMutableStateProvider.h"
#include "platform/graphics/CompositorMutation.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

using cc::FakeImplTaskRunnerProvider;
using cc::FakeLayerTreeHostImpl;
using cc::FakeOutputSurface;
using cc::LayerImpl;
using cc::LayerTreeSettings;
using cc::OutputSurface;
using cc::TestTaskGraphRunner;
using cc::TestSharedBitmapManager;

class CompositorMutableStateTest : public testing::Test {
public:
    CompositorMutableStateTest()
        : m_outputSurface(FakeOutputSurface::Create3d())
    {
        LayerTreeSettings settings;
        settings.layer_transforms_should_scale_layer_contents = true;
        m_hostImpl.reset(new FakeLayerTreeHostImpl(settings, &m_taskRunnerProvider, &m_sharedBitmapManager, &m_taskGraphRunner));
        m_hostImpl->SetVisible(true);
        EXPECT_TRUE(m_hostImpl->InitializeRenderer(m_outputSurface.get()));
    }

    void SetLayerPropertiesForTesting(LayerImpl* layer)
    {
        layer->SetTransform(gfx::Transform());
        layer->SetPosition(gfx::PointF());
        layer->SetBounds(gfx::Size(100, 100));
        layer->Set3dSortingContextId(0);
        layer->SetDrawsContent(true);
    }

    FakeLayerTreeHostImpl& hostImpl() { return *m_hostImpl; }

    LayerImpl* rootLayer() { return m_hostImpl->active_tree()->root_layer_for_testing(); }

private:
    // The cc testing machinery has fairly deep dependency on having a main
    // message loop (one example is the task runner provider). We construct one
    // here so that it's installed in TLA and can be found by other cc classes.
    base::MessageLoop m_messageLoop;
    TestSharedBitmapManager m_sharedBitmapManager;
    TestTaskGraphRunner m_taskGraphRunner;
    FakeImplTaskRunnerProvider m_taskRunnerProvider;
    std::unique_ptr<OutputSurface> m_outputSurface;
    std::unique_ptr<FakeLayerTreeHostImpl> m_hostImpl;
};

TEST_F(CompositorMutableStateTest, NoMutableState)
{
    // In this test, there are no layers with either an element id or mutable
    // properties. We should not be able to get any mutable state.
    std::unique_ptr<LayerImpl> root = LayerImpl::Create(hostImpl().active_tree(), 42);
    SetLayerPropertiesForTesting(root.get());

    hostImpl().SetViewportSize(root->bounds());
    hostImpl().active_tree()->SetRootLayerForTesting(std::move(root));
    hostImpl().UpdateNumChildrenAndDrawPropertiesForActiveTree();

    CompositorMutations mutations;
    CompositorMutableStateProvider provider(hostImpl().active_tree(), &mutations);
    std::unique_ptr<CompositorMutableState> state(provider.getMutableStateFor(42));
    EXPECT_FALSE(state);
}

TEST_F(CompositorMutableStateTest, MutableStateMutableProperties)
{
    // In this test, there is a layer with an element id and mutable properties.
    // In this case, we should get a valid mutable state for this element id that
    // has a real effect on the corresponding layer.
    std::unique_ptr<LayerImpl> root = LayerImpl::Create(hostImpl().active_tree(), 42);

    std::unique_ptr<LayerImpl> scopedLayer =
        LayerImpl::Create(hostImpl().active_tree(), 11);
    LayerImpl* layer = scopedLayer.get();
    layer->SetScrollClipLayer(root->id());

    root->test_properties()->AddChild(std::move(scopedLayer));

    SetLayerPropertiesForTesting(layer);

    int primaryId = 12;
    root->SetElementId(createCompositorElementId(primaryId, CompositorSubElementId::Primary));
    layer->SetElementId(createCompositorElementId(primaryId, CompositorSubElementId::Scroll));

    root->SetMutableProperties(CompositorMutableProperty::kOpacity | CompositorMutableProperty::kTransform);
    layer->SetMutableProperties(CompositorMutableProperty::kScrollLeft | CompositorMutableProperty::kScrollTop);

    hostImpl().SetViewportSize(layer->bounds());
    hostImpl().active_tree()->SetRootLayerForTesting(std::move(root));
    hostImpl().UpdateNumChildrenAndDrawPropertiesForActiveTree();

    CompositorMutations mutations;
    CompositorMutableStateProvider provider(hostImpl().active_tree(), &mutations);

    std::unique_ptr<CompositorMutableState> state(provider.getMutableStateFor(primaryId));
    EXPECT_TRUE(state.get());

    EXPECT_EQ(1.0, rootLayer()->Opacity());
    EXPECT_EQ(gfx::Transform().ToString(), rootLayer()->transform().ToString());
    EXPECT_EQ(0.0, layer->CurrentScrollOffset().x());
    EXPECT_EQ(0.0, layer->CurrentScrollOffset().y());

    gfx::Transform zero(0, 0, 0, 0, 0, 0);
    state->setOpacity(0.5);
    state->setTransform(zero.matrix());
    state->setScrollLeft(1.0);
    state->setScrollTop(1.0);

    EXPECT_EQ(0.5, rootLayer()->Opacity());
    EXPECT_EQ(zero.ToString(), rootLayer()->transform().ToString());
    EXPECT_EQ(1.0, layer->CurrentScrollOffset().x());
    EXPECT_EQ(1.0, layer->CurrentScrollOffset().y());

    // The corresponding mutation should reflect the changed values.
    EXPECT_EQ(1ul, mutations.map.size());

    const CompositorMutation& mutation = *mutations.map.find(primaryId)->value;
    EXPECT_TRUE(mutation.isOpacityMutated());
    EXPECT_TRUE(mutation.isTransformMutated());
    EXPECT_TRUE(mutation.isScrollLeftMutated());
    EXPECT_TRUE(mutation.isScrollTopMutated());

    EXPECT_EQ(0.5, mutation.opacity());
    EXPECT_EQ(zero.ToString(), gfx::Transform(mutation.transform()).ToString());
    EXPECT_EQ(1.0, mutation.scrollLeft());
    EXPECT_EQ(1.0, mutation.scrollTop());
}

} // namespace blink
