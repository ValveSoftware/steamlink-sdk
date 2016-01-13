// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/resource_provider.h"

#include <algorithm>
#include <map>
#include <set>

#include "base/bind.h"
#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "cc/base/scoped_ptr_deque.h"
#include "cc/output/output_surface.h"
#include "cc/resources/returned_resource.h"
#include "cc/resources/shared_bitmap_manager.h"
#include "cc/resources/single_release_callback.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_texture.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gfx/rect.h"

using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::SetArgPointee;
using testing::StrictMock;
using testing::_;

namespace cc {
namespace {

static void EmptyReleaseCallback(uint32 sync_point, bool lost_resource) {}

static void SharedMemoryReleaseCallback(scoped_ptr<base::SharedMemory> memory,
                                        uint32 sync_point,
                                        bool lost_resource) {}

static void ReleaseTextureMailbox(uint32* release_sync_point,
                                  bool* release_lost_resource,
                                  uint32 sync_point,
                                  bool lost_resource) {
  *release_sync_point = sync_point;
  *release_lost_resource = lost_resource;
}

static void ReleaseSharedMemoryCallback(
    scoped_ptr<base::SharedMemory> shared_memory,
    bool* release_called,
    uint32* release_sync_point,
    bool* lost_resource_result,
    uint32 sync_point,
    bool lost_resource) {
  *release_called = true;
  *release_sync_point = sync_point;
  *lost_resource_result = lost_resource;
}

static scoped_ptr<base::SharedMemory> CreateAndFillSharedMemory(
    const gfx::Size& size,
    uint32_t value) {
  scoped_ptr<base::SharedMemory> shared_memory(new base::SharedMemory);
  CHECK(shared_memory->CreateAndMapAnonymous(4 * size.GetArea()));
  uint32_t* pixels = reinterpret_cast<uint32_t*>(shared_memory->memory());
  CHECK(pixels);
  std::fill_n(pixels, size.GetArea(), value);
  return shared_memory.Pass();
}

class TextureStateTrackingContext : public TestWebGraphicsContext3D {
 public:
  MOCK_METHOD2(bindTexture, void(GLenum target, GLuint texture));
  MOCK_METHOD3(texParameteri, void(GLenum target, GLenum pname, GLint param));
  MOCK_METHOD1(waitSyncPoint, void(GLuint sync_point));
  MOCK_METHOD0(insertSyncPoint, GLuint(void));
  MOCK_METHOD2(produceTextureCHROMIUM,
               void(GLenum target, const GLbyte* mailbox));
  MOCK_METHOD2(consumeTextureCHROMIUM,
               void(GLenum target, const GLbyte* mailbox));

  // Force all textures to be consecutive numbers starting at "1",
  // so we easily can test for them.
  virtual GLuint NextTextureId() OVERRIDE {
    base::AutoLock lock(namespace_->lock);
    return namespace_->next_texture_id++;
  }
  virtual void RetireTextureId(GLuint) OVERRIDE {}
};

// Shared data between multiple ResourceProviderContext. This contains mailbox
// contents as well as information about sync points.
class ContextSharedData {
 public:
  static scoped_ptr<ContextSharedData> Create() {
    return make_scoped_ptr(new ContextSharedData());
  }

  uint32 InsertSyncPoint() { return next_sync_point_++; }

  void GenMailbox(GLbyte* mailbox) {
    memset(mailbox, 0, GL_MAILBOX_SIZE_CHROMIUM);
    memcpy(mailbox, &next_mailbox_, sizeof(next_mailbox_));
    ++next_mailbox_;
  }

  void ProduceTexture(const GLbyte* mailbox_name,
                      uint32 sync_point,
                      scoped_refptr<TestTexture> texture) {
    unsigned mailbox = 0;
    memcpy(&mailbox, mailbox_name, sizeof(mailbox));
    ASSERT_TRUE(mailbox && mailbox < next_mailbox_);
    textures_[mailbox] = texture;
    ASSERT_LT(sync_point_for_mailbox_[mailbox], sync_point);
    sync_point_for_mailbox_[mailbox] = sync_point;
  }

  scoped_refptr<TestTexture> ConsumeTexture(const GLbyte* mailbox_name,
                                            uint32 sync_point) {
    unsigned mailbox = 0;
    memcpy(&mailbox, mailbox_name, sizeof(mailbox));
    DCHECK(mailbox && mailbox < next_mailbox_);

    // If the latest sync point the context has waited on is before the sync
    // point for when the mailbox was set, pretend we never saw that
    // ProduceTexture.
    if (sync_point_for_mailbox_[mailbox] > sync_point) {
      NOTREACHED();
      return scoped_refptr<TestTexture>();
    }
    return textures_[mailbox];
  }

 private:
  ContextSharedData() : next_sync_point_(1), next_mailbox_(1) {}

  uint32 next_sync_point_;
  unsigned next_mailbox_;
  typedef base::hash_map<unsigned, scoped_refptr<TestTexture> > TextureMap;
  TextureMap textures_;
  base::hash_map<unsigned, uint32> sync_point_for_mailbox_;
};

class ResourceProviderContext : public TestWebGraphicsContext3D {
 public:
  static scoped_ptr<ResourceProviderContext> Create(
      ContextSharedData* shared_data) {
    return make_scoped_ptr(new ResourceProviderContext(shared_data));
  }

  virtual GLuint insertSyncPoint() OVERRIDE {
    uint32 sync_point = shared_data_->InsertSyncPoint();
    // Commit the produceTextureCHROMIUM calls at this point, so that
    // they're associated with the sync point.
    for (PendingProduceTextureList::iterator it =
             pending_produce_textures_.begin();
         it != pending_produce_textures_.end();
         ++it) {
      shared_data_->ProduceTexture(
          (*it)->mailbox, sync_point, (*it)->texture);
    }
    pending_produce_textures_.clear();
    return sync_point;
  }

  virtual void waitSyncPoint(GLuint sync_point) OVERRIDE {
    last_waited_sync_point_ = std::max(sync_point, last_waited_sync_point_);
  }

  unsigned last_waited_sync_point() const { return last_waited_sync_point_; }

  virtual void texStorage2DEXT(GLenum target,
                               GLint levels,
                               GLuint internalformat,
                               GLint width,
                               GLint height) OVERRIDE {
    CheckTextureIsBound(target);
    ASSERT_EQ(static_cast<unsigned>(GL_TEXTURE_2D), target);
    ASSERT_EQ(1, levels);
    GLenum format = GL_RGBA;
    switch (internalformat) {
      case GL_RGBA8_OES:
        break;
      case GL_BGRA8_EXT:
        format = GL_BGRA_EXT;
        break;
      default:
        NOTREACHED();
    }
    AllocateTexture(gfx::Size(width, height), format);
  }

  virtual void texImage2D(GLenum target,
                          GLint level,
                          GLenum internalformat,
                          GLsizei width,
                          GLsizei height,
                          GLint border,
                          GLenum format,
                          GLenum type,
                          const void* pixels) OVERRIDE {
    CheckTextureIsBound(target);
    ASSERT_EQ(static_cast<unsigned>(GL_TEXTURE_2D), target);
    ASSERT_FALSE(level);
    ASSERT_EQ(internalformat, format);
    ASSERT_FALSE(border);
    ASSERT_EQ(static_cast<unsigned>(GL_UNSIGNED_BYTE), type);
    AllocateTexture(gfx::Size(width, height), format);
    if (pixels)
      SetPixels(0, 0, width, height, pixels);
  }

  virtual void texSubImage2D(GLenum target,
                             GLint level,
                             GLint xoffset,
                             GLint yoffset,
                             GLsizei width,
                             GLsizei height,
                             GLenum format,
                             GLenum type,
                             const void* pixels) OVERRIDE {
    CheckTextureIsBound(target);
    ASSERT_EQ(static_cast<unsigned>(GL_TEXTURE_2D), target);
    ASSERT_FALSE(level);
    ASSERT_EQ(static_cast<unsigned>(GL_UNSIGNED_BYTE), type);
    {
      base::AutoLock lock_for_texture_access(namespace_->lock);
      ASSERT_EQ(GLDataFormat(BoundTexture(target)->format), format);
    }
    ASSERT_TRUE(pixels);
    SetPixels(xoffset, yoffset, width, height, pixels);
  }

  virtual void genMailboxCHROMIUM(GLbyte* mailbox) OVERRIDE {
    return shared_data_->GenMailbox(mailbox);
  }

  virtual void produceTextureCHROMIUM(GLenum target,
                                      const GLbyte* mailbox) OVERRIDE {
    CheckTextureIsBound(target);

    // Delay moving the texture into the mailbox until the next
    // InsertSyncPoint, so that it is not visible to other contexts that
    // haven't waited on that sync point.
    scoped_ptr<PendingProduceTexture> pending(new PendingProduceTexture);
    memcpy(pending->mailbox, mailbox, sizeof(pending->mailbox));
    base::AutoLock lock_for_texture_access(namespace_->lock);
    pending->texture = BoundTexture(target);
    pending_produce_textures_.push_back(pending.Pass());
  }

  virtual void consumeTextureCHROMIUM(GLenum target,
                                      const GLbyte* mailbox) OVERRIDE {
    CheckTextureIsBound(target);
    base::AutoLock lock_for_texture_access(namespace_->lock);
    scoped_refptr<TestTexture> texture =
        shared_data_->ConsumeTexture(mailbox, last_waited_sync_point_);
    namespace_->textures.Replace(BoundTextureId(target), texture);
  }

  void GetPixels(const gfx::Size& size,
                 ResourceFormat format,
                 uint8_t* pixels) {
    CheckTextureIsBound(GL_TEXTURE_2D);
    base::AutoLock lock_for_texture_access(namespace_->lock);
    scoped_refptr<TestTexture> texture = BoundTexture(GL_TEXTURE_2D);
    ASSERT_EQ(texture->size, size);
    ASSERT_EQ(texture->format, format);
    memcpy(pixels, texture->data.get(), TextureSizeBytes(size, format));
  }

 protected:
  explicit ResourceProviderContext(ContextSharedData* shared_data)
      : shared_data_(shared_data),
        last_waited_sync_point_(0) {}

 private:
  void AllocateTexture(const gfx::Size& size, GLenum format) {
    CheckTextureIsBound(GL_TEXTURE_2D);
    ResourceFormat texture_format = RGBA_8888;
    switch (format) {
      case GL_RGBA:
        texture_format = RGBA_8888;
        break;
      case GL_BGRA_EXT:
        texture_format = BGRA_8888;
        break;
    }
    base::AutoLock lock_for_texture_access(namespace_->lock);
    BoundTexture(GL_TEXTURE_2D)->Reallocate(size, texture_format);
  }

  void SetPixels(int xoffset,
                 int yoffset,
                 int width,
                 int height,
                 const void* pixels) {
    CheckTextureIsBound(GL_TEXTURE_2D);
    base::AutoLock lock_for_texture_access(namespace_->lock);
    scoped_refptr<TestTexture> texture = BoundTexture(GL_TEXTURE_2D);
    ASSERT_TRUE(texture->data.get());
    ASSERT_TRUE(xoffset >= 0 && xoffset + width <= texture->size.width());
    ASSERT_TRUE(yoffset >= 0 && yoffset + height <= texture->size.height());
    ASSERT_TRUE(pixels);
    size_t in_pitch = TextureSizeBytes(gfx::Size(width, 1), texture->format);
    size_t out_pitch =
        TextureSizeBytes(gfx::Size(texture->size.width(), 1), texture->format);
    uint8_t* dest = texture->data.get() + yoffset * out_pitch +
                    TextureSizeBytes(gfx::Size(xoffset, 1), texture->format);
    const uint8_t* src = static_cast<const uint8_t*>(pixels);
    for (int i = 0; i < height; ++i) {
      memcpy(dest, src, in_pitch);
      dest += out_pitch;
      src += in_pitch;
    }
  }

  struct PendingProduceTexture {
    GLbyte mailbox[GL_MAILBOX_SIZE_CHROMIUM];
    scoped_refptr<TestTexture> texture;
  };
  typedef ScopedPtrDeque<PendingProduceTexture> PendingProduceTextureList;
  ContextSharedData* shared_data_;
  GLuint last_waited_sync_point_;
  PendingProduceTextureList pending_produce_textures_;
};

void GetResourcePixels(ResourceProvider* resource_provider,
                       ResourceProviderContext* context,
                       ResourceProvider::ResourceId id,
                       const gfx::Size& size,
                       ResourceFormat format,
                       uint8_t* pixels) {
  switch (resource_provider->default_resource_type()) {
    case ResourceProvider::GLTexture: {
      ResourceProvider::ScopedReadLockGL lock_gl(resource_provider, id);
      ASSERT_NE(0U, lock_gl.texture_id());
      context->bindTexture(GL_TEXTURE_2D, lock_gl.texture_id());
      context->GetPixels(size, format, pixels);
      break;
    }
    case ResourceProvider::Bitmap: {
      ResourceProvider::ScopedReadLockSoftware lock_software(resource_provider,
                                                             id);
      memcpy(pixels,
             lock_software.sk_bitmap()->getPixels(),
             lock_software.sk_bitmap()->getSize());
      break;
    }
    case ResourceProvider::InvalidType:
      NOTREACHED();
      break;
  }
}

class ResourceProviderTest
    : public testing::TestWithParam<ResourceProvider::ResourceType> {
 public:
  ResourceProviderTest()
      : shared_data_(ContextSharedData::Create()),
        context3d_(NULL),
        child_context_(NULL) {
    switch (GetParam()) {
      case ResourceProvider::GLTexture: {
        scoped_ptr<ResourceProviderContext> context3d(
            ResourceProviderContext::Create(shared_data_.get()));
        context3d_ = context3d.get();

        scoped_refptr<TestContextProvider> context_provider =
            TestContextProvider::Create(
                context3d.PassAs<TestWebGraphicsContext3D>());

        output_surface_ = FakeOutputSurface::Create3d(context_provider);

        scoped_ptr<ResourceProviderContext> child_context_owned =
            ResourceProviderContext::Create(shared_data_.get());
        child_context_ = child_context_owned.get();
        child_output_surface_ = FakeOutputSurface::Create3d(
            child_context_owned.PassAs<TestWebGraphicsContext3D>());
        break;
      }
      case ResourceProvider::Bitmap:
        output_surface_ = FakeOutputSurface::CreateSoftware(
            make_scoped_ptr(new SoftwareOutputDevice));
        child_output_surface_ = FakeOutputSurface::CreateSoftware(
            make_scoped_ptr(new SoftwareOutputDevice));
        break;
      case ResourceProvider::InvalidType:
        NOTREACHED();
        break;
    }
    CHECK(output_surface_->BindToClient(&output_surface_client_));
    CHECK(child_output_surface_->BindToClient(&child_output_surface_client_));

    shared_bitmap_manager_.reset(new TestSharedBitmapManager());

    resource_provider_ = ResourceProvider::Create(
        output_surface_.get(), shared_bitmap_manager_.get(), 0, false, 1,
        false);
    child_resource_provider_ = ResourceProvider::Create(
        child_output_surface_.get(),
        shared_bitmap_manager_.get(),
        0,
        false,
        1,
        false);
  }

  static void CollectResources(ReturnedResourceArray* array,
                               const ReturnedResourceArray& returned) {
    array->insert(array->end(), returned.begin(), returned.end());
  }

  static ReturnCallback GetReturnCallback(ReturnedResourceArray* array) {
    return base::Bind(&ResourceProviderTest::CollectResources, array);
  }

  static void SetResourceFilter(ResourceProvider* resource_provider,
                                ResourceProvider::ResourceId id,
                                GLenum filter) {
    ResourceProvider::ScopedSamplerGL sampler(
        resource_provider, id, GL_TEXTURE_2D, filter);
  }

  ResourceProviderContext* context() { return context3d_; }

  ResourceProvider::ResourceId CreateChildMailbox(uint32* release_sync_point,
                                                  bool* lost_resource,
                                                  bool* release_called,
                                                  uint32* sync_point) {
    if (GetParam() == ResourceProvider::GLTexture) {
      unsigned texture = child_context_->createTexture();
      gpu::Mailbox gpu_mailbox;
      child_context_->bindTexture(GL_TEXTURE_2D, texture);
      child_context_->genMailboxCHROMIUM(gpu_mailbox.name);
      child_context_->produceTextureCHROMIUM(GL_TEXTURE_2D, gpu_mailbox.name);
      *sync_point = child_context_->insertSyncPoint();
      EXPECT_LT(0u, *sync_point);

      scoped_ptr<base::SharedMemory> shared_memory;
      scoped_ptr<SingleReleaseCallback> callback =
          SingleReleaseCallback::Create(base::Bind(ReleaseSharedMemoryCallback,
                                                   base::Passed(&shared_memory),
                                                   release_called,
                                                   release_sync_point,
                                                   lost_resource));
      return child_resource_provider_->CreateResourceFromTextureMailbox(
          TextureMailbox(gpu_mailbox, GL_TEXTURE_2D, *sync_point),
          callback.Pass());
    } else {
      gfx::Size size(64, 64);
      scoped_ptr<base::SharedMemory> shared_memory(
          CreateAndFillSharedMemory(size, 0));

      base::SharedMemory* shared_memory_ptr = shared_memory.get();
      scoped_ptr<SingleReleaseCallback> callback =
          SingleReleaseCallback::Create(base::Bind(ReleaseSharedMemoryCallback,
                                                   base::Passed(&shared_memory),
                                                   release_called,
                                                   release_sync_point,
                                                   lost_resource));
      return child_resource_provider_->CreateResourceFromTextureMailbox(
          TextureMailbox(shared_memory_ptr, size), callback.Pass());
    }
  }

 protected:
  scoped_ptr<ContextSharedData> shared_data_;
  ResourceProviderContext* context3d_;
  ResourceProviderContext* child_context_;
  FakeOutputSurfaceClient output_surface_client_;
  FakeOutputSurfaceClient child_output_surface_client_;
  scoped_ptr<OutputSurface> output_surface_;
  scoped_ptr<OutputSurface> child_output_surface_;
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<ResourceProvider> child_resource_provider_;
  scoped_ptr<TestSharedBitmapManager> shared_bitmap_manager_;
};

void CheckCreateResource(ResourceProvider::ResourceType expected_default_type,
                         ResourceProvider* resource_provider,
                         ResourceProviderContext* context) {
  DCHECK_EQ(expected_default_type, resource_provider->default_resource_type());

  gfx::Size size(1, 1);
  ResourceFormat format = RGBA_8888;
  size_t pixel_size = TextureSizeBytes(size, format);
  ASSERT_EQ(4U, pixel_size);

  ResourceProvider::ResourceId id = resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  EXPECT_EQ(1, static_cast<int>(resource_provider->num_resources()));
  if (expected_default_type == ResourceProvider::GLTexture)
    EXPECT_EQ(0u, context->NumTextures());

  uint8_t data[4] = { 1, 2, 3, 4 };
  gfx::Rect rect(size);
  resource_provider->SetPixels(id, data, rect, rect, gfx::Vector2d());
  if (expected_default_type == ResourceProvider::GLTexture)
    EXPECT_EQ(1u, context->NumTextures());

  uint8_t result[4] = { 0 };
  GetResourcePixels(resource_provider, context, id, size, format, result);
  EXPECT_EQ(0, memcmp(data, result, pixel_size));

  resource_provider->DeleteResource(id);
  EXPECT_EQ(0, static_cast<int>(resource_provider->num_resources()));
  if (expected_default_type == ResourceProvider::GLTexture)
    EXPECT_EQ(0u, context->NumTextures());
}

TEST_P(ResourceProviderTest, Basic) {
  CheckCreateResource(GetParam(), resource_provider_.get(), context());
}

TEST_P(ResourceProviderTest, Upload) {
  gfx::Size size(2, 2);
  ResourceFormat format = RGBA_8888;
  size_t pixel_size = TextureSizeBytes(size, format);
  ASSERT_EQ(16U, pixel_size);

  ResourceProvider::ResourceId id = resource_provider_->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);

