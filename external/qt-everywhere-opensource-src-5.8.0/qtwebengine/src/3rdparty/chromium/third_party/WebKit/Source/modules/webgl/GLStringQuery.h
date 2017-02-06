// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GLStringQuery_h
#define GLStringQuery_h

#include "gpu/command_buffer/client/gles2_interface.h"
#include "wtf/text/WTFString.h"

namespace blink {

// Performs a query for a string on GLES2Interface and returns a WTF::String. If
// it is unable to query the string, it wil return an empty WTF::String.
class GLStringQuery {
public:
    struct ProgramInfoLog {
        static void LengthFunction(gpu::gles2::GLES2Interface* gl, GLuint id, GLint* length)
        {
            return gl->GetProgramiv(id, GL_INFO_LOG_LENGTH, length);
        }
        static void LogFunction(gpu::gles2::GLES2Interface* gl, GLuint id, GLint length, GLint* returnedLength, LChar* ptr)
        {
            return gl->GetProgramInfoLog(id, length, returnedLength, reinterpret_cast<GLchar*>(ptr));
        }
    };

    struct ShaderInfoLog {
        static void LengthFunction(gpu::gles2::GLES2Interface* gl, GLuint id, GLint* length)
        {
            gl->GetShaderiv(id, GL_INFO_LOG_LENGTH, length);
        }
        static void LogFunction(gpu::gles2::GLES2Interface* gl, GLuint id, GLint length, GLint* returnedLength, LChar* ptr)
        {
            gl->GetShaderInfoLog(id, length, returnedLength, reinterpret_cast<GLchar*>(ptr));
        }
    };

    struct TranslatedShaderSourceANGLE {
        static void LengthFunction(gpu::gles2::GLES2Interface* gl, GLuint id, GLint* length)
        {
            gl->GetShaderiv(id, GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE, length);
        }
        static void LogFunction(gpu::gles2::GLES2Interface* gl, GLuint id, GLint length, GLint* returnedLength, LChar* ptr)
        {
            gl->GetTranslatedShaderSourceANGLE(id, length, returnedLength, reinterpret_cast<GLchar*>(ptr));
        }
    };

    GLStringQuery(gpu::gles2::GLES2Interface* gl) : m_gl(gl) {}

    template<class Traits>
    WTF::String Run(GLuint id) {
        GLint length = 0;
        Traits::LengthFunction(m_gl, id, &length);
        if (!length)
            return WTF::emptyString();
        LChar* logPtr;
        RefPtr<WTF::StringImpl> nameImpl = WTF::StringImpl::createUninitialized(length, logPtr);
        GLsizei returnedLength = 0;
        Traits::LogFunction(m_gl, id, length, &returnedLength, logPtr);
        // The returnedLength excludes the null terminator. If this check wasn't
        // true, then we'd need to tell the returned String the real length.
        DCHECK_EQ(returnedLength + 1, length);
        return String(nameImpl.release());
    }

private:
    gpu::gles2::GLES2Interface* m_gl;
};

} // namespace blink
#endif // GLStringQuery_h
