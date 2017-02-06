// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/memory/shared_memory.h"
#include "base/test/test_message_loop.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_test_common.h"
#include "ipc/ipc_test_sink.h"
#include "ui/gl/gl_context_stub_with_extensions.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace gpu {

static constexpr int kFakeTextureIds[] = {1, 2};

class GpuChannelTest : public GpuChannelTestCommon {
 public:
  GpuChannelTest() : GpuChannelTestCommon() {}
  ~GpuChannelTest() override {}

  void SetUp() override {
    // We need GL bindings to actually initialize command buffers.
    gl::SetGLGetProcAddressProc(gl::MockGLInterface::GetGLProcAddress);
    gl::GLSurfaceTestSupport::InitializeOneOffWithMockBindings();

    // This GLInterface is a stub for the gl driver.
    gl_interface_.reset(new testing::NiceMock<gl::MockGLInterface>);
    gl::MockGLInterface::SetGLInterface(gl_interface_.get());

    using testing::AnyNumber;
    using testing::NotNull;
    using testing::Return;
    using testing::SetArgPointee;
    using testing::SetArrayArgument;
    // We need it to return non-null strings in order for things to not crash.
    EXPECT_CALL(*gl_interface_, GetString(GL_RENDERER))
        .Times(AnyNumber())
        .WillRepeatedly(Return(reinterpret_cast<const GLubyte*>("")));
    EXPECT_CALL(*gl_interface_, GetString(GL_VERSION))
        .Times(AnyNumber())
        .WillRepeatedly(Return(reinterpret_cast<const GLubyte*>("2.0")));
    EXPECT_CALL(*gl_interface_, GetString(GL_EXTENSIONS))
        .Times(AnyNumber())
        .WillRepeatedly(Return(
            reinterpret_cast<const GLubyte*>("GL_EXT_framebuffer_object ")));
    // And we need some values to be large enough to initialize ContextGroup.
    EXPECT_CALL(*gl_interface_,
                GetIntegerv(GL_MAX_RENDERBUFFER_SIZE, NotNull()))
        .Times(AnyNumber())
        .WillRepeatedly(SetArgPointee<1>(512));
    EXPECT_CALL(*gl_interface_, GetIntegerv(GL_MAX_VERTEX_ATTRIBS, NotNull()))
        .Times(AnyNumber())
        .WillRepeatedly(SetArgPointee<1>(8));
    EXPECT_CALL(*gl_interface_,
                GetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, NotNull()))
        .Times(AnyNumber())
        .WillRepeatedly(SetArgPointee<1>(8));
    EXPECT_CALL(*gl_interface_,
                GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, NotNull()))
        .Times(AnyNumber())
        .WillRepeatedly(SetArgPointee<1>(8));
    EXPECT_CALL(*gl_interface_, GetIntegerv(GL_MAX_TEXTURE_SIZE, NotNull()))
        .Times(AnyNumber())
        .WillRepeatedly(SetArgPointee<1>(2048));
    EXPECT_CALL(*gl_interface_,
                GetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, NotNull()))
        .Times(AnyNumber())
        .WillRepeatedly(SetArgPointee<1>(2048));
    EXPECT_CALL(*gl_interface_, GetIntegerv(GL_MAX_VARYING_FLOATS, NotNull()))
        .Times(AnyNumber())
        .WillRepeatedly(SetArgPointee<1>(8 * 4));
    EXPECT_CALL(*gl_interface_,
                GetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, NotNull()))
        .Times(AnyNumber())
        .WillRepeatedly(SetArgPointee<1>(128 * 4));
    EXPECT_CALL(*gl_interface_,
                GetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, NotNull()))
        .Times(AnyNumber())
        .WillRepeatedly(SetArgPointee<1>(16 * 4));
    EXPECT_CALL(*gl_interface_,
                GetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, NotNull()))
        .Times(AnyNumber())
        .WillRepeatedly(SetArgPointee<1>(32));
    EXPECT_CALL(*gl_interface_, GetIntegerv(GL_MAX_VIEWPORT_DIMS, NotNull()))
        .Times(AnyNumber())
        .WillRepeatedly(SetArgPointee<1>(1024 << 8));
    EXPECT_CALL(*gl_interface_, GetIntegerv(GL_ALPHA_BITS, NotNull()))
        .Times(AnyNumber())
        .WillRepeatedly(SetArgPointee<1>(8));
    EXPECT_CALL(*gl_interface_, GetIntegerv(GL_DEPTH_BITS, NotNull()))
        .Times(AnyNumber())
        .WillRepeatedly(SetArgPointee<1>(24));
    EXPECT_CALL(*gl_interface_, GetIntegerv(GL_STENCIL_BITS, NotNull()))
        .Times(AnyNumber())
        .WillRepeatedly(SetArgPointee<1>(8));
    // Allow generating non-0 resources so code does not DCHECK.
    EXPECT_CALL(*gl_interface_, GenFramebuffersEXT(1, NotNull()))
        .Times(AnyNumber())
        .WillRepeatedly(SetArgPointee<1>(1));
    EXPECT_CALL(*gl_interface_, GenTextures(1, NotNull()))
        .Times(AnyNumber())
        .WillRepeatedly(
            SetArrayArgument<1>(kFakeTextureIds, kFakeTextureIds + 1));
    EXPECT_CALL(*gl_interface_, GenTextures(2, NotNull()))
        .Times(AnyNumber())
        .WillRepeatedly(
            SetArrayArgument<1>(kFakeTextureIds, kFakeTextureIds + 2));
    // And errors should be squashed.
    EXPECT_CALL(*gl_interface_, CheckFramebufferStatusEXT(GL_FRAMEBUFFER))
        .Times(AnyNumber())
        .WillRepeatedly(Return(GL_FRAMEBUFFER_COMPLETE));

    // Dynamic bindings must be set up for the GLES2DecoderImpl, which requires
    // a GLContext. Use a GLContextStub which does nothing but call through to
    // our |gl_interface| above.
    stub_context_ = new gl::GLContextStub;
    stub_surface_ = new gl::GLSurfaceStub;
    stub_context_->MakeCurrent(stub_surface_.get());
    gl::GLSurfaceTestSupport::InitializeDynamicMockBindings(
        stub_context_.get());

    GpuChannelTestCommon::SetUp();
  }

  void TearDown() override {
    GpuChannelTestCommon::TearDown();

    stub_context_ = nullptr;
    stub_surface_ = nullptr;
    gl::MockGLInterface::SetGLInterface(nullptr);
    gl::ClearGLBindings();
    gl_interface_ = nullptr;
  }

  GpuChannel* CreateChannel(int32_t client_id,
                            bool allow_view_command_buffers,
                            bool allow_real_time_streams) {
    DCHECK(channel_manager());
    uint64_t kClientTracingId = 1;
    channel_manager()->EstablishChannel(
        client_id, kClientTracingId, false /* preempts */,
        allow_view_command_buffers, allow_real_time_streams);
    return channel_manager()->LookupChannel(client_id);
  }

  void HandleMessage(GpuChannel* channel, IPC::Message* msg) {
    TestGpuChannel* test_channel = static_cast<TestGpuChannel*>(channel);

    // Some IPCs (such as GpuCommandBufferMsg_Initialize) will generate more
    // delayed responses, drop those if they exist.
    test_channel->sink()->ClearMessages();

    test_channel->HandleMessageForTesting(*msg);
    if (msg->is_sync()) {
      const IPC::Message* reply_msg = test_channel->sink()->GetMessageAt(0);
      CHECK(reply_msg);
      CHECK(!reply_msg->is_reply_error());

      CHECK(IPC::SyncMessage::IsMessageReplyTo(
          *reply_msg, IPC::SyncMessage::GetMessageId(*msg)));

      IPC::MessageReplyDeserializer* deserializer =
          static_cast<IPC::SyncMessage*>(msg)->GetReplyDeserializer();
      CHECK(deserializer);
      deserializer->SerializeOutputParameters(*reply_msg);

      delete deserializer;

      test_channel->sink()->ClearMessages();
    }

    delete msg;
  }

  base::SharedMemoryHandle GetSharedHandle() {
    base::SharedMemory shared_memory;
    shared_memory.CreateAnonymous(10);
    base::SharedMemoryHandle shmem_handle;
    shared_memory.ShareToProcess(base::GetCurrentProcessHandle(),
                                 &shmem_handle);
    return shmem_handle;
  }

 private:
  base::TestMessageLoop message_loop_;
  std::unique_ptr<gl::MockGLInterface> gl_interface_;
  scoped_refptr<gl::GLContextStub> stub_context_;
  scoped_refptr<gl::GLSurfaceStub> stub_surface_;
};

