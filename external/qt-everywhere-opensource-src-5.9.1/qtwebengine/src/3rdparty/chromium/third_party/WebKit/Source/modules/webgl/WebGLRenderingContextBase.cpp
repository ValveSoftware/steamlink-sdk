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

#include "modules/webgl/WebGLRenderingContextBase.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptWrappableVisitor.h"
#include "bindings/core/v8/V8BindingMacros.h"
#include "bindings/modules/v8/HTMLCanvasElementOrOffscreenCanvas.h"
#include "bindings/modules/v8/WebGLAny.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMTypedArray.h"
#include "core/dom/FlexibleArrayBufferView.h"
#include "core/fetch/ImageResource.h"
#include "core/frame/ImageBitmap.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/ImageData.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/layout/LayoutBox.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "modules/webgl/ANGLEInstancedArrays.h"
#include "modules/webgl/EXTBlendMinMax.h"
#include "modules/webgl/EXTFragDepth.h"
#include "modules/webgl/EXTShaderTextureLOD.h"
#include "modules/webgl/EXTTextureFilterAnisotropic.h"
#include "modules/webgl/GLStringQuery.h"
#include "modules/webgl/OESElementIndexUint.h"
#include "modules/webgl/OESStandardDerivatives.h"
#include "modules/webgl/OESTextureFloat.h"
#include "modules/webgl/OESTextureFloatLinear.h"
#include "modules/webgl/OESTextureHalfFloat.h"
#include "modules/webgl/OESTextureHalfFloatLinear.h"
#include "modules/webgl/OESVertexArrayObject.h"
#include "modules/webgl/WebGLActiveInfo.h"
#include "modules/webgl/WebGLBuffer.h"
#include "modules/webgl/WebGLCompressedTextureASTC.h"
#include "modules/webgl/WebGLCompressedTextureATC.h"
#include "modules/webgl/WebGLCompressedTextureETC.h"
#include "modules/webgl/WebGLCompressedTextureETC1.h"
#include "modules/webgl/WebGLCompressedTexturePVRTC.h"
#include "modules/webgl/WebGLCompressedTextureS3TC.h"
#include "modules/webgl/WebGLCompressedTextureS3TCsRGB.h"
#include "modules/webgl/WebGLContextAttributeHelpers.h"
#include "modules/webgl/WebGLContextEvent.h"
#include "modules/webgl/WebGLContextGroup.h"
#include "modules/webgl/WebGLDebugRendererInfo.h"
#include "modules/webgl/WebGLDebugShaders.h"
#include "modules/webgl/WebGLDepthTexture.h"
#include "modules/webgl/WebGLDrawBuffers.h"
#include "modules/webgl/WebGLFramebuffer.h"
#include "modules/webgl/WebGLLoseContext.h"
#include "modules/webgl/WebGLProgram.h"
#include "modules/webgl/WebGLRenderbuffer.h"
#include "modules/webgl/WebGLShader.h"
#include "modules/webgl/WebGLShaderPrecisionFormat.h"
#include "modules/webgl/WebGLUniformLocation.h"
#include "modules/webgl/WebGLVertexArrayObject.h"
#include "modules/webgl/WebGLVertexArrayObjectOES.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/WaitableEvent.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "platform/graphics/gpu/AcceleratedImageBufferSurface.h"
#include "public/platform/Platform.h"
#include "wtf/CheckedNumeric.h"
#include "wtf/Functional.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/StringUTF8Adaptor.h"
#include "wtf/typed_arrays/ArrayBufferContents.h"
#include <memory>

namespace blink {

namespace {

const double secondsBetweenRestoreAttempts = 1.0;
const int maxGLErrorsAllowedToConsole = 256;
const unsigned maxGLActiveContexts = 16;
const unsigned maxGLActiveContextsOnWorker = 4;

unsigned currentMaxGLContexts() {
  return isMainThread() ? maxGLActiveContexts : maxGLActiveContextsOnWorker;
}

using WebGLRenderingContextBaseSet =
    PersistentHeapHashSet<WeakMember<WebGLRenderingContextBase>>;
WebGLRenderingContextBaseSet& activeContexts() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      ThreadSpecific<WebGLRenderingContextBaseSet>, activeContexts,
      new ThreadSpecific<WebGLRenderingContextBaseSet>());
  if (!activeContexts.isSet())
    activeContexts->registerAsStaticReference();
  return *activeContexts;
}

using WebGLRenderingContextBaseMap =
    PersistentHeapHashMap<WeakMember<WebGLRenderingContextBase>, int>;
WebGLRenderingContextBaseMap& forciblyEvictedContexts() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      ThreadSpecific<WebGLRenderingContextBaseMap>, forciblyEvictedContexts,
      new ThreadSpecific<WebGLRenderingContextBaseMap>());
  if (!forciblyEvictedContexts.isSet())
    forciblyEvictedContexts->registerAsStaticReference();
  return *forciblyEvictedContexts;
}

}  // namespace

ScopedRGBEmulationColorMask::ScopedRGBEmulationColorMask(
    gpu::gles2::GLES2Interface* contextGL,
    GLboolean* colorMask,
    DrawingBuffer* drawingBuffer)
    : m_contextGL(contextGL),
      m_requiresEmulation(drawingBuffer->requiresAlphaChannelToBePreserved()) {
  if (m_requiresEmulation) {
    memcpy(m_colorMask, colorMask, 4 * sizeof(GLboolean));
    m_contextGL->ColorMask(m_colorMask[0], m_colorMask[1], m_colorMask[2],
                           false);
  }
}

ScopedRGBEmulationColorMask::~ScopedRGBEmulationColorMask() {
  if (m_requiresEmulation)
    m_contextGL->ColorMask(m_colorMask[0], m_colorMask[1], m_colorMask[2],
                           m_colorMask[3]);
}

void WebGLRenderingContextBase::forciblyLoseOldestContext(
    const String& reason) {
  WebGLRenderingContextBase* candidate = oldestContext();
  if (!candidate)
    return;

  candidate->printWarningToConsole(reason);
  InspectorInstrumentation::didFireWebGLWarning(candidate->canvas());

  // This will call deactivateContext once the context has actually been lost.
  candidate->forceLostContext(WebGLRenderingContextBase::SyntheticLostContext,
                              WebGLRenderingContextBase::WhenAvailable);
}

WebGLRenderingContextBase* WebGLRenderingContextBase::oldestContext() {
  if (activeContexts().isEmpty())
    return nullptr;

  WebGLRenderingContextBase* candidate = *(activeContexts().begin());
  ASSERT(!candidate->isContextLost());
  for (WebGLRenderingContextBase* context : activeContexts()) {
    ASSERT(!context->isContextLost());
    if (context->contextGL()->GetLastFlushIdCHROMIUM() <
        candidate->contextGL()->GetLastFlushIdCHROMIUM()) {
      candidate = context;
    }
  }

  return candidate;
}

WebGLRenderingContextBase* WebGLRenderingContextBase::oldestEvictedContext() {
  if (forciblyEvictedContexts().isEmpty())
    return nullptr;

  WebGLRenderingContextBase* candidate = nullptr;
  int generation = -1;
  for (WebGLRenderingContextBase* context : forciblyEvictedContexts().keys()) {
    if (!candidate || forciblyEvictedContexts().get(context) < generation) {
      candidate = context;
      generation = forciblyEvictedContexts().get(context);
    }
  }

  return candidate;
}

void WebGLRenderingContextBase::activateContext(
    WebGLRenderingContextBase* context) {
  unsigned maxGLContexts = currentMaxGLContexts();
  unsigned removedContexts = 0;
  while (activeContexts().size() >= maxGLContexts &&
         removedContexts < maxGLContexts) {
    forciblyLoseOldestContext(
        "WARNING: Too many active WebGL contexts. Oldest context will be "
        "lost.");
    removedContexts++;
  }

  ASSERT(!context->isContextLost());
  activeContexts().add(context);
}

void WebGLRenderingContextBase::deactivateContext(
    WebGLRenderingContextBase* context) {
  activeContexts().remove(context);
}

void WebGLRenderingContextBase::addToEvictedList(
    WebGLRenderingContextBase* context) {
  static int generation = 0;
  forciblyEvictedContexts().set(context, generation++);
}

void WebGLRenderingContextBase::removeFromEvictedList(
    WebGLRenderingContextBase* context) {
  forciblyEvictedContexts().remove(context);
}

void WebGLRenderingContextBase::willDestroyContext(
    WebGLRenderingContextBase* context) {
  // These two sets keep weak references to their contexts;
  // verify that the GC already removed the |context| entries.
  ASSERT(!forciblyEvictedContexts().contains(context));
  ASSERT(!activeContexts().contains(context));

  unsigned maxGLContexts = currentMaxGLContexts();
  // Try to re-enable the oldest inactive contexts.
  while (activeContexts().size() < maxGLContexts &&
         forciblyEvictedContexts().size()) {
    WebGLRenderingContextBase* evictedContext = oldestEvictedContext();
    if (!evictedContext->m_restoreAllowed) {
      forciblyEvictedContexts().remove(evictedContext);
      continue;
    }

    IntSize desiredSize =
        DrawingBuffer::adjustSize(evictedContext->clampedCanvasSize(),
                                  IntSize(), evictedContext->m_maxTextureSize);

    // If there's room in the pixel budget for this context, restore it.
    if (!desiredSize.isEmpty()) {
      forciblyEvictedContexts().remove(evictedContext);
      evictedContext->forceRestoreContext();
    }
    break;
  }
}

namespace {

GLint clamp(GLint value, GLint min, GLint max) {
  if (value < min)
    value = min;
  if (value > max)
    value = max;
  return value;
}

// Return true if a character belongs to the ASCII subset as defined in
// GLSL ES 1.0 spec section 3.1.
bool validateCharacter(unsigned char c) {
  // Printing characters are valid except " $ ` @ \ ' DEL.
  if (c >= 32 && c <= 126 && c != '"' && c != '$' && c != '`' && c != '@' &&
      c != '\\' && c != '\'')
    return true;
  // Horizontal tab, line feed, vertical tab, form feed, carriage return
  // are also valid.
  if (c >= 9 && c <= 13)
    return true;
  return false;
}

bool isPrefixReserved(const String& name) {
  if (name.startsWith("gl_") || name.startsWith("webgl_") ||
      name.startsWith("_webgl_"))
    return true;
  return false;
}

// Strips comments from shader text. This allows non-ASCII characters
// to be used in comments without potentially breaking OpenGL
// implementations not expecting characters outside the GLSL ES set.
class StripComments {
 public:
  StripComments(const String& str)
      : m_parseState(BeginningOfLine),
        m_sourceString(str),
        m_length(str.length()),
        m_position(0) {
    parse();
  }

  String result() { return m_builder.toString(); }

 private:
  bool hasMoreCharacters() const { return (m_position < m_length); }

  void parse() {
    while (hasMoreCharacters()) {
      process(current());
      // process() might advance the position.
      if (hasMoreCharacters())
        advance();
    }
  }

  void process(UChar);

  bool peek(UChar& character) const {
    if (m_position + 1 >= m_length)
      return false;
    character = m_sourceString[m_position + 1];
    return true;
  }

  UChar current() {
    SECURITY_DCHECK(m_position < m_length);
    return m_sourceString[m_position];
  }

  void advance() { ++m_position; }

  static bool isNewline(UChar character) {
    // Don't attempt to canonicalize newline related characters.
    return (character == '\n' || character == '\r');
  }

  void emit(UChar character) { m_builder.append(character); }

  enum ParseState {
    // Have not seen an ASCII non-whitespace character yet on
    // this line. Possible that we might see a preprocessor
    // directive.
    BeginningOfLine,

    // Have seen at least one ASCII non-whitespace character
    // on this line.
    MiddleOfLine,

    // Handling a preprocessor directive. Passes through all
    // characters up to the end of the line. Disables comment
    // processing.
    InPreprocessorDirective,

    // Handling a single-line comment. The comment text is
    // replaced with a single space.
    InSingleLineComment,

    // Handling a multi-line comment. Newlines are passed
    // through to preserve line numbers.
    InMultiLineComment
  };

  ParseState m_parseState;
  String m_sourceString;
  unsigned m_length;
  unsigned m_position;
  StringBuilder m_builder;
};

void StripComments::process(UChar c) {
  if (isNewline(c)) {
    // No matter what state we are in, pass through newlines
    // so we preserve line numbers.
    emit(c);

    if (m_parseState != InMultiLineComment)
      m_parseState = BeginningOfLine;

    return;
  }

  UChar temp = 0;
  switch (m_parseState) {
    case BeginningOfLine:
      if (WTF::isASCIISpace(c)) {
        emit(c);
        break;
      }

      if (c == '#') {
        m_parseState = InPreprocessorDirective;
        emit(c);
        break;
      }

      // Transition to normal state and re-handle character.
      m_parseState = MiddleOfLine;
      process(c);
      break;

    case MiddleOfLine:
      if (c == '/' && peek(temp)) {
        if (temp == '/') {
          m_parseState = InSingleLineComment;
          emit(' ');
          advance();
          break;
        }

        if (temp == '*') {
          m_parseState = InMultiLineComment;
          // Emit the comment start in case the user has
          // an unclosed comment and we want to later
          // signal an error.
          emit('/');
          emit('*');
          advance();
          break;
        }
      }

      emit(c);
      break;

    case InPreprocessorDirective:
      // No matter what the character is, just pass it
      // through. Do not parse comments in this state. This
      // might not be the right thing to do long term, but it
      // should handle the #error preprocessor directive.
      emit(c);
      break;

    case InSingleLineComment:
      // Line-continuation characters are processed before comment processing.
      // Advance string if a new line character is immediately behind
      // line-continuation character.
      if (c == '\\') {
        if (peek(temp) && isNewline(temp))
          advance();
      }

      // The newline code at the top of this function takes care
      // of resetting our state when we get out of the
      // single-line comment. Swallow all other characters.
      break;

    case InMultiLineComment:
      if (c == '*' && peek(temp) && temp == '/') {
        emit('*');
        emit('/');
        m_parseState = MiddleOfLine;
        advance();
        break;
      }

      // Swallow all other characters. Unclear whether we may
      // want or need to just emit a space per character to try
      // to preserve column numbers for debugging purposes.
      break;
  }
}

static bool shouldFailContextCreationForTesting = false;
}  // namespace

class ScopedTexture2DRestorer {
  STACK_ALLOCATED();

 public:
  explicit ScopedTexture2DRestorer(WebGLRenderingContextBase* context)
      : m_context(context) {}

  ~ScopedTexture2DRestorer() { m_context->restoreCurrentTexture2D(); }

 private:
  Member<WebGLRenderingContextBase> m_context;
};

class ScopedFramebufferRestorer {
  STACK_ALLOCATED();

 public:
  explicit ScopedFramebufferRestorer(WebGLRenderingContextBase* context)
      : m_context(context) {}

  ~ScopedFramebufferRestorer() { m_context->restoreCurrentFramebuffer(); }

 private:
  Member<WebGLRenderingContextBase> m_context;
};

static void formatWebGLStatusString(const StringView& glInfo,
                                    const StringView& infoString,
                                    StringBuilder& builder) {
  if (infoString.isEmpty())
    return;
  builder.append(", ");
  builder.append(glInfo);
  builder.append(" = ");
  builder.append(infoString);
}

static String extractWebGLContextCreationError(
    const Platform::GraphicsInfo& info) {
  StringBuilder builder;
  builder.append("Could not create a WebGL context");
  formatWebGLStatusString(
      "VENDOR",
      info.vendorId ? String::format("0x%04x", info.vendorId) : "0xffff",
      builder);
  formatWebGLStatusString(
      "DEVICE",
      info.deviceId ? String::format("0x%04x", info.deviceId) : "0xffff",
      builder);
  formatWebGLStatusString("GL_VENDOR", info.vendorInfo, builder);
  formatWebGLStatusString("GL_RENDERER", info.rendererInfo, builder);
  formatWebGLStatusString("GL_VERSION", info.driverVersion, builder);
  formatWebGLStatusString("Sandboxed", info.sandboxed ? "yes" : "no", builder);
  formatWebGLStatusString("Optimus", info.optimus ? "yes" : "no", builder);
  formatWebGLStatusString("AMD switchable", info.amdSwitchable ? "yes" : "no",
                          builder);
  formatWebGLStatusString(
      "Reset notification strategy",
      String::format("0x%04x", info.resetNotificationStrategy).utf8().data(),
      builder);
  formatWebGLStatusString("GPU process crash count",
                          String::number(info.processCrashCount), builder);
  formatWebGLStatusString("ErrorMessage", info.errorMessage.utf8().data(),
                          builder);
  builder.append('.');
  return builder.toString();
}

struct ContextProviderCreationInfo {
  // Inputs.
  Platform::ContextAttributes contextAttributes;
  Platform::GraphicsInfo* glInfo;
  KURL url;
  // Outputs.
  std::unique_ptr<WebGraphicsContext3DProvider> createdContextProvider;
};

static void createContextProviderOnMainThread(
    ContextProviderCreationInfo* creationInfo,
    WaitableEvent* waitableEvent) {
  ASSERT(isMainThread());
  creationInfo->createdContextProvider =
      wrapUnique(Platform::current()->createOffscreenGraphicsContext3DProvider(
          creationInfo->contextAttributes, creationInfo->url, 0,
          creationInfo->glInfo));
  waitableEvent->signal();
}

static std::unique_ptr<WebGraphicsContext3DProvider>
createContextProviderOnWorkerThread(
    Platform::ContextAttributes contextAttributes,
    Platform::GraphicsInfo* glInfo,
    const KURL& url) {
  WaitableEvent waitableEvent;
  ContextProviderCreationInfo creationInfo;
  creationInfo.contextAttributes = contextAttributes;
  creationInfo.glInfo = glInfo;
  creationInfo.url = url;
  WebTaskRunner* taskRunner =
      Platform::current()->mainThread()->getWebTaskRunner();
  taskRunner->postTask(BLINK_FROM_HERE,
                       crossThreadBind(&createContextProviderOnMainThread,
                                       crossThreadUnretained(&creationInfo),
                                       crossThreadUnretained(&waitableEvent)));
  waitableEvent.wait();
  return std::move(creationInfo.createdContextProvider);
}

std::unique_ptr<WebGraphicsContext3DProvider>
WebGLRenderingContextBase::createContextProviderInternal(
    HTMLCanvasElement* canvas,
    ScriptState* scriptState,
    const CanvasContextCreationAttributes& attributes,
    unsigned webGLVersion) {
  // Exactly one of these must be provided.
  DCHECK_EQ(!canvas, !!scriptState);
  // The canvas is only given on the main thread.
  DCHECK(!canvas || isMainThread());

  Platform::ContextAttributes contextAttributes =
      toPlatformContextAttributes(attributes, webGLVersion);
  Platform::GraphicsInfo glInfo;
  std::unique_ptr<WebGraphicsContext3DProvider> contextProvider;
  const auto& url = canvas ? canvas->document().topDocument().url()
                           : scriptState->getExecutionContext()->url();
  if (isMainThread()) {
    contextProvider = wrapUnique(
        Platform::current()->createOffscreenGraphicsContext3DProvider(
            contextAttributes, url, 0, &glInfo));
  } else {
    contextProvider =
        createContextProviderOnWorkerThread(contextAttributes, &glInfo, url);
  }
  if (contextProvider && !contextProvider->bindToCurrentThread()) {
    contextProvider = nullptr;
    String errorString(glInfo.errorMessage.utf8().data());
    errorString.insert("bindToCurrentThread failed: ", 0);
    glInfo.errorMessage = errorString;
  }
  if (!contextProvider || shouldFailContextCreationForTesting) {
    shouldFailContextCreationForTesting = false;
    if (canvas)
      canvas->dispatchEvent(WebGLContextEvent::create(
          EventTypeNames::webglcontextcreationerror, false, true,
          extractWebGLContextCreationError(glInfo)));
    return nullptr;
  }
  gpu::gles2::GLES2Interface* gl = contextProvider->contextGL();
  if (!String(gl->GetString(GL_EXTENSIONS))
           .contains("GL_OES_packed_depth_stencil")) {
    if (canvas)
      canvas->dispatchEvent(WebGLContextEvent::create(
          EventTypeNames::webglcontextcreationerror, false, true,
          "OES_packed_depth_stencil support is required."));
    return nullptr;
  }
  return contextProvider;
}

std::unique_ptr<WebGraphicsContext3DProvider>
WebGLRenderingContextBase::createWebGraphicsContext3DProvider(
    HTMLCanvasElement* canvas,
    const CanvasContextCreationAttributes& attributes,
    unsigned webGLVersion) {
  Document& document = canvas->document();
  LocalFrame* frame = document.frame();
  if (!frame) {
    canvas->dispatchEvent(WebGLContextEvent::create(
        EventTypeNames::webglcontextcreationerror, false, true,
        "Web page was not allowed to create a WebGL context."));
    return nullptr;
  }
  Settings* settings = frame->settings();

  // The FrameLoaderClient might block creation of a new WebGL context despite
  // the page settings; in particular, if WebGL contexts were lost one or more
  // times via the GL_ARB_robustness extension.
  if (!frame->loader().client()->allowWebGL(settings &&
                                            settings->webGLEnabled())) {
    canvas->dispatchEvent(WebGLContextEvent::create(
        EventTypeNames::webglcontextcreationerror, false, true,
        "Web page was not allowed to create a WebGL context."));
    return nullptr;
  }

  return createContextProviderInternal(canvas, nullptr, attributes,
                                       webGLVersion);
}

std::unique_ptr<WebGraphicsContext3DProvider>
WebGLRenderingContextBase::createWebGraphicsContext3DProvider(
    ScriptState* scriptState,
    const CanvasContextCreationAttributes& attributes,
    unsigned webGLVersion) {
  return createContextProviderInternal(nullptr, scriptState, attributes,
                                       webGLVersion);
}

void WebGLRenderingContextBase::forceNextWebGLContextCreationToFail() {
  shouldFailContextCreationForTesting = true;
}

ImageBitmap* WebGLRenderingContextBase::transferToImageBitmapBase(
    ScriptState* scriptState) {
  UseCounter::Feature feature =
      UseCounter::OffscreenCanvasTransferToImageBitmapWebGL;
  UseCounter::count(scriptState->getExecutionContext(), feature);
  if (!drawingBuffer())
    return nullptr;
  return ImageBitmap::create(drawingBuffer()->transferToStaticBitmapImage());
}

void WebGLRenderingContextBase::commit(ScriptState* scriptState,
                                       ExceptionState& exceptionState) {
  UseCounter::Feature feature = UseCounter::OffscreenCanvasCommitWebGL;
  UseCounter::count(scriptState->getExecutionContext(), feature);
  if (!getOffscreenCanvas()) {
    exceptionState.throwDOMException(InvalidStateError,
                                     "Commit() was called on a rendering "
                                     "context that was not created from an "
                                     "OffscreenCanvas.");
    return;
  }
  // no HTMLCanvas associated, thrown InvalidStateError
  if (!getOffscreenCanvas()->hasPlaceholderCanvas()) {
    exceptionState.throwDOMException(InvalidStateError,
                                     "Commit() was called on a context whose "
                                     "OffscreenCanvas is not associated with a "
                                     "canvas element.");
    return;
  }
  if (!drawingBuffer())
    return;
  double commitStartTime = WTF::monotonicallyIncreasingTime();
  // TODO(crbug.com/646864): Make commit() work correctly with
  // { preserveDrawingBuffer : true }.
  getOffscreenCanvas()->getOrCreateFrameDispatcher()->dispatchFrame(
      std::move(drawingBuffer()->transferToStaticBitmapImage()),
      commitStartTime,
      drawingBuffer()->contextProvider()->isSoftwareRendering());
}

PassRefPtr<Image> WebGLRenderingContextBase::getImage(
    AccelerationHint hint,
    SnapshotReason reason) const {
  if (!drawingBuffer())
    return nullptr;

  drawingBuffer()->resolveAndBindForReadAndDraw();
  IntSize size = clampedCanvasSize();
  OpacityMode opacityMode =
      creationAttributes().hasAlpha() ? NonOpaque : Opaque;
  std::unique_ptr<AcceleratedImageBufferSurface> surface =
      makeUnique<AcceleratedImageBufferSurface>(size, opacityMode);
  if (!surface->isValid())
    return nullptr;
  std::unique_ptr<ImageBuffer> buffer = ImageBuffer::create(std::move(surface));
  if (!buffer->copyRenderingResultsFromDrawingBuffer(drawingBuffer(),
                                                     BackBuffer)) {
    // copyRenderingResultsFromDrawingBuffer is expected to always succeed
    // because we've explicitly created an Accelerated surface and have already
    // validated it.
    NOTREACHED();
    return nullptr;
  }
  return buffer->newImageSnapshot(hint, reason);
}

ImageData* WebGLRenderingContextBase::toImageData(SnapshotReason reason) const {
  // TODO: Furnish toImageData in webgl renderingcontext for jpeg and webp
  // images. See crbug.com/657531.
  ImageData* imageData = nullptr;
  if (this->drawingBuffer()) {
    sk_sp<SkImage> snapshot = this->drawingBuffer()
                                  ->transferToStaticBitmapImage()
                                  ->imageForCurrentFrame();
    if (snapshot) {
      imageData = ImageData::create(this->getOffscreenCanvas()->size());
      SkImageInfo imageInfo = SkImageInfo::Make(
          this->drawingBufferWidth(), this->drawingBufferHeight(),
          kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);
      snapshot->readPixels(imageInfo, imageData->data()->data(),
                           imageInfo.minRowBytes(), 0, 0);
    }
  }
  return imageData;
}

namespace {

// Exposed by GL_ANGLE_depth_texture
static const GLenum kSupportedInternalFormatsOESDepthTex[] = {
    GL_DEPTH_COMPONENT, GL_DEPTH_STENCIL,
};

// Exposed by GL_EXT_sRGB
static const GLenum kSupportedInternalFormatsEXTsRGB[] = {
    GL_SRGB, GL_SRGB_ALPHA_EXT,
};

// ES3 enums supported by both CopyTexImage and TexImage.
static const GLenum kSupportedInternalFormatsES3[] = {
    GL_R8,           GL_RG8,      GL_RGB565,   GL_RGB8,       GL_RGBA4,
    GL_RGB5_A1,      GL_RGBA8,    GL_RGB10_A2, GL_RGB10_A2UI, GL_SRGB8,
    GL_SRGB8_ALPHA8, GL_R8I,      GL_R8UI,     GL_R16I,       GL_R16UI,
    GL_R32I,         GL_R32UI,    GL_RG8I,     GL_RG8UI,      GL_RG16I,
    GL_RG16UI,       GL_RG32I,    GL_RG32UI,   GL_RGBA8I,     GL_RGBA8UI,
    GL_RGBA16I,      GL_RGBA16UI, GL_RGBA32I,  GL_RGBA32UI,
};

// ES3 enums only supported by TexImage
static const GLenum kSupportedInternalFormatsTexImageES3[] = {
    GL_R8_SNORM,
    GL_R16F,
    GL_R32F,
    GL_RG8_SNORM,
    GL_RG16F,
    GL_RG32F,
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
    GL_RGBA8_SNORM,
    GL_RGBA16F,
    GL_RGBA32F,
    GL_DEPTH_COMPONENT16,
    GL_DEPTH_COMPONENT24,
    GL_DEPTH_COMPONENT32F,
    GL_DEPTH24_STENCIL8,
    GL_DEPTH32F_STENCIL8,
};

// ES3 enums supported by TexImageSource
static const GLenum kSupportedInternalFormatsTexImageSourceES3[] = {
    GL_R8,      GL_R16F,           GL_R32F,         GL_R8UI,    GL_RG8,
    GL_RG16F,   GL_RG32F,          GL_RG8UI,        GL_RGB8,    GL_SRGB8,
    GL_RGB565,  GL_R11F_G11F_B10F, GL_RGB9_E5,      GL_RGB16F,  GL_RGB32F,
    GL_RGB8UI,  GL_RGBA8,          GL_SRGB8_ALPHA8, GL_RGB5_A1, GL_RGBA4,
    GL_RGBA16F, GL_RGBA32F,        GL_RGBA8UI,
};

// ES2 enums
// Internalformat must equal format in ES2.
static const GLenum kSupportedFormatsES2[] = {
    GL_RGB, GL_RGBA, GL_LUMINANCE_ALPHA, GL_LUMINANCE, GL_ALPHA,
};

// Exposed by GL_ANGLE_depth_texture
static const GLenum kSupportedFormatsOESDepthTex[] = {
    GL_DEPTH_COMPONENT, GL_DEPTH_STENCIL,
};

// Exposed by GL_EXT_sRGB
static const GLenum kSupportedFormatsEXTsRGB[] = {
    GL_SRGB, GL_SRGB_ALPHA_EXT,
};

// ES3 enums
static const GLenum kSupportedFormatsES3[] = {
    GL_RED,           GL_RED_INTEGER,  GL_RG,
    GL_RG_INTEGER,    GL_RGB,          GL_RGB_INTEGER,
    GL_RGBA,          GL_RGBA_INTEGER, GL_DEPTH_COMPONENT,
    GL_DEPTH_STENCIL,
};

// ES3 enums supported by TexImageSource
static const GLenum kSupportedFormatsTexImageSourceES3[] = {
    GL_RED, GL_RED_INTEGER, GL_RG,   GL_RG_INTEGER,
    GL_RGB, GL_RGB_INTEGER, GL_RGBA, GL_RGBA_INTEGER,
};

// ES2 enums
static const GLenum kSupportedTypesES2[] = {
    GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT_5_6_5, GL_UNSIGNED_SHORT_4_4_4_4,
    GL_UNSIGNED_SHORT_5_5_5_1,
};

// Exposed by GL_OES_texture_float
static const GLenum kSupportedTypesOESTexFloat[] = {
    GL_FLOAT,
};

// Exposed by GL_OES_texture_half_float
static const GLenum kSupportedTypesOESTexHalfFloat[] = {
    GL_HALF_FLOAT_OES,
};

// Exposed by GL_ANGLE_depth_texture
static const GLenum kSupportedTypesOESDepthTex[] = {
    GL_UNSIGNED_SHORT, GL_UNSIGNED_INT, GL_UNSIGNED_INT_24_8,
};

// ES3 enums
static const GLenum kSupportedTypesES3[] = {
    GL_BYTE,
    GL_UNSIGNED_SHORT,
    GL_SHORT,
    GL_UNSIGNED_INT,
    GL_INT,
    GL_HALF_FLOAT,
    GL_FLOAT,
    GL_UNSIGNED_INT_2_10_10_10_REV,
    GL_UNSIGNED_INT_10F_11F_11F_REV,
    GL_UNSIGNED_INT_5_9_9_9_REV,
    GL_UNSIGNED_INT_24_8,
    GL_FLOAT_32_UNSIGNED_INT_24_8_REV,
};

// ES3 enums supported by TexImageSource
static const GLenum kSupportedTypesTexImageSourceES3[] = {
    GL_HALF_FLOAT, GL_FLOAT, GL_UNSIGNED_INT_10F_11F_11F_REV,
};

bool isUnsignedIntegerFormat(GLenum internalformat) {
  switch (internalformat) {
    case GL_R8UI:
    case GL_R16UI:
    case GL_R32UI:
    case GL_RG8UI:
    case GL_RG16UI:
    case GL_RG32UI:
    case GL_RGB8UI:
    case GL_RGB16UI:
    case GL_RGB32UI:
    case GL_RGBA8UI:
    case GL_RGB10_A2UI:
    case GL_RGBA16UI:
    case GL_RGBA32UI:
      return true;
    default:
      return false;
  }
}

bool isSignedIntegerFormat(GLenum internalformat) {
  switch (internalformat) {
    case GL_R8I:
    case GL_R16I:
    case GL_R32I:
    case GL_RG8I:
    case GL_RG16I:
    case GL_RG32I:
    case GL_RGB8I:
    case GL_RGB16I:
    case GL_RGB32I:
    case GL_RGBA8I:
    case GL_RGBA16I:
    case GL_RGBA32I:
      return true;
    default:
      return false;
  }
}

bool isIntegerFormat(GLenum internalformat) {
  return (isUnsignedIntegerFormat(internalformat) ||
          isSignedIntegerFormat(internalformat));
}

bool isFloatType(GLenum type) {
  switch (type) {
    case GL_FLOAT:
    case GL_HALF_FLOAT:
    case GL_HALF_FLOAT_OES:
    case GL_UNSIGNED_INT_10F_11F_11F_REV:
      return true;
    default:
      return false;
  }
}

bool isSRGBFormat(GLenum internalformat) {
  switch (internalformat) {
    case GL_SRGB_EXT:
    case GL_SRGB_ALPHA_EXT:
    case GL_SRGB8:
    case GL_SRGB8_ALPHA8:
      return true;
    default:
      return false;
  }
}

}  // namespace

WebGLRenderingContextBase::WebGLRenderingContextBase(
    OffscreenCanvas* passedOffscreenCanvas,
    std::unique_ptr<WebGraphicsContext3DProvider> contextProvider,
    const CanvasContextCreationAttributes& requestedAttributes,
    unsigned version)
    : WebGLRenderingContextBase(nullptr,
                                passedOffscreenCanvas,
                                std::move(contextProvider),
                                requestedAttributes,
                                version) {}

WebGLRenderingContextBase::WebGLRenderingContextBase(
    HTMLCanvasElement* passedCanvas,
    std::unique_ptr<WebGraphicsContext3DProvider> contextProvider,
    const CanvasContextCreationAttributes& requestedAttributes,
    unsigned version)
    : WebGLRenderingContextBase(passedCanvas,
                                nullptr,
                                std::move(contextProvider),
                                requestedAttributes,
                                version) {}

