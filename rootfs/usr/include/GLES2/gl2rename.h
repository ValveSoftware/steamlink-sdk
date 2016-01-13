/****************************************************************************
*
*    Copyright 2012 - 2014 Vivante Corporation, Sunnyvale, California.
*    All Rights Reserved.
*
*    Permission is hereby granted, free of charge, to any person obtaining
*    a copy of this software and associated documentation files (the
*    'Software'), to deal in the Software without restriction, including
*    without limitation the rights to use, copy, modify, merge, publish,
*    distribute, sub license, and/or sell copies of the Software, and to
*    permit persons to whom the Software is furnished to do so, subject
*    to the following conditions:
*
*    The above copyright notice and this permission notice (including the
*    next paragraph) shall be included in all copies or substantial
*    portions of the Software.
*
*    THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
*    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
*    IN NO EVENT SHALL VIVANTE AND/OR ITS SUPPLIERS BE LIABLE FOR ANY
*    CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
*    TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
*    SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*****************************************************************************/



#ifndef __gl2rename_h_
#define __gl2rename_h_

#if defined(_GL_2_APPENDIX)

#define _GL_2_RENAME_2(api, appendix)    api ## appendix
#define _GL_2_RENAME_1(api, appendix)    _GL_2_RENAME_2(api, appendix)
#define gcmGLES2(api)                    _GL_2_RENAME_1(api, _GL_2_APPENDIX)

#define glActiveTexture                    gcmGLES2(glActiveTexture)
#define glAttachShader                    gcmGLES2(glAttachShader)
#define glBindAttribLocation            gcmGLES2(glBindAttribLocation)
#define glBindBuffer                    gcmGLES2(glBindBuffer)
#define glBindFramebuffer                gcmGLES2(glBindFramebuffer)
#define glBindRenderbuffer                gcmGLES2(glBindRenderbuffer)
#define glBindTexture                    gcmGLES2(glBindTexture)
#define glBindVertexArrayOES            gcmGLES2(glBindVertexArrayOES)
#define glBlendColor                    gcmGLES2(glBlendColor)
#define glBlendEquation                    gcmGLES2(glBlendEquation)
#define glBlendEquationSeparate            gcmGLES2(glBlendEquationSeparate)
#define glBlendFunc                        gcmGLES2(glBlendFunc)
#define glBlendFuncSeparate                gcmGLES2(glBlendFuncSeparate)
#define glBufferData                    gcmGLES2(glBufferData)
#define glBufferSubData                    gcmGLES2(glBufferSubData)
#define glCheckFramebufferStatus        gcmGLES2(glCheckFramebufferStatus)
#define glClear                            gcmGLES2(glClear)
#define glClearColor                    gcmGLES2(glClearColor)
#define glClearDepthf                    gcmGLES2(glClearDepthf)
#define glClearStencil                    gcmGLES2(glClearStencil)
#define glColorMask                        gcmGLES2(glColorMask)
#define glCompileShader                    gcmGLES2(glCompileShader)
#define glCompressedTexImage2D            gcmGLES2(glCompressedTexImage2D)
#define glCompressedTexImage3DOES        gcmGLES2(glCompressedTexImage3DOES)
#define glCompressedTexSubImage2D        gcmGLES2(glCompressedTexSubImage2D)
#define glCompressedTexSubImage3DOES    gcmGLES2(glCompressedTexSubImage3DOES)
#define glCopyTexImage2D                gcmGLES2(glCopyTexImage2D)
#define glCopyTexSubImage2D                gcmGLES2(glCopyTexSubImage2D)
#define glCopyTexSubImage3DOES            gcmGLES2(glCopyTexSubImage3DOES)
#define glCreateProgram                    gcmGLES2(glCreateProgram)
#define glCreateShader                    gcmGLES2(glCreateShader)
#define glCullFace                        gcmGLES2(glCullFace)
#define glDeleteBuffers                    gcmGLES2(glDeleteBuffers)
#define glDeleteFramebuffers            gcmGLES2(glDeleteFramebuffers)
#define glDeleteProgram                    gcmGLES2(glDeleteProgram)
#define glDeleteRenderbuffers            gcmGLES2(glDeleteRenderbuffers)
#define glDeleteShader                    gcmGLES2(glDeleteShader)
#define glDeleteTextures                gcmGLES2(glDeleteTextures)
#define glDeleteVertexArraysOES            gcmGLES2(glDeleteVertexArraysOES)
#define glDepthFunc                        gcmGLES2(glDepthFunc)
#define glDepthMask                        gcmGLES2(glDepthMask)
#define glDepthRangef                    gcmGLES2(glDepthRangef)
#define glDetachShader                    gcmGLES2(glDetachShader)
#define glDisable                        gcmGLES2(glDisable)
#define glDisableVertexAttribArray        gcmGLES2(glDisableVertexAttribArray)
#define glDrawArrays                    gcmGLES2(glDrawArrays)
#define glDrawElements                    gcmGLES2(glDrawElements)
#define glEGLImageTargetRenderbufferStorageOES \
            gcmGLES2(glEGLImageTargetRenderbufferStorageOES)