#if defined(OS_WIN)
const SurfaceHandle kFakeSurfaceHandle = reinterpret_cast<SurfaceHandle>(1);
#else
const SurfaceHandle kFakeSurfaceHandle = 1;
#endif

TEST_F(GpuChannelTest, CreateViewCommandBufferAllowed) {
  int32_t kClientId = 1;
  bool allow_view_command_buffers = true;
  GpuChannel* channel =
      CreateChannel(kClientId, allow_view_command_buffers, false);
  ASSERT_TRUE(channel);

  SurfaceHandle surface_handle = kFakeSurfaceHandle;
  DCHECK_NE(surface_handle, kNullSurfaceHandle);

  int32_t kRouteId = 1;
  GPUCreateCommandBufferConfig init_params;
  init_params.surface_handle = surface_handle;
  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.stream_id = 0;
  init_params.stream_priority = GpuStreamPriority::NORMAL;
  init_params.attribs = gles2::ContextCreationAttribHelper();
  init_params.active_url = GURL();
  bool result = false;
  gpu::Capabilities capabilities;
  HandleMessage(channel, new GpuChannelMsg_CreateCommandBuffer(
                             init_params, kRouteId, GetSharedHandle(), &result,
                             &capabilities));
  EXPECT_TRUE(result);

  GpuCommandBufferStub* stub = channel->LookupCommandBuffer(kRouteId);
  ASSERT_TRUE(stub);
}

