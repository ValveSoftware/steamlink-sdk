/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/webgl/WebGLDrawBuffers.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "modules/webgl/WebGLFramebuffer.h"

namespace blink {

WebGLDrawBuffers::WebGLDrawBuffers(WebGLRenderingContextBase* context)
    : WebGLExtension(context)
{
    context->extensionsUtil()->ensureExtensionEnabled("GL_EXT_draw_buffers");
}

WebGLDrawBuffers::~WebGLDrawBuffers()
{
}

WebGLExtensionName WebGLDrawBuffers::name() const
{
    return WebGLDrawBuffersName;
}

WebGLDrawBuffers* WebGLDrawBuffers::create(WebGLRenderingContextBase* context)
{
    return new WebGLDrawBuffers(context);
}

// static
bool WebGLDrawBuffers::supported(WebGLRenderingContextBase* context)
{
    return (context->extensionsUtil()->supportsExtension("GL_EXT_draw_buffers")
        && satisfiesWebGLRequirements(context));
}

// static
const char* WebGLDrawBuffers::extensionName()
{
    return "WEBGL_draw_buffers";
}

void WebGLDrawBuffers::drawBuffersWEBGL(const Vector<GLenum>& buffers)
{
    WebGLExtensionScopedContext scoped(this);
    if (scoped.isLost())
        return;
    GLsizei n = buffers.size();
    const GLenum* bufs = buffers.data();
    if (!scoped.context()->m_framebufferBinding) {
        if (n != 1) {
            scoped.context()->synthesizeGLError(GL_INVALID_OPERATION, "drawBuffersWEBGL", "must provide exactly one buffer");
            return;
        }
        if (bufs[0] != GL_BACK && bufs[0] != GL_NONE) {
            scoped.context()->synthesizeGLError(GL_INVALID_OPERATION, "drawBuffersWEBGL", "BACK or NONE");
            return;
        }
        // Because the backbuffer is simulated on all current WebKit ports, we need to change BACK to COLOR_ATTACHMENT0.
        GLenum value = (bufs[0] == GL_BACK) ? GL_COLOR_ATTACHMENT0 : GL_NONE;
        scoped.context()->contextGL()->DrawBuffersEXT(1, &value);
        scoped.context()->setBackDrawBuffer(bufs[0]);
    } else {
        if (n > scoped.context()->maxDrawBuffers()) {
            scoped.context()->synthesizeGLError(GL_INVALID_VALUE, "drawBuffersWEBGL", "more than max draw buffers");
            return;
        }
        for (GLsizei i = 0; i < n; ++i) {
            if (bufs[i] != GL_NONE && bufs[i] != static_cast<GLenum>(GL_COLOR_ATTACHMENT0_EXT + i)) {
                scoped.context()->synthesizeGLError(GL_INVALID_OPERATION, "drawBuffersWEBGL", "COLOR_ATTACHMENTi_EXT or NONE");
                return;
            }
        }
        scoped.context()->m_framebufferBinding->drawBuffers(buffers);
    }
}

// static
bool WebGLDrawBuffers::satisfiesWebGLRequirements(WebGLRenderingContextBase* webglContext)
{
    gpu::gles2::GLES2Interface* gl = webglContext->contextGL();
    Extensions3DUtil* extensionsUtil = webglContext->extensionsUtil();

    // This is called after we make sure GL_EXT_draw_buffers is supported.
    GLint maxDrawBuffers = 0;
    GLint maxColorAttachments = 0;
    gl->GetIntegerv(GL_MAX_DRAW_BUFFERS_EXT, &maxDrawBuffers);
    gl->GetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT, &maxColorAttachments);
    if (maxDrawBuffers < 4 || maxColorAttachments < 4)
        return false;

    GLuint fbo;
    gl->GenFramebuffers(1, &fbo);
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo);

    const unsigned char* buffer = 0; // Chromium doesn't allow init data for depth/stencil tetxures.
    bool supportsDepth = (extensionsUtil->supportsExtension("GL_CHROMIUM_depth_texture")
        || extensionsUtil->supportsExtension("GL_OES_depth_texture")
        || extensionsUtil->supportsExtension("GL_ARB_depth_texture"));
    bool supportsDepthStencil = (extensionsUtil->supportsExtension("GL_EXT_packed_depth_stencil")
        || extensionsUtil->supportsExtension("GL_OES_packed_depth_stencil"));
    GLuint depthStencil = 0;
    if (supportsDepthStencil) {
        gl->GenTextures(1, &depthStencil);
        gl->BindTexture(GL_TEXTURE_2D, depthStencil);
        gl->TexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_STENCIL_OES, 1, 1, 0, GL_DEPTH_STENCIL_OES, GL_UNSIGNED_INT_24_8_OES, buffer);
    }
    GLuint depth = 0;
    if (supportsDepth) {
        gl->GenTextures(1, &depth);
        gl->BindTexture(GL_TEXTURE_2D, depth);
        gl->TexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 1, 1, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, buffer);
    }

    Vector<GLuint> colors;
    bool ok = true;
    GLint maxAllowedBuffers = std::min(maxDrawBuffers, maxColorAttachments);
    for (GLint i = 0; i < maxAllowedBuffers; ++i) {
        GLuint color;

        gl->GenTextures(1, &color);
        colors.append(color);
        gl->BindTexture(GL_TEXTURE_2D, color);
        gl->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
        gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, color, 0);
        if (gl->CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            ok = false;
            break;
        }
        if (supportsDepth) {
            gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);
            if (gl->CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                ok = false;
                break;
            }
            gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
        }
        if (supportsDepthStencil) {
            // For ES 2.0 contexts DEPTH_STENCIL is not available natively, so we emulate it
            // at the command buffer level for WebGL contexts.
            gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depthStencil, 0);
            if (gl->CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                ok = false;
                break;
            }
            gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
        }
    }

    webglContext->restoreCurrentFramebuffer();
    gl->DeleteFramebuffers(1, &fbo);
    webglContext->restoreCurrentTexture2D();
    if (supportsDepth)
        gl->DeleteTextures(1, &depth);
    if (supportsDepthStencil)
        gl->DeleteTextures(1, &depthStencil);
    gl->DeleteTextures(colors.size(), colors.data());

    return ok;
}

} // namespace blink