  uint8_t image[16] = { 0 };
  gfx::Rect image_rect(size);
  resource_provider_->SetPixels(
      id, image, image_rect, image_rect, gfx::Vector2d());

  for (uint8_t i = 0; i < pixel_size; ++i)
    image[i] = i;

  uint8_t result[16] = { 0 };
  {
    gfx::Rect source_rect(0, 0, 1, 1);
    gfx::Vector2d dest_offset(0, 0);
    resource_provider_->SetPixels(
        id, image, image_rect, source_rect, dest_offset);

    uint8_t expected[16] = { 0, 1, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    GetResourcePixels(
        resource_provider_.get(), context(), id, size, format, result);
    EXPECT_EQ(0, memcmp(expected, result, pixel_size));
  }
  {
    gfx::Rect source_rect(0, 0, 1, 1);
    gfx::Vector2d dest_offset(1, 1);
    resource_provider_->SetPixels(
        id, image, image_rect, source_rect, dest_offset);

    uint8_t expected[16] = { 0, 1, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3 };
    GetResourcePixels(
        resource_provider_.get(), context(), id, size, format, result);
    EXPECT_EQ(0, memcmp(expected, result, pixel_size));
  }
  {
    gfx::Rect source_rect(1, 0, 1, 1);
    gfx::Vector2d dest_offset(0, 1);
    resource_provider_->SetPixels(
        id, image, image_rect, source_rect, dest_offset);

    uint8_t expected[16] = { 0, 1, 2, 3, 0, 0, 0, 0, 4, 5, 6, 7, 0, 1, 2, 3 };
    GetResourcePixels(
        resource_provider_.get(), context(), id, size, format, result);
    EXPECT_EQ(0, memcmp(expected, result, pixel_size));
  }
  {
    gfx::Rect offset_image_rect(gfx::Point(100, 100), size);
    gfx::Rect source_rect(100, 100, 1, 1);
    gfx::Vector2d dest_offset(1, 0);
    resource_provider_->SetPixels(
        id, image, offset_image_rect, source_rect, dest_offset);

    uint8_t expected[16] = { 0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3 };
    GetResourcePixels(
        resource_provider_.get(), context(), id, size, format, result);
    EXPECT_EQ(0, memcmp(expected, result, pixel_size));
  }

  resource_provider_->DeleteResource(id);
}

TEST_P(ResourceProviderTest, TransferGLResources) {
  if (GetParam() != ResourceProvider::GLTexture)
    return;
  gfx::Size size(1, 1);
  ResourceFormat format = RGBA_8888;
  size_t pixel_size = TextureSizeBytes(size, format);
  ASSERT_EQ(4U, pixel_size);

  ResourceProvider::ResourceId id1 = child_resource_provider_->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  uint8_t data1[4] = { 1, 2, 3, 4 };
  gfx::Rect rect(size);
  child_resource_provider_->SetPixels(id1, data1, rect, rect, gfx::Vector2d());

  ResourceProvider::ResourceId id2 = child_resource_provider_->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  uint8_t data2[4] = { 5, 5, 5, 5 };
  child_resource_provider_->SetPixels(id2, data2, rect, rect, gfx::Vector2d());

  ResourceProvider::ResourceId id3 = child_resource_provider_->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  child_resource_provider_->MapImageRasterBuffer(id3);
  child_resource_provider_->UnmapImageRasterBuffer(id3);

  GLuint external_texture_id = child_context_->createExternalTexture();
  child_context_->bindTexture(GL_TEXTURE_EXTERNAL_OES, external_texture_id);

  gpu::Mailbox external_mailbox;
  child_context_->genMailboxCHROMIUM(external_mailbox.name);
  child_context_->produceTextureCHROMIUM(GL_TEXTURE_EXTERNAL_OES,
                                         external_mailbox.name);
  const GLuint external_sync_point = child_context_->insertSyncPoint();
  ResourceProvider::ResourceId id4 =
      child_resource_provider_->CreateResourceFromTextureMailbox(
          TextureMailbox(
              external_mailbox, GL_TEXTURE_EXTERNAL_OES, external_sync_point),
          SingleReleaseCallback::Create(base::Bind(&EmptyReleaseCallback)));

  ReturnedResourceArray returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));
  {
    // Transfer some resources to the parent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(id1);
    resource_ids_to_transfer.push_back(id2);
    resource_ids_to_transfer.push_back(id3);
    resource_ids_to_transfer.push_back(id4);
    TransferableResourceArray list;
    child_resource_provider_->PrepareSendToParent(resource_ids_to_transfer,
                                                  &list);
    ASSERT_EQ(4u, list.size());
    EXPECT_NE(0u, list[0].mailbox_holder.sync_point);
    EXPECT_NE(0u, list[1].mailbox_holder.sync_point);
    EXPECT_EQ(list[0].mailbox_holder.sync_point,
              list[1].mailbox_holder.sync_point);
    EXPECT_NE(0u, list[2].mailbox_holder.sync_point);
    EXPECT_EQ(list[0].mailbox_holder.sync_point,
              list[2].mailbox_holder.sync_point);
    EXPECT_EQ(external_sync_point, list[3].mailbox_holder.sync_point);
    EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D),
              list[0].mailbox_holder.texture_target);
    EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D),
              list[1].mailbox_holder.texture_target);
    EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D),
              list[2].mailbox_holder.texture_target);
    EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_EXTERNAL_OES),
              list[3].mailbox_holder.texture_target);
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id1));
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id2));
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id3));
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id4));
    resource_provider_->ReceiveFromChild(child_id, list);
    EXPECT_NE(list[0].mailbox_holder.sync_point,
              context3d_->last_waited_sync_point());
    {
      ResourceProvider::ScopedReadLockGL lock(resource_provider_.get(),
                                              list[0].id);
    }
    EXPECT_EQ(list[0].mailbox_holder.sync_point,
              context3d_->last_waited_sync_point());
    resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                      resource_ids_to_transfer);
  }

  EXPECT_EQ(4u, resource_provider_->num_resources());
  ResourceProvider::ResourceIdMap resource_map =
      resource_provider_->GetChildToParentMap(child_id);
  ResourceProvider::ResourceId mapped_id1 = resource_map[id1];
  ResourceProvider::ResourceId mapped_id2 = resource_map[id2];
  ResourceProvider::ResourceId mapped_id3 = resource_map[id3];
  ResourceProvider::ResourceId mapped_id4 = resource_map[id4];
  EXPECT_NE(0u, mapped_id1);
  EXPECT_NE(0u, mapped_id2);
  EXPECT_NE(0u, mapped_id3);
  EXPECT_NE(0u, mapped_id4);
  EXPECT_FALSE(resource_provider_->InUseByConsumer(id1));
  EXPECT_FALSE(resource_provider_->InUseByConsumer(id2));
  EXPECT_FALSE(resource_provider_->InUseByConsumer(id3));
  EXPECT_FALSE(resource_provider_->InUseByConsumer(id4));

  uint8_t result[4] = { 0 };
  GetResourcePixels(
      resource_provider_.get(), context(), mapped_id1, size, format, result);
  EXPECT_EQ(0, memcmp(data1, result, pixel_size));

  GetResourcePixels(
      resource_provider_.get(), context(), mapped_id2, size, format, result);
  EXPECT_EQ(0, memcmp(data2, result, pixel_size));

  {
    // Check that transfering again the same resource from the child to the
    // parent works.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(id1);
    resource_ids_to_transfer.push_back(id2);
    resource_ids_to_transfer.push_back(id3);
    TransferableResourceArray list;
    child_resource_provider_->PrepareSendToParent(resource_ids_to_transfer,
                                                  &list);
    EXPECT_EQ(3u, list.size());
    EXPECT_EQ(id1, list[0].id);
    EXPECT_EQ(id2, list[1].id);
    EXPECT_EQ(id3, list[2].id);
    EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D),
              list[0].mailbox_holder.texture_target);
    EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D),
              list[1].mailbox_holder.texture_target);
    EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D),
              list[2].mailbox_holder.texture_target);
    ReturnedResourceArray returned;
    TransferableResource::ReturnResources(list, &returned);
    child_resource_provider_->ReceiveReturnsFromParent(returned);
    // ids were exported twice, we returned them only once, they should still
    // be in-use.
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id1));
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id2));
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id3));
  }
  {
    EXPECT_EQ(0u, returned_to_child.size());

    // Transfer resources back from the parent to the child. Set no resources as
    // being in use.
    ResourceProvider::ResourceIdArray no_resources;
    resource_provider_->DeclareUsedResourcesFromChild(child_id, no_resources);

    ASSERT_EQ(4u, returned_to_child.size());
    EXPECT_NE(0u, returned_to_child[0].sync_point);
    EXPECT_NE(0u, returned_to_child[1].sync_point);
    EXPECT_NE(0u, returned_to_child[2].sync_point);
    EXPECT_NE(0u, returned_to_child[3].sync_point);
    EXPECT_FALSE(returned_to_child[0].lost);
    EXPECT_FALSE(returned_to_child[1].lost);
    EXPECT_FALSE(returned_to_child[2].lost);
    EXPECT_FALSE(returned_to_child[3].lost);
    child_resource_provider_->ReceiveReturnsFromParent(returned_to_child);
    returned_to_child.clear();
  }
  EXPECT_FALSE(child_resource_provider_->InUseByConsumer(id1));
  EXPECT_FALSE(child_resource_provider_->InUseByConsumer(id2));
  EXPECT_FALSE(child_resource_provider_->InUseByConsumer(id3));
  EXPECT_FALSE(child_resource_provider_->InUseByConsumer(id4));

  {
    ResourceProvider::ScopedReadLockGL lock(child_resource_provider_.get(),
                                            id1);
    ASSERT_NE(0U, lock.texture_id());
    child_context_->bindTexture(GL_TEXTURE_2D, lock.texture_id());
    child_context_->GetPixels(size, format, result);
    EXPECT_EQ(0, memcmp(data1, result, pixel_size));
  }
  {
    ResourceProvider::ScopedReadLockGL lock(child_resource_provider_.get(),
                                            id2);
    ASSERT_NE(0U, lock.texture_id());
    child_context_->bindTexture(GL_TEXTURE_2D, lock.texture_id());
    child_context_->GetPixels(size, format, result);
    EXPECT_EQ(0, memcmp(data2, result, pixel_size));
  }
  {
    ResourceProvider::ScopedReadLockGL lock(child_resource_provider_.get(),
                                            id3);
    ASSERT_NE(0U, lock.texture_id());
    child_context_->bindTexture(GL_TEXTURE_2D, lock.texture_id());
  }
  {
    // Transfer resources to the parent again.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(id1);
    resource_ids_to_transfer.push_back(id2);
    resource_ids_to_transfer.push_back(id3);
    resource_ids_to_transfer.push_back(id4);
    TransferableResourceArray list;
    child_resource_provider_->PrepareSendToParent(resource_ids_to_transfer,
                                                  &list);
    ASSERT_EQ(4u, list.size());
    EXPECT_EQ(id1, list[0].id);
    EXPECT_EQ(id2, list[1].id);
    EXPECT_EQ(id3, list[2].id);
    EXPECT_EQ(id4, list[3].id);
    EXPECT_NE(0u, list[0].mailbox_holder.sync_point);
    EXPECT_NE(0u, list[1].mailbox_holder.sync_point);
    EXPECT_NE(0u, list[2].mailbox_holder.sync_point);
    EXPECT_NE(0u, list[3].mailbox_holder.sync_point);
    EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D),
              list[0].mailbox_holder.texture_target);
    EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D),
              list[1].mailbox_holder.texture_target);
    EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D),
              list[2].mailbox_holder.texture_target);
    EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_EXTERNAL_OES),
              list[3].mailbox_holder.texture_target);
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id1));
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id2));
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id3));
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id4));
    resource_provider_->ReceiveFromChild(child_id, list);
    resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                      resource_ids_to_transfer);
  }

  EXPECT_EQ(0u, returned_to_child.size());

  EXPECT_EQ(4u, resource_provider_->num_resources());
  resource_provider_->DestroyChild(child_id);
  EXPECT_EQ(0u, resource_provider_->num_resources());

  ASSERT_EQ(4u, returned_to_child.size());
  EXPECT_NE(0u, returned_to_child[0].sync_point);
  EXPECT_NE(0u, returned_to_child[1].sync_point);
  EXPECT_NE(0u, returned_to_child[2].sync_point);
  EXPECT_NE(0u, returned_to_child[3].sync_point);
  EXPECT_FALSE(returned_to_child[0].lost);
  EXPECT_FALSE(returned_to_child[1].lost);
  EXPECT_FALSE(returned_to_child[2].lost);
  EXPECT_FALSE(returned_to_child[3].lost);
}