WebGLRenderingContextBase::WebGLRenderingContextBase(
    HTMLCanvasElement* passedCanvas,
    OffscreenCanvas* passedOffscreenCanvas,
    std::unique_ptr<WebGraphicsContext3DProvider> contextProvider,
    const CanvasContextCreationAttributes& requestedAttributes,
    unsigned version)
    : CanvasRenderingContext(passedCanvas,
                             passedOffscreenCanvas,
                             requestedAttributes),
      m_isHidden(false),
      m_contextLostMode(NotLostContext),
      m_autoRecoveryMethod(Manual),
      m_dispatchContextLostEventTimer(
          this,
          &WebGLRenderingContextBase::dispatchContextLostEvent),
      m_restoreAllowed(false),
      m_restoreTimer(this, &WebGLRenderingContextBase::maybeRestoreContext),
      m_boundArrayBuffer(this, nullptr),
      m_boundVertexArrayObject(this, nullptr),
      m_currentProgram(this, nullptr),
      m_framebufferBinding(this, nullptr),
      m_renderbufferBinding(this, nullptr),
      m_generatedImageCache(4),
      m_synthesizedErrorsToConsole(true),
      m_numGLErrorsToConsoleAllowed(maxGLErrorsAllowedToConsole),
      m_onePlusMaxNonDefaultTextureUnit(0),
      m_isWebGL2FormatsTypesAdded(false),
      m_isWebGL2TexImageSourceFormatsTypesAdded(false),
      m_isWebGL2InternalFormatsCopyTexImageAdded(false),
      m_isOESTextureFloatFormatsTypesAdded(false),
      m_isOESTextureHalfFloatFormatsTypesAdded(false),
      m_isWebGLDepthTextureFormatsTypesAdded(false),
      m_isEXTsRGBFormatsTypesAdded(false),
      m_version(version) {
  ASSERT(contextProvider);

  m_contextGroup = WebGLContextGroup::create();
  m_contextGroup->addContext(this);

  m_maxViewportDims[0] = m_maxViewportDims[1] = 0;
  contextProvider->contextGL()->GetIntegerv(GL_MAX_VIEWPORT_DIMS,
                                            m_maxViewportDims);

  RefPtr<DrawingBuffer> buffer;
  // On Mac OS, DrawingBuffer is using an IOSurface as its backing storage, this
  // allows WebGL-rendered canvases to be composited by the OS rather than
  // Chrome.
  // IOSurfaces are only compatible with the GL_TEXTURE_RECTANGLE_ARB binding
  // target. So to avoid the knowledge of GL_TEXTURE_RECTANGLE_ARB type textures
  // being introduced into more areas of the code, we use the code path of
  // non-WebGLImageChromium for OffscreenCanvas.
  // See detailed discussion in crbug.com/649668.
  if (passedOffscreenCanvas)
    buffer = createDrawingBuffer(std::move(contextProvider),
                                 DrawingBuffer::DisallowChromiumImage);
  else
    buffer = createDrawingBuffer(std::move(contextProvider),
                                 DrawingBuffer::AllowChromiumImage);
  if (!buffer) {
    m_contextLostMode = SyntheticLostContext;
    return;
  }

  m_drawingBuffer = buffer.release();
  m_drawingBuffer->addNewMailboxCallback(
      WTF::bind(&WebGLRenderingContextBase::notifyCanvasContextChanged,
                wrapWeakPersistent(this)));
  drawingBuffer()->bind(GL_FRAMEBUFFER);
  setupFlags();

#define ADD_VALUES_TO_SET(set, values)                    \
  for (size_t i = 0; i < WTF_ARRAY_LENGTH(values); ++i) { \
    set.insert(values[i]);                                \
  }

  ADD_VALUES_TO_SET(m_supportedInternalFormats, kSupportedFormatsES2);
  ADD_VALUES_TO_SET(m_supportedTexImageSourceInternalFormats,
                    kSupportedFormatsES2);
  ADD_VALUES_TO_SET(m_supportedInternalFormatsCopyTexImage,
                    kSupportedFormatsES2);
  ADD_VALUES_TO_SET(m_supportedFormats, kSupportedFormatsES2);
  ADD_VALUES_TO_SET(m_supportedTexImageSourceFormats, kSupportedFormatsES2);
  ADD_VALUES_TO_SET(m_supportedTypes, kSupportedTypesES2);
  ADD_VALUES_TO_SET(m_supportedTexImageSourceTypes, kSupportedTypesES2);
}

PassRefPtr<DrawingBuffer> WebGLRenderingContextBase::createDrawingBuffer(
    std::unique_ptr<WebGraphicsContext3DProvider> contextProvider,
    DrawingBuffer::ChromiumImageUsage chromiumImageUsage) {
  bool premultipliedAlpha = creationAttributes().premultipliedAlpha();
  bool wantAlphaChannel = creationAttributes().alpha();
  bool wantDepthBuffer = creationAttributes().depth();
  bool wantStencilBuffer = creationAttributes().stencil();
  bool wantAntialiasing = creationAttributes().antialias();
  DrawingBuffer::PreserveDrawingBuffer preserve =
      creationAttributes().preserveDrawingBuffer() ? DrawingBuffer::Preserve
                                                   : DrawingBuffer::Discard;
  DrawingBuffer::WebGLVersion webGLVersion = DrawingBuffer::WebGL1;
  if (version() == 1) {
    webGLVersion = DrawingBuffer::WebGL1;
  } else if (version() == 2) {
    webGLVersion = DrawingBuffer::WebGL2;
  } else {
    NOTREACHED();
  }
  return DrawingBuffer::create(
      std::move(contextProvider), this, clampedCanvasSize(), premultipliedAlpha,
      wantAlphaChannel, wantDepthBuffer, wantStencilBuffer, wantAntialiasing,
      preserve, webGLVersion, chromiumImageUsage);
}

void WebGLRenderingContextBase::initializeNewContext() {
  ASSERT(!isContextLost());
  ASSERT(drawingBuffer());

  m_markedCanvasDirty = false;
  m_activeTextureUnit = 0;
  m_packAlignment = 4;
  m_unpackAlignment = 4;
  m_unpackFlipY = false;
  m_unpackPremultiplyAlpha = false;
  m_unpackColorspaceConversion = GC3D_BROWSER_DEFAULT_WEBGL;
  m_boundArrayBuffer = nullptr;
  m_currentProgram = nullptr;
  m_framebufferBinding = nullptr;
  m_renderbufferBinding = nullptr;
  m_depthMask = true;
  m_stencilEnabled = false;
  m_stencilMask = 0xFFFFFFFF;
  m_stencilMaskBack = 0xFFFFFFFF;
  m_stencilFuncRef = 0;
  m_stencilFuncRefBack = 0;
  m_stencilFuncMask = 0xFFFFFFFF;
  m_stencilFuncMaskBack = 0xFFFFFFFF;
  m_numGLErrorsToConsoleAllowed = maxGLErrorsAllowedToConsole;

  m_clearColor[0] = m_clearColor[1] = m_clearColor[2] = m_clearColor[3] = 0;
  m_scissorEnabled = false;
  m_clearDepth = 1;
  m_clearStencil = 0;
  m_colorMask[0] = m_colorMask[1] = m_colorMask[2] = m_colorMask[3] = true;

  GLint numCombinedTextureImageUnits = 0;
  contextGL()->GetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,
                           &numCombinedTextureImageUnits);
  m_textureUnits.clear();
  m_textureUnits.resize(numCombinedTextureImageUnits);

  GLint numVertexAttribs = 0;
  contextGL()->GetIntegerv(GL_MAX_VERTEX_ATTRIBS, &numVertexAttribs);
  m_maxVertexAttribs = numVertexAttribs;

  m_maxTextureSize = 0;
  contextGL()->GetIntegerv(GL_MAX_TEXTURE_SIZE, &m_maxTextureSize);
  m_maxTextureLevel =
      WebGLTexture::computeLevelCount(m_maxTextureSize, m_maxTextureSize, 1);
  m_maxCubeMapTextureSize = 0;
  contextGL()->GetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE,
                           &m_maxCubeMapTextureSize);
  m_max3DTextureSize = 0;
  m_max3DTextureLevel = 0;
  m_maxArrayTextureLayers = 0;
  if (isWebGL2OrHigher()) {
    contextGL()->GetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &m_max3DTextureSize);
    m_max3DTextureLevel = WebGLTexture::computeLevelCount(
        m_max3DTextureSize, m_max3DTextureSize, m_max3DTextureSize);
    contextGL()->GetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS,
                             &m_maxArrayTextureLayers);
  }
  m_maxCubeMapTextureLevel = WebGLTexture::computeLevelCount(
      m_maxCubeMapTextureSize, m_maxCubeMapTextureSize, 1);
  m_maxRenderbufferSize = 0;
  contextGL()->GetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &m_maxRenderbufferSize);

  // These two values from EXT_draw_buffers are lazily queried.
  m_maxDrawBuffers = 0;
  m_maxColorAttachments = 0;

  m_backDrawBuffer = GL_BACK;

  m_readBufferOfDefaultFramebuffer = GL_BACK;

  m_defaultVertexArrayObject = WebGLVertexArrayObject::create(
      this, WebGLVertexArrayObjectBase::VaoTypeDefault);
  addContextObject(m_defaultVertexArrayObject.get());

  m_boundVertexArrayObject = m_defaultVertexArrayObject;

  m_vertexAttribType.resize(m_maxVertexAttribs);

  contextGL()->Viewport(0, 0, drawingBufferWidth(), drawingBufferHeight());
  m_scissorBox[0] = m_scissorBox[1] = 0;
  m_scissorBox[2] = drawingBufferWidth();
  m_scissorBox[3] = drawingBufferHeight();
  contextGL()->Scissor(m_scissorBox[0], m_scissorBox[1], m_scissorBox[2],
                       m_scissorBox[3]);

  drawingBuffer()->contextProvider()->setLostContextCallback(
      convertToBaseCallback(WTF::bind(
          &WebGLRenderingContextBase::forceLostContext,
          wrapWeakPersistent(this), WebGLRenderingContextBase::RealLostContext,
          WebGLRenderingContextBase::Auto)));
  drawingBuffer()->contextProvider()->setErrorMessageCallback(
      convertToBaseCallback(
          WTF::bind(&WebGLRenderingContextBase::onErrorMessage,
                    wrapWeakPersistent(this))));

  // If WebGL 2, the PRIMITIVE_RESTART_FIXED_INDEX should be always enabled.
  // See the section <Primitive Restart is Always Enabled> in WebGL 2 spec:
  // https://www.khronos.org/registry/webgl/specs/latest/2.0/#4.1.4
  if (isWebGL2OrHigher())
    contextGL()->Enable(GL_PRIMITIVE_RESTART_FIXED_INDEX);

  // This ensures that the context has a valid "lastFlushID" and won't be
  // mistakenly identified as the "least recently used" context.
  contextGL()->Flush();

  for (int i = 0; i < WebGLExtensionNameCount; ++i)
    m_extensionEnabled[i] = false;

  m_isWebGL2FormatsTypesAdded = false;
  m_isWebGL2TexImageSourceFormatsTypesAdded = false;
  m_isWebGL2InternalFormatsCopyTexImageAdded = false;
  m_isOESTextureFloatFormatsTypesAdded = false;
  m_isOESTextureHalfFloatFormatsTypesAdded = false;
  m_isWebGLDepthTextureFormatsTypesAdded = false;
  m_isEXTsRGBFormatsTypesAdded = false;

  m_supportedInternalFormats.clear();
  ADD_VALUES_TO_SET(m_supportedInternalFormats, kSupportedFormatsES2);
  m_supportedTexImageSourceInternalFormats.clear();
  ADD_VALUES_TO_SET(m_supportedTexImageSourceInternalFormats,
                    kSupportedFormatsES2);
  m_supportedInternalFormatsCopyTexImage.clear();
  ADD_VALUES_TO_SET(m_supportedInternalFormatsCopyTexImage,
                    kSupportedFormatsES2);
  m_supportedFormats.clear();
  ADD_VALUES_TO_SET(m_supportedFormats, kSupportedFormatsES2);
  m_supportedTexImageSourceFormats.clear();
  ADD_VALUES_TO_SET(m_supportedTexImageSourceFormats, kSupportedFormatsES2);
  m_supportedTypes.clear();
  ADD_VALUES_TO_SET(m_supportedTypes, kSupportedTypesES2);
  m_supportedTexImageSourceTypes.clear();
  ADD_VALUES_TO_SET(m_supportedTexImageSourceTypes, kSupportedTypesES2);

  // The DrawingBuffer was unable to store the state that dirtied when it was
  // initialized. Restore it now.
  drawingBuffer()->restoreAllState();
  activateContext(this);
}

void WebGLRenderingContextBase::setupFlags() {
  ASSERT(drawingBuffer());
  if (canvas()) {
    if (Page* p = canvas()->document().page()) {
      m_synthesizedErrorsToConsole =
          p->settings().webGLErrorsToConsoleEnabled();
    }
  }

  m_isDepthStencilSupported =
      extensionsUtil()->isExtensionEnabled("GL_OES_packed_depth_stencil");
}

void WebGLRenderingContextBase::addCompressedTextureFormat(GLenum format) {
  if (!m_compressedTextureFormats.contains(format))
    m_compressedTextureFormats.append(format);
}

void WebGLRenderingContextBase::removeAllCompressedTextureFormats() {
  m_compressedTextureFormats.clear();
}

// Helper function for V8 bindings to identify what version of WebGL a
// CanvasRenderingContext supports.
unsigned WebGLRenderingContextBase::getWebGLVersion(
    const CanvasRenderingContext* context) {
  if (!context->is3d())
    return 0;
  return static_cast<const WebGLRenderingContextBase*>(context)->version();
}

WebGLRenderingContextBase::~WebGLRenderingContextBase() {
  // Remove all references to WebGLObjects so if they are the last reference
  // they will be freed before the last context is removed from the context
  // group.
  m_boundArrayBuffer = nullptr;
  m_defaultVertexArrayObject = nullptr;
  m_boundVertexArrayObject = nullptr;
  m_currentProgram = nullptr;
  m_framebufferBinding = nullptr;
  m_renderbufferBinding = nullptr;

  // WebGLTexture shared objects will be detached and deleted
  // m_contextGroup->removeContext(this), which will bring about deleteTexture()
  // calls.  We null these out to avoid accessing those members in
  // deleteTexture().
  for (size_t i = 0; i < m_textureUnits.size(); ++i) {
    m_textureUnits[i].m_texture2DBinding = nullptr;
    m_textureUnits[i].m_textureCubeMapBinding = nullptr;
    m_textureUnits[i].m_texture3DBinding = nullptr;
    m_textureUnits[i].m_texture2DArrayBinding = nullptr;
  }

  detachAndRemoveAllObjects();

  // Release all extensions now.
  for (ExtensionTracker* tracker : m_extensions) {
    tracker->loseExtension(true);
  }
  m_extensions.clear();

  // Context must be removed from the group prior to the destruction of the
  // GL context, otherwise shared objects may not be properly deleted.
  m_contextGroup->removeContext(this);

  destroyContext();

  willDestroyContext(this);
}

void WebGLRenderingContextBase::destroyContext() {
  if (!drawingBuffer())
    return;

  m_extensionsUtil.reset();

  std::unique_ptr<WTF::Closure> nullClosure;
  std::unique_ptr<WTF::Function<void(const char*, int32_t)>> nullFunction;
  drawingBuffer()->contextProvider()->setLostContextCallback(
      convertToBaseCallback(std::move(nullClosure)));
  drawingBuffer()->contextProvider()->setErrorMessageCallback(
      convertToBaseCallback(std::move(nullFunction)));
  drawingBuffer()->addNewMailboxCallback(nullptr);

  ASSERT(drawingBuffer());
  m_drawingBuffer->beginDestruction();
  m_drawingBuffer.clear();
}

void WebGLRenderingContextBase::markContextChanged(
    ContentChangeType changeType) {
  if (m_framebufferBinding || isContextLost())
    return;

  drawingBuffer()->markContentsChanged();

  if (!canvas())
    return;

  LayoutBox* layoutBox = canvas()->layoutBox();
  if (layoutBox && layoutBox->hasAcceleratedCompositing()) {
    m_markedCanvasDirty = true;
    canvas()->clearCopiedImage();
    layoutBox->contentChanged(changeType);
  } else {
    if (!m_markedCanvasDirty) {
      m_markedCanvasDirty = true;
      canvas()->didDraw(
          FloatRect(FloatPoint(0, 0), FloatSize(clampedCanvasSize())));
    }
  }
}

void WebGLRenderingContextBase::onErrorMessage(const char* message,
                                               int32_t id) {
  if (m_synthesizedErrorsToConsole)
    printGLErrorToConsole(message);
  InspectorInstrumentation::didFireWebGLErrorOrWarning(canvas(), message);
}

void WebGLRenderingContextBase::notifyCanvasContextChanged() {
  if (!canvas())
    return;

  canvas()->notifyListenersCanvasChanged();
}

WebGLRenderingContextBase::HowToClear
WebGLRenderingContextBase::clearIfComposited(GLbitfield mask) {
  if (isContextLost())
    return Skipped;

  if (!drawingBuffer()->bufferClearNeeded() || (mask && m_framebufferBinding))
    return Skipped;

  Nullable<WebGLContextAttributes> contextAttributes;
  getContextAttributes(contextAttributes);
  if (contextAttributes.isNull()) {
    // Unlikely, but context was lost.
    return Skipped;
  }

  // Determine if it's possible to combine the clear the user asked for and this
  // clear.
  bool combinedClear = mask && !m_scissorEnabled;

  contextGL()->Disable(GL_SCISSOR_TEST);
  if (combinedClear && (mask & GL_COLOR_BUFFER_BIT)) {
    contextGL()->ClearColor(m_colorMask[0] ? m_clearColor[0] : 0,
                            m_colorMask[1] ? m_clearColor[1] : 0,
                            m_colorMask[2] ? m_clearColor[2] : 0,
                            m_colorMask[3] ? m_clearColor[3] : 0);
  } else {
    contextGL()->ClearColor(0, 0, 0, 0);
  }
  contextGL()->ColorMask(true, true, true,
                         !drawingBuffer()->requiresAlphaChannelToBePreserved());
  GLbitfield clearMask = GL_COLOR_BUFFER_BIT;
  if (contextAttributes.get().depth()) {
    if (!combinedClear || !m_depthMask || !(mask & GL_DEPTH_BUFFER_BIT))
      contextGL()->ClearDepthf(1.0f);
    clearMask |= GL_DEPTH_BUFFER_BIT;
    contextGL()->DepthMask(true);
  }
  if (contextAttributes.get().stencil() ||
      drawingBuffer()->hasImplicitStencilBuffer()) {
    if (combinedClear && (mask & GL_STENCIL_BUFFER_BIT))
      contextGL()->ClearStencil(m_clearStencil & m_stencilMask);
    else
      contextGL()->ClearStencil(0);
    clearMask |= GL_STENCIL_BUFFER_BIT;
    contextGL()->StencilMaskSeparate(GL_FRONT, 0xFFFFFFFF);
  }

  contextGL()->ColorMask(
      true, true, true,
      !drawingBuffer()->defaultBufferRequiresAlphaChannelToBePreserved());
  drawingBuffer()->clearFramebuffers(clearMask);

  // Call the DrawingBufferClient method to restore scissor test, mask, and
  // clear values, because we dirtied them above.
  DrawingBufferClientRestoreScissorTest();
  DrawingBufferClientRestoreMaskAndClearValues();

  drawingBuffer()->setBufferClearNeeded(false);

  return combinedClear ? CombinedClear : JustClear;
}

void WebGLRenderingContextBase::restoreScissorEnabled() {
  if (isContextLost())
    return;

  if (m_scissorEnabled) {
    contextGL()->Enable(GL_SCISSOR_TEST);
  } else {
    contextGL()->Disable(GL_SCISSOR_TEST);
  }
}

void WebGLRenderingContextBase::restoreScissorBox() {
  if (isContextLost())
    return;

  contextGL()->Scissor(m_scissorBox[0], m_scissorBox[1], m_scissorBox[2],
                       m_scissorBox[3]);
}

void WebGLRenderingContextBase::restoreClearColor() {
  if (isContextLost())
    return;

  contextGL()->ClearColor(m_clearColor[0], m_clearColor[1], m_clearColor[2],
                          m_clearColor[3]);
}

void WebGLRenderingContextBase::restoreColorMask() {
  if (isContextLost())
    return;

  contextGL()->ColorMask(m_colorMask[0], m_colorMask[1], m_colorMask[2],
                         m_colorMask[3]);
}

void WebGLRenderingContextBase::markLayerComposited() {
  if (!isContextLost())
    drawingBuffer()->setBufferClearNeeded(true);
}

void WebGLRenderingContextBase::setIsHidden(bool hidden) {
  m_isHidden = hidden;
  if (drawingBuffer())
    drawingBuffer()->setIsHidden(hidden);

  if (!hidden && isContextLost() && m_restoreAllowed &&
      m_autoRecoveryMethod == Auto) {
    ASSERT(!m_restoreTimer.isActive());
    m_restoreTimer.startOneShot(0, BLINK_FROM_HERE);
  }
}

bool WebGLRenderingContextBase::paintRenderingResultsToCanvas(
    SourceDrawingBuffer sourceBuffer) {
  if (isContextLost())
    return false;

  bool mustClearNow = clearIfComposited() != Skipped;
  if (!m_markedCanvasDirty && !mustClearNow)
    return false;

  canvas()->clearCopiedImage();
  m_markedCanvasDirty = false;

  if (!canvas()->buffer())
    return false;

  ScopedTexture2DRestorer restorer(this);
  ScopedFramebufferRestorer fboRestorer(this);

  drawingBuffer()->resolveAndBindForReadAndDraw();
  if (!canvas()->buffer()->copyRenderingResultsFromDrawingBuffer(
          drawingBuffer(), sourceBuffer)) {
    // Currently, copyRenderingResultsFromDrawingBuffer is expected to always
    // succeed because cases where canvas()-buffer() is not accelerated are
    // handle before reaching this point.  If that assumption ever stops holding
    // true, we may need to implement a fallback right here.
    ASSERT_NOT_REACHED();
    return false;
  }

  return true;
}

ImageData* WebGLRenderingContextBase::paintRenderingResultsToImageData(
    SourceDrawingBuffer sourceBuffer) {
  if (isContextLost())
    return nullptr;
  if (creationAttributes().premultipliedAlpha())
    return nullptr;

  clearIfComposited();
  drawingBuffer()->resolveAndBindForReadAndDraw();
  ScopedFramebufferRestorer restorer(this);
  int width, height;
  WTF::ArrayBufferContents contents;
  if (!drawingBuffer()->paintRenderingResultsToImageData(
          width, height, sourceBuffer, contents))
    return nullptr;
  DOMArrayBuffer* imageDataPixels = DOMArrayBuffer::create(contents);

  return ImageData::create(
      IntSize(width, height),
      DOMUint8ClampedArray::create(imageDataPixels, 0,
                                   imageDataPixels->byteLength()));
}

void WebGLRenderingContextBase::reshape(int width, int height) {
  if (isContextLost())
    return;

  if (isWebGL2OrHigher()) {
    contextGL()->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  }

  // This is an approximation because at WebGLRenderingContextBase level we
  // don't know if the underlying FBO uses textures or renderbuffers.
  GLint maxSize = std::min(m_maxTextureSize, m_maxRenderbufferSize);
  GLint maxWidth = std::min(maxSize, m_maxViewportDims[0]);
  GLint maxHeight = std::min(maxSize, m_maxViewportDims[1]);
  width = clamp(width, 1, maxWidth);
  height = clamp(height, 1, maxHeight);

  // Limit drawing buffer area to 4k*4k to avoid memory exhaustion. Width or
  // height may be larger than 4k as long as it's within the max viewport
  // dimensions and total area remains within the limit.
  // For example: 5120x2880 should be fine.
  const int maxArea = 4096 * 4096;
  int currentArea = width * height;
  if (currentArea > maxArea) {
    // If we've exceeded the area limit scale the buffer down, preserving
    // ascpect ratio, until it fits.
    float scaleFactor =
        sqrtf(static_cast<float>(maxArea) / static_cast<float>(currentArea));
    width = std::max(1, static_cast<int>(width * scaleFactor));
    height = std::max(1, static_cast<int>(height * scaleFactor));
  }

  // We don't have to mark the canvas as dirty, since the newly created image
  // buffer will also start off clear (and this matches what reshape will do).
  drawingBuffer()->resize(IntSize(width, height));
}

int WebGLRenderingContextBase::drawingBufferWidth() const {
  return isContextLost() ? 0 : drawingBuffer()->size().width();
}

int WebGLRenderingContextBase::drawingBufferHeight() const {
  return isContextLost() ? 0 : drawingBuffer()->size().height();
}

void WebGLRenderingContextBase::activeTexture(GLenum texture) {
  if (isContextLost())
    return;
  if (texture - GL_TEXTURE0 >= m_textureUnits.size()) {
    synthesizeGLError(GL_INVALID_ENUM, "activeTexture",
                      "texture unit out of range");
    return;
  }
  m_activeTextureUnit = texture - GL_TEXTURE0;
  contextGL()->ActiveTexture(texture);
}

void WebGLRenderingContextBase::attachShader(WebGLProgram* program,
                                             WebGLShader* shader) {
  if (isContextLost() || !validateWebGLObject("attachShader", program) ||
      !validateWebGLObject("attachShader", shader))
    return;
  if (!program->attachShader(shader)) {
    synthesizeGLError(GL_INVALID_OPERATION, "attachShader",
                      "shader attachment already has shader");
    return;
  }
  contextGL()->AttachShader(objectOrZero(program), objectOrZero(shader));
  shader->onAttached();
}

void WebGLRenderingContextBase::bindAttribLocation(WebGLProgram* program,
                                                   GLuint index,
                                                   const String& name) {
  if (isContextLost() || !validateWebGLObject("bindAttribLocation", program))
    return;
  if (!validateLocationLength("bindAttribLocation", name))
    return;
  if (isPrefixReserved(name)) {
    synthesizeGLError(GL_INVALID_OPERATION, "bindAttribLocation",
                      "reserved prefix");
    return;
  }
  contextGL()->BindAttribLocation(objectOrZero(program), index,
                                  name.utf8().data());
}

bool WebGLRenderingContextBase::checkObjectToBeBound(const char* functionName,
                                                     WebGLObject* object,
                                                     bool& deleted) {
  deleted = false;
  if (isContextLost())
    return false;
  if (object) {
    if (!object->validate(contextGroup(), this)) {
      synthesizeGLError(GL_INVALID_OPERATION, functionName,
                        "object not from this context");
      return false;
    }
    deleted = !object->hasObject();
  }
  return true;
}

bool WebGLRenderingContextBase::validateAndUpdateBufferBindTarget(
    const char* functionName,
    GLenum target,
    WebGLBuffer* buffer) {
  if (!validateBufferTarget(functionName, target))
    return false;

  if (buffer && buffer->getInitialTarget() &&
      buffer->getInitialTarget() != target) {
    synthesizeGLError(GL_INVALID_OPERATION, functionName,
                      "buffers can not be used with multiple targets");
    return false;
  }

  switch (target) {
    case GL_ARRAY_BUFFER:
      m_boundArrayBuffer = buffer;
      break;
    case GL_ELEMENT_ARRAY_BUFFER:
      m_boundVertexArrayObject->setElementArrayBuffer(buffer);
      break;
    default:
      ASSERT_NOT_REACHED();
      return false;
  }

  if (buffer && !buffer->getInitialTarget())
    buffer->setInitialTarget(target);
  return true;
}

void WebGLRenderingContextBase::bindBuffer(GLenum target, WebGLBuffer* buffer) {
  bool deleted;
  if (!checkObjectToBeBound("bindBuffer", buffer, deleted))
    return;
  if (deleted) {
    synthesizeGLError(GL_INVALID_OPERATION, "bindBuffer",
                      "attempt to bind a deleted buffer");
    return;
  }
  if (!validateAndUpdateBufferBindTarget("bindBuffer", target, buffer))
    return;
  contextGL()->BindBuffer(target, objectOrZero(buffer));
}

void WebGLRenderingContextBase::bindFramebuffer(GLenum target,
                                                WebGLFramebuffer* buffer) {
  bool deleted;
  if (!checkObjectToBeBound("bindFramebuffer", buffer, deleted))
    return;
  if (deleted) {
    synthesizeGLError(GL_INVALID_OPERATION, "bindFramebuffer",
                      "attempt to bind a deleted framebuffer");
    return;
  }

  if (target != GL_FRAMEBUFFER) {
    synthesizeGLError(GL_INVALID_ENUM, "bindFramebuffer", "invalid target");
    return;
  }

  setFramebuffer(target, buffer);
}

void WebGLRenderingContextBase::bindRenderbuffer(
    GLenum target,
    WebGLRenderbuffer* renderBuffer) {
  bool deleted;
  if (!checkObjectToBeBound("bindRenderbuffer", renderBuffer, deleted))
    return;
  if (deleted) {
    synthesizeGLError(GL_INVALID_OPERATION, "bindRenderbuffer",
                      "attempt to bind a deleted renderbuffer");
    return;
  }
  if (target != GL_RENDERBUFFER) {
    synthesizeGLError(GL_INVALID_ENUM, "bindRenderbuffer", "invalid target");
    return;
  }
  m_renderbufferBinding = renderBuffer;
  contextGL()->BindRenderbuffer(target, objectOrZero(renderBuffer));
  if (renderBuffer)
    renderBuffer->setHasEverBeenBound();
}

void WebGLRenderingContextBase::bindTexture(GLenum target,
                                            WebGLTexture* texture) {
  bool deleted;
  if (!checkObjectToBeBound("bindTexture", texture, deleted))
    return;
  if (deleted) {
    synthesizeGLError(GL_INVALID_OPERATION, "bindTexture",
                      "attempt to bind a deleted texture");
    return;
  }
  if (texture && texture->getTarget() && texture->getTarget() != target) {
    synthesizeGLError(GL_INVALID_OPERATION, "bindTexture",
                      "textures can not be used with multiple targets");
    return;
  }

  if (target == GL_TEXTURE_2D) {
    m_textureUnits[m_activeTextureUnit].m_texture2DBinding =
        TraceWrapperMember<WebGLTexture>(this, texture);
  } else if (target == GL_TEXTURE_CUBE_MAP) {
    m_textureUnits[m_activeTextureUnit].m_textureCubeMapBinding =
        TraceWrapperMember<WebGLTexture>(this, texture);
  } else if (isWebGL2OrHigher() && target == GL_TEXTURE_2D_ARRAY) {
    m_textureUnits[m_activeTextureUnit].m_texture2DArrayBinding =
        TraceWrapperMember<WebGLTexture>(this, texture);
  } else if (isWebGL2OrHigher() && target == GL_TEXTURE_3D) {
    m_textureUnits[m_activeTextureUnit].m_texture3DBinding =
        TraceWrapperMember<WebGLTexture>(this, texture);
  } else {
    synthesizeGLError(GL_INVALID_ENUM, "bindTexture", "invalid target");
    return;
  }

  contextGL()->BindTexture(target, objectOrZero(texture));
  if (texture) {
    texture->setTarget(target);
    m_onePlusMaxNonDefaultTextureUnit =
        max(m_activeTextureUnit + 1, m_onePlusMaxNonDefaultTextureUnit);
  } else {
    // If the disabled index is the current maximum, trace backwards to find the
    // new max enabled texture index
    if (m_onePlusMaxNonDefaultTextureUnit == m_activeTextureUnit + 1) {
      findNewMaxNonDefaultTextureUnit();
    }
  }

  // Note: previously we used to automatically set the TEXTURE_WRAP_R
  // repeat mode to CLAMP_TO_EDGE for cube map textures, because OpenGL
  // ES 2.0 doesn't expose this flag (a bug in the specification) and
  // otherwise the application has no control over the seams in this
  // dimension. However, it appears that supporting this properly on all
  // platforms is fairly involved (will require a HashMap from texture ID
  // in all ports), and we have not had any complaints, so the logic has
  // been removed.
}

void WebGLRenderingContextBase::blendColor(GLfloat red,
                                           GLfloat green,
                                           GLfloat blue,
                                           GLfloat alpha) {
  if (isContextLost())
    return;
  contextGL()->BlendColor(red, green, blue, alpha);
}

void WebGLRenderingContextBase::blendEquation(GLenum mode) {
  if (isContextLost() || !validateBlendEquation("blendEquation", mode))
    return;
  contextGL()->BlendEquation(mode);
}

void WebGLRenderingContextBase::blendEquationSeparate(GLenum modeRGB,
                                                      GLenum modeAlpha) {
  if (isContextLost() ||
      !validateBlendEquation("blendEquationSeparate", modeRGB) ||
      !validateBlendEquation("blendEquationSeparate", modeAlpha))
    return;
  contextGL()->BlendEquationSeparate(modeRGB, modeAlpha);
}

void WebGLRenderingContextBase::blendFunc(GLenum sfactor, GLenum dfactor) {
  if (isContextLost() ||
      !validateBlendFuncFactors("blendFunc", sfactor, dfactor))
    return;
  contextGL()->BlendFunc(sfactor, dfactor);
}

void WebGLRenderingContextBase::blendFuncSeparate(GLenum srcRGB,
                                                  GLenum dstRGB,
                                                  GLenum srcAlpha,
                                                  GLenum dstAlpha) {
  // Note: Alpha does not have the same restrictions as RGB.
  if (isContextLost() ||
      !validateBlendFuncFactors("blendFuncSeparate", srcRGB, dstRGB))
    return;
  contextGL()->BlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void WebGLRenderingContextBase::bufferDataImpl(GLenum target,
                                               long long size,
                                               const void* data,
                                               GLenum usage) {
  WebGLBuffer* buffer = validateBufferDataTarget("bufferData", target);
  if (!buffer)
    return;

  if (!validateBufferDataUsage("bufferData", usage))
    return;

  if (!validateValueFitNonNegInt32("bufferData", "size", size))
    return;

  buffer->setSize(size);

  contextGL()->BufferData(target, static_cast<GLsizeiptr>(size), data, usage);
}

void WebGLRenderingContextBase::bufferData(GLenum target,
                                           long long size,
                                           GLenum usage) {
  if (isContextLost())
    return;
  bufferDataImpl(target, size, 0, usage);
}

void WebGLRenderingContextBase::bufferData(GLenum target,
                                           DOMArrayBuffer* data,
                                           GLenum usage) {
  if (isContextLost())
    return;
  if (!data) {
    synthesizeGLError(GL_INVALID_VALUE, "bufferData", "no data");
    return;
  }
  bufferDataImpl(target, data->byteLength(), data->data(), usage);
}

void WebGLRenderingContextBase::bufferData(GLenum target,
                                           DOMArrayBufferView* data,
                                           GLenum usage) {
  if (isContextLost())
    return;
  DCHECK(data);
  bufferDataImpl(target, data->byteLength(), data->baseAddress(), usage);
}

void WebGLRenderingContextBase::bufferSubDataImpl(GLenum target,
                                                  long long offset,
                                                  GLsizeiptr size,
                                                  const void* data) {
  WebGLBuffer* buffer = validateBufferDataTarget("bufferSubData", target);
  if (!buffer)
    return;
  if (!validateValueFitNonNegInt32("bufferSubData", "offset", offset))
    return;
  if (!data)
    return;
  if (offset + static_cast<long long>(size) > buffer->getSize()) {
    synthesizeGLError(GL_INVALID_VALUE, "bufferSubData", "buffer overflow");
    return;
  }

  contextGL()->BufferSubData(target, static_cast<GLintptr>(offset), size, data);
}

void WebGLRenderingContextBase::bufferSubData(GLenum target,
                                              long long offset,
                                              DOMArrayBuffer* data) {
  if (isContextLost())
    return;
  DCHECK(data);
  bufferSubDataImpl(target, offset, data->byteLength(), data->data());
}

void WebGLRenderingContextBase::bufferSubData(
    GLenum target,
    long long offset,
    const FlexibleArrayBufferView& data) {
  if (isContextLost())
    return;
  DCHECK(data);
  bufferSubDataImpl(target, offset, data.byteLength(),
                    data.baseAddressMaybeOnStack());
}

bool WebGLRenderingContextBase::validateFramebufferTarget(GLenum target) {
  if (target == GL_FRAMEBUFFER)
    return true;
  return false;
}

WebGLFramebuffer* WebGLRenderingContextBase::getFramebufferBinding(
    GLenum target) {
  if (target == GL_FRAMEBUFFER)
    return m_framebufferBinding.get();
  return nullptr;
}

WebGLFramebuffer* WebGLRenderingContextBase::getReadFramebufferBinding() {
  return m_framebufferBinding.get();
}

GLenum WebGLRenderingContextBase::checkFramebufferStatus(GLenum target) {
  if (isContextLost())
    return GL_FRAMEBUFFER_UNSUPPORTED;
  if (!validateFramebufferTarget(target)) {
    synthesizeGLError(GL_INVALID_ENUM, "checkFramebufferStatus",
                      "invalid target");
    return 0;
  }
  WebGLFramebuffer* framebufferBinding = getFramebufferBinding(target);
  if (framebufferBinding) {
    const char* reason = "framebuffer incomplete";
    GLenum status = framebufferBinding->checkDepthStencilStatus(&reason);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
      emitGLWarning("checkFramebufferStatus", reason);
      return status;
    }
  }
  return contextGL()->CheckFramebufferStatus(target);
}

