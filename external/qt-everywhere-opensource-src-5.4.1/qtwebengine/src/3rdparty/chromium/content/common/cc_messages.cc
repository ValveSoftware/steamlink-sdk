// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cc_messages.h"

#include "cc/output/compositor_frame.h"
#include "cc/output/filter_operations.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/content_switches.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkFlattenableSerialization.h"
#include "ui/gfx/transform.h"

namespace IPC {

void ParamTraits<cc::FilterOperation>::Write(
    Message* m, const param_type& p) {
  WriteParam(m, p.type());
  switch (p.type()) {
    case cc::FilterOperation::GRAYSCALE:
    case cc::FilterOperation::SEPIA:
    case cc::FilterOperation::SATURATE:
    case cc::FilterOperation::HUE_ROTATE:
    case cc::FilterOperation::INVERT:
    case cc::FilterOperation::BRIGHTNESS:
    case cc::FilterOperation::SATURATING_BRIGHTNESS:
    case cc::FilterOperation::CONTRAST:
    case cc::FilterOperation::OPACITY:
    case cc::FilterOperation::BLUR:
      WriteParam(m, p.amount());
      break;
    case cc::FilterOperation::DROP_SHADOW:
      WriteParam(m, p.drop_shadow_offset());
      WriteParam(m, p.amount());
      WriteParam(m, p.drop_shadow_color());
      break;
    case cc::FilterOperation::COLOR_MATRIX:
      for (int i = 0; i < 20; ++i)
        WriteParam(m, p.matrix()[i]);
      break;
    case cc::FilterOperation::ZOOM:
      WriteParam(m, p.amount());
      WriteParam(m, p.zoom_inset());
      break;
    case cc::FilterOperation::REFERENCE:
      WriteParam(m, p.image_filter());
      break;
    case cc::FilterOperation::ALPHA_THRESHOLD:
      NOTREACHED();
      break;
  }
}

bool ParamTraits<cc::FilterOperation>::Read(
    const Message* m, PickleIterator* iter, param_type* r) {
  cc::FilterOperation::FilterType type;
  float amount;
  gfx::Point drop_shadow_offset;
  SkColor drop_shadow_color;
  SkScalar matrix[20];
  int zoom_inset;

  if (!ReadParam(m, iter, &type))
    return false;
  r->set_type(type);

  bool success = false;
  switch (type) {
    case cc::FilterOperation::GRAYSCALE:
    case cc::FilterOperation::SEPIA:
    case cc::FilterOperation::SATURATE:
    case cc::FilterOperation::HUE_ROTATE:
    case cc::FilterOperation::INVERT:
    case cc::FilterOperation::BRIGHTNESS:
    case cc::FilterOperation::SATURATING_BRIGHTNESS:
    case cc::FilterOperation::CONTRAST:
    case cc::FilterOperation::OPACITY:
    case cc::FilterOperation::BLUR:
      if (ReadParam(m, iter, &amount)) {
        r->set_amount(amount);
        success = true;
      }
      break;
    case cc::FilterOperation::DROP_SHADOW:
      if (ReadParam(m, iter, &drop_shadow_offset) &&
          ReadParam(m, iter, &amount) &&
          ReadParam(m, iter, &drop_shadow_color)) {
        r->set_drop_shadow_offset(drop_shadow_offset);
        r->set_amount(amount);
        r->set_drop_shadow_color(drop_shadow_color);
        success = true;
      }
      break;
    case cc::FilterOperation::COLOR_MATRIX: {
      int i;
      for (i = 0; i < 20; ++i) {
        if (!ReadParam(m, iter, &matrix[i]))
          break;
      }
      if (i == 20) {
        r->set_matrix(matrix);
        success = true;
      }
      break;
    }
    case cc::FilterOperation::ZOOM:
      if (ReadParam(m, iter, &amount) &&
          ReadParam(m, iter, &zoom_inset) &&
          amount >= 0.f &&
          zoom_inset >= 0) {
        r->set_amount(amount);
        r->set_zoom_inset(zoom_inset);
        success = true;
      }
      break;
    case cc::FilterOperation::REFERENCE: {
      skia::RefPtr<SkImageFilter> filter;
      if (!ReadParam(m, iter, &filter)) {
        success = false;
        break;
      }
      r->set_image_filter(filter);
      success = true;
      break;
    }
    case cc::FilterOperation::ALPHA_THRESHOLD:
      break;
  }
  return success;
}

void ParamTraits<cc::FilterOperation>::Log(
    const param_type& p, std::string* l) {
  l->append("(");
  LogParam(static_cast<unsigned>(p.type()), l);
  l->append(", ");

  switch (p.type()) {
    case cc::FilterOperation::GRAYSCALE:
    case cc::FilterOperation::SEPIA:
    case cc::FilterOperation::SATURATE:
    case cc::FilterOperation::HUE_ROTATE:
    case cc::FilterOperation::INVERT:
    case cc::FilterOperation::BRIGHTNESS:
    case cc::FilterOperation::SATURATING_BRIGHTNESS:
    case cc::FilterOperation::CONTRAST:
    case cc::FilterOperation::OPACITY:
    case cc::FilterOperation::BLUR:
      LogParam(p.amount(), l);
      break;
    case cc::FilterOperation::DROP_SHADOW:
      LogParam(p.drop_shadow_offset(), l);
      l->append(", ");
      LogParam(p.amount(), l);
      l->append(", ");
      LogParam(p.drop_shadow_color(), l);
      break;
    case cc::FilterOperation::COLOR_MATRIX:
      for (int i = 0; i < 20; ++i) {
        if (i)
          l->append(", ");
        LogParam(p.matrix()[i], l);
      }
      break;
    case cc::FilterOperation::ZOOM:
      LogParam(p.amount(), l);
      l->append(", ");
      LogParam(p.zoom_inset(), l);
      break;
    case cc::FilterOperation::REFERENCE:
      LogParam(p.image_filter(), l);
      break;
    case cc::FilterOperation::ALPHA_THRESHOLD:
      NOTREACHED();
      break;
  }
  l->append(")");
}

void ParamTraits<cc::FilterOperations>::Write(
    Message* m, const param_type& p) {
  WriteParam(m, p.size());
  for (std::size_t i = 0; i < p.size(); ++i) {
    WriteParam(m, p.at(i));
  }
}

bool ParamTraits<cc::FilterOperations>::Read(
    const Message* m, PickleIterator* iter, param_type* r) {
  size_t count;
  if (!ReadParam(m, iter, &count))
    return false;

  for (std::size_t i = 0; i < count; ++i) {
    cc::FilterOperation op = cc::FilterOperation::CreateEmptyFilter();
    if (!ReadParam(m, iter, &op))
      return false;
    r->Append(op);
  }
  return true;
}

void ParamTraits<cc::FilterOperations>::Log(
    const param_type& p, std::string* l) {
  l->append("(");
  for (std::size_t i = 0; i < p.size(); ++i) {
    if (i)
      l->append(", ");
    LogParam(p.at(i), l);
  }
  l->append(")");
}

void ParamTraits<skia::RefPtr<SkImageFilter> >::Write(
    Message* m, const param_type& p) {
  SkImageFilter* filter = p.get();
  if (filter) {
    skia::RefPtr<SkData> data =
        skia::AdoptRef(SkValidatingSerializeFlattenable(filter));
    m->WriteData(static_cast<const char*>(data->data()), data->size());
  } else {
    m->WriteData(0, 0);
  }
}

bool ParamTraits<skia::RefPtr<SkImageFilter> >::Read(
    const Message* m, PickleIterator* iter, param_type* r) {
  const char* data = 0;
  int length = 0;
  if (!m->ReadData(iter, &data, &length))
    return false;
  if (length > 0) {
    SkFlattenable* flattenable = SkValidatingDeserializeFlattenable(
        data, length, SkImageFilter::GetFlattenableType());
    *r = skia::AdoptRef(static_cast<SkImageFilter*>(flattenable));
  } else {
    r->clear();
  }
  return true;
}

void ParamTraits<skia::RefPtr<SkImageFilter> >::Log(
    const param_type& p, std::string* l) {
  l->append("(");
  LogParam(p.get() ? p->countInputs() : 0, l);
  l->append(")");
}

void ParamTraits<gfx::Transform>::Write(
    Message* m, const param_type& p) {
#ifdef SK_MSCALAR_IS_FLOAT
  float column_major_data[16];
  p.matrix().asColMajorf(column_major_data);
#else
  double column_major_data[16];
  p.matrix().asColMajord(column_major_data);
#endif
  m->WriteBytes(&column_major_data, sizeof(SkMScalar) * 16);
}

bool ParamTraits<gfx::Transform>::Read(
    const Message* m, PickleIterator* iter, param_type* r) {
  const char* column_major_data;
  if (!m->ReadBytes(iter, &column_major_data, sizeof(SkMScalar) * 16))
    return false;
  r->matrix().setColMajor(
      reinterpret_cast<const SkMScalar*>(column_major_data));
  return true;
}

void ParamTraits<gfx::Transform>::Log(
    const param_type& p, std::string* l) {
#ifdef SK_MSCALAR_IS_FLOAT
  float row_major_data[16];
  p.matrix().asRowMajorf(row_major_data);
#else
  double row_major_data[16];
  p.matrix().asRowMajord(row_major_data);
#endif
  l->append("(");
  for (int i = 0; i < 16; ++i) {
    if (i > 0)
      l->append(", ");
    LogParam(row_major_data[i], l);
  }
  l->append(") ");
}

void ParamTraits<cc::RenderPass>::Write(
    Message* m, const param_type& p) {
  WriteParam(m, p.id);
  WriteParam(m, p.output_rect);
  WriteParam(m, p.damage_rect);
  WriteParam(m, p.transform_to_root_target);
  WriteParam(m, p.has_transparent_background);
  WriteParam(m, p.shared_quad_state_list.size());
  WriteParam(m, p.quad_list.size());

  size_t shared_quad_state_index = 0;
  size_t last_shared_quad_state_index = kuint32max;
  for (size_t i = 0; i < p.quad_list.size(); ++i) {
    const cc::DrawQuad* quad = p.quad_list[i];
    DCHECK(quad->rect.Contains(quad->visible_rect))
        << quad->material << " rect: " << quad->rect.ToString()
        << " visible_rect: " << quad->visible_rect.ToString();
    DCHECK(quad->opaque_rect.IsEmpty() ||
           quad->rect.Contains(quad->opaque_rect))
        << quad->material << " rect: " << quad->rect.ToString()
        << " opaque_rect: " << quad->opaque_rect.ToString();

    switch (quad->material) {
      case cc::DrawQuad::CHECKERBOARD:
        WriteParam(m, *cc::CheckerboardDrawQuad::MaterialCast(quad));
        break;
      case cc::DrawQuad::DEBUG_BORDER:
        WriteParam(m, *cc::DebugBorderDrawQuad::MaterialCast(quad));
        break;
      case cc::DrawQuad::IO_SURFACE_CONTENT:
        WriteParam(m, *cc::IOSurfaceDrawQuad::MaterialCast(quad));
        break;
      case cc::DrawQuad::PICTURE_CONTENT:
        NOTREACHED();
        break;
      case cc::DrawQuad::TEXTURE_CONTENT:
        WriteParam(m, *cc::TextureDrawQuad::MaterialCast(quad));
        break;
      case cc::DrawQuad::RENDER_PASS:
        WriteParam(m, *cc::RenderPassDrawQuad::MaterialCast(quad));
        break;
      case cc::DrawQuad::SOLID_COLOR:
        WriteParam(m, *cc::SolidColorDrawQuad::MaterialCast(quad));
        break;
      case cc::DrawQuad::SURFACE_CONTENT:
        WriteParam(m, *cc::SurfaceDrawQuad::MaterialCast(quad));
        break;
      case cc::DrawQuad::TILED_CONTENT:
        WriteParam(m, *cc::TileDrawQuad::MaterialCast(quad));
        break;
      case cc::DrawQuad::STREAM_VIDEO_CONTENT:
        WriteParam(m, *cc::StreamVideoDrawQuad::MaterialCast(quad));
        break;
      case cc::DrawQuad::YUV_VIDEO_CONTENT:
        WriteParam(m, *cc::YUVVideoDrawQuad::MaterialCast(quad));
        break;
      case cc::DrawQuad::INVALID:
        break;
    }

    const cc::ScopedPtrVector<cc::SharedQuadState>& sqs_list =
        p.shared_quad_state_list;

    // This is an invalid index.
    size_t bad_index = sqs_list.size();

    // Null shared quad states should not occur.
    DCHECK(quad->shared_quad_state);
    if (!quad->shared_quad_state) {
      WriteParam(m, bad_index);
      continue;
    }

    // SharedQuadStates should appear in the order they are used by DrawQuads.
    // Find the SharedQuadState for this DrawQuad.
    while (shared_quad_state_index < sqs_list.size() &&
           quad->shared_quad_state != sqs_list[shared_quad_state_index])
      ++shared_quad_state_index;

    DCHECK_LT(shared_quad_state_index, sqs_list.size());
    if (shared_quad_state_index >= sqs_list.size()) {
      WriteParam(m, bad_index);
      continue;
    }

    WriteParam(m, shared_quad_state_index);
    if (shared_quad_state_index != last_shared_quad_state_index) {
      WriteParam(m, *sqs_list[shared_quad_state_index]);
      last_shared_quad_state_index = shared_quad_state_index;
    }
  }
}

static size_t ReserveSizeForRenderPassWrite(const cc::RenderPass& p) {
  size_t to_reserve = sizeof(cc::RenderPass);

  to_reserve += p.shared_quad_state_list.size() * sizeof(cc::SharedQuadState);

  // The shared_quad_state_index for each quad.
  to_reserve += p.quad_list.size() * sizeof(size_t);

  // The largest quad type, verified by a unit test.
  to_reserve += p.quad_list.size() * sizeof(cc::RenderPassDrawQuad);
  return to_reserve;
}

template<typename QuadType>
static scoped_ptr<cc::DrawQuad> ReadDrawQuad(const Message* m,
                                             PickleIterator* iter) {
  scoped_ptr<QuadType> quad = QuadType::Create();
  if (!ReadParam(m, iter, quad.get()))
    return scoped_ptr<QuadType>().template PassAs<cc::DrawQuad>();
  return quad.template PassAs<cc::DrawQuad>();
}

bool ParamTraits<cc::RenderPass>::Read(
    const Message* m, PickleIterator* iter, param_type* p) {
  cc::RenderPass::Id id(-1, -1);
  gfx::Rect output_rect;
  gfx::Rect damage_rect;
  gfx::Transform transform_to_root_target;
  bool has_transparent_background;
  size_t shared_quad_state_list_size;
  size_t quad_list_size;

  if (!ReadParam(m, iter, &id) ||
      !ReadParam(m, iter, &output_rect) ||
      !ReadParam(m, iter, &damage_rect) ||
      !ReadParam(m, iter, &transform_to_root_target) ||
      !ReadParam(m, iter, &has_transparent_background) ||
      !ReadParam(m, iter, &shared_quad_state_list_size) ||
      !ReadParam(m, iter, &quad_list_size))
    return false;

  p->SetAll(id,
            output_rect,
            damage_rect,
            transform_to_root_target,
            has_transparent_background);

  size_t last_shared_quad_state_index = kuint32max;
  for (size_t i = 0; i < quad_list_size; ++i) {
    cc::DrawQuad::Material material;
    PickleIterator temp_iter = *iter;
    if (!ReadParam(m, &temp_iter, &material))
      return false;

    scoped_ptr<cc::DrawQuad> draw_quad;
    switch (material) {
      case cc::DrawQuad::CHECKERBOARD:
        draw_quad = ReadDrawQuad<cc::CheckerboardDrawQuad>(m, iter);
        break;
      case cc::DrawQuad::DEBUG_BORDER:
        draw_quad = ReadDrawQuad<cc::DebugBorderDrawQuad>(m, iter);
        break;
      case cc::DrawQuad::IO_SURFACE_CONTENT:
        draw_quad = ReadDrawQuad<cc::IOSurfaceDrawQuad>(m, iter);
        break;
      case cc::DrawQuad::PICTURE_CONTENT:
        NOTREACHED();
        return false;
      case cc::DrawQuad::SURFACE_CONTENT:
        draw_quad = ReadDrawQuad<cc::SurfaceDrawQuad>(m, iter);
        break;
      case cc::DrawQuad::TEXTURE_CONTENT:
        draw_quad = ReadDrawQuad<cc::TextureDrawQuad>(m, iter);
        break;
      case cc::DrawQuad::RENDER_PASS:
        draw_quad = ReadDrawQuad<cc::RenderPassDrawQuad>(m, iter);
        break;
      case cc::DrawQuad::SOLID_COLOR:
        draw_quad = ReadDrawQuad<cc::SolidColorDrawQuad>(m, iter);
        break;
      case cc::DrawQuad::TILED_CONTENT:
        draw_quad = ReadDrawQuad<cc::TileDrawQuad>(m, iter);
        break;
      case cc::DrawQuad::STREAM_VIDEO_CONTENT:
        draw_quad = ReadDrawQuad<cc::StreamVideoDrawQuad>(m, iter);
        break;
      case cc::DrawQuad::YUV_VIDEO_CONTENT:
        draw_quad = ReadDrawQuad<cc::YUVVideoDrawQuad>(m, iter);
        break;
      case cc::DrawQuad::INVALID:
        break;
    }
    if (!draw_quad)
      return false;
    if (!draw_quad->rect.Contains(draw_quad->visible_rect)) {
      LOG(ERROR) << "Quad with invalid visible rect " << draw_quad->material
                 << " rect: " << draw_quad->rect.ToString()
                 << " visible_rect: " << draw_quad->visible_rect.ToString();
      return false;
    }
    if (!draw_quad->opaque_rect.IsEmpty() &&
        !draw_quad->rect.Contains(draw_quad->opaque_rect)) {
      LOG(ERROR) << "Quad with invalid opaque rect " << draw_quad->material
                 << " rect: " << draw_quad->rect.ToString()
                 << " opaque_rect: " << draw_quad->opaque_rect.ToString();
      return false;
    }

    size_t shared_quad_state_index;
    if (!ReadParam(m, iter, &shared_quad_state_index))
      return false;
    if (shared_quad_state_index >= shared_quad_state_list_size)
      return false;
    // SharedQuadState indexes should be in ascending order.
    if (last_shared_quad_state_index != kuint32max &&
        shared_quad_state_index < last_shared_quad_state_index)
      return false;

    // If the quad has a new shared quad state, read it in.
    if (last_shared_quad_state_index != shared_quad_state_index) {
      cc::SharedQuadState* state = p->CreateAndAppendSharedQuadState();
      if (!ReadParam(m, iter, state))
        return false;
      last_shared_quad_state_index = shared_quad_state_index;
    }

    draw_quad->shared_quad_state = p->shared_quad_state_list.back();
    p->quad_list.push_back(draw_quad.Pass());
  }

  return true;
}

void ParamTraits<cc::RenderPass>::Log(
    const param_type& p, std::string* l) {
  l->append("RenderPass((");
  LogParam(p.id, l);
  l->append("), ");
  LogParam(p.output_rect, l);
  l->append(", ");
  LogParam(p.damage_rect, l);
  l->append(", ");
  LogParam(p.transform_to_root_target, l);
  l->append(", ");
  LogParam(p.has_transparent_background, l);
  l->append(", ");

  l->append("[");
  for (size_t i = 0; i < p.shared_quad_state_list.size(); ++i) {
    if (i)
      l->append(", ");
    LogParam(*p.shared_quad_state_list[i], l);
  }
  l->append("], [");
  for (size_t i = 0; i < p.quad_list.size(); ++i) {
    if (i)
      l->append(", ");
    const cc::DrawQuad* quad = p.quad_list[i];
    switch (quad->material) {
      case cc::DrawQuad::CHECKERBOARD:
        LogParam(*cc::CheckerboardDrawQuad::MaterialCast(quad), l);
        break;
      case cc::DrawQuad::DEBUG_BORDER:
        LogParam(*cc::DebugBorderDrawQuad::MaterialCast(quad), l);
        break;
      case cc::DrawQuad::IO_SURFACE_CONTENT:
        LogParam(*cc::IOSurfaceDrawQuad::MaterialCast(quad), l);
        break;
      case cc::DrawQuad::PICTURE_CONTENT:
        NOTREACHED();
        break;
      case cc::DrawQuad::TEXTURE_CONTENT:
        LogParam(*cc::TextureDrawQuad::MaterialCast(quad), l);
        break;
      case cc::DrawQuad::RENDER_PASS:
        LogParam(*cc::RenderPassDrawQuad::MaterialCast(quad), l);
        break;
      case cc::DrawQuad::SOLID_COLOR:
        LogParam(*cc::SolidColorDrawQuad::MaterialCast(quad), l);
        break;
      case cc::DrawQuad::SURFACE_CONTENT:
        LogParam(*cc::SurfaceDrawQuad::MaterialCast(quad), l);
        break;
      case cc::DrawQuad::TILED_CONTENT:
        LogParam(*cc::TileDrawQuad::MaterialCast(quad), l);
        break;
      case cc::DrawQuad::STREAM_VIDEO_CONTENT:
        LogParam(*cc::StreamVideoDrawQuad::MaterialCast(quad), l);
        break;
      case cc::DrawQuad::YUV_VIDEO_CONTENT:
        LogParam(*cc::YUVVideoDrawQuad::MaterialCast(quad), l);
        break;
      case cc::DrawQuad::INVALID:
        break;
    }
  }
  l->append("])");
}

namespace {
  enum CompositorFrameType {
    NO_FRAME,
    DELEGATED_FRAME,
    GL_FRAME,
    SOFTWARE_FRAME,
  };
}

void ParamTraits<cc::CompositorFrame>::Write(Message* m,
                                             const param_type& p) {
  WriteParam(m, p.metadata);
  if (p.delegated_frame_data) {
    DCHECK(!p.gl_frame_data);
    DCHECK(!p.software_frame_data);
    WriteParam(m, static_cast<int>(DELEGATED_FRAME));
    WriteParam(m, *p.delegated_frame_data);
  } else if (p.gl_frame_data) {
    DCHECK(!p.software_frame_data);
    WriteParam(m, static_cast<int>(GL_FRAME));
    WriteParam(m, *p.gl_frame_data);
  } else if (p.software_frame_data) {
    WriteParam(m, static_cast<int>(SOFTWARE_FRAME));
    WriteParam(m, *p.software_frame_data);
  } else {
    WriteParam(m, static_cast<int>(NO_FRAME));
  }
}

bool ParamTraits<cc::CompositorFrame>::Read(const Message* m,
                                            PickleIterator* iter,
                                            param_type* p) {
  if (!ReadParam(m, iter, &p->metadata))
    return false;

  int compositor_frame_type;
  if (!ReadParam(m, iter, &compositor_frame_type))
    return false;

  switch (compositor_frame_type) {
    case DELEGATED_FRAME:
      p->delegated_frame_data.reset(new cc::DelegatedFrameData());
      if (!ReadParam(m, iter, p->delegated_frame_data.get()))
        return false;
      break;
    case GL_FRAME:
      p->gl_frame_data.reset(new cc::GLFrameData());
      if (!ReadParam(m, iter, p->gl_frame_data.get()))
        return false;
      break;
    case SOFTWARE_FRAME:
      p->software_frame_data.reset(new cc::SoftwareFrameData());
      if (!ReadParam(m, iter, p->software_frame_data.get()))
        return false;
      break;
    case NO_FRAME:
      break;
    default:
      return false;
  }
  return true;
}

void ParamTraits<cc::CompositorFrame>::Log(const param_type& p,
                                           std::string* l) {
  l->append("CompositorFrame(");
  LogParam(p.metadata, l);
  l->append(", ");
  if (p.delegated_frame_data)
    LogParam(*p.delegated_frame_data, l);
  else if (p.gl_frame_data)
    LogParam(*p.gl_frame_data, l);
  else if (p.software_frame_data)
    LogParam(*p.software_frame_data, l);
  l->append(")");
}

void ParamTraits<cc::CompositorFrameAck>::Write(Message* m,
                                                const param_type& p) {
  WriteParam(m, p.resources);
  WriteParam(m, p.last_software_frame_id);
  if (p.gl_frame_data) {
    WriteParam(m, static_cast<int>(GL_FRAME));
    WriteParam(m, *p.gl_frame_data);
  } else {
    WriteParam(m, static_cast<int>(NO_FRAME));
  }
}

bool ParamTraits<cc::CompositorFrameAck>::Read(const Message* m,
                                               PickleIterator* iter,
                                               param_type* p) {
  if (!ReadParam(m, iter, &p->resources))
    return false;

  if (!ReadParam(m, iter, &p->last_software_frame_id))
    return false;

  int compositor_frame_type;
  if (!ReadParam(m, iter, &compositor_frame_type))
    return false;

  switch (compositor_frame_type) {
    case NO_FRAME:
      break;
    case GL_FRAME:
      p->gl_frame_data.reset(new cc::GLFrameData());
      if (!ReadParam(m, iter, p->gl_frame_data.get()))
        return false;
      break;
    default:
      return false;
  }
  return true;
}

void ParamTraits<cc::CompositorFrameAck>::Log(const param_type& p,
                                              std::string* l) {
  l->append("CompositorFrameAck(");
  LogParam(p.resources, l);
  l->append(", ");
  LogParam(p.last_software_frame_id, l);
  l->append(", ");
  if (p.gl_frame_data)
    LogParam(*p.gl_frame_data, l);
  l->append(")");
}

void ParamTraits<cc::DelegatedFrameData>::Write(Message* m,
                                                const param_type& p) {
  DCHECK_NE(0u, p.render_pass_list.size());

  size_t to_reserve = sizeof(p.device_scale_factor);
  to_reserve += p.resource_list.size() * sizeof(cc::TransferableResource);
  for (size_t i = 0; i < p.render_pass_list.size(); ++i) {
    const cc::RenderPass* pass = p.render_pass_list[i];
    to_reserve += ReserveSizeForRenderPassWrite(*pass);
  }
  m->Reserve(to_reserve);

  WriteParam(m, p.device_scale_factor);
  WriteParam(m, p.resource_list);
  WriteParam(m, p.render_pass_list.size());
  for (size_t i = 0; i < p.render_pass_list.size(); ++i)
    WriteParam(m, *p.render_pass_list[i]);
}

bool ParamTraits<cc::DelegatedFrameData>::Read(const Message* m,
                                               PickleIterator* iter,
                                               param_type* p) {
  if (!ReadParam(m, iter, &p->device_scale_factor))
    return false;

  const static size_t kMaxRenderPasses = 10000;

  size_t num_render_passes;
  if (!ReadParam(m, iter, &p->resource_list) ||
      !ReadParam(m, iter, &num_render_passes) ||
      num_render_passes > kMaxRenderPasses || num_render_passes == 0)
    return false;
  for (size_t i = 0; i < num_render_passes; ++i) {
    scoped_ptr<cc::RenderPass> render_pass = cc::RenderPass::Create();
    if (!ReadParam(m, iter, render_pass.get()))
      return false;
    p->render_pass_list.push_back(render_pass.Pass());
  }
  return true;
}

void ParamTraits<cc::DelegatedFrameData>::Log(const param_type& p,
                                              std::string* l) {
  l->append("DelegatedFrameData(");
  LogParam(p.device_scale_factor, l);
  LogParam(p.resource_list, l);
  l->append(", [");
  for (size_t i = 0; i < p.render_pass_list.size(); ++i) {
    if (i)
      l->append(", ");
    LogParam(*p.render_pass_list[i], l);
  }
  l->append("])");
}

void ParamTraits<cc::SoftwareFrameData>::Write(Message* m,
                                               const param_type& p) {
  DCHECK(cc::SharedBitmap::VerifySizeInBytes(p.size));

  m->Reserve(sizeof(cc::SoftwareFrameData));
  WriteParam(m, p.id);
  WriteParam(m, p.size);
  WriteParam(m, p.damage_rect);
  WriteParam(m, p.bitmap_id);
}

bool ParamTraits<cc::SoftwareFrameData>::Read(const Message* m,
                                              PickleIterator* iter,
                                              param_type* p) {
  if (!ReadParam(m, iter, &p->id))
    return false;
  if (!ReadParam(m, iter, &p->size) ||
      !cc::SharedBitmap::VerifySizeInBytes(p->size))
    return false;
  if (!ReadParam(m, iter, &p->damage_rect))
    return false;
  if (!ReadParam(m, iter, &p->bitmap_id))
    return false;
  return true;
}

void ParamTraits<cc::SoftwareFrameData>::Log(const param_type& p,
                                             std::string* l) {
  l->append("SoftwareFrameData(");
  LogParam(p.id, l);
  l->append(", ");
  LogParam(p.size, l);
  l->append(", ");
  LogParam(p.damage_rect, l);
  l->append(", ");
  LogParam(p.bitmap_id, l);
  l->append(")");
}

}  // namespace IPC
