/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCanvas3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "enumtostringmap_p.h"

QT_BEGIN_NAMESPACE
QT_CANVAS3D_BEGIN_NAMESPACE

static EnumToStringMap *theInstance = 0;
static unsigned int theInstanceCount = 0;

EnumToStringMap *EnumToStringMap::newInstance()
{
    if (theInstance) {
        theInstanceCount++;
        return theInstance;
    }

    theInstance = new EnumToStringMap();
    theInstanceCount++;
    return theInstance;
}

void EnumToStringMap::deleteInstance()
{
    theInstanceCount--;
    if (theInstanceCount <= 0) {
        delete theInstance;
        theInstance = 0;
    }
}

EnumToStringMap::EnumToStringMap() :
    m_unknown("<unknown>")
{

    m_map[CanvasContext::DEPTH_BUFFER_BIT] = "DEPTH_BUFFER_BIT";
    m_map[CanvasContext::STENCIL_BUFFER_BIT] = "STENCIL_BUFFER_BIT";
    m_map[CanvasContext::COLOR_BUFFER_BIT] = "COLOR_BUFFER_BIT";

    m_map[(CanvasContext::DEPTH_AND_COLOR_BUFFER_BIT)] = "DEPTH_BUFFER_BIT | COLOR_BUFFER_BIT";
    m_map[(CanvasContext::DEPTH_AND_STENCIL_AND_COLOR_BUFFER_BIT)] = "DEPTH_BUFFER_BIT | STENCIL_BUFFER_BIT | COLOR_BUFFER_BIT";
    m_map[CanvasContext::STENCIL_BUFFER_BIT] = "STENCIL_BUFFER_BIT";
    m_map[CanvasContext::COLOR_BUFFER_BIT] = "COLOR_BUFFER_BIT";

    m_map[CanvasContext::LINE_LOOP] = "LINE_LOOP";
    m_map[CanvasContext::LINE_STRIP] = "LINE_STRIP";
    m_map[CanvasContext::TRIANGLES] = "TRIANGLES";
    m_map[CanvasContext::TRIANGLE_STRIP] = "TRIANGLE_STRIP";
    m_map[CanvasContext::TRIANGLE_FAN] = "TRIANGLE_FAN";

    m_map[CanvasContext::SRC_COLOR] = "SRC_COLOR";
    m_map[CanvasContext::ONE_MINUS_SRC_COLOR] = "ONE_MINUS_SRC_COLOR";
    m_map[CanvasContext::SRC_ALPHA] = "SRC_ALPHA";
    m_map[CanvasContext::ONE_MINUS_SRC_ALPHA] = "ONE_MINUS_SRC_ALPHA";
    m_map[CanvasContext::DST_ALPHA] = "DST_ALPHA";
    m_map[CanvasContext::ONE_MINUS_DST_ALPHA] = "ONE_MINUS_DST_ALPHA";

    m_map[CanvasContext::DST_COLOR] = "DST_COLOR";
    m_map[CanvasContext::ONE_MINUS_DST_COLOR] = "ONE_MINUS_DST_COLOR";
    m_map[CanvasContext::SRC_ALPHA_SATURATE] = "SRC_ALPHA_SATURATE";

    m_map[CanvasContext::FUNC_ADD] = "FUNC_ADD";
    m_map[CanvasContext::BLEND_EQUATION] = "BLEND_EQUATION";
    m_map[CanvasContext::BLEND_EQUATION_RGB] = "BLEND_EQUATION_RGB";
    m_map[CanvasContext::BLEND_EQUATION_ALPHA] = "BLEND_EQUATION_ALPHA";

    m_map[CanvasContext::FUNC_SUBTRACT] = "FUNC_SUBTRACT";
    m_map[CanvasContext::FUNC_REVERSE_SUBTRACT] = "FUNC_REVERSE_SUBTRACT";

    m_map[CanvasContext::BLEND_DST_RGB] = "BLEND_DST_RGB";
    m_map[CanvasContext::BLEND_SRC_RGB] = "BLEND_SRC_RGB";
    m_map[CanvasContext::BLEND_DST_ALPHA] = "BLEND_DST_ALPHA";
    m_map[CanvasContext::BLEND_SRC_ALPHA] = "BLEND_SRC_ALPHA";
    m_map[CanvasContext::CONSTANT_COLOR] = "CONSTANT_COLOR";
    m_map[CanvasContext::ONE_MINUS_CONSTANT_COLOR] = "ONE_MINUS_CONSTANT_COLOR";
    m_map[CanvasContext::CONSTANT_ALPHA] = "CONSTANT_ALPHA";
    m_map[CanvasContext::ONE_MINUS_CONSTANT_ALPHA] = "ONE_MINUS_CONSTANT_ALPHA";
    m_map[CanvasContext::BLEND_COLOR] = "BLEND_COLOR";

    m_map[CanvasContext::ARRAY_BUFFER] = "ARRAY_BUFFER";
    m_map[CanvasContext::ELEMENT_ARRAY_BUFFER] = "ELEMENT_ARRAY_BUFFER";
    m_map[CanvasContext::ARRAY_BUFFER_BINDING] = "ARRAY_BUFFER_BINDING";
    m_map[CanvasContext::ELEMENT_ARRAY_BUFFER_BINDING] = "ELEMENT_ARRAY_BUFFER_BINDING";

    m_map[CanvasContext::STREAM_DRAW] = "STREAM_DRAW";
    m_map[CanvasContext::STATIC_DRAW] = "STATIC_DRAW";
    m_map[CanvasContext::DYNAMIC_DRAW] = "DYNAMIC_DRAW";

    m_map[CanvasContext::BUFFER_SIZE] = "BUFFER_SIZE";
    m_map[CanvasContext::BUFFER_USAGE] = "BUFFER_USAGE";

    m_map[CanvasContext::CURRENT_VERTEX_ATTRIB] = "CURRENT_VERTEX_ATTRIB";

    m_map[CanvasContext::FRONT] = "FRONT";
    m_map[CanvasContext::BACK] = "BACK";
    m_map[CanvasContext::FRONT_AND_BACK] = "FRONT_AND_BACK";

    m_map[CanvasContext::CULL_FACE] = "CULL_FACE";
    m_map[CanvasContext::BLEND] = "BLEND";
    m_map[CanvasContext::DITHER] = "DITHER";
    m_map[CanvasContext::STENCIL_TEST] = "STENCIL_TEST";
    m_map[CanvasContext::DEPTH_TEST] = "DEPTH_TEST";
    m_map[CanvasContext::SCISSOR_TEST] = "SCISSOR_TEST";
    m_map[CanvasContext::POLYGON_OFFSET_FILL] = "POLYGON_OFFSET_FILL";
    m_map[CanvasContext::SAMPLE_ALPHA_TO_COVERAGE] = "SAMPLE_ALPHA_TO_COVERAGE";
    m_map[CanvasContext::SAMPLE_COVERAGE] = "SAMPLE_COVERAGE";

    m_map[CanvasContext::INVALID_ENUM] = "INVALID_ENUM";
    m_map[CanvasContext::INVALID_VALUE] = "INVALID_VALUE";
    m_map[CanvasContext::INVALID_OPERATION] = "INVALID_OPERATION";
    m_map[CanvasContext::OUT_OF_MEMORY] = "OUT_OF_MEMORY";

    m_map[CanvasContext::CW] = "CW";
    m_map[CanvasContext::CCW] = "CCW";

    m_map[CanvasContext::LINE_WIDTH] = "LINE_WIDTH";
    m_map[CanvasContext::ALIASED_POINT_SIZE_RANGE] = "ALIASED_POINT_SIZE_RANGE";
    m_map[CanvasContext::ALIASED_LINE_WIDTH_RANGE] = "ALIASED_LINE_WIDTH_RANGE";
    m_map[CanvasContext::CULL_FACE_MODE] = "CULL_FACE_MODE";
    m_map[CanvasContext::FRONT_FACE] = "FRONT_FACE";
    m_map[CanvasContext::DEPTH_RANGE] = "DEPTH_RANGE";
    m_map[CanvasContext::DEPTH_WRITEMASK] = "DEPTH_WRITEMASK";
    m_map[CanvasContext::DEPTH_CLEAR_VALUE] = "DEPTH_CLEAR_VALUE";
    m_map[CanvasContext::DEPTH_FUNC] = "DEPTH_FUNC";
    m_map[CanvasContext::STENCIL_CLEAR_VALUE] = "STENCIL_CLEAR_VALUE";
    m_map[CanvasContext::STENCIL_FUNC] = "STENCIL_FUNC";
    m_map[CanvasContext::STENCIL_FAIL] = "STENCIL_FAIL";
    m_map[CanvasContext::STENCIL_PASS_DEPTH_FAIL] = "STENCIL_PASS_DEPTH_FAIL";
    m_map[CanvasContext::STENCIL_PASS_DEPTH_PASS] = "STENCIL_PASS_DEPTH_PASS";
    m_map[CanvasContext::STENCIL_REF] = "STENCIL_REF";
    m_map[CanvasContext::STENCIL_VALUE_MASK] = "STENCIL_VALUE_MASK";
    m_map[CanvasContext::STENCIL_WRITEMASK] = "STENCIL_WRITEMASK";
    m_map[CanvasContext::STENCIL_BACK_FUNC] = "STENCIL_BACK_FUNC";
    m_map[CanvasContext::STENCIL_BACK_FAIL] = "STENCIL_BACK_FAIL";
    m_map[CanvasContext::STENCIL_BACK_PASS_DEPTH_FAIL] = "STENCIL_BACK_PASS_DEPTH_FAIL";
    m_map[CanvasContext::STENCIL_BACK_PASS_DEPTH_PASS] = "STENCIL_BACK_PASS_DEPTH_PASS";
    m_map[CanvasContext::STENCIL_BACK_REF] = "STENCIL_BACK_REF";
    m_map[CanvasContext::STENCIL_BACK_VALUE_MASK] = "STENCIL_BACK_VALUE_MASK";
    m_map[CanvasContext::STENCIL_BACK_WRITEMASK] = "STENCIL_BACK_WRITEMASK";
    m_map[CanvasContext::VIEWPORT] = "VIEWPORT";
    m_map[CanvasContext::SCISSOR_BOX] = "SCISSOR_BOX";

    m_map[CanvasContext::COLOR_CLEAR_VALUE] = "COLOR_CLEAR_VALUE";
    m_map[CanvasContext::COLOR_WRITEMASK] = "COLOR_WRITEMASK";
    m_map[CanvasContext::UNPACK_ALIGNMENT] = "UNPACK_ALIGNMENT";
    m_map[CanvasContext::PACK_ALIGNMENT] = "PACK_ALIGNMENT";
    m_map[CanvasContext::MAX_TEXTURE_SIZE] = "MAX_TEXTURE_SIZE";
    m_map[CanvasContext::MAX_VIEWPORT_DIMS] = "MAX_VIEWPORT_DIMS";
    m_map[CanvasContext::SUBPIXEL_BITS] = "SUBPIXEL_BITS";
    m_map[CanvasContext::RED_BITS] = "RED_BITS";
    m_map[CanvasContext::GREEN_BITS] = "GREEN_BITS";
    m_map[CanvasContext::BLUE_BITS] = "BLUE_BITS";
    m_map[CanvasContext::ALPHA_BITS] = "ALPHA_BITS";
    m_map[CanvasContext::DEPTH_BITS] = "DEPTH_BITS";
    m_map[CanvasContext::STENCIL_BITS] = "STENCIL_BITS";
    m_map[CanvasContext::POLYGON_OFFSET_UNITS] = "POLYGON_OFFSET_UNITS";

    m_map[CanvasContext::POLYGON_OFFSET_FACTOR] = "POLYGON_OFFSET_FACTOR";
    m_map[CanvasContext::TEXTURE_BINDING_2D] = "TEXTURE_BINDING_2D";
    m_map[CanvasContext::SAMPLE_BUFFERS] = "SAMPLE_BUFFERS";
    m_map[CanvasContext::SAMPLES] = "SAMPLES";
    m_map[CanvasContext::SAMPLE_COVERAGE_VALUE] = "SAMPLE_COVERAGE_VALUE";
    m_map[CanvasContext::SAMPLE_COVERAGE_INVERT] = "SAMPLE_COVERAGE_INVERT";

    m_map[CanvasContext::COMPRESSED_TEXTURE_FORMATS] = "COMPRESSED_TEXTURE_FORMATS";

    m_map[CanvasContext::DONT_CARE] = "DONT_CARE";
    m_map[CanvasContext::FASTEST] = "FASTEST";
    m_map[CanvasContext::NICEST] = "NICEST";

    m_map[CanvasContext::GENERATE_MIPMAP_HINT] = "GENERATE_MIPMAP_HINT";

    m_map[CanvasContext::BYTE] = "BYTE";
    m_map[CanvasContext::UNSIGNED_BYTE] = "UNSIGNED_BYTE";
    m_map[CanvasContext::SHORT] = "SHORT";
    m_map[CanvasContext::UNSIGNED_SHORT] = "UNSIGNED_SHORT";
    m_map[CanvasContext::INT] = "INT";
    m_map[CanvasContext::UNSIGNED_INT] = "UNSIGNED_INT";
    m_map[CanvasContext::FLOAT] = "FLOAT";

    m_map[CanvasContext::DEPTH_COMPONENT] = "DEPTH_COMPONENT";
    m_map[CanvasContext::ALPHA] = "ALPHA";
    m_map[CanvasContext::RGB] = "RGB";
    m_map[CanvasContext::RGBA] = "RGBA";
    m_map[CanvasContext::LUMINANCE] = "LUMINANCE";
    m_map[CanvasContext::LUMINANCE_ALPHA] = "LUMINANCE_ALPHA";

    m_map[CanvasContext::UNSIGNED_SHORT_4_4_4_4] = "UNSIGNED_SHORT_4_4_4_4";
    m_map[CanvasContext::UNSIGNED_SHORT_5_5_5_1] = "UNSIGNED_SHORT_5_5_5_1";
    m_map[CanvasContext::UNSIGNED_SHORT_5_6_5] = "UNSIGNED_SHORT_5_6_5";

    m_map[CanvasContext::FRAGMENT_SHADER] = "FRAGMENT_SHADER";
    m_map[CanvasContext::VERTEX_SHADER] = "VERTEX_SHADER";
    m_map[CanvasContext::MAX_VERTEX_ATTRIBS] = "MAX_VERTEX_ATTRIBS";
    m_map[CanvasContext::MAX_VERTEX_UNIFORM_VECTORS] = "MAX_VERTEX_UNIFORM_VECTORS";
    m_map[CanvasContext::MAX_VARYING_VECTORS] = "MAX_VARYING_VECTORS";
    m_map[CanvasContext::MAX_COMBINED_TEXTURE_IMAGE_UNITS] = "MAX_COMBINED_TEXTURE_IMAGE_UNITS";
    m_map[CanvasContext::MAX_VERTEX_TEXTURE_IMAGE_UNITS] = "MAX_VERTEX_TEXTURE_IMAGE_UNITS";
    m_map[CanvasContext::MAX_TEXTURE_IMAGE_UNITS] = "MAX_TEXTURE_IMAGE_UNITS";
    m_map[CanvasContext::MAX_FRAGMENT_UNIFORM_VECTORS] = "MAX_FRAGMENT_UNIFORM_VECTORS";
    m_map[CanvasContext::SHADER_TYPE] = "SHADER_TYPE";
    m_map[CanvasContext::DELETE_STATUS] = "DELETE_STATUS";
    m_map[CanvasContext::LINK_STATUS] = "LINK_STATUS";
    m_map[CanvasContext::VALIDATE_STATUS] = "VALIDATE_STATUS";
    m_map[CanvasContext::ATTACHED_SHADERS] = "ATTACHED_SHADERS";
    m_map[CanvasContext::ACTIVE_UNIFORMS] = "ACTIVE_UNIFORMS";
    m_map[CanvasContext::ACTIVE_ATTRIBUTES] = "ACTIVE_ATTRIBUTES";
    m_map[CanvasContext::SHADING_LANGUAGE_VERSION] = "SHADING_LANGUAGE_VERSION";
    m_map[CanvasContext::CURRENT_PROGRAM] = "CURRENT_PROGRAM";

    m_map[CanvasContext::NEVER] = "NEVER";
    m_map[CanvasContext::LESS] = "LESS";
    m_map[CanvasContext::EQUAL] = "EQUAL";
    m_map[CanvasContext::LEQUAL] = "LEQUAL";
    m_map[CanvasContext::GREATER] = "GREATER";
    m_map[CanvasContext::NOTEQUAL] = "NOTEQUAL";
    m_map[CanvasContext::GEQUAL] = "GEQUAL";
    m_map[CanvasContext::ALWAYS] = "ALWAYS";

    m_map[CanvasContext::KEEP] = "KEEP";
    m_map[CanvasContext::REPLACE] = "REPLACE";
    m_map[CanvasContext::INCR] = "INCR";
    m_map[CanvasContext::DECR] = "DECR";
    m_map[CanvasContext::INVERT] = "INVERT";
    m_map[CanvasContext::INCR_WRAP] = "INCR_WRAP";
    m_map[CanvasContext::DECR_WRAP] = "DECR_WRAP";

    m_map[CanvasContext::VENDOR] = "VENDOR";
    m_map[CanvasContext::RENDERER] = "RENDERER";
    m_map[CanvasContext::VERSION] = "VERSION";

    m_map[CanvasContext::NEAREST] = "NEAREST";
    m_map[CanvasContext::LINEAR] = "LINEAR";

    m_map[CanvasContext::NEAREST_MIPMAP_NEAREST] = "NEAREST_MIPMAP_NEAREST";
    m_map[CanvasContext::LINEAR_MIPMAP_NEAREST] = "LINEAR_MIPMAP_NEAREST";
    m_map[CanvasContext::NEAREST_MIPMAP_LINEAR] = "NEAREST_MIPMAP_LINEAR";
    m_map[CanvasContext::LINEAR_MIPMAP_LINEAR] = "LINEAR_MIPMAP_LINEAR";

    m_map[CanvasContext::TEXTURE_MAG_FILTER] = "TEXTURE_MAG_FILTER";
    m_map[CanvasContext::TEXTURE_MIN_FILTER] = "TEXTURE_MIN_FILTER";
    m_map[CanvasContext::TEXTURE_WRAP_S] = "TEXTURE_WRAP_S";
    m_map[CanvasContext::TEXTURE_WRAP_T] = "TEXTURE_WRAP_T";

    m_map[CanvasContext::TEXTURE_2D] = "TEXTURE_2D";
    m_map[CanvasContext::TEXTURE] = "TEXTURE";

    m_map[CanvasContext::TEXTURE_CUBE_MAP] = "TEXTURE_CUBE_MAP";
    m_map[CanvasContext::TEXTURE_BINDING_CUBE_MAP] = "TEXTURE_BINDING_CUBE_MAP";
    m_map[CanvasContext::TEXTURE_CUBE_MAP_POSITIVE_X] = "TEXTURE_CUBE_MAP_POSITIVE_X";
    m_map[CanvasContext::TEXTURE_CUBE_MAP_NEGATIVE_X] = "TEXTURE_CUBE_MAP_NEGATIVE_X";
    m_map[CanvasContext::TEXTURE_CUBE_MAP_POSITIVE_Y] = "TEXTURE_CUBE_MAP_POSITIVE_Y";
    m_map[CanvasContext::TEXTURE_CUBE_MAP_NEGATIVE_Y] = "TEXTURE_CUBE_MAP_NEGATIVE_Y";
    m_map[CanvasContext::TEXTURE_CUBE_MAP_POSITIVE_Z] = "TEXTURE_CUBE_MAP_POSITIVE_Z";
    m_map[CanvasContext::TEXTURE_CUBE_MAP_NEGATIVE_Z] = "TEXTURE_CUBE_MAP_NEGATIVE_Z";
    m_map[CanvasContext::MAX_CUBE_MAP_TEXTURE_SIZE] = "MAX_CUBE_MAP_TEXTURE_SIZE";

    m_map[CanvasContext::TEXTURE0] = "TEXTURE0";
    m_map[CanvasContext::TEXTURE1] = "TEXTURE1";
    m_map[CanvasContext::TEXTURE2] = "TEXTURE2";
    m_map[CanvasContext::TEXTURE3] = "TEXTURE3";
    m_map[CanvasContext::TEXTURE4] = "TEXTURE4";
    m_map[CanvasContext::TEXTURE5] = "TEXTURE5";
    m_map[CanvasContext::TEXTURE6] = "TEXTURE6";
    m_map[CanvasContext::TEXTURE7] = "TEXTURE7";
    m_map[CanvasContext::TEXTURE8] = "TEXTURE8";
    m_map[CanvasContext::TEXTURE9] = "TEXTURE9";
    m_map[CanvasContext::TEXTURE10] = "TEXTURE10";
    m_map[CanvasContext::TEXTURE11] = "TEXTURE11";
    m_map[CanvasContext::TEXTURE12] = "TEXTURE12";
    m_map[CanvasContext::TEXTURE13] = "TEXTURE13";
    m_map[CanvasContext::TEXTURE14] = "TEXTURE14";
    m_map[CanvasContext::TEXTURE15] = "TEXTURE15";
    m_map[CanvasContext::TEXTURE16] = "TEXTURE16";
    m_map[CanvasContext::TEXTURE17] = "TEXTURE17";
    m_map[CanvasContext::TEXTURE18] = "TEXTURE18";
    m_map[CanvasContext::TEXTURE19] = "TEXTURE19";
    m_map[CanvasContext::TEXTURE20] = "TEXTURE20";
    m_map[CanvasContext::TEXTURE21] = "TEXTURE21";
    m_map[CanvasContext::TEXTURE22] = "TEXTURE22";
    m_map[CanvasContext::TEXTURE23] = "TEXTURE23";
    m_map[CanvasContext::TEXTURE24] = "TEXTURE24";
    m_map[CanvasContext::TEXTURE25] = "TEXTURE25";
    m_map[CanvasContext::TEXTURE26] = "TEXTURE26";
    m_map[CanvasContext::TEXTURE27] = "TEXTURE27";
    m_map[CanvasContext::TEXTURE28] = "TEXTURE28";
    m_map[CanvasContext::TEXTURE29] = "TEXTURE29";
    m_map[CanvasContext::TEXTURE30] = "TEXTURE30";
    m_map[CanvasContext::TEXTURE31] = "TEXTURE31";
    m_map[CanvasContext::ACTIVE_TEXTURE] = "ACTIVE_TEXTURE";

    m_map[CanvasContext::REPEAT] = "REPEAT";
    m_map[CanvasContext::CLAMP_TO_EDGE] = "CLAMP_TO_EDGE";
    m_map[CanvasContext::MIRRORED_REPEAT] = "MIRRORED_REPEAT";

    m_map[CanvasContext::FLOAT_VEC2] = "FLOAT_VEC2";
    m_map[CanvasContext::FLOAT_VEC3] = "FLOAT_VEC3";
    m_map[CanvasContext::FLOAT_VEC4] = "FLOAT_VEC4";
    m_map[CanvasContext::INT_VEC2] = "INT_VEC2";
    m_map[CanvasContext::INT_VEC3] = "INT_VEC3";
    m_map[CanvasContext::INT_VEC4] = "INT_VEC4";
    m_map[CanvasContext::BOOL] = "BOOL";
    m_map[CanvasContext::BOOL_VEC2] = "BOOL_VEC2";
    m_map[CanvasContext::BOOL_VEC3] = "BOOL_VEC3";
    m_map[CanvasContext::BOOL_VEC4] = "BOOL_VEC4";
    m_map[CanvasContext::FLOAT_MAT2] = "FLOAT_MAT2";
    m_map[CanvasContext::FLOAT_MAT3] = "FLOAT_MAT3";
    m_map[CanvasContext::FLOAT_MAT4] = "FLOAT_MAT4";
    m_map[CanvasContext::SAMPLER_2D] = "SAMPLER_2D";
    m_map[CanvasContext::SAMPLER_CUBE] = "SAMPLER_CUBE";

    m_map[CanvasContext::VERTEX_ATTRIB_ARRAY_ENABLED] = "VERTEX_ATTRIB_ARRAY_ENABLED";
    m_map[CanvasContext::VERTEX_ATTRIB_ARRAY_SIZE] = "VERTEX_ATTRIB_ARRAY_SIZE";
    m_map[CanvasContext::VERTEX_ATTRIB_ARRAY_STRIDE] = "VERTEX_ATTRIB_ARRAY_STRIDE";
    m_map[CanvasContext::VERTEX_ATTRIB_ARRAY_TYPE] = "VERTEX_ATTRIB_ARRAY_TYPE";
    m_map[CanvasContext::VERTEX_ATTRIB_ARRAY_NORMALIZED] = "VERTEX_ATTRIB_ARRAY_NORMALIZED";
    m_map[CanvasContext::VERTEX_ATTRIB_ARRAY_POINTER] = "VERTEX_ATTRIB_ARRAY_POINTER";
    m_map[CanvasContext::VERTEX_ATTRIB_ARRAY_BUFFER_BINDING] = "VERTEX_ATTRIB_ARRAY_BUFFER_BINDING";

    m_map[CanvasContext::COMPILE_STATUS] = "COMPILE_STATUS";

    m_map[CanvasContext::LOW_FLOAT] = "LOW_FLOAT";
    m_map[CanvasContext::MEDIUM_FLOAT] = "MEDIUM_FLOAT";
    m_map[CanvasContext::HIGH_FLOAT] = "HIGH_FLOAT";
    m_map[CanvasContext::LOW_INT] = "LOW_INT";
    m_map[CanvasContext::MEDIUM_INT] = "MEDIUM_INT";
    m_map[CanvasContext::HIGH_INT] = "HIGH_INT";

    m_map[CanvasContext::FRAMEBUFFER] = "FRAMEBUFFER";
    m_map[CanvasContext::RENDERBUFFER] = "RENDERBUFFER";

    m_map[CanvasContext::RGBA4] = "RGBA4";
    m_map[CanvasContext::RGB5_A1] = "RGB5_A1";
    m_map[CanvasContext::RGB565] = "RGB565";
    m_map[CanvasContext::DEPTH_COMPONENT16] = "DEPTH_COMPONENT16";
    m_map[CanvasContext::STENCIL_INDEX] = "STENCIL_INDEX";
    m_map[CanvasContext::STENCIL_INDEX8] = "STENCIL_INDEX8";
    m_map[CanvasContext::DEPTH_STENCIL] = "DEPTH_STENCIL";

    m_map[CanvasContext::RENDERBUFFER_WIDTH] = "RENDERBUFFER_WIDTH";
    m_map[CanvasContext::RENDERBUFFER_HEIGHT] = "RENDERBUFFER_HEIGHT";
    m_map[CanvasContext::RENDERBUFFER_INTERNAL_FORMAT] = "RENDERBUFFER_INTERNAL_FORMAT";
    m_map[CanvasContext::RENDERBUFFER_RED_SIZE] = "RENDERBUFFER_RED_SIZE";
    m_map[CanvasContext::RENDERBUFFER_GREEN_SIZE] = "RENDERBUFFER_GREEN_SIZE";
    m_map[CanvasContext::RENDERBUFFER_BLUE_SIZE] = "RENDERBUFFER_BLUE_SIZE";
    m_map[CanvasContext::RENDERBUFFER_ALPHA_SIZE] = "RENDERBUFFER_ALPHA_SIZE";
    m_map[CanvasContext::RENDERBUFFER_DEPTH_SIZE] = "RENDERBUFFER_DEPTH_SIZE";
    m_map[CanvasContext::RENDERBUFFER_STENCIL_SIZE] = "RENDERBUFFER_STENCIL_SIZE";

    m_map[CanvasContext::FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE] = "FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE";
    m_map[CanvasContext::FRAMEBUFFER_ATTACHMENT_OBJECT_NAME] = "FRAMEBUFFER_ATTACHMENT_OBJECT_NAME";
    m_map[CanvasContext::FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL] = "FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL";
    m_map[CanvasContext::FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE] = "FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE";

    m_map[CanvasContext::COLOR_ATTACHMENT0] = "COLOR_ATTACHMENT0";
    m_map[CanvasContext::DEPTH_ATTACHMENT] = "DEPTH_ATTACHMENT";
    m_map[CanvasContext::STENCIL_ATTACHMENT] = "STENCIL_ATTACHMENT";
    m_map[CanvasContext::DEPTH_STENCIL_ATTACHMENT] = "DEPTH_STENCIL_ATTACHMENT";

    m_map[CanvasContext::FRAMEBUFFER_COMPLETE] = "FRAMEBUFFER_COMPLETE";
    m_map[CanvasContext::FRAMEBUFFER_INCOMPLETE_ATTACHMENT] = "FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
    m_map[CanvasContext::FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT] = "FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
    m_map[CanvasContext::FRAMEBUFFER_INCOMPLETE_DIMENSIONS] = "FRAMEBUFFER_INCOMPLETE_DIMENSIONS";
    m_map[CanvasContext::FRAMEBUFFER_UNSUPPORTED] = "FRAMEBUFFER_UNSUPPORTED";

    m_map[CanvasContext::FRAMEBUFFER_BINDING] = "FRAMEBUFFER_BINDING";
    m_map[CanvasContext::RENDERBUFFER_BINDING] = "RENDERBUFFER_BINDING";
    m_map[CanvasContext::MAX_RENDERBUFFER_SIZE] = "MAX_RENDERBUFFER_SIZE";

    m_map[CanvasContext::INVALID_FRAMEBUFFER_OPERATION] = "INVALID_FRAMEBUFFER_OPERATION";

    m_map[CanvasContext::UNPACK_FLIP_Y_WEBGL] = "UNPACK_FLIP_Y_WEBGL";
    m_map[CanvasContext::UNPACK_PREMULTIPLY_ALPHA_WEBGL] = "UNPACK_PREMULTIPLY_ALPHA_WEBGL";
    m_map[CanvasContext::CONTEXT_LOST_WEBGL] = "CONTEXT_LOST_WEBGL";
    m_map[CanvasContext::UNPACK_COLORSPACE_CONVERSION_WEBGL] = "UNPACK_COLORSPACE_CONVERSION_WEBGL";
    m_map[CanvasContext::BROWSER_DEFAULT_WEBGL] = "BROWSER_DEFAULT_WEBGL";

    // cases where single value can map to multiple strings
    m_map[CanvasContext::ZERO] = "ZERO/NO_ERROR/POINTS";
    m_map[CanvasContext::ONE] = "ONE/LINES";
}

QString EnumToStringMap::lookUp(const CanvasContext::glEnums value) const
{
    if (m_map.contains(value))
        return m_map[value];

    return QString("0x0%1").arg(value, 0, 16);
}

QString EnumToStringMap::lookUp(const GLuint value) const
{
    return lookUp((CanvasContext::glEnums)value);
}

QT_CANVAS3D_END_NAMESPACE
QT_END_NAMESPACE