TEST_F(GpuChannelTest, CreateViewCommandBufferDisallowed) {
  int32_t kClientId = 1;
  bool allow_view_command_buffers = false;
  GpuChannel* channel =
      CreateChannel(kClientId, allow_view_command_buffers, false);
  ASSERT_TRUE(channel);

  SurfaceHandle surface_handle = kFakeSurfaceHandle;
  DCHECK_NE(surface_handle, kNullSurfaceHandle);

  int32_t kRouteId = 1;
  GPUCreateCommandBufferConfig init_params;
  init_params.surface_handle = surface_handle;
  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.stream_id = 0;
  init_params.stream_priority = GpuStreamPriority::NORMAL;
  init_params.attribs = gles2::ContextCreationAttribHelper();
  init_params.active_url = GURL();
  bool result = false;
  gpu::Capabilities capabilities;
  HandleMessage(channel, new GpuChannelMsg_CreateCommandBuffer(
                             init_params, kRouteId, GetSharedHandle(), &result,
                             &capabilities));
  EXPECT_FALSE(result);

  GpuCommandBufferStub* stub = channel->LookupCommandBuffer(kRouteId);
  EXPECT_FALSE(stub);
}

TEST_F(GpuChannelTest, CreateOffscreenCommandBuffer) {
  int32_t kClientId = 1;
  GpuChannel* channel = CreateChannel(kClientId, true, false);
  ASSERT_TRUE(channel);

  int32_t kRouteId = 1;
  GPUCreateCommandBufferConfig init_params;
  init_params.surface_handle = kNullSurfaceHandle;
  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.stream_id = 0;
  init_params.stream_priority = GpuStreamPriority::NORMAL;
  init_params.attribs = gles2::ContextCreationAttribHelper();
  init_params.active_url = GURL();
  bool result = false;
  gpu::Capabilities capabilities;
  HandleMessage(channel, new GpuChannelMsg_CreateCommandBuffer(
                             init_params, kRouteId, GetSharedHandle(), &result,
                             &capabilities));
  EXPECT_TRUE(result);

  GpuCommandBufferStub* stub = channel->LookupCommandBuffer(kRouteId);
  EXPECT_TRUE(stub);
}

TEST_F(GpuChannelTest, IncompatibleStreamIds) {
  int32_t kClientId = 1;
  GpuChannel* channel = CreateChannel(kClientId, true, false);
  ASSERT_TRUE(channel);

  // Create first context.
  int32_t kRouteId1 = 1;
  int32_t kStreamId1 = 1;
  GPUCreateCommandBufferConfig init_params;
  init_params.surface_handle = kNullSurfaceHandle;
  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.stream_id = kStreamId1;
  init_params.stream_priority = GpuStreamPriority::NORMAL;
  init_params.attribs = gles2::ContextCreationAttribHelper();
  init_params.active_url = GURL();
  bool result = false;
  gpu::Capabilities capabilities;
  HandleMessage(channel, new GpuChannelMsg_CreateCommandBuffer(
                             init_params, kRouteId1, GetSharedHandle(), &result,
                             &capabilities));
  EXPECT_TRUE(result);

  GpuCommandBufferStub* stub = channel->LookupCommandBuffer(kRouteId1);
  EXPECT_TRUE(stub);

  // Create second context in same share group but different stream.
  int32_t kRouteId2 = 2;
  int32_t kStreamId2 = 2;

  init_params.share_group_id = kRouteId1;
  init_params.stream_id = kStreamId2;
  init_params.stream_priority = GpuStreamPriority::NORMAL;
  init_params.attribs = gles2::ContextCreationAttribHelper();
  init_params.active_url = GURL();
  HandleMessage(channel, new GpuChannelMsg_CreateCommandBuffer(
                             init_params, kRouteId2, GetSharedHandle(), &result,
                             &capabilities));
  EXPECT_FALSE(result);

  stub = channel->LookupCommandBuffer(kRouteId2);
  EXPECT_FALSE(stub);
}