void WebGLRenderingContextBase::clear(GLbitfield mask) {
  if (isContextLost())
    return;
  if (mask &
      ~(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) {
    synthesizeGLError(GL_INVALID_VALUE, "clear", "invalid mask");
    return;
  }
  const char* reason = "framebuffer incomplete";
  if (m_framebufferBinding &&
      m_framebufferBinding->checkDepthStencilStatus(&reason) !=
          GL_FRAMEBUFFER_COMPLETE) {
    synthesizeGLError(GL_INVALID_FRAMEBUFFER_OPERATION, "clear", reason);
    return;
  }

  ScopedRGBEmulationColorMask emulationColorMask(contextGL(), m_colorMask,
                                                 m_drawingBuffer.get());

  if (clearIfComposited(mask) != CombinedClear) {
    // If clearing the default back buffer's depth buffer, also clear the
    // stencil buffer, if one was allocated implicitly. This avoids performance
    // problems on some GPUs.
    if (!m_framebufferBinding && drawingBuffer()->hasImplicitStencilBuffer() &&
        (mask & GL_DEPTH_BUFFER_BIT)) {
      // It shouldn't matter what value it's cleared to, since in other queries
      // in the API, we claim that the stencil buffer doesn't exist.
      mask |= GL_STENCIL_BUFFER_BIT;
    }
    contextGL()->Clear(mask);
  }
  markContextChanged(CanvasChanged);
}

void WebGLRenderingContextBase::clearColor(GLfloat r,
                                           GLfloat g,
                                           GLfloat b,
                                           GLfloat a) {
  if (isContextLost())
    return;
  if (std::isnan(r))
    r = 0;
  if (std::isnan(g))
    g = 0;
  if (std::isnan(b))
    b = 0;
  if (std::isnan(a))
    a = 1;
  m_clearColor[0] = r;
  m_clearColor[1] = g;
  m_clearColor[2] = b;
  m_clearColor[3] = a;
  contextGL()->ClearColor(r, g, b, a);
}

void WebGLRenderingContextBase::clearDepth(GLfloat depth) {
  if (isContextLost())
    return;
  m_clearDepth = depth;
  contextGL()->ClearDepthf(depth);
}

void WebGLRenderingContextBase::clearStencil(GLint s) {
  if (isContextLost())
    return;
  m_clearStencil = s;
  contextGL()->ClearStencil(s);
}

void WebGLRenderingContextBase::colorMask(GLboolean red,
                                          GLboolean green,
                                          GLboolean blue,
                                          GLboolean alpha) {
  if (isContextLost())
    return;
  m_colorMask[0] = red;
  m_colorMask[1] = green;
  m_colorMask[2] = blue;
  m_colorMask[3] = alpha;
  contextGL()->ColorMask(red, green, blue, alpha);
}

void WebGLRenderingContextBase::compileShader(WebGLShader* shader) {
  if (isContextLost() || !validateWebGLObject("compileShader", shader))
    return;
  contextGL()->CompileShader(objectOrZero(shader));
}

void WebGLRenderingContextBase::compressedTexImage2D(GLenum target,
                                                     GLint level,
                                                     GLenum internalformat,
                                                     GLsizei width,
                                                     GLsizei height,
                                                     GLint border,
                                                     DOMArrayBufferView* data) {
  if (isContextLost())
    return;
  if (!validateTexture2DBinding("compressedTexImage2D", target))
    return;
  if (!validateCompressedTexFormat("compressedTexImage2D", internalformat))
    return;
  contextGL()->CompressedTexImage2D(target, level, internalformat, width,
                                    height, border, data->byteLength(),
                                    data->baseAddress());
}

void WebGLRenderingContextBase::compressedTexSubImage2D(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    DOMArrayBufferView* data) {
  if (isContextLost())
    return;
  if (!validateTexture2DBinding("compressedTexSubImage2D", target))
    return;
  if (!validateCompressedTexFormat("compressedTexSubImage2D", format))
    return;
  contextGL()->CompressedTexSubImage2D(target, level, xoffset, yoffset, width,
                                       height, format, data->byteLength(),
                                       data->baseAddress());
}

bool WebGLRenderingContextBase::validateSettableTexFormat(
    const char* functionName,
    GLenum format) {
  if (isWebGL2OrHigher())
    return true;

  if (WebGLImageConversion::getChannelBitsByFormat(format) &
      WebGLImageConversion::ChannelDepthStencil) {
    synthesizeGLError(GL_INVALID_OPERATION, functionName,
                      "format can not be set, only rendered to");
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::validateCopyTexFormat(const char* functionName,
                                                      GLenum internalformat) {
  if (!m_isWebGL2InternalFormatsCopyTexImageAdded && isWebGL2OrHigher()) {
    ADD_VALUES_TO_SET(m_supportedInternalFormatsCopyTexImage,
                      kSupportedInternalFormatsES3);
    m_isWebGL2InternalFormatsCopyTexImageAdded = true;
  }

  if (m_supportedInternalFormatsCopyTexImage.find(internalformat) ==
      m_supportedInternalFormatsCopyTexImage.end()) {
    synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid internalformat");
    return false;
  }

  return true;
}

void WebGLRenderingContextBase::copyTexImage2D(GLenum target,
                                               GLint level,
                                               GLenum internalformat,
                                               GLint x,
                                               GLint y,
                                               GLsizei width,
                                               GLsizei height,
                                               GLint border) {
  if (isContextLost())
    return;
  if (!validateTexture2DBinding("copyTexImage2D", target))
    return;
  if (!validateCopyTexFormat("copyTexImage2D", internalformat))
    return;
  if (!validateSettableTexFormat("copyTexImage2D", internalformat))
    return;
  WebGLFramebuffer* readFramebufferBinding = nullptr;
  if (!validateReadBufferAndGetInfo("copyTexImage2D", readFramebufferBinding))
    return;
  clearIfComposited();
  ScopedDrawingBufferBinder binder(drawingBuffer(), readFramebufferBinding);
  contextGL()->CopyTexImage2D(target, level, internalformat, x, y, width,
                              height, border);
}

void WebGLRenderingContextBase::copyTexSubImage2D(GLenum target,
                                                  GLint level,
                                                  GLint xoffset,
                                                  GLint yoffset,
                                                  GLint x,
                                                  GLint y,
                                                  GLsizei width,
                                                  GLsizei height) {
  if (isContextLost())
    return;
  if (!validateTexture2DBinding("copyTexSubImage2D", target))
    return;
  WebGLFramebuffer* readFramebufferBinding = nullptr;
  if (!validateReadBufferAndGetInfo("copyTexSubImage2D",
                                    readFramebufferBinding))
    return;
  clearIfComposited();
  ScopedDrawingBufferBinder binder(drawingBuffer(), readFramebufferBinding);
  contextGL()->CopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width,
                                 height);
}

WebGLBuffer* WebGLRenderingContextBase::createBuffer() {
  if (isContextLost())
    return nullptr;
  WebGLBuffer* o = WebGLBuffer::create(this);
  addSharedObject(o);
  return o;
}

WebGLFramebuffer* WebGLRenderingContextBase::createFramebuffer() {
  if (isContextLost())
    return nullptr;
  WebGLFramebuffer* o = WebGLFramebuffer::create(this);
  addContextObject(o);
  return o;
}

WebGLTexture* WebGLRenderingContextBase::createTexture() {
  if (isContextLost())
    return nullptr;
  WebGLTexture* o = WebGLTexture::create(this);
  addSharedObject(o);
  return o;
}

WebGLProgram* WebGLRenderingContextBase::createProgram() {
  if (isContextLost())
    return nullptr;
  WebGLProgram* o = WebGLProgram::create(this);
  addSharedObject(o);
  return o;
}

WebGLRenderbuffer* WebGLRenderingContextBase::createRenderbuffer() {
  if (isContextLost())
    return nullptr;
  WebGLRenderbuffer* o = WebGLRenderbuffer::create(this);
  addSharedObject(o);
  return o;
}

void WebGLRenderingContextBase::setBoundVertexArrayObject(
    WebGLVertexArrayObjectBase* arrayObject) {
  if (arrayObject)
    m_boundVertexArrayObject = arrayObject;
  else
    m_boundVertexArrayObject = m_defaultVertexArrayObject;
}

WebGLShader* WebGLRenderingContextBase::createShader(GLenum type) {
  if (isContextLost())
    return nullptr;
  if (type != GL_VERTEX_SHADER && type != GL_FRAGMENT_SHADER) {
    synthesizeGLError(GL_INVALID_ENUM, "createShader", "invalid shader type");
    return nullptr;
  }

  WebGLShader* o = WebGLShader::create(this, type);
  addSharedObject(o);
  return o;
}

void WebGLRenderingContextBase::cullFace(GLenum mode) {
  if (isContextLost())
    return;
  contextGL()->CullFace(mode);
}

bool WebGLRenderingContextBase::deleteObject(WebGLObject* object) {
  if (isContextLost() || !object)
    return false;
  if (!object->validate(contextGroup(), this)) {
    synthesizeGLError(GL_INVALID_OPERATION, "delete",
                      "object does not belong to this context");
    return false;
  }
  if (object->hasObject()) {
    // We need to pass in context here because we want
    // things in this context unbound.
    object->deleteObject(contextGL());
  }
  return true;
}

void WebGLRenderingContextBase::deleteBuffer(WebGLBuffer* buffer) {
  if (!deleteObject(buffer))
    return;
  removeBoundBuffer(buffer);
}

void WebGLRenderingContextBase::deleteFramebuffer(
    WebGLFramebuffer* framebuffer) {
  if (!deleteObject(framebuffer))
    return;
  if (framebuffer == m_framebufferBinding) {
    m_framebufferBinding = nullptr;
    // Have to call drawingBuffer()->bind() here to bind back to internal fbo.
    drawingBuffer()->bind(GL_FRAMEBUFFER);
  }
}

void WebGLRenderingContextBase::deleteProgram(WebGLProgram* program) {
  deleteObject(program);
  // We don't reset m_currentProgram to 0 here because the deletion of the
  // current program is delayed.
}

void WebGLRenderingContextBase::deleteRenderbuffer(
    WebGLRenderbuffer* renderbuffer) {
  if (!deleteObject(renderbuffer))
    return;
  if (renderbuffer == m_renderbufferBinding) {
    m_renderbufferBinding = nullptr;
  }
  if (m_framebufferBinding)
    m_framebufferBinding->removeAttachmentFromBoundFramebuffer(GL_FRAMEBUFFER,
                                                               renderbuffer);
  if (getFramebufferBinding(GL_READ_FRAMEBUFFER))
    getFramebufferBinding(GL_READ_FRAMEBUFFER)
        ->removeAttachmentFromBoundFramebuffer(GL_READ_FRAMEBUFFER,
                                               renderbuffer);
}

void WebGLRenderingContextBase::deleteShader(WebGLShader* shader) {
  deleteObject(shader);
}

void WebGLRenderingContextBase::deleteTexture(WebGLTexture* texture) {
  if (!deleteObject(texture))
    return;

  int maxBoundTextureIndex = -1;
  for (size_t i = 0; i < m_onePlusMaxNonDefaultTextureUnit; ++i) {
    if (texture == m_textureUnits[i].m_texture2DBinding) {
      m_textureUnits[i].m_texture2DBinding = nullptr;
      maxBoundTextureIndex = i;
    }
    if (texture == m_textureUnits[i].m_textureCubeMapBinding) {
      m_textureUnits[i].m_textureCubeMapBinding = nullptr;
      maxBoundTextureIndex = i;
    }
    if (isWebGL2OrHigher()) {
      if (texture == m_textureUnits[i].m_texture3DBinding) {
        m_textureUnits[i].m_texture3DBinding = nullptr;
        maxBoundTextureIndex = i;
      }
      if (texture == m_textureUnits[i].m_texture2DArrayBinding) {
        m_textureUnits[i].m_texture2DArrayBinding = nullptr;
        maxBoundTextureIndex = i;
      }
    }
  }
  if (m_framebufferBinding)
    m_framebufferBinding->removeAttachmentFromBoundFramebuffer(GL_FRAMEBUFFER,
                                                               texture);
  if (getFramebufferBinding(GL_READ_FRAMEBUFFER))
    getFramebufferBinding(GL_READ_FRAMEBUFFER)
        ->removeAttachmentFromBoundFramebuffer(GL_READ_FRAMEBUFFER, texture);

  // If the deleted was bound to the the current maximum index, trace backwards
  // to find the new max texture index.
  if (m_onePlusMaxNonDefaultTextureUnit ==
      static_cast<unsigned long>(maxBoundTextureIndex + 1)) {
    findNewMaxNonDefaultTextureUnit();
  }
}

void WebGLRenderingContextBase::depthFunc(GLenum func) {
  if (isContextLost())
    return;
  contextGL()->DepthFunc(func);
}

void WebGLRenderingContextBase::depthMask(GLboolean flag) {
  if (isContextLost())
    return;
  m_depthMask = flag;
  contextGL()->DepthMask(flag);
}

void WebGLRenderingContextBase::depthRange(GLfloat zNear, GLfloat zFar) {
  if (isContextLost())
    return;
  // Check required by WebGL spec section 6.12
  if (zNear > zFar) {
    synthesizeGLError(GL_INVALID_OPERATION, "depthRange", "zNear > zFar");
    return;
  }
  contextGL()->DepthRangef(zNear, zFar);
}

void WebGLRenderingContextBase::detachShader(WebGLProgram* program,
                                             WebGLShader* shader) {
  if (isContextLost() || !validateWebGLObject("detachShader", program) ||
      !validateWebGLObject("detachShader", shader))
    return;
  if (!program->detachShader(shader)) {
    synthesizeGLError(GL_INVALID_OPERATION, "detachShader",
                      "shader not attached");
    return;
  }
  contextGL()->DetachShader(objectOrZero(program), objectOrZero(shader));
  shader->onDetached(contextGL());
}

void WebGLRenderingContextBase::disable(GLenum cap) {
  if (isContextLost() || !validateCapability("disable", cap))
    return;
  if (cap == GL_STENCIL_TEST) {
    m_stencilEnabled = false;
    applyStencilTest();
    return;
  }
  if (cap == GL_SCISSOR_TEST)
    m_scissorEnabled = false;
  contextGL()->Disable(cap);
}

void WebGLRenderingContextBase::disableVertexAttribArray(GLuint index) {
  if (isContextLost())
    return;
  if (index >= m_maxVertexAttribs) {
    synthesizeGLError(GL_INVALID_VALUE, "disableVertexAttribArray",
                      "index out of range");
    return;
  }

  m_boundVertexArrayObject->setAttribEnabled(index, false);
  contextGL()->DisableVertexAttribArray(index);
}

bool WebGLRenderingContextBase::validateRenderingState(
    const char* functionName) {
  // Command buffer will not error if no program is bound.
  if (!m_currentProgram) {
    synthesizeGLError(GL_INVALID_OPERATION, functionName,
                      "no valid shader program in use");
    return false;
  }

  return true;
}

bool WebGLRenderingContextBase::validateWebGLObject(const char* functionName,
                                                    WebGLObject* object) {
  DCHECK(object);
  if (!object->hasObject()) {
    synthesizeGLError(GL_INVALID_VALUE, functionName,
                      "no object or object deleted");
    return false;
  }
  if (!object->validate(contextGroup(), this)) {
    synthesizeGLError(GL_INVALID_OPERATION, functionName,
                      "object does not belong to this context");
    return false;
  }
  return true;
}

void WebGLRenderingContextBase::drawArrays(GLenum mode,
                                           GLint first,
                                           GLsizei count) {
  if (!validateDrawArrays("drawArrays"))
    return;

  if (!m_boundVertexArrayObject->isAllEnabledAttribBufferBound()) {
    synthesizeGLError(GL_INVALID_OPERATION, "drawArrays",
                      "no buffer is bound to enabled attribute");
    return;
  }

  ScopedRGBEmulationColorMask emulationColorMask(contextGL(), m_colorMask,
                                                 m_drawingBuffer.get());
  clearIfComposited();
  contextGL()->DrawArrays(mode, first, count);
  markContextChanged(CanvasChanged);
}

void WebGLRenderingContextBase::drawElements(GLenum mode,
                                             GLsizei count,
                                             GLenum type,
                                             long long offset) {
  if (!validateDrawElements("drawElements", type, offset))
    return;

  if (!m_boundVertexArrayObject->isAllEnabledAttribBufferBound()) {
    synthesizeGLError(GL_INVALID_OPERATION, "drawElements",
                      "no buffer is bound to enabled attribute");
    return;
  }

  ScopedRGBEmulationColorMask emulationColorMask(contextGL(), m_colorMask,
                                                 m_drawingBuffer.get());
  clearIfComposited();
  contextGL()->DrawElements(
      mode, count, type,
      reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
  markContextChanged(CanvasChanged);
}

void WebGLRenderingContextBase::drawArraysInstancedANGLE(GLenum mode,
                                                         GLint first,
                                                         GLsizei count,
                                                         GLsizei primcount) {
  if (!validateDrawArrays("drawArraysInstancedANGLE"))
    return;

  if (!m_boundVertexArrayObject->isAllEnabledAttribBufferBound()) {
    synthesizeGLError(GL_INVALID_OPERATION, "drawArraysInstancedANGLE",
                      "no buffer is bound to enabled attribute");
    return;
  }

  ScopedRGBEmulationColorMask emulationColorMask(contextGL(), m_colorMask,
                                                 m_drawingBuffer.get());
  clearIfComposited();
  contextGL()->DrawArraysInstancedANGLE(mode, first, count, primcount);
  markContextChanged(CanvasChanged);
}

void WebGLRenderingContextBase::drawElementsInstancedANGLE(GLenum mode,
                                                           GLsizei count,
                                                           GLenum type,
                                                           long long offset,
                                                           GLsizei primcount) {
  if (!validateDrawElements("drawElementsInstancedANGLE", type, offset))
    return;

  if (!m_boundVertexArrayObject->isAllEnabledAttribBufferBound()) {
    synthesizeGLError(GL_INVALID_OPERATION, "drawElementsInstancedANGLE",
                      "no buffer is bound to enabled attribute");
    return;
  }

  ScopedRGBEmulationColorMask emulationColorMask(contextGL(), m_colorMask,
                                                 m_drawingBuffer.get());
  clearIfComposited();
  contextGL()->DrawElementsInstancedANGLE(
      mode, count, type, reinterpret_cast<void*>(static_cast<intptr_t>(offset)),
      primcount);
  markContextChanged(CanvasChanged);
}

void WebGLRenderingContextBase::enable(GLenum cap) {
  if (isContextLost() || !validateCapability("enable", cap))
    return;
  if (cap == GL_STENCIL_TEST) {
    m_stencilEnabled = true;
    applyStencilTest();
    return;
  }
  if (cap == GL_SCISSOR_TEST)
    m_scissorEnabled = true;
  contextGL()->Enable(cap);
}

void WebGLRenderingContextBase::enableVertexAttribArray(GLuint index) {
  if (isContextLost())
    return;
  if (index >= m_maxVertexAttribs) {
    synthesizeGLError(GL_INVALID_VALUE, "enableVertexAttribArray",
                      "index out of range");
    return;
  }

  m_boundVertexArrayObject->setAttribEnabled(index, true);
  contextGL()->EnableVertexAttribArray(index);
}

void WebGLRenderingContextBase::finish() {
  if (isContextLost())
    return;
  contextGL()->Flush();  // Intentionally a flush, not a finish.
}

void WebGLRenderingContextBase::flush() {
  if (isContextLost())
    return;
  contextGL()->Flush();
}

void WebGLRenderingContextBase::framebufferRenderbuffer(
    GLenum target,
    GLenum attachment,
    GLenum renderbuffertarget,
    WebGLRenderbuffer* buffer) {
  if (isContextLost() ||
      !validateFramebufferFuncParameters("framebufferRenderbuffer", target,
                                         attachment))
    return;
  if (renderbuffertarget != GL_RENDERBUFFER) {
    synthesizeGLError(GL_INVALID_ENUM, "framebufferRenderbuffer",
                      "invalid target");
    return;
  }
  if (buffer && !buffer->validate(contextGroup(), this)) {
    synthesizeGLError(GL_INVALID_OPERATION, "framebufferRenderbuffer",
                      "no buffer or buffer not from this context");
    return;
  }
  // Don't allow the default framebuffer to be mutated; all current
  // implementations use an FBO internally in place of the default
  // FBO.
  WebGLFramebuffer* framebufferBinding = getFramebufferBinding(target);
  if (!framebufferBinding || !framebufferBinding->object()) {
    synthesizeGLError(GL_INVALID_OPERATION, "framebufferRenderbuffer",
                      "no framebuffer bound");
    return;
  }
  framebufferBinding->setAttachmentForBoundFramebuffer(target, attachment,
                                                       buffer);
  applyStencilTest();
}

void WebGLRenderingContextBase::framebufferTexture2D(GLenum target,
                                                     GLenum attachment,
                                                     GLenum textarget,
                                                     WebGLTexture* texture,
                                                     GLint level) {
  if (isContextLost() ||
      !validateFramebufferFuncParameters("framebufferTexture2D", target,
                                         attachment))
    return;
  if (texture && !texture->validate(contextGroup(), this)) {
    synthesizeGLError(GL_INVALID_OPERATION, "framebufferTexture2D",
                      "no texture or texture not from this context");
    return;
  }
  // Don't allow the default framebuffer to be mutated; all current
  // implementations use an FBO internally in place of the default
  // FBO.
  WebGLFramebuffer* framebufferBinding = getFramebufferBinding(target);
  if (!framebufferBinding || !framebufferBinding->object()) {
    synthesizeGLError(GL_INVALID_OPERATION, "framebufferTexture2D",
                      "no framebuffer bound");
    return;
  }
  framebufferBinding->setAttachmentForBoundFramebuffer(
      target, attachment, textarget, texture, level, 0);
  applyStencilTest();
}

void WebGLRenderingContextBase::frontFace(GLenum mode) {
  if (isContextLost())
    return;
  contextGL()->FrontFace(mode);
}

void WebGLRenderingContextBase::generateMipmap(GLenum target) {
  if (isContextLost())
    return;
  if (!validateTextureBinding("generateMipmap", target))
    return;
  contextGL()->GenerateMipmap(target);
}

WebGLActiveInfo* WebGLRenderingContextBase::getActiveAttrib(
    WebGLProgram* program,
    GLuint index) {
  if (isContextLost() || !validateWebGLObject("getActiveAttrib", program))
    return nullptr;
  GLuint programId = objectNonZero(program);
  GLint maxNameLength = -1;
  contextGL()->GetProgramiv(programId, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH,
                            &maxNameLength);
  if (maxNameLength < 0)
    return nullptr;
  if (maxNameLength == 0) {
    synthesizeGLError(GL_INVALID_VALUE, "getActiveAttrib",
                      "no active attributes exist");
    return nullptr;
  }
  LChar* namePtr;
  RefPtr<StringImpl> nameImpl =
      StringImpl::createUninitialized(maxNameLength, namePtr);
  GLsizei length = 0;
  GLint size = -1;
  GLenum type = 0;
  contextGL()->GetActiveAttrib(programId, index, maxNameLength, &length, &size,
                               &type, reinterpret_cast<GLchar*>(namePtr));
  if (size < 0)
    return nullptr;
  return WebGLActiveInfo::create(nameImpl->substring(0, length), type, size);
}

WebGLActiveInfo* WebGLRenderingContextBase::getActiveUniform(
    WebGLProgram* program,
    GLuint index) {
  if (isContextLost() || !validateWebGLObject("getActiveUniform", program))
    return nullptr;
  GLuint programId = objectNonZero(program);
  GLint maxNameLength = -1;
  contextGL()->GetProgramiv(programId, GL_ACTIVE_UNIFORM_MAX_LENGTH,
                            &maxNameLength);
  if (maxNameLength < 0)
    return nullptr;
  if (maxNameLength == 0) {
    synthesizeGLError(GL_INVALID_VALUE, "getActiveUniform",
                      "no active uniforms exist");
    return nullptr;
  }
  LChar* namePtr;
  RefPtr<StringImpl> nameImpl =
      StringImpl::createUninitialized(maxNameLength, namePtr);
  GLsizei length = 0;
  GLint size = -1;
  GLenum type = 0;
  contextGL()->GetActiveUniform(programId, index, maxNameLength, &length, &size,
                                &type, reinterpret_cast<GLchar*>(namePtr));
  if (size < 0)
    return nullptr;
  return WebGLActiveInfo::create(nameImpl->substring(0, length), type, size);
}

Nullable<HeapVector<Member<WebGLShader>>>
WebGLRenderingContextBase::getAttachedShaders(WebGLProgram* program) {
  if (isContextLost() || !validateWebGLObject("getAttachedShaders", program))
    return nullptr;

  HeapVector<Member<WebGLShader>> shaderObjects;
  const GLenum shaderType[] = {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER};
  for (unsigned i = 0; i < sizeof(shaderType) / sizeof(GLenum); ++i) {
    WebGLShader* shader = program->getAttachedShader(shaderType[i]);
    if (shader)
      shaderObjects.append(shader);
  }
  return shaderObjects;
}

GLint WebGLRenderingContextBase::getAttribLocation(WebGLProgram* program,
                                                   const String& name) {
  if (isContextLost() || !validateWebGLObject("getAttribLocation", program))
    return -1;
  if (!validateLocationLength("getAttribLocation", name))
    return -1;
  if (!validateString("getAttribLocation", name))
    return -1;
  if (isPrefixReserved(name))
    return -1;
  if (!program->linkStatus(this)) {
    synthesizeGLError(GL_INVALID_OPERATION, "getAttribLocation",
                      "program not linked");
    return 0;
  }
  return contextGL()->GetAttribLocation(objectOrZero(program),
                                        name.utf8().data());
}

bool WebGLRenderingContextBase::validateBufferTarget(const char* functionName,
                                                     GLenum target) {
  switch (target) {
    case GL_ARRAY_BUFFER:
    case GL_ELEMENT_ARRAY_BUFFER:
      return true;
    default:
      synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid target");
      return false;
  }
}

ScriptValue WebGLRenderingContextBase::getBufferParameter(
    ScriptState* scriptState,
    GLenum target,
    GLenum pname) {
  if (isContextLost() || !validateBufferTarget("getBufferParameter", target))
    return ScriptValue::createNull(scriptState);

  switch (pname) {
    case GL_BUFFER_USAGE: {
      GLint value = 0;
      contextGL()->GetBufferParameteriv(target, pname, &value);
      return WebGLAny(scriptState, static_cast<unsigned>(value));
    }
    case GL_BUFFER_SIZE: {
      GLint value = 0;
      contextGL()->GetBufferParameteriv(target, pname, &value);
      if (!isWebGL2OrHigher())
        return WebGLAny(scriptState, value);
      return WebGLAny(scriptState, static_cast<GLint64>(value));
    }
    default:
      synthesizeGLError(GL_INVALID_ENUM, "getBufferParameter",
                        "invalid parameter name");
      return ScriptValue::createNull(scriptState);
  }
}

void WebGLRenderingContextBase::getContextAttributes(
    Nullable<WebGLContextAttributes>& result) {
  if (isContextLost())
    return;
  result.set(toWebGLContextAttributes(creationAttributes()));
  // Some requested attributes may not be honored, so we need to query the
  // underlying context/drawing buffer and adjust accordingly.
  if (creationAttributes().depth() && !drawingBuffer()->hasDepthBuffer())
    result.get().setDepth(false);
  if (creationAttributes().stencil() && !drawingBuffer()->hasStencilBuffer())
    result.get().setStencil(false);
  result.get().setAntialias(drawingBuffer()->multisample());
}

GLenum WebGLRenderingContextBase::getError() {
  if (!m_lostContextErrors.isEmpty()) {
    GLenum error = m_lostContextErrors.first();
    m_lostContextErrors.remove(0);
    return error;
  }

  if (isContextLost())
    return GL_NO_ERROR;

  if (!m_syntheticErrors.isEmpty()) {
    GLenum error = m_syntheticErrors.first();
    m_syntheticErrors.remove(0);
    return error;
  }

  return contextGL()->GetError();
}

const char* const* WebGLRenderingContextBase::ExtensionTracker::prefixes()
    const {
  static const char* const unprefixed[] = {
      "", 0,
  };
  return m_prefixes ? m_prefixes : unprefixed;
}

bool WebGLRenderingContextBase::ExtensionTracker::matchesNameWithPrefixes(
    const String& name) const {
  const char* const* prefixSet = prefixes();
  for (; *prefixSet; ++prefixSet) {
    String prefixedName = String(*prefixSet) + extensionName();
    if (equalIgnoringCase(prefixedName, name)) {
      return true;
    }
  }
  return false;
}

bool WebGLRenderingContextBase::extensionSupportedAndAllowed(
    const ExtensionTracker* tracker) {
  if (tracker->draft() &&
      !RuntimeEnabledFeatures::webGLDraftExtensionsEnabled())
    return false;
  if (!tracker->supported(this))
    return false;
  return true;
}

ScriptValue WebGLRenderingContextBase::getExtension(ScriptState* scriptState,
                                                    const String& name) {
  WebGLExtension* extension = nullptr;

  if (!isContextLost()) {
    for (size_t i = 0; i < m_extensions.size(); ++i) {
      ExtensionTracker* tracker = m_extensions[i];
      if (tracker->matchesNameWithPrefixes(name)) {
        if (extensionSupportedAndAllowed(tracker)) {
          extension = tracker->getExtension(this);
          if (extension) {
            if (!m_extensionEnabled[extension->name()]) {
              m_extensionEnabled[extension->name()] = true;
            }
          }
        }
        break;
      }
    }
  }

  v8::Local<v8::Value> wrappedExtension =
      toV8(extension, scriptState->context()->Global(), scriptState->isolate());

  return ScriptValue(scriptState, wrappedExtension);
}

ScriptValue WebGLRenderingContextBase::getFramebufferAttachmentParameter(
    ScriptState* scriptState,
    GLenum target,
    GLenum attachment,
    GLenum pname) {
  if (isContextLost() ||
      !validateFramebufferFuncParameters("getFramebufferAttachmentParameter",
                                         target, attachment))
    return ScriptValue::createNull(scriptState);

  if (!m_framebufferBinding || !m_framebufferBinding->object()) {
    synthesizeGLError(GL_INVALID_OPERATION, "getFramebufferAttachmentParameter",
                      "no framebuffer bound");
    return ScriptValue::createNull(scriptState);
  }

  WebGLSharedObject* attachmentObject =
      m_framebufferBinding->getAttachmentObject(attachment);
  if (!attachmentObject) {
    if (pname == GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE)
      return WebGLAny(scriptState, GL_NONE);
    // OpenGL ES 2.0 specifies INVALID_ENUM in this case, while desktop GL
    // specifies INVALID_OPERATION.
    synthesizeGLError(GL_INVALID_ENUM, "getFramebufferAttachmentParameter",
                      "invalid parameter name");
    return ScriptValue::createNull(scriptState);
  }

  ASSERT(attachmentObject->isTexture() || attachmentObject->isRenderbuffer());
  if (attachmentObject->isTexture()) {
    switch (pname) {
      case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
        return WebGLAny(scriptState, GL_TEXTURE);
      case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
        return WebGLAny(scriptState, attachmentObject);
      case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
      case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE: {
        GLint value = 0;
        contextGL()->GetFramebufferAttachmentParameteriv(target, attachment,
                                                         pname, &value);
        return WebGLAny(scriptState, value);
      }
      case GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT:
        if (extensionEnabled(EXTsRGBName)) {
          GLint value = 0;
          contextGL()->GetFramebufferAttachmentParameteriv(target, attachment,
                                                           pname, &value);
          return WebGLAny(scriptState, static_cast<unsigned>(value));
        }
        synthesizeGLError(GL_INVALID_ENUM, "getFramebufferAttachmentParameter",
                          "invalid parameter name for renderbuffer attachment");
        return ScriptValue::createNull(scriptState);
      default:
        synthesizeGLError(GL_INVALID_ENUM, "getFramebufferAttachmentParameter",
                          "invalid parameter name for texture attachment");
        return ScriptValue::createNull(scriptState);
    }
  } else {
    switch (pname) {
      case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
        return WebGLAny(scriptState, GL_RENDERBUFFER);
      case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
        return WebGLAny(scriptState, attachmentObject);
      case GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT:
        if (extensionEnabled(EXTsRGBName)) {
          GLint value = 0;
          contextGL()->GetFramebufferAttachmentParameteriv(target, attachment,
                                                           pname, &value);
          return WebGLAny(scriptState, value);
        }
        synthesizeGLError(GL_INVALID_ENUM, "getFramebufferAttachmentParameter",
                          "invalid parameter name for renderbuffer attachment");
        return ScriptValue::createNull(scriptState);
      default:
        synthesizeGLError(GL_INVALID_ENUM, "getFramebufferAttachmentParameter",
                          "invalid parameter name for renderbuffer attachment");
        return ScriptValue::createNull(scriptState);
    }
  }
}

ScriptValue WebGLRenderingContextBase::getParameter(ScriptState* scriptState,
                                                    GLenum pname) {
  if (isContextLost())
    return ScriptValue::createNull(scriptState);
  const int intZero = 0;
  switch (pname) {
    case GL_ACTIVE_TEXTURE:
      return getUnsignedIntParameter(scriptState, pname);
    case GL_ALIASED_LINE_WIDTH_RANGE:
      return getWebGLFloatArrayParameter(scriptState, pname);
    case GL_ALIASED_POINT_SIZE_RANGE:
      return getWebGLFloatArrayParameter(scriptState, pname);
    case GL_ALPHA_BITS:
      if (m_drawingBuffer->requiresAlphaChannelToBePreserved())
        return WebGLAny(scriptState, 0);
      return getIntParameter(scriptState, pname);
    case GL_ARRAY_BUFFER_BINDING:
      return WebGLAny(scriptState, m_boundArrayBuffer.get());
    case GL_BLEND:
      return getBooleanParameter(scriptState, pname);
    case GL_BLEND_COLOR:
      return getWebGLFloatArrayParameter(scriptState, pname);
    case GL_BLEND_DST_ALPHA:
      return getUnsignedIntParameter(scriptState, pname);
    case GL_BLEND_DST_RGB:
      return getUnsignedIntParameter(scriptState, pname);
    case GL_BLEND_EQUATION_ALPHA:
      return getUnsignedIntParameter(scriptState, pname);
    case GL_BLEND_EQUATION_RGB:
      return getUnsignedIntParameter(scriptState, pname);
    case GL_BLEND_SRC_ALPHA:
      return getUnsignedIntParameter(scriptState, pname);
    case GL_BLEND_SRC_RGB:
      return getUnsignedIntParameter(scriptState, pname);
    case GL_BLUE_BITS:
      return getIntParameter(scriptState, pname);
    case GL_COLOR_CLEAR_VALUE:
      return getWebGLFloatArrayParameter(scriptState, pname);
    case GL_COLOR_WRITEMASK:
      return getBooleanArrayParameter(scriptState, pname);
    case GL_COMPRESSED_TEXTURE_FORMATS:
      return WebGLAny(scriptState, DOMUint32Array::create(
                                       m_compressedTextureFormats.data(),
                                       m_compressedTextureFormats.size()));
    case GL_CULL_FACE:
      return getBooleanParameter(scriptState, pname);
    case GL_CULL_FACE_MODE:
      return getUnsignedIntParameter(scriptState, pname);
    case GL_CURRENT_PROGRAM:
      return WebGLAny(scriptState, m_currentProgram.get());
    case GL_DEPTH_BITS:
      if (!m_framebufferBinding && !creationAttributes().depth())
        return WebGLAny(scriptState, intZero);
      return getIntParameter(scriptState, pname);
    case GL_DEPTH_CLEAR_VALUE:
      return getFloatParameter(scriptState, pname);
    case GL_DEPTH_FUNC:
      return getUnsignedIntParameter(scriptState, pname);
    case GL_DEPTH_RANGE:
      return getWebGLFloatArrayParameter(scriptState, pname);
    case GL_DEPTH_TEST:
      return getBooleanParameter(scriptState, pname);
    case GL_DEPTH_WRITEMASK:
      return getBooleanParameter(scriptState, pname);
    case GL_DITHER:
      return getBooleanParameter(scriptState, pname);
    case GL_ELEMENT_ARRAY_BUFFER_BINDING:
      return WebGLAny(scriptState,
                      m_boundVertexArrayObject->boundElementArrayBuffer());
    case GL_FRAMEBUFFER_BINDING:
      return WebGLAny(scriptState, m_framebufferBinding.get());
    case GL_FRONT_FACE:
      return getUnsignedIntParameter(scriptState, pname);
    case GL_GENERATE_MIPMAP_HINT:
      return getUnsignedIntParameter(scriptState, pname);
    case GL_GREEN_BITS:
      return getIntParameter(scriptState, pname);
    case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
      return getIntParameter(scriptState, pname);
    case GL_IMPLEMENTATION_COLOR_READ_TYPE:
      return getIntParameter(scriptState, pname);
    case GL_LINE_WIDTH:
      return getFloatParameter(scriptState, pname);
    case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
      return getIntParameter(scriptState, pname);
    case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
      return getIntParameter(scriptState, pname);
    case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
      return getIntParameter(scriptState, pname);
    case GL_MAX_RENDERBUFFER_SIZE:
      return getIntParameter(scriptState, pname);
    case GL_MAX_TEXTURE_IMAGE_UNITS:
      return getIntParameter(scriptState, pname);
    case GL_MAX_TEXTURE_SIZE:
      return getIntParameter(scriptState, pname);
    case GL_MAX_VARYING_VECTORS:
      return getIntParameter(scriptState, pname);
    case GL_MAX_VERTEX_ATTRIBS:
      return getIntParameter(scriptState, pname);
    case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
      return getIntParameter(scriptState, pname);
    case GL_MAX_VERTEX_UNIFORM_VECTORS:
      return getIntParameter(scriptState, pname);
    case GL_MAX_VIEWPORT_DIMS:
      return getWebGLIntArrayParameter(scriptState, pname);
    case GL_NUM_SHADER_BINARY_FORMATS:
      // FIXME: should we always return 0 for this?
      return getIntParameter(scriptState, pname);
    case GL_PACK_ALIGNMENT:
      return getIntParameter(scriptState, pname);
    case GL_POLYGON_OFFSET_FACTOR:
      return getFloatParameter(scriptState, pname);
    case GL_POLYGON_OFFSET_FILL:
      return getBooleanParameter(scriptState, pname);
    case GL_POLYGON_OFFSET_UNITS:
      return getFloatParameter(scriptState, pname);
    case GL_RED_BITS:
      return getIntParameter(scriptState, pname);
    case GL_RENDERBUFFER_BINDING:
      return WebGLAny(scriptState, m_renderbufferBinding.get());
    case GL_RENDERER:
      return WebGLAny(scriptState, String("WebKit WebGL"));
    case GL_SAMPLE_BUFFERS:
      return getIntParameter(scriptState, pname);
    case GL_SAMPLE_COVERAGE_INVERT:
      return getBooleanParameter(scriptState, pname);
    case GL_SAMPLE_COVERAGE_VALUE:
      return getFloatParameter(scriptState, pname);
    case GL_SAMPLES:
      return getIntParameter(scriptState, pname);
    case GL_SCISSOR_BOX:
      return getWebGLIntArrayParameter(scriptState, pname);
    case GL_SCISSOR_TEST:
      return getBooleanParameter(scriptState, pname);
    case GL_SHADING_LANGUAGE_VERSION:
      return WebGLAny(
          scriptState,
          "WebGL GLSL ES 1.0 (" +
              String(contextGL()->GetString(GL_SHADING_LANGUAGE_VERSION)) +
              ")");
    case GL_STENCIL_BACK_FAIL:
      return getUnsignedIntParameter(scriptState, pname);
    case GL_STENCIL_BACK_FUNC:
      return getUnsignedIntParameter(scriptState, pname);
    case GL_STENCIL_BACK_PASS_DEPTH_FAIL:
      return getUnsignedIntParameter(scriptState, pname);
    case GL_STENCIL_BACK_PASS_DEPTH_PASS:
      return getUnsignedIntParameter(scriptState, pname);
    case GL_STENCIL_BACK_REF:
      return getIntParameter(scriptState, pname);
    case GL_STENCIL_BACK_VALUE_MASK:
      return getUnsignedIntParameter(scriptState, pname);
    case GL_STENCIL_BACK_WRITEMASK:
      return getUnsignedIntParameter(scriptState, pname);
    case GL_STENCIL_BITS:
      if (!m_framebufferBinding && !creationAttributes().stencil())
        return WebGLAny(scriptState, intZero);
      return getIntParameter(scriptState, pname);
    case GL_STENCIL_CLEAR_VALUE:
      return getIntParameter(scriptState, pname);
    case GL_STENCIL_FAIL:
      return getUnsignedIntParameter(scriptState, pname);
    case GL_STENCIL_FUNC:
      return getUnsignedIntParameter(scriptState, pname);
    case GL_STENCIL_PASS_DEPTH_FAIL:
      return getUnsignedIntParameter(scriptState, pname);
    case GL_STENCIL_PASS_DEPTH_PASS:
      return getUnsignedIntParameter(scriptState, pname);
    case GL_STENCIL_REF:
      return getIntParameter(scriptState, pname);
    case GL_STENCIL_TEST:
      return getBooleanParameter(scriptState, pname);
    case GL_STENCIL_VALUE_MASK:
      return getUnsignedIntParameter(scriptState, pname);
    case GL_STENCIL_WRITEMASK:
      return getUnsignedIntParameter(scriptState, pname);
    case GL_SUBPIXEL_BITS:
      return getIntParameter(scriptState, pname);
    case GL_TEXTURE_BINDING_2D:
      return WebGLAny(
          scriptState,
          m_textureUnits[m_activeTextureUnit].m_texture2DBinding.get());
    case GL_TEXTURE_BINDING_CUBE_MAP:
      return WebGLAny(
          scriptState,
          m_textureUnits[m_activeTextureUnit].m_textureCubeMapBinding.get());
    case GL_UNPACK_ALIGNMENT:
      return getIntParameter(scriptState, pname);
    case GC3D_UNPACK_FLIP_Y_WEBGL:
      return WebGLAny(scriptState, m_unpackFlipY);
    case GC3D_UNPACK_PREMULTIPLY_ALPHA_WEBGL:
      return WebGLAny(scriptState, m_unpackPremultiplyAlpha);
    case GC3D_UNPACK_COLORSPACE_CONVERSION_WEBGL:
      return WebGLAny(scriptState, m_unpackColorspaceConversion);
    case GL_VENDOR:
      return WebGLAny(scriptState, String("WebKit"));
    case GL_VERSION:
      return WebGLAny(
          scriptState,
          "WebGL 1.0 (" + String(contextGL()->GetString(GL_VERSION)) + ")");
    case GL_VIEWPORT:
      return getWebGLIntArrayParameter(scriptState, pname);
    case GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES:  // OES_standard_derivatives
      if (extensionEnabled(OESStandardDerivativesName) || isWebGL2OrHigher())
        return getUnsignedIntParameter(scriptState,
                                       GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES);
      synthesizeGLError(
          GL_INVALID_ENUM, "getParameter",
          "invalid parameter name, OES_standard_derivatives not enabled");
      return ScriptValue::createNull(scriptState);
    case WebGLDebugRendererInfo::kUnmaskedRendererWebgl:
      if (extensionEnabled(WebGLDebugRendererInfoName))
        return WebGLAny(scriptState,
                        String(contextGL()->GetString(GL_RENDERER)));
      synthesizeGLError(
          GL_INVALID_ENUM, "getParameter",
          "invalid parameter name, WEBGL_debug_renderer_info not enabled");
      return ScriptValue::createNull(scriptState);
    case WebGLDebugRendererInfo::kUnmaskedVendorWebgl:
      if (extensionEnabled(WebGLDebugRendererInfoName))
        return WebGLAny(scriptState, String(contextGL()->GetString(GL_VENDOR)));
      synthesizeGLError(
          GL_INVALID_ENUM, "getParameter",
          "invalid parameter name, WEBGL_debug_renderer_info not enabled");
      return ScriptValue::createNull(scriptState);
    case GL_VERTEX_ARRAY_BINDING_OES:  // OES_vertex_array_object
      if (extensionEnabled(OESVertexArrayObjectName) || isWebGL2OrHigher()) {
        if (!m_boundVertexArrayObject->isDefaultObject())
          return WebGLAny(scriptState, m_boundVertexArrayObject.get());
        return ScriptValue::createNull(scriptState);
      }
      synthesizeGLError(
          GL_INVALID_ENUM, "getParameter",
          "invalid parameter name, OES_vertex_array_object not enabled");
      return ScriptValue::createNull(scriptState);
    case GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT:  // EXT_texture_filter_anisotropic
      if (extensionEnabled(EXTTextureFilterAnisotropicName))
        return getUnsignedIntParameter(scriptState,
                                       GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT);
      synthesizeGLError(
          GL_INVALID_ENUM, "getParameter",
          "invalid parameter name, EXT_texture_filter_anisotropic not enabled");
      return ScriptValue::createNull(scriptState);
    case GL_MAX_COLOR_ATTACHMENTS_EXT:  // EXT_draw_buffers BEGIN
      if (extensionEnabled(WebGLDrawBuffersName) || isWebGL2OrHigher())
        return WebGLAny(scriptState, maxColorAttachments());
      synthesizeGLError(
          GL_INVALID_ENUM, "getParameter",
          "invalid parameter name, WEBGL_draw_buffers not enabled");
      return ScriptValue::createNull(scriptState);
    case GL_MAX_DRAW_BUFFERS_EXT:
      if (extensionEnabled(WebGLDrawBuffersName) || isWebGL2OrHigher())
        return WebGLAny(scriptState, maxDrawBuffers());
      synthesizeGLError(
          GL_INVALID_ENUM, "getParameter",
          "invalid parameter name, WEBGL_draw_buffers not enabled");
      return ScriptValue::createNull(scriptState);
    case GL_TIMESTAMP_EXT:
      if (extensionEnabled(EXTDisjointTimerQueryName))
        return WebGLAny(scriptState, 0);
      synthesizeGLError(
          GL_INVALID_ENUM, "getParameter",
          "invalid parameter name, EXT_disjoint_timer_query not enabled");
      return ScriptValue::createNull(scriptState);
    case GL_GPU_DISJOINT_EXT:
      if (extensionEnabled(EXTDisjointTimerQueryName))
        return getBooleanParameter(scriptState, GL_GPU_DISJOINT_EXT);
      synthesizeGLError(
          GL_INVALID_ENUM, "getParameter",
          "invalid parameter name, EXT_disjoint_timer_query not enabled");
      return ScriptValue::createNull(scriptState);

    default:
      if ((extensionEnabled(WebGLDrawBuffersName) || isWebGL2OrHigher()) &&
          pname >= GL_DRAW_BUFFER0_EXT &&
          pname < static_cast<GLenum>(GL_DRAW_BUFFER0_EXT + maxDrawBuffers())) {
        GLint value = GL_NONE;
        if (m_framebufferBinding)
          value = m_framebufferBinding->getDrawBuffer(pname);
        else  // emulated backbuffer
          value = m_backDrawBuffer;
        return WebGLAny(scriptState, value);
      }
      synthesizeGLError(GL_INVALID_ENUM, "getParameter",
                        "invalid parameter name");
      return ScriptValue::createNull(scriptState);
  }
}

ScriptValue WebGLRenderingContextBase::getProgramParameter(
    ScriptState* scriptState,
    WebGLProgram* program,
    GLenum pname) {
  if (isContextLost() || !validateWebGLObject("getProgramParameter", program))
    return ScriptValue::createNull(scriptState);

  GLint value = 0;
  switch (pname) {
    case GL_DELETE_STATUS:
      return WebGLAny(scriptState, program->isDeleted());
    case GL_VALIDATE_STATUS:
      contextGL()->GetProgramiv(objectOrZero(program), pname, &value);
      return WebGLAny(scriptState, static_cast<bool>(value));
    case GL_LINK_STATUS:
      return WebGLAny(scriptState, program->linkStatus(this));
    case GL_ACTIVE_UNIFORM_BLOCKS:
    case GL_TRANSFORM_FEEDBACK_VARYINGS:
      if (!isWebGL2OrHigher()) {
        synthesizeGLError(GL_INVALID_ENUM, "getProgramParameter",
                          "invalid parameter name");
        return ScriptValue::createNull(scriptState);
      }
    case GL_ATTACHED_SHADERS:
    case GL_ACTIVE_ATTRIBUTES:
    case GL_ACTIVE_UNIFORMS:
      contextGL()->GetProgramiv(objectOrZero(program), pname, &value);
      return WebGLAny(scriptState, value);
    case GL_TRANSFORM_FEEDBACK_BUFFER_MODE:
      if (isWebGL2OrHigher()) {
        contextGL()->GetProgramiv(objectOrZero(program), pname, &value);
        return WebGLAny(scriptState, static_cast<unsigned>(value));
      }
    default:
      synthesizeGLError(GL_INVALID_ENUM, "getProgramParameter",
                        "invalid parameter name");
      return ScriptValue::createNull(scriptState);
  }
}

String WebGLRenderingContextBase::getProgramInfoLog(WebGLProgram* program) {
  if (isContextLost() || !validateWebGLObject("getProgramInfoLog", program))
    return String();
  GLStringQuery query(contextGL());
  return query.Run<GLStringQuery::ProgramInfoLog>(objectNonZero(program));
}

ScriptValue WebGLRenderingContextBase::getRenderbufferParameter(
    ScriptState* scriptState,
    GLenum target,
    GLenum pname) {
  if (isContextLost())
    return ScriptValue::createNull(scriptState);
  if (target != GL_RENDERBUFFER) {
    synthesizeGLError(GL_INVALID_ENUM, "getRenderbufferParameter",
                      "invalid target");
    return ScriptValue::createNull(scriptState);
  }
  if (!m_renderbufferBinding || !m_renderbufferBinding->object()) {
    synthesizeGLError(GL_INVALID_OPERATION, "getRenderbufferParameter",
                      "no renderbuffer bound");
    return ScriptValue::createNull(scriptState);
  }

  GLint value = 0;
  switch (pname) {
    case GL_RENDERBUFFER_SAMPLES:
      if (!isWebGL2OrHigher()) {
        synthesizeGLError(GL_INVALID_ENUM, "getRenderbufferParameter",
                          "invalid parameter name");
        return ScriptValue::createNull(scriptState);
      }
    case GL_RENDERBUFFER_WIDTH:
    case GL_RENDERBUFFER_HEIGHT:
    case GL_RENDERBUFFER_RED_SIZE:
    case GL_RENDERBUFFER_GREEN_SIZE:
    case GL_RENDERBUFFER_BLUE_SIZE:
    case GL_RENDERBUFFER_ALPHA_SIZE:
    case GL_RENDERBUFFER_DEPTH_SIZE:
      contextGL()->GetRenderbufferParameteriv(target, pname, &value);
      return WebGLAny(scriptState, value);
    case GL_RENDERBUFFER_STENCIL_SIZE:
      contextGL()->GetRenderbufferParameteriv(target, pname, &value);
      return WebGLAny(scriptState, value);
    case GL_RENDERBUFFER_INTERNAL_FORMAT:
      return WebGLAny(scriptState, m_renderbufferBinding->internalFormat());
    default:
      synthesizeGLError(GL_INVALID_ENUM, "getRenderbufferParameter",
                        "invalid parameter name");
      return ScriptValue::createNull(scriptState);
  }
}

ScriptValue WebGLRenderingContextBase::getShaderParameter(
    ScriptState* scriptState,
    WebGLShader* shader,
    GLenum pname) {
  if (isContextLost() || !validateWebGLObject("getShaderParameter", shader))
    return ScriptValue::createNull(scriptState);
  GLint value = 0;
  switch (pname) {
    case GL_DELETE_STATUS:
      return WebGLAny(scriptState, shader->isDeleted());
    case GL_COMPILE_STATUS:
      contextGL()->GetShaderiv(objectOrZero(shader), pname, &value);
      return WebGLAny(scriptState, static_cast<bool>(value));
    case GL_SHADER_TYPE:
      contextGL()->GetShaderiv(objectOrZero(shader), pname, &value);
      return WebGLAny(scriptState, static_cast<unsigned>(value));
    default:
      synthesizeGLError(GL_INVALID_ENUM, "getShaderParameter",
                        "invalid parameter name");
      return ScriptValue::createNull(scriptState);
  }
}

String WebGLRenderingContextBase::getShaderInfoLog(WebGLShader* shader) {
  if (isContextLost() || !validateWebGLObject("getShaderInfoLog", shader))
    return String();
  GLStringQuery query(contextGL());
  return query.Run<GLStringQuery::ShaderInfoLog>(objectNonZero(shader));
}

WebGLShaderPrecisionFormat* WebGLRenderingContextBase::getShaderPrecisionFormat(
    GLenum shaderType,
    GLenum precisionType) {
  if (isContextLost())
    return nullptr;
  switch (shaderType) {
    case GL_VERTEX_SHADER:
    case GL_FRAGMENT_SHADER:
      break;
    default:
      synthesizeGLError(GL_INVALID_ENUM, "getShaderPrecisionFormat",
                        "invalid shader type");
      return nullptr;
  }
  switch (precisionType) {
    case GL_LOW_FLOAT:
    case GL_MEDIUM_FLOAT:
    case GL_HIGH_FLOAT:
    case GL_LOW_INT:
    case GL_MEDIUM_INT:
    case GL_HIGH_INT:
      break;
    default:
      synthesizeGLError(GL_INVALID_ENUM, "getShaderPrecisionFormat",
                        "invalid precision type");
      return nullptr;
  }

  GLint range[2] = {0, 0};
  GLint precision = 0;
  contextGL()->GetShaderPrecisionFormat(shaderType, precisionType, range,
                                        &precision);
  return WebGLShaderPrecisionFormat::create(range[0], range[1], precision);
}

String WebGLRenderingContextBase::getShaderSource(WebGLShader* shader) {
  if (isContextLost() || !validateWebGLObject("getShaderSource", shader))
    return String();
  return ensureNotNull(shader->source());
}

Nullable<Vector<String>> WebGLRenderingContextBase::getSupportedExtensions() {
  if (isContextLost())
    return nullptr;

  Vector<String> result;

  for (size_t i = 0; i < m_extensions.size(); ++i) {
    ExtensionTracker* tracker = m_extensions[i].get();
    if (extensionSupportedAndAllowed(tracker)) {
      const char* const* prefixes = tracker->prefixes();
      for (; *prefixes; ++prefixes) {
        String prefixedName = String(*prefixes) + tracker->extensionName();
        result.append(prefixedName);
      }
    }
  }

  return result;
}

ScriptValue WebGLRenderingContextBase::getTexParameter(ScriptState* scriptState,
                                                       GLenum target,
                                                       GLenum pname) {
  if (isContextLost())
    return ScriptValue::createNull(scriptState);
  if (!validateTextureBinding("getTexParameter", target))
    return ScriptValue::createNull(scriptState);
  switch (pname) {
    case GL_TEXTURE_MAG_FILTER:
    case GL_TEXTURE_MIN_FILTER:
    case GL_TEXTURE_WRAP_S:
    case GL_TEXTURE_WRAP_T: {
      GLint value = 0;
      contextGL()->GetTexParameteriv(target, pname, &value);
      return WebGLAny(scriptState, static_cast<unsigned>(value));
    }
    case GL_TEXTURE_MAX_ANISOTROPY_EXT:  // EXT_texture_filter_anisotropic
      if (extensionEnabled(EXTTextureFilterAnisotropicName)) {
        GLfloat value = 0.f;
        contextGL()->GetTexParameterfv(target, pname, &value);
        return WebGLAny(scriptState, value);
      }
      synthesizeGLError(
          GL_INVALID_ENUM, "getTexParameter",
          "invalid parameter name, EXT_texture_filter_anisotropic not enabled");
      return ScriptValue::createNull(scriptState);
    default:
      synthesizeGLError(GL_INVALID_ENUM, "getTexParameter",
                        "invalid parameter name");
      return ScriptValue::createNull(scriptState);
  }
}

ScriptValue WebGLRenderingContextBase::getUniform(
    ScriptState* scriptState,
    WebGLProgram* program,
    const WebGLUniformLocation* uniformLocation) {
  if (isContextLost() || !validateWebGLObject("getUniform", program))
    return ScriptValue::createNull(scriptState);
  DCHECK(uniformLocation);
  if (uniformLocation->program() != program) {
    synthesizeGLError(GL_INVALID_OPERATION, "getUniform",
                      "no uniformlocation or not valid for this program");
    return ScriptValue::createNull(scriptState);
  }
  GLint location = uniformLocation->location();

  GLuint programId = objectNonZero(program);
  GLint maxNameLength = -1;
  contextGL()->GetProgramiv(programId, GL_ACTIVE_UNIFORM_MAX_LENGTH,
                            &maxNameLength);
  if (maxNameLength < 0)
    return ScriptValue::createNull(scriptState);
  if (maxNameLength == 0) {
    synthesizeGLError(GL_INVALID_VALUE, "getUniform",
                      "no active uniforms exist");
    return ScriptValue::createNull(scriptState);
  }

  // FIXME: make this more efficient using WebGLUniformLocation and caching
  // types in it.
  GLint activeUniforms = 0;
  contextGL()->GetProgramiv(programId, GL_ACTIVE_UNIFORMS, &activeUniforms);
  for (GLint i = 0; i < activeUniforms; i++) {
    LChar* namePtr;
    RefPtr<StringImpl> nameImpl =
        StringImpl::createUninitialized(maxNameLength, namePtr);
    GLsizei length = 0;
    GLint size = -1;
    GLenum type = 0;
    contextGL()->GetActiveUniform(programId, i, maxNameLength, &length, &size,
                                  &type, reinterpret_cast<GLchar*>(namePtr));
    if (size < 0)
      return ScriptValue::createNull(scriptState);
    String name(nameImpl->substring(0, length));
    StringBuilder nameBuilder;
    // Strip "[0]" from the name if it's an array.
    if (size > 1 && name.endsWith("[0]"))
      name = name.left(name.length() - 3);
    // If it's an array, we need to iterate through each element, appending
    // "[index]" to the name.
    for (GLint index = 0; index < size; ++index) {
      nameBuilder.clear();
      nameBuilder.append(name);
      if (size > 1 && index >= 1) {
        nameBuilder.append('[');
        nameBuilder.appendNumber(index);
        nameBuilder.append(']');
      }
      // Now need to look this up by name again to find its location
      GLint loc = contextGL()->GetUniformLocation(
          objectOrZero(program), nameBuilder.toString().utf8().data());
      if (loc == location) {
        // Found it. Use the type in the ActiveInfo to determine the return
        // type.
        GLenum baseType;
        unsigned length;
        switch (type) {
          case GL_BOOL:
            baseType = GL_BOOL;
            length = 1;
            break;
          case GL_BOOL_VEC2:
            baseType = GL_BOOL;
            length = 2;
            break;
          case GL_BOOL_VEC3:
            baseType = GL_BOOL;
            length = 3;
            break;
          case GL_BOOL_VEC4:
            baseType = GL_BOOL;
            length = 4;
            break;
          case GL_INT:
            baseType = GL_INT;
            length = 1;
            break;
          case GL_INT_VEC2:
            baseType = GL_INT;
            length = 2;
            break;
          case GL_INT_VEC3:
            baseType = GL_INT;
            length = 3;
            break;
          case GL_INT_VEC4:
            baseType = GL_INT;
            length = 4;
            break;
          case GL_FLOAT:
            baseType = GL_FLOAT;
            length = 1;
            break;
          case GL_FLOAT_VEC2:
            baseType = GL_FLOAT;
            length = 2;
            break;
          case GL_FLOAT_VEC3:
            baseType = GL_FLOAT;
            length = 3;
            break;
          case GL_FLOAT_VEC4:
            baseType = GL_FLOAT;
            length = 4;
            break;
          case GL_FLOAT_MAT2:
            baseType = GL_FLOAT;
            length = 4;
            break;
          case GL_FLOAT_MAT3:
            baseType = GL_FLOAT;
            length = 9;
            break;
          case GL_FLOAT_MAT4:
            baseType = GL_FLOAT;
            length = 16;
            break;
          case GL_SAMPLER_2D:
          case GL_SAMPLER_CUBE:
            baseType = GL_INT;
            length = 1;
            break;
          default:
            if (!isWebGL2OrHigher()) {
              // Can't handle this type
              synthesizeGLError(GL_INVALID_VALUE, "getUniform",
                                "unhandled type");
              return ScriptValue::createNull(scriptState);
            }
            // handle GLenums for WebGL 2.0 or higher
            switch (type) {
              case GL_UNSIGNED_INT:
                baseType = GL_UNSIGNED_INT;
                length = 1;
                break;
              case GL_UNSIGNED_INT_VEC2:
                baseType = GL_UNSIGNED_INT;
                length = 2;
                break;
              case GL_UNSIGNED_INT_VEC3:
                baseType = GL_UNSIGNED_INT;
                length = 3;
                break;
              case GL_UNSIGNED_INT_VEC4:
                baseType = GL_UNSIGNED_INT;
                length = 4;
                break;
              case GL_FLOAT_MAT2x3:
                baseType = GL_FLOAT;
                length = 6;
                break;
              case GL_FLOAT_MAT2x4:
                baseType = GL_FLOAT;
                length = 8;
                break;
              case GL_FLOAT_MAT3x2:
                baseType = GL_FLOAT;
                length = 6;
                break;
              case GL_FLOAT_MAT3x4:
                baseType = GL_FLOAT;
                length = 12;
                break;
              case GL_FLOAT_MAT4x2:
                baseType = GL_FLOAT;
                length = 8;
                break;
              case GL_FLOAT_MAT4x3:
                baseType = GL_FLOAT;
                length = 12;
                break;
              case GL_SAMPLER_3D:
              case GL_SAMPLER_2D_ARRAY:
                baseType = GL_INT;
                length = 1;
                break;
              default:
                // Can't handle this type
                synthesizeGLError(GL_INVALID_VALUE, "getUniform",
                                  "unhandled type");
                return ScriptValue::createNull(scriptState);
            }
        }
        switch (baseType) {
          case GL_FLOAT: {
            GLfloat value[16] = {0};
            contextGL()->GetUniformfv(objectOrZero(program), location, value);
            if (length == 1)
              return WebGLAny(scriptState, value[0]);
            return WebGLAny(scriptState,
                            DOMFloat32Array::create(value, length));
          }
          case GL_INT: {
            GLint value[4] = {0};
            contextGL()->GetUniformiv(objectOrZero(program), location, value);
            if (length == 1)
              return WebGLAny(scriptState, value[0]);
            return WebGLAny(scriptState, DOMInt32Array::create(value, length));
          }
          case GL_UNSIGNED_INT: {
            GLuint value[4] = {0};
            contextGL()->GetUniformuiv(objectOrZero(program), location, value);
            if (length == 1)
              return WebGLAny(scriptState, value[0]);
            return WebGLAny(scriptState, DOMUint32Array::create(value, length));
          }
          case GL_BOOL: {
            GLint value[4] = {0};
            contextGL()->GetUniformiv(objectOrZero(program), location, value);
            if (length > 1) {
              bool boolValue[16] = {0};
              for (unsigned j = 0; j < length; j++)
                boolValue[j] = static_cast<bool>(value[j]);
              return WebGLAny(scriptState, boolValue, length);
            }
            return WebGLAny(scriptState, static_cast<bool>(value[0]));
          }
          default:
            NOTIMPLEMENTED();
        }
      }
    }
  }
  // If we get here, something went wrong in our unfortunately complex logic
  // above
  synthesizeGLError(GL_INVALID_VALUE, "getUniform", "unknown error");
  return ScriptValue::createNull(scriptState);
}

WebGLUniformLocation* WebGLRenderingContextBase::getUniformLocation(
    WebGLProgram* program,
    const String& name) {
  if (isContextLost() || !validateWebGLObject("getUniformLocation", program))
    return nullptr;
  if (!validateLocationLength("getUniformLocation", name))
    return nullptr;
  if (!validateString("getUniformLocation", name))
    return nullptr;
  if (isPrefixReserved(name))
    return nullptr;
  if (!program->linkStatus(this)) {
    synthesizeGLError(GL_INVALID_OPERATION, "getUniformLocation",
                      "program not linked");
    return nullptr;
  }
  GLint uniformLocation = contextGL()->GetUniformLocation(objectOrZero(program),
                                                          name.utf8().data());
  if (uniformLocation == -1)
    return nullptr;
  return WebGLUniformLocation::create(program, uniformLocation);
}

ScriptValue WebGLRenderingContextBase::getVertexAttrib(ScriptState* scriptState,
                                                       GLuint index,
                                                       GLenum pname) {
  if (isContextLost())
    return ScriptValue::createNull(scriptState);
  if (index >= m_maxVertexAttribs) {
    synthesizeGLError(GL_INVALID_VALUE, "getVertexAttrib",
                      "index out of range");
    return ScriptValue::createNull(scriptState);
  }

  if ((extensionEnabled(ANGLEInstancedArraysName) || isWebGL2OrHigher()) &&
      pname == GL_VERTEX_ATTRIB_ARRAY_DIVISOR_ANGLE) {
    GLint value = 0;
    contextGL()->GetVertexAttribiv(index, pname, &value);
    return WebGLAny(scriptState, value);
  }

  switch (pname) {
    case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
      return WebGLAny(scriptState,
                      m_boundVertexArrayObject->getArrayBufferForAttrib(index));
    case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
    case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED: {
      GLint value = 0;
      contextGL()->GetVertexAttribiv(index, pname, &value);
      return WebGLAny(scriptState, static_cast<bool>(value));
    }
    case GL_VERTEX_ATTRIB_ARRAY_SIZE:
    case GL_VERTEX_ATTRIB_ARRAY_STRIDE: {
      GLint value = 0;
      contextGL()->GetVertexAttribiv(index, pname, &value);
      return WebGLAny(scriptState, value);
    }
    case GL_VERTEX_ATTRIB_ARRAY_TYPE: {
      GLint value = 0;
      contextGL()->GetVertexAttribiv(index, pname, &value);
      return WebGLAny(scriptState, static_cast<GLenum>(value));
    }
    case GL_CURRENT_VERTEX_ATTRIB: {
      switch (m_vertexAttribType[index]) {
        case Float32ArrayType: {
          GLfloat floatValue[4];
          contextGL()->GetVertexAttribfv(index, pname, floatValue);
          return WebGLAny(scriptState, DOMFloat32Array::create(floatValue, 4));
        }
        case Int32ArrayType: {
          GLint intValue[4];
          contextGL()->GetVertexAttribIiv(index, pname, intValue);
          return WebGLAny(scriptState, DOMInt32Array::create(intValue, 4));
        }
        case Uint32ArrayType: {
          GLuint uintValue[4];
          contextGL()->GetVertexAttribIuiv(index, pname, uintValue);
          return WebGLAny(scriptState, DOMUint32Array::create(uintValue, 4));
        }
        default:
          ASSERT_NOT_REACHED();
          break;
      }
      return ScriptValue::createNull(scriptState);
    }
    case GL_VERTEX_ATTRIB_ARRAY_INTEGER:
      if (isWebGL2OrHigher()) {
        GLint value = 0;
        contextGL()->GetVertexAttribiv(index, pname, &value);
        return WebGLAny(scriptState, static_cast<bool>(value));
      }
    // fall through to default error case
    default:
      synthesizeGLError(GL_INVALID_ENUM, "getVertexAttrib",
                        "invalid parameter name");
      return ScriptValue::createNull(scriptState);
  }
}

long long WebGLRenderingContextBase::getVertexAttribOffset(GLuint index,
                                                           GLenum pname) {
  if (isContextLost())
    return 0;
  GLvoid* result = nullptr;
  // NOTE: If pname is ever a value that returns more than 1 element
  // this will corrupt memory.
  contextGL()->GetVertexAttribPointerv(index, pname, &result);
  return static_cast<long long>(reinterpret_cast<intptr_t>(result));
}

void WebGLRenderingContextBase::hint(GLenum target, GLenum mode) {
  if (isContextLost())
    return;
  bool isValid = false;
  switch (target) {
    case GL_GENERATE_MIPMAP_HINT:
      isValid = true;
      break;
    case GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES:  // OES_standard_derivatives
      if (extensionEnabled(OESStandardDerivativesName) || isWebGL2OrHigher())
        isValid = true;
      break;
  }
  if (!isValid) {
    synthesizeGLError(GL_INVALID_ENUM, "hint", "invalid target");
    return;
  }
  contextGL()->Hint(target, mode);
}

GLboolean WebGLRenderingContextBase::isBuffer(WebGLBuffer* buffer) {
  if (!buffer || isContextLost())
    return 0;

  if (!buffer->hasEverBeenBound())
    return 0;
  if (buffer->isDeleted())
    return 0;

  return contextGL()->IsBuffer(buffer->object());
}

bool WebGLRenderingContextBase::isContextLost() const {
  return m_contextLostMode != NotLostContext;
}

GLboolean WebGLRenderingContextBase::isEnabled(GLenum cap) {
  if (isContextLost() || !validateCapability("isEnabled", cap))
    return 0;
  if (cap == GL_STENCIL_TEST)
    return m_stencilEnabled;
  return contextGL()->IsEnabled(cap);
}

GLboolean WebGLRenderingContextBase::isFramebuffer(
    WebGLFramebuffer* framebuffer) {
  if (!framebuffer || isContextLost())
    return 0;

  if (!framebuffer->hasEverBeenBound())
    return 0;
  if (framebuffer->isDeleted())
    return 0;

  return contextGL()->IsFramebuffer(framebuffer->object());
}

GLboolean WebGLRenderingContextBase::isProgram(WebGLProgram* program) {
  if (!program || isContextLost())
    return 0;

  return contextGL()->IsProgram(program->object());
}

GLboolean WebGLRenderingContextBase::isRenderbuffer(
    WebGLRenderbuffer* renderbuffer) {
  if (!renderbuffer || isContextLost())
    return 0;

  if (!renderbuffer->hasEverBeenBound())
    return 0;
  if (renderbuffer->isDeleted())
    return 0;

  return contextGL()->IsRenderbuffer(renderbuffer->object());
}

GLboolean WebGLRenderingContextBase::isShader(WebGLShader* shader) {
  if (!shader || isContextLost())
    return 0;

  return contextGL()->IsShader(shader->object());
}

GLboolean WebGLRenderingContextBase::isTexture(WebGLTexture* texture) {
  if (!texture || isContextLost())
    return 0;

  if (!texture->hasEverBeenBound())
    return 0;
  if (texture->isDeleted())
    return 0;

  return contextGL()->IsTexture(texture->object());
}

void WebGLRenderingContextBase::lineWidth(GLfloat width) {
  if (isContextLost())
    return;
  contextGL()->LineWidth(width);
}

void WebGLRenderingContextBase::linkProgram(WebGLProgram* program) {
  if (isContextLost() || !validateWebGLObject("linkProgram", program))
    return;

  if (program->activeTransformFeedbackCount() > 0) {
    synthesizeGLError(
        GL_INVALID_OPERATION, "linkProgram",
        "program being used by one or more active transform feedback objects");
    return;
  }

  contextGL()->LinkProgram(objectOrZero(program));
  program->increaseLinkCount();
}

void WebGLRenderingContextBase::pixelStorei(GLenum pname, GLint param) {
  if (isContextLost())
    return;
  switch (pname) {
    case GC3D_UNPACK_FLIP_Y_WEBGL:
      m_unpackFlipY = param;
      break;
    case GC3D_UNPACK_PREMULTIPLY_ALPHA_WEBGL:
      m_unpackPremultiplyAlpha = param;
      break;
    case GC3D_UNPACK_COLORSPACE_CONVERSION_WEBGL:
      if (static_cast<GLenum>(param) == GC3D_BROWSER_DEFAULT_WEBGL ||
          param == GL_NONE) {
        m_unpackColorspaceConversion = static_cast<GLenum>(param);
      } else {
        synthesizeGLError(
            GL_INVALID_VALUE, "pixelStorei",
            "invalid parameter for UNPACK_COLORSPACE_CONVERSION_WEBGL");
        return;
      }
      break;
    case GL_PACK_ALIGNMENT:
    case GL_UNPACK_ALIGNMENT:
      if (param == 1 || param == 2 || param == 4 || param == 8) {
        if (pname == GL_PACK_ALIGNMENT) {
          m_packAlignment = param;
        } else {  // GL_UNPACK_ALIGNMENT:
          m_unpackAlignment = param;
        }
        contextGL()->PixelStorei(pname, param);
      } else {
        synthesizeGLError(GL_INVALID_VALUE, "pixelStorei",
                          "invalid parameter for alignment");
        return;
      }
      break;
    default:
      synthesizeGLError(GL_INVALID_ENUM, "pixelStorei",
                        "invalid parameter name");
      return;
  }
}

void WebGLRenderingContextBase::polygonOffset(GLfloat factor, GLfloat units) {
  if (isContextLost())
    return;
  contextGL()->PolygonOffset(factor, units);
}

bool WebGLRenderingContextBase::validateReadBufferAndGetInfo(
    const char* functionName,
    WebGLFramebuffer*& readFramebufferBinding) {
  readFramebufferBinding = getReadFramebufferBinding();
  if (readFramebufferBinding) {
    const char* reason = "framebuffer incomplete";
    if (readFramebufferBinding->checkDepthStencilStatus(&reason) !=
        GL_FRAMEBUFFER_COMPLETE) {
      synthesizeGLError(GL_INVALID_FRAMEBUFFER_OPERATION, functionName, reason);
      return false;
    }
  } else {
    if (m_readBufferOfDefaultFramebuffer == GL_NONE) {
      ASSERT(isWebGL2OrHigher());
      synthesizeGLError(GL_INVALID_OPERATION, functionName,
                        "no image to read from");
      return false;
    }
  }
  return true;
}

bool WebGLRenderingContextBase::validateReadPixelsFormatAndType(
    GLenum format,
    GLenum type,
    DOMArrayBufferView* buffer) {
  switch (format) {
    case GL_ALPHA:
    case GL_RGB:
    case GL_RGBA:
      break;
    default:
      synthesizeGLError(GL_INVALID_ENUM, "readPixels", "invalid format");
      return false;
  }

  switch (type) {
    case GL_UNSIGNED_BYTE:
      if (buffer && buffer->type() != DOMArrayBufferView::TypeUint8) {
        synthesizeGLError(
            GL_INVALID_OPERATION, "readPixels",
            "type UNSIGNED_BYTE but ArrayBufferView not Uint8Array");
        return false;
      }
      return true;
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_5_5_5_1:
      if (buffer && buffer->type() != DOMArrayBufferView::TypeUint16) {
        synthesizeGLError(
            GL_INVALID_OPERATION, "readPixels",
            "type UNSIGNED_SHORT but ArrayBufferView not Uint16Array");
        return false;
      }
      return true;
    case GL_FLOAT:
      if (extensionEnabled(OESTextureFloatName) ||
          extensionEnabled(OESTextureHalfFloatName)) {
        if (buffer && buffer->type() != DOMArrayBufferView::TypeFloat32) {
          synthesizeGLError(GL_INVALID_OPERATION, "readPixels",
                            "type FLOAT but ArrayBufferView not Float32Array");
          return false;
        }
        return true;
      }
      synthesizeGLError(GL_INVALID_ENUM, "readPixels", "invalid type");
      return false;
    case GL_HALF_FLOAT_OES:
      if (extensionEnabled(OESTextureHalfFloatName)) {
        if (buffer && buffer->type() != DOMArrayBufferView::TypeUint16) {
          synthesizeGLError(
              GL_INVALID_OPERATION, "readPixels",
              "type HALF_FLOAT_OES but ArrayBufferView not Uint16Array");
          return false;
        }
        return true;
      }
      synthesizeGLError(GL_INVALID_ENUM, "readPixels", "invalid type");
      return false;
    default:
      synthesizeGLError(GL_INVALID_ENUM, "readPixels", "invalid type");
      return false;
  }
}

WebGLImageConversion::PixelStoreParams
WebGLRenderingContextBase::getPackPixelStoreParams() {
  WebGLImageConversion::PixelStoreParams params;
  params.alignment = m_packAlignment;
  return params;
}

WebGLImageConversion::PixelStoreParams
WebGLRenderingContextBase::getUnpackPixelStoreParams(TexImageDimension) {
  WebGLImageConversion::PixelStoreParams params;
  params.alignment = m_unpackAlignment;
  return params;
}

bool WebGLRenderingContextBase::validateReadPixelsFuncParameters(
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    DOMArrayBufferView* buffer,
    long long bufferSize) {
  if (!validateReadPixelsFormatAndType(format, type, buffer))
    return false;

  // Calculate array size, taking into consideration of pack parameters.
  unsigned totalBytesRequired = 0, totalSkipBytes = 0;
  GLenum error = WebGLImageConversion::computeImageSizeInBytes(
      format, type, width, height, 1, getPackPixelStoreParams(),
      &totalBytesRequired, 0, &totalSkipBytes);
  if (error != GL_NO_ERROR) {
    synthesizeGLError(error, "readPixels", "invalid dimensions");
    return false;
  }
  if (bufferSize <
      static_cast<long long>(totalBytesRequired + totalSkipBytes)) {
    synthesizeGLError(GL_INVALID_OPERATION, "readPixels",
                      "buffer is not large enough for dimensions");
    return false;
  }
  return true;
}

void WebGLRenderingContextBase::readPixels(GLint x,
                                           GLint y,
                                           GLsizei width,
                                           GLsizei height,
                                           GLenum format,
                                           GLenum type,
                                           DOMArrayBufferView* pixels) {
  readPixelsHelper(x, y, width, height, format, type, pixels, 0);
}

void WebGLRenderingContextBase::readPixelsHelper(GLint x,
                                                 GLint y,
                                                 GLsizei width,
                                                 GLsizei height,
                                                 GLenum format,
                                                 GLenum type,
                                                 DOMArrayBufferView* pixels,
                                                 GLuint offset) {
  if (isContextLost())
    return;
  // Due to WebGL's same-origin restrictions, it is not possible to
  // taint the origin using the WebGL API.
  ASSERT(canvas()->originClean());
  // Validate input parameters.
  if (!pixels) {
    synthesizeGLError(GL_INVALID_VALUE, "readPixels",
                      "no destination ArrayBufferView");
    return;
  }
  CheckedNumeric<GLuint> offsetInBytes = offset;
  offsetInBytes *= pixels->typeSize();
  if (!offsetInBytes.IsValid() ||
      offsetInBytes.ValueOrDie() > pixels->byteLength()) {
    synthesizeGLError(GL_INVALID_VALUE, "readPixels",
                      "destination offset out of range");
    return;
  }
  const char* reason = "framebuffer incomplete";
  WebGLFramebuffer* framebuffer = getReadFramebufferBinding();
  if (framebuffer &&
      framebuffer->checkDepthStencilStatus(&reason) !=
          GL_FRAMEBUFFER_COMPLETE) {
    synthesizeGLError(GL_INVALID_FRAMEBUFFER_OPERATION, "readPixels", reason);
    return;
  }
  if (!validateReadPixelsFuncParameters(
          width, height, format, type, pixels,
          static_cast<long long>(pixels->byteLength() -
                                 offsetInBytes.ValueOrDie()))) {
    return;
  }
  clearIfComposited();
  uint8_t* data =
      static_cast<uint8_t*>(pixels->baseAddress()) + offsetInBytes.ValueOrDie();
  {
    ScopedDrawingBufferBinder binder(drawingBuffer(), framebuffer);
    contextGL()->ReadPixels(x, y, width, height, format, type, data);
  }
}

void WebGLRenderingContextBase::renderbufferStorageImpl(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height,
    const char* functionName) {
  ASSERT(!samples);             // |samples| > 0 is only valid in WebGL2's
                                // renderbufferStorageMultisample().
  ASSERT(!isWebGL2OrHigher());  // Make sure this is overridden in WebGL 2.
  switch (internalformat) {
    case GL_DEPTH_COMPONENT16:
    case GL_RGBA4:
    case GL_RGB5_A1:
    case GL_RGB565:
    case GL_STENCIL_INDEX8:
      contextGL()->RenderbufferStorage(target, internalformat, width, height);
      m_renderbufferBinding->setInternalFormat(internalformat);
      m_renderbufferBinding->setSize(width, height);
      break;
    case GL_SRGB8_ALPHA8_EXT:
      if (!extensionEnabled(EXTsRGBName)) {
        synthesizeGLError(GL_INVALID_ENUM, functionName, "sRGB not enabled");
        break;
      }
      contextGL()->RenderbufferStorage(target, internalformat, width, height);
      m_renderbufferBinding->setInternalFormat(internalformat);
      m_renderbufferBinding->setSize(width, height);
      break;
    case GL_DEPTH_STENCIL_OES:
      ASSERT(isDepthStencilSupported());
      contextGL()->RenderbufferStorage(target, GL_DEPTH24_STENCIL8_OES, width,
                                       height);
      m_renderbufferBinding->setSize(width, height);
      m_renderbufferBinding->setInternalFormat(internalformat);
      break;
    default:
      synthesizeGLError(GL_INVALID_ENUM, functionName,
                        "invalid internalformat");
      break;
  }
}

void WebGLRenderingContextBase::renderbufferStorage(GLenum target,
                                                    GLenum internalformat,
                                                    GLsizei width,
                                                    GLsizei height) {
  const char* functionName = "renderbufferStorage";
  if (isContextLost())
    return;
  if (target != GL_RENDERBUFFER) {
    synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid target");
    return;
  }
  if (!m_renderbufferBinding || !m_renderbufferBinding->object()) {
    synthesizeGLError(GL_INVALID_OPERATION, functionName,
                      "no bound renderbuffer");
    return;
  }
  if (!validateSize(functionName, width, height))
    return;
  renderbufferStorageImpl(target, 0, internalformat, width, height,
                          functionName);
  applyStencilTest();
}

void WebGLRenderingContextBase::sampleCoverage(GLfloat value,
                                               GLboolean invert) {
  if (isContextLost())
    return;
  contextGL()->SampleCoverage(value, invert);
}

void WebGLRenderingContextBase::scissor(GLint x,
                                        GLint y,
                                        GLsizei width,
                                        GLsizei height) {
  if (isContextLost())
    return;
  m_scissorBox[0] = x;
  m_scissorBox[1] = y;
  m_scissorBox[2] = width;
  m_scissorBox[3] = height;
  contextGL()->Scissor(x, y, width, height);
}

void WebGLRenderingContextBase::shaderSource(WebGLShader* shader,
                                             const String& string) {
  if (isContextLost() || !validateWebGLObject("shaderSource", shader))
    return;
  String stringWithoutComments = StripComments(string).result();
  // TODO(danakj): Make validateShaderSource reject characters > 255 (or utf16
  // Strings) so we don't need to use StringUTF8Adaptor.
  if (!validateShaderSource(stringWithoutComments))
    return;
  shader->setSource(string);
  WTF::StringUTF8Adaptor adaptor(stringWithoutComments);
  const GLchar* shaderData = adaptor.data();
  // TODO(danakj): Use base::saturated_cast<GLint>.
  const GLint shaderLength = adaptor.length();
  contextGL()->ShaderSource(objectOrZero(shader), 1, &shaderData,
                            &shaderLength);
}

void WebGLRenderingContextBase::stencilFunc(GLenum func,
                                            GLint ref,
                                            GLuint mask) {
  if (isContextLost())
    return;
  if (!validateStencilOrDepthFunc("stencilFunc", func))
    return;
  m_stencilFuncRef = ref;
  m_stencilFuncRefBack = ref;
  m_stencilFuncMask = mask;
  m_stencilFuncMaskBack = mask;
  contextGL()->StencilFunc(func, ref, mask);
}

void WebGLRenderingContextBase::stencilFuncSeparate(GLenum face,
                                                    GLenum func,
                                                    GLint ref,
                                                    GLuint mask) {
  if (isContextLost())
    return;
  if (!validateStencilOrDepthFunc("stencilFuncSeparate", func))
    return;
  switch (face) {
    case GL_FRONT_AND_BACK:
      m_stencilFuncRef = ref;
      m_stencilFuncRefBack = ref;
      m_stencilFuncMask = mask;
      m_stencilFuncMaskBack = mask;
      break;
    case GL_FRONT:
      m_stencilFuncRef = ref;
      m_stencilFuncMask = mask;
      break;
    case GL_BACK:
      m_stencilFuncRefBack = ref;
      m_stencilFuncMaskBack = mask;
      break;
    default:
      synthesizeGLError(GL_INVALID_ENUM, "stencilFuncSeparate", "invalid face");
      return;
  }
  contextGL()->StencilFuncSeparate(face, func, ref, mask);
}

void WebGLRenderingContextBase::stencilMask(GLuint mask) {
  if (isContextLost())
    return;
  m_stencilMask = mask;
  m_stencilMaskBack = mask;
  contextGL()->StencilMask(mask);
}

void WebGLRenderingContextBase::stencilMaskSeparate(GLenum face, GLuint mask) {
  if (isContextLost())
    return;
  switch (face) {
    case GL_FRONT_AND_BACK:
      m_stencilMask = mask;
      m_stencilMaskBack = mask;
      break;
    case GL_FRONT:
      m_stencilMask = mask;
      break;
    case GL_BACK:
      m_stencilMaskBack = mask;
      break;
    default:
      synthesizeGLError(GL_INVALID_ENUM, "stencilMaskSeparate", "invalid face");
      return;
  }
  contextGL()->StencilMaskSeparate(face, mask);
}

void WebGLRenderingContextBase::stencilOp(GLenum fail,
                                          GLenum zfail,
                                          GLenum zpass) {
  if (isContextLost())
    return;
  contextGL()->StencilOp(fail, zfail, zpass);
}

void WebGLRenderingContextBase::stencilOpSeparate(GLenum face,
                                                  GLenum fail,
                                                  GLenum zfail,
                                                  GLenum zpass) {
  if (isContextLost())
    return;
  contextGL()->StencilOpSeparate(face, fail, zfail, zpass);
}

GLenum WebGLRenderingContextBase::convertTexInternalFormat(
    GLenum internalformat,
    GLenum type) {
  // Convert to sized internal formats that are renderable with
  // GL_CHROMIUM_color_buffer_float_rgb(a).
  if (type == GL_FLOAT && internalformat == GL_RGBA &&
      extensionsUtil()->isExtensionEnabled(
          "GL_CHROMIUM_color_buffer_float_rgba"))
    return GL_RGBA32F_EXT;
  if (type == GL_FLOAT && internalformat == GL_RGB &&
      extensionsUtil()->isExtensionEnabled(
          "GL_CHROMIUM_color_buffer_float_rgb"))
    return GL_RGB32F_EXT;
  return internalformat;
}

void WebGLRenderingContextBase::texImage2DBase(GLenum target,
                                               GLint level,
                                               GLint internalformat,
                                               GLsizei width,
                                               GLsizei height,
                                               GLint border,
                                               GLenum format,
                                               GLenum type,
                                               const void* pixels) {
  // All calling functions check isContextLost, so a duplicate check is not
  // needed here.
  contextGL()->TexImage2D(target, level,
                          convertTexInternalFormat(internalformat, type), width,
                          height, border, format, type, pixels);
}

void WebGLRenderingContextBase::texImageImpl(
    TexImageFunctionID functionID,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    GLenum format,
    GLenum type,
    Image* image,
    WebGLImageConversion::ImageHtmlDomSource domSource,
    bool flipY,
    bool premultiplyAlpha,
    const IntRect& sourceImageRect,
    GLsizei depth,
    GLint unpackImageHeight) {
  const char* funcName = getTexImageFunctionName(functionID);
  // All calling functions check isContextLost, so a duplicate check is not
  // needed here.
  if (type == GL_UNSIGNED_INT_10F_11F_11F_REV) {
    // The UNSIGNED_INT_10F_11F_11F_REV type pack/unpack isn't implemented.
    type = GL_FLOAT;
  }
  Vector<uint8_t> data;

  IntRect subRect = sourceImageRect;
  if (subRect == sentinelEmptyRect()) {
    // Recalculate based on the size of the Image.
    subRect = safeGetImageSize(image);
  }

  bool selectingSubRectangle = false;
  if (!validateTexImageSubRectangle(funcName, functionID, image, subRect, depth,
                                    unpackImageHeight,
                                    &selectingSubRectangle)) {
    return;
  }

  // Adjust the source image rectangle if doing a y-flip.
  IntRect adjustedSourceImageRect = subRect;
  if (flipY) {
    adjustedSourceImageRect.setY(image->height() -
                                 adjustedSourceImageRect.maxY());
  }

  WebGLImageConversion::ImageExtractor imageExtractor(
      image, domSource, premultiplyAlpha,
      m_unpackColorspaceConversion == GL_NONE);
  if (!imageExtractor.imagePixelData()) {
    synthesizeGLError(GL_INVALID_VALUE, funcName, "bad image data");
    return;
  }

  WebGLImageConversion::DataFormat sourceDataFormat =
      imageExtractor.imageSourceFormat();
  WebGLImageConversion::AlphaOp alphaOp = imageExtractor.imageAlphaOp();
  const void* imagePixelData = imageExtractor.imagePixelData();

  bool needConversion = true;
  if (type == GL_UNSIGNED_BYTE &&
      sourceDataFormat == WebGLImageConversion::DataFormatRGBA8 &&
      format == GL_RGBA && alphaOp == WebGLImageConversion::AlphaDoNothing &&
      !flipY && !selectingSubRectangle && depth == 1) {
    needConversion = false;
  } else {
    if (!WebGLImageConversion::packImageData(
            image, imagePixelData, format, type, flipY, alphaOp,
            sourceDataFormat, imageExtractor.imageWidth(),
            imageExtractor.imageHeight(), adjustedSourceImageRect, depth,
            imageExtractor.imageSourceUnpackAlignment(), unpackImageHeight,
            data)) {
      synthesizeGLError(GL_INVALID_VALUE, funcName, "packImage error");
      return;
    }
  }

  resetUnpackParameters();
  if (functionID == TexImage2D) {
    texImage2DBase(target, level, internalformat,
                   adjustedSourceImageRect.width(),
                   adjustedSourceImageRect.height(), 0, format, type,
                   needConversion ? data.data() : imagePixelData);
  } else if (functionID == TexSubImage2D) {
    contextGL()->TexSubImage2D(target, level, xoffset, yoffset,
                               adjustedSourceImageRect.width(),
                               adjustedSourceImageRect.height(), format, type,
                               needConversion ? data.data() : imagePixelData);
  } else {
    // 3D functions.
    if (functionID == TexImage3D) {
      contextGL()->TexImage3D(
          target, level, internalformat, adjustedSourceImageRect.width(),
          adjustedSourceImageRect.height(), depth, 0, format, type,
          needConversion ? data.data() : imagePixelData);
    } else {
      DCHECK_EQ(functionID, TexSubImage3D);
      contextGL()->TexSubImage3D(
          target, level, xoffset, yoffset, zoffset,
          adjustedSourceImageRect.width(), adjustedSourceImageRect.height(),
          depth, format, type, needConversion ? data.data() : imagePixelData);
    }
  }
  restoreUnpackParameters();
}

bool WebGLRenderingContextBase::validateTexFunc(
    const char* functionName,
    TexImageFunctionType functionType,
    TexFuncValidationSourceType sourceType,
    GLenum target,
    GLint level,
    GLenum internalformat,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    GLenum format,
    GLenum type,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset) {
  if (!validateTexFuncLevel(functionName, target, level))
    return false;

  if (!validateTexFuncParameters(functionName, functionType, sourceType, target,
                                 level, internalformat, width, height, depth,
                                 border, format, type))
    return false;

  if (functionType == TexSubImage) {
    if (!validateSettableTexFormat(functionName, format))
      return false;
    if (!validateSize(functionName, xoffset, yoffset, zoffset))
      return false;
  } else {
    // For SourceArrayBufferView, function validateTexFuncData() would handle
    // whether to validate the SettableTexFormat
    // by checking if the ArrayBufferView is null or not.
    if (sourceType != SourceArrayBufferView) {
      if (!validateSettableTexFormat(functionName, format))
        return false;
    }
  }

  return true;
}

bool WebGLRenderingContextBase::validateValueFitNonNegInt32(
    const char* functionName,
    const char* paramName,
    long long value) {
  if (value < 0) {
    String errorMsg = String(paramName) + " < 0";
    synthesizeGLError(GL_INVALID_VALUE, functionName, errorMsg.ascii().data());
    return false;
  }
  if (value > static_cast<long long>(std::numeric_limits<int>::max())) {
    String errorMsg = String(paramName) + " more than 32-bit";
    synthesizeGLError(GL_INVALID_OPERATION, functionName,
                      errorMsg.ascii().data());
    return false;
  }
  return true;
}

// TODO(fmalita): figure why WebGLImageConversion::ImageExtractor can't handle
// SVG-backed images, and get rid of this intermediate step.
PassRefPtr<Image> WebGLRenderingContextBase::drawImageIntoBuffer(
    PassRefPtr<Image> passImage,
    int width,
    int height,
    const char* functionName) {
  RefPtr<Image> image(passImage);
  ASSERT(image);

  IntSize size(width, height);
  ImageBuffer* buf = m_generatedImageCache.imageBuffer(size);
  if (!buf) {
    synthesizeGLError(GL_OUT_OF_MEMORY, functionName, "out of memory");
    return nullptr;
  }

  if (!image->currentFrameKnownToBeOpaque())
    buf->canvas()->clear(SK_ColorTRANSPARENT);

  IntRect srcRect(IntPoint(), image->size());
  IntRect destRect(0, 0, size.width(), size.height());
  SkPaint paint;
  image->draw(buf->canvas(), paint, destRect, srcRect,
              DoNotRespectImageOrientation, Image::DoNotClampImageToSourceRect);
  return buf->newImageSnapshot(PreferNoAcceleration,
                               SnapshotReasonWebGLDrawImageIntoBuffer);
}

WebGLTexture* WebGLRenderingContextBase::validateTexImageBinding(
    const char* funcName,
    TexImageFunctionID functionID,
    GLenum target) {
  return validateTexture2DBinding(funcName, target);
}

const char* WebGLRenderingContextBase::getTexImageFunctionName(
    TexImageFunctionID funcName) {
  switch (funcName) {
    case TexImage2D:
      return "texImage2D";
    case TexSubImage2D:
      return "texSubImage2D";
    case TexSubImage3D:
      return "texSubImage3D";
    case TexImage3D:
      return "texImage3D";
    default:  // Adding default to prevent compile error
      return "";
  }
}

IntRect WebGLRenderingContextBase::sentinelEmptyRect() {
  // Return a rectangle with -1 width and height so we can recognize
  // it later and recalculate it based on the Image whose data we'll
  // upload. It's important that there be no possible differences in
  // the logic which computes the image's size.
  return IntRect(0, 0, -1, -1);
}

IntRect WebGLRenderingContextBase::safeGetImageSize(Image* image) {
  if (!image)
    return IntRect();

  return getTextureSourceSize(image);
}

IntRect WebGLRenderingContextBase::getImageDataSize(ImageData* pixels) {
  DCHECK(pixels);
  return getTextureSourceSize(pixels);
}

void WebGLRenderingContextBase::texImageHelperDOMArrayBufferView(
    TexImageFunctionID functionID,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    GLenum format,
    GLenum type,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    DOMArrayBufferView* pixels,
    NullDisposition nullDisposition,
    GLuint srcOffset) {
  const char* funcName = getTexImageFunctionName(functionID);
  if (isContextLost())
    return;
  if (!validateTexImageBinding(funcName, functionID, target))
    return;
  TexImageFunctionType functionType;
  if (functionID == TexImage2D || functionID == TexImage3D)
    functionType = TexImage;
  else
    functionType = TexSubImage;
  if (!validateTexFunc(funcName, functionType, SourceArrayBufferView, target,
                       level, internalformat, width, height, depth, border,
                       format, type, xoffset, yoffset, zoffset))
    return;
  TexImageDimension sourceType;
  if (functionID == TexImage2D || functionID == TexSubImage2D)
    sourceType = Tex2D;
  else
    sourceType = Tex3D;
  if (!validateTexFuncData(funcName, sourceType, level, width, height, depth,
                           format, type, pixels, nullDisposition, srcOffset))
    return;
  uint8_t* data =
      reinterpret_cast<uint8_t*>(pixels ? pixels->baseAddress() : 0);
  if (srcOffset) {
    DCHECK(pixels);
    // No need to check overflow because validateTexFuncData() already did.
    data += srcOffset * pixels->typeSize();
  }
  Vector<uint8_t> tempData;
  bool changeUnpackAlignment = false;
  if (data && (m_unpackFlipY || m_unpackPremultiplyAlpha)) {
    if (sourceType == Tex2D) {
      if (!WebGLImageConversion::extractTextureData(
              width, height, format, type, m_unpackAlignment, m_unpackFlipY,
              m_unpackPremultiplyAlpha, data, tempData))
        return;
      data = tempData.data();
    }
    changeUnpackAlignment = true;
  }
  // TODO(crbug.com/666064): implement flipY and premultiplyAlpha for
  // tex(Sub)3D.
  if (functionID == TexImage3D) {
    contextGL()->TexImage3D(target, level,
                            convertTexInternalFormat(internalformat, type),
                            width, height, depth, border, format, type, data);
    return;
  }
  if (functionID == TexSubImage3D) {
    contextGL()->TexSubImage3D(target, level, xoffset, yoffset, zoffset, width,
                               height, depth, format, type, data);
    return;
  }

  if (changeUnpackAlignment)
    resetUnpackParameters();
  if (functionID == TexImage2D)
    texImage2DBase(target, level, internalformat, width, height, border, format,
                   type, data);
  else if (functionID == TexSubImage2D)
    contextGL()->TexSubImage2D(target, level, xoffset, yoffset, width, height,
                               format, type, data);
  if (changeUnpackAlignment)
    restoreUnpackParameters();
}

void WebGLRenderingContextBase::texImage2D(GLenum target,
                                           GLint level,
                                           GLint internalformat,
                                           GLsizei width,
                                           GLsizei height,
                                           GLint border,
                                           GLenum format,
                                           GLenum type,
                                           DOMArrayBufferView* pixels) {
  texImageHelperDOMArrayBufferView(TexImage2D, target, level, internalformat,
                                   width, height, 1, border, format, type, 0, 0,
                                   0, pixels, NullAllowed, 0);
}

void WebGLRenderingContextBase::texImageHelperImageData(
    TexImageFunctionID functionID,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLint border,
    GLenum format,
    GLenum type,
    GLsizei depth,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    ImageData* pixels,
    const IntRect& sourceImageRect,
    GLint unpackImageHeight) {
  const char* funcName = getTexImageFunctionName(functionID);
  if (isContextLost())
    return;
  DCHECK(pixels);
  if (pixels->data()->bufferBase()->isNeutered()) {
    synthesizeGLError(GL_INVALID_VALUE, funcName,
                      "The source data has been neutered.");
    return;
  }
  if (!validateTexImageBinding(funcName, functionID, target))
    return;
  TexImageFunctionType functionType;
  if (functionID == TexImage2D || functionID == TexImage3D)
    functionType = TexImage;
  else
    functionType = TexSubImage;
  if (!validateTexFunc(funcName, functionType, SourceImageData, target, level,
                       internalformat, pixels->width(), pixels->height(), depth,
                       border, format, type, xoffset, yoffset, zoffset))
    return;

  bool selectingSubRectangle = false;
  if (!validateTexImageSubRectangle(funcName, functionID, pixels,
                                    sourceImageRect, depth, unpackImageHeight,
                                    &selectingSubRectangle)) {
    return;
  }
  // Adjust the source image rectangle if doing a y-flip.
  IntRect adjustedSourceImageRect = sourceImageRect;
  if (m_unpackFlipY) {
    adjustedSourceImageRect.setY(pixels->height() -
                                 adjustedSourceImageRect.maxY());
  }

  Vector<uint8_t> data;
  bool needConversion = true;
  // The data from ImageData is always of format RGBA8.
  // No conversion is needed if destination format is RGBA and type is
  // UNSIGNED_BYTE and no Flip or Premultiply operation is required.
  if (!m_unpackFlipY && !m_unpackPremultiplyAlpha && format == GL_RGBA &&
      type == GL_UNSIGNED_BYTE && !selectingSubRectangle && depth == 1) {
    needConversion = false;
  } else {
    if (type == GL_UNSIGNED_INT_10F_11F_11F_REV) {
      // The UNSIGNED_INT_10F_11F_11F_REV type pack/unpack isn't implemented.
      type = GL_FLOAT;
    }
    if (!WebGLImageConversion::extractImageData(
            pixels->data()->data(),
            WebGLImageConversion::DataFormat::DataFormatRGBA8, pixels->size(),
            adjustedSourceImageRect, depth, unpackImageHeight, format, type,
            m_unpackFlipY, m_unpackPremultiplyAlpha, data)) {
      synthesizeGLError(GL_INVALID_VALUE, funcName, "bad image data");
      return;
    }
  }
  resetUnpackParameters();
  const uint8_t* bytes = needConversion ? data.data() : pixels->data()->data();
  if (functionID == TexImage2D) {
    DCHECK_EQ(unpackImageHeight, 0);
    texImage2DBase(
        target, level, internalformat, adjustedSourceImageRect.width(),
        adjustedSourceImageRect.height(), border, format, type, bytes);
  } else if (functionID == TexSubImage2D) {
    DCHECK_EQ(unpackImageHeight, 0);
    contextGL()->TexSubImage2D(
        target, level, xoffset, yoffset, adjustedSourceImageRect.width(),
        adjustedSourceImageRect.height(), format, type, bytes);
  } else {
    GLint uploadHeight = adjustedSourceImageRect.height();
    if (unpackImageHeight) {
      // GL_UNPACK_IMAGE_HEIGHT overrides the passed-in height.
      uploadHeight = unpackImageHeight;
    }
    if (functionID == TexImage3D) {
      contextGL()->TexImage3D(target, level, internalformat,
                              adjustedSourceImageRect.width(), uploadHeight,
                              depth, border, format, type, bytes);
    } else {
      DCHECK_EQ(functionID, TexSubImage3D);
      contextGL()->TexSubImage3D(target, level, xoffset, yoffset, zoffset,
                                 adjustedSourceImageRect.width(), uploadHeight,
                                 depth, format, type, bytes);
    }
  }
  restoreUnpackParameters();
}

void WebGLRenderingContextBase::texImage2D(GLenum target,
                                           GLint level,
                                           GLint internalformat,
                                           GLenum format,
                                           GLenum type,
                                           ImageData* pixels) {
  texImageHelperImageData(TexImage2D, target, level, internalformat, 0, format,
                          type, 1, 0, 0, 0, pixels, getImageDataSize(pixels),
                          0);
}

void WebGLRenderingContextBase::texImageHelperHTMLImageElement(
    TexImageFunctionID functionID,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLenum format,
    GLenum type,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    HTMLImageElement* image,
    const IntRect& sourceImageRect,
    GLsizei depth,
    GLint unpackImageHeight,
    ExceptionState& exceptionState) {
  const char* funcName = getTexImageFunctionName(functionID);
  if (isContextLost())
    return;
  if (!validateHTMLImageElement(funcName, image, exceptionState))
    return;
  if (!validateTexImageBinding(funcName, functionID, target))
    return;

  RefPtr<Image> imageForRender = image->cachedImage()->getImage();
  if (imageForRender && imageForRender->isSVGImage())
    imageForRender = drawImageIntoBuffer(
        imageForRender.release(), image->width(), image->height(), funcName);

  TexImageFunctionType functionType;
  if (functionID == TexImage2D || functionID == TexImage3D)
    functionType = TexImage;
  else
    functionType = TexSubImage;
  if (!imageForRender ||
      !validateTexFunc(funcName, functionType, SourceHTMLImageElement, target,
                       level, internalformat, imageForRender->width(),
                       imageForRender->height(), depth, 0, format, type,
                       xoffset, yoffset, zoffset))
    return;

  texImageImpl(functionID, target, level, internalformat, xoffset, yoffset,
               zoffset, format, type, imageForRender.get(),
               WebGLImageConversion::HtmlDomImage, m_unpackFlipY,
               m_unpackPremultiplyAlpha, sourceImageRect, depth,
               unpackImageHeight);
}

void WebGLRenderingContextBase::texImage2D(GLenum target,
                                           GLint level,
                                           GLint internalformat,
                                           GLenum format,
                                           GLenum type,
                                           HTMLImageElement* image,
                                           ExceptionState& exceptionState) {
  texImageHelperHTMLImageElement(TexImage2D, target, level, internalformat,
                                 format, type, 0, 0, 0, image,
                                 sentinelEmptyRect(), 1, 0, exceptionState);
}

bool WebGLRenderingContextBase::canUseTexImageByGPU(
    TexImageFunctionID functionID,
    GLint internalformat,
    GLenum type) {
  if (functionID == TexImage2D &&
      (isFloatType(type) || isIntegerFormat(internalformat) ||
       isSRGBFormat(internalformat)))
    return false;
  // TODO(crbug.com/622958): Implement GPU-to-GPU path for WebGL 2 and more
  // internal formats.
  if (functionID == TexSubImage2D &&
      (isWebGL2OrHigher() || extensionEnabled(OESTextureFloatName) ||
       extensionEnabled(OESTextureHalfFloatName) ||
       extensionEnabled(EXTsRGBName)))
    return false;
  return true;
}

void WebGLRenderingContextBase::texImageCanvasByGPU(
    HTMLCanvasElement* canvas,
    GLuint targetTexture,
    GLenum targetInternalformat,
    GLenum targetType,
    GLint targetLevel,
    GLint xoffset,
    GLint yoffset,
    const IntRect& sourceSubRectangle) {
  if (!canvas->is3D()) {
    ImageBuffer* buffer = canvas->buffer();
    if (buffer &&
        !buffer->copyToPlatformTexture(
            contextGL(), targetTexture, targetInternalformat, targetType,
            targetLevel, m_unpackPremultiplyAlpha, m_unpackFlipY,
            IntPoint(xoffset, yoffset), sourceSubRectangle)) {
      NOTREACHED();
    }
  } else {
    WebGLRenderingContextBase* gl =
        toWebGLRenderingContextBase(canvas->renderingContext());
    ScopedTexture2DRestorer restorer(gl);
    if (!gl->drawingBuffer()->copyToPlatformTexture(
            contextGL(), targetTexture, targetInternalformat, targetType,
            targetLevel, m_unpackPremultiplyAlpha, !m_unpackFlipY,
            IntPoint(xoffset, yoffset), sourceSubRectangle, BackBuffer)) {
      NOTREACHED();
    }
  }
}

void WebGLRenderingContextBase::texImageByGPU(
    TexImageByGPUType functionType,
    WebGLTexture* texture,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLenum type,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    CanvasImageSource* image,
    const IntRect& sourceSubRectangle) {
  DCHECK(image->isCanvasElement() || image->isImageBitmap());
  int width = sourceSubRectangle.width();
  int height = sourceSubRectangle.height();

  ScopedTexture2DRestorer restorer(this);

  GLuint targetTexture = texture->object();
  GLenum targetType = type;
  GLenum targetInternalformat = internalformat;
  GLint targetLevel = level;
  bool possibleDirectCopy = false;
  if (functionType == TexImage2DByGPU) {
    possibleDirectCopy = Extensions3DUtil::canUseCopyTextureCHROMIUM(
        target, internalformat, type, level);
  }

  GLint copyXOffset = xoffset;
  GLint copyYOffset = yoffset;

  // if direct copy is not possible, create a temporary texture and then copy
  // from canvas to temporary texture to target texture.
  if (!possibleDirectCopy) {
    targetLevel = 0;
    targetInternalformat = GL_RGBA;
    targetType = GL_UNSIGNED_BYTE;
    contextGL()->GenTextures(1, &targetTexture);
    contextGL()->BindTexture(GL_TEXTURE_2D, targetTexture);
    contextGL()->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                               GL_NEAREST);
    contextGL()->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                               GL_NEAREST);
    contextGL()->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                               GL_CLAMP_TO_EDGE);
    contextGL()->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                               GL_CLAMP_TO_EDGE);
    contextGL()->TexImage2D(GL_TEXTURE_2D, 0, targetInternalformat, width,
                            height, 0, GL_RGBA, targetType, 0);
    copyXOffset = 0;
    copyYOffset = 0;
  }

  if (image->isCanvasElement()) {
    texImageCanvasByGPU(static_cast<HTMLCanvasElement*>(image), targetTexture,
                        targetInternalformat, targetType, targetLevel,
                        copyXOffset, copyYOffset, sourceSubRectangle);
  } else {
    texImageBitmapByGPU(static_cast<ImageBitmap*>(image), targetTexture,
                        targetInternalformat, targetType, targetLevel,
                        !m_unpackFlipY);
  }

  if (!possibleDirectCopy) {
    GLuint tmpFBO;
    contextGL()->GenFramebuffers(1, &tmpFBO);
    contextGL()->BindFramebuffer(GL_FRAMEBUFFER, tmpFBO);
    contextGL()->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                      GL_TEXTURE_2D, targetTexture, 0);
    contextGL()->BindTexture(texture->getTarget(), texture->object());
    if (functionType == TexImage2DByGPU) {
      contextGL()->CopyTexSubImage2D(target, level, 0, 0, 0, 0, width, height);
    } else if (functionType == TexSubImage2DByGPU) {
      contextGL()->CopyTexSubImage2D(target, level, xoffset, yoffset, 0, 0,
                                     width, height);
    } else if (functionType == TexSubImage3DByGPU) {
      contextGL()->CopyTexSubImage3D(target, level, xoffset, yoffset, zoffset,
                                     0, 0, width, height);
    }
    contextGL()->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                      GL_TEXTURE_2D, 0, 0);
    restoreCurrentFramebuffer();
    contextGL()->DeleteFramebuffers(1, &tmpFBO);
    contextGL()->DeleteTextures(1, &targetTexture);
  }
}

