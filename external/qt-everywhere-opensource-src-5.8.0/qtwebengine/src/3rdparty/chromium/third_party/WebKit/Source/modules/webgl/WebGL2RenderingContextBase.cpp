// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webgl/WebGL2RenderingContextBase.h"

#include "bindings/modules/v8/WebGLAny.h"
#include "core/frame/ImageBitmap.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/ImageData.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "modules/webgl/WebGLActiveInfo.h"
#include "modules/webgl/WebGLBuffer.h"
#include "modules/webgl/WebGLFenceSync.h"
#include "modules/webgl/WebGLFramebuffer.h"
#include "modules/webgl/WebGLProgram.h"
#include "modules/webgl/WebGLQuery.h"
#include "modules/webgl/WebGLRenderbuffer.h"
#include "modules/webgl/WebGLSampler.h"
#include "modules/webgl/WebGLSync.h"
#include "modules/webgl/WebGLTexture.h"
#include "modules/webgl/WebGLTransformFeedback.h"
#include "modules/webgl/WebGLUniformLocation.h"
#include "modules/webgl/WebGLVertexArrayObject.h"
#include "platform/CheckedInt.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/WTFString.h"
#include <memory>

using WTF::String;

namespace blink {

namespace {

GLsync syncObjectOrZero(const WebGLSync* object)
{
    return object ? object->object() : nullptr;
}

} // namespace

// These enums are from manual pages for glTexStorage2D/glTexStorage3D.
const GLenum kSupportedInternalFormatsStorage[] = {
    GL_R8,
    GL_R8_SNORM,
    GL_R16F,
    GL_R32F,
    GL_R8UI,
    GL_R8I,
    GL_R16UI,
    GL_R16I,
    GL_R32UI,
    GL_R32I,
    GL_RG8,
    GL_RG8_SNORM,
    GL_RG16F,
    GL_RG32F,
    GL_RG8UI,
    GL_RG8I,
    GL_RG16UI,
    GL_RG16I,
    GL_RG32UI,
    GL_RG32I,
    GL_RGB8,
    GL_SRGB8,
    GL_RGB565,
    GL_RGB8_SNORM,
    GL_R11F_G11F_B10F,
    GL_RGB9_E5,
    GL_RGB16F,
    GL_RGB32F,
    GL_RGB8UI,
    GL_RGB8I,
    GL_RGB16UI,
    GL_RGB16I,
    GL_RGB32UI,
    GL_RGB32I,
    GL_RGBA8,
    GL_SRGB8_ALPHA8,
    GL_RGBA8_SNORM,
    GL_RGB5_A1,
    GL_RGBA4,
    GL_RGB10_A2,
    GL_RGBA16F,
    GL_RGBA32F,
    GL_RGBA8UI,
    GL_RGBA8I,
    GL_RGB10_A2UI,
    GL_RGBA16UI,
    GL_RGBA16I,
    GL_RGBA32UI,
    GL_RGBA32I,
    GL_DEPTH_COMPONENT16,
    GL_DEPTH_COMPONENT24,
    GL_DEPTH_COMPONENT32F,
    GL_DEPTH24_STENCIL8,
    GL_DEPTH32F_STENCIL8,
};

const GLenum kCompressedTextureFormatsETC2EAC[] = {
    GL_COMPRESSED_R11_EAC,
    GL_COMPRESSED_SIGNED_R11_EAC,
    GL_COMPRESSED_RG11_EAC,
    GL_COMPRESSED_SIGNED_RG11_EAC,
    GL_COMPRESSED_RGB8_ETC2,
    GL_COMPRESSED_SRGB8_ETC2,
    GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,
    GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2,
    GL_COMPRESSED_RGBA8_ETC2_EAC,
    GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC,
};

WebGL2RenderingContextBase::WebGL2RenderingContextBase(HTMLCanvasElement* passedCanvas, std::unique_ptr<WebGraphicsContext3DProvider> contextProvider, const WebGLContextAttributes& requestedAttributes)
    : WebGLRenderingContextBase(passedCanvas, std::move(contextProvider), requestedAttributes)
{
    m_supportedInternalFormatsStorage.insert(kSupportedInternalFormatsStorage, kSupportedInternalFormatsStorage + WTF_ARRAY_LENGTH(kSupportedInternalFormatsStorage));
    m_supportedInternalFormatsStorage.insert(kCompressedTextureFormatsETC2EAC, kCompressedTextureFormatsETC2EAC + WTF_ARRAY_LENGTH(kCompressedTextureFormatsETC2EAC));
    m_compressedTextureFormatsETC2EAC.insert(kCompressedTextureFormatsETC2EAC, kCompressedTextureFormatsETC2EAC + WTF_ARRAY_LENGTH(kCompressedTextureFormatsETC2EAC));
    m_compressedTextureFormats.append(kCompressedTextureFormatsETC2EAC, WTF_ARRAY_LENGTH(kCompressedTextureFormatsETC2EAC));
}

WebGL2RenderingContextBase::~WebGL2RenderingContextBase()
{
    m_readFramebufferBinding = nullptr;

    m_boundCopyReadBuffer = nullptr;
    m_boundCopyWriteBuffer = nullptr;
    m_boundPixelPackBuffer = nullptr;
    m_boundPixelUnpackBuffer = nullptr;
    m_boundTransformFeedbackBuffer = nullptr;
    m_boundUniformBuffer = nullptr;

    m_currentBooleanOcclusionQuery = nullptr;
    m_currentTransformFeedbackPrimitivesWrittenQuery = nullptr;
}

void WebGL2RenderingContextBase::initializeNewContext()
{
    ASSERT(!isContextLost());
    ASSERT(drawingBuffer());

    m_readFramebufferBinding = nullptr;

    m_boundCopyReadBuffer = nullptr;
    m_boundCopyWriteBuffer = nullptr;
    m_boundPixelPackBuffer = nullptr;
    m_boundPixelUnpackBuffer = nullptr;
    m_boundTransformFeedbackBuffer = nullptr;
    m_boundUniformBuffer = nullptr;

    m_currentBooleanOcclusionQuery = nullptr;
    m_currentTransformFeedbackPrimitivesWrittenQuery = nullptr;

    GLint numCombinedTextureImageUnits = 0;
    contextGL()->GetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &numCombinedTextureImageUnits);
    m_samplerUnits.clear();
    m_samplerUnits.resize(numCombinedTextureImageUnits);

    m_maxTransformFeedbackSeparateAttribs = 0;
    contextGL()->GetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, &m_maxTransformFeedbackSeparateAttribs);
    m_boundIndexedTransformFeedbackBuffers.clear();
    m_boundIndexedTransformFeedbackBuffers.resize(m_maxTransformFeedbackSeparateAttribs);

    GLint maxUniformBufferBindings = 0;
    contextGL()->GetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &maxUniformBufferBindings);
    m_boundIndexedUniformBuffers.clear();
    m_boundIndexedUniformBuffers.resize(maxUniformBufferBindings);
    m_maxBoundUniformBufferIndex = 0;

    m_packRowLength = 0;
    m_packSkipPixels = 0;
    m_packSkipRows = 0;
    m_unpackRowLength = 0;
    m_unpackImageHeight = 0;
    m_unpackSkipPixels = 0;
    m_unpackSkipRows = 0;
    m_unpackSkipImages = 0;

    WebGLRenderingContextBase::initializeNewContext();
}

void WebGL2RenderingContextBase::copyBufferSubData(GLenum readTarget, GLenum writeTarget, long long readOffset, long long writeOffset, long long size)
{
    if (isContextLost())
        return;

    if (!validateValueFitNonNegInt32("copyBufferSubData", "readOffset", readOffset)
        || !validateValueFitNonNegInt32("copyBufferSubData", "writeOffset", writeOffset)
        || !validateValueFitNonNegInt32("copyBufferSubData", "size", size)) {
        return;
    }

    WebGLBuffer* readBuffer = validateBufferDataTarget("copyBufferSubData", readTarget);
    if (!readBuffer)
        return;

    WebGLBuffer* writeBuffer = validateBufferDataTarget("copyBufferSubData", writeTarget);
    if (!writeBuffer)
        return;

    if (readOffset + size > readBuffer->getSize() || writeOffset + size > writeBuffer->getSize()) {
        synthesizeGLError(GL_INVALID_VALUE, "copyBufferSubData", "buffer overflow");
        return;
    }

    if ((writeBuffer->getInitialTarget() == GL_ELEMENT_ARRAY_BUFFER && readBuffer->getInitialTarget() != GL_ELEMENT_ARRAY_BUFFER)
        || (writeBuffer->getInitialTarget() != GL_ELEMENT_ARRAY_BUFFER && readBuffer->getInitialTarget() == GL_ELEMENT_ARRAY_BUFFER)) {
        synthesizeGLError(GL_INVALID_OPERATION, "copyBufferSubData", "Cannot copy into an element buffer destination from a non-element buffer source");
        return;
    }

    if (writeBuffer->getInitialTarget() == 0)
        writeBuffer->setInitialTarget(readBuffer->getInitialTarget());

    contextGL()->CopyBufferSubData(readTarget, writeTarget, static_cast<GLintptr>(readOffset), static_cast<GLintptr>(writeOffset), static_cast<GLsizeiptr>(size));
}

void WebGL2RenderingContextBase::getBufferSubData(GLenum target, long long offset, DOMArrayBuffer* returnedData)
{
    if (isContextLost())
        return;

    if (!returnedData) {
        synthesizeGLError(GL_INVALID_VALUE, "getBufferSubData", "ArrayBuffer cannot be null");
        return;
    }

    if (!validateValueFitNonNegInt32("getBufferSubData", "offset", offset)) {
        return;
    }

    WebGLBuffer* buffer = validateBufferDataTarget("getBufferSubData", target);
    if (!buffer)
        return;
    if (offset + returnedData->byteLength() > buffer->getSize()) {
        synthesizeGLError(GL_INVALID_VALUE, "getBufferSubData", "buffer overflow");
        return;
    }

    void* mappedData = contextGL()->MapBufferRange(target, static_cast<GLintptr>(offset), returnedData->byteLength(), GL_MAP_READ_BIT);

    if (!mappedData)
        return;

    memcpy(returnedData->data(), mappedData, returnedData->byteLength());

    contextGL()->UnmapBuffer(target);
}

void WebGL2RenderingContextBase::blitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
    if (isContextLost())
        return;

    contextGL()->BlitFramebufferCHROMIUM(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

bool WebGL2RenderingContextBase::validateTexFuncLayer(const char* functionName, GLenum texTarget, GLint layer)
{
    if (layer < 0) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "layer out of range");
        return false;
    }
    switch (texTarget) {
    case GL_TEXTURE_3D:
        if (layer > m_max3DTextureSize - 1) {
            synthesizeGLError(GL_INVALID_VALUE, functionName, "layer out of range");
            return false;
        }
        break;
    case GL_TEXTURE_2D_ARRAY:
        if (layer > m_maxArrayTextureLayers - 1) {
            synthesizeGLError(GL_INVALID_VALUE, functionName, "layer out of range");
            return false;
        }
        break;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
    return true;
}

void WebGL2RenderingContextBase::framebufferTextureLayer(ScriptState* scriptState, GLenum target, GLenum attachment, WebGLTexture* texture, GLint level, GLint layer)
{
    if (isContextLost() || !validateFramebufferFuncParameters("framebufferTextureLayer", target, attachment))
        return;
    if (texture && !texture->validate(contextGroup(), this)) {
        synthesizeGLError(GL_INVALID_VALUE, "framebufferTextureLayer", "no texture or texture not from this context");
        return;
    }
    GLenum textarget = texture ? texture->getTarget() : 0;
    if (texture) {
        if (textarget != GL_TEXTURE_3D && textarget != GL_TEXTURE_2D_ARRAY) {
            synthesizeGLError(GL_INVALID_OPERATION, "framebufferTextureLayer", "invalid texture type");
            return;
        }
        if (!validateTexFuncLayer("framebufferTextureLayer", textarget, layer))
            return;
        if (!validateTexFuncLevel("framebufferTextureLayer", textarget, level))
            return;
    }

    WebGLFramebuffer* framebufferBinding = getFramebufferBinding(target);
    if (!framebufferBinding || !framebufferBinding->object()) {
        synthesizeGLError(GL_INVALID_OPERATION, "framebufferTextureLayer", "no framebuffer bound");
        return;
    }
    if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
        contextGL()->FramebufferTextureLayer(target, GL_DEPTH_ATTACHMENT, objectOrZero(texture), level, layer);
        contextGL()->FramebufferTextureLayer(target, GL_STENCIL_ATTACHMENT, objectOrZero(texture), level, layer);
    } else {
        contextGL()->FramebufferTextureLayer(target, attachment, objectOrZero(texture), level, layer);
    }
    if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
        // On ES3, DEPTH_STENCIL_ATTACHMENT is like an alias for DEPTH_ATTACHMENT + STENCIL_ATTACHMENT.
        // We divide it here so in WebGLFramebuffer, we don't have to handle DEPTH_STENCIL_ATTACHMENT in WebGL 2.
        framebufferBinding->setAttachmentForBoundFramebuffer(target, GL_DEPTH_ATTACHMENT, textarget, texture, level, layer);
        framebufferBinding->setAttachmentForBoundFramebuffer(target, GL_STENCIL_ATTACHMENT, textarget, texture, level, layer);
        preserveObjectWrapper(scriptState, framebufferBinding, V8HiddenValue::webglAttachments(scriptState->isolate()), framebufferBinding->getPersistentCache(), GL_DEPTH_ATTACHMENT, texture);
        preserveObjectWrapper(scriptState, framebufferBinding, V8HiddenValue::webglAttachments(scriptState->isolate()), framebufferBinding->getPersistentCache(), GL_STENCIL_ATTACHMENT, texture);
    } else {
        framebufferBinding->setAttachmentForBoundFramebuffer(target, attachment, textarget, texture, level, layer);
        preserveObjectWrapper(scriptState, framebufferBinding, V8HiddenValue::webglAttachments(scriptState->isolate()), framebufferBinding->getPersistentCache(), attachment, texture);
    }
    applyStencilTest();
}

ScriptValue WebGL2RenderingContextBase::getInternalformatParameter(ScriptState* scriptState, GLenum target, GLenum internalformat, GLenum pname)
{
    if (isContextLost())
        return ScriptValue::createNull(scriptState);

    if (target != GL_RENDERBUFFER) {
        synthesizeGLError(GL_INVALID_ENUM, "getInternalformatParameter", "invalid target");
        return ScriptValue::createNull(scriptState);
    }

    bool floatType = false;

    switch (internalformat) {
    // Renderbuffer doesn't support unsized internal formats,
    // though GL_RGB and GL_RGBA are color-renderable.
    case GL_RGB:
    case GL_RGBA:
    // Multisampling is not supported for signed and unsigned integer internal formats.
    case GL_R8UI:
    case GL_R8I:
    case GL_R16UI:
    case GL_R16I:
    case GL_R32UI:
    case GL_R32I:
    case GL_RG8UI:
    case GL_RG8I:
    case GL_RG16UI:
    case GL_RG16I:
    case GL_RG32UI:
    case GL_RG32I:
    case GL_RGBA8UI:
    case GL_RGBA8I:
    case GL_RGB10_A2UI:
    case GL_RGBA16UI:
    case GL_RGBA16I:
    case GL_RGBA32UI:
    case GL_RGBA32I:
        return WebGLAny(scriptState, DOMInt32Array::create(0));
    case GL_R8:
    case GL_RG8:
    case GL_RGB8:
    case GL_RGB565:
    case GL_RGBA8:
    case GL_SRGB8_ALPHA8:
    case GL_RGB5_A1:
    case GL_RGBA4:
    case GL_RGB10_A2:
    case GL_DEPTH_COMPONENT16:
    case GL_DEPTH_COMPONENT24:
    case GL_DEPTH_COMPONENT32F:
    case GL_DEPTH24_STENCIL8:
    case GL_DEPTH32F_STENCIL8:
    case GL_STENCIL_INDEX8:
        break;
    case GL_R16F:
    case GL_RG16F:
    case GL_RGBA16F:
    case GL_R32F:
    case GL_RG32F:
    case GL_RGBA32F:
    case GL_R11F_G11F_B10F:
        if (!extensionEnabled(EXTColorBufferFloatName)) {
            synthesizeGLError(GL_INVALID_ENUM, "getInternalformatParameter", "invalid internalformat when EXT_color_buffer_float is not enabled");
            return ScriptValue::createNull(scriptState);
        }
        floatType = true;
        break;
    default:
        synthesizeGLError(GL_INVALID_ENUM, "getInternalformatParameter", "invalid internalformat");
        return ScriptValue::createNull(scriptState);
    }

    switch (pname) {
    case GL_SAMPLES:
        {
            std::unique_ptr<GLint[]> values;
            GLint length = -1;
            if (!floatType) {
                contextGL()->GetInternalformativ(target, internalformat, GL_NUM_SAMPLE_COUNTS, 1, &length);
                if (length <= 0)
                    return WebGLAny(scriptState, DOMInt32Array::create(0));

                values = wrapArrayUnique(new GLint[length]);
                for (GLint ii = 0; ii < length; ++ii)
                    values[ii] = 0;
                contextGL()->GetInternalformativ(target, internalformat, GL_SAMPLES, length, values.get());
            } else {
                length = 1;
                values = wrapArrayUnique(new GLint[1]);
                values[0] = 1;
            }
            return WebGLAny(scriptState, DOMInt32Array::create(values.get(), length));
        }
    default:
        synthesizeGLError(GL_INVALID_ENUM, "getInternalformatParameter", "invalid parameter name");
        return ScriptValue::createNull(scriptState);
    }
}