TEST_F(GpuChannelTest, StreamLifetime) {
  int32_t kClientId = 1;
  GpuChannel* channel = CreateChannel(kClientId, true, false);
  ASSERT_TRUE(channel);

  // Create first context.
  int32_t kRouteId1 = 1;
  int32_t kStreamId1 = 1;
  GpuStreamPriority kStreamPriority1 = GpuStreamPriority::NORMAL;
  GPUCreateCommandBufferConfig init_params;
  init_params.surface_handle = kNullSurfaceHandle;
  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.stream_id = kStreamId1;
  init_params.stream_priority = kStreamPriority1;
  init_params.attribs = gles2::ContextCreationAttribHelper();
  init_params.active_url = GURL();
  bool result = false;
  gpu::Capabilities capabilities;
  HandleMessage(channel, new GpuChannelMsg_CreateCommandBuffer(
                             init_params, kRouteId1, GetSharedHandle(), &result,
                             &capabilities));
  EXPECT_TRUE(result);

  GpuCommandBufferStub* stub = channel->LookupCommandBuffer(kRouteId1);
  EXPECT_TRUE(stub);

  HandleMessage(channel, new GpuChannelMsg_DestroyCommandBuffer(kRouteId1));
  stub = channel->LookupCommandBuffer(kRouteId1);
  EXPECT_FALSE(stub);

  // Create second context in same share group but different stream.
  int32_t kRouteId2 = 2;
  int32_t kStreamId2 = 2;
  GpuStreamPriority kStreamPriority2 = GpuStreamPriority::LOW;

  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.stream_id = kStreamId2;
  init_params.stream_priority = kStreamPriority2;
  init_params.attribs = gles2::ContextCreationAttribHelper();
  init_params.active_url = GURL();
  HandleMessage(channel, new GpuChannelMsg_CreateCommandBuffer(
                             init_params, kRouteId2, GetSharedHandle(), &result,
                             &capabilities));
  EXPECT_TRUE(result);

  stub = channel->LookupCommandBuffer(kRouteId2);
  EXPECT_TRUE(stub);
}

TEST_F(GpuChannelTest, RealTimeStreamsDisallowed) {
  int32_t kClientId = 1;
  bool allow_real_time_streams = false;
  GpuChannel* channel = CreateChannel(kClientId, true, allow_real_time_streams);
  ASSERT_TRUE(channel);

  // Create first context.
  int32_t kRouteId = 1;
  int32_t kStreamId = 1;
  GpuStreamPriority kStreamPriority = GpuStreamPriority::REAL_TIME;
  GPUCreateCommandBufferConfig init_params;
  init_params.surface_handle = kNullSurfaceHandle;
  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.stream_id = kStreamId;
  init_params.stream_priority = kStreamPriority;
  init_params.attribs = gles2::ContextCreationAttribHelper();
  init_params.active_url = GURL();
  bool result = false;
  gpu::Capabilities capabilities;
  HandleMessage(channel, new GpuChannelMsg_CreateCommandBuffer(
                             init_params, kRouteId, GetSharedHandle(), &result,
                             &capabilities));
  EXPECT_FALSE(result);

  GpuCommandBufferStub* stub = channel->LookupCommandBuffer(kRouteId);
  EXPECT_FALSE(stub);
}