void WebGLRenderingContextBase::texImageHelperHTMLCanvasElement(
    TexImageFunctionID functionID,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLenum format,
    GLenum type,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    HTMLCanvasElement* canvas,
    const IntRect& sourceSubRectangle,
    GLsizei depth,
    GLint unpackImageHeight,
    ExceptionState& exceptionState) {
  const char* funcName = getTexImageFunctionName(functionID);
  if (isContextLost())
    return;
  if (!validateHTMLCanvasElement(funcName, canvas, exceptionState))
    return;
  WebGLTexture* texture = validateTexImageBinding(funcName, functionID, target);
  if (!texture)
    return;
  TexImageFunctionType functionType;
  if (functionID == TexImage2D)
    functionType = TexImage;
  else
    functionType = TexSubImage;
  if (!validateTexFunc(funcName, functionType, SourceHTMLCanvasElement, target,
                       level, internalformat, canvas->width(), canvas->height(),
                       depth, 0, format, type, xoffset, yoffset, zoffset))
    return;

  // Note that the sub-rectangle validation is needed for the GPU-GPU
  // copy case, but is redundant for the software upload case
  // (texImageImpl).
  bool selectingSubRectangle = false;
  if (!validateTexImageSubRectangle(
          funcName, functionID, canvas, sourceSubRectangle, depth,
          unpackImageHeight, &selectingSubRectangle)) {
    return;
  }

  if (functionID == TexImage2D || functionID == TexSubImage2D) {
    // texImageByGPU relies on copyTextureCHROMIUM which doesn't support
    // float/integer/sRGB internal format.
    // TODO(crbug.com/622958): relax the constrains if copyTextureCHROMIUM is
    // upgraded to handle more formats.
    if (!canvas->renderingContext() ||
        !canvas->renderingContext()->isAccelerated() ||
        !canUseTexImageByGPU(functionID, internalformat, type)) {
      // 2D canvas has only FrontBuffer.
      texImageImpl(functionID, target, level, internalformat, xoffset, yoffset,
                   zoffset, format, type,
                   canvas->copiedImage(FrontBuffer, PreferAcceleration).get(),
                   WebGLImageConversion::HtmlDomCanvas, m_unpackFlipY,
                   m_unpackPremultiplyAlpha, sourceSubRectangle, 1, 0);
      return;
    }

    // The GPU-GPU copy path uses the Y-up coordinate system.
    IntRect adjustedSourceSubRectangle = sourceSubRectangle;
    if (!m_unpackFlipY) {
      adjustedSourceSubRectangle.setY(canvas->height() -
                                      adjustedSourceSubRectangle.maxY());
    }

    if (functionID == TexImage2D) {
      texImage2DBase(target, level, internalformat, sourceSubRectangle.width(),
                     sourceSubRectangle.height(), 0, format, type, 0);
      texImageByGPU(TexImage2DByGPU, texture, target, level, internalformat,
                    type, 0, 0, 0, canvas, adjustedSourceSubRectangle);
    } else {
      texImageByGPU(TexSubImage2DByGPU, texture, target, level, GL_RGBA, type,
                    xoffset, yoffset, 0, canvas, adjustedSourceSubRectangle);
    }
  } else {
    // 3D functions.

    // TODO(zmo): Implement GPU-to-GPU copy path (crbug.com/612542).
    // Note that code will also be needed to copy to layers of 3D
    // textures, and elements of 2D texture arrays.
    texImageImpl(functionID, target, level, internalformat, xoffset, yoffset,
                 zoffset, format, type,
                 canvas->copiedImage(FrontBuffer, PreferAcceleration).get(),
                 WebGLImageConversion::HtmlDomCanvas, m_unpackFlipY,
                 m_unpackPremultiplyAlpha, sourceSubRectangle, depth,
                 unpackImageHeight);
  }
}