TEST_P(ResourceProviderTest, ReadLockCountStopsReturnToChildOrDelete) {
  if (GetParam() != ResourceProvider::GLTexture)
    return;
  gfx::Size size(1, 1);
  ResourceFormat format = RGBA_8888;

  ResourceProvider::ResourceId id1 = child_resource_provider_->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  uint8_t data1[4] = {1, 2, 3, 4};
  gfx::Rect rect(size);
  child_resource_provider_->SetPixels(id1, data1, rect, rect, gfx::Vector2d());

  ReturnedResourceArray returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));
  {
    // Transfer some resources to the parent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(id1);
    TransferableResourceArray list;
    child_resource_provider_->PrepareSendToParent(resource_ids_to_transfer,
                                                  &list);
    ASSERT_EQ(1u, list.size());
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id1));

    resource_provider_->ReceiveFromChild(child_id, list);

    ResourceProvider::ScopedReadLockGL lock(resource_provider_.get(),
                                            list[0].id);

    resource_provider_->DeclareUsedResourcesFromChild(
        child_id, ResourceProvider::ResourceIdArray());
    EXPECT_EQ(0u, returned_to_child.size());
  }

  EXPECT_EQ(1u, returned_to_child.size());
  child_resource_provider_->ReceiveReturnsFromParent(returned_to_child);

  {
    ResourceProvider::ScopedReadLockGL lock(child_resource_provider_.get(),
                                            id1);
    child_resource_provider_->DeleteResource(id1);
    EXPECT_EQ(1u, child_resource_provider_->num_resources());
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id1));
  }

  EXPECT_EQ(0u, child_resource_provider_->num_resources());
  resource_provider_->DestroyChild(child_id);
}

TEST_P(ResourceProviderTest, TransferSoftwareResources) {
  if (GetParam() != ResourceProvider::Bitmap)
    return;

  gfx::Size size(1, 1);
  ResourceFormat format = RGBA_8888;
  size_t pixel_size = TextureSizeBytes(size, format);
  ASSERT_EQ(4U, pixel_size);

  ResourceProvider::ResourceId id1 = child_resource_provider_->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  uint8_t data1[4] = { 1, 2, 3, 4 };
  gfx::Rect rect(size);
  child_resource_provider_->SetPixels(id1, data1, rect, rect, gfx::Vector2d());

  ResourceProvider::ResourceId id2 = child_resource_provider_->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  uint8_t data2[4] = { 5, 5, 5, 5 };
  child_resource_provider_->SetPixels(id2, data2, rect, rect, gfx::Vector2d());

  ResourceProvider::ResourceId id3 = child_resource_provider_->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  uint8_t data3[4] = { 6, 7, 8, 9 };
  SkImageInfo info = SkImageInfo::MakeN32Premul(size.width(), size.height());
  SkCanvas* raster_canvas = child_resource_provider_->MapImageRasterBuffer(id3);
  raster_canvas->writePixels(info, data3, info.minRowBytes(), 0, 0);
  child_resource_provider_->UnmapImageRasterBuffer(id3);

  scoped_ptr<base::SharedMemory> shared_memory(new base::SharedMemory());
  shared_memory->CreateAndMapAnonymous(1);
  base::SharedMemory* shared_memory_ptr = shared_memory.get();
  ResourceProvider::ResourceId id4 =
      child_resource_provider_->CreateResourceFromTextureMailbox(
          TextureMailbox(shared_memory_ptr, gfx::Size(1, 1)),
          SingleReleaseCallback::Create(base::Bind(
              &SharedMemoryReleaseCallback, base::Passed(&shared_memory))));

  ReturnedResourceArray returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));
  {
    // Transfer some resources to the parent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(id1);
    resource_ids_to_transfer.push_back(id2);
    resource_ids_to_transfer.push_back(id3);
    resource_ids_to_transfer.push_back(id4);
    TransferableResourceArray list;
    child_resource_provider_->PrepareSendToParent(resource_ids_to_transfer,
                                                  &list);
    ASSERT_EQ(4u, list.size());
    EXPECT_EQ(0u, list[0].mailbox_holder.sync_point);
    EXPECT_EQ(0u, list[1].mailbox_holder.sync_point);
    EXPECT_EQ(0u, list[2].mailbox_holder.sync_point);
    EXPECT_EQ(0u, list[3].mailbox_holder.sync_point);
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id1));
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id2));
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id3));
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id4));
    resource_provider_->ReceiveFromChild(child_id, list);
    resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                      resource_ids_to_transfer);
  }

  EXPECT_EQ(4u, resource_provider_->num_resources());
  ResourceProvider::ResourceIdMap resource_map =
      resource_provider_->GetChildToParentMap(child_id);
  ResourceProvider::ResourceId mapped_id1 = resource_map[id1];
  ResourceProvider::ResourceId mapped_id2 = resource_map[id2];
  ResourceProvider::ResourceId mapped_id3 = resource_map[id3];
  ResourceProvider::ResourceId mapped_id4 = resource_map[id4];
  EXPECT_NE(0u, mapped_id1);
  EXPECT_NE(0u, mapped_id2);
  EXPECT_NE(0u, mapped_id3);
  EXPECT_NE(0u, mapped_id4);
  EXPECT_FALSE(resource_provider_->InUseByConsumer(id1));
  EXPECT_FALSE(resource_provider_->InUseByConsumer(id2));
  EXPECT_FALSE(resource_provider_->InUseByConsumer(id3));
  EXPECT_FALSE(resource_provider_->InUseByConsumer(id4));

  uint8_t result[4] = { 0 };
  GetResourcePixels(
      resource_provider_.get(), context(), mapped_id1, size, format, result);
  EXPECT_EQ(0, memcmp(data1, result, pixel_size));

  GetResourcePixels(
      resource_provider_.get(), context(), mapped_id2, size, format, result);
  EXPECT_EQ(0, memcmp(data2, result, pixel_size));

  GetResourcePixels(
      resource_provider_.get(), context(), mapped_id3, size, format, result);
  EXPECT_EQ(0, memcmp(data3, result, pixel_size));

  {
    // Check that transfering again the same resource from the child to the
    // parent works.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(id1);
    resource_ids_to_transfer.push_back(id2);
    resource_ids_to_transfer.push_back(id3);
    TransferableResourceArray list;
    child_resource_provider_->PrepareSendToParent(resource_ids_to_transfer,
                                                  &list);
    EXPECT_EQ(3u, list.size());
    EXPECT_EQ(id1, list[0].id);
    EXPECT_EQ(id2, list[1].id);
    EXPECT_EQ(id3, list[2].id);
    ReturnedResourceArray returned;
    TransferableResource::ReturnResources(list, &returned);
    child_resource_provider_->ReceiveReturnsFromParent(returned);
    // ids were exported twice, we returned them only once, they should still
    // be in-use.
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id1));
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id2));
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id3));
  }
  {
    EXPECT_EQ(0u, returned_to_child.size());

    // Transfer resources back from the parent to the child. Set no resources as
    // being in use.
    ResourceProvider::ResourceIdArray no_resources;
    resource_provider_->DeclareUsedResourcesFromChild(child_id, no_resources);

    ASSERT_EQ(4u, returned_to_child.size());
    EXPECT_EQ(0u, returned_to_child[0].sync_point);
    EXPECT_EQ(0u, returned_to_child[1].sync_point);
    EXPECT_EQ(0u, returned_to_child[2].sync_point);
    EXPECT_EQ(0u, returned_to_child[3].sync_point);
    std::set<ResourceProvider::ResourceId> expected_ids;
    expected_ids.insert(id1);
    expected_ids.insert(id2);
    expected_ids.insert(id3);
    expected_ids.insert(id4);
    std::set<ResourceProvider::ResourceId> returned_ids;
    for (unsigned i = 0; i < 4; i++)
      returned_ids.insert(returned_to_child[i].id);
    EXPECT_EQ(expected_ids, returned_ids);
    EXPECT_FALSE(returned_to_child[0].lost);
    EXPECT_FALSE(returned_to_child[1].lost);
    EXPECT_FALSE(returned_to_child[2].lost);
    EXPECT_FALSE(returned_to_child[3].lost);
    child_resource_provider_->ReceiveReturnsFromParent(returned_to_child);
    returned_to_child.clear();
  }
  EXPECT_FALSE(child_resource_provider_->InUseByConsumer(id1));
  EXPECT_FALSE(child_resource_provider_->InUseByConsumer(id2));
  EXPECT_FALSE(child_resource_provider_->InUseByConsumer(id3));
  EXPECT_FALSE(child_resource_provider_->InUseByConsumer(id4));

  {
    ResourceProvider::ScopedReadLockSoftware lock(
        child_resource_provider_.get(), id1);
    const SkBitmap* sk_bitmap = lock.sk_bitmap();
    EXPECT_EQ(sk_bitmap->width(), size.width());
    EXPECT_EQ(sk_bitmap->height(), size.height());
    EXPECT_EQ(0, memcmp(data1, sk_bitmap->getPixels(), pixel_size));
  }
  {
    ResourceProvider::ScopedReadLockSoftware lock(
        child_resource_provider_.get(), id2);
    const SkBitmap* sk_bitmap = lock.sk_bitmap();
    EXPECT_EQ(sk_bitmap->width(), size.width());
    EXPECT_EQ(sk_bitmap->height(), size.height());
    EXPECT_EQ(0, memcmp(data2, sk_bitmap->getPixels(), pixel_size));
  }
  {
    ResourceProvider::ScopedReadLockSoftware lock(
        child_resource_provider_.get(), id3);
    const SkBitmap* sk_bitmap = lock.sk_bitmap();
    EXPECT_EQ(sk_bitmap->width(), size.width());
    EXPECT_EQ(sk_bitmap->height(), size.height());
    EXPECT_EQ(0, memcmp(data3, sk_bitmap->getPixels(), pixel_size));
  }
  {
    // Transfer resources to the parent again.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(id1);
    resource_ids_to_transfer.push_back(id2);
    resource_ids_to_transfer.push_back(id3);
    resource_ids_to_transfer.push_back(id4);
    TransferableResourceArray list;
    child_resource_provider_->PrepareSendToParent(resource_ids_to_transfer,
                                                  &list);
    ASSERT_EQ(4u, list.size());
    EXPECT_EQ(id1, list[0].id);
    EXPECT_EQ(id2, list[1].id);
    EXPECT_EQ(id3, list[2].id);
    EXPECT_EQ(id4, list[3].id);
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id1));
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id2));
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id3));
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id4));
    resource_provider_->ReceiveFromChild(child_id, list);
    resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                      resource_ids_to_transfer);
  }

  EXPECT_EQ(0u, returned_to_child.size());

  EXPECT_EQ(4u, resource_provider_->num_resources());
  resource_provider_->DestroyChild(child_id);
  EXPECT_EQ(0u, resource_provider_->num_resources());

  ASSERT_EQ(4u, returned_to_child.size());
  EXPECT_EQ(0u, returned_to_child[0].sync_point);
  EXPECT_EQ(0u, returned_to_child[1].sync_point);
  EXPECT_EQ(0u, returned_to_child[2].sync_point);
  EXPECT_EQ(0u, returned_to_child[3].sync_point);
  std::set<ResourceProvider::ResourceId> expected_ids;
  expected_ids.insert(id1);
  expected_ids.insert(id2);
  expected_ids.insert(id3);
  expected_ids.insert(id4);
  std::set<ResourceProvider::ResourceId> returned_ids;
  for (unsigned i = 0; i < 4; i++)
    returned_ids.insert(returned_to_child[i].id);
  EXPECT_EQ(expected_ids, returned_ids);
  EXPECT_FALSE(returned_to_child[0].lost);
  EXPECT_FALSE(returned_to_child[1].lost);
  EXPECT_FALSE(returned_to_child[2].lost);
  EXPECT_FALSE(returned_to_child[3].lost);
}

TEST_P(ResourceProviderTest, TransferGLToSoftware) {
  if (GetParam() != ResourceProvider::Bitmap)
    return;

  scoped_ptr<ResourceProviderContext> child_context_owned(
      ResourceProviderContext::Create(shared_data_.get()));

  FakeOutputSurfaceClient child_output_surface_client;
  scoped_ptr<OutputSurface> child_output_surface(FakeOutputSurface::Create3d(
      child_context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(child_output_surface->BindToClient(&child_output_surface_client));

  scoped_ptr<ResourceProvider> child_resource_provider(ResourceProvider::Create(
      child_output_surface.get(), shared_bitmap_manager_.get(), 0, false, 1,
      false));

  gfx::Size size(1, 1);
  ResourceFormat format = RGBA_8888;
  size_t pixel_size = TextureSizeBytes(size, format);
  ASSERT_EQ(4U, pixel_size);

  ResourceProvider::ResourceId id1 = child_resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  uint8_t data1[4] = { 1, 2, 3, 4 };
  gfx::Rect rect(size);
  child_resource_provider->SetPixels(id1, data1, rect, rect, gfx::Vector2d());

  ReturnedResourceArray returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));
  {
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(id1);
    TransferableResourceArray list;
    child_resource_provider->PrepareSendToParent(resource_ids_to_transfer,
                                                 &list);
    ASSERT_EQ(1u, list.size());
    EXPECT_NE(0u, list[0].mailbox_holder.sync_point);
    EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D),
              list[0].mailbox_holder.texture_target);
    EXPECT_TRUE(child_resource_provider->InUseByConsumer(id1));
    resource_provider_->ReceiveFromChild(child_id, list);
  }

  EXPECT_EQ(0u, resource_provider_->num_resources());
  ASSERT_EQ(1u, returned_to_child.size());
  EXPECT_EQ(returned_to_child[0].id, id1);
  ResourceProvider::ResourceIdMap resource_map =
      resource_provider_->GetChildToParentMap(child_id);
  ResourceProvider::ResourceId mapped_id1 = resource_map[id1];
  EXPECT_EQ(0u, mapped_id1);

  resource_provider_->DestroyChild(child_id);
  EXPECT_EQ(0u, resource_provider_->num_resources());

  ASSERT_EQ(1u, returned_to_child.size());
  EXPECT_FALSE(returned_to_child[0].lost);
}

TEST_P(ResourceProviderTest, TransferInvalidSoftware) {
  if (GetParam() != ResourceProvider::Bitmap)
    return;

  gfx::Size size(1, 1);
  ResourceFormat format = RGBA_8888;
  size_t pixel_size = TextureSizeBytes(size, format);
  ASSERT_EQ(4U, pixel_size);

  ResourceProvider::ResourceId id1 = child_resource_provider_->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  uint8_t data1[4] = { 1, 2, 3, 4 };
  gfx::Rect rect(size);
  child_resource_provider_->SetPixels(id1, data1, rect, rect, gfx::Vector2d());

  ReturnedResourceArray returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));
  {
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(id1);
    TransferableResourceArray list;
    child_resource_provider_->PrepareSendToParent(resource_ids_to_transfer,
                                                  &list);
    ASSERT_EQ(1u, list.size());
    // Make invalid.
    list[0].mailbox_holder.mailbox.name[1] = 5;
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id1));
    resource_provider_->ReceiveFromChild(child_id, list);
  }

  EXPECT_EQ(1u, resource_provider_->num_resources());
  EXPECT_EQ(0u, returned_to_child.size());

  ResourceProvider::ResourceIdMap resource_map =
      resource_provider_->GetChildToParentMap(child_id);
  ResourceProvider::ResourceId mapped_id1 = resource_map[id1];
  EXPECT_NE(0u, mapped_id1);
  {
    ResourceProvider::ScopedReadLockSoftware lock(resource_provider_.get(),
                                                  mapped_id1);
    EXPECT_FALSE(lock.valid());
  }

  resource_provider_->DestroyChild(child_id);
  EXPECT_EQ(0u, resource_provider_->num_resources());

  ASSERT_EQ(1u, returned_to_child.size());
  EXPECT_FALSE(returned_to_child[0].lost);
}