#define glEGLImageTargetTexture2DOES \
            gcmGLES2(glEGLImageTargetTexture2DOES)
#define glEnable                        gcmGLES2(glEnable)
#define glEnableVertexAttribArray        gcmGLES2(glEnableVertexAttribArray)
#define glFinish                        gcmGLES2(glFinish)
#define glFlush                            gcmGLES2(glFlush)
#define glFramebufferRenderbuffer        gcmGLES2(glFramebufferRenderbuffer)
#define glFramebufferTexture2D            gcmGLES2(glFramebufferTexture2D)
#define glFramebufferTexture3DOES        gcmGLES2(glFramebufferTexture3DOES)
#define glFrontFace                        gcmGLES2(glFrontFace)
#define glGenBuffers                    gcmGLES2(glGenBuffers)
#define glGenFramebuffers                gcmGLES2(glGenFramebuffers)
#define glGenRenderbuffers                gcmGLES2(glGenRenderbuffers)
#define glGenTextures                    gcmGLES2(glGenTextures)
#define glGenerateMipmap                gcmGLES2(glGenerateMipmap)
#define glGenVertexArraysOES            gcmGLES2(glGenVertexArraysOES)
#define glGetActiveAttrib                gcmGLES2(glGetActiveAttrib)
#define glGetActiveUniform                gcmGLES2(glGetActiveUniform)
#define glGetAttachedShaders            gcmGLES2(glGetAttachedShaders)
#define glGetAttribLocation                gcmGLES2(glGetAttribLocation)
#define glGetBooleanv                    gcmGLES2(glGetBooleanv)
#define glGetBufferParameteriv            gcmGLES2(glGetBufferParameteriv)
#define glGetError                        gcmGLES2(glGetError)
#define glGetFloatv                        gcmGLES2(glGetFloatv)
#define glGetFramebufferAttachmentParameteriv \
            gcmGLES2(glGetFramebufferAttachmentParameteriv)
#define glGetIntegerv                    gcmGLES2(glGetIntegerv)
#define glGetProgramBinaryOES            gcmGLES2(glGetProgramBinaryOES)
#define glGetProgramInfoLog                gcmGLES2(glGetProgramInfoLog)
#define glGetProgramiv                    gcmGLES2(glGetProgramiv)
#define glGetRenderbufferParameteriv \
            gcmGLES2(glGetRenderbufferParameteriv)