void WebGLRenderingContextBase::texImage2D(GLenum target,
                                           GLint level,
                                           GLint internalformat,
                                           GLenum format,
                                           GLenum type,
                                           HTMLCanvasElement* canvas,
                                           ExceptionState& exceptionState) {
  texImageHelperHTMLCanvasElement(
      TexImage2D, target, level, internalformat, format, type, 0, 0, 0, canvas,
      getTextureSourceSize(canvas), 1, 0, exceptionState);
}

PassRefPtr<Image> WebGLRenderingContextBase::videoFrameToImage(
    HTMLVideoElement* video) {
  IntSize size(video->videoWidth(), video->videoHeight());
  ImageBuffer* buf = m_generatedImageCache.imageBuffer(size);
  if (!buf) {
    synthesizeGLError(GL_OUT_OF_MEMORY, "texImage2D", "out of memory");
    return nullptr;
  }
  IntRect destRect(0, 0, size.width(), size.height());
  video->paintCurrentFrame(buf->canvas(), destRect, nullptr);
  return buf->newImageSnapshot();
}

void WebGLRenderingContextBase::texImageHelperHTMLVideoElement(
    TexImageFunctionID functionID,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLenum format,
    GLenum type,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    HTMLVideoElement* video,
    const IntRect& sourceImageRect,
    GLsizei depth,
    GLint unpackImageHeight,
    ExceptionState& exceptionState) {
  const char* funcName = getTexImageFunctionName(functionID);
  if (isContextLost())
    return;
  if (!validateHTMLVideoElement(funcName, video, exceptionState))
    return;
  WebGLTexture* texture = validateTexImageBinding(funcName, functionID, target);
  if (!texture)
    return;
  TexImageFunctionType functionType;
  if (functionID == TexImage2D || functionID == TexImage3D)
    functionType = TexImage;
  else
    functionType = TexSubImage;
  if (!validateTexFunc(funcName, functionType, SourceHTMLVideoElement, target,
                       level, internalformat, video->videoWidth(),
                       video->videoHeight(), 1, 0, format, type, xoffset,
                       yoffset, zoffset))
    return;

  bool sourceImageRectIsDefault =
      sourceImageRect == sentinelEmptyRect() ||
      sourceImageRect ==
          IntRect(0, 0, video->videoWidth(), video->videoHeight());
  if (functionID == TexImage2D && sourceImageRectIsDefault && depth == 1) {
    DCHECK_EQ(xoffset, 0);
    DCHECK_EQ(yoffset, 0);
    DCHECK_EQ(zoffset, 0);
    // Go through the fast path doing a GPU-GPU textures copy without a readback
    // to system memory if possible.  Otherwise, it will fall back to the normal
    // SW path.
    if (GL_TEXTURE_2D == target) {
      if (Extensions3DUtil::canUseCopyTextureCHROMIUM(target, internalformat,
                                                      type, level) &&
          video->copyVideoTextureToPlatformTexture(
              contextGL(), texture->object(), internalformat, type,
              m_unpackPremultiplyAlpha, m_unpackFlipY)) {
        return;
      }

      // Try using an accelerated image buffer, this allows YUV conversion to be
      // done on the GPU.
      std::unique_ptr<ImageBufferSurface> surface =
          wrapUnique(new AcceleratedImageBufferSurface(
              IntSize(video->videoWidth(), video->videoHeight())));
      if (surface->isValid()) {
        std::unique_ptr<ImageBuffer> imageBuffer(
            ImageBuffer::create(std::move(surface)));
        if (imageBuffer) {
          // The video element paints an RGBA frame into our surface here. By
          // using an AcceleratedImageBufferSurface, we enable the
          // WebMediaPlayer implementation to do any necessary color space
          // conversion on the GPU (though it
          // may still do a CPU conversion and upload the results).
          video->paintCurrentFrame(
              imageBuffer->canvas(),
              IntRect(0, 0, video->videoWidth(), video->videoHeight()),
              nullptr);

          // This is a straight GPU-GPU copy, any necessary color space
          // conversion was handled in the paintCurrentFrameInContext() call.

          // Note that copyToPlatformTexture no longer allocates the
          // destination texture.
          texImage2DBase(target, level, internalformat, video->videoWidth(),
                         video->videoHeight(), 0, format, type, nullptr);

          if (imageBuffer->copyToPlatformTexture(
                  contextGL(), texture->object(), internalformat, type, level,
                  m_unpackPremultiplyAlpha, m_unpackFlipY, IntPoint(0, 0),
                  IntRect(0, 0, video->videoWidth(), video->videoHeight()))) {
            return;
          }
        }
      }
    }
  }

  RefPtr<Image> image = videoFrameToImage(video);
  if (!image)
    return;
  texImageImpl(functionID, target, level, internalformat, xoffset, yoffset,
               zoffset, format, type, image.get(),
               WebGLImageConversion::HtmlDomVideo, m_unpackFlipY,
               m_unpackPremultiplyAlpha, sourceImageRect, depth,
               unpackImageHeight);
}

