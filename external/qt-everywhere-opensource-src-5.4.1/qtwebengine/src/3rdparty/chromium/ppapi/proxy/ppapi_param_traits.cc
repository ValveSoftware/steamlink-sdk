// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppapi_param_traits.h"

#include <string.h>  // For memcpy

#include "ppapi/c/pp_resource.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/proxy/serialized_flash_menu.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/private/ppb_x509_certificate_private_shared.h"

namespace IPC {

namespace {

// Deserializes a vector from IPC. This special version must be used instead
// of the default IPC version when the vector contains a SerializedVar, either
// directly or indirectly (i.e. a vector of objects that have a SerializedVar
// inside them).
//
// The default vector deserializer does resize and then we deserialize into
// those allocated slots. However, the implementation of vector (at least in
// GCC's implementation), creates a new empty object using the default
// constructor, and then sets the rest of the items to that empty one using the
// copy constructor.
//
// Since we allocate the inner class when you call the default constructor and
// transfer the inner class when you do operator=, the entire vector will end
// up referring to the same inner class. Deserializing into this will just end
// up overwriting the same item over and over, since all the SerializedVars
// will refer to the same thing.
//
// The solution is to make a new object for each deserialized item, and then
// add it to the vector one at a time.
template<typename T>
bool ReadVectorWithoutCopy(const Message* m,
                           PickleIterator* iter,
                           std::vector<T>* output) {
  // This part is just a copy of the the default ParamTraits vector Read().
  int size;
  // ReadLength() checks for < 0 itself.
  if (!m->ReadLength(iter, &size))
    return false;
  // Resizing beforehand is not safe, see BUG 1006367 for details.
  if (INT_MAX / sizeof(T) <= static_cast<size_t>(size))
    return false;

  output->reserve(size);
  for (int i = 0; i < size; i++) {
    T cur;
    if (!ReadParam(m, iter, &cur))
      return false;
    output->push_back(cur);
  }
  return true;
}

// This serializes the vector of items to the IPC message in exactly the same
// way as the "regular" IPC vector serializer does. But having the code here
// saves us from having to copy this code into all ParamTraits that use the
// ReadVectorWithoutCopy function for deserializing.
template<typename T>
void WriteVectorWithoutCopy(Message* m, const std::vector<T>& p) {
  WriteParam(m, static_cast<int>(p.size()));
  for (size_t i = 0; i < p.size(); i++)
    WriteParam(m, p[i]);
}

}  // namespace

// PP_Bool ---------------------------------------------------------------------

// static
void ParamTraits<PP_Bool>::Write(Message* m, const param_type& p) {
  ParamTraits<bool>::Write(m, PP_ToBool(p));
}

// static
bool ParamTraits<PP_Bool>::Read(const Message* m,
                                PickleIterator* iter,
                                param_type* r) {
  // We specifically want to be strict here about what types of input we accept,
  // which ParamTraits<bool> does for us. We don't want to deserialize "2" into
  // a PP_Bool, for example.
  bool result = false;
  if (!ParamTraits<bool>::Read(m, iter, &result))
    return false;
  *r = PP_FromBool(result);
  return true;
}

// static
void ParamTraits<PP_Bool>::Log(const param_type& p, std::string* l) {
}

// PP_NetAddress_Private -------------------------------------------------------

// static
void ParamTraits<PP_NetAddress_Private>::Write(Message* m,
                                               const param_type& p) {
  WriteParam(m, p.size);
  m->WriteBytes(p.data, static_cast<int>(p.size));
}

// static
bool ParamTraits<PP_NetAddress_Private>::Read(const Message* m,
                                              PickleIterator* iter,
                                              param_type* p) {
  uint16 size;
  if (!ReadParam(m, iter, &size))
    return false;
  if (size > sizeof(p->data))
    return false;
  p->size = size;

  const char* data;
  if (!m->ReadBytes(iter, &data, size))
    return false;
  memcpy(p->data, data, size);
  return true;
}

// static
void ParamTraits<PP_NetAddress_Private>::Log(const param_type& p,
                                             std::string* l) {
  l->append("<PP_NetAddress_Private (");
  LogParam(p.size, l);
  l->append(" bytes)>");
}

// HostResource ----------------------------------------------------------------

// static
void ParamTraits<ppapi::HostResource>::Write(Message* m,
                                             const param_type& p) {
  ParamTraits<PP_Instance>::Write(m, p.instance());
  ParamTraits<PP_Resource>::Write(m, p.host_resource());
}

// static
bool ParamTraits<ppapi::HostResource>::Read(const Message* m,
                                            PickleIterator* iter,
                                            param_type* r) {
  PP_Instance instance;
  PP_Resource resource;
  if (!ParamTraits<PP_Instance>::Read(m, iter, &instance) ||
      !ParamTraits<PP_Resource>::Read(m, iter, &resource))
    return false;
  r->SetHostResource(instance, resource);
  return true;
}

// static
void ParamTraits<ppapi::HostResource>::Log(const param_type& p,
                                           std::string* l) {
}

// SerializedVar ---------------------------------------------------------------

// static
void ParamTraits<ppapi::proxy::SerializedVar>::Write(Message* m,
                                                     const param_type& p) {
  p.WriteToMessage(m);
}

// static
bool ParamTraits<ppapi::proxy::SerializedVar>::Read(const Message* m,
                                                    PickleIterator* iter,
                                                    param_type* r) {
  return r->ReadFromMessage(m, iter);
}

// static
void ParamTraits<ppapi::proxy::SerializedVar>::Log(const param_type& p,
                                                   std::string* l) {
}

// std::vector<SerializedVar> --------------------------------------------------

void ParamTraits< std::vector<ppapi::proxy::SerializedVar> >::Write(
    Message* m,
    const param_type& p) {
  WriteVectorWithoutCopy(m, p);
}

// static
bool ParamTraits< std::vector<ppapi::proxy::SerializedVar> >::Read(
    const Message* m,
    PickleIterator* iter,
    param_type* r) {
  return ReadVectorWithoutCopy(m, iter, r);
}

// static
void ParamTraits< std::vector<ppapi::proxy::SerializedVar> >::Log(
    const param_type& p,
    std::string* l) {
}

// ppapi::PpapiPermissions -----------------------------------------------------

void ParamTraits<ppapi::PpapiPermissions>::Write(Message* m,
                                                 const param_type& p) {
  ParamTraits<uint32_t>::Write(m, p.GetBits());
}

// static
bool ParamTraits<ppapi::PpapiPermissions>::Read(const Message* m,
                                                PickleIterator* iter,
                                                param_type* r) {
  uint32_t bits;
  if (!ParamTraits<uint32_t>::Read(m, iter, &bits))
    return false;
  *r = ppapi::PpapiPermissions(bits);
  return true;
}

// static
void ParamTraits<ppapi::PpapiPermissions>::Log(const param_type& p,
                                               std::string* l) {
}

// SerializedHandle ------------------------------------------------------------

// static
void ParamTraits<ppapi::proxy::SerializedHandle>::Write(Message* m,
                                                        const param_type& p) {
  ppapi::proxy::SerializedHandle::WriteHeader(p.header(), m);
  switch (p.type()) {
    case ppapi::proxy::SerializedHandle::SHARED_MEMORY:
      ParamTraits<base::SharedMemoryHandle>::Write(m, p.shmem());
      break;
    case ppapi::proxy::SerializedHandle::SOCKET:
    case ppapi::proxy::SerializedHandle::FILE:
      ParamTraits<IPC::PlatformFileForTransit>::Write(m, p.descriptor());
      break;
    case ppapi::proxy::SerializedHandle::INVALID:
      break;
    // No default so the compiler will warn on new types.
  }
}

// static
bool ParamTraits<ppapi::proxy::SerializedHandle>::Read(const Message* m,
                                                       PickleIterator* iter,
                                                       param_type* r) {
  ppapi::proxy::SerializedHandle::Header header;
  if (!ppapi::proxy::SerializedHandle::ReadHeader(iter, &header))
    return false;
  switch (header.type) {
    case ppapi::proxy::SerializedHandle::SHARED_MEMORY: {
      base::SharedMemoryHandle handle;
      if (ParamTraits<base::SharedMemoryHandle>::Read(m, iter, &handle)) {
        r->set_shmem(handle, header.size);
        return true;
      }
      break;
    }
    case ppapi::proxy::SerializedHandle::SOCKET: {
      IPC::PlatformFileForTransit socket;
      if (ParamTraits<IPC::PlatformFileForTransit>::Read(m, iter, &socket)) {
        r->set_socket(socket);
        return true;
      }
      break;
    }
    case ppapi::proxy::SerializedHandle::FILE: {
      IPC::PlatformFileForTransit desc;
      if (ParamTraits<IPC::PlatformFileForTransit>::Read(m, iter, &desc)) {
        r->set_file_handle(desc, header.open_flags, header.file_io);
        return true;
      }
      break;
    }
    case ppapi::proxy::SerializedHandle::INVALID:
      return true;
    // No default so the compiler will warn us if a new type is added.
  }
  return false;
}

// static
void ParamTraits<ppapi::proxy::SerializedHandle>::Log(const param_type& p,
                                                      std::string* l) {
}

// PPBURLLoader_UpdateProgress_Params ------------------------------------------

// static
void ParamTraits<ppapi::proxy::PPBURLLoader_UpdateProgress_Params>::Write(
    Message* m,
    const param_type& p) {
  ParamTraits<PP_Instance>::Write(m, p.instance);
  ParamTraits<ppapi::HostResource>::Write(m, p.resource);
  ParamTraits<int64_t>::Write(m, p.bytes_sent);
  ParamTraits<int64_t>::Write(m, p.total_bytes_to_be_sent);
  ParamTraits<int64_t>::Write(m, p.bytes_received);
  ParamTraits<int64_t>::Write(m, p.total_bytes_to_be_received);
}

// static
bool ParamTraits<ppapi::proxy::PPBURLLoader_UpdateProgress_Params>::Read(
    const Message* m,
    PickleIterator* iter,
    param_type* r) {
  return
      ParamTraits<PP_Instance>::Read(m, iter, &r->instance) &&
      ParamTraits<ppapi::HostResource>::Read(m, iter, &r->resource) &&
      ParamTraits<int64_t>::Read(m, iter, &r->bytes_sent) &&
      ParamTraits<int64_t>::Read(m, iter, &r->total_bytes_to_be_sent) &&
      ParamTraits<int64_t>::Read(m, iter, &r->bytes_received) &&
      ParamTraits<int64_t>::Read(m, iter, &r->total_bytes_to_be_received);
}

// static
void ParamTraits<ppapi::proxy::PPBURLLoader_UpdateProgress_Params>::Log(
    const param_type& p,
    std::string* l) {
}

#if !defined(OS_NACL) && !defined(NACL_WIN64)
// PPBFlash_DrawGlyphs_Params --------------------------------------------------
// static
void ParamTraits<ppapi::proxy::PPBFlash_DrawGlyphs_Params>::Write(
    Message* m,
    const param_type& p) {
  ParamTraits<PP_Instance>::Write(m, p.instance);
  ParamTraits<ppapi::HostResource>::Write(m, p.image_data);
  ParamTraits<ppapi::proxy::SerializedFontDescription>::Write(m, p.font_desc);
  ParamTraits<uint32_t>::Write(m, p.color);
  ParamTraits<PP_Point>::Write(m, p.position);
  ParamTraits<PP_Rect>::Write(m, p.clip);
  ParamTraits<float>::Write(m, p.transformation[0][0]);
  ParamTraits<float>::Write(m, p.transformation[0][1]);
  ParamTraits<float>::Write(m, p.transformation[0][2]);
  ParamTraits<float>::Write(m, p.transformation[1][0]);
  ParamTraits<float>::Write(m, p.transformation[1][1]);
  ParamTraits<float>::Write(m, p.transformation[1][2]);
  ParamTraits<float>::Write(m, p.transformation[2][0]);
  ParamTraits<float>::Write(m, p.transformation[2][1]);
  ParamTraits<float>::Write(m, p.transformation[2][2]);
  ParamTraits<PP_Bool>::Write(m, p.allow_subpixel_aa);
  ParamTraits<std::vector<uint16_t> >::Write(m, p.glyph_indices);
  ParamTraits<std::vector<PP_Point> >::Write(m, p.glyph_advances);
}

// static
bool ParamTraits<ppapi::proxy::PPBFlash_DrawGlyphs_Params>::Read(
    const Message* m,
    PickleIterator* iter,
    param_type* r) {
  return
      ParamTraits<PP_Instance>::Read(m, iter, &r->instance) &&
      ParamTraits<ppapi::HostResource>::Read(m, iter, &r->image_data) &&
      ParamTraits<ppapi::proxy::SerializedFontDescription>::Read(m, iter,
                                                              &r->font_desc) &&
      ParamTraits<uint32_t>::Read(m, iter, &r->color) &&
      ParamTraits<PP_Point>::Read(m, iter, &r->position) &&
      ParamTraits<PP_Rect>::Read(m, iter, &r->clip) &&
      ParamTraits<float>::Read(m, iter, &r->transformation[0][0]) &&
      ParamTraits<float>::Read(m, iter, &r->transformation[0][1]) &&
      ParamTraits<float>::Read(m, iter, &r->transformation[0][2]) &&
      ParamTraits<float>::Read(m, iter, &r->transformation[1][0]) &&
      ParamTraits<float>::Read(m, iter, &r->transformation[1][1]) &&
      ParamTraits<float>::Read(m, iter, &r->transformation[1][2]) &&
      ParamTraits<float>::Read(m, iter, &r->transformation[2][0]) &&
      ParamTraits<float>::Read(m, iter, &r->transformation[2][1]) &&
      ParamTraits<float>::Read(m, iter, &r->transformation[2][2]) &&
      ParamTraits<PP_Bool>::Read(m, iter, &r->allow_subpixel_aa) &&
      ParamTraits<std::vector<uint16_t> >::Read(m, iter, &r->glyph_indices) &&
      ParamTraits<std::vector<PP_Point> >::Read(m, iter, &r->glyph_advances) &&
      r->glyph_indices.size() == r->glyph_advances.size();
}

// static
void ParamTraits<ppapi::proxy::PPBFlash_DrawGlyphs_Params>::Log(
    const param_type& p,
    std::string* l) {
}

// SerializedDirEntry ----------------------------------------------------------

// static
void ParamTraits<ppapi::proxy::SerializedDirEntry>::Write(Message* m,
                                                          const param_type& p) {
  ParamTraits<std::string>::Write(m, p.name);
  ParamTraits<bool>::Write(m, p.is_dir);
}

// static
bool ParamTraits<ppapi::proxy::SerializedDirEntry>::Read(const Message* m,
                                                         PickleIterator* iter,
                                                         param_type* r) {
  return ParamTraits<std::string>::Read(m, iter, &r->name) &&
         ParamTraits<bool>::Read(m, iter, &r->is_dir);
}

// static
void ParamTraits<ppapi::proxy::SerializedDirEntry>::Log(const param_type& p,
                                                        std::string* l) {
}

// ppapi::proxy::SerializedFontDescription -------------------------------------

// static
void ParamTraits<ppapi::proxy::SerializedFontDescription>::Write(
    Message* m,
    const param_type& p) {
  ParamTraits<std::string>::Write(m, p.face);
  ParamTraits<int32_t>::Write(m, p.family);
  ParamTraits<uint32_t>::Write(m, p.size);
  ParamTraits<int32_t>::Write(m, p.weight);
  ParamTraits<PP_Bool>::Write(m, p.italic);
  ParamTraits<PP_Bool>::Write(m, p.small_caps);
  ParamTraits<int32_t>::Write(m, p.letter_spacing);
  ParamTraits<int32_t>::Write(m, p.word_spacing);
}

// static
bool ParamTraits<ppapi::proxy::SerializedFontDescription>::Read(
    const Message* m,
    PickleIterator* iter,
    param_type* r) {
  return
      ParamTraits<std::string>::Read(m, iter, &r->face) &&
      ParamTraits<int32_t>::Read(m, iter, &r->family) &&
      ParamTraits<uint32_t>::Read(m, iter, &r->size) &&
      ParamTraits<int32_t>::Read(m, iter, &r->weight) &&
      ParamTraits<PP_Bool>::Read(m, iter, &r->italic) &&
      ParamTraits<PP_Bool>::Read(m, iter, &r->small_caps) &&
      ParamTraits<int32_t>::Read(m, iter, &r->letter_spacing) &&
      ParamTraits<int32_t>::Read(m, iter, &r->word_spacing);
}

// static
void ParamTraits<ppapi::proxy::SerializedFontDescription>::Log(
    const param_type& p,
    std::string* l) {
}
#endif  // !defined(OS_NACL) && !defined(NACL_WIN64)

// ppapi::proxy::SerializedTrueTypeFontDesc ------------------------------------

// static
void ParamTraits<ppapi::proxy::SerializedTrueTypeFontDesc>::Write(
    Message* m,
    const param_type& p) {
  ParamTraits<std::string>::Write(m, p.family);
  ParamTraits<PP_TrueTypeFontFamily_Dev>::Write(m, p.generic_family);
  ParamTraits<PP_TrueTypeFontStyle_Dev>::Write(m, p.style);
  ParamTraits<PP_TrueTypeFontWeight_Dev>::Write(m, p.weight);
  ParamTraits<PP_TrueTypeFontWidth_Dev>::Write(m, p.width);
  ParamTraits<PP_TrueTypeFontCharset_Dev>::Write(m, p.charset);
}

// static
bool ParamTraits<ppapi::proxy::SerializedTrueTypeFontDesc>::Read(
    const Message* m,
    PickleIterator* iter,
    param_type* r) {
  return
      ParamTraits<std::string>::Read(m, iter, &r->family) &&
      ParamTraits<PP_TrueTypeFontFamily_Dev>::Read(m, iter,
                                                   &r->generic_family) &&
      ParamTraits<PP_TrueTypeFontStyle_Dev>::Read(m, iter, &r->style) &&
      ParamTraits<PP_TrueTypeFontWeight_Dev>::Read(m, iter, &r->weight) &&
      ParamTraits<PP_TrueTypeFontWidth_Dev>::Read(m, iter, &r->width) &&
      ParamTraits<PP_TrueTypeFontCharset_Dev>::Read(m, iter, &r->charset);
}

// static
void ParamTraits<ppapi::proxy::SerializedTrueTypeFontDesc>::Log(
    const param_type& p,
    std::string* l) {
}

#if !defined(OS_NACL) && !defined(NACL_WIN64)
// ppapi::PepperFilePath -------------------------------------------------------

// static
void ParamTraits<ppapi::PepperFilePath>::Write(Message* m,
                                               const param_type& p) {
  WriteParam(m, static_cast<unsigned>(p.domain()));
  WriteParam(m, p.path());
}

// static
bool ParamTraits<ppapi::PepperFilePath>::Read(const Message* m,
                                              PickleIterator* iter,
                                              param_type* p) {
  unsigned domain;
  base::FilePath path;
  if (!ReadParam(m, iter, &domain) || !ReadParam(m, iter, &path))
    return false;
  if (domain > ppapi::PepperFilePath::DOMAIN_MAX_VALID)
    return false;

  *p = ppapi::PepperFilePath(
      static_cast<ppapi::PepperFilePath::Domain>(domain), path);
  return true;
}

// static
void ParamTraits<ppapi::PepperFilePath>::Log(const param_type& p,
                                             std::string* l) {
  l->append("(");
  LogParam(static_cast<unsigned>(p.domain()), l);
  l->append(", ");
  LogParam(p.path(), l);
  l->append(")");
}

// SerializedFlashMenu ---------------------------------------------------------

// static
void ParamTraits<ppapi::proxy::SerializedFlashMenu>::Write(
    Message* m,
    const param_type& p) {
  p.WriteToMessage(m);
}

// static
bool ParamTraits<ppapi::proxy::SerializedFlashMenu>::Read(const Message* m,
                                                          PickleIterator* iter,
                                                          param_type* r) {
  return r->ReadFromMessage(m, iter);
}

// static
void ParamTraits<ppapi::proxy::SerializedFlashMenu>::Log(const param_type& p,
                                                         std::string* l) {
}
#endif  // !defined(OS_NACL) && !defined(NACL_WIN64)

// PPB_X509Certificate_Fields --------------------------------------------------

// static
void ParamTraits<ppapi::PPB_X509Certificate_Fields>::Write(
    Message* m,
    const param_type& p) {
  ParamTraits<base::ListValue>::Write(m, p.values_);
}

// static
bool ParamTraits<ppapi::PPB_X509Certificate_Fields>::Read(const Message* m,
                                                          PickleIterator* iter,
                                                          param_type* r) {
  return ParamTraits<base::ListValue>::Read(m, iter, &(r->values_));
}

// static
void ParamTraits<ppapi::PPB_X509Certificate_Fields>::Log(const param_type& p,
                                                         std::string* l) {
}

// ppapi::SocketOptionData -----------------------------------------------------

// static
void ParamTraits<ppapi::SocketOptionData>::Write(Message* m,
                                                 const param_type& p) {
  ppapi::SocketOptionData::Type type = p.GetType();
  ParamTraits<int32_t>::Write(m, static_cast<int32_t>(type));
  switch (type) {
    case ppapi::SocketOptionData::TYPE_INVALID: {
      break;
    }
    case ppapi::SocketOptionData::TYPE_BOOL: {
      bool out_value = false;
      bool result = p.GetBool(&out_value);
      // Suppress unused variable warnings.
      static_cast<void>(result);
      DCHECK(result);

      ParamTraits<bool>::Write(m, out_value);
      break;
    }
    case ppapi::SocketOptionData::TYPE_INT32: {
      int32_t out_value = 0;
      bool result = p.GetInt32(&out_value);
      // Suppress unused variable warnings.
      static_cast<void>(result);
      DCHECK(result);

      ParamTraits<int32_t>::Write(m, out_value);
      break;
    }
    // No default so the compiler will warn on new types.
  }
}

// static
bool ParamTraits<ppapi::SocketOptionData>::Read(const Message* m,
                                                PickleIterator* iter,
                                                param_type* r) {
  *r = ppapi::SocketOptionData();
  int32_t type = 0;
  if (!ParamTraits<int32_t>::Read(m, iter, &type))
    return false;
  if (type != ppapi::SocketOptionData::TYPE_INVALID &&
      type != ppapi::SocketOptionData::TYPE_BOOL &&
      type != ppapi::SocketOptionData::TYPE_INT32) {
    return false;
  }
  switch (static_cast<ppapi::SocketOptionData::Type>(type)) {
    case ppapi::SocketOptionData::TYPE_INVALID: {
      return true;
    }
    case ppapi::SocketOptionData::TYPE_BOOL: {
      bool value = false;
      if (!ParamTraits<bool>::Read(m, iter, &value))
        return false;
      r->SetBool(value);
      return true;
    }
    case ppapi::SocketOptionData::TYPE_INT32: {
      int32_t value = 0;
      if (!ParamTraits<int32_t>::Read(m, iter, &value))
        return false;
      r->SetInt32(value);
      return true;
    }
    // No default so the compiler will warn on new types.
  }
  return false;
}

// static
void ParamTraits<ppapi::SocketOptionData>::Log(const param_type& p,
                                               std::string* l) {
}

// ppapi::CompositorLayerData --------------------------------------------------

// static
void ParamTraits<ppapi::CompositorLayerData::Transform>::Write(
    Message* m,
    const param_type& p) {
  for (size_t i = 0; i < arraysize(p.matrix); i++)
    ParamTraits<float>::Write(m, p.matrix[i]);
}

// static
bool ParamTraits<ppapi::CompositorLayerData::Transform>::Read(
    const Message* m,
    PickleIterator* iter,
    param_type* r) {
  for (size_t i = 0; i < arraysize(r->matrix);i++) {
    if (!ParamTraits<float>::Read(m, iter, &r->matrix[i]))
      return false;
  }
  return true;
}

void ParamTraits<ppapi::CompositorLayerData::Transform>::Log(
    const param_type& p,
    std::string* l) {
}

}  // namespace IPC