#define glGetShaderInfoLog                gcmGLES2(glGetShaderInfoLog)
#define glGetShaderPrecisionFormat        gcmGLES2(glGetShaderPrecisionFormat)
#define glGetShaderSource                gcmGLES2(glGetShaderSource)
#define glGetShaderiv                    gcmGLES2(glGetShaderiv)
#define glGetString                        gcmGLES2(glGetString)
#define glGetTexParameterfv                gcmGLES2(glGetTexParameterfv)
#define glGetTexParameteriv                gcmGLES2(glGetTexParameteriv)
#define glGetUniformLocation            gcmGLES2(glGetUniformLocation)
#define glGetUniformfv                    gcmGLES2(glGetUniformfv)
#define glGetUniformiv                    gcmGLES2(glGetUniformiv)
#define glGetVertexAttribPointerv        gcmGLES2(glGetVertexAttribPointerv)
#define glGetVertexAttribfv                gcmGLES2(glGetVertexAttribfv)
#define glGetVertexAttribiv                gcmGLES2(glGetVertexAttribiv)
#define glHint                            gcmGLES2(glHint)
#define glIsBuffer                        gcmGLES2(glIsBuffer)
#define glIsEnabled                        gcmGLES2(glIsEnabled)
#define glIsFramebuffer                    gcmGLES2(glIsFramebuffer)
#define glIsProgram                        gcmGLES2(glIsProgram)
#define glIsRenderbuffer                gcmGLES2(glIsRenderbuffer)
#define glIsShader                        gcmGLES2(glIsShader)
#define glIsTexture                        gcmGLES2(glIsTexture)
#define glIsVertexArrayOES                gcmGLES2(glIsVertexArrayOES)
#define glLineWidth                        gcmGLES2(glLineWidth)
#define glLinkProgram                    gcmGLES2(glLinkProgram)
#define glPixelStorei                    gcmGLES2(glPixelStorei)
#define glPolygonOffset                    gcmGLES2(glPolygonOffset)
#define glProgramBinaryOES                gcmGLES2(glProgramBinaryOES)
#define glReadPixels                    gcmGLES2(glReadPixels)
#define glReleaseShaderCompiler            gcmGLES2(glReleaseShaderCompiler)
#define glRenderbufferStorage            gcmGLES2(glRenderbufferStorage)
#define glSampleCoverage                gcmGLES2(glSampleCoverage)
#define glScissor                        gcmGLES2(glScissor)
#define glShaderBinary                    gcmGLES2(glShaderBinary)
#define glShaderSource                    gcmGLES2(glShaderSource)
#define glStencilFunc                    gcmGLES2(glStencilFunc)
#define glStencilFuncSeparate            gcmGLES2(glStencilFuncSeparate)
#define glStencilMask                    gcmGLES2(glStencilMask)
#define glStencilMaskSeparate            gcmGLES2(glStencilMaskSeparate)
#define glStencilOp                        gcmGLES2(glStencilOp)
#define glStencilOpSeparate                gcmGLES2(glStencilOpSeparate)
#define glTexImage2D                    gcmGLES2(glTexImage2D)
#define glTexImage3DOES                    gcmGLES2(glTexImage3DOES)
#define glTexParameterf                    gcmGLES2(glTexParameterf)
#define glTexParameterfv                gcmGLES2(glTexParameterfv)
#define glTexParameteri                    gcmGLES2(glTexParameteri)
#define glTexParameteriv                gcmGLES2(glTexParameteriv)
#define glTexSubImage2D                    gcmGLES2(glTexSubImage2D)
#define glTexSubImage3DOES                gcmGLES2(glTexSubImage3DOES)
#define glUniform1f                        gcmGLES2(glUniform1f)
#define glUniform1fv                    gcmGLES2(glUniform1fv)
#define glUniform1i                        gcmGLES2(glUniform1i)
#define glUniform1iv                    gcmGLES2(glUniform1iv)
#define glUniform2f                        gcmGLES2(glUniform2f)
#define glUniform2fv                    gcmGLES2(glUniform2fv)
#define glUniform2i                        gcmGLES2(glUniform2i)
#define glUniform2iv                    gcmGLES2(glUniform2iv)
#define glUniform3f                        gcmGLES2(glUniform3f)
#define glUniform3fv                    gcmGLES2(glUniform3fv)
#define glUniform3i                        gcmGLES2(glUniform3i)
#define glUniform3iv                    gcmGLES2(glUniform3iv)
#define glUniform4f                        gcmGLES2(glUniform4f)
#define glUniform4fv                    gcmGLES2(glUniform4fv)
#define glUniform4i                        gcmGLES2(glUniform4i)
#define glUniform4iv                    gcmGLES2(glUniform4iv)
#define glUniformMatrix2fv                gcmGLES2(glUniformMatrix2fv)
#define glUniformMatrix3fv                gcmGLES2(glUniformMatrix3fv)
#define glUniformMatrix4fv                gcmGLES2(glUniformMatrix4fv)
#define glUseProgram                    gcmGLES2(glUseProgram)
#define glValidateProgram                gcmGLES2(glValidateProgram)
#define glVertexAttrib1f                gcmGLES2(glVertexAttrib1f)
#define glVertexAttrib1fv                gcmGLES2(glVertexAttrib1fv)
#define glVertexAttrib2f                gcmGLES2(glVertexAttrib2f)
#define glVertexAttrib2fv                gcmGLES2(glVertexAttrib2fv)
#define glVertexAttrib3f                gcmGLES2(glVertexAttrib3f)
#define glVertexAttrib3fv                gcmGLES2(glVertexAttrib3fv)
#define glVertexAttrib4f                gcmGLES2(glVertexAttrib4f)
#define glVertexAttrib4fv                gcmGLES2(glVertexAttrib4fv)
#define glVertexAttribPointer            gcmGLES2(glVertexAttribPointer)
#define glViewport                        gcmGLES2(glViewport)
#define glMultiDrawArraysEXT            gcmGLES2(glMultiDrawArraysEXT)
#define glMultiDrawElementsEXT          gcmGLES2(glMultiDrawElementsEXT)
#define glTexDirectVIVMap                gcmGLES2(glTexDirectVIVMap)
#define glTexDirectVIV                    gcmGLES2(glTexDirectVIV)
#define glTexDirectTiledMapVIV          gcmGLES2(glTexDirectTiledMapVIV)
#define glTexDirectInvalidateVIV        gcmGLES2(glTexDirectInvalidateVIV)
#define glMapBufferOES                    gcmGLES2(glMapBufferOES)
#define glUnmapBufferOES                gcmGLES2(glUnmapBufferOES)
#define glGetBufferPointervOES            gcmGLES2(glGetBufferPointervOES)

#endif /* _GL_2_APPENDIX */
#endif /* __gl2rename_h_ */
