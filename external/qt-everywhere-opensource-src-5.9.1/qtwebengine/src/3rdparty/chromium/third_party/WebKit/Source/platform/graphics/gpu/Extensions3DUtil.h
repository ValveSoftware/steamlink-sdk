// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Extensions3DUtil_h
#define Extensions3DUtil_h

#include "platform/PlatformExport.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "wtf/Allocator.h"
#include "wtf/HashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace blink {

class PLATFORM_EXPORT Extensions3DUtil final {
  USING_FAST_MALLOC(Extensions3DUtil);
  WTF_MAKE_NONCOPYABLE(Extensions3DUtil);

 public:
  // Creates a new Extensions3DUtil. If the passed GLES2Interface has been
  // spontaneously lost, returns null.
  static std::unique_ptr<Extensions3DUtil> create(gpu::gles2::GLES2Interface*);
  ~Extensions3DUtil();

  bool isValid() { return m_isValid; }

  bool supportsExtension(const String& name);
  bool ensureExtensionEnabled(const String& name);
  bool isExtensionEnabled(const String& name);

  static bool canUseCopyTextureCHROMIUM(GLenum destTarget,
                                        GLenum destFormat,
                                        GLenum destType,
                                        GLint level);

 private:
  Extensions3DUtil(gpu::gles2::GLES2Interface*);
  void initializeExtensions();

  gpu::gles2::GLES2Interface* m_gl;
  HashSet<String> m_enabledExtensions;
  HashSet<String> m_requestableExtensions;
  bool m_isValid;
};

}  // namespace blink

#endif  // Extensions3DUtil_h