TEST_P(ResourceProviderTest, DeleteExportedResources) {
  gfx::Size size(1, 1);
  ResourceFormat format = RGBA_8888;
  size_t pixel_size = TextureSizeBytes(size, format);
  ASSERT_EQ(4U, pixel_size);

  ResourceProvider::ResourceId id1 = child_resource_provider_->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  uint8_t data1[4] = { 1, 2, 3, 4 };
  gfx::Rect rect(size);
  child_resource_provider_->SetPixels(id1, data1, rect, rect, gfx::Vector2d());

  ResourceProvider::ResourceId id2 = child_resource_provider_->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  uint8_t data2[4] = {5, 5, 5, 5};
  child_resource_provider_->SetPixels(id2, data2, rect, rect, gfx::Vector2d());

  ReturnedResourceArray returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));
  {
    // Transfer some resources to the parent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(id1);
    resource_ids_to_transfer.push_back(id2);
    TransferableResourceArray list;
    child_resource_provider_->PrepareSendToParent(resource_ids_to_transfer,
                                                  &list);
    ASSERT_EQ(2u, list.size());
    if (GetParam() == ResourceProvider::GLTexture) {
      EXPECT_NE(0u, list[0].mailbox_holder.sync_point);
      EXPECT_NE(0u, list[1].mailbox_holder.sync_point);
    }
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id1));
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id2));
    resource_provider_->ReceiveFromChild(child_id, list);
    resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                      resource_ids_to_transfer);
  }

  EXPECT_EQ(2u, resource_provider_->num_resources());
  ResourceProvider::ResourceIdMap resource_map =
      resource_provider_->GetChildToParentMap(child_id);
  ResourceProvider::ResourceId mapped_id1 = resource_map[id1];
  ResourceProvider::ResourceId mapped_id2 = resource_map[id2];
  EXPECT_NE(0u, mapped_id1);
  EXPECT_NE(0u, mapped_id2);
  EXPECT_FALSE(resource_provider_->InUseByConsumer(id1));
  EXPECT_FALSE(resource_provider_->InUseByConsumer(id2));

  {
    // The parent transfers the resources to the grandparent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(mapped_id1);
    resource_ids_to_transfer.push_back(mapped_id2);
    TransferableResourceArray list;
    resource_provider_->PrepareSendToParent(resource_ids_to_transfer, &list);

    ASSERT_EQ(2u, list.size());
    if (GetParam() == ResourceProvider::GLTexture) {
      EXPECT_NE(0u, list[0].mailbox_holder.sync_point);
      EXPECT_NE(0u, list[1].mailbox_holder.sync_point);
    }
    EXPECT_TRUE(resource_provider_->InUseByConsumer(id1));
    EXPECT_TRUE(resource_provider_->InUseByConsumer(id2));

    // Release the resource in the parent. Set no resources as being in use. The
    // resources are exported so that can't be transferred back yet.
    ResourceProvider::ResourceIdArray no_resources;
    resource_provider_->DeclareUsedResourcesFromChild(child_id, no_resources);

    EXPECT_EQ(0u, returned_to_child.size());
    EXPECT_EQ(2u, resource_provider_->num_resources());

    // Return the resources from the grandparent to the parent. They should be
    // returned to the child then.
    EXPECT_EQ(2u, list.size());
    EXPECT_EQ(mapped_id1, list[0].id);
    EXPECT_EQ(mapped_id2, list[1].id);
    ReturnedResourceArray returned;
    TransferableResource::ReturnResources(list, &returned);
    resource_provider_->ReceiveReturnsFromParent(returned);

    EXPECT_EQ(0u, resource_provider_->num_resources());
    ASSERT_EQ(2u, returned_to_child.size());
    if (GetParam() == ResourceProvider::GLTexture) {
      EXPECT_NE(0u, returned_to_child[0].sync_point);
      EXPECT_NE(0u, returned_to_child[1].sync_point);
    }
    EXPECT_FALSE(returned_to_child[0].lost);
    EXPECT_FALSE(returned_to_child[1].lost);
  }
}

TEST_P(ResourceProviderTest, DestroyChildWithExportedResources) {
  gfx::Size size(1, 1);
  ResourceFormat format = RGBA_8888;
  size_t pixel_size = TextureSizeBytes(size, format);
  ASSERT_EQ(4U, pixel_size);

  ResourceProvider::ResourceId id1 = child_resource_provider_->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  uint8_t data1[4] = {1, 2, 3, 4};
  gfx::Rect rect(size);
  child_resource_provider_->SetPixels(id1, data1, rect, rect, gfx::Vector2d());

  ResourceProvider::ResourceId id2 = child_resource_provider_->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  uint8_t data2[4] = {5, 5, 5, 5};
  child_resource_provider_->SetPixels(id2, data2, rect, rect, gfx::Vector2d());

  ReturnedResourceArray returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));
  {
    // Transfer some resources to the parent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(id1);
    resource_ids_to_transfer.push_back(id2);
    TransferableResourceArray list;
    child_resource_provider_->PrepareSendToParent(resource_ids_to_transfer,
                                                  &list);
    ASSERT_EQ(2u, list.size());
    if (GetParam() == ResourceProvider::GLTexture) {
      EXPECT_NE(0u, list[0].mailbox_holder.sync_point);
      EXPECT_NE(0u, list[1].mailbox_holder.sync_point);
    }
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id1));
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id2));
    resource_provider_->ReceiveFromChild(child_id, list);
    resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                      resource_ids_to_transfer);
  }

  EXPECT_EQ(2u, resource_provider_->num_resources());
  ResourceProvider::ResourceIdMap resource_map =
      resource_provider_->GetChildToParentMap(child_id);
  ResourceProvider::ResourceId mapped_id1 = resource_map[id1];
  ResourceProvider::ResourceId mapped_id2 = resource_map[id2];
  EXPECT_NE(0u, mapped_id1);
  EXPECT_NE(0u, mapped_id2);
  EXPECT_FALSE(resource_provider_->InUseByConsumer(id1));
  EXPECT_FALSE(resource_provider_->InUseByConsumer(id2));

  {
    // The parent transfers the resources to the grandparent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(mapped_id1);
    resource_ids_to_transfer.push_back(mapped_id2);
    TransferableResourceArray list;
    resource_provider_->PrepareSendToParent(resource_ids_to_transfer, &list);

    ASSERT_EQ(2u, list.size());
    if (GetParam() == ResourceProvider::GLTexture) {
      EXPECT_NE(0u, list[0].mailbox_holder.sync_point);
      EXPECT_NE(0u, list[1].mailbox_holder.sync_point);
    }
    EXPECT_TRUE(resource_provider_->InUseByConsumer(id1));
    EXPECT_TRUE(resource_provider_->InUseByConsumer(id2));

    // Release the resource in the parent. Set no resources as being in use. The
    // resources are exported so that can't be transferred back yet.
    ResourceProvider::ResourceIdArray no_resources;
    resource_provider_->DeclareUsedResourcesFromChild(child_id, no_resources);

    // Destroy the child, the resources should not be returned yet.
    EXPECT_EQ(0u, returned_to_child.size());
    EXPECT_EQ(2u, resource_provider_->num_resources());

    resource_provider_->DestroyChild(child_id);

    EXPECT_EQ(2u, resource_provider_->num_resources());
    ASSERT_EQ(0u, returned_to_child.size());

    // Return a resource from the grandparent, it should be returned at this
    // point.
    EXPECT_EQ(2u, list.size());
    EXPECT_EQ(mapped_id1, list[0].id);
    EXPECT_EQ(mapped_id2, list[1].id);
    TransferableResourceArray return_list;
    return_list.push_back(list[1]);
    list.pop_back();
    ReturnedResourceArray returned;
    TransferableResource::ReturnResources(return_list, &returned);
    resource_provider_->ReceiveReturnsFromParent(returned);

    EXPECT_EQ(1u, resource_provider_->num_resources());
    ASSERT_EQ(1u, returned_to_child.size());
    if (GetParam() == ResourceProvider::GLTexture) {
      EXPECT_NE(0u, returned_to_child[0].sync_point);
    }
    EXPECT_FALSE(returned_to_child[0].lost);
    returned_to_child.clear();

    // Destroy the parent resource provider. The resource that's left should be
    // lost at this point, and returned.
    resource_provider_.reset();
    ASSERT_EQ(1u, returned_to_child.size());
    if (GetParam() == ResourceProvider::GLTexture) {
      EXPECT_NE(0u, returned_to_child[0].sync_point);
    }
    EXPECT_TRUE(returned_to_child[0].lost);
  }
}

TEST_P(ResourceProviderTest, DeleteTransferredResources) {
  gfx::Size size(1, 1);
  ResourceFormat format = RGBA_8888;
  size_t pixel_size = TextureSizeBytes(size, format);
  ASSERT_EQ(4U, pixel_size);

  ResourceProvider::ResourceId id = child_resource_provider_->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  uint8_t data[4] = { 1, 2, 3, 4 };
  gfx::Rect rect(size);
  child_resource_provider_->SetPixels(id, data, rect, rect, gfx::Vector2d());

  ReturnedResourceArray returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));
  {
    // Transfer some resource to the parent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(id);
    TransferableResourceArray list;
    child_resource_provider_->PrepareSendToParent(resource_ids_to_transfer,
                                                  &list);
    ASSERT_EQ(1u, list.size());
    if (GetParam() == ResourceProvider::GLTexture)
      EXPECT_NE(0u, list[0].mailbox_holder.sync_point);
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id));
    resource_provider_->ReceiveFromChild(child_id, list);
    resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                      resource_ids_to_transfer);
  }

  // Delete textures in the child, while they are transfered.
  child_resource_provider_->DeleteResource(id);
  EXPECT_EQ(1u, child_resource_provider_->num_resources());
  {
    EXPECT_EQ(0u, returned_to_child.size());

    // Transfer resources back from the parent to the child. Set no resources as
    // being in use.
    ResourceProvider::ResourceIdArray no_resources;
    resource_provider_->DeclareUsedResourcesFromChild(child_id, no_resources);

    ASSERT_EQ(1u, returned_to_child.size());
    if (GetParam() == ResourceProvider::GLTexture)
      EXPECT_NE(0u, returned_to_child[0].sync_point);
    child_resource_provider_->ReceiveReturnsFromParent(returned_to_child);
  }
  EXPECT_EQ(0u, child_resource_provider_->num_resources());
}

TEST_P(ResourceProviderTest, UnuseTransferredResources) {
  gfx::Size size(1, 1);
  ResourceFormat format = RGBA_8888;
  size_t pixel_size = TextureSizeBytes(size, format);
  ASSERT_EQ(4U, pixel_size);

  ResourceProvider::ResourceId id = child_resource_provider_->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  uint8_t data[4] = {1, 2, 3, 4};
  gfx::Rect rect(size);
  child_resource_provider_->SetPixels(id, data, rect, rect, gfx::Vector2d());

  ReturnedResourceArray returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));
  const ResourceProvider::ResourceIdMap& map =
      resource_provider_->GetChildToParentMap(child_id);
  {
    // Transfer some resource to the parent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(id);
    TransferableResourceArray list;
    child_resource_provider_->PrepareSendToParent(resource_ids_to_transfer,
                                                  &list);
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id));
    resource_provider_->ReceiveFromChild(child_id, list);
    resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                      resource_ids_to_transfer);
  }
  TransferableResourceArray sent_to_top_level;
  {
    // Parent transfers to top-level.
    ASSERT_TRUE(map.find(id) != map.end());
    ResourceProvider::ResourceId parent_id = map.find(id)->second;
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(parent_id);
    resource_provider_->PrepareSendToParent(resource_ids_to_transfer,
                                            &sent_to_top_level);
    EXPECT_TRUE(resource_provider_->InUseByConsumer(parent_id));
  }
  {
    // Stop using resource.
    ResourceProvider::ResourceIdArray empty;
    resource_provider_->DeclareUsedResourcesFromChild(child_id, empty);
    // Resource is not yet returned to the child, since it's in use by the
    // top-level.
    EXPECT_TRUE(returned_to_child.empty());
  }
  {
    // Send the resource to the parent again.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(id);
    TransferableResourceArray list;
    child_resource_provider_->PrepareSendToParent(resource_ids_to_transfer,
                                                  &list);
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id));
    resource_provider_->ReceiveFromChild(child_id, list);
    resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                      resource_ids_to_transfer);
  }
  {
    // Receive returns back from top-level.
    ReturnedResourceArray returned;
    TransferableResource::ReturnResources(sent_to_top_level, &returned);
    resource_provider_->ReceiveReturnsFromParent(returned);
    // Resource is still not yet returned to the child, since it's declared used
    // in the parent.
    EXPECT_TRUE(returned_to_child.empty());
    ASSERT_TRUE(map.find(id) != map.end());
    ResourceProvider::ResourceId parent_id = map.find(id)->second;
    EXPECT_FALSE(resource_provider_->InUseByConsumer(parent_id));
  }
  {
    sent_to_top_level.clear();
    // Parent transfers again to top-level.
    ASSERT_TRUE(map.find(id) != map.end());
    ResourceProvider::ResourceId parent_id = map.find(id)->second;
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(parent_id);
    resource_provider_->PrepareSendToParent(resource_ids_to_transfer,
                                            &sent_to_top_level);
    EXPECT_TRUE(resource_provider_->InUseByConsumer(parent_id));
  }
  {
    // Receive returns back from top-level.
    ReturnedResourceArray returned;
    TransferableResource::ReturnResources(sent_to_top_level, &returned);
    resource_provider_->ReceiveReturnsFromParent(returned);
    // Resource is still not yet returned to the child, since it's still
    // declared used in the parent.
    EXPECT_TRUE(returned_to_child.empty());
    ASSERT_TRUE(map.find(id) != map.end());
    ResourceProvider::ResourceId parent_id = map.find(id)->second;
    EXPECT_FALSE(resource_provider_->InUseByConsumer(parent_id));
  }
  {
    // Stop using resource.
    ResourceProvider::ResourceIdArray empty;
    resource_provider_->DeclareUsedResourcesFromChild(child_id, empty);
    // Resource should have been returned to the child, since it's no longer in
    // use by the top-level.
    ASSERT_EQ(1u, returned_to_child.size());
    EXPECT_EQ(id, returned_to_child[0].id);
    EXPECT_EQ(2, returned_to_child[0].count);
    child_resource_provider_->ReceiveReturnsFromParent(returned_to_child);
    returned_to_child.clear();
    EXPECT_FALSE(child_resource_provider_->InUseByConsumer(id));
  }
}

