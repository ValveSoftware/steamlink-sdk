// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_TEXTURE_LAYER_H_
#define CC_LAYERS_TEXTURE_LAYER_H_

#include <string>

#include "base/callback.h"
#include "base/synchronization/lock.h"
#include "cc/base/cc_export.h"
#include "cc/layers/layer.h"
#include "cc/resources/texture_mailbox.h"

namespace cc {
class BlockingTaskRunner;
class SingleReleaseCallback;
class TextureLayerClient;

// A Layer containing a the rendered output of a plugin instance.
class CC_EXPORT TextureLayer : public Layer {
 public:
  class CC_EXPORT TextureMailboxHolder
      : public base::RefCountedThreadSafe<TextureMailboxHolder> {
   public:
    class CC_EXPORT MainThreadReference {
     public:
      explicit MainThreadReference(TextureMailboxHolder* holder);
      ~MainThreadReference();
      TextureMailboxHolder* holder() { return holder_.get(); }

     private:
      scoped_refptr<TextureMailboxHolder> holder_;
      DISALLOW_COPY_AND_ASSIGN(MainThreadReference);
    };

    const TextureMailbox& mailbox() const { return mailbox_; }
    void Return(uint32 sync_point, bool is_lost);

    // Gets a ReleaseCallback that can be called from another thread. Note: the
    // caller must ensure the callback is called.
    scoped_ptr<SingleReleaseCallback> GetCallbackForImplThread();

   protected:
    friend class TextureLayer;

    // Protected visiblity so only TextureLayer and unit tests can create these.
    static scoped_ptr<MainThreadReference> Create(
        const TextureMailbox& mailbox,
        scoped_ptr<SingleReleaseCallback> release_callback);
    virtual ~TextureMailboxHolder();

   private:
    friend class base::RefCountedThreadSafe<TextureMailboxHolder>;
    friend class MainThreadReference;
    explicit TextureMailboxHolder(
        const TextureMailbox& mailbox,
        scoped_ptr<SingleReleaseCallback> release_callback);

    void InternalAddRef();
    void InternalRelease();
    void ReturnAndReleaseOnImplThread(uint32 sync_point, bool is_lost);

    // This member is thread safe, and is accessed on main and impl threads.
    const scoped_refptr<BlockingTaskRunner> message_loop_;

    // These members are only accessed on the main thread, or on the impl thread
    // during commit where the main thread is blocked.
    unsigned internal_references_;
    TextureMailbox mailbox_;
    scoped_ptr<SingleReleaseCallback> release_callback_;

    // This lock guards the sync_point_ and is_lost_ fields because they can be
    // accessed on both the impl and main thread. We do this to ensure that the
    // values of these fields are well-ordered such that the last call to
    // ReturnAndReleaseOnImplThread() defines their values.
    base::Lock arguments_lock_;
    uint32 sync_point_;
    bool is_lost_;
    DISALLOW_COPY_AND_ASSIGN(TextureMailboxHolder);
  };

  // Used when mailbox names are specified instead of texture IDs.
  static scoped_refptr<TextureLayer> CreateForMailbox(
      TextureLayerClient* client);

  // Resets the client, which also resets the texture.
  void ClearClient();

  // Resets the texture.
  void ClearTexture();

  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;

  // Sets whether this texture should be Y-flipped at draw time. Defaults to
  // true.
  void SetFlipped(bool flipped);

  // Sets a UV transform to be used at draw time. Defaults to (0, 0) and (1, 1).
  void SetUV(const gfx::PointF& top_left, const gfx::PointF& bottom_right);

  // Sets an opacity value per vertex. It will be multiplied by the layer
  // opacity value.
  void SetVertexOpacity(float bottom_left,
                        float top_left,
                        float top_right,
                        float bottom_right);

  // Sets whether the alpha channel is premultiplied or unpremultiplied.
  // Defaults to true.
  void SetPremultipliedAlpha(bool premultiplied_alpha);

  // Sets whether the texture should be blended with the background color
  // at draw time. Defaults to false.
  void SetBlendBackgroundColor(bool blend);

  // Sets whether this context should rate limit on damage to prevent too many
  // frames from being queued up before the compositor gets a chance to run.
  // Requires a non-nil client.  Defaults to false.
  void SetRateLimitContext(bool rate_limit);

  // Code path for plugins which supply their own mailbox.
  void SetTextureMailbox(const TextureMailbox& mailbox,
                         scoped_ptr<SingleReleaseCallback> release_callback);

  // Use this for special cases where the same texture is used to back the
  // TextureLayer across all frames.
  // WARNING: DON'T ACTUALLY USE THIS WHAT YOU ARE DOING IS WRONG.
  // TODO(danakj): Remove this when pepper doesn't need it. crbug.com/350204
  void SetTextureMailboxWithoutReleaseCallback(const TextureMailbox& mailbox);

  virtual void SetNeedsDisplayRect(const gfx::RectF& dirty_rect) OVERRIDE;

  virtual void SetLayerTreeHost(LayerTreeHost* layer_tree_host) OVERRIDE;
  virtual bool DrawsContent() const OVERRIDE;
  virtual bool Update(ResourceUpdateQueue* queue,
                      const OcclusionTracker<Layer>* occlusion) OVERRIDE;
  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE;
  virtual Region VisibleContentOpaqueRegion() const OVERRIDE;

 protected:
  explicit TextureLayer(TextureLayerClient* client);
  virtual ~TextureLayer();

 private:
  void SetTextureMailboxInternal(
      const TextureMailbox& mailbox,
      scoped_ptr<SingleReleaseCallback> release_callback,
      bool requires_commit,
      bool allow_mailbox_reuse);

  TextureLayerClient* client_;

  bool flipped_;
  gfx::PointF uv_top_left_;
  gfx::PointF uv_bottom_right_;
  // [bottom left, top left, top right, bottom right]
  float vertex_opacity_[4];
  bool premultiplied_alpha_;
  bool blend_background_color_;
  bool rate_limit_context_;

  scoped_ptr<TextureMailboxHolder::MainThreadReference> holder_ref_;
  bool needs_set_mailbox_;

  DISALLOW_COPY_AND_ASSIGN(TextureLayer);
};

}  // namespace cc
#endif  // CC_LAYERS_TEXTURE_LAYER_H_