bool WebGL2RenderingContextBase::checkAndTranslateAttachments(const char* functionName, GLenum target, Vector<GLenum>& attachments)
{
    if (!validateFramebufferTarget(target)) {
        synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid target");
        return false;
    }

    WebGLFramebuffer* framebufferBinding = getFramebufferBinding(target);
    ASSERT(framebufferBinding || drawingBuffer());
    if (!framebufferBinding) {
        // For the default framebuffer
        // Translate GL_COLOR/GL_DEPTH/GL_STENCIL, because the default framebuffer of WebGL is not fb 0, it is an internal fbo
        for (size_t i = 0; i < attachments.size(); ++i) {
            switch (attachments[i]) {
            case GL_COLOR:
                attachments[i] = GL_COLOR_ATTACHMENT0;
                break;
            case GL_DEPTH:
                attachments[i] = GL_DEPTH_ATTACHMENT;
                break;
            case GL_STENCIL:
                attachments[i] = GL_STENCIL_ATTACHMENT;
                break;
            default:
                synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid attachment");
                return false;
            }
        }
    }
    return true;
}

bool WebGL2RenderingContextBase::canUseTexImageByGPU(TexImageFunctionID functionID, GLint internalformat, GLenum type)
{
    switch (internalformat) {
    case GL_RGB565:
    case GL_RGBA4:
    case GL_RGB5_A1:
        // FIXME: ES3 limitation that CopyTexImage with sized internalformat,
        // component sizes have to match the source color format.
        return false;
    default:
        break;
    }
    return WebGLRenderingContextBase::canUseTexImageByGPU(functionID, internalformat, type);
}

void WebGL2RenderingContextBase::invalidateFramebuffer(GLenum target, const Vector<GLenum>& attachments)
{
    if (isContextLost())
        return;

    Vector<GLenum> translatedAttachments = attachments;
    if (!checkAndTranslateAttachments("invalidateFramebuffer", target, translatedAttachments))
        return;
    contextGL()->InvalidateFramebuffer(target, translatedAttachments.size(), translatedAttachments.data());
}

void WebGL2RenderingContextBase::invalidateSubFramebuffer(GLenum target, const Vector<GLenum>& attachments, GLint x, GLint y, GLsizei width, GLsizei height)
{
    if (isContextLost())
        return;

    Vector<GLenum> translatedAttachments = attachments;
    if (!checkAndTranslateAttachments("invalidateSubFramebuffer", target, translatedAttachments))
        return;
    contextGL()->InvalidateSubFramebuffer(target, translatedAttachments.size(), translatedAttachments.data(), x, y, width, height);
}

void WebGL2RenderingContextBase::readBuffer(GLenum mode)
{
    if (isContextLost())
        return;

    switch (mode) {
    case GL_BACK:
    case GL_NONE:
    case GL_COLOR_ATTACHMENT0:
        break;
    default:
        if (mode > GL_COLOR_ATTACHMENT0
            && mode < static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + maxColorAttachments()))
            break;
        synthesizeGLError(GL_INVALID_ENUM, "readBuffer", "invalid read buffer");
        return;
    }

    WebGLFramebuffer* readFramebufferBinding = getFramebufferBinding(GL_READ_FRAMEBUFFER);
    if (!readFramebufferBinding) {
        ASSERT(drawingBuffer());
        if (mode != GL_BACK && mode != GL_NONE) {
            synthesizeGLError(GL_INVALID_OPERATION, "readBuffer", "invalid read buffer");
            return;
        }
        m_readBufferOfDefaultFramebuffer = mode;
        // translate GL_BACK to GL_COLOR_ATTACHMENT0, because the default
        // framebuffer for WebGL is not fb 0, it is an internal fbo.
        if (mode == GL_BACK)
            mode = GL_COLOR_ATTACHMENT0;
    } else {
        if (mode == GL_BACK) {
            synthesizeGLError(GL_INVALID_OPERATION, "readBuffer", "invalid read buffer");
            return;
        }
        readFramebufferBinding->readBuffer(mode);
    }
    contextGL()->ReadBuffer(mode);
}

void WebGL2RenderingContextBase::pixelStorei(GLenum pname, GLint param)
{
    if (isContextLost())
        return;
    if (param < 0) {
        synthesizeGLError(GL_INVALID_VALUE, "pixelStorei", "negative value");
        return;
    }
    switch (pname) {
    case GL_PACK_ROW_LENGTH:
        m_packRowLength = param;
        break;
    case GL_PACK_SKIP_PIXELS:
        m_packSkipPixels = param;
        break;
    case GL_PACK_SKIP_ROWS:
        m_packSkipRows = param;
        break;
    case GL_UNPACK_ROW_LENGTH:
        m_unpackRowLength = param;
        break;
    case GL_UNPACK_IMAGE_HEIGHT:
        m_unpackImageHeight = param;
        break;
    case GL_UNPACK_SKIP_PIXELS:
        m_unpackSkipPixels = param;
        break;
    case GL_UNPACK_SKIP_ROWS:
        m_unpackSkipRows = param;
        break;
    case GL_UNPACK_SKIP_IMAGES:
        m_unpackSkipImages = param;
        break;
    default:
        WebGLRenderingContextBase::pixelStorei(pname, param);
        return;
    }
    contextGL()->PixelStorei(pname, param);
}

void WebGL2RenderingContextBase::readPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, DOMArrayBufferView* pixels)
{
    if (isContextLost())
        return;
    if (m_boundPixelPackBuffer.get()) {
        synthesizeGLError(GL_INVALID_OPERATION, "readPixels", "PIXEL_PACK buffer should not be bound");
        return;
    }

    WebGLRenderingContextBase::readPixels(x, y, width, height, format, type, pixels);
}

void WebGL2RenderingContextBase::readPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, long long offset)
{
    if (isContextLost())
        return;

    // Due to WebGL's same-origin restrictions, it is not possible to
    // taint the origin using the WebGL API.
    ASSERT(canvas()->originClean());

    if (!validateValueFitNonNegInt32("readPixels", "offset", offset))
        return;

    WebGLBuffer* buffer = m_boundPixelPackBuffer.get();
    if (!buffer) {
        synthesizeGLError(GL_INVALID_OPERATION, "readPixels", "no PIXEL_PACK buffer bound");
        return;
    }

    const char* reason = "framebuffer incomplete";
    WebGLFramebuffer* framebuffer = getReadFramebufferBinding();
    if (framebuffer && framebuffer->checkDepthStencilStatus(&reason) != GL_FRAMEBUFFER_COMPLETE) {
        synthesizeGLError(GL_INVALID_FRAMEBUFFER_OPERATION, "readPixels", reason);
        return;
    }

    long long size = buffer->getSize() - offset;
    // If size is negative, or size is not large enough to store pixels, those cases
    // are handled by validateReadPixelsFuncParameters to generate INVALID_OPERATION.
    if (!validateReadPixelsFuncParameters(width, height, format, type, nullptr, size))
        return;

    clearIfComposited();

    {
        ScopedDrawingBufferBinder binder(drawingBuffer(), framebuffer);
        contextGL()->ReadPixels(x, y, width, height, format, type, reinterpret_cast<void*>(offset));
    }
}

void WebGL2RenderingContextBase::renderbufferStorageImpl(
    GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height,
    const char* functionName)
{
    switch (internalformat) {
    case GL_R8UI:
    case GL_R8I:
    case GL_R16UI:
    case GL_R16I:
    case GL_R32UI:
    case GL_R32I:
    case GL_RG8UI:
    case GL_RG8I:
    case GL_RG16UI:
    case GL_RG16I:
    case GL_RG32UI:
    case GL_RG32I:
    case GL_RGBA8UI:
    case GL_RGBA8I:
    case GL_RGB10_A2UI:
    case GL_RGBA16UI:
    case GL_RGBA16I:
    case GL_RGBA32UI:
    case GL_RGBA32I:
        if (samples > 0) {
            synthesizeGLError(GL_INVALID_OPERATION, functionName,
                "for integer formats, samples > 0");
            return;
        }
    case GL_R8:
    case GL_RG8:
    case GL_RGB8:
    case GL_RGB565:
    case GL_RGBA8:
    case GL_SRGB8_ALPHA8:
    case GL_RGB5_A1:
    case GL_RGBA4:
    case GL_RGB10_A2:
    case GL_DEPTH_COMPONENT16:
    case GL_DEPTH_COMPONENT24:
    case GL_DEPTH_COMPONENT32F:
    case GL_DEPTH24_STENCIL8:
    case GL_DEPTH32F_STENCIL8:
    case GL_STENCIL_INDEX8:
        if (!samples) {
            contextGL()->RenderbufferStorage(target, internalformat, width, height);
        } else {
            GLint maxNumberOfSamples = 0;
            contextGL()->GetInternalformativ(target, internalformat, GL_SAMPLES, 1, &maxNumberOfSamples);
            if (samples > maxNumberOfSamples) {
                synthesizeGLError(GL_INVALID_OPERATION, functionName, "samples out of range");
                return;
            }
            contextGL()->RenderbufferStorageMultisampleCHROMIUM(
                target, samples, internalformat, width, height);
        }
        break;
    case GL_DEPTH_STENCIL:
        // To be WebGL 1 backward compatible.
        if (samples > 0) {
            synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid internalformat");
            return;
        }
        contextGL()->RenderbufferStorage(target, GL_DEPTH24_STENCIL8, width, height);
        break;
    case GL_R16F:
    case GL_RG16F:
    case GL_RGBA16F:
    case GL_R32F:
    case GL_RG32F:
    case GL_RGBA32F:
    case GL_R11F_G11F_B10F:
        if (!extensionEnabled(EXTColorBufferFloatName)) {
            synthesizeGLError(GL_INVALID_ENUM, functionName, "EXT_color_buffer_float not enabled");
            return;
        }
        if (samples) {
            synthesizeGLError(GL_INVALID_VALUE, functionName, "multisampled float buffers not supported");
            return;
        }
        contextGL()->RenderbufferStorage(target, internalformat, width, height);
        break;
    default:
        synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid internalformat");
        return;
    }
    m_renderbufferBinding->setInternalFormat(internalformat);
    m_renderbufferBinding->setSize(width, height);
}

void WebGL2RenderingContextBase::renderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)
{
    const char* functionName = "renderbufferStorageMultisample";
    if (isContextLost())
        return;
    if (target != GL_RENDERBUFFER) {
        synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid target");
        return;
    }
    if (!m_renderbufferBinding || !m_renderbufferBinding->object()) {
        synthesizeGLError(GL_INVALID_OPERATION, functionName, "no bound renderbuffer");
        return;
    }
    if (!validateSize("renderbufferStorage", width, height))
        return;
    if (samples < 0) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "samples < 0");
        return;
    }
    renderbufferStorageImpl(target, samples, internalformat, width, height, functionName);
    applyStencilTest();
}

void WebGL2RenderingContextBase::resetUnpackParameters()
{
    WebGLRenderingContextBase::resetUnpackParameters();

    if (!m_unpackRowLength)
        contextGL()->PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    if (!m_unpackImageHeight)
        contextGL()->PixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
    if (!m_unpackSkipPixels)
        contextGL()->PixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    if (!m_unpackSkipRows)
        contextGL()->PixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    if (!m_unpackSkipImages)
        contextGL()->PixelStorei(GL_UNPACK_SKIP_IMAGES, 0);
}

void WebGL2RenderingContextBase::restoreUnpackParameters()
{
    WebGLRenderingContextBase::restoreUnpackParameters();

    if (!m_unpackRowLength)
        contextGL()->PixelStorei(GL_UNPACK_ROW_LENGTH, m_unpackRowLength);
    if (!m_unpackImageHeight)
        contextGL()->PixelStorei(GL_UNPACK_IMAGE_HEIGHT, m_unpackImageHeight);
    if (!m_unpackSkipPixels)
        contextGL()->PixelStorei(GL_UNPACK_SKIP_PIXELS, m_unpackSkipPixels);
    if (!m_unpackSkipRows)
        contextGL()->PixelStorei(GL_UNPACK_SKIP_ROWS, m_unpackSkipRows);
    if (!m_unpackSkipImages)
        contextGL()->PixelStorei(GL_UNPACK_SKIP_IMAGES, m_unpackSkipImages);
}

/* Texture objects */
bool WebGL2RenderingContextBase::validateTexStorage(const char* functionName, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, TexStorageType functionType)
{
    if (functionType == TexStorageType2D) {
        if (target != GL_TEXTURE_2D && target != GL_TEXTURE_CUBE_MAP) {
            synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid 2D target");
            return false;
        }
    } else {
        if (target != GL_TEXTURE_3D && target != GL_TEXTURE_2D_ARRAY) {
            synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid 3D target");
            return false;
        }
    }

    if (functionType == TexStorageType3D && target != GL_TEXTURE_2D_ARRAY && m_compressedTextureFormatsETC2EAC.find(internalformat) != m_compressedTextureFormatsETC2EAC.end()) {
        synthesizeGLError(GL_INVALID_OPERATION, functionName, "target for ETC2/EAC internal formats must be TEXTURE_2D_ARRAY");
        return false;
    }

    if (m_supportedInternalFormatsStorage.find(internalformat) == m_supportedInternalFormatsStorage.end()
        && (functionType == TexStorageType2D && !m_compressedTextureFormats.contains(internalformat))) {
        synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid internalformat");
        return false;
    }

    if (width <= 0 || height <= 0 || depth <= 0) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "invalid dimensions");
        return false;
    }

    if (levels <= 0) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "invalid levels");
        return false;
    }

    if (target == GL_TEXTURE_3D) {
        if (levels > log2(std::max(std::max(width, height), depth)) + 1) {
            synthesizeGLError(GL_INVALID_OPERATION, functionName, "to many levels");
            return false;
        }
    } else {
        if (levels > log2(std::max(width, height)) + 1) {
            synthesizeGLError(GL_INVALID_OPERATION, functionName, "to many levels");
            return false;
        }
    }

    return true;
}

void WebGL2RenderingContextBase::texImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, GLintptr offset)
{
    if (isContextLost())
        return;
    if (!validateTexture2DBinding("texImage2D", target))
        return;
    if (!m_boundPixelUnpackBuffer) {
        synthesizeGLError(GL_INVALID_OPERATION, "texImage2D", "no bound PIXEL_UNPACK_BUFFER");
        return;
    }
    if (!validateTexFunc("texImage2D", TexImage, SourceUnpackBuffer, target, level, internalformat, width, height, 1, border, format, type, 0, 0, 0))
        return;
    if (!validateValueFitNonNegInt32("texImage2D", "offset", offset))
        return;

    contextGL()->TexImage2D(target, level, convertTexInternalFormat(internalformat, type), width, height, border, format, type, reinterpret_cast<const void *>(offset));
}

