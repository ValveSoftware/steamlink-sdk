// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/skia_bindings/gl_bindings_skia_cmd_buffer.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"

using gpu::gles2::GLES2Interface;

namespace {
template <typename R, typename... Args>
std::function<R(Args...)> gles_bind(R (GLES2Interface::*func)(Args...),
                                    GLES2Interface* gles2Interface) {
  return [func, gles2Interface](Args... args) {
    return (gles2Interface->*func)(args...);
  };
}
}  // namespace

namespace skia_bindings {

sk_sp<GrGLInterface> CreateGLES2InterfaceBindings(GLES2Interface* impl) {
  sk_sp<GrGLInterface> interface(new GrGLInterface);
  interface->fStandard = kGLES_GrGLStandard;
  interface->fExtensions.init(
      kGLES_GrGLStandard, gles_bind(&GLES2Interface::GetString, impl), nullptr,
      gles_bind(&GLES2Interface::GetIntegerv, impl));

  GrGLInterface::Functions* functions = &interface->fFunctions;
  functions->fActiveTexture = gles_bind(&GLES2Interface::ActiveTexture, impl);
  functions->fAttachShader = gles_bind(&GLES2Interface::AttachShader, impl);
  functions->fBindAttribLocation =
      gles_bind(&GLES2Interface::BindAttribLocation, impl);
  functions->fBindBuffer = gles_bind(&GLES2Interface::BindBuffer, impl);
  functions->fBindTexture = gles_bind(&GLES2Interface::BindTexture, impl);
  functions->fBindVertexArray =
      gles_bind(&GLES2Interface::BindVertexArrayOES, impl);
  functions->fBlendBarrier = gles_bind(&GLES2Interface::BlendBarrierKHR, impl);
  functions->fBlendColor = gles_bind(&GLES2Interface::BlendColor, impl);
  functions->fBlendEquation = gles_bind(&GLES2Interface::BlendEquation, impl);
  functions->fBlendFunc = gles_bind(&GLES2Interface::BlendFunc, impl);
  functions->fBufferData = gles_bind(&GLES2Interface::BufferData, impl);
  functions->fBufferSubData = gles_bind(&GLES2Interface::BufferSubData, impl);
  functions->fClear = gles_bind(&GLES2Interface::Clear, impl);
  functions->fClearColor = gles_bind(&GLES2Interface::ClearColor, impl);
  functions->fClearStencil = gles_bind(&GLES2Interface::ClearStencil, impl);
  functions->fColorMask = gles_bind(&GLES2Interface::ColorMask, impl);
  functions->fCompileShader = gles_bind(&GLES2Interface::CompileShader, impl);
  functions->fCompressedTexImage2D =
      gles_bind(&GLES2Interface::CompressedTexImage2D, impl);
  functions->fCopyTexSubImage2D =
      gles_bind(&GLES2Interface::CopyTexSubImage2D, impl);
  functions->fCreateProgram = gles_bind(&GLES2Interface::CreateProgram, impl);
  functions->fCreateShader = gles_bind(&GLES2Interface::CreateShader, impl);
  functions->fCullFace = gles_bind(&GLES2Interface::CullFace, impl);
  functions->fDeleteBuffers = gles_bind(&GLES2Interface::DeleteBuffers, impl);
  functions->fDeleteProgram = gles_bind(&GLES2Interface::DeleteProgram, impl);
  functions->fDeleteShader = gles_bind(&GLES2Interface::DeleteShader, impl);
  functions->fDeleteTextures = gles_bind(&GLES2Interface::DeleteTextures, impl);
  functions->fDeleteVertexArrays =
      gles_bind(&GLES2Interface::DeleteVertexArraysOES, impl);
  functions->fDepthMask = gles_bind(&GLES2Interface::DepthMask, impl);
  functions->fDisable = gles_bind(&GLES2Interface::Disable, impl);
  functions->fDisableVertexAttribArray =
      gles_bind(&GLES2Interface::DisableVertexAttribArray, impl);
  functions->fDiscardFramebuffer =
      gles_bind(&GLES2Interface::DiscardFramebufferEXT, impl);
  functions->fDrawArrays = gles_bind(&GLES2Interface::DrawArrays, impl);
  functions->fDrawElements = gles_bind(&GLES2Interface::DrawElements, impl);
  functions->fEnable = gles_bind(&GLES2Interface::Enable, impl);
  functions->fEnableVertexAttribArray =
      gles_bind(&GLES2Interface::EnableVertexAttribArray, impl);
  functions->fFinish = gles_bind(&GLES2Interface::Finish, impl);
  functions->fFlush = gles_bind(&GLES2Interface::Flush, impl);
  functions->fFrontFace = gles_bind(&GLES2Interface::FrontFace, impl);
  functions->fGenBuffers = gles_bind(&GLES2Interface::GenBuffers, impl);
  functions->fGenTextures = gles_bind(&GLES2Interface::GenTextures, impl);
  functions->fGenVertexArrays =
      gles_bind(&GLES2Interface::GenVertexArraysOES, impl);
  functions->fGetBufferParameteriv =
      gles_bind(&GLES2Interface::GetBufferParameteriv, impl);
  functions->fGetError = gles_bind(&GLES2Interface::GetError, impl);
  functions->fGetIntegerv = gles_bind(&GLES2Interface::GetIntegerv, impl);
  functions->fGetProgramInfoLog =
      gles_bind(&GLES2Interface::GetProgramInfoLog, impl);
  functions->fGetProgramiv = gles_bind(&GLES2Interface::GetProgramiv, impl);
  functions->fGetShaderInfoLog =
      gles_bind(&GLES2Interface::GetShaderInfoLog, impl);
  functions->fGetShaderiv = gles_bind(&GLES2Interface::GetShaderiv, impl);
  functions->fGetShaderPrecisionFormat =
      gles_bind(&GLES2Interface::GetShaderPrecisionFormat, impl);
  functions->fGetString = gles_bind(&GLES2Interface::GetString, impl);
  functions->fGetUniformLocation =
      gles_bind(&GLES2Interface::GetUniformLocation, impl);
  functions->fInsertEventMarker =
      gles_bind(&GLES2Interface::InsertEventMarkerEXT, impl);
  functions->fLineWidth = gles_bind(&GLES2Interface::LineWidth, impl);
  functions->fLinkProgram = gles_bind(&GLES2Interface::LinkProgram, impl);
  functions->fMapBufferSubData =
      gles_bind(&GLES2Interface::MapBufferSubDataCHROMIUM, impl);
  functions->fMapTexSubImage2D =
      gles_bind(&GLES2Interface::MapTexSubImage2DCHROMIUM, impl);
  functions->fPixelStorei = gles_bind(&GLES2Interface::PixelStorei, impl);
  functions->fPopGroupMarker =
      gles_bind(&GLES2Interface::PopGroupMarkerEXT, impl);
  functions->fPushGroupMarker =
      gles_bind(&GLES2Interface::PushGroupMarkerEXT, impl);
  functions->fReadPixels = gles_bind(&GLES2Interface::ReadPixels, impl);
  functions->fScissor = gles_bind(&GLES2Interface::Scissor, impl);
  functions->fShaderSource = gles_bind(&GLES2Interface::ShaderSource, impl);
  functions->fStencilFunc = gles_bind(&GLES2Interface::StencilFunc, impl);
  functions->fStencilFuncSeparate =
      gles_bind(&GLES2Interface::StencilFuncSeparate, impl);
  functions->fStencilMask = gles_bind(&GLES2Interface::StencilMask, impl);
  functions->fStencilMaskSeparate =
      gles_bind(&GLES2Interface::StencilMaskSeparate, impl);
  functions->fStencilOp = gles_bind(&GLES2Interface::StencilOp, impl);
  functions->fStencilOpSeparate =
      gles_bind(&GLES2Interface::StencilOpSeparate, impl);
  functions->fTexImage2D = gles_bind(&GLES2Interface::TexImage2D, impl);
  functions->fTexParameteri = gles_bind(&GLES2Interface::TexParameteri, impl);
  functions->fTexParameteriv = gles_bind(&GLES2Interface::TexParameteriv, impl);
  functions->fTexStorage2D = gles_bind(&GLES2Interface::TexStorage2DEXT, impl);
  functions->fTexSubImage2D = gles_bind(&GLES2Interface::TexSubImage2D, impl);
  functions->fUniform1f = gles_bind(&GLES2Interface::Uniform1f, impl);
  functions->fUniform1i = gles_bind(&GLES2Interface::Uniform1i, impl);
  functions->fUniform1fv = gles_bind(&GLES2Interface::Uniform1fv, impl);
  functions->fUniform1iv = gles_bind(&GLES2Interface::Uniform1iv, impl);
  functions->fUniform2f = gles_bind(&GLES2Interface::Uniform2f, impl);
  functions->fUniform2i = gles_bind(&GLES2Interface::Uniform2i, impl);
  functions->fUniform2fv = gles_bind(&GLES2Interface::Uniform2fv, impl);
  functions->fUniform2iv = gles_bind(&GLES2Interface::Uniform2iv, impl);
  functions->fUniform3f = gles_bind(&GLES2Interface::Uniform3f, impl);
  functions->fUniform3i = gles_bind(&GLES2Interface::Uniform3i, impl);
  functions->fUniform3fv = gles_bind(&GLES2Interface::Uniform3fv, impl);
  functions->fUniform3iv = gles_bind(&GLES2Interface::Uniform3iv, impl);
  functions->fUniform4f = gles_bind(&GLES2Interface::Uniform4f, impl);
  functions->fUniform4i = gles_bind(&GLES2Interface::Uniform4i, impl);
  functions->fUniform4fv = gles_bind(&GLES2Interface::Uniform4fv, impl);
  functions->fUniform4iv = gles_bind(&GLES2Interface::Uniform4iv, impl);
  functions->fUniformMatrix2fv =
      gles_bind(&GLES2Interface::UniformMatrix2fv, impl);
  functions->fUniformMatrix3fv =
      gles_bind(&GLES2Interface::UniformMatrix3fv, impl);
  functions->fUniformMatrix4fv =
      gles_bind(&GLES2Interface::UniformMatrix4fv, impl);
  functions->fUnmapBufferSubData =
      gles_bind(&GLES2Interface::UnmapBufferSubDataCHROMIUM, impl);
  functions->fUnmapTexSubImage2D =
      gles_bind(&GLES2Interface::UnmapTexSubImage2DCHROMIUM, impl);
  functions->fUseProgram = gles_bind(&GLES2Interface::UseProgram, impl);
  functions->fVertexAttrib1f = gles_bind(&GLES2Interface::VertexAttrib1f, impl);
  functions->fVertexAttrib2fv =
      gles_bind(&GLES2Interface::VertexAttrib2fv, impl);
  functions->fVertexAttrib3fv =
      gles_bind(&GLES2Interface::VertexAttrib3fv, impl);
  functions->fVertexAttrib4fv =
      gles_bind(&GLES2Interface::VertexAttrib4fv, impl);
  functions->fVertexAttribPointer =
      gles_bind(&GLES2Interface::VertexAttribPointer, impl);
  functions->fViewport = gles_bind(&GLES2Interface::Viewport, impl);
  functions->fBindFramebuffer =
      gles_bind(&GLES2Interface::BindFramebuffer, impl);
  functions->fBindRenderbuffer =
      gles_bind(&GLES2Interface::BindRenderbuffer, impl);
  functions->fCheckFramebufferStatus =
      gles_bind(&GLES2Interface::CheckFramebufferStatus, impl);
  functions->fDeleteFramebuffers =
      gles_bind(&GLES2Interface::DeleteFramebuffers, impl);
  functions->fDeleteRenderbuffers =
      gles_bind(&GLES2Interface::DeleteRenderbuffers, impl);
  functions->fFramebufferRenderbuffer =
      gles_bind(&GLES2Interface::FramebufferRenderbuffer, impl);
  functions->fFramebufferTexture2D =
      gles_bind(&GLES2Interface::FramebufferTexture2D, impl);
  functions->fFramebufferTexture2DMultisample =
      gles_bind(&GLES2Interface::FramebufferTexture2DMultisampleEXT, impl);
  functions->fGenFramebuffers =
      gles_bind(&GLES2Interface::GenFramebuffers, impl);
  functions->fGenRenderbuffers =
      gles_bind(&GLES2Interface::GenRenderbuffers, impl);
  functions->fGetFramebufferAttachmentParameteriv =
      gles_bind(&GLES2Interface::GetFramebufferAttachmentParameteriv, impl);
  functions->fGetRenderbufferParameteriv =
      gles_bind(&GLES2Interface::GetRenderbufferParameteriv, impl);
  functions->fRenderbufferStorage =
      gles_bind(&GLES2Interface::RenderbufferStorage, impl);
  functions->fRenderbufferStorageMultisample =
      gles_bind(&GLES2Interface::RenderbufferStorageMultisampleCHROMIUM, impl);
  functions->fRenderbufferStorageMultisampleES2EXT =
      gles_bind(&GLES2Interface::RenderbufferStorageMultisampleEXT, impl);
  functions->fBindFragDataLocation =
      gles_bind(&GLES2Interface::BindFragDataLocationEXT, impl);
  functions->fBindFragDataLocationIndexed =
      gles_bind(&GLES2Interface::BindFragDataLocationIndexedEXT, impl);
  functions->fBindUniformLocation =
      gles_bind(&GLES2Interface::BindUniformLocationCHROMIUM, impl);
  functions->fBlitFramebuffer =
      gles_bind(&GLES2Interface::BlitFramebufferCHROMIUM, impl);
  functions->fGenerateMipmap = gles_bind(&GLES2Interface::GenerateMipmap, impl);
  functions->fMatrixLoadf =
      gles_bind(&GLES2Interface::MatrixLoadfCHROMIUM, impl);
  functions->fMatrixLoadIdentity =
      gles_bind(&GLES2Interface::MatrixLoadIdentityCHROMIUM, impl);
  functions->fPathCommands =
      gles_bind(&GLES2Interface::PathCommandsCHROMIUM, impl);
  functions->fPathParameteri =
      gles_bind(&GLES2Interface::PathParameteriCHROMIUM, impl);
  functions->fPathParameterf =
      gles_bind(&GLES2Interface::PathParameterfCHROMIUM, impl);
  functions->fGenPaths = gles_bind(&GLES2Interface::GenPathsCHROMIUM, impl);
  functions->fIsPath = gles_bind(&GLES2Interface::IsPathCHROMIUM, impl);
  functions->fDeletePaths =
      gles_bind(&GLES2Interface::DeletePathsCHROMIUM, impl);
  functions->fPathStencilFunc =
      gles_bind(&GLES2Interface::PathStencilFuncCHROMIUM, impl);
  functions->fStencilFillPath =
      gles_bind(&GLES2Interface::StencilFillPathCHROMIUM, impl);
  functions->fStencilStrokePath =
      gles_bind(&GLES2Interface::StencilStrokePathCHROMIUM, impl);
  functions->fCoverFillPath =
      gles_bind(&GLES2Interface::CoverFillPathCHROMIUM, impl);
  functions->fCoverStrokePath =
      gles_bind(&GLES2Interface::CoverStrokePathCHROMIUM, impl);
  functions->fStencilThenCoverFillPath =
      gles_bind(&GLES2Interface::StencilThenCoverFillPathCHROMIUM, impl);
  functions->fStencilThenCoverStrokePath =
      gles_bind(&GLES2Interface::StencilThenCoverStrokePathCHROMIUM, impl);
  functions->fStencilFillPathInstanced =
      gles_bind(&GLES2Interface::StencilFillPathInstancedCHROMIUM, impl);
  functions->fStencilStrokePathInstanced =
      gles_bind(&GLES2Interface::StencilStrokePathInstancedCHROMIUM, impl);
  functions->fCoverFillPathInstanced =
      gles_bind(&GLES2Interface::CoverFillPathInstancedCHROMIUM, impl);
  functions->fCoverStrokePathInstanced =
      gles_bind(&GLES2Interface::CoverStrokePathInstancedCHROMIUM, impl);
  functions->fStencilThenCoverFillPathInstanced = gles_bind(
      &GLES2Interface::StencilThenCoverFillPathInstancedCHROMIUM, impl);
  functions->fStencilThenCoverStrokePathInstanced = gles_bind(
      &GLES2Interface::StencilThenCoverStrokePathInstancedCHROMIUM, impl);
  functions->fProgramPathFragmentInputGen =
      gles_bind(&GLES2Interface::ProgramPathFragmentInputGenCHROMIUM, impl);
  functions->fBindFragmentInputLocation =
      gles_bind(&GLES2Interface::BindFragmentInputLocationCHROMIUM, impl);
  functions->fCoverageModulation =
      gles_bind(&GLES2Interface::CoverageModulationCHROMIUM, impl);
  return interface;
}

}  // namespace skia_bindings