void WebGLRenderingContextBase::texImageBitmapByGPU(ImageBitmap* bitmap,
                                                    GLuint targetTexture,
                                                    GLenum targetInternalformat,
                                                    GLenum targetType,
                                                    GLint targetLevel,
                                                    bool flipY) {
  bitmap->bitmapImage()->copyToTexture(drawingBuffer()->contextProvider(),
                                       targetTexture, targetInternalformat,
                                       targetType, flipY);
}

void WebGLRenderingContextBase::texImage2D(GLenum target,
                                           GLint level,
                                           GLint internalformat,
                                           GLenum format,
                                           GLenum type,
                                           HTMLVideoElement* video,
                                           ExceptionState& exceptionState) {
  texImageHelperHTMLVideoElement(TexImage2D, target, level, internalformat,
                                 format, type, 0, 0, 0, video,
                                 sentinelEmptyRect(), 1, 0, exceptionState);
}

void WebGLRenderingContextBase::texImageHelperImageBitmap(
    TexImageFunctionID functionID,
    GLenum target,
    GLint level,
    GLint internalformat,
    GLenum format,
    GLenum type,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    ImageBitmap* bitmap,
    const IntRect& sourceSubRect,
    GLsizei depth,
    GLint unpackImageHeight,
    ExceptionState& exceptionState) {
  const char* funcName = getTexImageFunctionName(functionID);
  if (isContextLost())
    return;
  if (!validateImageBitmap(funcName, bitmap, exceptionState))
    return;
  WebGLTexture* texture = validateTexImageBinding(funcName, functionID, target);
  if (!texture)
    return;

  bool selectingSubRectangle = false;
  if (!validateTexImageSubRectangle(funcName, functionID, bitmap, sourceSubRect,
                                    depth, unpackImageHeight,
                                    &selectingSubRectangle)) {
    return;
  }

  TexImageFunctionType functionType;
  if (functionID == TexImage2D)
    functionType = TexImage;
  else
    functionType = TexSubImage;

  GLsizei width = sourceSubRect.width();
  GLsizei height = sourceSubRect.height();
  if (!validateTexFunc(funcName, functionType, SourceImageBitmap, target, level,
                       internalformat, width, height, depth, 0, format, type,
                       xoffset, yoffset, zoffset))
    return;
  ASSERT(bitmap->bitmapImage());

  // TODO(kbr): make this work for sub-rectangles of ImageBitmaps.
  if (functionID != TexSubImage3D && functionID != TexImage3D &&
      bitmap->isAccelerated() &&
      canUseTexImageByGPU(functionID, internalformat, type) &&
      !selectingSubRectangle) {
    if (functionID == TexImage2D) {
      texImage2DBase(target, level, internalformat, width, height, 0, format,
                     type, 0);
      texImageByGPU(TexImage2DByGPU, texture, target, level, internalformat,
                    type, 0, 0, 0, bitmap, sourceSubRect);
    } else if (functionID == TexSubImage2D) {
      texImageByGPU(TexSubImage2DByGPU, texture, target, level, GL_RGBA, type,
                    xoffset, yoffset, 0, bitmap, sourceSubRect);
    }
    return;
  }
  sk_sp<SkImage> skImage = bitmap->bitmapImage()->imageForCurrentFrame();
  SkPixmap pixmap;
  uint8_t* pixelDataPtr = nullptr;
  RefPtr<Uint8Array> pixelData;
  // In the case where an ImageBitmap is not texture backed, peekPixels() always
  // succeed.  However, when it is texture backed and !canUseTexImageByGPU, we
  // do a GPU read back.
  bool peekSucceed = skImage->peekPixels(&pixmap);
  if (peekSucceed) {
    pixelDataPtr = static_cast<uint8_t*>(pixmap.writable_addr());
  } else {
    pixelData = bitmap->copyBitmapData(
        bitmap->isPremultiplied() ? PremultiplyAlpha : DontPremultiplyAlpha);
    pixelDataPtr = pixelData->data();
  }
  Vector<uint8_t> data;
  bool needConversion = true;
  bool havePeekableRGBA =
      (peekSucceed &&
       pixmap.colorType() == SkColorType::kRGBA_8888_SkColorType);
  bool isPixelDataRGBA = (havePeekableRGBA || !peekSucceed);
  if (isPixelDataRGBA && format == GL_RGBA && type == GL_UNSIGNED_BYTE &&
      !selectingSubRectangle && depth == 1) {
    needConversion = false;
  } else {
    if (type == GL_UNSIGNED_INT_10F_11F_11F_REV) {
      // The UNSIGNED_INT_10F_11F_11F_REV type pack/unpack isn't implemented.
      type = GL_FLOAT;
    }
    // In the case of ImageBitmap, we do not need to apply flipY or
    // premultiplyAlpha.
    bool isPixelDataBGRA =
        pixmap.colorType() == SkColorType::kBGRA_8888_SkColorType;
    if ((isPixelDataBGRA &&
         !WebGLImageConversion::extractImageData(
             pixelDataPtr, WebGLImageConversion::DataFormat::DataFormatBGRA8,
             bitmap->size(), sourceSubRect, depth, unpackImageHeight, format,
             type, false, false, data)) ||
        (isPixelDataRGBA &&
         !WebGLImageConversion::extractImageData(
             pixelDataPtr, WebGLImageConversion::DataFormat::DataFormatRGBA8,
             bitmap->size(), sourceSubRect, depth, unpackImageHeight, format,
             type, false, false, data))) {
      synthesizeGLError(GL_INVALID_VALUE, funcName, "bad image data");
      return;
    }
  }
  resetUnpackParameters();
  if (functionID == TexImage2D) {
    texImage2DBase(target, level, internalformat, width, height, 0, format,
                   type, needConversion ? data.data() : pixelDataPtr);
  } else if (functionID == TexSubImage2D) {
    contextGL()->TexSubImage2D(target, level, xoffset, yoffset, width, height,
                               format, type,
                               needConversion ? data.data() : pixelDataPtr);
  } else if (functionID == TexImage3D) {
    contextGL()->TexImage3D(target, level, internalformat, width, height, depth,
                            0, format, type,
                            needConversion ? data.data() : pixelDataPtr);
  } else {
    DCHECK_EQ(functionID, TexSubImage3D);
    contextGL()->TexSubImage3D(target, level, xoffset, yoffset, zoffset, width,
                               height, depth, format, type,
                               needConversion ? data.data() : pixelDataPtr);
  }
  restoreUnpackParameters();
}

void WebGLRenderingContextBase::texImage2D(GLenum target,
                                           GLint level,
                                           GLint internalformat,
                                           GLenum format,
                                           GLenum type,
                                           ImageBitmap* bitmap,
                                           ExceptionState& exceptionState) {
  texImageHelperImageBitmap(TexImage2D, target, level, internalformat, format,
                            type, 0, 0, 0, bitmap, getTextureSourceSize(bitmap),
                            1, 0, exceptionState);
}

void WebGLRenderingContextBase::texParameter(GLenum target,
                                             GLenum pname,
                                             GLfloat paramf,
                                             GLint parami,
                                             bool isFloat) {
  if (isContextLost())
    return;
  if (!validateTextureBinding("texParameter", target))
    return;
  switch (pname) {
    case GL_TEXTURE_MIN_FILTER:
    case GL_TEXTURE_MAG_FILTER:
      break;
    case GL_TEXTURE_WRAP_R:
      // fall through to WRAP_S and WRAP_T for WebGL 2 or higher
      if (!isWebGL2OrHigher()) {
        synthesizeGLError(GL_INVALID_ENUM, "texParameter",
                          "invalid parameter name");
        return;
      }
    case GL_TEXTURE_WRAP_S:
    case GL_TEXTURE_WRAP_T:
      if ((isFloat && paramf != GL_CLAMP_TO_EDGE &&
           paramf != GL_MIRRORED_REPEAT && paramf != GL_REPEAT) ||
          (!isFloat && parami != GL_CLAMP_TO_EDGE &&
           parami != GL_MIRRORED_REPEAT && parami != GL_REPEAT)) {
        synthesizeGLError(GL_INVALID_ENUM, "texParameter", "invalid parameter");
        return;
      }
      break;
    case GL_TEXTURE_MAX_ANISOTROPY_EXT:  // EXT_texture_filter_anisotropic
      if (!extensionEnabled(EXTTextureFilterAnisotropicName)) {
        synthesizeGLError(
            GL_INVALID_ENUM, "texParameter",
            "invalid parameter, EXT_texture_filter_anisotropic not enabled");
        return;
      }
      break;
    case GL_TEXTURE_COMPARE_FUNC:
    case GL_TEXTURE_COMPARE_MODE:
    case GL_TEXTURE_BASE_LEVEL:
    case GL_TEXTURE_MAX_LEVEL:
    case GL_TEXTURE_MAX_LOD:
    case GL_TEXTURE_MIN_LOD:
      if (!isWebGL2OrHigher()) {
        synthesizeGLError(GL_INVALID_ENUM, "texParameter",
                          "invalid parameter name");
        return;
      }
      break;
    default:
      synthesizeGLError(GL_INVALID_ENUM, "texParameter",
                        "invalid parameter name");
      return;
  }
  if (isFloat) {
    contextGL()->TexParameterf(target, pname, paramf);
  } else {
    contextGL()->TexParameteri(target, pname, parami);
  }
}

void WebGLRenderingContextBase::texParameterf(GLenum target,
                                              GLenum pname,
                                              GLfloat param) {
  texParameter(target, pname, param, 0, true);
}

void WebGLRenderingContextBase::texParameteri(GLenum target,
                                              GLenum pname,
                                              GLint param) {
  texParameter(target, pname, 0, param, false);
}

void WebGLRenderingContextBase::texSubImage2D(GLenum target,
                                              GLint level,
                                              GLint xoffset,
                                              GLint yoffset,
                                              GLsizei width,
                                              GLsizei height,
                                              GLenum format,
                                              GLenum type,
                                              DOMArrayBufferView* pixels) {
  texImageHelperDOMArrayBufferView(TexSubImage2D, target, level, 0, width,
                                   height, 1, 0, format, type, xoffset, yoffset,
                                   0, pixels, NullNotAllowed, 0);
}

void WebGLRenderingContextBase::texSubImage2D(GLenum target,
                                              GLint level,
                                              GLint xoffset,
                                              GLint yoffset,
                                              GLenum format,
                                              GLenum type,
                                              ImageData* pixels) {
  texImageHelperImageData(TexSubImage2D, target, level, 0, 0, format, type, 1,
                          xoffset, yoffset, 0, pixels, getImageDataSize(pixels),
                          0);
}

void WebGLRenderingContextBase::texSubImage2D(GLenum target,
                                              GLint level,
                                              GLint xoffset,
                                              GLint yoffset,
                                              GLenum format,
                                              GLenum type,
                                              HTMLImageElement* image,
                                              ExceptionState& exceptionState) {
  texImageHelperHTMLImageElement(TexSubImage2D, target, level, 0, format, type,
                                 xoffset, yoffset, 0, image,
                                 sentinelEmptyRect(), 1, 0, exceptionState);
}

void WebGLRenderingContextBase::texSubImage2D(GLenum target,
                                              GLint level,
                                              GLint xoffset,
                                              GLint yoffset,
                                              GLenum format,
                                              GLenum type,
                                              HTMLCanvasElement* canvas,
                                              ExceptionState& exceptionState) {
  texImageHelperHTMLCanvasElement(
      TexSubImage2D, target, level, 0, format, type, xoffset, yoffset, 0,
      canvas, getTextureSourceSize(canvas), 1, 0, exceptionState);
}

void WebGLRenderingContextBase::texSubImage2D(GLenum target,
                                              GLint level,
                                              GLint xoffset,
                                              GLint yoffset,
                                              GLenum format,
                                              GLenum type,
                                              HTMLVideoElement* video,
                                              ExceptionState& exceptionState) {
  texImageHelperHTMLVideoElement(TexSubImage2D, target, level, 0, format, type,
                                 xoffset, yoffset, 0, video,
                                 sentinelEmptyRect(), 1, 0, exceptionState);
}

void WebGLRenderingContextBase::texSubImage2D(GLenum target,
                                              GLint level,
                                              GLint xoffset,
                                              GLint yoffset,
                                              GLenum format,
                                              GLenum type,
                                              ImageBitmap* bitmap,
                                              ExceptionState& exceptionState) {
  texImageHelperImageBitmap(TexSubImage2D, target, level, 0, format, type,
                            xoffset, yoffset, 0, bitmap,
                            getTextureSourceSize(bitmap), 1, 0, exceptionState);
}

void WebGLRenderingContextBase::uniform1f(const WebGLUniformLocation* location,
                                          GLfloat x) {
  if (isContextLost() || !location)
    return;

  if (location->program() != m_currentProgram) {
    synthesizeGLError(GL_INVALID_OPERATION, "uniform1f",
                      "location not for current program");
    return;
  }

  contextGL()->Uniform1f(location->location(), x);
}

void WebGLRenderingContextBase::uniform1fv(const WebGLUniformLocation* location,
                                           const FlexibleFloat32ArrayView& v) {
  if (isContextLost() ||
      !validateUniformParameters<WTF::Float32Array>("uniform1fv", location, v,
                                                    1))
    return;

  contextGL()->Uniform1fv(location->location(), v.length(),
                          v.dataMaybeOnStack());
}

void WebGLRenderingContextBase::uniform1fv(const WebGLUniformLocation* location,
                                           Vector<GLfloat>& v) {
  if (isContextLost() ||
      !validateUniformParameters("uniform1fv", location, v.data(), v.size(), 1))
    return;

  contextGL()->Uniform1fv(location->location(), v.size(), v.data());
}

void WebGLRenderingContextBase::uniform1i(const WebGLUniformLocation* location,
                                          GLint x) {
  if (isContextLost() || !location)
    return;

  if (location->program() != m_currentProgram) {
    synthesizeGLError(GL_INVALID_OPERATION, "uniform1i",
                      "location not for current program");
    return;
  }

  contextGL()->Uniform1i(location->location(), x);
}

void WebGLRenderingContextBase::uniform1iv(const WebGLUniformLocation* location,
                                           const FlexibleInt32ArrayView& v) {
  if (isContextLost() ||
      !validateUniformParameters<WTF::Int32Array>("uniform1iv", location, v, 1))
    return;

  contextGL()->Uniform1iv(location->location(), v.length(),
                          v.dataMaybeOnStack());
}

void WebGLRenderingContextBase::uniform1iv(const WebGLUniformLocation* location,
                                           Vector<GLint>& v) {
  if (isContextLost() ||
      !validateUniformParameters("uniform1iv", location, v.data(), v.size(), 1))
    return;

  contextGL()->Uniform1iv(location->location(), v.size(), v.data());
}

void WebGLRenderingContextBase::uniform2f(const WebGLUniformLocation* location,
                                          GLfloat x,
                                          GLfloat y) {
  if (isContextLost() || !location)
    return;

  if (location->program() != m_currentProgram) {
    synthesizeGLError(GL_INVALID_OPERATION, "uniform2f",
                      "location not for current program");
    return;
  }

  contextGL()->Uniform2f(location->location(), x, y);
}

void WebGLRenderingContextBase::uniform2fv(const WebGLUniformLocation* location,
                                           const FlexibleFloat32ArrayView& v) {
  if (isContextLost() ||
      !validateUniformParameters<WTF::Float32Array>("uniform2fv", location, v,
                                                    2))
    return;

  contextGL()->Uniform2fv(location->location(), v.length() >> 1,
                          v.dataMaybeOnStack());
}

void WebGLRenderingContextBase::uniform2fv(const WebGLUniformLocation* location,
                                           Vector<GLfloat>& v) {
  if (isContextLost() ||
      !validateUniformParameters("uniform2fv", location, v.data(), v.size(), 2))
    return;

  contextGL()->Uniform2fv(location->location(), v.size() >> 1, v.data());
}

void WebGLRenderingContextBase::uniform2i(const WebGLUniformLocation* location,
                                          GLint x,
                                          GLint y) {
  if (isContextLost() || !location)
    return;

  if (location->program() != m_currentProgram) {
    synthesizeGLError(GL_INVALID_OPERATION, "uniform2i",
                      "location not for current program");
    return;
  }

  contextGL()->Uniform2i(location->location(), x, y);
}

void WebGLRenderingContextBase::uniform2iv(const WebGLUniformLocation* location,
                                           const FlexibleInt32ArrayView& v) {
  if (isContextLost() ||
      !validateUniformParameters<WTF::Int32Array>("uniform2iv", location, v, 2))
    return;

  contextGL()->Uniform2iv(location->location(), v.length() >> 1,
                          v.dataMaybeOnStack());
}

void WebGLRenderingContextBase::uniform2iv(const WebGLUniformLocation* location,
                                           Vector<GLint>& v) {
  if (isContextLost() ||
      !validateUniformParameters("uniform2iv", location, v.data(), v.size(), 2))
    return;

  contextGL()->Uniform2iv(location->location(), v.size() >> 1, v.data());
}

void WebGLRenderingContextBase::uniform3f(const WebGLUniformLocation* location,
                                          GLfloat x,
                                          GLfloat y,
                                          GLfloat z) {
  if (isContextLost() || !location)
    return;

  if (location->program() != m_currentProgram) {
    synthesizeGLError(GL_INVALID_OPERATION, "uniform3f",
                      "location not for current program");
    return;
  }

  contextGL()->Uniform3f(location->location(), x, y, z);
}

void WebGLRenderingContextBase::uniform3fv(const WebGLUniformLocation* location,
                                           const FlexibleFloat32ArrayView& v) {
  if (isContextLost() ||
      !validateUniformParameters<WTF::Float32Array>("uniform3fv", location, v,
                                                    3))
    return;

  contextGL()->Uniform3fv(location->location(), v.length() / 3,
                          v.dataMaybeOnStack());
}

void WebGLRenderingContextBase::uniform3fv(const WebGLUniformLocation* location,
                                           Vector<GLfloat>& v) {
  if (isContextLost() ||
      !validateUniformParameters("uniform3fv", location, v.data(), v.size(), 3))
    return;

  contextGL()->Uniform3fv(location->location(), v.size() / 3, v.data());
}