void WebGL2RenderingContextBase::texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, GLintptr offset)
{
    if (isContextLost())
        return;
    if (!validateTexture2DBinding("texSubImage2D", target))
        return;
    if (!m_boundPixelUnpackBuffer) {
        synthesizeGLError(GL_INVALID_OPERATION, "texSubImage2D", "no bound PIXEL_UNPACK_BUFFER");
        return;
    }
    if (!validateTexFunc("texSubImage2D", TexSubImage, SourceUnpackBuffer, target, level, 0, width, height, 1, 0, format, type, xoffset, yoffset, 0))
        return;
    if (!validateValueFitNonNegInt32("texSubImage2D", "offset", offset))
        return;

    contextGL()->TexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, reinterpret_cast<const void*>(offset));
}

void WebGL2RenderingContextBase::texImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, DOMArrayBufferView* data)
{
    WebGLRenderingContextBase::texImage2D(target, level, internalformat, width, height, border, format, type, data);
}

void WebGL2RenderingContextBase::texImage2D(GLenum target, GLint level, GLint internalformat, GLenum format, GLenum type, ImageData* imageData)
{
    WebGLRenderingContextBase::texImage2D(target, level, internalformat, format, type, imageData);
}

void WebGL2RenderingContextBase::texImage2D(GLenum target, GLint level, GLint internalformat, GLenum format, GLenum type, HTMLImageElement* image, ExceptionState& exceptionState)
{
    WebGLRenderingContextBase::texImage2D(target, level, internalformat, format, type, image, exceptionState);
}

void WebGL2RenderingContextBase::texImage2D(GLenum target, GLint level, GLint internalformat, GLenum format, GLenum type, HTMLCanvasElement* canvas, ExceptionState& exceptionState)
{
    WebGLRenderingContextBase::texImage2D(target, level, internalformat, format, type, canvas, exceptionState);
}

void WebGL2RenderingContextBase::texImage2D(GLenum target, GLint level, GLint internalformat, GLenum format, GLenum type, HTMLVideoElement* video, ExceptionState& exceptionState)
{
    WebGLRenderingContextBase::texImage2D(target, level, internalformat, format, type, video, exceptionState);
}

void WebGL2RenderingContextBase::texImage2D(GLenum target, GLint level, GLint internalformat, GLenum format, GLenum type, ImageBitmap* imageBitMap, ExceptionState& exceptionState)
{
    WebGLRenderingContextBase::texImage2D(target, level, internalformat, format, type, imageBitMap, exceptionState);
}

void WebGL2RenderingContextBase::texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
    GLsizei width, GLsizei height, GLenum format, GLenum type, DOMArrayBufferView* pixels)
{
    WebGLRenderingContextBase::texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

void WebGL2RenderingContextBase::texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
    GLenum format, GLenum type, ImageData* pixels)
{
    WebGLRenderingContextBase::texSubImage2D(target, level, xoffset, yoffset, format, type, pixels);
}

void WebGL2RenderingContextBase::texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
    GLenum format, GLenum type, HTMLImageElement* image, ExceptionState& exceptionState)
{
    WebGLRenderingContextBase::texSubImage2D(target, level, xoffset, yoffset, format, type, image, exceptionState);
}

void WebGL2RenderingContextBase::texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
    GLenum format, GLenum type, HTMLCanvasElement* canvas, ExceptionState& exceptionState)
{
    WebGLRenderingContextBase::texSubImage2D(target, level, xoffset, yoffset, format, type, canvas, exceptionState);
}

void WebGL2RenderingContextBase::texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
    GLenum format, GLenum type, HTMLVideoElement* video, ExceptionState& exceptionState)
{
    WebGLRenderingContextBase::texSubImage2D(target, level, xoffset, yoffset, format, type, video, exceptionState);
}

void WebGL2RenderingContextBase::texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
    GLenum format, GLenum type, ImageBitmap* bitmap, ExceptionState& exceptionState)
{
    WebGLRenderingContextBase::texSubImage2D(target, level, xoffset, yoffset, format, type, bitmap, exceptionState);
}

void WebGL2RenderingContextBase::texStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
    if (isContextLost() || !validateTexStorage("texStorage2D", target, levels, internalformat, width, height, 1, TexStorageType2D))
        return;

    contextGL()->TexStorage2DEXT(target, levels, internalformat, width, height);
}

void WebGL2RenderingContextBase::texStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
    if (isContextLost() || !validateTexStorage("texStorage3D", target, levels, internalformat, width, height, depth, TexStorageType3D))
        return;

    contextGL()->TexStorage3D(target, levels, internalformat, width, height, depth);
}

void WebGL2RenderingContextBase::texImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, DOMArrayBufferView* pixels)
{
    texImageHelperDOMArrayBufferView(TexImage3D, target, level, internalformat, width, height, border, format, type, depth, 0, 0, 0, pixels);
}

void WebGL2RenderingContextBase::texImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, GLintptr offset)
{
    if (isContextLost())
        return;
    if (!validateTexture3DBinding("texImage3D", target))
        return;
    if (!m_boundPixelUnpackBuffer) {
        synthesizeGLError(GL_INVALID_OPERATION, "texImage3D", "no bound PIXEL_UNPACK_BUFFER");
        return;
    }
    if (!validateTexFunc("texImage3D", TexImage, SourceUnpackBuffer, target, level, internalformat, width, height, depth, border, format, type, 0, 0, 0))
        return;
    if (!validateValueFitNonNegInt32("texImage3D", "offset", offset))
        return;

    contextGL()->TexImage3D(target, level, convertTexInternalFormat(internalformat, type), width, height, depth, border, format, type, reinterpret_cast<const void *>(offset));
}

void WebGL2RenderingContextBase::texSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, DOMArrayBufferView* pixels)
{
    texImageHelperDOMArrayBufferView(TexSubImage3D, target, level, 0, width, height, 0, format, type, depth, xoffset, yoffset, zoffset, pixels);
}

void WebGL2RenderingContextBase::texSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, GLintptr offset)
{
    if (isContextLost())
        return;
    if (!validateTexture3DBinding("texSubImage3D", target))
        return;
    if (!m_boundPixelUnpackBuffer) {
        synthesizeGLError(GL_INVALID_OPERATION, "texSubImage3D", "no bound PIXEL_UNPACK_BUFFER");
        return;
    }
    if (!validateTexFunc("texSubImage3D", TexSubImage, SourceUnpackBuffer, target, level, 0, width, height, depth, 0, format, type, xoffset, yoffset, zoffset))
        return;
    if (!validateValueFitNonNegInt32("texSubImage3D", "offset", offset))
        return;

    contextGL()->TexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, reinterpret_cast<const void*>(offset));
}

void WebGL2RenderingContextBase::texSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLenum format, GLenum type, ImageData* pixels)
{
    texImageHelperImageData(TexSubImage3D, target, level, 0, 0, format, type, 1, xoffset, yoffset, zoffset, pixels);
}

void WebGL2RenderingContextBase::texSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLenum format, GLenum type, HTMLImageElement* image, ExceptionState& exceptionState)
{
    texImageHelperHTMLImageElement(TexSubImage3D, target, level, 0, format, type, xoffset, yoffset, zoffset, image, exceptionState);
}

void WebGL2RenderingContextBase::texSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLenum format, GLenum type, HTMLCanvasElement* canvas, ExceptionState& exceptionState)
{
    texImageHelperHTMLCanvasElement(TexSubImage3D, target, level, 0, format, type, xoffset, yoffset, zoffset, canvas, exceptionState);
}

void WebGL2RenderingContextBase::texSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLenum format, GLenum type, HTMLVideoElement* video, ExceptionState& exceptionState)
{
    texImageHelperHTMLVideoElement(TexSubImage3D, target, level, 0, format, type, xoffset, yoffset, zoffset, video, exceptionState);
}

void WebGL2RenderingContextBase::texSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLenum format, GLenum type, ImageBitmap* bitmap, ExceptionState& exceptionState)
{
    texImageHelperImageBitmap(TexSubImage3D, target, level, 0, format, type, xoffset, yoffset, zoffset, bitmap, exceptionState);
}

void WebGL2RenderingContextBase::copyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
    if (isContextLost())
        return;
    if (!validateTexture3DBinding("copyTexSubImage3D", target))
        return;
    WebGLFramebuffer* readFramebufferBinding = nullptr;
    if (!validateReadBufferAndGetInfo("copyTexSubImage3D", readFramebufferBinding))
        return;
    clearIfComposited();
    ScopedDrawingBufferBinder binder(drawingBuffer(), readFramebufferBinding);
    contextGL()->CopyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y, width, height);
}

void WebGL2RenderingContextBase::compressedTexImage3D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, DOMArrayBufferView* data)
{
    if (isContextLost())
        return;
    if (!validateTexture3DBinding("compressedTexImage3D", target))
        return;
    if (!validateCompressedTexFormat("compressedTexImage3D", internalformat))
        return;
    contextGL()->CompressedTexImage3D(target, level, internalformat, width, height, depth, border, data->byteLength(), data->baseAddress());
}

void WebGL2RenderingContextBase::compressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, DOMArrayBufferView* data)
{
    if (isContextLost())
        return;
    if (!validateTexture3DBinding("compressedTexSubImage3D", target))
        return;
    if (!validateCompressedTexFormat("compressedTexSubImage3D", format))
        return;
    contextGL()->CompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset,
        width, height, depth, format, data->byteLength(), data->baseAddress());
}

GLint WebGL2RenderingContextBase::getFragDataLocation(WebGLProgram* program, const String& name)
{
    if (isContextLost() || !validateWebGLObject("getFragDataLocation", program))
        return -1;

    return contextGL()->GetFragDataLocation(objectOrZero(program), name.utf8().data());
}

void WebGL2RenderingContextBase::uniform1ui(const WebGLUniformLocation* location, GLuint v0)
{
    if (isContextLost() || !location)
        return;

    if (location->program() != m_currentProgram) {
        synthesizeGLError(GL_INVALID_OPERATION, "uniform1ui", "location not for current program");
        return;
    }

    contextGL()->Uniform1ui(location->location(), v0);
}

void WebGL2RenderingContextBase::uniform2ui(const WebGLUniformLocation* location, GLuint v0, GLuint v1)
{
    if (isContextLost() || !location)
        return;

    if (location->program() != m_currentProgram) {
        synthesizeGLError(GL_INVALID_OPERATION, "uniform2ui", "location not for current program");
        return;
    }

    contextGL()->Uniform2ui(location->location(), v0, v1);
}

void WebGL2RenderingContextBase::uniform3ui(const WebGLUniformLocation* location, GLuint v0, GLuint v1, GLuint v2)
{
    if (isContextLost() || !location)
        return;

    if (location->program() != m_currentProgram) {
        synthesizeGLError(GL_INVALID_OPERATION, "uniform3ui", "location not for current program");
        return;
    }

    contextGL()->Uniform3ui(location->location(), v0, v1, v2);
}

void WebGL2RenderingContextBase::uniform4ui(const WebGLUniformLocation* location, GLuint v0, GLuint v1, GLuint v2, GLuint v3)
{
    if (isContextLost() || !location)
        return;

    if (location->program() != m_currentProgram) {
        synthesizeGLError(GL_INVALID_OPERATION, "uniform4ui", "location not for current program");
        return;
    }

    contextGL()->Uniform4ui(location->location(), v0, v1, v2, v3);
}

void WebGL2RenderingContextBase::uniform1uiv(const WebGLUniformLocation* location, const FlexibleUint32ArrayView& v)
{
    if (isContextLost() || !validateUniformParameters<WTF::Uint32Array>("uniform1uiv", location, v, 1))
        return;

    contextGL()->Uniform1uiv(location->location(), v.length(), v.dataMaybeOnStack());
}

void WebGL2RenderingContextBase::uniform1uiv(const WebGLUniformLocation* location, Vector<GLuint>& value)
{
    if (isContextLost() || !validateUniformParameters("uniform1uiv", location, value.data(), value.size(), 1))
        return;

    contextGL()->Uniform1uiv(location->location(), value.size(), value.data());
}

void WebGL2RenderingContextBase::uniform2uiv(const WebGLUniformLocation* location, const FlexibleUint32ArrayView& v)
{
    if (isContextLost() || !validateUniformParameters<WTF::Uint32Array>("uniform2uiv", location, v, 2))
        return;

    contextGL()->Uniform2uiv(location->location(), v.length() >> 1, v.dataMaybeOnStack());
}

void WebGL2RenderingContextBase::uniform2uiv(const WebGLUniformLocation* location, Vector<GLuint>& value)
{
    if (isContextLost() || !validateUniformParameters("uniform2uiv", location, value.data(), value.size(), 2))
        return;

    contextGL()->Uniform2uiv(location->location(), value.size() / 2, value.data());
}

void WebGL2RenderingContextBase::uniform3uiv(const WebGLUniformLocation* location, const FlexibleUint32ArrayView& v)
{
    if (isContextLost() || !validateUniformParameters<WTF::Uint32Array>("uniform3uiv", location, v, 3))
        return;

    contextGL()->Uniform3uiv(location->location(), v.length() / 3, v.dataMaybeOnStack());
}

void WebGL2RenderingContextBase::uniform3uiv(const WebGLUniformLocation* location, Vector<GLuint>& value)
{
    if (isContextLost() || !validateUniformParameters("uniform3uiv", location, value.data(), value.size(), 3))
        return;

    contextGL()->Uniform3uiv(location->location(), value.size() / 3, value.data());
}

void WebGL2RenderingContextBase::uniform4uiv(const WebGLUniformLocation* location, const FlexibleUint32ArrayView& v)
{
    if (isContextLost() || !validateUniformParameters<WTF::Uint32Array>("uniform4uiv", location, v, 4))
        return;

    contextGL()->Uniform4uiv(location->location(), v.length() >> 2, v.dataMaybeOnStack());
}

void WebGL2RenderingContextBase::uniform4uiv(const WebGLUniformLocation* location, Vector<GLuint>& value)
{
    if (isContextLost() || !validateUniformParameters("uniform4uiv", location, value.data(), value.size(), 4))
        return;

    contextGL()->Uniform4uiv(location->location(), value.size() / 4, value.data());
}

void WebGL2RenderingContextBase::uniformMatrix2x3fv(const WebGLUniformLocation* location, GLboolean transpose, DOMFloat32Array* value)
{
    if (isContextLost() || !validateUniformMatrixParameters("uniformMatrix2x3fv", location, transpose, value, 6))
        return;
    contextGL()->UniformMatrix2x3fv(location->location(), value->length() / 6, transpose, value->data());
}

void WebGL2RenderingContextBase::uniformMatrix2x3fv(const WebGLUniformLocation* location, GLboolean transpose, Vector<GLfloat>& value)
{
    if (isContextLost() || !validateUniformMatrixParameters("uniformMatrix2x3fv", location, transpose, value.data(), value.size(), 6))
        return;
    contextGL()->UniformMatrix2x3fv(location->location(), value.size() / 6, transpose, value.data());
}

void WebGL2RenderingContextBase::uniformMatrix3x2fv(const WebGLUniformLocation* location, GLboolean transpose, DOMFloat32Array* value)
{
    if (isContextLost() || !validateUniformMatrixParameters("uniformMatrix3x2fv", location, transpose, value, 6))
        return;
    contextGL()->UniformMatrix3x2fv(location->location(), value->length() / 6, transpose, value->data());
}

void WebGL2RenderingContextBase::uniformMatrix3x2fv(const WebGLUniformLocation* location, GLboolean transpose, Vector<GLfloat>& value)
{
    if (isContextLost() || !validateUniformMatrixParameters("uniformMatrix3x2fv", location, transpose, value.data(), value.size(), 6))
        return;
    contextGL()->UniformMatrix3x2fv(location->location(), value.size() / 6, transpose, value.data());
}

void WebGL2RenderingContextBase::uniformMatrix2x4fv(const WebGLUniformLocation* location, GLboolean transpose, DOMFloat32Array* value)
{
    if (isContextLost() || !validateUniformMatrixParameters("uniformMatrix2x4fv", location, transpose, value, 8))
        return;
    contextGL()->UniformMatrix2x4fv(location->location(), value->length() / 8, transpose, value->data());
}

void WebGL2RenderingContextBase::uniformMatrix2x4fv(const WebGLUniformLocation* location, GLboolean transpose, Vector<GLfloat>& value)
{
    if (isContextLost() || !validateUniformMatrixParameters("uniformMatrix2x4fv", location, transpose, value.data(), value.size(), 8))
        return;
    contextGL()->UniformMatrix2x4fv(location->location(), value.size() / 8, transpose, value.data());
}

