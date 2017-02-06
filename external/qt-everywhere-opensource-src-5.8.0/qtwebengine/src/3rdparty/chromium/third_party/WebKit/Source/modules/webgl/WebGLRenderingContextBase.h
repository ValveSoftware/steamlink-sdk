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

#ifndef WebGLRenderingContextBase_h
#define WebGLRenderingContextBase_h

#include "bindings/core/v8/Nullable.h"
#include "bindings/core/v8/ScopedPersistent.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/dom/DOMTypedArray.h"
#include "core/dom/TypedFlexibleArrayBufferView.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/layout/ContentChangeType.h"
#include "modules/webgl/WebGLContextAttributes.h"
#include "modules/webgl/WebGLExtensionName.h"
#include "modules/webgl/WebGLTexture.h"
#include "modules/webgl/WebGLVertexArrayObjectBase.h"
#include "platform/Timer.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/gpu/DrawingBuffer.h"
#include "platform/graphics/gpu/Extensions3DUtil.h"
#include "platform/graphics/gpu/WebGLImageConversion.h"
#include "public/platform/Platform.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "wtf/text/WTFString.h"
#include <memory>
#include <set>

namespace blink {
class WebLayer;
}

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace blink {

class ANGLEInstancedArrays;
class EXTBlendMinMax;
class EXTDisjointTimerQuery;
class EXTFragDepth;
class EXTShaderTextureLOD;
class EXTsRGB;
class EXTTextureFilterAnisotropic;
class ExceptionState;
class HTMLImageElement;
class HTMLVideoElement;
class ImageBitmap;
class ImageBuffer;
class ImageData;
class IntSize;
class OESElementIndexUint;
class OESStandardDerivatives;
class OESTextureFloat;
class OESTextureFloatLinear;
class OESTextureHalfFloat;
class OESTextureHalfFloatLinear;
class OESVertexArrayObject;
class WaitableEvent;
class WebGLActiveInfo;
class WebGLBuffer;
class WebGLCompressedTextureASTC;
class WebGLCompressedTextureATC;
class WebGLCompressedTextureETC1;
class WebGLCompressedTexturePVRTC;
class WebGLCompressedTextureS3TC;
class WebGLContextGroup;
class WebGLContextObject;
class WebGLDebugRendererInfo;
class WebGLDebugShaders;
class WebGLDepthTexture;
class WebGLDrawBuffers;
class WebGLExtension;
class WebGLFramebuffer;
class WebGLLoseContext;
class WebGLObject;
class WebGLProgram;
class WebGLRenderbuffer;
class WebGLShader;
class WebGLShaderPrecisionFormat;
class WebGLSharedObject;
class WebGLUniformLocation;
class WebGLVertexArrayObjectBase;

class WebGLRenderingContextLostCallback;
class WebGLRenderingContextErrorMessageCallback;

// This class uses the color mask to prevent drawing to the alpha channel, if
// the DrawingBuffer requires RGB emulation.
class ScopedRGBEmulationColorMask {
public:
    ScopedRGBEmulationColorMask(gpu::gles2::GLES2Interface*, GLboolean* colorMask, DrawingBuffer*);
    ~ScopedRGBEmulationColorMask();

private:
    gpu::gles2::GLES2Interface* m_contextGL;
    GLboolean m_colorMask[4];
    const bool m_requiresEmulation;
};

class MODULES_EXPORT WebGLRenderingContextBase : public CanvasRenderingContext {
public:
    ~WebGLRenderingContextBase() override;

    virtual unsigned version() const = 0;
    virtual String contextName() const = 0;
    virtual void registerContextExtensions() = 0;

    virtual void initializeNewContext();

    static unsigned getWebGLVersion(const CanvasRenderingContext*);

    static std::unique_ptr<WebGraphicsContext3DProvider> createWebGraphicsContext3DProvider(HTMLCanvasElement*, WebGLContextAttributes, unsigned webGLVersion);
    static std::unique_ptr<WebGraphicsContext3DProvider> createWebGraphicsContext3DProvider(ScriptState*, WebGLContextAttributes, unsigned webGLVersion);
    static void forceNextWebGLContextCreationToFail();

    int drawingBufferWidth() const;
    int drawingBufferHeight() const;

    void activeTexture(GLenum texture);
    void attachShader(ScriptState*, WebGLProgram*, WebGLShader*);
    void bindAttribLocation(WebGLProgram*, GLuint index, const String& name);
    void bindBuffer(ScriptState*, GLenum target, WebGLBuffer*);
    virtual void bindFramebuffer(ScriptState*, GLenum target, WebGLFramebuffer*);
    void bindRenderbuffer(ScriptState*, GLenum target, WebGLRenderbuffer*);
    void bindTexture(ScriptState*, GLenum target, WebGLTexture*);
    void blendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
    void blendEquation(GLenum mode);
    void blendEquationSeparate(GLenum modeRGB, GLenum modeAlpha);
    void blendFunc(GLenum sfactor, GLenum dfactor);
    void blendFuncSeparate(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);

    void bufferData(GLenum target, long long size, GLenum usage);
    void bufferData(GLenum target, DOMArrayBuffer* data, GLenum usage);
    void bufferData(GLenum target, DOMArrayBufferView* data, GLenum usage);
    void bufferSubData(GLenum target, long long offset, DOMArrayBuffer* data);
    void bufferSubData(GLenum target, long long offset, const FlexibleArrayBufferView& data);

    GLenum checkFramebufferStatus(GLenum target);
    void clear(GLbitfield mask);
    void clearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
    void clearDepth(GLfloat);
    void clearStencil(GLint);
    void colorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
    void compileShader(WebGLShader*);

    void compressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, DOMArrayBufferView* data);
    void compressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, DOMArrayBufferView* data);

    void copyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
    void copyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);

    WebGLBuffer* createBuffer();
    WebGLFramebuffer* createFramebuffer();
    WebGLProgram* createProgram();
    WebGLRenderbuffer* createRenderbuffer();
    WebGLShader* createShader(GLenum type);
    WebGLTexture* createTexture();

    void cullFace(GLenum mode);

    void deleteBuffer(WebGLBuffer*);
    virtual void deleteFramebuffer(WebGLFramebuffer*);
    void deleteProgram(WebGLProgram*);
    void deleteRenderbuffer(WebGLRenderbuffer*);
    void deleteShader(WebGLShader*);
    void deleteTexture(WebGLTexture*);

    void depthFunc(GLenum);
    void depthMask(GLboolean);
    void depthRange(GLfloat zNear, GLfloat zFar);
    void detachShader(ScriptState*, WebGLProgram*, WebGLShader*);
    void disable(GLenum cap);
    void disableVertexAttribArray(GLuint index);
    void drawArrays(GLenum mode, GLint first, GLsizei count);
    void drawElements(GLenum mode, GLsizei count, GLenum type, long long offset);

    void drawArraysInstancedANGLE(GLenum mode, GLint first, GLsizei count, GLsizei primcount);
    void drawElementsInstancedANGLE(GLenum mode, GLsizei count, GLenum type, long long offset, GLsizei primcount);

    void enable(GLenum cap);
    void enableVertexAttribArray(GLuint index);
    void finish();
    void flush();
    void framebufferRenderbuffer(ScriptState*, GLenum target, GLenum attachment, GLenum renderbuffertarget, WebGLRenderbuffer*);
    void framebufferTexture2D(ScriptState*, GLenum target, GLenum attachment, GLenum textarget, WebGLTexture*, GLint level);
    void frontFace(GLenum mode);
    void generateMipmap(GLenum target);

    WebGLActiveInfo* getActiveAttrib(WebGLProgram*, GLuint index);
    WebGLActiveInfo* getActiveUniform(WebGLProgram*, GLuint index);
    bool getAttachedShaders(WebGLProgram*, HeapVector<Member<WebGLShader>>&);
    Nullable<HeapVector<Member<WebGLShader>>> getAttachedShaders(WebGLProgram*);
    GLint getAttribLocation(WebGLProgram*, const String& name);
    ScriptValue getBufferParameter(ScriptState*, GLenum target, GLenum pname);
    void getContextAttributes(Nullable<WebGLContextAttributes>&);
    GLenum getError();
    ScriptValue getExtension(ScriptState*, const String& name);
    virtual ScriptValue getFramebufferAttachmentParameter(ScriptState*, GLenum target, GLenum attachment, GLenum pname);
    virtual ScriptValue getParameter(ScriptState*, GLenum pname);
    ScriptValue getProgramParameter(ScriptState*, WebGLProgram*, GLenum pname);
    String getProgramInfoLog(WebGLProgram*);
    ScriptValue getRenderbufferParameter(ScriptState*, GLenum target, GLenum pname);
    ScriptValue getShaderParameter(ScriptState*, WebGLShader*, GLenum pname);
    String getShaderInfoLog(WebGLShader*);
    WebGLShaderPrecisionFormat* getShaderPrecisionFormat(GLenum shaderType, GLenum precisionType);
    String getShaderSource(WebGLShader*);
    Nullable<Vector<String>> getSupportedExtensions();
    virtual ScriptValue getTexParameter(ScriptState*, GLenum target, GLenum pname);
    ScriptValue getUniform(ScriptState*, WebGLProgram*, const WebGLUniformLocation*);
    WebGLUniformLocation* getUniformLocation(WebGLProgram*, const String&);
    ScriptValue getVertexAttrib(ScriptState*, GLuint index, GLenum pname);
    long long getVertexAttribOffset(GLuint index, GLenum pname);

    void hint(GLenum target, GLenum mode);
    GLboolean isBuffer(WebGLBuffer*);
    bool isContextLost() const override;
    GLboolean isEnabled(GLenum cap);
    GLboolean isFramebuffer(WebGLFramebuffer*);
    GLboolean isProgram(WebGLProgram*);
    GLboolean isRenderbuffer(WebGLRenderbuffer*);
    GLboolean isShader(WebGLShader*);
    GLboolean isTexture(WebGLTexture*);

    void lineWidth(GLfloat);
    void linkProgram(WebGLProgram*);
    virtual void pixelStorei(GLenum pname, GLint param);
    void polygonOffset(GLfloat factor, GLfloat units);
    virtual void readPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, DOMArrayBufferView* pixels);
    void renderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
    void sampleCoverage(GLfloat value, GLboolean invert);
    void scissor(GLint x, GLint y, GLsizei width, GLsizei height);
    void shaderSource(WebGLShader*, const String&);
    void stencilFunc(GLenum func, GLint ref, GLuint mask);
    void stencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask);
    void stencilMask(GLuint);
    void stencilMaskSeparate(GLenum face, GLuint mask);
    void stencilOp(GLenum fail, GLenum zfail, GLenum zpass);
    void stencilOpSeparate(GLenum face, GLenum fail, GLenum zfail, GLenum zpass);

    void texImage2D(GLenum target, GLint level, GLint internalformat,
        GLsizei width, GLsizei height, GLint border,
        GLenum format, GLenum type, DOMArrayBufferView*);
    void texImage2D(GLenum target, GLint level, GLint internalformat,
        GLenum format, GLenum type, ImageData*);
    void texImage2D(GLenum target, GLint level, GLint internalformat,
        GLenum format, GLenum type, HTMLImageElement*, ExceptionState&);
    void texImage2D(GLenum target, GLint level, GLint internalformat,
        GLenum format, GLenum type, HTMLCanvasElement*, ExceptionState&);
    void texImage2D(GLenum target, GLint level, GLint internalformat,
        GLenum format, GLenum type, HTMLVideoElement*, ExceptionState&);
    void texImage2D(GLenum target, GLint level, GLint internalformat,
        GLenum format, GLenum type, ImageBitmap*, ExceptionState&);

    void texParameterf(GLenum target, GLenum pname, GLfloat param);
    void texParameteri(GLenum target, GLenum pname, GLint param);

    void texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
        GLsizei width, GLsizei height,
        GLenum format, GLenum type, DOMArrayBufferView*);
    void texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
        GLenum format, GLenum type, ImageData*);
    void texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
        GLenum format, GLenum type, HTMLImageElement*, ExceptionState&);
    void texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
        GLenum format, GLenum type, HTMLCanvasElement*, ExceptionState&);
    void texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
        GLenum format, GLenum type, HTMLVideoElement*, ExceptionState&);
    void texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
        GLenum format, GLenum type, ImageBitmap*, ExceptionState&);

    void uniform1f(const WebGLUniformLocation*, GLfloat x);
    void uniform1fv(const WebGLUniformLocation*, const FlexibleFloat32ArrayView&);
    void uniform1fv(const WebGLUniformLocation*, Vector<GLfloat>&);
    void uniform1i(const WebGLUniformLocation*, GLint x);
    void uniform1iv(const WebGLUniformLocation*, const FlexibleInt32ArrayView&);
    void uniform1iv(const WebGLUniformLocation*, Vector<GLint>&);
    void uniform2f(const WebGLUniformLocation*, GLfloat x, GLfloat y);
    void uniform2fv(const WebGLUniformLocation*, const FlexibleFloat32ArrayView&);
    void uniform2fv(const WebGLUniformLocation*, Vector<GLfloat>&);
    void uniform2i(const WebGLUniformLocation*, GLint x, GLint y);
    void uniform2iv(const WebGLUniformLocation*, const FlexibleInt32ArrayView&);
    void uniform2iv(const WebGLUniformLocation*, Vector<GLint>&);
    void uniform3f(const WebGLUniformLocation*, GLfloat x, GLfloat y, GLfloat z);
    void uniform3fv(const WebGLUniformLocation*, const FlexibleFloat32ArrayView&);
    void uniform3fv(const WebGLUniformLocation*, Vector<GLfloat>&);
    void uniform3i(const WebGLUniformLocation*, GLint x, GLint y, GLint z);
    void uniform3iv(const WebGLUniformLocation*, const FlexibleInt32ArrayView&);
    void uniform3iv(const WebGLUniformLocation*, Vector<GLint>&);
    void uniform4f(const WebGLUniformLocation*, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
    void uniform4fv(const WebGLUniformLocation*, const FlexibleFloat32ArrayView&);
    void uniform4fv(const WebGLUniformLocation*, Vector<GLfloat>&);
    void uniform4i(const WebGLUniformLocation*, GLint x, GLint y, GLint z, GLint w);
    void uniform4iv(const WebGLUniformLocation*, const FlexibleInt32ArrayView&);
    void uniform4iv(const WebGLUniformLocation*, Vector<GLint>&);
    void uniformMatrix2fv(const WebGLUniformLocation*, GLboolean transpose, DOMFloat32Array* value);
    void uniformMatrix2fv(const WebGLUniformLocation*, GLboolean transpose, Vector<GLfloat>& value);
    void uniformMatrix3fv(const WebGLUniformLocation*, GLboolean transpose, DOMFloat32Array* value);
    void uniformMatrix3fv(const WebGLUniformLocation*, GLboolean transpose, Vector<GLfloat>& value);
    void uniformMatrix4fv(const WebGLUniformLocation*, GLboolean transpose, DOMFloat32Array* value);
    void uniformMatrix4fv(const WebGLUniformLocation*, GLboolean transpose, Vector<GLfloat>& value);

    void useProgram(ScriptState*, WebGLProgram*);
    void validateProgram(WebGLProgram*);

    void vertexAttrib1f(GLuint index, GLfloat x);
    void vertexAttrib1fv(GLuint index, const DOMFloat32Array* values);
    void vertexAttrib1fv(GLuint index, const Vector<GLfloat>& values);
    void vertexAttrib2f(GLuint index, GLfloat x, GLfloat y);
    void vertexAttrib2fv(GLuint index, const DOMFloat32Array* values);
    void vertexAttrib2fv(GLuint index, const Vector<GLfloat>& values);
    void vertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z);
    void vertexAttrib3fv(GLuint index, const DOMFloat32Array* values);
    void vertexAttrib3fv(GLuint index, const Vector<GLfloat>& values);
    void vertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
    void vertexAttrib4fv(GLuint index, const DOMFloat32Array* values);
    void vertexAttrib4fv(GLuint index, const Vector<GLfloat>& values);
    void vertexAttribPointer(ScriptState*, GLuint index, GLint size, GLenum type, GLboolean normalized,
        GLsizei stride, long long offset);

    void vertexAttribDivisorANGLE(GLuint index, GLuint divisor);

    void viewport(GLint x, GLint y, GLsizei width, GLsizei height);

    // WEBGL_lose_context support
    enum AutoRecoveryMethod {
        // Don't restore automatically.
        Manual,

        // Restore when resources are available.
        WhenAvailable,

        // Restore as soon as possible, but only when
        // the canvas is visible.
        Auto
    };
    void loseContext(LostContextMode) override;
    void forceLostContext(LostContextMode, AutoRecoveryMethod);
    void forceRestoreContext();
    void loseContextImpl(LostContextMode, AutoRecoveryMethod);

    gpu::gles2::GLES2Interface* contextGL() const
    {
        DrawingBuffer* d = drawingBuffer();
        if (!d)
            return nullptr;
        return d->contextGL();
    }
    WebGLContextGroup* contextGroup() const { return m_contextGroup.get(); }
    Extensions3DUtil* extensionsUtil();

    void reshape(int width, int height) override;

    void markLayerComposited() override;
    ImageData* paintRenderingResultsToImageData(SourceDrawingBuffer) override;

    void removeSharedObject(WebGLSharedObject*);
    void removeContextObject(WebGLContextObject*);

    unsigned maxVertexAttribs() const { return m_maxVertexAttribs; }

    // Eagerly finalize WebGLRenderingContextBase in order for it
    // to (first) be able to detach its WebGLContextObjects, before
    // they're later swept and finalized.
    EAGERLY_FINALIZE();
    DECLARE_EAGER_FINALIZATION_OPERATOR_NEW();
    DECLARE_VIRTUAL_TRACE();

    // Returns approximate gpu memory allocated per pixel.
    int externallyAllocatedBytesPerPixel() override;

    class TextureUnitState {
        DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    public:
        Member<WebGLTexture> m_texture2DBinding;
        Member<WebGLTexture> m_textureCubeMapBinding;
        Member<WebGLTexture> m_texture3DBinding;
        Member<WebGLTexture> m_texture2DArrayBinding;

        DECLARE_TRACE();
    };

    void setFilterQuality(SkFilterQuality) override;
    bool isWebGL2OrHigher() { return version() >= 2; }