class ResourceProviderTestTextureFilters : public ResourceProviderTest {
 public:
  static void RunTest(GLenum child_filter, GLenum parent_filter) {
    scoped_ptr<TextureStateTrackingContext> child_context_owned(
        new TextureStateTrackingContext);
    TextureStateTrackingContext* child_context = child_context_owned.get();

    FakeOutputSurfaceClient child_output_surface_client;
    scoped_ptr<OutputSurface> child_output_surface(FakeOutputSurface::Create3d(
        child_context_owned.PassAs<TestWebGraphicsContext3D>()));
    CHECK(child_output_surface->BindToClient(&child_output_surface_client));
    scoped_ptr<SharedBitmapManager> shared_bitmap_manager(
        new TestSharedBitmapManager());

    scoped_ptr<ResourceProvider> child_resource_provider(
        ResourceProvider::Create(child_output_surface.get(),
                                 shared_bitmap_manager.get(),
                                 0,
                                 false,
                                 1,
                                 false));

    scoped_ptr<TextureStateTrackingContext> parent_context_owned(
        new TextureStateTrackingContext);
    TextureStateTrackingContext* parent_context = parent_context_owned.get();

    FakeOutputSurfaceClient parent_output_surface_client;
    scoped_ptr<OutputSurface> parent_output_surface(FakeOutputSurface::Create3d(
        parent_context_owned.PassAs<TestWebGraphicsContext3D>()));
    CHECK(parent_output_surface->BindToClient(&parent_output_surface_client));

    scoped_ptr<ResourceProvider> parent_resource_provider(
        ResourceProvider::Create(parent_output_surface.get(),
                                 shared_bitmap_manager.get(),
                                 0,
                                 false,
                                 1,
                                 false));

    gfx::Size size(1, 1);
    ResourceFormat format = RGBA_8888;
    int child_texture_id = 1;
    int parent_texture_id = 2;

    size_t pixel_size = TextureSizeBytes(size, format);
    ASSERT_EQ(4U, pixel_size);

    ResourceProvider::ResourceId id = child_resource_provider->CreateResource(
        size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);

    // The new texture is created with GL_LINEAR.
    EXPECT_CALL(*child_context, bindTexture(GL_TEXTURE_2D, child_texture_id))
        .Times(2);  // Once to create and once to allocate.
    EXPECT_CALL(*child_context,
                texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    EXPECT_CALL(*child_context,
                texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    EXPECT_CALL(
        *child_context,
        texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    EXPECT_CALL(
        *child_context,
        texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    EXPECT_CALL(*child_context,
                texParameteri(GL_TEXTURE_2D,
                              GL_TEXTURE_POOL_CHROMIUM,
                              GL_TEXTURE_POOL_UNMANAGED_CHROMIUM));
    child_resource_provider->AllocateForTesting(id);
    Mock::VerifyAndClearExpectations(child_context);

    uint8_t data[4] = { 1, 2, 3, 4 };
    gfx::Rect rect(size);

    EXPECT_CALL(*child_context, bindTexture(GL_TEXTURE_2D, child_texture_id));
    child_resource_provider->SetPixels(id, data, rect, rect, gfx::Vector2d());
    Mock::VerifyAndClearExpectations(child_context);

    // The texture is set to |child_filter| in the child.
    EXPECT_CALL(*child_context, bindTexture(GL_TEXTURE_2D, child_texture_id));
    if (child_filter != GL_LINEAR) {
      EXPECT_CALL(
          *child_context,
          texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, child_filter));
      EXPECT_CALL(
          *child_context,
          texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, child_filter));
    }
    SetResourceFilter(child_resource_provider.get(), id, child_filter);
    Mock::VerifyAndClearExpectations(child_context);

    ReturnedResourceArray returned_to_child;
    int child_id = parent_resource_provider->CreateChild(
        GetReturnCallback(&returned_to_child));
    {
      // Transfer some resource to the parent.
      ResourceProvider::ResourceIdArray resource_ids_to_transfer;
      resource_ids_to_transfer.push_back(id);
      TransferableResourceArray list;

      EXPECT_CALL(*child_context, bindTexture(GL_TEXTURE_2D, child_texture_id));
      EXPECT_CALL(*child_context,
                  produceTextureCHROMIUM(GL_TEXTURE_2D, _));
      EXPECT_CALL(*child_context, insertSyncPoint());
      child_resource_provider->PrepareSendToParent(resource_ids_to_transfer,
                                                   &list);
      Mock::VerifyAndClearExpectations(child_context);

      ASSERT_EQ(1u, list.size());
      EXPECT_EQ(static_cast<unsigned>(child_filter), list[0].filter);

      EXPECT_CALL(*parent_context,
                  bindTexture(GL_TEXTURE_2D, parent_texture_id));
      EXPECT_CALL(*parent_context, consumeTextureCHROMIUM(GL_TEXTURE_2D, _));
      parent_resource_provider->ReceiveFromChild(child_id, list);
      {
        ResourceProvider::ScopedReadLockGL lock(parent_resource_provider.get(),
                                                list[0].id);
      }
      Mock::VerifyAndClearExpectations(parent_context);

      parent_resource_provider->DeclareUsedResourcesFromChild(
          child_id, resource_ids_to_transfer);
      Mock::VerifyAndClearExpectations(parent_context);
    }
    ResourceProvider::ResourceIdMap resource_map =
        parent_resource_provider->GetChildToParentMap(child_id);
    ResourceProvider::ResourceId mapped_id = resource_map[id];
    EXPECT_NE(0u, mapped_id);

    // The texture is set to |parent_filter| in the parent.
    EXPECT_CALL(*parent_context, bindTexture(GL_TEXTURE_2D, parent_texture_id));
    EXPECT_CALL(
        *parent_context,
        texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, parent_filter));
    EXPECT_CALL(
        *parent_context,
        texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, parent_filter));
    SetResourceFilter(parent_resource_provider.get(), mapped_id, parent_filter);
    Mock::VerifyAndClearExpectations(parent_context);

    // The texture should be reset to |child_filter| in the parent when it is
    // returned, since that is how it was received.
    EXPECT_CALL(*parent_context, bindTexture(GL_TEXTURE_2D, parent_texture_id));
    EXPECT_CALL(
        *parent_context,
        texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, child_filter));
    EXPECT_CALL(
        *parent_context,
        texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, child_filter));

    {
      EXPECT_EQ(0u, returned_to_child.size());

      // Transfer resources back from the parent to the child. Set no resources
      // as being in use.
      ResourceProvider::ResourceIdArray no_resources;
      EXPECT_CALL(*parent_context, insertSyncPoint());
      parent_resource_provider->DeclareUsedResourcesFromChild(child_id,
                                                              no_resources);
      Mock::VerifyAndClearExpectations(parent_context);

      ASSERT_EQ(1u, returned_to_child.size());
      child_resource_provider->ReceiveReturnsFromParent(returned_to_child);
    }

    // The child remembers the texture filter is set to |child_filter|.
    EXPECT_CALL(*child_context, bindTexture(GL_TEXTURE_2D, child_texture_id));
    SetResourceFilter(child_resource_provider.get(), id, child_filter);
    Mock::VerifyAndClearExpectations(child_context);
  }
};

TEST_P(ResourceProviderTest, TextureFilters_ChildNearestParentLinear) {
  if (GetParam() != ResourceProvider::GLTexture)
    return;
  ResourceProviderTestTextureFilters::RunTest(GL_NEAREST, GL_LINEAR);
}

TEST_P(ResourceProviderTest, TextureFilters_ChildLinearParentNearest) {
  if (GetParam() != ResourceProvider::GLTexture)
    return;
  ResourceProviderTestTextureFilters::RunTest(GL_LINEAR, GL_NEAREST);
}

TEST_P(ResourceProviderTest, TransferMailboxResources) {
  // Other mailbox transfers tested elsewhere.
  if (GetParam() != ResourceProvider::GLTexture)
    return;
  unsigned texture = context()->createTexture();
  context()->bindTexture(GL_TEXTURE_2D, texture);
  uint8_t data[4] = { 1, 2, 3, 4 };
  context()->texImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &data);
  gpu::Mailbox mailbox;
  context()->genMailboxCHROMIUM(mailbox.name);
  context()->produceTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
  uint32 sync_point = context()->insertSyncPoint();

  // All the logic below assumes that the sync points are all positive.
  EXPECT_LT(0u, sync_point);

  uint32 release_sync_point = 0;
  bool lost_resource = false;
  ReleaseCallback callback =
      base::Bind(ReleaseTextureMailbox, &release_sync_point, &lost_resource);
  ResourceProvider::ResourceId resource =
      resource_provider_->CreateResourceFromTextureMailbox(
          TextureMailbox(mailbox, GL_TEXTURE_2D, sync_point),
          SingleReleaseCallback::Create(callback));
  EXPECT_EQ(1u, context()->NumTextures());
  EXPECT_EQ(0u, release_sync_point);
  {
    // Transfer the resource, expect the sync points to be consistent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(resource);
    TransferableResourceArray list;
    resource_provider_->PrepareSendToParent(resource_ids_to_transfer, &list);
    ASSERT_EQ(1u, list.size());
    EXPECT_LE(sync_point, list[0].mailbox_holder.sync_point);
    EXPECT_EQ(0,
              memcmp(mailbox.name,
                     list[0].mailbox_holder.mailbox.name,
                     sizeof(mailbox.name)));
    EXPECT_EQ(0u, release_sync_point);

    context()->waitSyncPoint(list[0].mailbox_holder.sync_point);
    unsigned other_texture = context()->createTexture();
    context()->bindTexture(GL_TEXTURE_2D, other_texture);
    context()->consumeTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
    uint8_t test_data[4] = { 0 };
    context()->GetPixels(
        gfx::Size(1, 1), RGBA_8888, test_data);
    EXPECT_EQ(0, memcmp(data, test_data, sizeof(data)));
    context()->produceTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
    context()->deleteTexture(other_texture);
    list[0].mailbox_holder.sync_point = context()->insertSyncPoint();
    EXPECT_LT(0u, list[0].mailbox_holder.sync_point);

    // Receive the resource, then delete it, expect the sync points to be
    // consistent.
    ReturnedResourceArray returned;
    TransferableResource::ReturnResources(list, &returned);
    resource_provider_->ReceiveReturnsFromParent(returned);
    EXPECT_EQ(1u, context()->NumTextures());
    EXPECT_EQ(0u, release_sync_point);

    resource_provider_->DeleteResource(resource);
    EXPECT_LE(list[0].mailbox_holder.sync_point, release_sync_point);
    EXPECT_FALSE(lost_resource);
  }

  // We're going to do the same thing as above, but testing the case where we
  // delete the resource before we receive it back.
  sync_point = release_sync_point;
  EXPECT_LT(0u, sync_point);
  release_sync_point = 0;
  resource = resource_provider_->CreateResourceFromTextureMailbox(
      TextureMailbox(mailbox, GL_TEXTURE_2D, sync_point),
      SingleReleaseCallback::Create(callback));
  EXPECT_EQ(1u, context()->NumTextures());
  EXPECT_EQ(0u, release_sync_point);
  {
    // Transfer the resource, expect the sync points to be consistent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(resource);
    TransferableResourceArray list;
    resource_provider_->PrepareSendToParent(resource_ids_to_transfer, &list);
    ASSERT_EQ(1u, list.size());
    EXPECT_LE(sync_point, list[0].mailbox_holder.sync_point);
    EXPECT_EQ(0,
              memcmp(mailbox.name,
                     list[0].mailbox_holder.mailbox.name,
                     sizeof(mailbox.name)));
    EXPECT_EQ(0u, release_sync_point);

    context()->waitSyncPoint(list[0].mailbox_holder.sync_point);
    unsigned other_texture = context()->createTexture();
    context()->bindTexture(GL_TEXTURE_2D, other_texture);
    context()->consumeTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
    uint8_t test_data[4] = { 0 };
    context()->GetPixels(
        gfx::Size(1, 1), RGBA_8888, test_data);
    EXPECT_EQ(0, memcmp(data, test_data, sizeof(data)));
    context()->produceTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
    context()->deleteTexture(other_texture);
    list[0].mailbox_holder.sync_point = context()->insertSyncPoint();
    EXPECT_LT(0u, list[0].mailbox_holder.sync_point);

    // Delete the resource, which shouldn't do anything.
    resource_provider_->DeleteResource(resource);
    EXPECT_EQ(1u, context()->NumTextures());
    EXPECT_EQ(0u, release_sync_point);

    // Then receive the resource which should release the mailbox, expect the
    // sync points to be consistent.
    ReturnedResourceArray returned;
    TransferableResource::ReturnResources(list, &returned);
    resource_provider_->ReceiveReturnsFromParent(returned);
    EXPECT_LE(list[0].mailbox_holder.sync_point, release_sync_point);
    EXPECT_FALSE(lost_resource);
  }

  context()->waitSyncPoint(release_sync_point);
  context()->bindTexture(GL_TEXTURE_2D, texture);
  context()->consumeTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
  context()->deleteTexture(texture);
}

TEST_P(ResourceProviderTest, LostResourceInParent) {
  gfx::Size size(1, 1);
  ResourceFormat format = RGBA_8888;
  ResourceProvider::ResourceId resource =
      child_resource_provider_->CreateResource(
          size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  child_resource_provider_->AllocateForTesting(resource);
  // Expect a GL resource to be lost.
  bool should_lose_resource = GetParam() == ResourceProvider::GLTexture;

  ReturnedResourceArray returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));
  {
    // Transfer the resource to the parent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(resource);
    TransferableResourceArray list;
    child_resource_provider_->PrepareSendToParent(resource_ids_to_transfer,
                                                  &list);
    EXPECT_EQ(1u, list.size());

    resource_provider_->ReceiveFromChild(child_id, list);
    resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                      resource_ids_to_transfer);
  }

  // Lose the output surface in the parent.
  resource_provider_->DidLoseOutputSurface();

  {
    EXPECT_EQ(0u, returned_to_child.size());

    // Transfer resources back from the parent to the child. Set no resources as
    // being in use.
    ResourceProvider::ResourceIdArray no_resources;
    resource_provider_->DeclareUsedResourcesFromChild(child_id, no_resources);

    // Expect a GL resource to be lost.
    ASSERT_EQ(1u, returned_to_child.size());
    EXPECT_EQ(should_lose_resource, returned_to_child[0].lost);
    child_resource_provider_->ReceiveReturnsFromParent(returned_to_child);
    returned_to_child.clear();
  }

  // A GL resource should be lost.
  EXPECT_EQ(should_lose_resource, child_resource_provider_->IsLost(resource));

  // Lost resources stay in use in the parent forever.
  EXPECT_EQ(should_lose_resource,
            child_resource_provider_->InUseByConsumer(resource));
}

TEST_P(ResourceProviderTest, LostResourceInGrandParent) {
  gfx::Size size(1, 1);
  ResourceFormat format = RGBA_8888;
  ResourceProvider::ResourceId resource =
      child_resource_provider_->CreateResource(
          size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  child_resource_provider_->AllocateForTesting(resource);

  ReturnedResourceArray returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));
  {
    // Transfer the resource to the parent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(resource);
    TransferableResourceArray list;
    child_resource_provider_->PrepareSendToParent(resource_ids_to_transfer,
                                                  &list);
    EXPECT_EQ(1u, list.size());

    resource_provider_->ReceiveFromChild(child_id, list);
    resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                      resource_ids_to_transfer);
  }

  {
    ResourceProvider::ResourceIdMap resource_map =
        resource_provider_->GetChildToParentMap(child_id);
    ResourceProvider::ResourceId parent_resource = resource_map[resource];
    EXPECT_NE(0u, parent_resource);

    // Transfer to a grandparent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(parent_resource);
    TransferableResourceArray list;
    resource_provider_->PrepareSendToParent(resource_ids_to_transfer, &list);

    // Receive back a lost resource from the grandparent.
    EXPECT_EQ(1u, list.size());
    EXPECT_EQ(parent_resource, list[0].id);
    ReturnedResourceArray returned;
    TransferableResource::ReturnResources(list, &returned);
    EXPECT_EQ(1u, returned.size());
    EXPECT_EQ(parent_resource, returned[0].id);
    returned[0].lost = true;
    resource_provider_->ReceiveReturnsFromParent(returned);

    // The resource should be lost.
    EXPECT_TRUE(resource_provider_->IsLost(parent_resource));

    // Lost resources stay in use in the parent forever.
    EXPECT_TRUE(resource_provider_->InUseByConsumer(parent_resource));
  }

  {
    EXPECT_EQ(0u, returned_to_child.size());

    // Transfer resources back from the parent to the child. Set no resources as
    // being in use.
    ResourceProvider::ResourceIdArray no_resources;
    resource_provider_->DeclareUsedResourcesFromChild(child_id, no_resources);

    // Expect the resource to be lost.
    ASSERT_EQ(1u, returned_to_child.size());
    EXPECT_TRUE(returned_to_child[0].lost);
    child_resource_provider_->ReceiveReturnsFromParent(returned_to_child);
    returned_to_child.clear();
  }

  // The resource should be lost.
  EXPECT_TRUE(child_resource_provider_->IsLost(resource));

  // Lost resources stay in use in the parent forever.
  EXPECT_TRUE(child_resource_provider_->InUseByConsumer(resource));
}