void WebGL2RenderingContextBase::uniformMatrix4x2fv(const WebGLUniformLocation* location, GLboolean transpose, DOMFloat32Array* value)
{
    if (isContextLost() || !validateUniformMatrixParameters("uniformMatrix4x2fv", location, transpose, value, 8))
        return;
    contextGL()->UniformMatrix4x2fv(location->location(), value->length() / 8, transpose, value->data());
}

void WebGL2RenderingContextBase::uniformMatrix4x2fv(const WebGLUniformLocation* location, GLboolean transpose, Vector<GLfloat>& value)
{
    if (isContextLost() || !validateUniformMatrixParameters("uniformMatrix4x2fv", location, transpose, value.data(), value.size(), 8))
        return;
    contextGL()->UniformMatrix4x2fv(location->location(), value.size() / 8, transpose, value.data());
}

void WebGL2RenderingContextBase::uniformMatrix3x4fv(const WebGLUniformLocation* location, GLboolean transpose, DOMFloat32Array* value)
{
    if (isContextLost() || !validateUniformMatrixParameters("uniformMatrix3x4fv", location, transpose, value, 12))
        return;
    contextGL()->UniformMatrix3x4fv(location->location(), value->length() / 12, transpose, value->data());
}

void WebGL2RenderingContextBase::uniformMatrix3x4fv(const WebGLUniformLocation* location, GLboolean transpose, Vector<GLfloat>& value)
{
    if (isContextLost() || !validateUniformMatrixParameters("uniformMatrix3x4fv", location, transpose, value.data(), value.size(), 12))
        return;
    contextGL()->UniformMatrix3x4fv(location->location(), value.size() / 12, transpose, value.data());
}

void WebGL2RenderingContextBase::uniformMatrix4x3fv(const WebGLUniformLocation* location, GLboolean transpose, DOMFloat32Array* value)
{
    if (isContextLost() || !validateUniformMatrixParameters("uniformMatrix4x3fv", location, transpose, value, 12))
        return;
    contextGL()->UniformMatrix4x3fv(location->location(), value->length() / 12, transpose, value->data());
}

void WebGL2RenderingContextBase::uniformMatrix4x3fv(const WebGLUniformLocation* location, GLboolean transpose, Vector<GLfloat>& value)
{
    if (isContextLost() || !validateUniformMatrixParameters("uniformMatrix4x3fv", location, transpose, value.data(), value.size(), 12))
        return;
    contextGL()->UniformMatrix4x3fv(location->location(), value.size() / 12, transpose, value.data());
}

void WebGL2RenderingContextBase::vertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w)
{
    if (isContextLost())
        return;
    contextGL()->VertexAttribI4i(index, x, y, z, w);
    setVertexAttribType(index, Int32ArrayType);
}

void WebGL2RenderingContextBase::vertexAttribI4iv(GLuint index, const DOMInt32Array* v)
{
    if (isContextLost())
        return;
    if (!v || v->length() < 4) {
        synthesizeGLError(GL_INVALID_VALUE, "vertexAttribI4iv", "invalid array");
        return;
    }
    contextGL()->VertexAttribI4iv(index, v->data());
    setVertexAttribType(index, Int32ArrayType);
}

void WebGL2RenderingContextBase::vertexAttribI4iv(GLuint index, const Vector<GLint>& v)
{
    if (isContextLost())
        return;
    if (v.size() < 4) {
        synthesizeGLError(GL_INVALID_VALUE, "vertexAttribI4iv", "invalid array");
        return;
    }
    contextGL()->VertexAttribI4iv(index, v.data());
    setVertexAttribType(index, Int32ArrayType);
}

void WebGL2RenderingContextBase::vertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w)
{
    if (isContextLost())
        return;
    contextGL()->VertexAttribI4ui(index, x, y, z, w);
    setVertexAttribType(index, Uint32ArrayType);
}

void WebGL2RenderingContextBase::vertexAttribI4uiv(GLuint index, const DOMUint32Array* v)
{
    if (isContextLost())
        return;
    if (!v || v->length() < 4) {
        synthesizeGLError(GL_INVALID_VALUE, "vertexAttribI4uiv", "invalid array");
        return;
    }
    contextGL()->VertexAttribI4uiv(index, v->data());
    setVertexAttribType(index, Uint32ArrayType);
}

void WebGL2RenderingContextBase::vertexAttribI4uiv(GLuint index, const Vector<GLuint>& v)
{
    if (isContextLost())
        return;
    if (v.size() < 4) {
        synthesizeGLError(GL_INVALID_VALUE, "vertexAttribI4uiv", "invalid array");
        return;
    }
    contextGL()->VertexAttribI4uiv(index, v.data());
    setVertexAttribType(index, Uint32ArrayType);
}

void WebGL2RenderingContextBase::vertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, long long offset)
{
    if (isContextLost())
        return;
    if (index >= m_maxVertexAttribs) {
        synthesizeGLError(GL_INVALID_VALUE, "vertexAttribIPointer", "index out of range");
        return;
    }
    if (!validateValueFitNonNegInt32("vertexAttribIPointer", "offset", offset))
        return;
    if (!m_boundArrayBuffer) {
        synthesizeGLError(GL_INVALID_OPERATION, "vertexAttribIPointer", "no bound ARRAY_BUFFER");
        return;
    }

    m_boundVertexArrayObject->setArrayBufferForAttrib(index, m_boundArrayBuffer);
    contextGL()->VertexAttribIPointer(index, size, type, stride, reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
}

/* Writing to the drawing buffer */
void WebGL2RenderingContextBase::vertexAttribDivisor(GLuint index, GLuint divisor)
{
    if (isContextLost())
        return;

    if (index >= m_maxVertexAttribs) {
        synthesizeGLError(GL_INVALID_VALUE, "vertexAttribDivisor", "index out of range");
        return;
    }

    contextGL()->VertexAttribDivisorANGLE(index, divisor);
}

void WebGL2RenderingContextBase::drawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instanceCount)
{
    if (!validateDrawArrays("drawArraysInstanced"))
        return;

    if (!m_boundVertexArrayObject->isAllEnabledAttribBufferBound()) {
        synthesizeGLError(GL_INVALID_OPERATION, "drawArraysInstanced", "no buffer is bound to enabled attribute");
        return;
    }

    ScopedRGBEmulationColorMask emulationColorMask(contextGL(), m_colorMask, m_drawingBuffer.get());
    clearIfComposited();
    contextGL()->DrawArraysInstancedANGLE(mode, first, count, instanceCount);
    markContextChanged(CanvasChanged);
}

void WebGL2RenderingContextBase::drawElementsInstanced(GLenum mode, GLsizei count, GLenum type, long long offset, GLsizei instanceCount)
{
    if (!validateDrawElements("drawElementsInstanced", type, offset))
        return;

    if (!m_boundVertexArrayObject->isAllEnabledAttribBufferBound()) {
        synthesizeGLError(GL_INVALID_OPERATION, "drawElementsInstanced", "no buffer is bound to enabled attribute");
        return;
    }

    ScopedRGBEmulationColorMask emulationColorMask(contextGL(), m_colorMask, m_drawingBuffer.get());
    clearIfComposited();
    contextGL()->DrawElementsInstancedANGLE(mode, count, type, reinterpret_cast<void*>(static_cast<intptr_t>(offset)), instanceCount);
    markContextChanged(CanvasChanged);
}

void WebGL2RenderingContextBase::drawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, long long offset)
{
    if (!validateDrawElements("drawRangeElements", type, offset))
        return;

    if (!m_boundVertexArrayObject->isAllEnabledAttribBufferBound()) {
        synthesizeGLError(GL_INVALID_OPERATION, "drawRangeElements", "no buffer is bound to enabled attribute");
        return;
    }

    ScopedRGBEmulationColorMask emulationColorMask(contextGL(), m_colorMask, m_drawingBuffer.get());
    clearIfComposited();
    contextGL()->DrawRangeElements(mode, start, end, count, type, reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
    markContextChanged(CanvasChanged);
}

void WebGL2RenderingContextBase::drawBuffers(const Vector<GLenum>& buffers)
{
    if (isContextLost())
        return;

    ScopedRGBEmulationColorMask emulationColorMask(contextGL(), m_colorMask, m_drawingBuffer.get());
    GLsizei n = buffers.size();
    const GLenum* bufs = buffers.data();
    for (GLsizei i = 0; i < n; ++i) {
        switch (bufs[i]) {
        case GL_NONE:
        case GL_BACK:
        case GL_COLOR_ATTACHMENT0:
            break;
        default:
            if (bufs[i] > GL_COLOR_ATTACHMENT0
                && bufs[i] < static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + maxColorAttachments()))
                break;
            synthesizeGLError(GL_INVALID_ENUM, "drawBuffers", "invalid buffer");
            return;
        }
    }
    if (!m_framebufferBinding) {
        if (n != 1) {
            synthesizeGLError(GL_INVALID_OPERATION, "drawBuffers", "the number of buffers is not 1");
            return;
        }
        if (bufs[0] != GL_BACK && bufs[0] != GL_NONE) {
            synthesizeGLError(GL_INVALID_OPERATION, "drawBuffers", "BACK or NONE");
            return;
        }
        // Because the backbuffer is simulated on all current WebKit ports, we need to change BACK to COLOR_ATTACHMENT0.
        GLenum value = (bufs[0] == GL_BACK) ? GL_COLOR_ATTACHMENT0 : GL_NONE;
        contextGL()->DrawBuffersEXT(1, &value);
        setBackDrawBuffer(bufs[0]);
    } else {
        if (n > maxDrawBuffers()) {
            synthesizeGLError(GL_INVALID_VALUE, "drawBuffers", "more than max draw buffers");
            return;
        }
        for (GLsizei i = 0; i < n; ++i) {
            if (bufs[i] != GL_NONE && bufs[i] != static_cast<GLenum>(GL_COLOR_ATTACHMENT0_EXT + i)) {
                synthesizeGLError(GL_INVALID_OPERATION, "drawBuffers", "COLOR_ATTACHMENTi_EXT or NONE");
                return;
            }
        }
        m_framebufferBinding->drawBuffers(buffers);
    }
}

bool WebGL2RenderingContextBase::validateClearBuffer(const char* functionName, GLenum buffer, GLsizei size)
{
    switch (buffer) {
    case GL_COLOR:
        if (size < 4) {
            synthesizeGLError(GL_INVALID_VALUE, functionName, "invalid array size");
            return false;
        }
        break;
    case GL_DEPTH:
    case GL_STENCIL:
        if (size < 1) {
            synthesizeGLError(GL_INVALID_VALUE, functionName, "invalid array size");
            return false;
        }
        break;
    default:
        synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid buffer");
        return false;
    }
    return true;
}

WebGLTexture* WebGL2RenderingContextBase::validateTexImageBinding(const char* funcName, TexImageFunctionID functionID, GLenum target)
{
    if (functionID == TexImage3D || functionID == TexSubImage3D)
        return validateTexture3DBinding(funcName, target);
    return validateTexture2DBinding(funcName, target);
}

void WebGL2RenderingContextBase::clearBufferiv(GLenum buffer, GLint drawbuffer, DOMInt32Array* value)
{
    if (isContextLost() || !validateClearBuffer("clearBufferiv", buffer, value->length()))
        return;

    contextGL()->ClearBufferiv(buffer, drawbuffer, value->data());
}

void WebGL2RenderingContextBase::clearBufferiv(GLenum buffer, GLint drawbuffer, const Vector<GLint>& value)
{
    if (isContextLost() || !validateClearBuffer("clearBufferiv", buffer, value.size()))
        return;

    contextGL()->ClearBufferiv(buffer, drawbuffer, value.data());
}

void WebGL2RenderingContextBase::clearBufferuiv(GLenum buffer, GLint drawbuffer, DOMUint32Array* value)
{
    if (isContextLost() || !validateClearBuffer("clearBufferuiv", buffer, value->length()))
        return;

    contextGL()->ClearBufferuiv(buffer, drawbuffer, value->data());
}

void WebGL2RenderingContextBase::clearBufferuiv(GLenum buffer, GLint drawbuffer, const Vector<GLuint>& value)
{
    if (isContextLost() || !validateClearBuffer("clearBufferuiv", buffer, value.size()))
        return;

    contextGL()->ClearBufferuiv(buffer, drawbuffer, value.data());
}

void WebGL2RenderingContextBase::clearBufferfv(GLenum buffer, GLint drawbuffer, DOMFloat32Array* value)
{
    if (isContextLost() || !validateClearBuffer("clearBufferfv", buffer, value->length()))
        return;

    contextGL()->ClearBufferfv(buffer, drawbuffer, value->data());
}

void WebGL2RenderingContextBase::clearBufferfv(GLenum buffer, GLint drawbuffer, const Vector<GLfloat>& value)
{
    if (isContextLost() || !validateClearBuffer("clearBufferfv", buffer, value.size()))
        return;

    contextGL()->ClearBufferfv(buffer, drawbuffer, value.data());
}

void WebGL2RenderingContextBase::clearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil)
{
    if (isContextLost())
        return;

    contextGL()->ClearBufferfi(buffer, drawbuffer, depth, stencil);
}

WebGLQuery* WebGL2RenderingContextBase::createQuery()
{
    if (isContextLost())
        return nullptr;
    WebGLQuery* o = WebGLQuery::create(this);
    addSharedObject(o);
    return o;
}

