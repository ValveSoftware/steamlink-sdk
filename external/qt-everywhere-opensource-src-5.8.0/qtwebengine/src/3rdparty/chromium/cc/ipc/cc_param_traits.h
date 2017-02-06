// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// IPC Messages sent between compositor instances.

#ifndef CC_IPC_CC_PARAM_TRAITS_H_
#define CC_IPC_CC_PARAM_TRAITS_H_

#include "cc/ipc/cc_ipc_export.h"
#include "cc/ipc/cc_param_traits_macros.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/output/filter_operation.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/stream_video_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "gpu/ipc/common/gpu_command_buffer_traits.h"
#include "ipc/ipc_message_macros.h"

namespace gfx {
class Transform;
}

namespace cc {
class FilterOperations;
}

namespace IPC {

template <>
struct ParamTraits<cc::FilterOperation> {
  typedef cc::FilterOperation param_type;
  static void GetSize(base::PickleSizer* s, const param_type& p);
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<cc::FilterOperations> {
  typedef cc::FilterOperations param_type;
  static void GetSize(base::PickleSizer* s, const param_type& p);
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<sk_sp<SkImageFilter>> {
  typedef sk_sp<SkImageFilter> param_type;
  static void GetSize(base::PickleSizer* s, const param_type& p);
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct CC_IPC_EXPORT ParamTraits<cc::RenderPass> {
  typedef cc::RenderPass param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct CC_IPC_EXPORT ParamTraits<cc::SurfaceId> {
  typedef cc::SurfaceId param_type;
  static void GetSize(base::PickleSizer* s, const param_type& p);
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct CC_IPC_EXPORT ParamTraits<cc::CompositorFrame> {
  typedef cc::CompositorFrame param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct CC_IPC_EXPORT ParamTraits<cc::CompositorFrameAck> {
  typedef cc::CompositorFrameAck param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct CC_IPC_EXPORT ParamTraits<cc::DelegatedFrameData> {
  typedef cc::DelegatedFrameData param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct CC_IPC_EXPORT ParamTraits<cc::DrawQuad::Resources> {
  typedef cc::DrawQuad::Resources param_type;
  static void GetSize(base::PickleSizer* s, const param_type& p);
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct CC_IPC_EXPORT ParamTraits<cc::StreamVideoDrawQuad::OverlayResources> {
  typedef cc::StreamVideoDrawQuad::OverlayResources param_type;
  static void GetSize(base::PickleSizer* s, const param_type& p);
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct CC_IPC_EXPORT ParamTraits<cc::TextureDrawQuad::OverlayResources> {
  typedef cc::TextureDrawQuad::OverlayResources param_type;
  static void GetSize(base::PickleSizer* s, const param_type& p);
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CC_IPC_CC_PARAM_TRAITS_H_