TEST_F(GpuChannelTest, RealTimeStreamsAllowed) {
  int32_t kClientId = 1;
  bool allow_real_time_streams = true;
  GpuChannel* channel = CreateChannel(kClientId, true, allow_real_time_streams);
  ASSERT_TRUE(channel);

  // Create first context.
  int32_t kRouteId = 1;
  int32_t kStreamId = 1;
  GpuStreamPriority kStreamPriority = GpuStreamPriority::REAL_TIME;
  GPUCreateCommandBufferConfig init_params;
  init_params.surface_handle = kNullSurfaceHandle;
  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.stream_id = kStreamId;
  init_params.stream_priority = kStreamPriority;
  init_params.attribs = gles2::ContextCreationAttribHelper();
  init_params.active_url = GURL();
  bool result = false;
  gpu::Capabilities capabilities;
  HandleMessage(channel, new GpuChannelMsg_CreateCommandBuffer(
                             init_params, kRouteId, GetSharedHandle(), &result,
                             &capabilities));
  EXPECT_TRUE(result);

  GpuCommandBufferStub* stub = channel->LookupCommandBuffer(kRouteId);
  EXPECT_TRUE(stub);
}

TEST_F(GpuChannelTest, CreateFailsIfSharedContextIsLost) {
  int32_t kClientId = 1;
  GpuChannel* channel = CreateChannel(kClientId, false, false);
  ASSERT_TRUE(channel);

  // Create first context, we will share this one.
  int32_t kSharedRouteId = 1;
  {
    SCOPED_TRACE("kSharedRouteId");
    GPUCreateCommandBufferConfig init_params;
    init_params.surface_handle = kNullSurfaceHandle;
    init_params.share_group_id = MSG_ROUTING_NONE;
    init_params.stream_id = 0;
    init_params.stream_priority = GpuStreamPriority::NORMAL;
    init_params.attribs = gles2::ContextCreationAttribHelper();
    init_params.active_url = GURL();
    bool result = false;
    gpu::Capabilities capabilities;
    HandleMessage(channel, new GpuChannelMsg_CreateCommandBuffer(
                               init_params, kSharedRouteId, GetSharedHandle(),
                               &result, &capabilities));
    EXPECT_TRUE(result);
  }
  EXPECT_TRUE(channel->LookupCommandBuffer(kSharedRouteId));

  // This context shares with the first one, this should be possible.
  int32_t kFriendlyRouteId = 2;
  {
    SCOPED_TRACE("kFriendlyRouteId");
    GPUCreateCommandBufferConfig init_params;
    init_params.surface_handle = kNullSurfaceHandle;
    init_params.share_group_id = kSharedRouteId;
    init_params.stream_id = 0;
    init_params.stream_priority = GpuStreamPriority::NORMAL;
    init_params.attribs = gles2::ContextCreationAttribHelper();
    init_params.active_url = GURL();
    bool result = false;
    gpu::Capabilities capabilities;
    HandleMessage(channel, new GpuChannelMsg_CreateCommandBuffer(
                               init_params, kFriendlyRouteId, GetSharedHandle(),
                               &result, &capabilities));
    EXPECT_TRUE(result);
  }
  EXPECT_TRUE(channel->LookupCommandBuffer(kFriendlyRouteId));

  // The shared context is lost.
  channel->LookupCommandBuffer(kSharedRouteId)->MarkContextLost();

  // Meanwhile another context is being made pointing to the shared one. This
  // should fail.
  int32_t kAnotherRouteId = 3;
  {
    SCOPED_TRACE("kAnotherRouteId");
    GPUCreateCommandBufferConfig init_params;
    init_params.surface_handle = kNullSurfaceHandle;
    init_params.share_group_id = kSharedRouteId;
    init_params.stream_id = 0;
    init_params.stream_priority = GpuStreamPriority::NORMAL;
    init_params.attribs = gles2::ContextCreationAttribHelper();
    init_params.active_url = GURL();
    bool result = false;
    gpu::Capabilities capabilities;
    HandleMessage(channel, new GpuChannelMsg_CreateCommandBuffer(
                               init_params, kAnotherRouteId, GetSharedHandle(),
                               &result, &capabilities));
    EXPECT_FALSE(result);
  }
  EXPECT_FALSE(channel->LookupCommandBuffer(kAnotherRouteId));

  // The lost context is still around though (to verify the failure happened due
  // to the shared context being lost, not due to it being deleted).
  EXPECT_TRUE(channel->LookupCommandBuffer(kSharedRouteId));

  // Destroy the command buffers we initialized before destoying GL.
  HandleMessage(channel,
                new GpuChannelMsg_DestroyCommandBuffer(kFriendlyRouteId));
  HandleMessage(channel,
                new GpuChannelMsg_DestroyCommandBuffer(kSharedRouteId));
}

}  // namespace gpu