protected:
    friend class EXTDisjointTimerQuery;
    friend class WebGLDrawBuffers;
    friend class WebGLFramebuffer;
    friend class WebGLObject;
    friend class WebGLContextObject;
    friend class OESVertexArrayObject;
    friend class WebGLDebugShaders;
    friend class WebGLCompressedTextureASTC;
    friend class WebGLCompressedTextureATC;
    friend class WebGLCompressedTextureETC1;
    friend class WebGLCompressedTexturePVRTC;
    friend class WebGLCompressedTextureS3TC;
    friend class WebGLRenderingContextErrorMessageCallback;
    friend class WebGLVertexArrayObjectBase;
    friend class ScopedTexture2DRestorer;
    friend class ScopedFramebufferRestorer;

    WebGLRenderingContextBase(HTMLCanvasElement*, std::unique_ptr<WebGraphicsContext3DProvider>, const WebGLContextAttributes&);
    WebGLRenderingContextBase(OffscreenCanvas*, std::unique_ptr<WebGraphicsContext3DProvider>, const WebGLContextAttributes&);
    PassRefPtr<DrawingBuffer> createDrawingBuffer(std::unique_ptr<WebGraphicsContext3DProvider>);
    void setupFlags();

    // CanvasRenderingContext implementation.
    bool is3d() const override { return true; }
    bool isAccelerated() const override { return true; }
    void setIsHidden(bool) override;
    bool paintRenderingResultsToCanvas(SourceDrawingBuffer) override;
    WebLayer* platformLayer() const override;
    void stop() override;

    void addSharedObject(WebGLSharedObject*);
    void addContextObject(WebGLContextObject*);
    void detachAndRemoveAllObjects();

    void destroyContext();
    void markContextChanged(ContentChangeType);

    void onErrorMessage(const char*, int32_t id);

    void notifyCanvasContextChanged();

    // Query if depth_stencil buffer is supported.
    bool isDepthStencilSupported() { return m_isDepthStencilSupported; }

    // Helper to return the size in bytes of OpenGL data types
    // like GL_FLOAT, GL_INT, etc.
    unsigned sizeInBytes(GLenum type) const;

    // Check if each enabled vertex attribute is bound to a buffer.
    bool validateRenderingState(const char*);

    bool validateWebGLObject(const char*, WebGLObject*);

    // Adds a compressed texture format.
    void addCompressedTextureFormat(GLenum);
    void removeAllCompressedTextureFormats();

    // Set UNPACK_ALIGNMENT to 1, all other parameters to 0.
    virtual void resetUnpackParameters();
    // Restore the client unpack parameters.
    virtual void restoreUnpackParameters();

    PassRefPtr<Image> drawImageIntoBuffer(PassRefPtr<Image>, int width, int height, const char* functionName);

    PassRefPtr<Image> videoFrameToImage(HTMLVideoElement*);

    // Structure for rendering to a DrawingBuffer, instead of directly
    // to the back-buffer of m_context.
    RefPtr<DrawingBuffer> m_drawingBuffer;
    DrawingBuffer* drawingBuffer() const;

    RefPtr<WebGLContextGroup> m_contextGroup;

    bool m_isHidden;
    LostContextMode m_contextLostMode;
    AutoRecoveryMethod m_autoRecoveryMethod;
    // Dispatches a context lost event once it is determined that one is needed.
    // This is used for synthetic, WEBGL_lose_context and real context losses. For real ones, it's
    // likely that there's no JavaScript on the stack, but that might be dependent
    // on how exactly the platform discovers that the context was lost. For better
    // portability we always defer the dispatch of the event.
    Timer<WebGLRenderingContextBase> m_dispatchContextLostEventTimer;
    bool m_restoreAllowed;
    Timer<WebGLRenderingContextBase> m_restoreTimer;

    bool m_markedCanvasDirty;
    HeapHashSet<WeakMember<WebGLContextObject>> m_contextObjects;

    // List of bound VBO's. Used to maintain info about sizes for ARRAY_BUFFER and stored values for ELEMENT_ARRAY_BUFFER
    Member<WebGLBuffer> m_boundArrayBuffer;

    Member<WebGLVertexArrayObjectBase> m_defaultVertexArrayObject;
    Member<WebGLVertexArrayObjectBase> m_boundVertexArrayObject;
    bool m_preservedDefaultVAOObjectWrapper;
    void setBoundVertexArrayObject(ScriptState*, WebGLVertexArrayObjectBase*);

    enum VertexAttribValueType {
        Float32ArrayType,
        Int32ArrayType,
        Uint32ArrayType,
    };

    Vector<VertexAttribValueType> m_vertexAttribType;
    unsigned m_maxVertexAttribs;
    void setVertexAttribType(GLuint index, VertexAttribValueType);

    Member<WebGLProgram> m_currentProgram;
    Member<WebGLFramebuffer> m_framebufferBinding;
    Member<WebGLRenderbuffer> m_renderbufferBinding;

    HeapVector<TextureUnitState> m_textureUnits;
    unsigned long m_activeTextureUnit;

    Vector<GLenum> m_compressedTextureFormats;

    // Fixed-size cache of reusable image buffers for video texImage2D calls.
    class LRUImageBufferCache {
    public:
        LRUImageBufferCache(int capacity);
        // The pointer returned is owned by the image buffer map.
        ImageBuffer* imageBuffer(const IntSize&);
    private:
        void bubbleToFront(int idx);
        std::unique_ptr<std::unique_ptr<ImageBuffer>[]> m_buffers;
        int m_capacity;
    };
    LRUImageBufferCache m_generatedImageCache;

    GLint m_maxTextureSize;
    GLint m_maxCubeMapTextureSize;
    GLint m_max3DTextureSize;
    GLint m_maxArrayTextureLayers;
    GLint m_maxRenderbufferSize;
    GLint m_maxViewportDims[2];
    GLint m_maxTextureLevel;
    GLint m_maxCubeMapTextureLevel;
    GLint m_max3DTextureLevel;

    GLint m_maxDrawBuffers;
    GLint m_maxColorAttachments;
    GLenum m_backDrawBuffer;
    bool m_drawBuffersWebGLRequirementsChecked;
    bool m_drawBuffersSupported;

    GLenum m_readBufferOfDefaultFramebuffer;

    GLint m_packAlignment;
    GLint m_unpackAlignment;
    bool m_unpackFlipY;
    bool m_unpackPremultiplyAlpha;
    GLenum m_unpackColorspaceConversion;
    WebGLContextAttributes m_requestedAttributes;

    GLfloat m_clearColor[4];
    bool m_scissorEnabled;
    GLfloat m_clearDepth;
    GLint m_clearStencil;
    GLboolean m_colorMask[4];
    GLboolean m_depthMask;

    bool m_stencilEnabled;
    GLuint m_stencilMask, m_stencilMaskBack;
    GLint m_stencilFuncRef, m_stencilFuncRefBack; // Note that these are the user specified values, not the internal clamped value.
    GLuint m_stencilFuncMask, m_stencilFuncMaskBack;

    bool m_isDepthStencilSupported;

    bool m_synthesizedErrorsToConsole;
    int m_numGLErrorsToConsoleAllowed;

    unsigned long m_onePlusMaxNonDefaultTextureUnit;

    std::unique_ptr<Extensions3DUtil> m_extensionsUtil;

    enum ExtensionFlags {
        ApprovedExtension               = 0x00,
        // Extension that is behind the draft extensions runtime flag:
        DraftExtension                  = 0x01,
    };

    class ExtensionTracker : public GarbageCollected<ExtensionTracker> {
    public:
        ExtensionTracker(ExtensionFlags flags, const char* const* prefixes)
            : m_draft(flags & DraftExtension)
            , m_prefixes(prefixes)
        {
        }

        bool draft() const
        {
            return m_draft;
        }

        const char* const* prefixes() const;
        bool matchesNameWithPrefixes(const String&) const;

        virtual WebGLExtension* getExtension(WebGLRenderingContextBase*) = 0;
        virtual bool supported(WebGLRenderingContextBase*) const = 0;
        virtual const char* extensionName() const = 0;
        virtual void loseExtension(bool) = 0;

        DEFINE_INLINE_VIRTUAL_TRACE() { }

    private:
        bool m_draft;
        const char* const* m_prefixes;
    };

    template <typename T>
    class TypedExtensionTracker final : public ExtensionTracker {
    public:
        static TypedExtensionTracker<T>* create(Member<T>& extensionField, ExtensionFlags flags, const char* const* prefixes)
        {
            return new TypedExtensionTracker<T>(extensionField, flags, prefixes);
        }

        WebGLExtension* getExtension(WebGLRenderingContextBase* context) override
        {
            if (!m_extension) {
                m_extension = T::create(context);
                m_extensionField = m_extension;
            }

            return m_extension;
        }

        bool supported(WebGLRenderingContextBase* context) const override
        {
            return T::supported(context);
        }

        const char* extensionName() const override
        {
            return T::extensionName();
        }

        void loseExtension(bool force) override
        {
            if (m_extension) {
                m_extension->lose(force);
                if (m_extension->isLost())
                    m_extension = nullptr;
            }
        }

        DEFINE_INLINE_VIRTUAL_TRACE()
        {
            visitor->trace(m_extension);
            ExtensionTracker::trace(visitor);
        }

    private:
        TypedExtensionTracker(Member<T>& extensionField, ExtensionFlags flags, const char* const* prefixes)
            : ExtensionTracker(flags, prefixes)
            , m_extensionField(extensionField)
        {
        }

        GC_PLUGIN_IGNORE("http://crbug.com/519953")
        Member<T>& m_extensionField;
        // ExtensionTracker holds it's own reference to the extension to ensure
        // that it is not deleted before this object's destructor is called
        Member<T> m_extension;
    };

    bool m_extensionEnabled[WebGLExtensionNameCount];
    HeapVector<Member<ExtensionTracker>> m_extensions;

    template <typename T>
    void registerExtension(Member<T>& extensionPtr, ExtensionFlags flags = ApprovedExtension, const char* const* prefixes = nullptr)
    {
        m_extensions.append(TypedExtensionTracker<T>::create(extensionPtr, flags, prefixes));
    }

    bool extensionSupportedAndAllowed(const ExtensionTracker*);

    inline bool extensionEnabled(WebGLExtensionName name)
    {
        return m_extensionEnabled[name];
    }

    // ScopedDrawingBufferBinder is used for ReadPixels/CopyTexImage2D/CopySubImage2D to read from
    // a multisampled DrawingBuffer. In this situation, we need to blit to a single sampled buffer
    // for reading, during which the bindings could be changed and need to be recovered.
    class ScopedDrawingBufferBinder {
        STACK_ALLOCATED();
    public:
        ScopedDrawingBufferBinder(DrawingBuffer* drawingBuffer, WebGLFramebuffer* framebufferBinding)
            : m_drawingBuffer(drawingBuffer)
            , m_readFramebufferBinding(framebufferBinding)
        {
            // Commit DrawingBuffer if needed (e.g., for multisampling)
            if (!m_readFramebufferBinding && m_drawingBuffer)
                m_drawingBuffer->commit();
        }

        ~ScopedDrawingBufferBinder()
        {
            // Restore DrawingBuffer if needed
            if (!m_readFramebufferBinding && m_drawingBuffer)
                m_drawingBuffer->restoreFramebufferBindings();
        }

    private:
        DrawingBuffer* m_drawingBuffer;
        Member<WebGLFramebuffer> m_readFramebufferBinding;
    };

    // Errors raised by synthesizeGLError() while the context is lost.
    Vector<GLenum> m_lostContextErrors;
    // Other errors raised by synthesizeGLError().
    Vector<GLenum> m_syntheticErrors;

    bool m_isWebGL2FormatsTypesAdded;
    bool m_isWebGL2InternalFormatsCopyTexImageAdded;
    bool m_isOESTextureFloatFormatsTypesAdded;
    bool m_isOESTextureHalfFloatFormatsTypesAdded;
    bool m_isWebGLDepthTextureFormatsTypesAdded;
    bool m_isEXTsRGBFormatsTypesAdded;

    std::set<GLenum> m_supportedInternalFormats;
    std::set<GLenum> m_supportedInternalFormatsCopyTexImage;
    std::set<GLenum> m_supportedFormats;
    std::set<GLenum> m_supportedTypes;

    // Helpers for getParameter and others
    ScriptValue getBooleanParameter(ScriptState*, GLenum);
    ScriptValue getBooleanArrayParameter(ScriptState*, GLenum);
    ScriptValue getFloatParameter(ScriptState*, GLenum);
    ScriptValue getIntParameter(ScriptState*, GLenum);
    ScriptValue getInt64Parameter(ScriptState*, GLenum);
    ScriptValue getUnsignedIntParameter(ScriptState*, GLenum);
    ScriptValue getWebGLFloatArrayParameter(ScriptState*, GLenum);
    ScriptValue getWebGLIntArrayParameter(ScriptState*, GLenum);

    // Clear the backbuffer if it was composited since the last operation.
    // clearMask is set to the bitfield of any clear that would happen anyway at this time
    // and the function returns |CombinedClear| if that clear is now unnecessary.
    enum HowToClear {
        // Skip clearing the backbuffer.
        Skipped,
        // Clear the backbuffer.
        JustClear,
        // Combine webgl.clear() API with the backbuffer clear, so webgl.clear()
        // doesn't have to call glClear() again.
        CombinedClear
    };
    HowToClear clearIfComposited(GLbitfield clearMask = 0);

    // Helper to restore state that clearing the framebuffer may destroy.
    void restoreStateAfterClear();

    // Convert texture internal format.
    GLenum convertTexInternalFormat(GLenum internalformat, GLenum type);

    enum TexImageFunctionType {
        TexImage,
        TexSubImage,
        CopyTexImage,
        CompressedTexImage
    };
    enum TexImageFunctionID {
        TexImage2D,
        TexSubImage2D,
        TexImage3D,
        TexSubImage3D
    };
    enum TexImageByGPUType {
        TexImage2DByGPU,
        TexSubImage2DByGPU,
        TexSubImage3DByGPU
    };
    enum TexImageDimension {
        Tex2D,
        Tex3D
    };
    void texImage2DBase(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels);
    void texImageImpl(TexImageFunctionID, GLenum target, GLint level, GLint internalformat, GLint xoffset, GLint yoffset, GLint zoffset,
        GLenum format, GLenum type, Image*, WebGLImageConversion::ImageHtmlDomSource, bool flipY, bool premultiplyAlpha);

    // Copy from the source directly to the texture via the gpu, without a read-back to system memory.
    // Souce could be canvas or imageBitmap.
    void texImageByGPU(TexImageByGPUType, WebGLTexture*, GLenum target, GLint level, GLint internalformat,
        GLenum type, GLint xoffset, GLint yoffset, GLint zoffset, CanvasImageSource*);
    virtual bool canUseTexImageByGPU(TexImageFunctionID, GLint internalformat, GLenum type);

    virtual WebGLImageConversion::PixelStoreParams getPackPixelStoreParams();
    virtual WebGLImageConversion::PixelStoreParams getUnpackPixelStoreParams(TexImageDimension);

    // Helper function for copyTex{Sub}Image, check whether the internalformat
    // and the color buffer format of the current bound framebuffer combination
    // is valid.
    bool isTexInternalFormatColorBufferCombinationValid(GLenum texInternalFormat, GLenum colorBufferFormat);

    // Helper function to verify limits on the length of uniform and attribute locations.
    virtual unsigned getMaxWebGLLocationLength() const { return 256; }
    bool validateLocationLength(const char* functionName, const String&);

    // Helper function to check if size is non-negative.
    // Generate GL error and return false for negative inputs; otherwise, return true.
    bool validateSize(const char* functionName, GLint x, GLint y, GLint z = 0);

    // Helper function to check if all characters in the string belong to the
    // ASCII subset as defined in GLSL ES 1.0 spec section 3.1.
    bool validateString(const char* functionName, const String&);

    // Helper function to check texture binding target and texture bound to the target.
    // Generate GL errors and return 0 if target is invalid or texture bound is
    // null.  Otherwise, return the texture bound to the target.
    WebGLTexture* validateTextureBinding(const char* functionName, GLenum target);

    // Wrapper function for validateTexture2D(3D)Binding, used in texImageHelper functions.
    virtual WebGLTexture* validateTexImageBinding(const char*, TexImageFunctionID, GLenum);

    // Helper function to check texture 2D target and texture bound to the target.
    // Generate GL errors and return 0 if target is invalid or texture bound is
    // null.  Otherwise, return the texture bound to the target.
    WebGLTexture* validateTexture2DBinding(const char* functionName, GLenum target);

    // Helper function to check input internalformat/format/type for functions {copy}Tex{Sub}Image.
    // Generates GL error and returns false if parameters are invalid.
    bool validateTexFuncFormatAndType(const char* functionName, TexImageFunctionType, GLenum internalformat, GLenum format, GLenum type, GLint level);

    // Helper function to check readbuffer validity for copyTex{Sub}Image.
    // If yes, obtains the bound read framebuffer, returns true.
    // If not, generates a GL error, returns false.
    bool validateReadBufferAndGetInfo(const char* functionName, WebGLFramebuffer*& readFramebufferBinding);

    // Helper function to check format/type and ArrayBuffer view type for readPixels.
    // Generates INVALID_ENUM and returns false if parameters are invalid.
    // Generates INVALID_OPERATION if ArrayBuffer view type is incompatible with type.
    virtual bool validateReadPixelsFormatAndType(GLenum format, GLenum type, DOMArrayBufferView*);

    // Helper function to check parameters of readPixels. Returns true if all parameters
    // are valid. Otherwise, generates appropriate error and returns false.
    bool validateReadPixelsFuncParameters(GLsizei width, GLsizei height, GLenum format, GLenum type, DOMArrayBufferView*, long long bufferSize);

    virtual GLint getMaxTextureLevelForTarget(GLenum target);

    // Helper function to check input level for functions {copy}Tex{Sub}Image.
    // Generates GL error and returns false if level is invalid.
    bool validateTexFuncLevel(const char* functionName, GLenum target, GLint level);

    // Helper function to check if a 64-bit value is non-negative and can fit into a 32-bit integer.
    // Generates GL error and returns false if not.
    bool validateValueFitNonNegInt32(const char* functionName, const char* paramName, long long value);

    enum TexFuncValidationSourceType {
        SourceArrayBufferView,
        SourceImageData,
        SourceHTMLImageElement,
        SourceHTMLCanvasElement,
        SourceHTMLVideoElement,
        SourceImageBitmap,
        SourceUnpackBuffer,
    };

    // Helper function for tex{Sub}Image{2|3}D to check if the input format/type/level/target/width/height/depth/border/xoffset/yoffset/zoffset are valid.
    // Otherwise, it would return quickly without doing other work.
    bool validateTexFunc(const char* functionName, TexImageFunctionType, TexFuncValidationSourceType, GLenum target, GLint level, GLenum internalformat, GLsizei width,
        GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, GLint xoffset, GLint yoffset, GLint zoffset);

    // Helper function to check input width and height for functions {copy, compressed}Tex{Sub}Image.
    // Generates GL error and returns false if width or height is invalid.
    bool validateTexFuncDimensions(const char* functionName, TexImageFunctionType, GLenum target, GLint level, GLsizei width, GLsizei height, GLsizei depth);

    // Helper function to check input parameters for functions {copy}Tex{Sub}Image.
    // Generates GL error and returns false if parameters are invalid.
    bool validateTexFuncParameters(const char* functionName, TexImageFunctionType, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type);

    enum NullDisposition {
        NullAllowed,
        NullNotAllowed
    };

    // Helper function to validate that the given ArrayBufferView
    // is of the correct type and contains enough data for the texImage call.
    // Generates GL error and returns false if parameters are invalid.
    bool validateTexFuncData(const char* functionName, TexImageDimension, GLint level, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, DOMArrayBufferView* pixels, NullDisposition);

    // Helper function to validate a given texture format is settable as in
    // you can supply data to texImage2D, or call texImage2D, copyTexImage2D and
    // copyTexSubImage2D.
    // Generates GL error and returns false if the format is not settable.
    bool validateSettableTexFormat(const char* functionName, GLenum format);

    // Helper function to validate format for CopyTexImage.
    bool validateCopyTexFormat(const char* functionName, GLenum format);

    // Helper function for validating compressed texture formats.
    bool validateCompressedTexFormat(const char* functionName, GLenum format);

    // Helper function to validate if front/back stencilMask and stencilFunc settings are the same.
    bool validateStencilSettings(const char* functionName);

    // Helper function to validate stencil or depth func.
    bool validateStencilOrDepthFunc(const char* functionName, GLenum);

    // Helper function for texParameterf and texParameteri.
    void texParameter(GLenum target, GLenum pname, GLfloat paramf, GLint parami, bool isFloat);

    // Helper function to print GL errors to console.
    void printGLErrorToConsole(const String&);

    // Helper function to print warnings to console. Currently
    // used only to warn about use of obsolete functions.
    void printWarningToConsole(const String&);

    // Helper function to validate the target for checkFramebufferStatus and validateFramebufferFuncParameters.
    virtual bool validateFramebufferTarget(GLenum target);

    // Get the framebuffer bound to given target
    virtual WebGLFramebuffer* getFramebufferBinding(GLenum target);

    virtual WebGLFramebuffer* getReadFramebufferBinding();

    // Helper function to validate input parameters for framebuffer functions.
    // Generate GL error if parameters are illegal.
    bool validateFramebufferFuncParameters(const char* functionName, GLenum target, GLenum attachment);

    // Helper function to validate blend equation mode.
    bool validateBlendEquation(const char* functionName, GLenum);

    // Helper function to validate blend func factors.
    bool validateBlendFuncFactors(const char* functionName, GLenum src, GLenum dst);

    // Helper function to validate a GL capability.
    virtual bool validateCapability(const char* functionName, GLenum);

    // Helper function to validate input parameters for uniform functions.
    bool validateUniformParameters(const char* functionName, const WebGLUniformLocation*, DOMFloat32Array*, GLsizei mod);
    bool validateUniformParameters(const char* functionName, const WebGLUniformLocation*, DOMInt32Array*, GLsizei mod);
    bool validateUniformParameters(const char* functionName, const WebGLUniformLocation*, void*, GLsizei, GLsizei mod);
    bool validateUniformMatrixParameters(const char* functionName, const WebGLUniformLocation*, GLboolean transpose, DOMFloat32Array*, GLsizei mod);
    bool validateUniformMatrixParameters(const char* functionName, const WebGLUniformLocation*, GLboolean transpose, void*, GLsizei, GLsizei mod);

    template<typename WTFTypedArray>
    bool validateUniformParameters(const char* functionName, const WebGLUniformLocation* location, const TypedFlexibleArrayBufferView<WTFTypedArray>& v, GLsizei requiredMinSize)
    {
        if (!v.dataMaybeOnStack()) {
            synthesizeGLError(GL_INVALID_VALUE, functionName, "no array");
            return false;
        }
        return validateUniformMatrixParameters(functionName, location, false, v.dataMaybeOnStack(), v.length(), requiredMinSize);
    }

    // Helper function to validate the target for bufferData and getBufferParameter.
    virtual bool validateBufferTarget(const char* functionName, GLenum target);

    // Helper function to validate the target for bufferData.
    // Return the current bound buffer to target, or 0 if the target is invalid.
    virtual WebGLBuffer* validateBufferDataTarget(const char* functionName, GLenum target);
    // Helper function to validate the usage for bufferData.
    virtual bool validateBufferDataUsage(const char* functionName, GLenum usage);

    virtual bool validateAndUpdateBufferBindTarget(const char* functionName, GLenum target, WebGLBuffer*);

    virtual void removeBoundBuffer(WebGLBuffer*);

    // Helper function for tex{Sub}Image2D to make sure image is ready and wouldn't taint Origin.
    bool validateHTMLImageElement(const char* functionName, HTMLImageElement*, ExceptionState&);

    // Helper function for tex{Sub}Image2D to make sure canvas is ready and wouldn't taint Origin.
    bool validateHTMLCanvasElement(const char* functionName, HTMLCanvasElement*, ExceptionState&);

    // Helper function for tex{Sub}Image2D to make sure video is ready wouldn't taint Origin.
    bool validateHTMLVideoElement(const char* functionName, HTMLVideoElement*, ExceptionState&);

    // Helper function for tex{Sub}Image2D to make sure imagebitmap is ready and wouldn't taint Origin.
    bool validateImageBitmap(const char* functionName, ImageBitmap*, ExceptionState&);

    // Helper function to validate drawArrays(Instanced) calls
    bool validateDrawArrays(const char* functionName);

    // Helper function to validate drawElements(Instanced) calls
    bool validateDrawElements(const char* functionName, GLenum type, long long offset);

    // Helper functions to bufferData() and bufferSubData().
    void bufferDataImpl(GLenum target, long long size, const void* data, GLenum usage);
    void bufferSubDataImpl(GLenum target, long long offset, GLsizeiptr, const void* data);

    // Helper function for delete* (deleteBuffer, deleteProgram, etc) functions.
    // Return false if caller should return without further processing.
    bool deleteObject(WebGLObject*);

    // Helper function for bind* (bindBuffer, bindTexture, etc) and useProgram.
    // If the object has already been deleted, set deleted to true upon return.
    // Return false if caller should return without further processing.
    bool checkObjectToBeBound(const char* functionName, WebGLObject*, bool& deleted);

    void dispatchContextLostEvent(Timer<WebGLRenderingContextBase>*);
    // Helper for restoration after context lost.
    void maybeRestoreContext(Timer<WebGLRenderingContextBase>*);

    enum ConsoleDisplayPreference {
        DisplayInConsole,
        DontDisplayInConsole
    };

    // Reports an error to glGetError, sends a message to the JavaScript
    // console.
    void synthesizeGLError(GLenum, const char* functionName, const char* description, ConsoleDisplayPreference = DisplayInConsole);
    void emitGLWarning(const char* function, const char* reason);

    String ensureNotNull(const String&) const;

    // Enable or disable stencil test based on user setting and
    // whether the current FBO has a stencil buffer.
    void applyStencilTest();

    // Helper for enabling or disabling a capability.
    void enableOrDisable(GLenum capability, bool enable);

    // Clamp the width and height to GL_MAX_VIEWPORT_DIMS.
    IntSize clampedCanvasSize();

    // First time called, if EXT_draw_buffers is supported, query the value; otherwise return 0.
    // Later, return the cached value.
    GLint maxDrawBuffers();
    GLint maxColorAttachments();

    void setBackDrawBuffer(GLenum);
    void setFramebuffer(GLenum, WebGLFramebuffer*);

    virtual void restoreCurrentFramebuffer();
    void restoreCurrentTexture2D();

    void findNewMaxNonDefaultTextureUnit();

    virtual void renderbufferStorageImpl(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, const char* functionName);

    // Ensures that the JavaScript wrappers for objects that are
    // latched into the context's state, or which are implicitly
    // linked together (like programs and their attached shaders), are
    // not garbage collected before they should be.
    ScopedPersistent<v8::Array> m_2DTextureWrappers;
    ScopedPersistent<v8::Array> m_2DArrayTextureWrappers;
    ScopedPersistent<v8::Array> m_3DTextureWrappers;
    ScopedPersistent<v8::Array> m_cubeMapTextureWrappers;
    ScopedPersistent<v8::Array> m_extensionWrappers;

    // The "catch-all" array for the rest of the preserved object
    // wrappers. The enum below defines how the indices in this array
    // are used.
    enum PreservedWrapperIndex {
        PreservedArrayBuffer,
        PreservedElementArrayBuffer,
        PreservedFramebuffer,
        PreservedProgram,
        PreservedRenderbuffer,
        PreservedDefaultVAO,
        PreservedVAO,
        PreservedTransformFeedback,
    };
    ScopedPersistent<v8::Array> m_miscWrappers;

    static void preserveObjectWrapper(ScriptState*, ScriptWrappable* sourceObject, v8::Local<v8::String> hiddenValueName, ScopedPersistent<v8::Array>* persistentCache, uint32_t index, ScriptWrappable* targetObject);

    // Called to lazily instantiate the wrapper for the default VAO
    // during calls to bindBuffer and vertexAttribPointer (from
    // JavaScript).
    void maybePreserveDefaultVAOObjectWrapper(ScriptState*);

    friend class WebGLStateRestorer;
    friend class WebGLRenderingContextEvictionManager;

    static void activateContext(WebGLRenderingContextBase*);
    static void deactivateContext(WebGLRenderingContextBase*);
    static void addToEvictedList(WebGLRenderingContextBase*);
    static void removeFromEvictedList(WebGLRenderingContextBase*);
    static void willDestroyContext(WebGLRenderingContextBase*);
    static void forciblyLoseOldestContext(const String& reason);
    // Return the least recently used context's position in the active context vector.
    // If the vector is empty, return the maximum allowed active context number.
    static WebGLRenderingContextBase* oldestContext();
    static WebGLRenderingContextBase* oldestEvictedContext();

    ImageBitmap* transferToImageBitmapBase();

    // Helper functions for tex(Sub)Image2D && texSubImage3D
    void texImageHelperDOMArrayBufferView(TexImageFunctionID, GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, GLsizei, GLint, GLint, GLint, DOMArrayBufferView*);
    void texImageHelperImageData(TexImageFunctionID, GLenum, GLint, GLint, GLint, GLenum, GLenum, GLsizei, GLint, GLint, GLint, ImageData*);
    void texImageHelperHTMLImageElement(TexImageFunctionID, GLenum, GLint, GLint, GLenum, GLenum, GLint, GLint, GLint, HTMLImageElement*, ExceptionState&);
    void texImageHelperHTMLCanvasElement(TexImageFunctionID, GLenum, GLint, GLint, GLenum, GLenum, GLint, GLint, GLint, HTMLCanvasElement*, ExceptionState&);
    void texImageHelperHTMLVideoElement(TexImageFunctionID, GLenum, GLint, GLint, GLenum, GLenum, GLint, GLint, GLint, HTMLVideoElement*, ExceptionState&);
    void texImageHelperImageBitmap(TexImageFunctionID, GLenum, GLint, GLint, GLenum, GLenum, GLint, GLint, GLint, ImageBitmap*, ExceptionState&);
    static const char* getTexImageFunctionName(TexImageFunctionID);

private:
    WebGLRenderingContextBase(HTMLCanvasElement*, OffscreenCanvas*, std::unique_ptr<WebGraphicsContext3DProvider>, const WebGLContextAttributes&);
    static std::unique_ptr<WebGraphicsContext3DProvider> createContextProviderInternal(HTMLCanvasElement*, ScriptState*, WebGLContextAttributes, unsigned);
    void texImageCanvasByGPU(HTMLCanvasElement*, GLuint, GLenum, GLenum, GLint);
    void texImageBitmapByGPU(ImageBitmap*, GLuint, GLenum, GLenum, GLint, bool);
};

DEFINE_TYPE_CASTS(WebGLRenderingContextBase, CanvasRenderingContext, context, context->is3d(), context.is3d());

} // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::WebGLRenderingContextBase::TextureUnitState);

#endif // WebGLRenderingContextBase_h