void WebGL2RenderingContextBase::deleteQuery(WebGLQuery* query)
{
    if (isContextLost() || !query)
        return;

    if (m_currentBooleanOcclusionQuery == query) {
        contextGL()->EndQueryEXT(m_currentBooleanOcclusionQuery->getTarget());
        m_currentBooleanOcclusionQuery = nullptr;
    }

    if (m_currentTransformFeedbackPrimitivesWrittenQuery == query) {
        contextGL()->EndQueryEXT(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
        m_currentTransformFeedbackPrimitivesWrittenQuery = nullptr;
    }

    deleteObject(query);
}

GLboolean WebGL2RenderingContextBase::isQuery(WebGLQuery* query)
{
    if (isContextLost() || !query)
        return 0;

    return contextGL()->IsQueryEXT(query->object());
}

void WebGL2RenderingContextBase::beginQuery(ScriptState* scriptState, GLenum target, WebGLQuery* query)
{
    bool deleted;
    if (!query) {
        synthesizeGLError(GL_INVALID_OPERATION, "beginQuery", "query object is null");
        return;
    }

    if (!checkObjectToBeBound("beginQuery", query, deleted))
        return;
    if (deleted) {
        synthesizeGLError(GL_INVALID_OPERATION, "beginQuery", "attempted to begin a deleted query object");
        return;
    }

    if (query->getTarget() && query->getTarget() != target) {
        synthesizeGLError(GL_INVALID_OPERATION, "beginQuery", "query type does not match target");
        return;
    }

    switch (target) {
    case GL_ANY_SAMPLES_PASSED:
    case GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
        {
            if (m_currentBooleanOcclusionQuery) {
                synthesizeGLError(GL_INVALID_OPERATION, "beginQuery", "a query is already active for target");
                return;
            }
            m_currentBooleanOcclusionQuery = query;
        }
        break;
    case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
        {
            if (m_currentTransformFeedbackPrimitivesWrittenQuery) {
                synthesizeGLError(GL_INVALID_OPERATION, "beginQuery", "a query is already active for target");
                return;
            }
            m_currentTransformFeedbackPrimitivesWrittenQuery = query;
        }
        break;
    default:
        synthesizeGLError(GL_INVALID_ENUM, "beginQuery", "invalid target");
        return;
    }

    if (!query->getTarget())
        query->setTarget(target);

    contextGL()->BeginQueryEXT(target, query->object());
    preserveObjectWrapper(scriptState, this, V8HiddenValue::webglQueries(scriptState->isolate()), &m_queryWrappers, static_cast<uint32_t>(target), query);
}

void WebGL2RenderingContextBase::endQuery(GLenum target)
{
    if (isContextLost())
        return;

    switch (target) {
    case GL_ANY_SAMPLES_PASSED:
    case GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
        {
            if (m_currentBooleanOcclusionQuery && m_currentBooleanOcclusionQuery->getTarget() == target) {
                m_currentBooleanOcclusionQuery->resetCachedResult();
                m_currentBooleanOcclusionQuery = nullptr;
            } else {
                synthesizeGLError(GL_INVALID_OPERATION, "endQuery", "target query is not active");
                return;
            }
        }
        break;
    case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
        {
            if (m_currentTransformFeedbackPrimitivesWrittenQuery) {
                m_currentTransformFeedbackPrimitivesWrittenQuery->resetCachedResult();
                m_currentTransformFeedbackPrimitivesWrittenQuery = nullptr;
            } else {
                synthesizeGLError(GL_INVALID_OPERATION, "endQuery", "target query is not active");
                return;
            }
        }
        break;
    default:
        synthesizeGLError(GL_INVALID_ENUM, "endQuery", "invalid target");
        return;
    }

    contextGL()->EndQueryEXT(target);
}

WebGLQuery* WebGL2RenderingContextBase::getQuery(GLenum target, GLenum pname)
{
    if (isContextLost())
        return nullptr;

    if (pname != GL_CURRENT_QUERY) {
        synthesizeGLError(GL_INVALID_ENUM, "getQuery", "invalid parameter name");
        return nullptr;
    }

    switch (target) {
    case GL_ANY_SAMPLES_PASSED:
    case GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
        if (m_currentBooleanOcclusionQuery && m_currentBooleanOcclusionQuery->getTarget() == target)
            return m_currentBooleanOcclusionQuery;
        break;
    case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
        return m_currentTransformFeedbackPrimitivesWrittenQuery;
    default:
        synthesizeGLError(GL_INVALID_ENUM, "getQuery", "invalid target");
        return nullptr;
    }
    return nullptr;
}

ScriptValue WebGL2RenderingContextBase::getQueryParameter(ScriptState* scriptState, WebGLQuery* query, GLenum pname)
{
    bool deleted;
    if (!query) {
        synthesizeGLError(GL_INVALID_OPERATION, "getQueryParameter", "query object is null");
        return ScriptValue::createNull(scriptState);
    }
    if (!checkObjectToBeBound("getQueryParameter", query, deleted))
        return ScriptValue::createNull(scriptState);
    if (deleted) {
        synthesizeGLError(GL_INVALID_OPERATION, "getQueryParameter", "attempted to access to a deleted query object");
        return ScriptValue::createNull(scriptState);
    }

    // Query is non-null at this point.
    if (!query->getTarget()) {
        synthesizeGLError(GL_INVALID_OPERATION, "getQueryParameter", "'query' is not a query object yet, since it has't been used by beginQuery");
        return ScriptValue::createNull(scriptState);
    }
    if (query == m_currentBooleanOcclusionQuery || query == m_currentTransformFeedbackPrimitivesWrittenQuery) {
        synthesizeGLError(GL_INVALID_OPERATION, "getQueryParameter", "query is currently active");
        return ScriptValue::createNull(scriptState);
    }

    switch (pname) {
    case GL_QUERY_RESULT:
        {
            query->updateCachedResult(contextGL());
            return WebGLAny(scriptState, query->getQueryResult());
        }
    case GL_QUERY_RESULT_AVAILABLE:
        {
            query->updateCachedResult(contextGL());
            return WebGLAny(scriptState, query->isQueryResultAvailable());
        }
    default:
        synthesizeGLError(GL_INVALID_ENUM, "getQueryParameter", "invalid parameter name");
        return ScriptValue::createNull(scriptState);
    }
}

WebGLSampler* WebGL2RenderingContextBase::createSampler()
{
    if (isContextLost())
        return nullptr;
    WebGLSampler* o = WebGLSampler::create(this);
    addSharedObject(o);
    return o;
}

void WebGL2RenderingContextBase::deleteSampler(WebGLSampler* sampler)
{
    if (isContextLost())
        return;

    for (size_t i = 0; i < m_samplerUnits.size(); ++i) {
        if (sampler == m_samplerUnits[i]) {
            m_samplerUnits[i] = nullptr;
            contextGL()->BindSampler(i, 0);
        }
    }

    deleteObject(sampler);
}

GLboolean WebGL2RenderingContextBase::isSampler(WebGLSampler* sampler)
{
    if (isContextLost() || !sampler)
        return 0;

    return contextGL()->IsSampler(sampler->object());
}

void WebGL2RenderingContextBase::bindSampler(ScriptState* scriptState, GLuint unit, WebGLSampler* sampler)
{
    if (isContextLost())
        return;

    bool deleted;
    if (!checkObjectToBeBound("bindSampler", sampler, deleted))
        return;
    if (deleted) {
        synthesizeGLError(GL_INVALID_OPERATION, "bindSampler", "attempted to bind a deleted sampler");
        return;
    }

    if (unit >= m_samplerUnits.size()) {
        synthesizeGLError(GL_INVALID_VALUE, "bindSampler", "texture unit out of range");
        return;
    }

    m_samplerUnits[unit] = sampler;

    contextGL()->BindSampler(unit, objectOrZero(sampler));

    preserveObjectWrapper(scriptState, this, V8HiddenValue::webglSamplers(scriptState->isolate()), &m_samplerWrappers, static_cast<uint32_t>(unit), sampler);
}

void WebGL2RenderingContextBase::samplerParameter(WebGLSampler* sampler, GLenum pname, GLfloat paramf, GLint parami, bool isFloat)
{
    if (isContextLost() || !validateWebGLObject("samplerParameter", sampler))
        return;

    GLint param = isFloat ? static_cast<GLint>(paramf) : parami;
    switch (pname) {
    case GL_TEXTURE_MAX_LOD:
    case GL_TEXTURE_MIN_LOD:
        break;
    case GL_TEXTURE_COMPARE_FUNC:
        switch (param) {
        case GL_LEQUAL:
        case GL_GEQUAL:
        case GL_LESS:
        case GL_GREATER:
        case GL_EQUAL:
        case GL_NOTEQUAL:
        case GL_ALWAYS:
        case GL_NEVER:
            break;
        default:
            synthesizeGLError(GL_INVALID_ENUM, "samplerParameter", "invalid parameter");
            return;
        }
        break;
    case GL_TEXTURE_COMPARE_MODE:
        switch (param) {
        case GL_COMPARE_REF_TO_TEXTURE:
        case GL_NONE:
            break;
        default:
            synthesizeGLError(GL_INVALID_ENUM, "samplerParameter", "invalid parameter");
            return;
        }
        break;
    case GL_TEXTURE_MAG_FILTER:
        switch (param) {
        case GL_NEAREST:
        case GL_LINEAR:
            break;
        default:
            synthesizeGLError(GL_INVALID_ENUM, "samplerParameter", "invalid parameter");
            return;
        }
        break;
    case GL_TEXTURE_MIN_FILTER:
        switch (param) {
        case GL_NEAREST:
        case GL_LINEAR:
        case GL_NEAREST_MIPMAP_NEAREST:
        case GL_LINEAR_MIPMAP_NEAREST:
        case GL_NEAREST_MIPMAP_LINEAR:
        case GL_LINEAR_MIPMAP_LINEAR:
            break;
        default:
            synthesizeGLError(GL_INVALID_ENUM, "samplerParameter", "invalid parameter");
            return;
        }
        break;
    case GL_TEXTURE_WRAP_R:
    case GL_TEXTURE_WRAP_S:
    case GL_TEXTURE_WRAP_T:
        switch (param) {
        case GL_CLAMP_TO_EDGE:
        case GL_MIRRORED_REPEAT:
        case GL_REPEAT:
            break;
        default:
            synthesizeGLError(GL_INVALID_ENUM, "samplerParameter", "invalid parameter");
            return;
        }
        break;
    default:
        synthesizeGLError(GL_INVALID_ENUM, "samplerParameter", "invalid parameter name");
        return;
    }

    if (isFloat) {
        contextGL()->SamplerParameterf(objectOrZero(sampler), pname, paramf);
    } else {
        contextGL()->SamplerParameteri(objectOrZero(sampler), pname, parami);
    }
}

void WebGL2RenderingContextBase::samplerParameteri(WebGLSampler* sampler, GLenum pname, GLint param)
{
    samplerParameter(sampler, pname, 0, param, false);
}

void WebGL2RenderingContextBase::samplerParameterf(WebGLSampler* sampler, GLenum pname, GLfloat param)
{
    samplerParameter(sampler, pname, param, 0, true);
}

ScriptValue WebGL2RenderingContextBase::getSamplerParameter(ScriptState* scriptState, WebGLSampler* sampler, GLenum pname)
{
    if (isContextLost() || !validateWebGLObject("getSamplerParameter", sampler))
        return ScriptValue::createNull(scriptState);

    switch (pname) {
    case GL_TEXTURE_COMPARE_FUNC:
    case GL_TEXTURE_COMPARE_MODE:
    case GL_TEXTURE_MAG_FILTER:
    case GL_TEXTURE_MIN_FILTER:
    case GL_TEXTURE_WRAP_R:
    case GL_TEXTURE_WRAP_S:
    case GL_TEXTURE_WRAP_T:
        {
            GLint value = 0;
            contextGL()->GetSamplerParameteriv(objectOrZero(sampler), pname, &value);
            return WebGLAny(scriptState, static_cast<unsigned>(value));
        }
    case GL_TEXTURE_MAX_LOD:
    case GL_TEXTURE_MIN_LOD:
        {
            GLfloat value = 0.f;
            contextGL()->GetSamplerParameterfv(objectOrZero(sampler), pname, &value);
            return WebGLAny(scriptState, value);
        }
    default:
        synthesizeGLError(GL_INVALID_ENUM, "getSamplerParameter", "invalid parameter name");
        return ScriptValue::createNull(scriptState);
    }
}

WebGLSync* WebGL2RenderingContextBase::fenceSync(GLenum condition, GLbitfield flags)
{
    if (isContextLost())
        return nullptr;

    WebGLSync* o = WebGLFenceSync::create(this, condition, flags);
    addSharedObject(o);
    return o;
}

GLboolean WebGL2RenderingContextBase::isSync(WebGLSync* sync)
{
    if (isContextLost() || !sync)
        return 0;

    return contextGL()->IsSync(sync->object());
}

void WebGL2RenderingContextBase::deleteSync(WebGLSync* sync)
{
    deleteObject(sync);
}

GLenum WebGL2RenderingContextBase::clientWaitSync(WebGLSync* sync, GLbitfield flags, GLint64 timeout)
{
    if (isContextLost() || !validateWebGLObject("clientWaitSync", sync))
        return GL_WAIT_FAILED;

    if (timeout < -1) {
        synthesizeGLError(GL_INVALID_VALUE, "clientWaitSync", "timeout < -1");
        return GL_WAIT_FAILED;
    }

    GLuint64 timeout64 = timeout == -1 ? GL_TIMEOUT_IGNORED : static_cast<GLuint64>(timeout);
    return contextGL()->ClientWaitSync(syncObjectOrZero(sync), flags, timeout64);
}

void WebGL2RenderingContextBase::waitSync(WebGLSync* sync, GLbitfield flags, GLint64 timeout)
{
    if (isContextLost() || !validateWebGLObject("waitSync", sync))
        return;

    if (timeout < -1) {
        synthesizeGLError(GL_INVALID_VALUE, "waitSync", "timeout < -1");
        return;
    }

    GLuint64 timeout64 = timeout == -1 ? GL_TIMEOUT_IGNORED : static_cast<GLuint64>(timeout);
    contextGL()->WaitSync(syncObjectOrZero(sync), flags, timeout64);
}

ScriptValue WebGL2RenderingContextBase::getSyncParameter(ScriptState* scriptState, WebGLSync* sync, GLenum pname)
{
    if (isContextLost() || !validateWebGLObject("getSyncParameter", sync))
        return ScriptValue::createNull(scriptState);

    switch (pname) {
    case GL_OBJECT_TYPE:
    case GL_SYNC_STATUS:
    case GL_SYNC_CONDITION:
    case GL_SYNC_FLAGS:
        {
            GLint value = 0;
            GLsizei length = -1;
            contextGL()->GetSynciv(syncObjectOrZero(sync), pname, 1, &length, &value);
            return WebGLAny(scriptState, static_cast<unsigned>(value));
        }
    default:
        synthesizeGLError(GL_INVALID_ENUM, "getSyncParameter", "invalid parameter name");
        return ScriptValue::createNull(scriptState);
    }
}

WebGLTransformFeedback* WebGL2RenderingContextBase::createTransformFeedback()
{
    if (isContextLost())
        return nullptr;
    WebGLTransformFeedback* o = WebGLTransformFeedback::create(this);
    addSharedObject(o);
    return o;
}

void WebGL2RenderingContextBase::deleteTransformFeedback(WebGLTransformFeedback* feedback)
{
    if (feedback == m_transformFeedbackBinding)
        m_transformFeedbackBinding = nullptr;

    deleteObject(feedback);
}

GLboolean WebGL2RenderingContextBase::isTransformFeedback(WebGLTransformFeedback* feedback)
{
    if (isContextLost() || !feedback)
        return 0;

    if (!feedback->hasEverBeenBound())
        return 0;

    return contextGL()->IsTransformFeedback(feedback->object());
}

void WebGL2RenderingContextBase::bindTransformFeedback(ScriptState* scriptState, GLenum target, WebGLTransformFeedback* feedback)
{
    bool deleted;
    if (!checkObjectToBeBound("bindTransformFeedback", feedback, deleted))
        return;
    if (deleted) {
        synthesizeGLError(GL_INVALID_OPERATION, "bindTransformFeedback", "attempted to bind a deleted transform feedback object");
        return;
    }

    if (target != GL_TRANSFORM_FEEDBACK) {
        synthesizeGLError(GL_INVALID_ENUM, "bindTransformFeedback", "target must be TRANSFORM_FEEDBACK");
        return;
    }

    m_transformFeedbackBinding = feedback;

    contextGL()->BindTransformFeedback(target, objectOrZero(feedback));
    if (feedback) {
        feedback->setTarget(target);
        preserveObjectWrapper(scriptState, this, V8HiddenValue::webglMisc(scriptState->isolate()), &m_miscWrappers, static_cast<uint32_t>(PreservedTransformFeedback), feedback);
    }

}

void WebGL2RenderingContextBase::beginTransformFeedback(GLenum primitiveMode)
{
    if (isContextLost())
        return;
    if (!validateTransformFeedbackPrimitiveMode("beginTransformFeedback", primitiveMode))
        return;

    contextGL()->BeginTransformFeedback(primitiveMode);

    if (m_currentProgram)
        m_currentProgram->increaseActiveTransformFeedbackCount();

    if (m_transformFeedbackBinding)
        m_transformFeedbackBinding->setProgram(m_currentProgram);
}

void WebGL2RenderingContextBase::endTransformFeedback()
{
    if (isContextLost())
        return;

    contextGL()->EndTransformFeedback();

    if (m_currentProgram)
        m_currentProgram->decreaseActiveTransformFeedbackCount();
}

void WebGL2RenderingContextBase::transformFeedbackVaryings(WebGLProgram* program, const Vector<String>& varyings, GLenum bufferMode)
{
    if (isContextLost() || !validateWebGLObject("transformFeedbackVaryings", program))
        return;

    switch (bufferMode) {
    case GL_SEPARATE_ATTRIBS:
        if (varyings.size() > static_cast<size_t>(m_maxTransformFeedbackSeparateAttribs)) {
            synthesizeGLError(GL_INVALID_VALUE, "transformFeedbackVaryings", "too many varyings");
            return;
        }
        break;
    case GL_INTERLEAVED_ATTRIBS:
        break;
    default:
        synthesizeGLError(GL_INVALID_ENUM, "transformFeedbackVaryings", "invalid buffer mode");
        return;
    }

    Vector<CString> keepAlive; // Must keep these instances alive while looking at their data
    Vector<const char*> varyingStrings;
    for (size_t i = 0; i < varyings.size(); ++i) {
        keepAlive.append(varyings[i].ascii());
        varyingStrings.append(keepAlive.last().data());
    }

    contextGL()->TransformFeedbackVaryings(objectOrZero(program), varyings.size(), varyingStrings.data(), bufferMode);
}

WebGLActiveInfo* WebGL2RenderingContextBase::getTransformFeedbackVarying(WebGLProgram* program, GLuint index)
{
    if (isContextLost() || !validateWebGLObject("getTransformFeedbackVarying", program))
        return nullptr;

    if (!program->linkStatus(this)) {
        synthesizeGLError(GL_INVALID_OPERATION, "getTransformFeedbackVarying", "program not linked");
        return nullptr;
    }
    GLint maxIndex = 0;
    contextGL()->GetProgramiv(objectOrZero(program), GL_TRANSFORM_FEEDBACK_VARYINGS, &maxIndex);
    if (index >= static_cast<GLuint>(maxIndex)) {
        synthesizeGLError(GL_INVALID_VALUE, "getTransformFeedbackVarying", "invalid index");
        return nullptr;
    }

    GLint maxNameLength = -1;
    contextGL()->GetProgramiv(objectOrZero(program), GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH, &maxNameLength);
    if (maxNameLength <= 0) {
        return nullptr;
    }
    std::unique_ptr<GLchar[]> name = wrapArrayUnique(new GLchar[maxNameLength]);
    GLsizei length = 0;
    GLsizei size = 0;
    GLenum type = 0;
    contextGL()->GetTransformFeedbackVarying(objectOrZero(program), index, maxNameLength, &length, &size, &type, name.get());

    if (length == 0 || size == 0 || type == 0) {
        return nullptr;
    }

    return WebGLActiveInfo::create(String(name.get(), length), type, size);
}

void WebGL2RenderingContextBase::pauseTransformFeedback()
{
    if (isContextLost())
        return;

    contextGL()->PauseTransformFeedback();
}

void WebGL2RenderingContextBase::resumeTransformFeedback()
{
    if (isContextLost())
        return;

    if (m_transformFeedbackBinding && m_transformFeedbackBinding->getProgram() != m_currentProgram) {
        synthesizeGLError(GL_INVALID_OPERATION, "resumeTransformFeedback", "the program object is not active");
        return;
    }

    contextGL()->ResumeTransformFeedback();
}

bool WebGL2RenderingContextBase::validateTransformFeedbackPrimitiveMode(const char* functionName, GLenum primitiveMode)
{
    switch (primitiveMode) {
    case GL_POINTS:
    case GL_LINES:
    case GL_TRIANGLES:
        return true;
    default:
        synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid transform feedback primitive mode");
        return false;
    }
}

void WebGL2RenderingContextBase::bindBufferBase(GLenum target, GLuint index, WebGLBuffer* buffer)
{
    if (isContextLost())
        return;
    bool deleted;
    if (!checkObjectToBeBound("bindBufferBase", buffer, deleted))
        return;
    if (deleted)
        buffer = 0;
    if (!validateAndUpdateBufferBindBaseTarget("bindBufferBase", target, index, buffer))
        return;

    contextGL()->BindBufferBase(target, index, objectOrZero(buffer));
}

void WebGL2RenderingContextBase::bindBufferRange(GLenum target, GLuint index, WebGLBuffer* buffer, long long offset, long long size)
{
    if (isContextLost())
        return;
    bool deleted;
    if (!checkObjectToBeBound("bindBufferRange", buffer, deleted))
        return;
    if (deleted)
        buffer = 0;
    if (!validateValueFitNonNegInt32("bindBufferRange", "offset", offset)
        || !validateValueFitNonNegInt32("bindBufferRange", "size", size)) {
        return;
    }

    if (!validateAndUpdateBufferBindBaseTarget("bindBufferRange", target, index, buffer))
        return;

    contextGL()->BindBufferRange(target, index, objectOrZero(buffer), static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size));
}

