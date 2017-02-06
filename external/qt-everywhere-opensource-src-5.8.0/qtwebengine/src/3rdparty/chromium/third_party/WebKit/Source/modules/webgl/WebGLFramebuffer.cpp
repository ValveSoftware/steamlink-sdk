/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/webgl/WebGLFramebuffer.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "modules/webgl/WebGLRenderbuffer.h"
#include "modules/webgl/WebGLRenderingContextBase.h"
#include "modules/webgl/WebGLTexture.h"

namespace blink {

namespace {

class WebGLRenderbufferAttachment final : public WebGLFramebuffer::WebGLAttachment {
public:
    static WebGLFramebuffer::WebGLAttachment* create(WebGLRenderbuffer*);

    DECLARE_VIRTUAL_TRACE();

private:
    explicit WebGLRenderbufferAttachment(WebGLRenderbuffer*);
    WebGLRenderbufferAttachment() { }

    WebGLSharedObject* object() const override;
    bool isSharedObject(WebGLSharedObject*) const override;
    bool valid() const override;
    void onDetached(gpu::gles2::GLES2Interface*) override;
    void attach(gpu::gles2::GLES2Interface*, GLenum target, GLenum attachment) override;
    void unattach(gpu::gles2::GLES2Interface*, GLenum target, GLenum attachment) override;

    Member<WebGLRenderbuffer> m_renderbuffer;
};

WebGLFramebuffer::WebGLAttachment* WebGLRenderbufferAttachment::create(WebGLRenderbuffer* renderbuffer)
{
    return new WebGLRenderbufferAttachment(renderbuffer);
}

DEFINE_TRACE(WebGLRenderbufferAttachment)
{
    visitor->trace(m_renderbuffer);
    WebGLFramebuffer::WebGLAttachment::trace(visitor);
}

WebGLRenderbufferAttachment::WebGLRenderbufferAttachment(WebGLRenderbuffer* renderbuffer)
    : m_renderbuffer(renderbuffer)
{
}

WebGLSharedObject* WebGLRenderbufferAttachment::object() const
{
    return m_renderbuffer->object() ? m_renderbuffer.get() : 0;
}

bool WebGLRenderbufferAttachment::isSharedObject(WebGLSharedObject* object) const
{
    return object == m_renderbuffer;
}

bool WebGLRenderbufferAttachment::valid() const
{
    return m_renderbuffer->object();
}

void WebGLRenderbufferAttachment::onDetached(gpu::gles2::GLES2Interface* gl)
{
    m_renderbuffer->onDetached(gl);
}

void WebGLRenderbufferAttachment::attach(gpu::gles2::GLES2Interface* gl, GLenum target, GLenum attachment)
{
    GLuint object = objectOrZero(m_renderbuffer.get());
    gl->FramebufferRenderbuffer(target, attachment, GL_RENDERBUFFER, object);
}

void WebGLRenderbufferAttachment::unattach(gpu::gles2::GLES2Interface* gl, GLenum target, GLenum attachment)
{
    gl->FramebufferRenderbuffer(target, attachment, GL_RENDERBUFFER, 0);
}

class WebGLTextureAttachment final : public WebGLFramebuffer::WebGLAttachment {
public:
    static WebGLFramebuffer::WebGLAttachment* create(WebGLTexture*, GLenum target, GLint level, GLint layer);

    DECLARE_VIRTUAL_TRACE();

private:
    WebGLTextureAttachment(WebGLTexture*, GLenum target, GLint level, GLint layer);
    WebGLTextureAttachment() { }

    WebGLSharedObject* object() const override;
    bool isSharedObject(WebGLSharedObject*) const override;
    bool valid() const override;
    void onDetached(gpu::gles2::GLES2Interface*) override;
    void attach(gpu::gles2::GLES2Interface*, GLenum target, GLenum attachment) override;
    void unattach(gpu::gles2::GLES2Interface*, GLenum target, GLenum attachment) override;

