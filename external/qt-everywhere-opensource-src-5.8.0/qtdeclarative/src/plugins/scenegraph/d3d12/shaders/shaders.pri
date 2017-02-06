vertexcolor_VSPS = $$PWD/vertexcolor.hlsl
vertexcolor_vshader.input = vertexcolor_VSPS
vertexcolor_vshader.header = vs_vertexcolor.hlslh
vertexcolor_vshader.entry = VS_VertexColor
vertexcolor_vshader.type = vs_5_0
vertexcolor_pshader.input = vertexcolor_VSPS
vertexcolor_pshader.header = ps_vertexcolor.hlslh
vertexcolor_pshader.entry = PS_VertexColor
vertexcolor_pshader.type = ps_5_0

flatcolor_VSPS = $$PWD/flatcolor.hlsl
flatcolor_vshader.input = flatcolor_VSPS
flatcolor_vshader.header = vs_flatcolor.hlslh
flatcolor_vshader.entry = VS_FlatColor
flatcolor_vshader.type = vs_5_0
flatcolor_pshader.input = flatcolor_VSPS
flatcolor_pshader.header = ps_flatcolor.hlslh
flatcolor_pshader.entry = PS_FlatColor
flatcolor_pshader.type = ps_5_0

stencilclip_VSPS = $$PWD/stencilclip.hlsl
stencilclip_vshader.input = stencilclip_VSPS
stencilclip_vshader.header = vs_stencilclip.hlslh
stencilclip_vshader.entry = VS_StencilClip
stencilclip_vshader.type = vs_5_0
stencilclip_pshader.input = stencilclip_VSPS
stencilclip_pshader.header = ps_stencilclip.hlslh
stencilclip_pshader.entry = PS_StencilClip
stencilclip_pshader.type = ps_5_0

smoothcolor_VSPS = $$PWD/smoothcolor.hlsl
smoothcolor_vshader.input = smoothcolor_VSPS
smoothcolor_vshader.header = vs_smoothcolor.hlslh
smoothcolor_vshader.entry = VS_SmoothColor
smoothcolor_vshader.type = vs_5_0
smoothcolor_pshader.input = smoothcolor_VSPS
smoothcolor_pshader.header = ps_smoothcolor.hlslh
smoothcolor_pshader.entry = PS_SmoothColor
smoothcolor_pshader.type = ps_5_0

texture_VSPS = $$PWD/texture.hlsl
texture_vshader.input = texture_VSPS
texture_vshader.header = vs_texture.hlslh
texture_vshader.entry = VS_Texture
texture_vshader.type = vs_5_0
texture_pshader.input = texture_VSPS
texture_pshader.header = ps_texture.hlslh
texture_pshader.entry = PS_Texture
texture_pshader.type = ps_5_0

smoothtexture_VSPS = $$PWD/smoothtexture.hlsl
smoothtexture_vshader.input = smoothtexture_VSPS
smoothtexture_vshader.header = vs_smoothtexture.hlslh
smoothtexture_vshader.entry = VS_SmoothTexture
smoothtexture_vshader.type = vs_5_0
smoothtexture_pshader.input = smoothtexture_VSPS
smoothtexture_pshader.header = ps_smoothtexture.hlslh
smoothtexture_pshader.entry = PS_SmoothTexture
smoothtexture_pshader.type = ps_5_0

mipmapgen_CS = $$PWD/mipmapgen.hlsl
mipmapgen_cshader.input = mipmapgen_CS
mipmapgen_cshader.header = cs_mipmapgen.hlslh
mipmapgen_cshader.entry = CS_Generate4MipMaps
mipmapgen_cshader.type = cs_5_0

textmask_VSPS = $$PWD/textmask.hlsl
textmask_vshader.input = textmask_VSPS
textmask_vshader.header = vs_textmask.hlslh
textmask_vshader.entry = VS_TextMask
textmask_vshader.type = vs_5_0
textmask_pshader24.input = textmask_VSPS
textmask_pshader24.header = ps_textmask24.hlslh
textmask_pshader24.entry = PS_TextMask24
textmask_pshader24.type = ps_5_0
textmask_pshader32.input = textmask_VSPS
textmask_pshader32.header = ps_textmask32.hlslh
textmask_pshader32.entry = PS_TextMask32
textmask_pshader32.type = ps_5_0
textmask_pshader8.input = textmask_VSPS
textmask_pshader8.header = ps_textmask8.hlslh
textmask_pshader8.entry = PS_TextMask8
textmask_pshader8.type = ps_5_0
styledtext_vshader.input = textmask_VSPS
styledtext_vshader.header = vs_styledtext.hlslh
styledtext_vshader.entry = VS_StyledText
styledtext_vshader.type = vs_5_0
styledtext_pshader.input = textmask_VSPS
styledtext_pshader.header = ps_styledtext.hlslh
styledtext_pshader.entry = PS_StyledText
styledtext_pshader.type = ps_5_0
outlinedtext_vshader.input = textmask_VSPS
outlinedtext_vshader.header = vs_outlinedtext.hlslh
outlinedtext_vshader.entry = VS_OutlinedText
outlinedtext_vshader.type = vs_5_0
outlinedtext_pshader.input = textmask_VSPS
outlinedtext_pshader.header = ps_outlinedtext.hlslh
outlinedtext_pshader.entry = PS_OutlinedText
outlinedtext_pshader.type = ps_5_0

shadereffectdefault_VSPS = $$PWD/shadereffectdefault.hlsl
shadereffectdefault_vshader.input = shadereffectdefault_VSPS
shadereffectdefault_vshader.header = vs_shadereffectdefault.hlslh
shadereffectdefault_vshader.entry = VS_DefaultShaderEffect
shadereffectdefault_vshader.type = vs_5_0
shadereffectdefault_pshader.input = shadereffectdefault_VSPS
shadereffectdefault_pshader.header = ps_shadereffectdefault.hlslh
shadereffectdefault_pshader.entry = PS_DefaultShaderEffect
shadereffectdefault_pshader.type = ps_5_0

sprite_VSPS = $$PWD/sprite.hlsl
sprite_vshader.input = sprite_VSPS
sprite_vshader.header = vs_sprite.hlslh
sprite_vshader.entry = VS_Sprite
sprite_vshader.type = vs_5_0
sprite_pshader.input = sprite_VSPS
sprite_pshader.header = ps_sprite.hlslh
sprite_pshader.entry = PS_Sprite
sprite_pshader.type = ps_5_0

tdr_CS = $$PWD/tdr.hlsl
tdr_cshader.input = tdr_CS
tdr_cshader.header = cs_tdr.hlslh
tdr_cshader.entry = timeout
tdr_cshader.type = cs_5_0

HLSL_SHADERS = \
    vertexcolor_vshader vertexcolor_pshader \
    flatcolor_vshader flatcolor_pshader \
    stencilclip_vshader stencilclip_pshader \
    smoothcolor_vshader smoothcolor_pshader \
    texture_vshader texture_pshader \
    smoothtexture_vshader smoothtexture_pshader \
    mipmapgen_cshader \
    textmask_vshader textmask_pshader24 textmask_pshader32 textmask_pshader8 \
    styledtext_vshader styledtext_pshader outlinedtext_vshader outlinedtext_pshader \
    shadereffectdefault_vshader shadereffectdefault_pshader \
    sprite_vshader sprite_pshader \
    tdr_cshader

load(hlsl_bytecode_header)