ScriptValue WebGL2RenderingContextBase::getIndexedParameter(ScriptState* scriptState, GLenum target, GLuint index)
{
    if (isContextLost())
        return ScriptValue::createNull(scriptState);

    switch (target) {
    case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
        if (index >= m_boundIndexedTransformFeedbackBuffers.size()) {
            synthesizeGLError(GL_INVALID_VALUE, "getIndexedParameter", "index out of range");
            return ScriptValue::createNull(scriptState);
        }
        return WebGLAny(scriptState, m_boundIndexedTransformFeedbackBuffers[index].get());
    case GL_UNIFORM_BUFFER_BINDING:
        if (index >= m_boundIndexedUniformBuffers.size()) {
            synthesizeGLError(GL_INVALID_VALUE, "getIndexedParameter", "index out of range");
            return ScriptValue::createNull(scriptState);
        }
        return WebGLAny(scriptState, m_boundIndexedUniformBuffers[index].get());
    case GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
    case GL_TRANSFORM_FEEDBACK_BUFFER_START:
    case GL_UNIFORM_BUFFER_SIZE:
    case GL_UNIFORM_BUFFER_START:
        {
            GLint64 value = -1;
            contextGL()->GetInteger64i_v(target, index, &value);
            return WebGLAny(scriptState, value);
        }
    default:
        synthesizeGLError(GL_INVALID_ENUM, "getIndexedParameter", "invalid parameter name");
        return ScriptValue::createNull(scriptState);
    }
}

Vector<GLuint> WebGL2RenderingContextBase::getUniformIndices(WebGLProgram* program, const Vector<String>& uniformNames)
{
    Vector<GLuint> result;
    if (isContextLost() || !validateWebGLObject("getUniformIndices", program))
        return result;

    Vector<CString> keepAlive; // Must keep these instances alive while looking at their data
    Vector<const char*> uniformStrings;
    for (size_t i = 0; i < uniformNames.size(); ++i) {
        keepAlive.append(uniformNames[i].ascii());
        uniformStrings.append(keepAlive.last().data());
    }

    result.resize(uniformNames.size());
    contextGL()->GetUniformIndices(objectOrZero(program), uniformStrings.size(), uniformStrings.data(), result.data());
    return result;
}

ScriptValue WebGL2RenderingContextBase::getActiveUniforms(ScriptState* scriptState, WebGLProgram* program, const Vector<GLuint>& uniformIndices, GLenum pname)
{
    if (isContextLost() || !validateWebGLObject("getActiveUniforms", program))
        return ScriptValue::createNull(scriptState);

    enum ReturnType {
        EnumType,
        UnsignedIntType,
        IntType,
        BoolType
    };

    int returnType;
    switch (pname) {
    case GL_UNIFORM_TYPE:
        returnType = EnumType;
        break;
    case GL_UNIFORM_SIZE:
        returnType = UnsignedIntType;
        break;
    case GL_UNIFORM_BLOCK_INDEX:
    case GL_UNIFORM_OFFSET:
    case GL_UNIFORM_ARRAY_STRIDE:
    case GL_UNIFORM_MATRIX_STRIDE:
        returnType = IntType;
        break;
    case GL_UNIFORM_IS_ROW_MAJOR:
        returnType = BoolType;
        break;
    default:
        synthesizeGLError(GL_INVALID_ENUM, "getActiveUniforms", "invalid parameter name");
        return ScriptValue::createNull(scriptState);
    }

    GLint activeUniforms = -1;
    contextGL()->GetProgramiv(objectOrZero(program), GL_ACTIVE_UNIFORMS, &activeUniforms);

    GLuint activeUniformsUnsigned = activeUniforms;
    size_t size = uniformIndices.size();
    for (size_t i = 0; i < size; ++i) {
        if (uniformIndices[i] >= activeUniformsUnsigned) {
            synthesizeGLError(GL_INVALID_VALUE, "getActiveUniforms", "uniform index greater than ACTIVE_UNIFORMS");
            return ScriptValue::createNull(scriptState);
        }
    }

    Vector<GLint> result(size);
    contextGL()->GetActiveUniformsiv(objectOrZero(program), uniformIndices.size(), uniformIndices.data(), pname, result.data());
    switch (returnType) {
    case EnumType:
        {
            Vector<GLenum> enumResult(size);
            for (size_t i = 0; i < size; ++i)
                enumResult[i] = static_cast<GLenum>(result[i]);
            return WebGLAny(scriptState, enumResult);
        }
    case UnsignedIntType:
        {
            Vector<GLuint> uintResult(size);
            for (size_t i = 0; i < size; ++i)
                uintResult[i] = static_cast<GLuint>(result[i]);
            return WebGLAny(scriptState, uintResult);
        }
    case IntType:
        {
            return WebGLAny(scriptState, result);
        }
    case BoolType:
        {
            Vector<bool> boolResult(size);
            for (size_t i = 0; i < size; ++i)
                boolResult[i] = static_cast<bool>(result[i]);
            return WebGLAny(scriptState, boolResult);
        }
    default:
        ASSERT_NOT_REACHED();
        return ScriptValue::createNull(scriptState);
    }
}

GLuint WebGL2RenderingContextBase::getUniformBlockIndex(WebGLProgram* program, const String& uniformBlockName)
{
    if (isContextLost() || !validateWebGLObject("getUniformBlockIndex", program))
        return 0;
    if (!validateString("getUniformBlockIndex", uniformBlockName))
        return 0;

    return contextGL()->GetUniformBlockIndex(objectOrZero(program), uniformBlockName.utf8().data());
}

bool WebGL2RenderingContextBase::validateUniformBlockIndex(const char* functionName, WebGLProgram* program, GLuint blockIndex)
{
    ASSERT(program);
    if (!program->linkStatus(this)) {
        synthesizeGLError(GL_INVALID_OPERATION, functionName, "program not linked");
        return false;
    }
    GLint activeUniformBlocks = 0;
    contextGL()->GetProgramiv(objectOrZero(program), GL_ACTIVE_UNIFORM_BLOCKS, &activeUniformBlocks);
    if (blockIndex >= static_cast<GLuint>(activeUniformBlocks)) {
        synthesizeGLError(GL_INVALID_VALUE, functionName, "invalid uniform block index");
        return false;
    }
    return true;
}

ScriptValue WebGL2RenderingContextBase::getActiveUniformBlockParameter(ScriptState* scriptState, WebGLProgram* program, GLuint uniformBlockIndex, GLenum pname)
{
    if (isContextLost() || !validateWebGLObject("getActiveUniformBlockParameter", program))
        return ScriptValue::createNull(scriptState);

    if (!validateUniformBlockIndex("getActiveUniformBlockParameter", program, uniformBlockIndex))
        return ScriptValue::createNull(scriptState);

    switch (pname) {
    case GL_UNIFORM_BLOCK_BINDING:
    case GL_UNIFORM_BLOCK_DATA_SIZE:
    case GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS:
        {
            GLint intValue = 0;
            contextGL()->GetActiveUniformBlockiv(objectOrZero(program), uniformBlockIndex, pname, &intValue);
            return WebGLAny(scriptState, static_cast<unsigned>(intValue));
        }
    case GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES:
        {
            GLint uniformCount = 0;
            contextGL()->GetActiveUniformBlockiv(objectOrZero(program), uniformBlockIndex, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &uniformCount);

            Vector<GLint> indices(uniformCount);
            contextGL()->GetActiveUniformBlockiv(objectOrZero(program), uniformBlockIndex, pname, indices.data());
            return WebGLAny(scriptState, DOMUint32Array::create(reinterpret_cast<GLuint*>(indices.data()), indices.size()));
        }
    case GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER:
    case GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER:
        {
            GLint boolValue = 0;
            contextGL()->GetActiveUniformBlockiv(objectOrZero(program), uniformBlockIndex, pname, &boolValue);
            return WebGLAny(scriptState, static_cast<bool>(boolValue));
        }
    default:
        synthesizeGLError(GL_INVALID_ENUM, "getActiveUniformBlockParameter", "invalid parameter name");
        return ScriptValue::createNull(scriptState);
    }
}

String WebGL2RenderingContextBase::getActiveUniformBlockName(WebGLProgram* program, GLuint uniformBlockIndex)
{
    if (isContextLost() || !validateWebGLObject("getActiveUniformBlockName", program))
        return String();

    if (!validateUniformBlockIndex("getActiveUniformBlockName", program, uniformBlockIndex))
        return String();

    GLint maxNameLength = -1;
    contextGL()->GetProgramiv(objectOrZero(program), GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH, &maxNameLength);
    if (maxNameLength <= 0) {
        // This state indicates that there are no active uniform blocks
        synthesizeGLError(GL_INVALID_VALUE, "getActiveUniformBlockName", "invalid uniform block index");
        return String();
    }
    std::unique_ptr<GLchar[]> name = wrapArrayUnique(new GLchar[maxNameLength]);

    GLsizei length = 0;
    contextGL()->GetActiveUniformBlockName(objectOrZero(program), uniformBlockIndex, maxNameLength, &length, name.get());

    return String(name.get(), length);
}

void WebGL2RenderingContextBase::uniformBlockBinding(WebGLProgram* program, GLuint uniformBlockIndex, GLuint uniformBlockBinding)
{
    if (isContextLost() || !validateWebGLObject("uniformBlockBinding", program))
        return;

    if (!validateUniformBlockIndex("uniformBlockBinding", program, uniformBlockIndex))
        return;

    contextGL()->UniformBlockBinding(objectOrZero(program), uniformBlockIndex, uniformBlockBinding);
}

WebGLVertexArrayObject* WebGL2RenderingContextBase::createVertexArray()
{
    if (isContextLost())
        return nullptr;

    WebGLVertexArrayObject* o = WebGLVertexArrayObject::create(this, WebGLVertexArrayObjectBase::VaoTypeUser);
    addContextObject(o);
    return o;
}

void WebGL2RenderingContextBase::deleteVertexArray(ScriptState* scriptState, WebGLVertexArrayObject* vertexArray)
{
    if (isContextLost() || !vertexArray)
        return;

    if (!vertexArray->isDefaultObject() && vertexArray == m_boundVertexArrayObject)
        setBoundVertexArrayObject(scriptState, nullptr);

    vertexArray->deleteObject(contextGL());
}

GLboolean WebGL2RenderingContextBase::isVertexArray(WebGLVertexArrayObject* vertexArray)
{
    if (isContextLost() || !vertexArray)
        return 0;

    if (!vertexArray->hasEverBeenBound())
        return 0;

    return contextGL()->IsVertexArrayOES(vertexArray->object());
}

void WebGL2RenderingContextBase::bindVertexArray(ScriptState* scriptState, WebGLVertexArrayObject* vertexArray)
{
    if (isContextLost())
        return;

    if (vertexArray && (vertexArray->isDeleted() || !vertexArray->validate(0, this))) {
        synthesizeGLError(GL_INVALID_OPERATION, "bindVertexArray", "invalid vertexArray");
        return;
    }

    if (vertexArray && !vertexArray->isDefaultObject() && vertexArray->object()) {
        contextGL()->BindVertexArrayOES(objectOrZero(vertexArray));

        vertexArray->setHasEverBeenBound();
        setBoundVertexArrayObject(scriptState, vertexArray);
    } else {
        contextGL()->BindVertexArrayOES(0);
        setBoundVertexArrayObject(scriptState, nullptr);
    }
}

void WebGL2RenderingContextBase::bindFramebuffer(ScriptState* scriptState, GLenum target, WebGLFramebuffer* buffer)
{
    bool deleted;
    if (!checkObjectToBeBound("bindFramebuffer", buffer, deleted))
        return;

    if (deleted)
        buffer = 0;

    switch (target) {
    case GL_DRAW_FRAMEBUFFER:
        break;
    case GL_FRAMEBUFFER:
    case GL_READ_FRAMEBUFFER:
        m_readFramebufferBinding = buffer;
        break;
    default:
        synthesizeGLError(GL_INVALID_ENUM, "bindFramebuffer", "invalid target");
        return;
    }

    setFramebuffer(target, buffer);
    if (scriptState) {
        preserveObjectWrapper(scriptState, this, V8HiddenValue::webglMisc(scriptState->isolate()), &m_miscWrappers, static_cast<uint32_t>(PreservedFramebuffer), buffer);
    }
}

void WebGL2RenderingContextBase::deleteFramebuffer(WebGLFramebuffer* framebuffer)
{
    if (!deleteObject(framebuffer))
        return;
    GLenum target = 0;
    if (framebuffer == m_framebufferBinding) {
        if (framebuffer == m_readFramebufferBinding) {
            target = GL_FRAMEBUFFER;
            m_framebufferBinding = nullptr;
            m_readFramebufferBinding = nullptr;
        } else {
            target = GL_DRAW_FRAMEBUFFER;
            m_framebufferBinding = nullptr;
        }
    } else if (framebuffer == m_readFramebufferBinding) {
        target = GL_READ_FRAMEBUFFER;
        m_readFramebufferBinding = nullptr;
    }
    if (target) {
        drawingBuffer()->setFramebufferBinding(target, 0);
        // Have to call drawingBuffer()->bind() here to bind back to internal fbo.
        drawingBuffer()->bind(target);
    }
}