void WebGLRenderingContextBase::uniform3i(const WebGLUniformLocation* location,
                                          GLint x,
                                          GLint y,
                                          GLint z) {
  if (isContextLost() || !location)
    return;

  if (location->program() != m_currentProgram) {
    synthesizeGLError(GL_INVALID_OPERATION, "uniform3i",
                      "location not for current program");
    return;
  }

  contextGL()->Uniform3i(location->location(), x, y, z);
}

void WebGLRenderingContextBase::uniform3iv(const WebGLUniformLocation* location,
                                           const FlexibleInt32ArrayView& v) {
  if (isContextLost() ||
      !validateUniformParameters<WTF::Int32Array>("uniform3iv", location, v, 3))
    return;

  contextGL()->Uniform3iv(location->location(), v.length() / 3,
                          v.dataMaybeOnStack());
}

void WebGLRenderingContextBase::uniform3iv(const WebGLUniformLocation* location,
                                           Vector<GLint>& v) {
  if (isContextLost() ||
      !validateUniformParameters("uniform3iv", location, v.data(), v.size(), 3))
    return;

  contextGL()->Uniform3iv(location->location(), v.size() / 3, v.data());
}

void WebGLRenderingContextBase::uniform4f(const WebGLUniformLocation* location,
                                          GLfloat x,
                                          GLfloat y,
                                          GLfloat z,
                                          GLfloat w) {
  if (isContextLost() || !location)
    return;

  if (location->program() != m_currentProgram) {
    synthesizeGLError(GL_INVALID_OPERATION, "uniform4f",
                      "location not for current program");
    return;
  }

  contextGL()->Uniform4f(location->location(), x, y, z, w);
}

void WebGLRenderingContextBase::uniform4fv(const WebGLUniformLocation* location,
                                           const FlexibleFloat32ArrayView& v) {
  if (isContextLost() ||
      !validateUniformParameters<WTF::Float32Array>("uniform4fv", location, v,
                                                    4))
    return;

  contextGL()->Uniform4fv(location->location(), v.length() >> 2,
                          v.dataMaybeOnStack());
}

void WebGLRenderingContextBase::uniform4fv(const WebGLUniformLocation* location,
                                           Vector<GLfloat>& v) {
  if (isContextLost() ||
      !validateUniformParameters("uniform4fv", location, v.data(), v.size(), 4))
    return;

  contextGL()->Uniform4fv(location->location(), v.size() >> 2, v.data());
}

void WebGLRenderingContextBase::uniform4i(const WebGLUniformLocation* location,
                                          GLint x,
                                          GLint y,
                                          GLint z,
                                          GLint w) {
  if (isContextLost() || !location)
    return;

  if (location->program() != m_currentProgram) {
    synthesizeGLError(GL_INVALID_OPERATION, "uniform4i",
                      "location not for current program");
    return;
  }

  contextGL()->Uniform4i(location->location(), x, y, z, w);
}

void WebGLRenderingContextBase::uniform4iv(const WebGLUniformLocation* location,
                                           const FlexibleInt32ArrayView& v) {
  if (isContextLost() ||
      !validateUniformParameters<WTF::Int32Array>("uniform4iv", location, v, 4))
    return;

  contextGL()->Uniform4iv(location->location(), v.length() >> 2,
                          v.dataMaybeOnStack());
}

void WebGLRenderingContextBase::uniform4iv(const WebGLUniformLocation* location,
                                           Vector<GLint>& v) {
  if (isContextLost() ||
      !validateUniformParameters("uniform4iv", location, v.data(), v.size(), 4))
    return;

  contextGL()->Uniform4iv(location->location(), v.size() >> 2, v.data());
}

void WebGLRenderingContextBase::uniformMatrix2fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    DOMFloat32Array* v) {
  if (isContextLost() ||
      !validateUniformMatrixParameters("uniformMatrix2fv", location, transpose,
                                       v, 4))
    return;
  contextGL()->UniformMatrix2fv(location->location(), v->length() >> 2,
                                transpose, v->data());
}

void WebGLRenderingContextBase::uniformMatrix2fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    Vector<GLfloat>& v) {
  if (isContextLost() ||
      !validateUniformMatrixParameters("uniformMatrix2fv", location, transpose,
                                       v.data(), v.size(), 4))
    return;
  contextGL()->UniformMatrix2fv(location->location(), v.size() >> 2, transpose,
                                v.data());
}

void WebGLRenderingContextBase::uniformMatrix3fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    DOMFloat32Array* v) {
  if (isContextLost() ||
      !validateUniformMatrixParameters("uniformMatrix3fv", location, transpose,
                                       v, 9))
    return;
  contextGL()->UniformMatrix3fv(location->location(), v->length() / 9,
                                transpose, v->data());
}

void WebGLRenderingContextBase::uniformMatrix3fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    Vector<GLfloat>& v) {
  if (isContextLost() ||
      !validateUniformMatrixParameters("uniformMatrix3fv", location, transpose,
                                       v.data(), v.size(), 9))
    return;
  contextGL()->UniformMatrix3fv(location->location(), v.size() / 9, transpose,
                                v.data());
}

void WebGLRenderingContextBase::uniformMatrix4fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    DOMFloat32Array* v) {
  if (isContextLost() ||
      !validateUniformMatrixParameters("uniformMatrix4fv", location, transpose,
                                       v, 16))
    return;
  contextGL()->UniformMatrix4fv(location->location(), v->length() >> 4,
                                transpose, v->data());
}

void WebGLRenderingContextBase::uniformMatrix4fv(
    const WebGLUniformLocation* location,
    GLboolean transpose,
    Vector<GLfloat>& v) {
  if (isContextLost() ||
      !validateUniformMatrixParameters("uniformMatrix4fv", location, transpose,
                                       v.data(), v.size(), 16))
    return;
  contextGL()->UniformMatrix4fv(location->location(), v.size() >> 4, transpose,
                                v.data());
}

void WebGLRenderingContextBase::useProgram(WebGLProgram* program) {
  bool deleted;
  if (!checkObjectToBeBound("useProgram", program, deleted))
    return;
  if (deleted)
    program = 0;
  if (program && !program->linkStatus(this)) {
    synthesizeGLError(GL_INVALID_OPERATION, "useProgram", "program not valid");
    return;
  }

  if (m_currentProgram != program) {
    if (m_currentProgram)
      m_currentProgram->onDetached(contextGL());
    m_currentProgram = program;
    contextGL()->UseProgram(objectOrZero(program));
    if (program)
      program->onAttached();
  }
}

void WebGLRenderingContextBase::validateProgram(WebGLProgram* program) {
  if (isContextLost() || !validateWebGLObject("validateProgram", program))
    return;
  contextGL()->ValidateProgram(objectOrZero(program));
}

void WebGLRenderingContextBase::setVertexAttribType(
    GLuint index,
    VertexAttribValueType type) {
  if (index < m_maxVertexAttribs)
    m_vertexAttribType[index] = type;
}