TEST_P(ResourceProviderTest, LostMailboxInParent) {
  uint32 release_sync_point = 0;
  bool lost_resource = false;
  bool release_called = false;
  uint32 sync_point = 0;
  ResourceProvider::ResourceId resource = CreateChildMailbox(
      &release_sync_point, &lost_resource, &release_called, &sync_point);

  ReturnedResourceArray returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));
  {
    // Transfer the resource to the parent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(resource);
    TransferableResourceArray list;
    child_resource_provider_->PrepareSendToParent(resource_ids_to_transfer,
                                                  &list);
    EXPECT_EQ(1u, list.size());

    resource_provider_->ReceiveFromChild(child_id, list);
    resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                      resource_ids_to_transfer);
  }

  // Lose the output surface in the parent.
  resource_provider_->DidLoseOutputSurface();

  {
    EXPECT_EQ(0u, returned_to_child.size());

    // Transfer resources back from the parent to the child. Set no resources as
    // being in use.
    ResourceProvider::ResourceIdArray no_resources;
    resource_provider_->DeclareUsedResourcesFromChild(child_id, no_resources);

    ASSERT_EQ(1u, returned_to_child.size());
    // Losing an output surface only loses hardware resources.
    EXPECT_EQ(returned_to_child[0].lost,
              GetParam() == ResourceProvider::GLTexture);
    child_resource_provider_->ReceiveReturnsFromParent(returned_to_child);
    returned_to_child.clear();
  }

  // Delete the resource in the child. Expect the resource to be lost if it's
  // a GL texture.
  child_resource_provider_->DeleteResource(resource);
  EXPECT_EQ(lost_resource, GetParam() == ResourceProvider::GLTexture);
}

TEST_P(ResourceProviderTest, LostMailboxInGrandParent) {
  uint32 release_sync_point = 0;
  bool lost_resource = false;
  bool release_called = false;
  uint32 sync_point = 0;
  ResourceProvider::ResourceId resource = CreateChildMailbox(
      &release_sync_point, &lost_resource, &release_called, &sync_point);

  ReturnedResourceArray returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));
  {
    // Transfer the resource to the parent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(resource);
    TransferableResourceArray list;
    child_resource_provider_->PrepareSendToParent(resource_ids_to_transfer,
                                                  &list);
    EXPECT_EQ(1u, list.size());

    resource_provider_->ReceiveFromChild(child_id, list);
    resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                      resource_ids_to_transfer);
  }

  {
    ResourceProvider::ResourceIdMap resource_map =
        resource_provider_->GetChildToParentMap(child_id);
    ResourceProvider::ResourceId parent_resource = resource_map[resource];
    EXPECT_NE(0u, parent_resource);

    // Transfer to a grandparent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(parent_resource);
    TransferableResourceArray list;
    resource_provider_->PrepareSendToParent(resource_ids_to_transfer, &list);

    // Receive back a lost resource from the grandparent.
    EXPECT_EQ(1u, list.size());
    EXPECT_EQ(parent_resource, list[0].id);
    ReturnedResourceArray returned;
    TransferableResource::ReturnResources(list, &returned);
    EXPECT_EQ(1u, returned.size());
    EXPECT_EQ(parent_resource, returned[0].id);
    returned[0].lost = true;
    resource_provider_->ReceiveReturnsFromParent(returned);
  }

  {
    EXPECT_EQ(0u, returned_to_child.size());

    // Transfer resources back from the parent to the child. Set no resources as
    // being in use.
    ResourceProvider::ResourceIdArray no_resources;
    resource_provider_->DeclareUsedResourcesFromChild(child_id, no_resources);

    // Expect the resource to be lost.
    ASSERT_EQ(1u, returned_to_child.size());
    EXPECT_TRUE(returned_to_child[0].lost);
    child_resource_provider_->ReceiveReturnsFromParent(returned_to_child);
    returned_to_child.clear();
  }

  // Delete the resource in the child. Expect the resource to be lost.
  child_resource_provider_->DeleteResource(resource);
  EXPECT_TRUE(lost_resource);
}

TEST_P(ResourceProviderTest, Shutdown) {
  uint32 release_sync_point = 0;
  bool lost_resource = false;
  bool release_called = false;
  uint32 sync_point = 0;
  CreateChildMailbox(
      &release_sync_point, &lost_resource, &release_called, &sync_point);

  EXPECT_EQ(0u, release_sync_point);
  EXPECT_FALSE(lost_resource);

  child_resource_provider_.reset();

  if (GetParam() == ResourceProvider::GLTexture) {
    EXPECT_LE(sync_point, release_sync_point);
  }
  EXPECT_TRUE(release_called);
  EXPECT_FALSE(lost_resource);
}

TEST_P(ResourceProviderTest, ShutdownWithExportedResource) {
  uint32 release_sync_point = 0;
  bool lost_resource = false;
  bool release_called = false;
  uint32 sync_point = 0;
  ResourceProvider::ResourceId resource = CreateChildMailbox(
      &release_sync_point, &lost_resource, &release_called, &sync_point);

  // Transfer the resource, so we can't release it properly on shutdown.
  ResourceProvider::ResourceIdArray resource_ids_to_transfer;
  resource_ids_to_transfer.push_back(resource);
  TransferableResourceArray list;
  child_resource_provider_->PrepareSendToParent(resource_ids_to_transfer,
                                                &list);

  EXPECT_EQ(0u, release_sync_point);
  EXPECT_FALSE(lost_resource);

  child_resource_provider_.reset();

  // Since the resource is in the parent, the child considers it lost.
  EXPECT_EQ(0u, release_sync_point);
  EXPECT_TRUE(lost_resource);
}

TEST_P(ResourceProviderTest, LostContext) {
  // TextureMailbox callbacks only exist for GL textures for now.
  if (GetParam() != ResourceProvider::GLTexture)
    return;
  unsigned texture = context()->createTexture();
  context()->bindTexture(GL_TEXTURE_2D, texture);
  gpu::Mailbox mailbox;
  context()->genMailboxCHROMIUM(mailbox.name);
  context()->produceTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
  uint32 sync_point = context()->insertSyncPoint();

  EXPECT_LT(0u, sync_point);

  uint32 release_sync_point = 0;
  bool lost_resource = false;
  scoped_ptr<SingleReleaseCallback> callback = SingleReleaseCallback::Create(
      base::Bind(ReleaseTextureMailbox, &release_sync_point, &lost_resource));
  resource_provider_->CreateResourceFromTextureMailbox(
      TextureMailbox(mailbox, GL_TEXTURE_2D, sync_point), callback.Pass());

  EXPECT_EQ(0u, release_sync_point);
  EXPECT_FALSE(lost_resource);

  resource_provider_->DidLoseOutputSurface();
  resource_provider_.reset();

  EXPECT_LE(sync_point, release_sync_point);
  EXPECT_TRUE(lost_resource);
}

TEST_P(ResourceProviderTest, ScopedSampler) {
  // Sampling is only supported for GL textures.
  if (GetParam() != ResourceProvider::GLTexture)
    return;

  scoped_ptr<TextureStateTrackingContext> context_owned(
      new TextureStateTrackingContext);
  TextureStateTrackingContext* context = context_owned.get();

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));

  scoped_ptr<ResourceProvider> resource_provider(ResourceProvider::Create(
      output_surface.get(), shared_bitmap_manager_.get(), 0, false, 1, false));

  gfx::Size size(1, 1);
  ResourceFormat format = RGBA_8888;
  int texture_id = 1;

  ResourceProvider::ResourceId id = resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);

  // Check that the texture gets created with the right sampler settings.
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id))
      .Times(2);  // Once to create and once to allocate.
  EXPECT_CALL(*context,
              texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
  EXPECT_CALL(*context,
              texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
  EXPECT_CALL(
      *context,
      texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  EXPECT_CALL(
      *context,
      texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
  EXPECT_CALL(*context,
              texParameteri(GL_TEXTURE_2D,
                            GL_TEXTURE_POOL_CHROMIUM,
                            GL_TEXTURE_POOL_UNMANAGED_CHROMIUM));

  resource_provider->AllocateForTesting(id);
  Mock::VerifyAndClearExpectations(context);

  // Creating a sampler with the default filter should not change any texture
  // parameters.
  {
    EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id));
    ResourceProvider::ScopedSamplerGL sampler(
        resource_provider.get(), id, GL_TEXTURE_2D, GL_LINEAR);
    Mock::VerifyAndClearExpectations(context);
  }

  // Using a different filter should be reflected in the texture parameters.
  {
    EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id));
    EXPECT_CALL(
        *context,
        texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    EXPECT_CALL(
        *context,
        texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    ResourceProvider::ScopedSamplerGL sampler(
        resource_provider.get(), id, GL_TEXTURE_2D, GL_NEAREST);
    Mock::VerifyAndClearExpectations(context);
  }

  // Test resetting to the default filter.
  {
    EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id));
    EXPECT_CALL(*context,
                texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    EXPECT_CALL(*context,
                texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    ResourceProvider::ScopedSamplerGL sampler(
        resource_provider.get(), id, GL_TEXTURE_2D, GL_LINEAR);
    Mock::VerifyAndClearExpectations(context);
  }
}

TEST_P(ResourceProviderTest, ManagedResource) {
  // Sampling is only supported for GL textures.
  if (GetParam() != ResourceProvider::GLTexture)
    return;

  scoped_ptr<TextureStateTrackingContext> context_owned(
      new TextureStateTrackingContext);
  TextureStateTrackingContext* context = context_owned.get();

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));

  scoped_ptr<ResourceProvider> resource_provider(ResourceProvider::Create(
      output_surface.get(), shared_bitmap_manager_.get(), 0, false, 1, false));

  gfx::Size size(1, 1);
  ResourceFormat format = RGBA_8888;
  int texture_id = 1;

  // Check that the texture gets created with the right sampler settings.
  ResourceProvider::ResourceId id = resource_provider->CreateManagedResource(
      size,
      GL_TEXTURE_2D,
      GL_CLAMP_TO_EDGE,
      ResourceProvider::TextureUsageAny,
      format);
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id));
  EXPECT_CALL(*context,
              texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
  EXPECT_CALL(*context,
              texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
  EXPECT_CALL(
      *context,
      texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  EXPECT_CALL(
      *context,
      texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
  EXPECT_CALL(*context,
              texParameteri(GL_TEXTURE_2D,
                            GL_TEXTURE_POOL_CHROMIUM,
                            GL_TEXTURE_POOL_MANAGED_CHROMIUM));
  resource_provider->CreateForTesting(id);
  EXPECT_NE(0u, id);

  Mock::VerifyAndClearExpectations(context);
}

TEST_P(ResourceProviderTest, TextureWrapMode) {
  // Sampling is only supported for GL textures.
  if (GetParam() != ResourceProvider::GLTexture)
    return;

  scoped_ptr<TextureStateTrackingContext> context_owned(
      new TextureStateTrackingContext);
  TextureStateTrackingContext* context = context_owned.get();

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));

  scoped_ptr<ResourceProvider> resource_provider(ResourceProvider::Create(
      output_surface.get(), shared_bitmap_manager_.get(), 0, false, 1, false));

  gfx::Size size(1, 1);
  ResourceFormat format = RGBA_8888;
  GLenum texture_pool = GL_TEXTURE_POOL_UNMANAGED_CHROMIUM;

  for (int texture_id = 1; texture_id <= 2; ++texture_id) {
    GLint wrap_mode = texture_id == 1 ? GL_CLAMP_TO_EDGE : GL_REPEAT;
    // Check that the texture gets created with the right sampler settings.
    ResourceProvider::ResourceId id =
        resource_provider->CreateGLTexture(size,
                                           GL_TEXTURE_2D,
                                           texture_pool,
                                           wrap_mode,
                                           ResourceProvider::TextureUsageAny,
                                           format);
    EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id));
    EXPECT_CALL(*context,
                texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    EXPECT_CALL(*context,
                texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    EXPECT_CALL(
        *context,
        texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_mode));
    EXPECT_CALL(
        *context,
        texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_mode));
    EXPECT_CALL(*context,
                texParameteri(GL_TEXTURE_2D,
                              GL_TEXTURE_POOL_CHROMIUM,
                              GL_TEXTURE_POOL_UNMANAGED_CHROMIUM));
    resource_provider->CreateForTesting(id);
    EXPECT_NE(0u, id);

    Mock::VerifyAndClearExpectations(context);
  }
}

TEST_P(ResourceProviderTest, TextureMailbox_SharedMemory) {
  if (GetParam() != ResourceProvider::Bitmap)
    return;

  gfx::Size size(64, 64);
  const uint32_t kBadBeef = 0xbadbeef;
  scoped_ptr<base::SharedMemory> shared_memory(
      CreateAndFillSharedMemory(size, kBadBeef));

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(
      FakeOutputSurface::CreateSoftware(make_scoped_ptr(
          new SoftwareOutputDevice)));
  CHECK(output_surface->BindToClient(&output_surface_client));

  scoped_ptr<ResourceProvider> resource_provider(ResourceProvider::Create(
      output_surface.get(), shared_bitmap_manager_.get(), 0, false, 1, false));

  scoped_ptr<SingleReleaseCallback> callback = SingleReleaseCallback::Create(
      base::Bind(&EmptyReleaseCallback));
  TextureMailbox mailbox(shared_memory.get(), size);

  ResourceProvider::ResourceId id =
      resource_provider->CreateResourceFromTextureMailbox(
          mailbox, callback.Pass());
  EXPECT_NE(0u, id);

  {
    ResourceProvider::ScopedReadLockSoftware lock(resource_provider.get(), id);
    const SkBitmap* sk_bitmap = lock.sk_bitmap();
    EXPECT_EQ(sk_bitmap->width(), size.width());
    EXPECT_EQ(sk_bitmap->height(), size.height());
    EXPECT_EQ(*sk_bitmap->getAddr32(16, 16), kBadBeef);
  }
}

TEST_P(ResourceProviderTest, TextureMailbox_GLTexture2D) {
  // Mailboxing is only supported for GL textures.
  if (GetParam() != ResourceProvider::GLTexture)
    return;

  scoped_ptr<TextureStateTrackingContext> context_owned(
      new TextureStateTrackingContext);
  TextureStateTrackingContext* context = context_owned.get();

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));

  scoped_ptr<ResourceProvider> resource_provider(ResourceProvider::Create(
      output_surface.get(), shared_bitmap_manager_.get(), 0, false, 1, false));

  unsigned texture_id = 1;
  uint32 sync_point = 30;
  unsigned target = GL_TEXTURE_2D;

  EXPECT_CALL(*context, bindTexture(_, _)).Times(0);
  EXPECT_CALL(*context, waitSyncPoint(_)).Times(0);
  EXPECT_CALL(*context, insertSyncPoint()).Times(0);
  EXPECT_CALL(*context, produceTextureCHROMIUM(_, _)).Times(0);
  EXPECT_CALL(*context, consumeTextureCHROMIUM(_, _)).Times(0);

  gpu::Mailbox gpu_mailbox;
  memcpy(gpu_mailbox.name, "Hello world", strlen("Hello world") + 1);
  scoped_ptr<SingleReleaseCallback> callback = SingleReleaseCallback::Create(
      base::Bind(&EmptyReleaseCallback));

  TextureMailbox mailbox(gpu_mailbox, target, sync_point);

  ResourceProvider::ResourceId id =
      resource_provider->CreateResourceFromTextureMailbox(
          mailbox, callback.Pass());
  EXPECT_NE(0u, id);

  Mock::VerifyAndClearExpectations(context);

  {
    // Using the texture does a consume of the mailbox.
    EXPECT_CALL(*context, bindTexture(target, texture_id));
    EXPECT_CALL(*context, waitSyncPoint(sync_point));
    EXPECT_CALL(*context, consumeTextureCHROMIUM(target, _));

    EXPECT_CALL(*context, insertSyncPoint()).Times(0);
    EXPECT_CALL(*context, produceTextureCHROMIUM(_, _)).Times(0);

    ResourceProvider::ScopedReadLockGL lock(resource_provider.get(), id);
    Mock::VerifyAndClearExpectations(context);

    // When done with it, a sync point should be inserted, but no produce is
    // necessary.
    EXPECT_CALL(*context, bindTexture(_, _)).Times(0);
    EXPECT_CALL(*context, insertSyncPoint());
    EXPECT_CALL(*context, produceTextureCHROMIUM(_, _)).Times(0);

    EXPECT_CALL(*context, waitSyncPoint(_)).Times(0);
    EXPECT_CALL(*context, consumeTextureCHROMIUM(_, _)).Times(0);
  }
}