    Member<WebGLTexture> m_texture;
    GLenum m_target;
    GLint m_level;
    GLint m_layer;
};

WebGLFramebuffer::WebGLAttachment* WebGLTextureAttachment::create(WebGLTexture* texture, GLenum target, GLint level, GLint layer)
{
    return new WebGLTextureAttachment(texture, target, level, layer);
}

DEFINE_TRACE(WebGLTextureAttachment)
{
    visitor->trace(m_texture);
    WebGLFramebuffer::WebGLAttachment::trace(visitor);
}

WebGLTextureAttachment::WebGLTextureAttachment(WebGLTexture* texture, GLenum target, GLint level, GLint layer)
    : m_texture(texture)
    , m_target(target)
    , m_level(level)
    , m_layer(layer)
{
}

WebGLSharedObject* WebGLTextureAttachment::object() const
{
    return m_texture->object() ? m_texture.get() : 0;
}

bool WebGLTextureAttachment::isSharedObject(WebGLSharedObject* object) const
{
    return object == m_texture;
}

bool WebGLTextureAttachment::valid() const
{
    return m_texture->object();
}

void WebGLTextureAttachment::onDetached(gpu::gles2::GLES2Interface* gl)
{
    m_texture->onDetached(gl);
}

void WebGLTextureAttachment::attach(gpu::gles2::GLES2Interface* gl, GLenum target, GLenum attachment)
{
    GLuint object = objectOrZero(m_texture.get());
    if (m_target == GL_TEXTURE_3D || m_target == GL_TEXTURE_2D_ARRAY) {
        gl->FramebufferTextureLayer(target, attachment, object, m_level, m_layer);
    } else {
        gl->FramebufferTexture2D(target, attachment, m_target, object, m_level);
    }
}

void WebGLTextureAttachment::unattach(gpu::gles2::GLES2Interface* gl, GLenum target, GLenum attachment)
{
    // GL_DEPTH_STENCIL_ATTACHMENT attachment is valid in ES3.
    if (m_target == GL_TEXTURE_3D || m_target == GL_TEXTURE_2D_ARRAY) {
        gl->FramebufferTextureLayer(target, attachment, 0, m_level, m_layer);
    } else {
        gl->FramebufferTexture2D(target, attachment, m_target, 0, m_level);
    }
}

} // anonymous namespace

WebGLFramebuffer::WebGLAttachment::WebGLAttachment()
{
}

WebGLFramebuffer::WebGLAttachment::~WebGLAttachment()
{
}

WebGLFramebuffer* WebGLFramebuffer::create(WebGLRenderingContextBase* ctx)
{
    return new WebGLFramebuffer(ctx);
}

WebGLFramebuffer::WebGLFramebuffer(WebGLRenderingContextBase* ctx)
    : WebGLContextObject(ctx)
    , m_object(0)
    , m_destructionInProgress(false)
    , m_hasEverBeenBound(false)
    , m_readBuffer(GL_COLOR_ATTACHMENT0)
{
    ctx->contextGL()->GenFramebuffers(1, &m_object);
}

WebGLFramebuffer::~WebGLFramebuffer()
{
    // Attachments in |m_attachments| will be deleted from other
    // places, and we must not touch that map in deleteObjectImpl once
    // the destructor has been entered.
    m_destructionInProgress = true;

    // See the comment in WebGLObject::detachAndDeleteObject().
    detachAndDeleteObject();
}

void WebGLFramebuffer::setAttachmentForBoundFramebuffer(GLenum target, GLenum attachment, GLenum texTarget, WebGLTexture* texture, GLint level, GLint layer)
{
    ASSERT(isBound(target));
    removeAttachmentFromBoundFramebuffer(target, attachment);
    if (!m_object)
        return;
    if (texture && texture->object()) {
        m_attachments.add(attachment, WebGLTextureAttachment::create(texture, texTarget, level, layer));
        drawBuffersIfNecessary(false);
        texture->onAttached();
    }
}

void WebGLFramebuffer::setAttachmentForBoundFramebuffer(GLenum target, GLenum attachment, WebGLRenderbuffer* renderbuffer)
{
    ASSERT(isBound(target));
    removeAttachmentFromBoundFramebuffer(target, attachment);
    if (!m_object)
        return;
    if (renderbuffer && renderbuffer->object()) {
        m_attachments.add(attachment, WebGLRenderbufferAttachment::create(renderbuffer));
        drawBuffersIfNecessary(false);
        renderbuffer->onAttached();
    }
}

void WebGLFramebuffer::attach(GLenum target, GLenum attachment, GLenum attachmentPoint)
{
    ASSERT(isBound(target));
    WebGLAttachment* attachmentObject = getAttachment(attachment);
    if (attachmentObject)
        attachmentObject->attach(context()->contextGL(), target, attachmentPoint);
}

WebGLSharedObject* WebGLFramebuffer::getAttachmentObject(GLenum attachment) const
{
    if (!m_object)
        return nullptr;
    WebGLAttachment* attachmentObject = getAttachment(attachment);
    return attachmentObject ? attachmentObject->object() : nullptr;
}

WebGLFramebuffer::WebGLAttachment* WebGLFramebuffer::getAttachment(GLenum attachment) const
{
    const AttachmentMap::const_iterator it = m_attachments.find(attachment);
    return (it != m_attachments.end()) ? it->value.get() : 0;
}

void WebGLFramebuffer::removeAttachmentFromBoundFramebuffer(GLenum target, GLenum attachment)
{
    ASSERT(isBound(target));
    if (!m_object)
        return;

    WebGLAttachment* attachmentObject = getAttachment(attachment);
    if (attachmentObject) {
        attachmentObject->onDetached(context()->contextGL());
        m_attachments.remove(attachment);
        drawBuffersIfNecessary(false);
        switch (attachment) {
        case GL_DEPTH_STENCIL_ATTACHMENT:
            attach(target, GL_DEPTH_ATTACHMENT, GL_DEPTH_ATTACHMENT);
            attach(target, GL_STENCIL_ATTACHMENT, GL_STENCIL_ATTACHMENT);
            break;
        case GL_DEPTH_ATTACHMENT:
        case GL_STENCIL_ATTACHMENT:
            attach(target, GL_DEPTH_STENCIL_ATTACHMENT, GL_DEPTH_STENCIL_ATTACHMENT);
            break;
        }
    }
}

void WebGLFramebuffer::removeAttachmentFromBoundFramebuffer(GLenum target, WebGLSharedObject* attachment)
{
    ASSERT(isBound(target));
    if (!m_object)
        return;
    if (!attachment)
        return;

    bool checkMore = true;
    while (checkMore) {
        checkMore = false;
        for (const auto& it : m_attachments) {
            WebGLAttachment* attachmentObject = it.value.get();
            if (attachmentObject->isSharedObject(attachment)) {
                GLenum attachmentType = it.key;
                attachmentObject->unattach(context()->contextGL(), target, attachmentType);
                removeAttachmentFromBoundFramebuffer(target, attachmentType);
                checkMore = true;
                break;
            }
        }
    }
}

GLenum WebGLFramebuffer::checkDepthStencilStatus(const char** reason) const
{
    if (context()->isWebGL2OrHigher())
        return GL_FRAMEBUFFER_COMPLETE;
    WebGLAttachment* depthAttachment = nullptr;
    WebGLAttachment* stencilAttachment = nullptr;
    WebGLAttachment* depthStencilAttachment = nullptr;
    for (const auto& it : m_attachments) {
        WebGLAttachment* attachment = it.value.get();
        ASSERT(attachment);
        switch (it.key) {
        case GL_DEPTH_ATTACHMENT:
            depthAttachment = attachment;
            break;
        case GL_STENCIL_ATTACHMENT:
            stencilAttachment = attachment;
            break;
        case GL_DEPTH_STENCIL_ATTACHMENT:
            depthStencilAttachment = attachment;
            break;
        default:
            break;
        }
    }
    if ((depthStencilAttachment && (depthAttachment || stencilAttachment))
        || (depthAttachment && stencilAttachment)) {
        *reason = "conflicting DEPTH/STENCIL/DEPTH_STENCIL attachments";
        return GL_FRAMEBUFFER_UNSUPPORTED;
    }
    return GL_FRAMEBUFFER_COMPLETE;
}

bool WebGLFramebuffer::hasStencilBuffer() const
{
    WebGLAttachment* attachment = getAttachment(GL_STENCIL_ATTACHMENT);
    if (!attachment)
        attachment = getAttachment(GL_DEPTH_STENCIL_ATTACHMENT);
    return attachment && attachment->valid();
}

void WebGLFramebuffer::deleteObjectImpl(gpu::gles2::GLES2Interface* gl)
{
    // Both the AttachmentMap and its WebGLAttachment objects are GCed
    // objects and cannot be accessed after the destructor has been
    // entered, as they may have been finalized already during the
    // same GC sweep. These attachments' OpenGL objects will be fully
    // destroyed once their JavaScript wrappers are collected.
    if (!m_destructionInProgress) {
        for (const auto& attachment : m_attachments)
            attachment.value->onDetached(gl);
    }

    gl->DeleteFramebuffers(1, &m_object);
    m_object = 0;
}

bool WebGLFramebuffer::isBound(GLenum target) const
{
    return (context()->getFramebufferBinding(target) == this);
}

void WebGLFramebuffer::drawBuffers(const Vector<GLenum>& bufs)
{
    m_drawBuffers = bufs;
    m_filteredDrawBuffers.resize(m_drawBuffers.size());
    for (size_t i = 0; i < m_filteredDrawBuffers.size(); ++i)
        m_filteredDrawBuffers[i] = GL_NONE;
    drawBuffersIfNecessary(true);
}

void WebGLFramebuffer::drawBuffersIfNecessary(bool force)
{
    if (context()->isWebGL2OrHigher()
        || context()->extensionEnabled(WebGLDrawBuffersName)) {
        bool reset = force;
        // This filtering works around graphics driver bugs on Mac OS X.
        for (size_t i = 0; i < m_drawBuffers.size(); ++i) {
            if (m_drawBuffers[i] != GL_NONE && getAttachment(m_drawBuffers[i])) {
                if (m_filteredDrawBuffers[i] != m_drawBuffers[i]) {
                    m_filteredDrawBuffers[i] = m_drawBuffers[i];
                    reset = true;
                }
            } else {
                if (m_filteredDrawBuffers[i] != GL_NONE) {
                    m_filteredDrawBuffers[i] = GL_NONE;
                    reset = true;
                }
            }
        }
        if (reset) {
            context()->contextGL()->DrawBuffersEXT(
                m_filteredDrawBuffers.size(), m_filteredDrawBuffers.data());
        }
    }
}

GLenum WebGLFramebuffer::getDrawBuffer(GLenum drawBuffer)
{
    int index = static_cast<int>(drawBuffer - GL_DRAW_BUFFER0_EXT);
    ASSERT(index >= 0);
    if (index < static_cast<int>(m_drawBuffers.size()))
        return m_drawBuffers[index];
    if (drawBuffer == GL_DRAW_BUFFER0_EXT)
        return GL_COLOR_ATTACHMENT0;
    return GL_NONE;
}

ScopedPersistent<v8::Array>* WebGLFramebuffer::getPersistentCache()
{
    return &m_attachmentWrappers;
}

DEFINE_TRACE(WebGLFramebuffer)
{
    visitor->trace(m_attachments);
    WebGLContextObject::trace(visitor);
}

} // namespace blink