ScriptValue WebGL2RenderingContextBase::getParameter(ScriptState* scriptState, GLenum pname)
{
    if (isContextLost())
        return ScriptValue::createNull(scriptState);
    switch (pname) {
    case GL_SHADING_LANGUAGE_VERSION: {
        return WebGLAny(scriptState, "WebGL GLSL ES 3.00 (" + String(contextGL()->GetString(GL_SHADING_LANGUAGE_VERSION)) + ")");
    }
    case GL_VERSION:
        return WebGLAny(scriptState, "WebGL 2.0 (" + String(contextGL()->GetString(GL_VERSION)) + ")");

    case GL_COPY_READ_BUFFER_BINDING:
        return WebGLAny(scriptState, m_boundCopyReadBuffer.get());
    case GL_COPY_WRITE_BUFFER_BINDING:
        return WebGLAny(scriptState, m_boundCopyWriteBuffer.get());
    case GL_DRAW_FRAMEBUFFER_BINDING:
        return WebGLAny(scriptState, m_framebufferBinding.get());
    case GL_FRAGMENT_SHADER_DERIVATIVE_HINT:
        return getUnsignedIntParameter(scriptState, pname);
    case GL_MAX_3D_TEXTURE_SIZE:
        return getIntParameter(scriptState, pname);
    case GL_MAX_ARRAY_TEXTURE_LAYERS:
        return getIntParameter(scriptState, pname);
    case GC3D_MAX_CLIENT_WAIT_TIMEOUT_WEBGL:
        return WebGLAny(scriptState, 0u);
    case GL_MAX_COLOR_ATTACHMENTS:
        return getIntParameter(scriptState, pname);
    case GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS:
        return getInt64Parameter(scriptState, pname);
    case GL_MAX_COMBINED_UNIFORM_BLOCKS:
        return getIntParameter(scriptState, pname);
    case GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS:
        return getInt64Parameter(scriptState, pname);
    case GL_MAX_DRAW_BUFFERS:
        return getIntParameter(scriptState, pname);
    case GL_MAX_ELEMENT_INDEX:
        return getInt64Parameter(scriptState, pname);
    case GL_MAX_ELEMENTS_INDICES:
        return getIntParameter(scriptState, pname);
    case GL_MAX_ELEMENTS_VERTICES:
        return getIntParameter(scriptState, pname);
    case GL_MAX_FRAGMENT_INPUT_COMPONENTS:
        return getIntParameter(scriptState, pname);
    case GL_MAX_FRAGMENT_UNIFORM_BLOCKS:
        return getIntParameter(scriptState, pname);
    case GL_MAX_FRAGMENT_UNIFORM_COMPONENTS:
        return getIntParameter(scriptState, pname);
    case GL_MAX_PROGRAM_TEXEL_OFFSET:
        return getIntParameter(scriptState, pname);
    case GL_MAX_SAMPLES:
        return getIntParameter(scriptState, pname);
    case GL_MAX_SERVER_WAIT_TIMEOUT:
        return getInt64Parameter(scriptState, pname);
    case GL_MAX_TEXTURE_LOD_BIAS:
        return getFloatParameter(scriptState, pname);
    case GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS:
        return getIntParameter(scriptState, pname);
    case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS:
        return getIntParameter(scriptState, pname);
    case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS:
        return getIntParameter(scriptState, pname);
    case GL_MAX_UNIFORM_BLOCK_SIZE:
        return getInt64Parameter(scriptState, pname);
    case GL_MAX_UNIFORM_BUFFER_BINDINGS:
        return getIntParameter(scriptState, pname);
    case GL_MAX_VARYING_COMPONENTS:
        return getIntParameter(scriptState, pname);
    case GL_MAX_VERTEX_OUTPUT_COMPONENTS:
        return getIntParameter(scriptState, pname);
    case GL_MAX_VERTEX_UNIFORM_BLOCKS:
        return getIntParameter(scriptState, pname);
    case GL_MAX_VERTEX_UNIFORM_COMPONENTS:
        return getIntParameter(scriptState, pname);
    case GL_MIN_PROGRAM_TEXEL_OFFSET:
        return getIntParameter(scriptState, pname);
    case GL_PACK_ROW_LENGTH:
        return getIntParameter(scriptState, pname);
    case GL_PACK_SKIP_PIXELS:
        return getIntParameter(scriptState, pname);
    case GL_PACK_SKIP_ROWS:
        return getIntParameter(scriptState, pname);
    case GL_PIXEL_PACK_BUFFER_BINDING:
        return WebGLAny(scriptState, m_boundPixelPackBuffer.get());
    case GL_PIXEL_UNPACK_BUFFER_BINDING:
        return WebGLAny(scriptState, m_boundPixelUnpackBuffer.get());
    case GL_RASTERIZER_DISCARD:
        return getBooleanParameter(scriptState, pname);
    case GL_READ_BUFFER:
        {
            GLenum value = 0;
            if (!isContextLost()) {
                WebGLFramebuffer* readFramebufferBinding = getFramebufferBinding(GL_READ_FRAMEBUFFER);
                if (!readFramebufferBinding)
                    value = m_readBufferOfDefaultFramebuffer;
                else
                    value = readFramebufferBinding->getReadBuffer();
            }
            return WebGLAny(scriptState, value);
        }
    case GL_READ_FRAMEBUFFER_BINDING:
        return WebGLAny(scriptState, m_readFramebufferBinding.get());
    case GL_SAMPLE_ALPHA_TO_COVERAGE:
        return getBooleanParameter(scriptState, pname);
    case GL_SAMPLE_COVERAGE:
        return getBooleanParameter(scriptState, pname);
    case GL_SAMPLER_BINDING:
        return WebGLAny(scriptState, m_samplerUnits[m_activeTextureUnit].get());
    case GL_TEXTURE_BINDING_2D_ARRAY:
        return WebGLAny(scriptState, m_textureUnits[m_activeTextureUnit].m_texture2DArrayBinding.get());
    case GL_TEXTURE_BINDING_3D:
        return WebGLAny(scriptState, m_textureUnits[m_activeTextureUnit].m_texture3DBinding.get());
    case GL_TRANSFORM_FEEDBACK_ACTIVE:
        return getBooleanParameter(scriptState, pname);
    case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
        return WebGLAny(scriptState, m_boundTransformFeedbackBuffer.get());
    case GL_TRANSFORM_FEEDBACK_BINDING:
        return WebGLAny(scriptState, m_transformFeedbackBinding.get());
    case GL_TRANSFORM_FEEDBACK_PAUSED:
        return getBooleanParameter(scriptState, pname);
    case GL_UNIFORM_BUFFER_BINDING:
        return WebGLAny(scriptState, m_boundUniformBuffer.get());
    case GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT:
        return getIntParameter(scriptState, pname);
    case GL_UNPACK_IMAGE_HEIGHT:
        return getIntParameter(scriptState, pname);
    case GL_UNPACK_ROW_LENGTH:
        return getIntParameter(scriptState, pname);
    case GL_UNPACK_SKIP_IMAGES:
        return getIntParameter(scriptState, pname);
    case GL_UNPACK_SKIP_PIXELS:
        return getIntParameter(scriptState, pname);
    case GL_UNPACK_SKIP_ROWS:
        return getIntParameter(scriptState, pname);

    default:
        return WebGLRenderingContextBase::getParameter(scriptState, pname);
    }
}

ScriptValue WebGL2RenderingContextBase::getInt64Parameter(ScriptState* scriptState, GLenum pname)
{
    GLint64 value = 0;
    if (!isContextLost())
        contextGL()->GetInteger64v(pname, &value);
    return WebGLAny(scriptState, value);
}

bool WebGL2RenderingContextBase::validateCapability(const char* functionName, GLenum cap)
{
    switch (cap) {
    case GL_RASTERIZER_DISCARD:
        return true;
    default:
        return WebGLRenderingContextBase::validateCapability(functionName, cap);
    }
}

bool WebGL2RenderingContextBase::isBufferBoundToTransformFeedback(WebGLBuffer* buffer)
{
    ASSERT(buffer);

    if (m_boundTransformFeedbackBuffer == buffer)
        return true;

    for (size_t i = 0; i < m_boundIndexedTransformFeedbackBuffers.size(); ++i) {
        if (m_boundIndexedTransformFeedbackBuffers[i] == buffer)
            return true;
    }

    return false;
}

bool WebGL2RenderingContextBase::isBufferBoundToNonTransformFeedback(WebGLBuffer* buffer)
{
    ASSERT(buffer);

    if (m_boundArrayBuffer == buffer
        || m_boundVertexArrayObject->boundElementArrayBuffer() == buffer
        || m_boundCopyReadBuffer == buffer
        || m_boundCopyWriteBuffer == buffer
        || m_boundPixelPackBuffer == buffer
        || m_boundPixelUnpackBuffer == buffer
        || m_boundUniformBuffer == buffer) {
        return true;
    }

    for (size_t i = 0; i <= m_maxBoundUniformBufferIndex; ++i) {
        if (m_boundIndexedUniformBuffers[i] == buffer)
            return true;
    }

    return false;
}

bool WebGL2RenderingContextBase::validateBufferTargetCompatibility(const char* functionName, GLenum target, WebGLBuffer* buffer)
{
    ASSERT(buffer);

    switch (buffer->getInitialTarget()) {
    case GL_ELEMENT_ARRAY_BUFFER:
        switch (target) {
        case GL_ARRAY_BUFFER:
        case GL_PIXEL_PACK_BUFFER:
        case GL_PIXEL_UNPACK_BUFFER:
        case GL_TRANSFORM_FEEDBACK_BUFFER:
        case GL_UNIFORM_BUFFER:
            synthesizeGLError(GL_INVALID_OPERATION, functionName,
                "element array buffers can not be bound to a different target");

            return false;
        default:
            break;
        }
        break;
    case GL_ARRAY_BUFFER:
    case GL_COPY_READ_BUFFER:
    case GL_COPY_WRITE_BUFFER:
    case GL_PIXEL_PACK_BUFFER:
    case GL_PIXEL_UNPACK_BUFFER:
    case GL_UNIFORM_BUFFER:
    case GL_TRANSFORM_FEEDBACK_BUFFER:
        if (target == GL_ELEMENT_ARRAY_BUFFER) {
            synthesizeGLError(GL_INVALID_OPERATION, functionName,
                "buffers bound to non ELEMENT_ARRAY_BUFFER targets can not be bound to ELEMENT_ARRAY_BUFFER target");
            return false;
        }
        break;
    default:
        break;
    }

    if (target == GL_TRANSFORM_FEEDBACK_BUFFER) {
        if (isBufferBoundToNonTransformFeedback(buffer)) {
            synthesizeGLError(GL_INVALID_OPERATION, functionName,
                "a buffer bound to TRANSFORM_FEEDBACK_BUFFER can not be bound to any other targets");
            return false;
        }
    } else if (isBufferBoundToTransformFeedback(buffer)) {
        synthesizeGLError(GL_INVALID_OPERATION, functionName,
            "a buffer bound to TRANSFORM_FEEDBACK_BUFFER can not be bound to any other targets");
        return false;
    }

    return true;
}

bool WebGL2RenderingContextBase::validateBufferTarget(const char* functionName, GLenum target)
{
    switch (target) {
    case GL_ARRAY_BUFFER:
    case GL_COPY_READ_BUFFER:
    case GL_COPY_WRITE_BUFFER:
    case GL_ELEMENT_ARRAY_BUFFER:
    case GL_PIXEL_PACK_BUFFER:
    case GL_PIXEL_UNPACK_BUFFER:
    case GL_TRANSFORM_FEEDBACK_BUFFER:
    case GL_UNIFORM_BUFFER:
        return true;
    default:
        synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid target");
        return false;
    }
}