TEST_P(ResourceProviderTest, TextureMailbox_GLTextureExternalOES) {
  // Mailboxing is only supported for GL textures.
  if (GetParam() != ResourceProvider::GLTexture)
    return;

  scoped_ptr<TextureStateTrackingContext> context_owned(
      new TextureStateTrackingContext);
  TextureStateTrackingContext* context = context_owned.get();

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));

  scoped_ptr<ResourceProvider> resource_provider(ResourceProvider::Create(
      output_surface.get(), shared_bitmap_manager_.get(), 0, false, 1, false));

  unsigned texture_id = 1;
  uint32 sync_point = 30;
  unsigned target = GL_TEXTURE_EXTERNAL_OES;

  EXPECT_CALL(*context, bindTexture(_, _)).Times(0);
  EXPECT_CALL(*context, waitSyncPoint(_)).Times(0);
  EXPECT_CALL(*context, insertSyncPoint()).Times(0);
  EXPECT_CALL(*context, produceTextureCHROMIUM(_, _)).Times(0);
  EXPECT_CALL(*context, consumeTextureCHROMIUM(_, _)).Times(0);

  gpu::Mailbox gpu_mailbox;
  memcpy(gpu_mailbox.name, "Hello world", strlen("Hello world") + 1);
  scoped_ptr<SingleReleaseCallback> callback = SingleReleaseCallback::Create(
      base::Bind(&EmptyReleaseCallback));

  TextureMailbox mailbox(gpu_mailbox, target, sync_point);

  ResourceProvider::ResourceId id =
      resource_provider->CreateResourceFromTextureMailbox(
          mailbox, callback.Pass());
  EXPECT_NE(0u, id);

  Mock::VerifyAndClearExpectations(context);

  {
    // Using the texture does a consume of the mailbox.
    EXPECT_CALL(*context, bindTexture(target, texture_id));
    EXPECT_CALL(*context, waitSyncPoint(sync_point));
    EXPECT_CALL(*context, consumeTextureCHROMIUM(target, _));

    EXPECT_CALL(*context, insertSyncPoint()).Times(0);
    EXPECT_CALL(*context, produceTextureCHROMIUM(_, _)).Times(0);

    ResourceProvider::ScopedReadLockGL lock(resource_provider.get(), id);
    Mock::VerifyAndClearExpectations(context);

    // When done with it, a sync point should be inserted, but no produce is
    // necessary.
    EXPECT_CALL(*context, bindTexture(_, _)).Times(0);
    EXPECT_CALL(*context, insertSyncPoint());
    EXPECT_CALL(*context, produceTextureCHROMIUM(_, _)).Times(0);

    EXPECT_CALL(*context, waitSyncPoint(_)).Times(0);
    EXPECT_CALL(*context, consumeTextureCHROMIUM(_, _)).Times(0);
  }
}

class AllocationTrackingContext3D : public TestWebGraphicsContext3D {
 public:
  MOCK_METHOD0(NextTextureId, GLuint());
  MOCK_METHOD1(RetireTextureId, void(GLuint id));
  MOCK_METHOD2(bindTexture, void(GLenum target, GLuint texture));
  MOCK_METHOD5(texStorage2DEXT,
               void(GLenum target,
                    GLint levels,
                    GLuint internalformat,
                    GLint width,
                    GLint height));
  MOCK_METHOD9(texImage2D,
               void(GLenum target,
                    GLint level,
                    GLenum internalformat,
                    GLsizei width,
                    GLsizei height,
                    GLint border,
                    GLenum format,
                    GLenum type,
                    const void* pixels));
  MOCK_METHOD9(texSubImage2D,
               void(GLenum target,
                    GLint level,
                    GLint xoffset,
                    GLint yoffset,
                    GLsizei width,
                    GLsizei height,
                    GLenum format,
                    GLenum type,
                    const void* pixels));
  MOCK_METHOD9(asyncTexImage2DCHROMIUM,
               void(GLenum target,
                    GLint level,
                    GLenum internalformat,
                    GLsizei width,
                    GLsizei height,
                    GLint border,
                    GLenum format,
                    GLenum type,
                    const void* pixels));
  MOCK_METHOD9(asyncTexSubImage2DCHROMIUM,
               void(GLenum target,
                    GLint level,
                    GLint xoffset,
                    GLint yoffset,
                    GLsizei width,
                    GLsizei height,
                    GLenum format,
                    GLenum type,
                    const void* pixels));
  MOCK_METHOD8(compressedTexImage2D,
               void(GLenum target,
                    GLint level,
                    GLenum internalformat,
                    GLsizei width,
                    GLsizei height,
                    GLint border,
                    GLsizei image_size,
                    const void* data));
  MOCK_METHOD1(waitAsyncTexImage2DCHROMIUM, void(GLenum));
  MOCK_METHOD4(createImageCHROMIUM, GLuint(GLsizei, GLsizei, GLenum, GLenum));
  MOCK_METHOD1(destroyImageCHROMIUM, void(GLuint));
  MOCK_METHOD1(mapImageCHROMIUM, void*(GLuint));
  MOCK_METHOD3(getImageParameterivCHROMIUM, void(GLuint, GLenum, GLint*));
  MOCK_METHOD1(unmapImageCHROMIUM, void(GLuint));
  MOCK_METHOD2(bindTexImage2DCHROMIUM, void(GLenum, GLint));
  MOCK_METHOD2(releaseTexImage2DCHROMIUM, void(GLenum, GLint));

  // We're mocking bindTexture, so we override
  // TestWebGraphicsContext3D::texParameteri to avoid assertions related to the
  // currently bound texture.
  virtual void texParameteri(GLenum target, GLenum pname, GLint param) {}
};

TEST_P(ResourceProviderTest, TextureAllocation) {
  // Only for GL textures.
  if (GetParam() != ResourceProvider::GLTexture)
    return;
  scoped_ptr<AllocationTrackingContext3D> context_owned(
      new StrictMock<AllocationTrackingContext3D>);
  AllocationTrackingContext3D* context = context_owned.get();

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));

  scoped_ptr<ResourceProvider> resource_provider(ResourceProvider::Create(
      output_surface.get(), shared_bitmap_manager_.get(), 0, false, 1, false));

  gfx::Size size(2, 2);
  gfx::Vector2d offset(0, 0);
  gfx::Rect rect(0, 0, 2, 2);
  ResourceFormat format = RGBA_8888;
  ResourceProvider::ResourceId id = 0;
  uint8_t pixels[16] = { 0 };
  int texture_id = 123;

  // Lazy allocation. Don't allocate when creating the resource.
  id = resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);

  EXPECT_CALL(*context, NextTextureId()).WillOnce(Return(texture_id));
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id)).Times(1);
  resource_provider->CreateForTesting(id);

  EXPECT_CALL(*context, RetireTextureId(texture_id)).Times(1);
  resource_provider->DeleteResource(id);

  Mock::VerifyAndClearExpectations(context);

  // Do allocate when we set the pixels.
  id = resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);

  EXPECT_CALL(*context, NextTextureId()).WillOnce(Return(texture_id));
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id)).Times(3);
  EXPECT_CALL(*context, texImage2D(_, _, _, 2, 2, _, _, _, _)).Times(1);
  EXPECT_CALL(*context, texSubImage2D(_, _, _, _, 2, 2, _, _, _)).Times(1);
  resource_provider->SetPixels(id, pixels, rect, rect, offset);

  EXPECT_CALL(*context, RetireTextureId(texture_id)).Times(1);
  resource_provider->DeleteResource(id);

  Mock::VerifyAndClearExpectations(context);

  // Same for async version.
  id = resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  resource_provider->AcquirePixelRasterBuffer(id);

  EXPECT_CALL(*context, NextTextureId()).WillOnce(Return(texture_id));
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id)).Times(2);
  EXPECT_CALL(*context, asyncTexImage2DCHROMIUM(_, _, _, 2, 2, _, _, _, _))
      .Times(1);
  resource_provider->BeginSetPixels(id);
  ASSERT_TRUE(resource_provider->DidSetPixelsComplete(id));

  resource_provider->ReleasePixelRasterBuffer(id);

  EXPECT_CALL(*context, RetireTextureId(texture_id)).Times(1);
  resource_provider->DeleteResource(id);

  Mock::VerifyAndClearExpectations(context);
}

TEST_P(ResourceProviderTest, TextureAllocationStorageUsageAny) {
  // Only for GL textures.
  if (GetParam() != ResourceProvider::GLTexture)
    return;
  scoped_ptr<AllocationTrackingContext3D> context_owned(
      new StrictMock<AllocationTrackingContext3D>);
  AllocationTrackingContext3D* context = context_owned.get();
  context->set_support_texture_storage(true);

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));

  scoped_ptr<ResourceProvider> resource_provider(ResourceProvider::Create(
      output_surface.get(), shared_bitmap_manager_.get(), 0, false, 1, false));

  gfx::Size size(2, 2);
  ResourceFormat format = RGBA_8888;
  ResourceProvider::ResourceId id = 0;
  int texture_id = 123;

  // Lazy allocation. Don't allocate when creating the resource.
  id = resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);

  EXPECT_CALL(*context, NextTextureId()).WillOnce(Return(texture_id));
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id)).Times(2);
  EXPECT_CALL(*context, texStorage2DEXT(_, _, _, 2, 2)).Times(1);
  resource_provider->AllocateForTesting(id);

  EXPECT_CALL(*context, RetireTextureId(texture_id)).Times(1);
  resource_provider->DeleteResource(id);

  Mock::VerifyAndClearExpectations(context);
}

TEST_P(ResourceProviderTest, TextureAllocationStorageUsageFramebuffer) {
  // Only for GL textures.
  if (GetParam() != ResourceProvider::GLTexture)
    return;
  scoped_ptr<AllocationTrackingContext3D> context_owned(
      new StrictMock<AllocationTrackingContext3D>);
  AllocationTrackingContext3D* context = context_owned.get();
  context->set_support_texture_storage(true);

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));

  scoped_ptr<ResourceProvider> resource_provider(ResourceProvider::Create(
      output_surface.get(), shared_bitmap_manager_.get(), 0, false, 1, false));

  gfx::Size size(2, 2);
  ResourceFormat format = RGBA_8888;
  ResourceProvider::ResourceId id = 0;
  int texture_id = 123;

  // Lazy allocation. Don't allocate when creating the resource.
  id = resource_provider->CreateResource(
      size,
      GL_CLAMP_TO_EDGE,
      ResourceProvider::TextureUsageFramebuffer,
      format);

  EXPECT_CALL(*context, NextTextureId()).WillOnce(Return(texture_id));
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id)).Times(2);
  EXPECT_CALL(*context, texImage2D(_, _, _, 2, 2, _, _, _, _)).Times(1);
  resource_provider->AllocateForTesting(id);

  EXPECT_CALL(*context, RetireTextureId(texture_id)).Times(1);
  resource_provider->DeleteResource(id);

  Mock::VerifyAndClearExpectations(context);
}

TEST_P(ResourceProviderTest, PixelBuffer_GLTexture) {
  if (GetParam() != ResourceProvider::GLTexture)
    return;
  scoped_ptr<AllocationTrackingContext3D> context_owned(
      new StrictMock<AllocationTrackingContext3D>);
  AllocationTrackingContext3D* context = context_owned.get();

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));

  gfx::Size size(2, 2);
  ResourceFormat format = RGBA_8888;
  ResourceProvider::ResourceId id = 0;
  int texture_id = 123;

  scoped_ptr<ResourceProvider> resource_provider(ResourceProvider::Create(
      output_surface.get(), shared_bitmap_manager_.get(), 0, false, 1, false));

  id = resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  resource_provider->AcquirePixelRasterBuffer(id);

  EXPECT_CALL(*context, NextTextureId()).WillOnce(Return(texture_id));
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id)).Times(2);
  EXPECT_CALL(*context, asyncTexImage2DCHROMIUM(_, _, _, 2, 2, _, _, _, _))
      .Times(1);
  resource_provider->BeginSetPixels(id);

  EXPECT_TRUE(resource_provider->DidSetPixelsComplete(id));

  resource_provider->ReleasePixelRasterBuffer(id);

  EXPECT_CALL(*context, RetireTextureId(texture_id)).Times(1);
  resource_provider->DeleteResource(id);

  Mock::VerifyAndClearExpectations(context);
}

TEST_P(ResourceProviderTest, ForcingAsyncUploadToComplete) {
  // Only for GL textures.
  if (GetParam() != ResourceProvider::GLTexture)
    return;
  scoped_ptr<AllocationTrackingContext3D> context_owned(
      new StrictMock<AllocationTrackingContext3D>);
  AllocationTrackingContext3D* context = context_owned.get();

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));

  gfx::Size size(2, 2);
  ResourceFormat format = RGBA_8888;
  ResourceProvider::ResourceId id = 0;
  int texture_id = 123;

  scoped_ptr<ResourceProvider> resource_provider(ResourceProvider::Create(
      output_surface.get(), shared_bitmap_manager_.get(), 0, false, 1, false));

  id = resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  resource_provider->AcquirePixelRasterBuffer(id);

  EXPECT_CALL(*context, NextTextureId()).WillOnce(Return(texture_id));
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id)).Times(2);
  EXPECT_CALL(*context, asyncTexImage2DCHROMIUM(_, _, _, 2, 2, _, _, _, _))
      .Times(1);
  resource_provider->BeginSetPixels(id);

  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id)).Times(1);
  EXPECT_CALL(*context, waitAsyncTexImage2DCHROMIUM(GL_TEXTURE_2D)).Times(1);
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, 0)).Times(1);
  resource_provider->ForceSetPixelsToComplete(id);

  resource_provider->ReleasePixelRasterBuffer(id);

  EXPECT_CALL(*context, RetireTextureId(texture_id)).Times(1);
  resource_provider->DeleteResource(id);

  Mock::VerifyAndClearExpectations(context);
}

TEST_P(ResourceProviderTest, PixelBufferLostContext) {
  scoped_ptr<AllocationTrackingContext3D> context_owned(
      new NiceMock<AllocationTrackingContext3D>);
  AllocationTrackingContext3D* context = context_owned.get();

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));

  gfx::Size size(2, 2);
  ResourceFormat format = RGBA_8888;
  ResourceProvider::ResourceId id = 0;
  int texture_id = 123;

  scoped_ptr<ResourceProvider> resource_provider(ResourceProvider::Create(
      output_surface.get(), shared_bitmap_manager_.get(), 0, false, 1, false));

  EXPECT_CALL(*context, NextTextureId()).WillRepeatedly(Return(texture_id));

  id = resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
  context->loseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
                               GL_INNOCENT_CONTEXT_RESET_ARB);

  resource_provider->AcquirePixelRasterBuffer(id);
  SkCanvas* raster_canvas = resource_provider->MapPixelRasterBuffer(id);
  EXPECT_TRUE(raster_canvas == NULL);
  resource_provider->UnmapPixelRasterBuffer(id);
  resource_provider->ReleasePixelRasterBuffer(id);
  Mock::VerifyAndClearExpectations(context);
}