void WebGLRenderingContextBase::vertexAttrib1f(GLuint index, GLfloat v0) {
  if (isContextLost())
    return;
  contextGL()->VertexAttrib1f(index, v0);
  setVertexAttribType(index, Float32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib1fv(GLuint index,
                                                const DOMFloat32Array* v) {
  if (isContextLost())
    return;
  if (!v || v->length() < 1) {
    synthesizeGLError(GL_INVALID_VALUE, "vertexAttrib1fv", "invalid array");
    return;
  }
  contextGL()->VertexAttrib1fv(index, v->data());
  setVertexAttribType(index, Float32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib1fv(GLuint index,
                                                const Vector<GLfloat>& v) {
  if (isContextLost())
    return;
  if (v.size() < 1) {
    synthesizeGLError(GL_INVALID_VALUE, "vertexAttrib1fv", "invalid array");
    return;
  }
  contextGL()->VertexAttrib1fv(index, v.data());
  setVertexAttribType(index, Float32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib2f(GLuint index,
                                               GLfloat v0,
                                               GLfloat v1) {
  if (isContextLost())
    return;
  contextGL()->VertexAttrib2f(index, v0, v1);
  setVertexAttribType(index, Float32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib2fv(GLuint index,
                                                const DOMFloat32Array* v) {
  if (isContextLost())
    return;
  if (!v || v->length() < 2) {
    synthesizeGLError(GL_INVALID_VALUE, "vertexAttrib2fv", "invalid array");
    return;
  }
  contextGL()->VertexAttrib2fv(index, v->data());
  setVertexAttribType(index, Float32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib2fv(GLuint index,
                                                const Vector<GLfloat>& v) {
  if (isContextLost())
    return;
  if (v.size() < 2) {
    synthesizeGLError(GL_INVALID_VALUE, "vertexAttrib2fv", "invalid array");
    return;
  }
  contextGL()->VertexAttrib2fv(index, v.data());
  setVertexAttribType(index, Float32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib3f(GLuint index,
                                               GLfloat v0,
                                               GLfloat v1,
                                               GLfloat v2) {
  if (isContextLost())
    return;
  contextGL()->VertexAttrib3f(index, v0, v1, v2);
  setVertexAttribType(index, Float32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib3fv(GLuint index,
                                                const DOMFloat32Array* v) {
  if (isContextLost())
    return;
  if (!v || v->length() < 3) {
    synthesizeGLError(GL_INVALID_VALUE, "vertexAttrib3fv", "invalid array");
    return;
  }
  contextGL()->VertexAttrib3fv(index, v->data());
  setVertexAttribType(index, Float32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib3fv(GLuint index,
                                                const Vector<GLfloat>& v) {
  if (isContextLost())
    return;
  if (v.size() < 3) {
    synthesizeGLError(GL_INVALID_VALUE, "vertexAttrib3fv", "invalid array");
    return;
  }
  contextGL()->VertexAttrib3fv(index, v.data());
  setVertexAttribType(index, Float32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib4f(GLuint index,
                                               GLfloat v0,
                                               GLfloat v1,
                                               GLfloat v2,
                                               GLfloat v3) {
  if (isContextLost())
    return;
  contextGL()->VertexAttrib4f(index, v0, v1, v2, v3);
  setVertexAttribType(index, Float32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib4fv(GLuint index,
                                                const DOMFloat32Array* v) {
  if (isContextLost())
    return;
  if (!v || v->length() < 4) {
    synthesizeGLError(GL_INVALID_VALUE, "vertexAttrib4fv", "invalid array");
    return;
  }
  contextGL()->VertexAttrib4fv(index, v->data());
  setVertexAttribType(index, Float32ArrayType);
}

void WebGLRenderingContextBase::vertexAttrib4fv(GLuint index,
                                                const Vector<GLfloat>& v) {
  if (isContextLost())
    return;
  if (v.size() < 4) {
    synthesizeGLError(GL_INVALID_VALUE, "vertexAttrib4fv", "invalid array");
    return;
  }
  contextGL()->VertexAttrib4fv(index, v.data());
  setVertexAttribType(index, Float32ArrayType);
}

void WebGLRenderingContextBase::vertexAttribPointer(GLuint index,
                                                    GLint size,
                                                    GLenum type,
                                                    GLboolean normalized,
                                                    GLsizei stride,
                                                    long long offset) {
  if (isContextLost())
    return;
  if (index >= m_maxVertexAttribs) {
    synthesizeGLError(GL_INVALID_VALUE, "vertexAttribPointer",
                      "index out of range");
    return;
  }
  if (!validateValueFitNonNegInt32("vertexAttribPointer", "offset", offset))
    return;
  if (!m_boundArrayBuffer) {
    synthesizeGLError(GL_INVALID_OPERATION, "vertexAttribPointer",
                      "no bound ARRAY_BUFFER");
    return;
  }

  m_boundVertexArrayObject->setArrayBufferForAttrib(index,
                                                    m_boundArrayBuffer.get());
  contextGL()->VertexAttribPointer(
      index, size, type, normalized, stride,
      reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
}

void WebGLRenderingContextBase::vertexAttribDivisorANGLE(GLuint index,
                                                         GLuint divisor) {
  if (isContextLost())
    return;

  if (index >= m_maxVertexAttribs) {
    synthesizeGLError(GL_INVALID_VALUE, "vertexAttribDivisorANGLE",
                      "index out of range");
    return;
  }

  contextGL()->VertexAttribDivisorANGLE(index, divisor);
}

void WebGLRenderingContextBase::viewport(GLint x,
                                         GLint y,
                                         GLsizei width,
                                         GLsizei height) {
  if (isContextLost())
    return;
  contextGL()->Viewport(x, y, width, height);
}

// Added to provide a unified interface with CanvasRenderingContext2D. Prefer
// calling forceLostContext instead.
void WebGLRenderingContextBase::loseContext(LostContextMode mode) {
  forceLostContext(mode, Manual);
}

void WebGLRenderingContextBase::forceLostContext(
    LostContextMode mode,
    AutoRecoveryMethod autoRecoveryMethod) {
  if (isContextLost()) {
    synthesizeGLError(GL_INVALID_OPERATION, "loseContext",
                      "context already lost");
    return;
  }

  m_contextGroup->loseContextGroup(mode, autoRecoveryMethod);
}

void WebGLRenderingContextBase::loseContextImpl(
    WebGLRenderingContextBase::LostContextMode mode,
    AutoRecoveryMethod autoRecoveryMethod) {
  if (isContextLost())
    return;

  m_contextLostMode = mode;
  ASSERT(m_contextLostMode != NotLostContext);
  m_autoRecoveryMethod = autoRecoveryMethod;

  detachAndRemoveAllObjects();

  // Lose all the extensions.
  for (size_t i = 0; i < m_extensions.size(); ++i) {
    ExtensionTracker* tracker = m_extensions[i];
    tracker->loseExtension(false);
  }

  for (size_t i = 0; i < WebGLExtensionNameCount; ++i)
    m_extensionEnabled[i] = false;

  removeAllCompressedTextureFormats();

  if (mode != RealLostContext)
    destroyContext();

  ConsoleDisplayPreference display =
      (mode == RealLostContext) ? DisplayInConsole : DontDisplayInConsole;
  synthesizeGLError(GC3D_CONTEXT_LOST_WEBGL, "loseContext", "context lost",
                    display);

  // Don't allow restoration unless the context lost event has both been
  // dispatched and its default behavior prevented.
  m_restoreAllowed = false;
  deactivateContext(this);
  if (m_autoRecoveryMethod == WhenAvailable)
    addToEvictedList(this);

  // Always defer the dispatch of the context lost event, to implement
  // the spec behavior of queueing a task.
  m_dispatchContextLostEventTimer.startOneShot(0, BLINK_FROM_HERE);
}

void WebGLRenderingContextBase::forceRestoreContext() {
  if (!isContextLost()) {
    synthesizeGLError(GL_INVALID_OPERATION, "restoreContext",
                      "context not lost");
    return;
  }

  if (!m_restoreAllowed) {
    if (m_contextLostMode == WebGLLoseContextLostContext)
      synthesizeGLError(GL_INVALID_OPERATION, "restoreContext",
                        "context restoration not allowed");
    return;
  }

  if (!m_restoreTimer.isActive())
    m_restoreTimer.startOneShot(0, BLINK_FROM_HERE);
}

WebLayer* WebGLRenderingContextBase::platformLayer() const {
  return isContextLost() ? 0 : drawingBuffer()->platformLayer();
}

void WebGLRenderingContextBase::setFilterQuality(
    SkFilterQuality filterQuality) {
  if (!isContextLost() && drawingBuffer()) {
    drawingBuffer()->setFilterQuality(filterQuality);
  }
}

Extensions3DUtil* WebGLRenderingContextBase::extensionsUtil() {
  if (!m_extensionsUtil) {
    gpu::gles2::GLES2Interface* gl = contextGL();
    m_extensionsUtil = Extensions3DUtil::create(gl);
    // The only reason the ExtensionsUtil should be invalid is if the gl context
    // is lost.
    ASSERT(m_extensionsUtil->isValid() ||
           gl->GetGraphicsResetStatusKHR() != GL_NO_ERROR);
  }
  return m_extensionsUtil.get();
}

void WebGLRenderingContextBase::removeSharedObject(WebGLSharedObject* object) {
  m_contextGroup->removeObject(object);
}

void WebGLRenderingContextBase::addSharedObject(WebGLSharedObject* object) {
  ASSERT(!isContextLost());
  m_contextGroup->addObject(object);
}

void WebGLRenderingContextBase::removeContextObject(
    WebGLContextObject* object) {
  m_contextObjects.remove(object);
}

void WebGLRenderingContextBase::visitChildDOMWrappers(
    v8::Isolate* isolate,
    const v8::Persistent<v8::Object>& wrapper) {
  if (isContextLost()) {
    return;
  }
  DOMWrapperWorld::setWrapperReferencesInAllWorlds(wrapper, m_boundArrayBuffer,
                                                   isolate);
  DOMWrapperWorld::setWrapperReferencesInAllWorlds(
      wrapper, m_renderbufferBinding, isolate);

  for (auto& unit : m_textureUnits) {
    DOMWrapperWorld::setWrapperReferencesInAllWorlds(
        wrapper, unit.m_texture2DBinding, isolate);
    DOMWrapperWorld::setWrapperReferencesInAllWorlds(
        wrapper, unit.m_textureCubeMapBinding, isolate);
    DOMWrapperWorld::setWrapperReferencesInAllWorlds(
        wrapper, unit.m_texture3DBinding, isolate);
    DOMWrapperWorld::setWrapperReferencesInAllWorlds(
        wrapper, unit.m_texture2DArrayBinding, isolate);
  }

  DOMWrapperWorld::setWrapperReferencesInAllWorlds(
      wrapper, m_framebufferBinding, isolate);
  if (m_framebufferBinding) {
    m_framebufferBinding->visitChildDOMWrappers(isolate, wrapper);
  }

  DOMWrapperWorld::setWrapperReferencesInAllWorlds(wrapper, m_currentProgram,
                                                   isolate);
  if (m_currentProgram) {
    m_currentProgram->visitChildDOMWrappers(isolate, wrapper);
  }

  DOMWrapperWorld::setWrapperReferencesInAllWorlds(
      wrapper, m_boundVertexArrayObject, isolate);
  if (m_boundVertexArrayObject) {
    m_boundVertexArrayObject->visitChildDOMWrappers(isolate, wrapper);
  }

  for (ExtensionTracker* tracker : m_extensions) {
    WebGLExtension* extension = tracker->getExtensionObjectIfAlreadyEnabled();
    DOMWrapperWorld::setWrapperReferencesInAllWorlds(wrapper, extension,
                                                     isolate);
  }
}

void WebGLRenderingContextBase::addContextObject(WebGLContextObject* object) {
  ASSERT(!isContextLost());
  m_contextObjects.add(object);
}

void WebGLRenderingContextBase::detachAndRemoveAllObjects() {
  while (m_contextObjects.size() > 0) {
    // Following detachContext() will remove the iterated object from
    // |m_contextObjects|, and thus we need to look up begin() every time.
    auto it = m_contextObjects.begin();
    (*it)->detachContext();
  }
}

void WebGLRenderingContextBase::stop() {
  if (!isContextLost()) {
    // Never attempt to restore the context because the page is being torn down.
    forceLostContext(SyntheticLostContext, Manual);
  }
}

bool WebGLRenderingContextBase::DrawingBufferClientIsBoundForDraw() {
  return !m_framebufferBinding;
}

void WebGLRenderingContextBase::DrawingBufferClientRestoreScissorTest() {
  if (!contextGL())
    return;
  if (m_scissorEnabled)
    contextGL()->Enable(GL_SCISSOR_TEST);
  else
    contextGL()->Disable(GL_SCISSOR_TEST);
}

void WebGLRenderingContextBase::DrawingBufferClientRestoreMaskAndClearValues() {
  if (!contextGL())
    return;
  contextGL()->ColorMask(m_colorMask[0], m_colorMask[1], m_colorMask[2],
                         m_colorMask[3]);
  contextGL()->DepthMask(m_depthMask);
  contextGL()->StencilMaskSeparate(GL_FRONT, m_stencilMask);

  contextGL()->ClearColor(m_clearColor[0], m_clearColor[1], m_clearColor[2],
                          m_clearColor[3]);
  contextGL()->ClearDepthf(m_clearDepth);
  contextGL()->ClearStencil(m_clearStencil);
}

void WebGLRenderingContextBase::DrawingBufferClientRestorePixelPackAlignment() {
  if (!contextGL())
    return;
  contextGL()->PixelStorei(GL_PACK_ALIGNMENT, m_packAlignment);
}

void WebGLRenderingContextBase::DrawingBufferClientRestoreTexture2DBinding() {
  if (!contextGL())
    return;
  restoreCurrentTexture2D();
}

void WebGLRenderingContextBase::
    DrawingBufferClientRestoreRenderbufferBinding() {
  if (!contextGL())
    return;
  contextGL()->BindRenderbuffer(GL_RENDERBUFFER,
                                objectOrZero(m_renderbufferBinding.get()));
}

void WebGLRenderingContextBase::DrawingBufferClientRestoreFramebufferBinding() {
  if (!contextGL())
    return;
  restoreCurrentFramebuffer();
}

void WebGLRenderingContextBase::
    DrawingBufferClientRestorePixelUnpackBufferBinding() {}

ScriptValue WebGLRenderingContextBase::getBooleanParameter(
    ScriptState* scriptState,
    GLenum pname) {
  GLboolean value = 0;
  if (!isContextLost())
    contextGL()->GetBooleanv(pname, &value);
  return WebGLAny(scriptState, static_cast<bool>(value));
}

ScriptValue WebGLRenderingContextBase::getBooleanArrayParameter(
    ScriptState* scriptState,
    GLenum pname) {
  if (pname != GL_COLOR_WRITEMASK) {
    NOTIMPLEMENTED();
    return WebGLAny(scriptState, 0, 0);
  }
  GLboolean value[4] = {0};
  if (!isContextLost())
    contextGL()->GetBooleanv(pname, value);
  bool boolValue[4];
  for (int ii = 0; ii < 4; ++ii)
    boolValue[ii] = static_cast<bool>(value[ii]);
  return WebGLAny(scriptState, boolValue, 4);
}

ScriptValue WebGLRenderingContextBase::getFloatParameter(
    ScriptState* scriptState,
    GLenum pname) {
  GLfloat value = 0;
  if (!isContextLost())
    contextGL()->GetFloatv(pname, &value);
  return WebGLAny(scriptState, value);
}

ScriptValue WebGLRenderingContextBase::getIntParameter(ScriptState* scriptState,
                                                       GLenum pname) {
  GLint value = 0;
  if (!isContextLost()) {
    contextGL()->GetIntegerv(pname, &value);
    switch (pname) {
      case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
      case GL_IMPLEMENTATION_COLOR_READ_TYPE:
        if (value == 0) {
          // This indicates read framebuffer is incomplete and an
          // INVALID_OPERATION has been generated.
          return ScriptValue::createNull(scriptState);
        }
        break;
      default:
        break;
    }
  }
  return WebGLAny(scriptState, value);
}

ScriptValue WebGLRenderingContextBase::getInt64Parameter(
    ScriptState* scriptState,
    GLenum pname) {
  GLint64 value = 0;
  if (!isContextLost())
    contextGL()->GetInteger64v(pname, &value);
  return WebGLAny(scriptState, value);
}

ScriptValue WebGLRenderingContextBase::getUnsignedIntParameter(
    ScriptState* scriptState,
    GLenum pname) {
  GLint value = 0;
  if (!isContextLost())
    contextGL()->GetIntegerv(pname, &value);
  return WebGLAny(scriptState, static_cast<unsigned>(value));
}

ScriptValue WebGLRenderingContextBase::getWebGLFloatArrayParameter(
    ScriptState* scriptState,
    GLenum pname) {
  GLfloat value[4] = {0};
  if (!isContextLost())
    contextGL()->GetFloatv(pname, value);
  unsigned length = 0;
  switch (pname) {
    case GL_ALIASED_POINT_SIZE_RANGE:
    case GL_ALIASED_LINE_WIDTH_RANGE:
    case GL_DEPTH_RANGE:
      length = 2;
      break;
    case GL_BLEND_COLOR:
    case GL_COLOR_CLEAR_VALUE:
      length = 4;
      break;
    default:
      NOTIMPLEMENTED();
  }
  return WebGLAny(scriptState, DOMFloat32Array::create(value, length));
}

ScriptValue WebGLRenderingContextBase::getWebGLIntArrayParameter(
    ScriptState* scriptState,
    GLenum pname) {
  GLint value[4] = {0};
  if (!isContextLost())
    contextGL()->GetIntegerv(pname, value);
  unsigned length = 0;
  switch (pname) {
    case GL_MAX_VIEWPORT_DIMS:
      length = 2;
      break;
    case GL_SCISSOR_BOX:
    case GL_VIEWPORT:
      length = 4;
      break;
    default:
      NOTIMPLEMENTED();
  }
  return WebGLAny(scriptState, DOMInt32Array::create(value, length));
}

WebGLTexture* WebGLRenderingContextBase::validateTexture2DBinding(
    const char* functionName,
    GLenum target) {
  WebGLTexture* tex = nullptr;
  switch (target) {
    case GL_TEXTURE_2D:
      tex = m_textureUnits[m_activeTextureUnit].m_texture2DBinding.get();
      break;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
      tex = m_textureUnits[m_activeTextureUnit].m_textureCubeMapBinding.get();
      break;
    default:
      synthesizeGLError(GL_INVALID_ENUM, functionName,
                        "invalid texture target");
      return nullptr;
  }
  if (!tex)
    synthesizeGLError(GL_INVALID_OPERATION, functionName,
                      "no texture bound to target");
  return tex;
}

WebGLTexture* WebGLRenderingContextBase::validateTextureBinding(
    const char* functionName,
    GLenum target) {
  WebGLTexture* tex = nullptr;
  switch (target) {
    case GL_TEXTURE_2D:
      tex = m_textureUnits[m_activeTextureUnit].m_texture2DBinding.get();
      break;
    case GL_TEXTURE_CUBE_MAP:
      tex = m_textureUnits[m_activeTextureUnit].m_textureCubeMapBinding.get();
      break;
    case GL_TEXTURE_3D:
      if (!isWebGL2OrHigher()) {
        synthesizeGLError(GL_INVALID_ENUM, functionName,
                          "invalid texture target");
        return nullptr;
      }
      tex = m_textureUnits[m_activeTextureUnit].m_texture3DBinding.get();
      break;
    case GL_TEXTURE_2D_ARRAY:
      if (!isWebGL2OrHigher()) {
        synthesizeGLError(GL_INVALID_ENUM, functionName,
                          "invalid texture target");
        return nullptr;
      }
      tex = m_textureUnits[m_activeTextureUnit].m_texture2DArrayBinding.get();
      break;
    default:
      synthesizeGLError(GL_INVALID_ENUM, functionName,
                        "invalid texture target");
      return nullptr;
  }
  if (!tex)
    synthesizeGLError(GL_INVALID_OPERATION, functionName,
                      "no texture bound to target");
  return tex;
}

bool WebGLRenderingContextBase::validateLocationLength(const char* functionName,
                                                       const String& string) {
  const unsigned maxWebGLLocationLength = getMaxWebGLLocationLength();
  if (string.length() > maxWebGLLocationLength) {
    synthesizeGLError(GL_INVALID_VALUE, functionName, "location length > 256");
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::validateSize(const char* functionName,
                                             GLint x,
                                             GLint y,
                                             GLint z) {
  if (x < 0 || y < 0 || z < 0) {
    synthesizeGLError(GL_INVALID_VALUE, functionName, "size < 0");
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::validateString(const char* functionName,
                                               const String& string) {
  for (size_t i = 0; i < string.length(); ++i) {
    if (!validateCharacter(string[i])) {
      synthesizeGLError(GL_INVALID_VALUE, functionName, "string not ASCII");
      return false;
    }
  }
  return true;
}

bool WebGLRenderingContextBase::validateShaderSource(const String& string) {
  for (size_t i = 0; i < string.length(); ++i) {
    // line-continuation character \ is supported in WebGL 2.0.
    if (isWebGL2OrHigher() && string[i] == '\\') {
      continue;
    }
    if (!validateCharacter(string[i])) {
      synthesizeGLError(GL_INVALID_VALUE, "shaderSource", "string not ASCII");
      return false;
    }
  }
  return true;
}

void WebGLRenderingContextBase::addExtensionSupportedFormatsTypes() {
  if (!m_isOESTextureFloatFormatsTypesAdded &&
      extensionEnabled(OESTextureFloatName)) {
    ADD_VALUES_TO_SET(m_supportedTypes, kSupportedTypesOESTexFloat);
    ADD_VALUES_TO_SET(m_supportedTexImageSourceTypes,
                      kSupportedTypesOESTexFloat);
    m_isOESTextureFloatFormatsTypesAdded = true;
  }

  if (!m_isOESTextureHalfFloatFormatsTypesAdded &&
      extensionEnabled(OESTextureHalfFloatName)) {
    ADD_VALUES_TO_SET(m_supportedTypes, kSupportedTypesOESTexHalfFloat);
    ADD_VALUES_TO_SET(m_supportedTexImageSourceTypes,
                      kSupportedTypesOESTexHalfFloat);
    m_isOESTextureHalfFloatFormatsTypesAdded = true;
  }

  if (!m_isWebGLDepthTextureFormatsTypesAdded &&
      extensionEnabled(WebGLDepthTextureName)) {
    ADD_VALUES_TO_SET(m_supportedInternalFormats,
                      kSupportedInternalFormatsOESDepthTex);
    ADD_VALUES_TO_SET(m_supportedTexImageSourceInternalFormats,
                      kSupportedInternalFormatsOESDepthTex);
    ADD_VALUES_TO_SET(m_supportedFormats, kSupportedFormatsOESDepthTex);
    ADD_VALUES_TO_SET(m_supportedTexImageSourceFormats,
                      kSupportedFormatsOESDepthTex);
    ADD_VALUES_TO_SET(m_supportedTypes, kSupportedTypesOESDepthTex);
    ADD_VALUES_TO_SET(m_supportedTexImageSourceTypes,
                      kSupportedTypesOESDepthTex);
    m_isWebGLDepthTextureFormatsTypesAdded = true;
  }

  if (!m_isEXTsRGBFormatsTypesAdded && extensionEnabled(EXTsRGBName)) {
    ADD_VALUES_TO_SET(m_supportedInternalFormats,
                      kSupportedInternalFormatsEXTsRGB);
    ADD_VALUES_TO_SET(m_supportedTexImageSourceInternalFormats,
                      kSupportedInternalFormatsEXTsRGB);
    ADD_VALUES_TO_SET(m_supportedFormats, kSupportedFormatsEXTsRGB);
    ADD_VALUES_TO_SET(m_supportedTexImageSourceFormats,
                      kSupportedFormatsEXTsRGB);
    m_isEXTsRGBFormatsTypesAdded = true;
  }
}

bool WebGLRenderingContextBase::validateTexImageSourceFormatAndType(
    const char* functionName,
    TexImageFunctionType functionType,
    GLenum internalformat,
    GLenum format,
    GLenum type) {
  if (!m_isWebGL2TexImageSourceFormatsTypesAdded && isWebGL2OrHigher()) {
    ADD_VALUES_TO_SET(m_supportedTexImageSourceInternalFormats,
                      kSupportedInternalFormatsTexImageSourceES3);
    ADD_VALUES_TO_SET(m_supportedTexImageSourceFormats,
                      kSupportedFormatsTexImageSourceES3);
    ADD_VALUES_TO_SET(m_supportedTexImageSourceTypes,
                      kSupportedTypesTexImageSourceES3);
    m_isWebGL2TexImageSourceFormatsTypesAdded = true;
  }

  if (!isWebGL2OrHigher()) {
    addExtensionSupportedFormatsTypes();
  }

  if (internalformat != 0 &&
      m_supportedTexImageSourceInternalFormats.find(internalformat) ==
          m_supportedTexImageSourceInternalFormats.end()) {
    if (functionType == TexImage) {
      synthesizeGLError(GL_INVALID_VALUE, functionName,
                        "invalid internalformat");
    } else {
      synthesizeGLError(GL_INVALID_ENUM, functionName,
                        "invalid internalformat");
    }
    return false;
  }
  if (m_supportedTexImageSourceFormats.find(format) ==
      m_supportedTexImageSourceFormats.end()) {
    synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid format");
    return false;
  }
  if (m_supportedTexImageSourceTypes.find(type) ==
      m_supportedTexImageSourceTypes.end()) {
    synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid type");
    return false;
  }

  return true;
}

bool WebGLRenderingContextBase::validateTexFuncFormatAndType(
    const char* functionName,
    TexImageFunctionType functionType,
    GLenum internalformat,
    GLenum format,
    GLenum type,
    GLint level) {
  if (!m_isWebGL2FormatsTypesAdded && isWebGL2OrHigher()) {
    ADD_VALUES_TO_SET(m_supportedInternalFormats, kSupportedInternalFormatsES3);
    ADD_VALUES_TO_SET(m_supportedInternalFormats,
                      kSupportedInternalFormatsTexImageES3);
    ADD_VALUES_TO_SET(m_supportedFormats, kSupportedFormatsES3);
    ADD_VALUES_TO_SET(m_supportedTypes, kSupportedTypesES3);
    m_isWebGL2FormatsTypesAdded = true;
  }

  if (!isWebGL2OrHigher()) {
    addExtensionSupportedFormatsTypes();
  }

  if (internalformat != 0 &&
      m_supportedInternalFormats.find(internalformat) ==
          m_supportedInternalFormats.end()) {
    if (functionType == TexImage) {
      synthesizeGLError(GL_INVALID_VALUE, functionName,
                        "invalid internalformat");
    } else {
      synthesizeGLError(GL_INVALID_ENUM, functionName,
                        "invalid internalformat");
    }
    return false;
  }
  if (m_supportedFormats.find(format) == m_supportedFormats.end()) {
    synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid format");
    return false;
  }
  if (m_supportedTypes.find(type) == m_supportedTypes.end()) {
    synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid type");
    return false;
  }

  if (format == GL_DEPTH_COMPONENT && level > 0 && !isWebGL2OrHigher()) {
    synthesizeGLError(GL_INVALID_OPERATION, functionName,
                      "level must be 0 for DEPTH_COMPONENT format");
    return false;
  }
  if (format == GL_DEPTH_STENCIL_OES && level > 0 && !isWebGL2OrHigher()) {
    synthesizeGLError(GL_INVALID_OPERATION, functionName,
                      "level must be 0 for DEPTH_STENCIL format");
    return false;
  }

  return true;
}

GLint WebGLRenderingContextBase::getMaxTextureLevelForTarget(GLenum target) {
  switch (target) {
    case GL_TEXTURE_2D:
      return m_maxTextureLevel;
    case GL_TEXTURE_CUBE_MAP:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
      return m_maxCubeMapTextureLevel;
  }
  return 0;
}

bool WebGLRenderingContextBase::validateTexFuncLevel(const char* functionName,
                                                     GLenum target,
                                                     GLint level) {
  if (level < 0) {
    synthesizeGLError(GL_INVALID_VALUE, functionName, "level < 0");
    return false;
  }
  GLint maxLevel = getMaxTextureLevelForTarget(target);
  if (maxLevel && level >= maxLevel) {
    synthesizeGLError(GL_INVALID_VALUE, functionName, "level out of range");
    return false;
  }
  // This function only checks if level is legal, so we return true and don't
  // generate INVALID_ENUM if target is illegal.
  return true;
}

bool WebGLRenderingContextBase::validateTexFuncDimensions(
    const char* functionName,
    TexImageFunctionType functionType,
    GLenum target,
    GLint level,
    GLsizei width,
    GLsizei height,
    GLsizei depth) {
  if (width < 0 || height < 0 || depth < 0) {
    synthesizeGLError(GL_INVALID_VALUE, functionName,
                      "width, height or depth < 0");
    return false;
  }

  switch (target) {
    case GL_TEXTURE_2D:
      if (width > (m_maxTextureSize >> level) ||
          height > (m_maxTextureSize >> level)) {
        synthesizeGLError(GL_INVALID_VALUE, functionName,
                          "width or height out of range");
        return false;
      }
      break;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
      if (functionType != TexSubImage && width != height) {
        synthesizeGLError(GL_INVALID_VALUE, functionName,
                          "width != height for cube map");
        return false;
      }
      // No need to check height here. For texImage width == height.
      // For texSubImage that will be checked when checking yoffset + height is
      // in range.
      if (width > (m_maxCubeMapTextureSize >> level)) {
        synthesizeGLError(GL_INVALID_VALUE, functionName,
                          "width or height out of range for cube map");
        return false;
      }
      break;
    case GL_TEXTURE_3D:
      if (isWebGL2OrHigher()) {
        if (width > (m_max3DTextureSize >> level) ||
            height > (m_max3DTextureSize >> level) ||
            depth > (m_max3DTextureSize >> level)) {
          synthesizeGLError(GL_INVALID_VALUE, functionName,
                            "width, height or depth out of range");
          return false;
        }
        break;
      }
    case GL_TEXTURE_2D_ARRAY:
      if (isWebGL2OrHigher()) {
        if (width > (m_maxTextureSize >> level) ||
            height > (m_maxTextureSize >> level) ||
            depth > m_maxArrayTextureLayers) {
          synthesizeGLError(GL_INVALID_VALUE, functionName,
                            "width, height or depth out of range");
          return false;
        }
        break;
      }
    default:
      synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid target");
      return false;
  }
  return true;
}

bool WebGLRenderingContextBase::validateTexFuncParameters(
    const char* functionName,
    TexImageFunctionType functionType,
    TexFuncValidationSourceType sourceType,
    GLenum target,
    GLint level,
    GLenum internalformat,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    GLenum format,
    GLenum type) {
  // We absolutely have to validate the format and type combination.
  // The texImage2D entry points taking HTMLImage, etc. will produce
  // temporary data based on this combination, so it must be legal.
  if (sourceType == SourceHTMLImageElement ||
      sourceType == SourceHTMLCanvasElement ||
      sourceType == SourceHTMLVideoElement || sourceType == SourceImageData ||
      sourceType == SourceImageBitmap) {
    if (!validateTexImageSourceFormatAndType(functionName, functionType,
                                             internalformat, format, type)) {
      return false;
    }
  } else {
    if (!validateTexFuncFormatAndType(functionName, functionType,
                                      internalformat, format, type, level)) {
      return false;
    }
  }

  if (!validateTexFuncDimensions(functionName, functionType, target, level,
                                 width, height, depth))
    return false;

  if (border) {
    synthesizeGLError(GL_INVALID_VALUE, functionName, "border != 0");
    return false;
  }

  return true;
}

bool WebGLRenderingContextBase::validateTexFuncData(
    const char* functionName,
    TexImageDimension texDimension,
    GLint level,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLenum format,
    GLenum type,
    DOMArrayBufferView* pixels,
    NullDisposition disposition,
    GLuint srcOffset) {
  // All calling functions check isContextLost, so a duplicate check is not
  // needed here.
  if (!pixels) {
    DCHECK_NE(disposition, NullNotReachable);
    if (disposition == NullAllowed)
      return true;
    synthesizeGLError(GL_INVALID_VALUE, functionName, "no pixels");
    return false;
  }

  if (!validateSettableTexFormat(functionName, format))
    return false;

  switch (type) {
    case GL_BYTE:
      if (pixels->type() != DOMArrayBufferView::TypeInt8) {
        synthesizeGLError(GL_INVALID_OPERATION, functionName,
                          "type BYTE but ArrayBufferView not Int8Array");
        return false;
      }
      break;
    case GL_UNSIGNED_BYTE:
      if (pixels->type() != DOMArrayBufferView::TypeUint8) {
        synthesizeGLError(
            GL_INVALID_OPERATION, functionName,
            "type UNSIGNED_BYTE but ArrayBufferView not Uint8Array");
        return false;
      }
      break;
    case GL_SHORT:
      if (pixels->type() != DOMArrayBufferView::TypeInt16) {
        synthesizeGLError(GL_INVALID_OPERATION, functionName,
                          "type SHORT but ArrayBufferView not Int16Array");
        return false;
      }
      break;
    case GL_UNSIGNED_SHORT:
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_5_5_5_1:
      if (pixels->type() != DOMArrayBufferView::TypeUint16) {
        synthesizeGLError(
            GL_INVALID_OPERATION, functionName,
            "type UNSIGNED_SHORT but ArrayBufferView not Uint16Array");
        return false;
      }
      break;
    case GL_INT:
      if (pixels->type() != DOMArrayBufferView::TypeInt32) {
        synthesizeGLError(GL_INVALID_OPERATION, functionName,
                          "type INT but ArrayBufferView not Int32Array");
        return false;
      }
      break;
    case GL_UNSIGNED_INT:
    case GL_UNSIGNED_INT_2_10_10_10_REV:
    case GL_UNSIGNED_INT_10F_11F_11F_REV:
    case GL_UNSIGNED_INT_5_9_9_9_REV:
    case GL_UNSIGNED_INT_24_8:
      if (pixels->type() != DOMArrayBufferView::TypeUint32) {
        synthesizeGLError(
            GL_INVALID_OPERATION, functionName,
            "type UNSIGNED_INT but ArrayBufferView not Uint32Array");
        return false;
      }
      break;
    case GL_FLOAT:  // OES_texture_float
      if (pixels->type() != DOMArrayBufferView::TypeFloat32) {
        synthesizeGLError(GL_INVALID_OPERATION, functionName,
                          "type FLOAT but ArrayBufferView not Float32Array");
        return false;
      }
      break;
    case GL_HALF_FLOAT:
    case GL_HALF_FLOAT_OES:  // OES_texture_half_float
      // As per the specification, ArrayBufferView should be null or a
      // Uint16Array when OES_texture_half_float is enabled.
      if (pixels->type() != DOMArrayBufferView::TypeUint16) {
        synthesizeGLError(GL_INVALID_OPERATION, functionName,
                          "type HALF_FLOAT_OES but ArrayBufferView is not NULL "
                          "and not Uint16Array");
        return false;
      }
      break;
    case GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
      synthesizeGLError(GL_INVALID_OPERATION, functionName,
                        "type FLOAT_32_UNSIGNED_INT_24_8_REV but "
                        "ArrayBufferView is not NULL");
      return false;
    default:
      ASSERT_NOT_REACHED();
  }

  unsigned totalBytesRequired, skipBytes;
  GLenum error = WebGLImageConversion::computeImageSizeInBytes(
      format, type, width, height, depth,
      getUnpackPixelStoreParams(texDimension), &totalBytesRequired, 0,
      &skipBytes);
  if (error != GL_NO_ERROR) {
    synthesizeGLError(error, functionName, "invalid texture dimensions");
    return false;
  }
  CheckedNumeric<uint32_t> total = srcOffset;
  total *= pixels->typeSize();
  total += totalBytesRequired;
  total += skipBytes;
  if (!total.IsValid() || pixels->byteLength() < total.ValueOrDie()) {
    synthesizeGLError(GL_INVALID_OPERATION, functionName,
                      "ArrayBufferView not big enough for request");
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::validateCompressedTexFormat(
    const char* functionName,
    GLenum format) {
  if (!m_compressedTextureFormats.contains(format)) {
    synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid format");
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::validateStencilSettings(
    const char* functionName) {
  if (m_stencilMask != m_stencilMaskBack ||
      m_stencilFuncRef != m_stencilFuncRefBack ||
      m_stencilFuncMask != m_stencilFuncMaskBack) {
    synthesizeGLError(GL_INVALID_OPERATION, functionName,
                      "front and back stencils settings do not match");
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::validateStencilOrDepthFunc(
    const char* functionName,
    GLenum func) {
  switch (func) {
    case GL_NEVER:
    case GL_LESS:
    case GL_LEQUAL:
    case GL_GREATER:
    case GL_GEQUAL:
    case GL_EQUAL:
    case GL_NOTEQUAL:
    case GL_ALWAYS:
      return true;
    default:
      synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid function");
      return false;
  }
}

void WebGLRenderingContextBase::printGLErrorToConsole(const String& message) {
  if (!m_numGLErrorsToConsoleAllowed)
    return;

  --m_numGLErrorsToConsoleAllowed;
  printWarningToConsole(message);

  if (!m_numGLErrorsToConsoleAllowed)
    printWarningToConsole(
        "WebGL: too many errors, no more errors will be reported to the "
        "console for this context.");

  return;
}

void WebGLRenderingContextBase::printWarningToConsole(const String& message) {
  if (!canvas())
    return;
  canvas()->document().addConsoleMessage(ConsoleMessage::create(
      RenderingMessageSource, WarningMessageLevel, message));
}

bool WebGLRenderingContextBase::validateFramebufferFuncParameters(
    const char* functionName,
    GLenum target,
    GLenum attachment) {
  if (!validateFramebufferTarget(target)) {
    synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid target");
    return false;
  }
  switch (attachment) {
    case GL_COLOR_ATTACHMENT0:
    case GL_DEPTH_ATTACHMENT:
    case GL_STENCIL_ATTACHMENT:
    case GL_DEPTH_STENCIL_ATTACHMENT:
      break;
    default:
      if ((extensionEnabled(WebGLDrawBuffersName) || isWebGL2OrHigher()) &&
          attachment > GL_COLOR_ATTACHMENT0 &&
          attachment <
              static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + maxColorAttachments()))
        break;
      synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid attachment");
      return false;
  }
  return true;
}

bool WebGLRenderingContextBase::validateBlendEquation(const char* functionName,
                                                      GLenum mode) {
  switch (mode) {
    case GL_FUNC_ADD:
    case GL_FUNC_SUBTRACT:
    case GL_FUNC_REVERSE_SUBTRACT:
      return true;
    case GL_MIN_EXT:
    case GL_MAX_EXT:
      if (extensionEnabled(EXTBlendMinMaxName) || isWebGL2OrHigher())
        return true;
      synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid mode");
      return false;
    default:
      synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid mode");
      return false;
  }
}

bool WebGLRenderingContextBase::validateBlendFuncFactors(
    const char* functionName,
    GLenum src,
    GLenum dst) {
  if (((src == GL_CONSTANT_COLOR || src == GL_ONE_MINUS_CONSTANT_COLOR) &&
       (dst == GL_CONSTANT_ALPHA || dst == GL_ONE_MINUS_CONSTANT_ALPHA)) ||
      ((dst == GL_CONSTANT_COLOR || dst == GL_ONE_MINUS_CONSTANT_COLOR) &&
       (src == GL_CONSTANT_ALPHA || src == GL_ONE_MINUS_CONSTANT_ALPHA))) {
    synthesizeGLError(GL_INVALID_OPERATION, functionName,
                      "incompatible src and dst");
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::validateCapability(const char* functionName,
                                                   GLenum cap) {
  switch (cap) {
    case GL_BLEND:
    case GL_CULL_FACE:
    case GL_DEPTH_TEST:
    case GL_DITHER:
    case GL_POLYGON_OFFSET_FILL:
    case GL_SAMPLE_ALPHA_TO_COVERAGE:
    case GL_SAMPLE_COVERAGE:
    case GL_SCISSOR_TEST:
    case GL_STENCIL_TEST:
      return true;
    default:
      synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid capability");
      return false;
  }
}

bool WebGLRenderingContextBase::validateUniformParameters(
    const char* functionName,
    const WebGLUniformLocation* location,
    DOMFloat32Array* v,
    GLsizei requiredMinSize) {
  if (!v) {
    synthesizeGLError(GL_INVALID_VALUE, functionName, "no array");
    return false;
  }
  return validateUniformMatrixParameters(
      functionName, location, false, v->data(), v->length(), requiredMinSize);
}

bool WebGLRenderingContextBase::validateUniformParameters(
    const char* functionName,
    const WebGLUniformLocation* location,
    DOMInt32Array* v,
    GLsizei requiredMinSize) {
  if (!v) {
    synthesizeGLError(GL_INVALID_VALUE, functionName, "no array");
    return false;
  }
  return validateUniformMatrixParameters(
      functionName, location, false, v->data(), v->length(), requiredMinSize);
}

bool WebGLRenderingContextBase::validateUniformParameters(
    const char* functionName,
    const WebGLUniformLocation* location,
    void* v,
    GLsizei size,
    GLsizei requiredMinSize) {
  return validateUniformMatrixParameters(functionName, location, false, v, size,
                                         requiredMinSize);
}

bool WebGLRenderingContextBase::validateUniformMatrixParameters(
    const char* functionName,
    const WebGLUniformLocation* location,
    GLboolean transpose,
    DOMFloat32Array* v,
    GLsizei requiredMinSize) {
  if (!v) {
    synthesizeGLError(GL_INVALID_VALUE, functionName, "no array");
    return false;
  }
  return validateUniformMatrixParameters(functionName, location, transpose,
                                         v->data(), v->length(),
                                         requiredMinSize);
}

bool WebGLRenderingContextBase::validateUniformMatrixParameters(
    const char* functionName,
    const WebGLUniformLocation* location,
    GLboolean transpose,
    void* v,
    GLsizei size,
    GLsizei requiredMinSize) {
  if (!location)
    return false;
  if (location->program() != m_currentProgram) {
    synthesizeGLError(GL_INVALID_OPERATION, functionName,
                      "location is not from current program");
    return false;
  }
  if (!v) {
    synthesizeGLError(GL_INVALID_VALUE, functionName, "no array");
    return false;
  }
  if (transpose && !isWebGL2OrHigher()) {
    synthesizeGLError(GL_INVALID_VALUE, functionName, "transpose not FALSE");
    return false;
  }
  if (size < requiredMinSize || (size % requiredMinSize)) {
    synthesizeGLError(GL_INVALID_VALUE, functionName, "invalid size");
    return false;
  }
  return true;
}

WebGLBuffer* WebGLRenderingContextBase::validateBufferDataTarget(
    const char* functionName,
    GLenum target) {
  WebGLBuffer* buffer = nullptr;
  switch (target) {
    case GL_ELEMENT_ARRAY_BUFFER:
      buffer = m_boundVertexArrayObject->boundElementArrayBuffer();
      break;
    case GL_ARRAY_BUFFER:
      buffer = m_boundArrayBuffer.get();
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

bool WebGLRenderingContextBase::validateBufferDataUsage(
    const char* functionName,
    GLenum usage) {
  switch (usage) {
    case GL_STREAM_DRAW:
    case GL_STATIC_DRAW:
    case GL_DYNAMIC_DRAW:
      return true;
    default:
      synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid usage");
      return false;
  }
}

void WebGLRenderingContextBase::removeBoundBuffer(WebGLBuffer* buffer) {
  if (m_boundArrayBuffer == buffer)
    m_boundArrayBuffer = nullptr;

  m_boundVertexArrayObject->unbindBuffer(buffer);
}

bool WebGLRenderingContextBase::validateHTMLImageElement(
    const char* functionName,
    HTMLImageElement* image,
    ExceptionState& exceptionState) {
  if (!image || !image->cachedImage()) {
    synthesizeGLError(GL_INVALID_VALUE, functionName, "no image");
    return false;
  }
  const KURL& url = image->cachedImage()->response().url();
  if (url.isNull() || url.isEmpty() || !url.isValid()) {
    synthesizeGLError(GL_INVALID_VALUE, functionName, "invalid image");
    return false;
  }

  if (wouldTaintOrigin(image)) {
    exceptionState.throwSecurityError("The cross-origin image at " +
                                      url.elidedString() +
                                      " may not be loaded.");
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::validateHTMLCanvasElement(
    const char* functionName,
    HTMLCanvasElement* canvas,
    ExceptionState& exceptionState) {
  if (!canvas || !canvas->isPaintable()) {
    synthesizeGLError(GL_INVALID_VALUE, functionName, "no canvas");
    return false;
  }
  if (wouldTaintOrigin(canvas)) {
    exceptionState.throwSecurityError("Tainted canvases may not be loaded.");
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::validateHTMLVideoElement(
    const char* functionName,
    HTMLVideoElement* video,
    ExceptionState& exceptionState) {
  if (!video || !video->videoWidth() || !video->videoHeight()) {
    synthesizeGLError(GL_INVALID_VALUE, functionName, "no video");
    return false;
  }

  if (wouldTaintOrigin(video)) {
    exceptionState.throwSecurityError(
        "The video element contains cross-origin data, and may not be loaded.");
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::validateImageBitmap(
    const char* functionName,
    ImageBitmap* bitmap,
    ExceptionState& exceptionState) {
  if (bitmap->isNeutered()) {
    synthesizeGLError(GL_INVALID_VALUE, functionName,
                      "The source data has been detached.");
    return false;
  }
  if (!bitmap->originClean()) {
    exceptionState.throwSecurityError(
        "The ImageBitmap contains cross-origin data, and may not be loaded.");
    return false;
  }
  return true;
}

bool WebGLRenderingContextBase::validateDrawArrays(const char* functionName) {
  if (isContextLost())
    return false;

  if (!validateStencilSettings(functionName))
    return false;

  if (!validateRenderingState(functionName)) {
    return false;
  }

  const char* reason = "framebuffer incomplete";
  if (m_framebufferBinding &&
      m_framebufferBinding->checkDepthStencilStatus(&reason) !=
          GL_FRAMEBUFFER_COMPLETE) {
    synthesizeGLError(GL_INVALID_FRAMEBUFFER_OPERATION, functionName, reason);
    return false;
  }

  return true;
}

bool WebGLRenderingContextBase::validateDrawElements(const char* functionName,
                                                     GLenum type,
                                                     long long offset) {
  if (isContextLost())
    return false;

  if (!validateStencilSettings(functionName))
    return false;

  if (type == GL_UNSIGNED_INT && !isWebGL2OrHigher() &&
      !extensionEnabled(OESElementIndexUintName)) {
    synthesizeGLError(GL_INVALID_ENUM, functionName, "invalid type");
    return false;
  }

  if (!validateValueFitNonNegInt32(functionName, "offset", offset))
    return false;

  if (!validateRenderingState(functionName)) {
    return false;
  }

  const char* reason = "framebuffer incomplete";
  if (m_framebufferBinding &&
      m_framebufferBinding->checkDepthStencilStatus(&reason) !=
          GL_FRAMEBUFFER_COMPLETE) {
    synthesizeGLError(GL_INVALID_FRAMEBUFFER_OPERATION, functionName, reason);
    return false;
  }

  return true;
}

void WebGLRenderingContextBase::dispatchContextLostEvent(TimerBase*) {
  WebGLContextEvent* event = WebGLContextEvent::create(
      EventTypeNames::webglcontextlost, false, true, "");
  if (getOffscreenCanvas())
    getOffscreenCanvas()->dispatchEvent(event);
  else
    canvas()->dispatchEvent(event);
  m_restoreAllowed = event->defaultPrevented();
  if (m_restoreAllowed && !m_isHidden) {
    if (m_autoRecoveryMethod == Auto)
      m_restoreTimer.startOneShot(0, BLINK_FROM_HERE);
  }
}

void WebGLRenderingContextBase::maybeRestoreContext(TimerBase*) {
  ASSERT(isContextLost());

  // The rendering context is not restored unless the default behavior of the
  // webglcontextlost event was prevented earlier.
  //
  // Because of the way m_restoreTimer is set up for real vs. synthetic lost
  // context events, we don't have to worry about this test short-circuiting
  // the retry loop for real context lost events.
  if (!m_restoreAllowed)
    return;

  if (canvas()) {
    LocalFrame* frame = canvas()->document().frame();
    if (!frame)
      return;

    Settings* settings = frame->settings();

    if (!frame->loader().client()->allowWebGL(settings &&
                                              settings->webGLEnabled()))
      return;
  }

  // If the context was lost due to RealLostContext, we need to destroy the old
  // DrawingBuffer before creating new DrawingBuffer to ensure resource budget
  // enough.
  if (drawingBuffer()) {
    m_drawingBuffer->beginDestruction();
    m_drawingBuffer.clear();
  }

  Platform::ContextAttributes attributes =
      toPlatformContextAttributes(creationAttributes(), version());
  Platform::GraphicsInfo glInfo;
  std::unique_ptr<WebGraphicsContext3DProvider> contextProvider;
  const auto& url = canvas()
                        ? canvas()->document().topDocument().url()
                        : getOffscreenCanvas()->getExecutionContext()->url();
  if (isMainThread()) {
    contextProvider = wrapUnique(
        Platform::current()->createOffscreenGraphicsContext3DProvider(
            attributes, url, 0, &glInfo));
  } else {
    contextProvider =
        createContextProviderOnWorkerThread(attributes, &glInfo, url);
  }
  RefPtr<DrawingBuffer> buffer;
  if (contextProvider && contextProvider->bindToCurrentThread()) {
    // Construct a new drawing buffer with the new GL context.
    if (canvas()) {
      buffer = createDrawingBuffer(std::move(contextProvider),
                                   DrawingBuffer::AllowChromiumImage);
    } else {
      // Please refer to comment at Line 1040 in this file.
      buffer = createDrawingBuffer(std::move(contextProvider),
                                   DrawingBuffer::DisallowChromiumImage);
    }
    // If DrawingBuffer::create() fails to allocate a fbo, |drawingBuffer| is
    // set to null.
  }
  if (!buffer) {
    if (m_contextLostMode == RealLostContext) {
      m_restoreTimer.startOneShot(secondsBetweenRestoreAttempts,
                                  BLINK_FROM_HERE);
    } else {
      // This likely shouldn't happen but is the best way to report it to the
      // WebGL app.
      synthesizeGLError(GL_INVALID_OPERATION, "", "error restoring context");
    }
    return;
  }

  m_drawingBuffer = buffer.release();
  m_drawingBuffer->addNewMailboxCallback(
      WTF::bind(&WebGLRenderingContextBase::notifyCanvasContextChanged,
                wrapWeakPersistent(this)));

  drawingBuffer()->bind(GL_FRAMEBUFFER);
  m_lostContextErrors.clear();
  m_contextLostMode = NotLostContext;
  m_autoRecoveryMethod = Manual;
  m_restoreAllowed = false;
  removeFromEvictedList(this);

  setupFlags();
  initializeNewContext();
  markContextChanged(CanvasContextChanged);
  WebGLContextEvent* event = WebGLContextEvent::create(
      EventTypeNames::webglcontextrestored, false, true, "");
  if (canvas())
    canvas()->dispatchEvent(event);
  else
    getOffscreenCanvas()->dispatchEvent(event);
}

String WebGLRenderingContextBase::ensureNotNull(const String& text) const {
  if (text.isNull())
    return WTF::emptyString();
  return text;
}

WebGLRenderingContextBase::LRUImageBufferCache::LRUImageBufferCache(
    int capacity)
    : m_buffers(wrapArrayUnique(new std::unique_ptr<ImageBuffer>[ capacity ])),
      m_capacity(capacity) {}

ImageBuffer* WebGLRenderingContextBase::LRUImageBufferCache::imageBuffer(
    const IntSize& size) {
  int i;
  for (i = 0; i < m_capacity; ++i) {
    ImageBuffer* buf = m_buffers[i].get();
    if (!buf)
      break;
    if (buf->size() != size)
      continue;
    bubbleToFront(i);
    return buf;
  }

  std::unique_ptr<ImageBuffer> temp(ImageBuffer::create(size));
  if (!temp)
    return nullptr;
  i = std::min(m_capacity - 1, i);
  m_buffers[i] = std::move(temp);

  ImageBuffer* buf = m_buffers[i].get();
  bubbleToFront(i);
  return buf;
}

void WebGLRenderingContextBase::LRUImageBufferCache::bubbleToFront(int idx) {
  for (int i = idx; i > 0; --i)
    m_buffers[i].swap(m_buffers[i - 1]);
}

namespace {

String GetErrorString(GLenum error) {
  switch (error) {
    case GL_INVALID_ENUM:
      return "INVALID_ENUM";
    case GL_INVALID_VALUE:
      return "INVALID_VALUE";
    case GL_INVALID_OPERATION:
      return "INVALID_OPERATION";
    case GL_OUT_OF_MEMORY:
      return "OUT_OF_MEMORY";
    case GL_INVALID_FRAMEBUFFER_OPERATION:
      return "INVALID_FRAMEBUFFER_OPERATION";
    case GC3D_CONTEXT_LOST_WEBGL:
      return "CONTEXT_LOST_WEBGL";
    default:
      return String::format("WebGL ERROR(0x%04X)", error);
  }
}

}  // namespace

void WebGLRenderingContextBase::synthesizeGLError(
    GLenum error,
    const char* functionName,
    const char* description,
    ConsoleDisplayPreference display) {
  String errorType = GetErrorString(error);
  if (m_synthesizedErrorsToConsole && display == DisplayInConsole) {
    String message = String("WebGL: ") + errorType + ": " +
                     String(functionName) + ": " + String(description);
    printGLErrorToConsole(message);
  }
  if (!isContextLost()) {
    if (!m_syntheticErrors.contains(error))
      m_syntheticErrors.append(error);
  } else {
    if (!m_lostContextErrors.contains(error))
      m_lostContextErrors.append(error);
  }
  InspectorInstrumentation::didFireWebGLError(canvas(), errorType);
}

void WebGLRenderingContextBase::emitGLWarning(const char* functionName,
                                              const char* description) {
  if (m_synthesizedErrorsToConsole) {
    String message =
        String("WebGL: ") + String(functionName) + ": " + String(description);
    printGLErrorToConsole(message);
  }
  InspectorInstrumentation::didFireWebGLWarning(canvas());
}

void WebGLRenderingContextBase::applyStencilTest() {
  bool haveStencilBuffer = false;

  if (m_framebufferBinding) {
    haveStencilBuffer = m_framebufferBinding->hasStencilBuffer();
  } else {
    Nullable<WebGLContextAttributes> attributes;
    getContextAttributes(attributes);
    haveStencilBuffer = !attributes.isNull() && attributes.get().stencil();
  }
  enableOrDisable(GL_STENCIL_TEST, m_stencilEnabled && haveStencilBuffer);
}

void WebGLRenderingContextBase::enableOrDisable(GLenum capability,
                                                bool enable) {
  if (isContextLost())
    return;
  if (enable)
    contextGL()->Enable(capability);
  else
    contextGL()->Disable(capability);
}

IntSize WebGLRenderingContextBase::clampedCanvasSize() const {
  int width, height;
  if (canvas()) {
    width = canvas()->width();
    height = canvas()->height();
  } else {
    width = getOffscreenCanvas()->width();
    height = getOffscreenCanvas()->height();
  }
  return IntSize(clamp(width, 1, m_maxViewportDims[0]),
                 clamp(height, 1, m_maxViewportDims[1]));
}

GLint WebGLRenderingContextBase::maxDrawBuffers() {
  if (isContextLost() ||
      !(extensionEnabled(WebGLDrawBuffersName) || isWebGL2OrHigher()))
    return 0;
  if (!m_maxDrawBuffers)
    contextGL()->GetIntegerv(GL_MAX_DRAW_BUFFERS_EXT, &m_maxDrawBuffers);
  if (!m_maxColorAttachments)
    contextGL()->GetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT,
                             &m_maxColorAttachments);
  // WEBGL_draw_buffers requires MAX_COLOR_ATTACHMENTS >= MAX_DRAW_BUFFERS.
  return std::min(m_maxDrawBuffers, m_maxColorAttachments);
}

GLint WebGLRenderingContextBase::maxColorAttachments() {
  if (isContextLost() ||
      !(extensionEnabled(WebGLDrawBuffersName) || isWebGL2OrHigher()))
    return 0;
  if (!m_maxColorAttachments)
    contextGL()->GetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT,
                             &m_maxColorAttachments);
  return m_maxColorAttachments;
}

void WebGLRenderingContextBase::setBackDrawBuffer(GLenum buf) {
  m_backDrawBuffer = buf;
}

void WebGLRenderingContextBase::setFramebuffer(GLenum target,
                                               WebGLFramebuffer* buffer) {
  if (buffer)
    buffer->setHasEverBeenBound();

  if (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER) {
    m_framebufferBinding = buffer;
    applyStencilTest();
  }
  if (!buffer) {
    // Instead of binding fb 0, bind the drawing buffer.
    drawingBuffer()->bind(target);
  } else {
    contextGL()->BindFramebuffer(target, buffer->object());
  }
}

void WebGLRenderingContextBase::restoreCurrentFramebuffer() {
  bindFramebuffer(GL_FRAMEBUFFER, m_framebufferBinding.get());
}

void WebGLRenderingContextBase::restoreCurrentTexture2D() {
  bindTexture(GL_TEXTURE_2D,
              m_textureUnits[m_activeTextureUnit].m_texture2DBinding.get());
}

void WebGLRenderingContextBase::findNewMaxNonDefaultTextureUnit() {
  // Trace backwards from the current max to find the new max non-default
  // texture unit
  int startIndex = m_onePlusMaxNonDefaultTextureUnit - 1;
  for (int i = startIndex; i >= 0; --i) {
    if (m_textureUnits[i].m_texture2DBinding ||
        m_textureUnits[i].m_textureCubeMapBinding) {
      m_onePlusMaxNonDefaultTextureUnit = i + 1;
      return;
    }
  }
  m_onePlusMaxNonDefaultTextureUnit = 0;
}

DEFINE_TRACE(WebGLRenderingContextBase::TextureUnitState) {
  visitor->trace(m_texture2DBinding);
  visitor->trace(m_textureCubeMapBinding);
  visitor->trace(m_texture3DBinding);
  visitor->trace(m_texture2DArrayBinding);
}

DEFINE_TRACE(WebGLRenderingContextBase) {
  visitor->trace(m_contextObjects);
  visitor->trace(m_boundArrayBuffer);
  visitor->trace(m_defaultVertexArrayObject);
  visitor->trace(m_boundVertexArrayObject);
  visitor->trace(m_currentProgram);
  visitor->trace(m_framebufferBinding);
  visitor->trace(m_renderbufferBinding);
  visitor->trace(m_textureUnits);
  visitor->trace(m_extensions);
  CanvasRenderingContext::trace(visitor);
}

DEFINE_TRACE_WRAPPERS(WebGLRenderingContextBase) {
  if (isContextLost()) {
    return;
  }
  visitor->traceWrappers(m_boundArrayBuffer);
  visitor->traceWrappers(m_renderbufferBinding);
  visitor->traceWrappers(m_framebufferBinding);
  visitor->traceWrappers(m_currentProgram);
  visitor->traceWrappers(m_boundVertexArrayObject);
  // Trace wrappers explicitly here since TextureUnitState is not a heap
  // object, i.e., we cannot set its mark bits.
  for (auto& unit : m_textureUnits) {
    visitor->traceWrappers(unit.m_texture2DBinding);
    visitor->traceWrappers(unit.m_textureCubeMapBinding);
    visitor->traceWrappers(unit.m_texture3DBinding);
    visitor->traceWrappers(unit.m_texture2DArrayBinding);
  }
  for (ExtensionTracker* tracker : m_extensions) {
    WebGLExtension* extension = tracker->getExtensionObjectIfAlreadyEnabled();
    visitor->traceWrappers(extension);
  }
  CanvasRenderingContext::traceWrappers(visitor);
}

int WebGLRenderingContextBase::externallyAllocatedBytesPerPixel() {
  if (isContextLost())
    return 0;

  int bytesPerPixel = 4;
  int totalBytesPerPixel =
      bytesPerPixel * 2;  // WebGL's front and back color buffers.
  int samples = drawingBuffer() ? drawingBuffer()->sampleCount() : 0;
  Nullable<WebGLContextAttributes> attribs;
  getContextAttributes(attribs);
  if (!attribs.isNull()) {
    // Handle memory from WebGL multisample and depth/stencil buffers.
    // It is enabled only in case of explicit resolve assuming that there
    // is no memory overhead for MSAA on tile-based GPU arch.
    if (attribs.get().antialias() && samples > 0 &&
        drawingBuffer()->explicitResolveOfMultisampleData()) {
      if (attribs.get().depth() || attribs.get().stencil())
        totalBytesPerPixel +=
            samples * bytesPerPixel;  // depth/stencil multisample buffer
      totalBytesPerPixel +=
          samples * bytesPerPixel;  // color multisample buffer
    } else if (attribs.get().depth() || attribs.get().stencil()) {
      totalBytesPerPixel += bytesPerPixel;  // regular depth/stencil buffer
    }
  }

  return totalBytesPerPixel;
}

DrawingBuffer* WebGLRenderingContextBase::drawingBuffer() const {
  return m_drawingBuffer.get();
}

void WebGLRenderingContextBase::resetUnpackParameters() {
  if (m_unpackAlignment != 1)
    contextGL()->PixelStorei(GL_UNPACK_ALIGNMENT, 1);
}

void WebGLRenderingContextBase::restoreUnpackParameters() {
  if (m_unpackAlignment != 1)
    contextGL()->PixelStorei(GL_UNPACK_ALIGNMENT, m_unpackAlignment);
}

void WebGLRenderingContextBase::getHTMLOrOffscreenCanvas(
    HTMLCanvasElementOrOffscreenCanvas& result) const {
  if (canvas())
    result.setHTMLCanvasElement(canvas());
  else
    result.setOffscreenCanvas(getOffscreenCanvas());
}

}  // namespace blink