bool WebGL2RenderingContextBase::validateAndUpdateBufferBindTarget(const char* functionName, GLenum target, WebGLBuffer* buffer)
{
    if (!validateBufferTarget(functionName, target))
        return false;

    if (buffer && !validateBufferTargetCompatibility(functionName, target, buffer))
        return false;

    switch (target) {
    case GL_ARRAY_BUFFER:
        m_boundArrayBuffer = buffer;
        break;
    case GL_COPY_READ_BUFFER:
        m_boundCopyReadBuffer = buffer;
        break;
    case GL_COPY_WRITE_BUFFER:
        m_boundCopyWriteBuffer = buffer;
        break;
    case GL_ELEMENT_ARRAY_BUFFER:
        m_boundVertexArrayObject->setElementArrayBuffer(buffer);
        break;
    case GL_PIXEL_PACK_BUFFER:
        m_boundPixelPackBuffer = buffer;
        break;
    case GL_PIXEL_UNPACK_BUFFER:
        m_boundPixelUnpackBuffer = buffer;
        break;
    case GL_TRANSFORM_FEEDBACK_BUFFER:
        m_boundTransformFeedbackBuffer = buffer;
        break;
    case GL_UNIFORM_BUFFER:
        m_boundUniformBuffer = buffer;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    if (buffer && !buffer->getInitialTarget())
        buffer->setInitialTarget(target);
    return true;
}

bool WebGL2RenderingContextBase::validateBufferBaseTarget(const char* functionName, GLenum target)
{
    switch (target) {
    case GL_TRANSFORM_FEEDBACK_BUFFER:
    case GL_UNIFORM_BUFFER:
        return true;
    default:
        synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid target");
        return false;
    }
}

bool WebGL2RenderingContextBase::validateAndUpdateBufferBindBaseTarget(const char* functionName, GLenum target, GLuint index, WebGLBuffer* buffer)
{
    if (!validateBufferBaseTarget(functionName, target))
        return false;

    if (buffer && !validateBufferTargetCompatibility(functionName, target, buffer))
        return false;

    switch (target) {
    case GL_TRANSFORM_FEEDBACK_BUFFER:
        if (index >= m_boundIndexedTransformFeedbackBuffers.size()) {
            synthesizeGLError(GL_INVALID_VALUE, functionName, "index out of range");
            return false;
        }
        m_boundIndexedTransformFeedbackBuffers[index] = buffer;
        m_boundTransformFeedbackBuffer = buffer;
        break;
    case GL_UNIFORM_BUFFER:
        if (index >= m_boundIndexedUniformBuffers.size()) {
            synthesizeGLError(GL_INVALID_VALUE, functionName, "index out of range");
            return false;
        }
        m_boundIndexedUniformBuffers[index] = buffer;
        m_boundUniformBuffer = buffer;

        // Keep track of what the maximum bound uniform buffer index is
        if (buffer) {
            if (index > m_maxBoundUniformBufferIndex)
                m_maxBoundUniformBufferIndex = index;
        } else if (m_maxBoundUniformBufferIndex > 0 && index == m_maxBoundUniformBufferIndex) {
            size_t i = m_maxBoundUniformBufferIndex - 1;
            for (; i > 0; --i) {
                if (m_boundIndexedUniformBuffers[i].get())
                    break;
            }
            m_maxBoundUniformBufferIndex = i;
        }
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    if (buffer && !buffer->getInitialTarget())
        buffer->setInitialTarget(target);
    return true;
}

bool WebGL2RenderingContextBase::validateFramebufferTarget(GLenum target)
{
    switch (target) {
    case GL_FRAMEBUFFER:
    case GL_READ_FRAMEBUFFER:
    case GL_DRAW_FRAMEBUFFER:
        return true;
    default:
        return false;
    }
}

bool WebGL2RenderingContextBase::validateReadPixelsFormatAndType(GLenum format, GLenum type, DOMArrayBufferView* buffer)
{
    switch (format) {
    case GL_RED:
    case GL_RED_INTEGER:
    case GL_RG:
    case GL_RG_INTEGER:
    case GL_RGB:
    case GL_RGB_INTEGER:
    case GL_RGBA:
    case GL_RGBA_INTEGER:
    case GL_LUMINANCE_ALPHA:
    case GL_LUMINANCE:
    case GL_ALPHA:
        break;
    default:
        synthesizeGLError(GL_INVALID_ENUM, "readPixels", "invalid format");
        return false;
    }

    switch (type) {
    case GL_UNSIGNED_BYTE:
        if (buffer && buffer->type() != DOMArrayBufferView::TypeUint8) {
            synthesizeGLError(GL_INVALID_OPERATION, "readPixels", "type UNSIGNED_BYTE but ArrayBufferView not Uint8Array");
            return false;
        }
        return true;
    case GL_BYTE:
        if (buffer && buffer->type() != DOMArrayBufferView::TypeInt8) {
            synthesizeGLError(GL_INVALID_OPERATION, "readPixels", "type BYTE but ArrayBufferView not Int8Array");
            return false;
        }
        return true;
    case GL_HALF_FLOAT:
        if (buffer && buffer->type() != DOMArrayBufferView::TypeUint16) {
            synthesizeGLError(GL_INVALID_OPERATION, "readPixels", "type HALF_FLOAT but ArrayBufferView not Uint16Array");
            return false;
        }
        return true;
    case GL_FLOAT:
        if (buffer && buffer->type() != DOMArrayBufferView::TypeFloat32) {
            synthesizeGLError(GL_INVALID_OPERATION, "readPixels", "type FLOAT but ArrayBufferView not Float32Array");
            return false;
        }
        return true;
    case GL_UNSIGNED_SHORT:
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_5_5_5_1:
        if (buffer && buffer->type() != DOMArrayBufferView::TypeUint16) {
            synthesizeGLError(GL_INVALID_OPERATION, "readPixels", "type UNSIGNED_SHORT but ArrayBufferView not Uint16Array");
            return false;
        }
        return true;
    case GL_SHORT:
        if (buffer && buffer->type() != DOMArrayBufferView::TypeInt16) {
            synthesizeGLError(GL_INVALID_OPERATION, "readPixels", "type SHORT but ArrayBufferView not Int16Array");
            return false;
        }
        return true;
    case GL_UNSIGNED_INT:
    case GL_UNSIGNED_INT_2_10_10_10_REV:
    case GL_UNSIGNED_INT_10F_11F_11F_REV:
    case GL_UNSIGNED_INT_5_9_9_9_REV:
        if (buffer && buffer->type() != DOMArrayBufferView::TypeUint32) {
            synthesizeGLError(GL_INVALID_OPERATION, "readPixels", "type UNSIGNED_INT but ArrayBufferView not Uint32Array");
            return false;
        }
        return true;
    case GL_INT:
        if (buffer && buffer->type() != DOMArrayBufferView::TypeInt32) {
            synthesizeGLError(GL_INVALID_OPERATION, "readPixels", "type INT but ArrayBufferView not Int32Array");
            return false;
        }
        return true;
    default:
        synthesizeGLError(GL_INVALID_ENUM, "readPixels", "invalid type");
        return false;
    }
}

WebGLFramebuffer* WebGL2RenderingContextBase::getFramebufferBinding(GLenum target)
{
    switch (target) {
    case GL_READ_FRAMEBUFFER:
        return m_readFramebufferBinding.get();
    case GL_DRAW_FRAMEBUFFER:
        return m_framebufferBinding.get();
    default:
        return WebGLRenderingContextBase::getFramebufferBinding(target);
    }
}

WebGLFramebuffer* WebGL2RenderingContextBase::getReadFramebufferBinding()
{
    return m_readFramebufferBinding.get();
}

bool WebGL2RenderingContextBase::validateGetFramebufferAttachmentParameterFunc(const char* functionName, GLenum target, GLenum attachment)
{
    if (!validateFramebufferTarget(target)) {
        synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid target");
        return false;
    }

    WebGLFramebuffer* framebufferBinding = getFramebufferBinding(target);
    ASSERT(framebufferBinding || drawingBuffer());
    if (!framebufferBinding) {
    // for the default framebuffer
        switch (attachment) {
        case GL_BACK:
        case GL_DEPTH:
        case GL_STENCIL:
            break;
        default:
            synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid attachment");
            return false;
        }
    } else {
    // for the FBO
        switch (attachment) {
        case GL_COLOR_ATTACHMENT0:
        case GL_DEPTH_ATTACHMENT:
        case GL_STENCIL_ATTACHMENT:
            break;
        case GL_DEPTH_STENCIL_ATTACHMENT:
            if (framebufferBinding->getAttachmentObject(GL_DEPTH_ATTACHMENT) != framebufferBinding->getAttachmentObject(GL_STENCIL_ATTACHMENT)) {
                synthesizeGLError(GL_INVALID_OPERATION, functionName, "different objects are bound to the depth and stencil attachment points");
                return false;
            }
            break;
        default:
            if (attachment > GL_COLOR_ATTACHMENT0
                && attachment < static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + maxColorAttachments()))
                break;
            synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid attachment");
            return false;
        }
    }
    return true;
}

ScriptValue WebGL2RenderingContextBase::getFramebufferAttachmentParameter(ScriptState* scriptState, GLenum target, GLenum attachment, GLenum pname)
{
    const char kFunctionName[] = "getFramebufferAttachmentParameter";
    if (isContextLost() || !validateGetFramebufferAttachmentParameterFunc(kFunctionName, target, attachment))
        return ScriptValue::createNull(scriptState);

    WebGLFramebuffer* framebufferBinding = getFramebufferBinding(target);
    ASSERT(!framebufferBinding || framebufferBinding->object());

    // Default framebuffer (an internal fbo)
    if (!framebufferBinding) {
        // We can use m_requestedAttribs because in WebGL 2, they are required to be honored.
        bool hasDepth = m_requestedAttributes.depth();
        bool hasStencil = m_requestedAttributes.stencil();
        bool hasAlpha = m_requestedAttributes.alpha();
        bool missingImage = (attachment == GL_DEPTH && !hasDepth)
            || (attachment == GL_STENCIL && !hasStencil);
        if (missingImage) {
            switch (pname) {
            case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
                return WebGLAny(scriptState, GL_NONE);
            default:
                synthesizeGLError(GL_INVALID_OPERATION, kFunctionName, "invalid parameter name");
                return ScriptValue::createNull(scriptState);
            }
        }
        switch (pname) {
        case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
            return WebGLAny(scriptState, GL_FRAMEBUFFER_DEFAULT);
        case GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE:
        case GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE:
        case GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE:
            {
                GLint value = attachment == GL_BACK ? 8 : 0;
                return WebGLAny(scriptState, value);
            }
        case GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE:
            {
                GLint value = (attachment == GL_BACK && hasAlpha) ? 8 : 0;
                return WebGLAny(scriptState, value);
            }
        case GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE:
            {
                // For ES3 capable backend, DEPTH24_STENCIL8 has to be supported.
                GLint value = attachment == GL_DEPTH ? 24 : 0;
                return WebGLAny(scriptState, value);
            }
        case GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE:
            {
                GLint value = attachment == GL_STENCIL ? 8 : 0;
                return WebGLAny(scriptState, value);
            }
        case GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE:
            return WebGLAny(scriptState, GL_UNSIGNED_NORMALIZED);
        case GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING:
            return WebGLAny(scriptState, GL_LINEAR);
        default:
            synthesizeGLError(GL_INVALID_ENUM, kFunctionName, "invalid parameter name");
            return ScriptValue::createNull(scriptState);
        }
    }

    WebGLSharedObject* attachmentObject = nullptr;
    if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
        WebGLSharedObject* depthAttachment = framebufferBinding->getAttachmentObject(GL_DEPTH_ATTACHMENT);
        WebGLSharedObject* stencilAttachment = framebufferBinding->getAttachmentObject(GL_STENCIL_ATTACHMENT);
        if (depthAttachment != stencilAttachment) {
            synthesizeGLError(GL_INVALID_OPERATION, kFunctionName,
                "different objects bound to DEPTH_ATTACHMENT and STENCIL_ATTACHMENT");
            return ScriptValue::createNull(scriptState);
        }
        attachmentObject = depthAttachment;
    } else {
        attachmentObject = framebufferBinding->getAttachmentObject(attachment);
    }

    if (!attachmentObject) {
        switch (pname) {
        case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
            return WebGLAny(scriptState, GL_NONE);
        case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
            return ScriptValue::createNull(scriptState);
        default:
            synthesizeGLError(GL_INVALID_OPERATION, kFunctionName, "invalid parameter name");
            return ScriptValue::createNull(scriptState);
        }
    }
    ASSERT(attachmentObject->isTexture() || attachmentObject->isRenderbuffer());

    switch (pname) {
    case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
        if (attachmentObject->isTexture())
            return WebGLAny(scriptState, GL_TEXTURE);
        return WebGLAny(scriptState, GL_RENDERBUFFER);
    case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
        return WebGLAny(scriptState, attachmentObject);
    case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
    case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER:
    case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
        if (!attachmentObject->isTexture())
            break;
    case GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE:
    case GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE:
    case GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE:
    case GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE:
    case GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE:
    case GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE:
        {
            GLint value = 0;
            contextGL()->GetFramebufferAttachmentParameteriv(target, attachment, pname, &value);
            return WebGLAny(scriptState, value);
        }
    case GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE:
        if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
            synthesizeGLError(GL_INVALID_OPERATION, kFunctionName,
                "COMPONENT_TYPE can't be queried for DEPTH_STENCIL_ATTACHMENT");
            return ScriptValue::createNull(scriptState);
        }
    case GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING:
        {
            GLint value = 0;
            contextGL()->GetFramebufferAttachmentParameteriv(target, attachment, pname, &value);
            return WebGLAny(scriptState, static_cast<unsigned>(value));
        }
    default:
        break;
    }
    synthesizeGLError(GL_INVALID_ENUM, kFunctionName, "invalid parameter name");
    return ScriptValue::createNull(scriptState);
}

DEFINE_TRACE(WebGL2RenderingContextBase)
{
    visitor->trace(m_readFramebufferBinding);
    visitor->trace(m_transformFeedbackBinding);
    visitor->trace(m_boundCopyReadBuffer);
    visitor->trace(m_boundCopyWriteBuffer);
    visitor->trace(m_boundPixelPackBuffer);
    visitor->trace(m_boundPixelUnpackBuffer);
    visitor->trace(m_boundTransformFeedbackBuffer);
    visitor->trace(m_boundUniformBuffer);
    visitor->trace(m_boundIndexedTransformFeedbackBuffers);
    visitor->trace(m_boundIndexedUniformBuffers);
    visitor->trace(m_currentBooleanOcclusionQuery);
    visitor->trace(m_currentTransformFeedbackPrimitivesWrittenQuery);
    visitor->trace(m_samplerUnits);
    WebGLRenderingContextBase::trace(visitor);
}

WebGLTexture* WebGL2RenderingContextBase::validateTexture3DBinding(const char* functionName, GLenum target)
{
    WebGLTexture* tex = nullptr;
    switch (target) {
    case GL_TEXTURE_2D_ARRAY:
        tex = m_textureUnits[m_activeTextureUnit].m_texture2DArrayBinding.get();
        break;
    case GL_TEXTURE_3D:
        tex = m_textureUnits[m_activeTextureUnit].m_texture3DBinding.get();
        break;
    default:
        synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid texture target");
        return nullptr;
    }
    if (!tex)
        synthesizeGLError(GL_INVALID_OPERATION, functionName, "no texture bound to target");
    return tex;
}

GLint WebGL2RenderingContextBase::getMaxTextureLevelForTarget(GLenum target)
{
    switch (target) {
    case GL_TEXTURE_3D:
        return m_max3DTextureLevel;
    case GL_TEXTURE_2D_ARRAY:
        return m_maxTextureLevel;
    }
    return WebGLRenderingContextBase::getMaxTextureLevelForTarget(target);
}

ScriptValue WebGL2RenderingContextBase::getTexParameter(ScriptState* scriptState, GLenum target, GLenum pname)
{
    if (isContextLost() || !validateTextureBinding("getTexParameter", target))
        return ScriptValue::createNull(scriptState);

    switch (pname) {
    case GL_TEXTURE_WRAP_R:
    case GL_TEXTURE_COMPARE_FUNC:
    case GL_TEXTURE_COMPARE_MODE:
    case GL_TEXTURE_IMMUTABLE_LEVELS:
        {
            GLint value = 0;
            contextGL()->GetTexParameteriv(target, pname, &value);
            return WebGLAny(scriptState, static_cast<unsigned>(value));
        }
    case GL_TEXTURE_IMMUTABLE_FORMAT:
        {
            GLint value = 0;
            contextGL()->GetTexParameteriv(target, pname, &value);
            return WebGLAny(scriptState, static_cast<bool>(value));
        }
    case GL_TEXTURE_BASE_LEVEL:
    case GL_TEXTURE_MAX_LEVEL:
        {
            GLint value = 0;
            contextGL()->GetTexParameteriv(target, pname, &value);
            return WebGLAny(scriptState, value);
        }
    case GL_TEXTURE_MAX_LOD:
    case GL_TEXTURE_MIN_LOD:
        {
            GLfloat value = 0.f;
            contextGL()->GetTexParameterfv(target, pname, &value);
            return WebGLAny(scriptState, value);
        }
    default:
        return WebGLRenderingContextBase::getTexParameter(scriptState, target, pname);
    }
}

WebGLBuffer* WebGL2RenderingContextBase::validateBufferDataTarget(const char* functionName, GLenum target)
{
    WebGLBuffer* buffer = nullptr;
    switch (target) {
    case GL_ELEMENT_ARRAY_BUFFER:
        buffer = m_boundVertexArrayObject->boundElementArrayBuffer();
        break;
    case GL_ARRAY_BUFFER:
        buffer = m_boundArrayBuffer.get();
        break;
    case GL_COPY_READ_BUFFER:
        buffer = m_boundCopyReadBuffer.get();
        break;
    case GL_COPY_WRITE_BUFFER:
        buffer = m_boundCopyWriteBuffer.get();
        break;
    case GL_PIXEL_PACK_BUFFER:
        buffer = m_boundPixelPackBuffer.get();
        break;
    case GL_PIXEL_UNPACK_BUFFER:
        buffer = m_boundPixelUnpackBuffer.get();
        break;
    case GL_TRANSFORM_FEEDBACK_BUFFER:
        buffer = m_boundTransformFeedbackBuffer.get();
        break;
    case GL_UNIFORM_BUFFER:
        buffer = m_boundUniformBuffer.get();
        break;
    default:
        synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid target");
        return nullptr;
    }
    if (!buffer) {
        synthesizeGLError(GL_INVALID_OPERATION, functionName, "no buffer");
        return nullptr;
    }
    return buffer;
}

bool WebGL2RenderingContextBase::validateBufferDataUsage(const char* functionName, GLenum usage)
{
    switch (usage) {
    case GL_STREAM_READ:
    case GL_STREAM_COPY:
    case GL_STATIC_READ:
    case GL_STATIC_COPY:
    case GL_DYNAMIC_READ:
    case GL_DYNAMIC_COPY:
        return true;
    default:
        return WebGLRenderingContextBase::validateBufferDataUsage(functionName, usage);
    }
}

void WebGL2RenderingContextBase::removeBoundBuffer(WebGLBuffer* buffer)
{
    if (m_boundCopyReadBuffer == buffer)
        m_boundCopyReadBuffer = nullptr;
    if (m_boundCopyWriteBuffer == buffer)
        m_boundCopyWriteBuffer = nullptr;
    if (m_boundPixelPackBuffer == buffer)
        m_boundPixelPackBuffer = nullptr;
    if (m_boundPixelUnpackBuffer == buffer)
        m_boundPixelUnpackBuffer = nullptr;
    if (m_boundTransformFeedbackBuffer == buffer)
        m_boundTransformFeedbackBuffer = nullptr;
    if (m_boundUniformBuffer == buffer)
        m_boundUniformBuffer = nullptr;

    WebGLRenderingContextBase::removeBoundBuffer(buffer);
}

void WebGL2RenderingContextBase::restoreCurrentFramebuffer()
{
    bindFramebuffer(nullptr, GL_DRAW_FRAMEBUFFER, m_framebufferBinding.get());
    bindFramebuffer(nullptr, GL_READ_FRAMEBUFFER, m_readFramebufferBinding.get());
}

WebGLImageConversion::PixelStoreParams WebGL2RenderingContextBase::getPackPixelStoreParams()
{
    WebGLImageConversion::PixelStoreParams params;
    params.alignment = m_packAlignment;
    params.rowLength = m_packRowLength;
    params.skipPixels = m_packSkipPixels;
    params.skipRows = m_packSkipRows;
    return params;
}

WebGLImageConversion::PixelStoreParams WebGL2RenderingContextBase::getUnpackPixelStoreParams(TexImageDimension dimension)
{
    WebGLImageConversion::PixelStoreParams params;
    params.alignment = m_unpackAlignment;
    params.rowLength = m_unpackRowLength;
    params.skipPixels = m_unpackSkipPixels;
    params.skipRows = m_unpackSkipRows;
    if (dimension == Tex3D) {
        params.imageHeight = m_unpackImageHeight;
        params.skipImages = m_unpackSkipImages;
    }
    return params;
}

} // namespace blink