TEST_P(ResourceProviderTest, Image_GLTexture) {
  // Only for GL textures.
  if (GetParam() != ResourceProvider::GLTexture)
    return;
  scoped_ptr<AllocationTrackingContext3D> context_owned(
      new StrictMock<AllocationTrackingContext3D>);
  AllocationTrackingContext3D* context = context_owned.get();

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));

  const int kWidth = 2;
  const int kHeight = 2;
  gfx::Size size(kWidth, kHeight);
  ResourceFormat format = RGBA_8888;
  ResourceProvider::ResourceId id = 0;
  const unsigned kTextureId = 123u;
  const unsigned kImageId = 234u;

  scoped_ptr<ResourceProvider> resource_provider(ResourceProvider::Create(
      output_surface.get(), shared_bitmap_manager_.get(), 0, false, 1, false));

  id = resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);

  const int kStride = 4;
  void* dummy_mapped_buffer_address = NULL;
  EXPECT_CALL(
      *context,
      createImageCHROMIUM(kWidth, kHeight, GL_RGBA8_OES, GL_IMAGE_MAP_CHROMIUM))
      .WillOnce(Return(kImageId))
      .RetiresOnSaturation();
  EXPECT_CALL(*context, getImageParameterivCHROMIUM(kImageId,
                                                    GL_IMAGE_ROWBYTES_CHROMIUM,
                                                    _))
      .WillOnce(SetArgPointee<2>(kStride))
      .RetiresOnSaturation();
  EXPECT_CALL(*context, mapImageCHROMIUM(kImageId))
      .WillOnce(Return(dummy_mapped_buffer_address))
      .RetiresOnSaturation();
  resource_provider->MapImageRasterBuffer(id);

  EXPECT_CALL(*context, unmapImageCHROMIUM(kImageId))
      .Times(1)
      .RetiresOnSaturation();
  resource_provider->UnmapImageRasterBuffer(id);

  EXPECT_CALL(*context, NextTextureId())
      .WillOnce(Return(kTextureId))
      .RetiresOnSaturation();
  // Once in CreateTextureId and once in BindForSampling
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, kTextureId)).Times(2)
      .RetiresOnSaturation();
  EXPECT_CALL(*context, bindTexImage2DCHROMIUM(GL_TEXTURE_2D, kImageId))
      .Times(1)
      .RetiresOnSaturation();
  {
    ResourceProvider::ScopedSamplerGL lock_gl(
        resource_provider.get(), id, GL_TEXTURE_2D, GL_LINEAR);
    EXPECT_EQ(kTextureId, lock_gl.texture_id());
  }

  EXPECT_CALL(
      *context,
      getImageParameterivCHROMIUM(kImageId, GL_IMAGE_ROWBYTES_CHROMIUM, _))
      .WillOnce(SetArgPointee<2>(kStride))
      .RetiresOnSaturation();
  EXPECT_CALL(*context, mapImageCHROMIUM(kImageId))
      .WillOnce(Return(dummy_mapped_buffer_address))
      .RetiresOnSaturation();
  resource_provider->MapImageRasterBuffer(id);

  EXPECT_CALL(*context, unmapImageCHROMIUM(kImageId))
      .Times(1)
      .RetiresOnSaturation();
  resource_provider->UnmapImageRasterBuffer(id);

  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, kTextureId)).Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*context, releaseTexImage2DCHROMIUM(GL_TEXTURE_2D, kImageId))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*context, bindTexImage2DCHROMIUM(GL_TEXTURE_2D, kImageId))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*context, RetireTextureId(kTextureId))
      .Times(1)
      .RetiresOnSaturation();
  {
    ResourceProvider::ScopedSamplerGL lock_gl(
        resource_provider.get(), id, GL_TEXTURE_2D, GL_LINEAR);
    EXPECT_EQ(kTextureId, lock_gl.texture_id());
  }

  EXPECT_CALL(*context, destroyImageCHROMIUM(kImageId))
      .Times(1)
      .RetiresOnSaturation();
}

TEST_P(ResourceProviderTest, Image_Bitmap) {
  if (GetParam() != ResourceProvider::Bitmap)
    return;
  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(
      FakeOutputSurface::CreateSoftware(make_scoped_ptr(
          new SoftwareOutputDevice)));
  CHECK(output_surface->BindToClient(&output_surface_client));

  gfx::Size size(1, 1);
  ResourceFormat format = RGBA_8888;
  ResourceProvider::ResourceId id = 0;
  const uint32_t kBadBeef = 0xbadbeef;

  scoped_ptr<ResourceProvider> resource_provider(ResourceProvider::Create(
      output_surface.get(), shared_bitmap_manager_.get(), 0, false, 1, false));

  id = resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);

  SkBitmap bitmap;
  bitmap.allocN32Pixels(size.width(), size.height());
  *(bitmap.getAddr32(0, 0)) = kBadBeef;
  SkCanvas* canvas = resource_provider->MapImageRasterBuffer(id);
  ASSERT_TRUE(!!canvas);
  canvas->writePixels(bitmap, 0, 0);
  resource_provider->UnmapImageRasterBuffer(id);

  {
    ResourceProvider::ScopedReadLockSoftware lock(resource_provider.get(), id);
    const SkBitmap* sk_bitmap = lock.sk_bitmap();
    EXPECT_EQ(sk_bitmap->width(), size.width());
    EXPECT_EQ(sk_bitmap->height(), size.height());
    EXPECT_EQ(*sk_bitmap->getAddr32(0, 0), kBadBeef);
  }

  resource_provider->DeleteResource(id);
}

TEST_P(ResourceProviderTest, CopyResource_GLTexture) {
  if (GetParam() != ResourceProvider::GLTexture)
    return;
  scoped_ptr<AllocationTrackingContext3D> context_owned(
      new StrictMock<AllocationTrackingContext3D>);
  AllocationTrackingContext3D* context = context_owned.get();
  context_owned->set_support_sync_query(true);

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  ASSERT_TRUE(output_surface->BindToClient(&output_surface_client));

  const int kWidth = 2;
  const int kHeight = 2;
  gfx::Size size(kWidth, kHeight);
  ResourceFormat format = RGBA_8888;
  ResourceProvider::ResourceId source_id = 0;
  ResourceProvider::ResourceId dest_id = 0;
  const unsigned kSourceTextureId = 123u;
  const unsigned kDestTextureId = 321u;
  const unsigned kImageId = 234u;

  scoped_ptr<ResourceProvider> resource_provider(ResourceProvider::Create(
      output_surface.get(), shared_bitmap_manager_.get(), 0, false, 1, false));

  source_id = resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);

  const int kStride = 4;
  void* dummy_mapped_buffer_address = NULL;
  EXPECT_CALL(
      *context,
      createImageCHROMIUM(kWidth, kHeight, GL_RGBA8_OES, GL_IMAGE_MAP_CHROMIUM))
      .WillOnce(Return(kImageId))
      .RetiresOnSaturation();
  EXPECT_CALL(
      *context,
      getImageParameterivCHROMIUM(kImageId, GL_IMAGE_ROWBYTES_CHROMIUM, _))
      .WillOnce(SetArgPointee<2>(kStride))
      .RetiresOnSaturation();
  EXPECT_CALL(*context, mapImageCHROMIUM(kImageId))
      .WillOnce(Return(dummy_mapped_buffer_address))
      .RetiresOnSaturation();
  resource_provider->MapImageRasterBuffer(source_id);
  Mock::VerifyAndClearExpectations(context);

  EXPECT_CALL(*context, unmapImageCHROMIUM(kImageId))
      .Times(1)
      .RetiresOnSaturation();
  resource_provider->UnmapImageRasterBuffer(source_id);
  Mock::VerifyAndClearExpectations(context);

  dest_id = resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);

  EXPECT_CALL(*context, NextTextureId())
      .WillOnce(Return(kDestTextureId))
      .RetiresOnSaturation();
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, kDestTextureId))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*context, NextTextureId())
      .WillOnce(Return(kSourceTextureId))
      .RetiresOnSaturation();
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, kSourceTextureId))
      .Times(2)
      .RetiresOnSaturation();
  EXPECT_CALL(*context, bindTexImage2DCHROMIUM(GL_TEXTURE_2D, kImageId))
      .Times(1)
      .RetiresOnSaturation();
  resource_provider->CopyResource(source_id, dest_id);
  Mock::VerifyAndClearExpectations(context);

  EXPECT_CALL(*context, destroyImageCHROMIUM(kImageId))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*context, RetireTextureId(kSourceTextureId))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*context, RetireTextureId(kDestTextureId))
      .Times(1)
      .RetiresOnSaturation();
  resource_provider->DeleteResource(source_id);
  resource_provider->DeleteResource(dest_id);
}

TEST_P(ResourceProviderTest, CopyResource_Bitmap) {
  if (GetParam() != ResourceProvider::Bitmap)
    return;
  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::CreateSoftware(
      make_scoped_ptr(new SoftwareOutputDevice)));
  CHECK(output_surface->BindToClient(&output_surface_client));

  gfx::Size size(1, 1);
  ResourceFormat format = RGBA_8888;
  ResourceProvider::ResourceId source_id = 0;
  ResourceProvider::ResourceId dest_id = 0;
  const uint32_t kBadBeef = 0xbadbeef;

  scoped_ptr<ResourceProvider> resource_provider(ResourceProvider::Create(
      output_surface.get(), shared_bitmap_manager_.get(), 0, false, 1, false));

  source_id = resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);

  SkBitmap bitmap;
  bitmap.allocN32Pixels(size.width(), size.height());
  *(bitmap.getAddr32(0, 0)) = kBadBeef;
  SkCanvas* canvas = resource_provider->MapImageRasterBuffer(source_id);
  ASSERT_TRUE(!!canvas);
  canvas->writePixels(bitmap, 0, 0);
  resource_provider->UnmapImageRasterBuffer(source_id);

  dest_id = resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);

  resource_provider->CopyResource(source_id, dest_id);

  {
    ResourceProvider::ScopedReadLockSoftware lock(resource_provider.get(),
                                                  dest_id);
    const SkBitmap* sk_bitmap = lock.sk_bitmap();
    EXPECT_EQ(sk_bitmap->width(), size.width());
    EXPECT_EQ(sk_bitmap->height(), size.height());
    EXPECT_EQ(*sk_bitmap->getAddr32(0, 0), kBadBeef);
  }

  resource_provider->DeleteResource(source_id);
  resource_provider->DeleteResource(dest_id);
}

void InitializeGLAndCheck(ContextSharedData* shared_data,
                          ResourceProvider* resource_provider,
                          FakeOutputSurface* output_surface) {
  scoped_ptr<ResourceProviderContext> context_owned =
      ResourceProviderContext::Create(shared_data);
  ResourceProviderContext* context = context_owned.get();

  scoped_refptr<TestContextProvider> context_provider =
      TestContextProvider::Create(
          context_owned.PassAs<TestWebGraphicsContext3D>());
  output_surface->InitializeAndSetContext3d(context_provider);
  resource_provider->InitializeGL();

  CheckCreateResource(ResourceProvider::GLTexture, resource_provider, context);
}

TEST(ResourceProviderTest, BasicInitializeGLSoftware) {
  scoped_ptr<ContextSharedData> shared_data = ContextSharedData::Create();
  bool delegated_rendering = false;
  scoped_ptr<FakeOutputSurface> output_surface(
      FakeOutputSurface::CreateDeferredGL(
          scoped_ptr<SoftwareOutputDevice>(new SoftwareOutputDevice),
          delegated_rendering));
  FakeOutputSurfaceClient client(output_surface.get());
  EXPECT_TRUE(output_surface->BindToClient(&client));
  scoped_ptr<SharedBitmapManager> shared_bitmap_manager(
      new TestSharedBitmapManager());
  scoped_ptr<ResourceProvider> resource_provider(ResourceProvider::Create(
      output_surface.get(), shared_bitmap_manager.get(), 0, false, 1, false));

  CheckCreateResource(ResourceProvider::Bitmap, resource_provider.get(), NULL);

  InitializeGLAndCheck(shared_data.get(),
                       resource_provider.get(),
                       output_surface.get());

  resource_provider->InitializeSoftware();
  output_surface->ReleaseGL();
  CheckCreateResource(ResourceProvider::Bitmap, resource_provider.get(), NULL);

  InitializeGLAndCheck(shared_data.get(),
                       resource_provider.get(),
                       output_surface.get());
}

TEST_P(ResourceProviderTest, CompressedTextureETC1Allocate) {
  if (GetParam() != ResourceProvider::GLTexture)
    return;

  scoped_ptr<AllocationTrackingContext3D> context_owned(
      new AllocationTrackingContext3D);
  AllocationTrackingContext3D* context = context_owned.get();
  context_owned->set_support_compressed_texture_etc1(true);

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));

  gfx::Size size(4, 4);
  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(),
                               shared_bitmap_manager_.get(),
                               0,
                               false,
                               1,
                               false));
  int texture_id = 123;

  ResourceProvider::ResourceId id = resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, ETC1);
  EXPECT_NE(0u, id);
  EXPECT_CALL(*context, NextTextureId()).WillOnce(Return(texture_id));
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id)).Times(2);
  resource_provider->AllocateForTesting(id);

  EXPECT_CALL(*context, RetireTextureId(texture_id)).Times(1);
  resource_provider->DeleteResource(id);
}

TEST_P(ResourceProviderTest, CompressedTextureETC1SetPixels) {
  if (GetParam() != ResourceProvider::GLTexture)
    return;

  scoped_ptr<AllocationTrackingContext3D> context_owned(
      new AllocationTrackingContext3D);
  AllocationTrackingContext3D* context = context_owned.get();
  context_owned->set_support_compressed_texture_etc1(true);

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));

  gfx::Size size(4, 4);
  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(output_surface.get(),
                               shared_bitmap_manager_.get(),
                               0,
                               false,
                               1,
                               false));
  int texture_id = 123;
  uint8_t pixels[8];

  ResourceProvider::ResourceId id = resource_provider->CreateResource(
      size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, ETC1);
  EXPECT_NE(0u, id);
  EXPECT_CALL(*context, NextTextureId()).WillOnce(Return(texture_id));
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id)).Times(3);
  EXPECT_CALL(*context,
              compressedTexImage2D(
                  _, 0, _, size.width(), size.height(), _, _, _)).Times(1);
  resource_provider->SetPixels(
      id, pixels, gfx::Rect(size), gfx::Rect(size), gfx::Vector2d(0, 0));

  EXPECT_CALL(*context, RetireTextureId(texture_id)).Times(1);
  resource_provider->DeleteResource(id);
}

INSTANTIATE_TEST_CASE_P(
    ResourceProviderTests,
    ResourceProviderTest,
    ::testing::Values(ResourceProvider::GLTexture, ResourceProvider::Bitmap));

class TextureIdAllocationTrackingContext : public TestWebGraphicsContext3D {
 public:
  virtual GLuint NextTextureId() OVERRIDE {
    base::AutoLock lock(namespace_->lock);
    return namespace_->next_texture_id++;
  }
  virtual void RetireTextureId(GLuint) OVERRIDE {}
  GLuint PeekTextureId() {
    base::AutoLock lock(namespace_->lock);
    return namespace_->next_texture_id;
  }
};

TEST(ResourceProviderTest, TextureAllocationChunkSize) {
  scoped_ptr<TextureIdAllocationTrackingContext> context_owned(
      new TextureIdAllocationTrackingContext);
  TextureIdAllocationTrackingContext* context = context_owned.get();

  FakeOutputSurfaceClient output_surface_client;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d(
      context_owned.PassAs<TestWebGraphicsContext3D>()));
  CHECK(output_surface->BindToClient(&output_surface_client));
  scoped_ptr<SharedBitmapManager> shared_bitmap_manager(
      new TestSharedBitmapManager());

  gfx::Size size(1, 1);
  ResourceFormat format = RGBA_8888;

  {
    size_t kTextureAllocationChunkSize = 1;
    scoped_ptr<ResourceProvider> resource_provider(
        ResourceProvider::Create(output_surface.get(),
                                 shared_bitmap_manager.get(),
                                 0,
                                 false,
                                 kTextureAllocationChunkSize,
                                 false));

    ResourceProvider::ResourceId id = resource_provider->CreateResource(
        size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
    resource_provider->AllocateForTesting(id);
    Mock::VerifyAndClearExpectations(context);

    DCHECK_EQ(2u, context->PeekTextureId());
    resource_provider->DeleteResource(id);
  }

  {
    size_t kTextureAllocationChunkSize = 8;
    scoped_ptr<ResourceProvider> resource_provider(
        ResourceProvider::Create(output_surface.get(),
                                 shared_bitmap_manager.get(),
                                 0,
                                 false,
                                 kTextureAllocationChunkSize,
                                 false));

    ResourceProvider::ResourceId id = resource_provider->CreateResource(
        size, GL_CLAMP_TO_EDGE, ResourceProvider::TextureUsageAny, format);
    resource_provider->AllocateForTesting(id);
    Mock::VerifyAndClearExpectations(context);

    DCHECK_EQ(10u, context->PeekTextureId());
    resource_provider->DeleteResource(id);
  }
}

}  // namespace
}  // namespace cc
