#!/bin/bash

source ../../setenv_external.sh

# libpostproc, which is needed by Kodi, requires --enable-gpl 
export as_type=gcc

CONFIGURE_OPTIONS="--enable-gpl --disable-programs --disable-encoders --disable-decoders --disable-parsers --disable-muxers --disable-demuxers --disable-bsfs --disable-doc"

AVAILABLE_ENCODERS="adpcm_adx adpcm_g722 adpcm_g726 adpcm_ima_qt adpcm_ima_wav adpcm_ms adpcm_swf adpcm_yamaha ayuv bmp flac gif huffyuv jpeg2000 jpegls libmp3lame libopus libspeex libtheora libvorbis libvpx_vp8 libvpx_vp9 libwebp libwebp_anim ljpeg mjpeg pbm pcm_alaw pcm_f32be pcm_f32le pcm_f64be pcm_f64le pcm_mulaw pcm_s16be pcm_s16be_planar pcm_s16le pcm_s16le_planar pcm_s24be pcm_s24daud pcm_s24le pcm_s24le_planar pcm_s32be pcm_s32le pcm_s32le_planar pcm_s8 pcm_s8_planar pcm_u16be pcm_u16le pcm_u24be pcm_u24le pcm_u32be pcm_u32le pcm_u8 pcx pgm pgmyuv png ppm rawvideo targa tiff vorbis xbm xwd y41p zlib"
for item in $AVAILABLE_ENCODERS
do CONFIGURE_OPTIONS="$CONFIGURE_OPTIONS --enable-encoder=$item"
done

AVAILABLE_DECODERS="adpcm_4xm adpcm_adx adpcm_afc adpcm_ct adpcm_dtk adpcm_ea adpcm_ea_maxis_xa adpcm_ea_r1 adpcm_ea_r2 adpcm_ea_r3 adpcm_ea_xas adpcm_g722 adpcm_g726 adpcm_g726le adpcm_ima_amv adpcm_ima_apc adpcm_ima_dk3 adpcm_ima_dk4 adpcm_ima_ea_eacs adpcm_ima_ea_sead adpcm_ima_iss adpcm_ima_oki adpcm_ima_qt adpcm_ima_rad adpcm_ima_smjpeg adpcm_ima_wav adpcm_ima_ws adpcm_ms adpcm_sbpro_2 adpcm_sbpro_3 adpcm_sbpro_4 adpcm_swf adpcm_thp adpcm_thp_le adpcm_vima adpcm_xa adpcm_yamaha bmp flac gif jpeg2000 libcelt libopenjpeg libopus libspeex libvorbis libvpx_vp8 libvpx_vp9 mjpeg mjpegb mp3 mp3adu mp3adufloat mp3float opus pbm pcm_alaw pcm_bluray pcm_dvd pcm_f32be pcm_f32le pcm_f64be pcm_f64le pcm_lxf pcm_mulaw pcm_s16be pcm_s16be_planar pcm_s16le pcm_s16le_planar pcm_s24be pcm_s24daud pcm_s24le pcm_s24le_planar pcm_s32be pcm_s32le pcm_s32le_planar pcm_s8 pcm_s8_planar pcm_u16be pcm_u16le pcm_u24be pcm_u24le pcm_u32be pcm_u32le pcm_u8 pcm_zork pcx pgm pgmyuv png ppm rawvideo targa theora tiff vorbis vp3 vp5 vp6 vp6a vp6f vp7 vp8 vp9 vplayer webp xbm xwd zlib"
for item in $AVAILABLE_DECODERS
do CONFIGURE_OPTIONS="$CONFIGURE_OPTIONS --enable-decoder=$item"
done

AVAILABLE_PARSERS="bmp flac h264 mjpeg opus png pnm vorbis vp3 vp8 vp9"
for item in $AVAILABLE_PARSERS
do CONFIGURE_OPTIONS="$CONFIGURE_OPTIONS --enable-parser=$item"
done

AVAILABLE_MUXERS="aiff flac gif h264 ico matroska matroska_audio md5 mjpeg mov mp3 null ogg opus pcm_alaw pcm_f32be pcm_f32le pcm_f64be pcm_f64le pcm_mulaw pcm_s16be pcm_s16le pcm_s24be pcm_s24le pcm_s32be pcm_s32le pcm_s8 pcm_u16be pcm_u16le pcm_u24be pcm_u24le pcm_u32be pcm_u32le pcm_u8 rawvideo rtp rtsp singlejpeg smjpeg wav webm webm_chunk webm_dash_manifest webp"
for item in $AVAILABLE_MUXERS
do CONFIGURE_OPTIONS="$CONFIGURE_OPTIONS --enable-muxer=$item"
done

AVAILABLE_DEMUXERS="aiff flac flic gif h264 matroska mjpeg mov mp3 ogg pcm_alaw pcm_f32be pcm_f32le pcm_f64be pcm_f64le pcm_mulaw pcm_s16be pcm_s16le pcm_s24be pcm_s24le pcm_s32be pcm_s32le pcm_s8 pcm_u16be pcm_u16le pcm_u24be pcm_u24le pcm_u32be pcm_u32le pcm_u8 rawvideo rtp rtsp smjpeg wav webm_dash_manifest"
for item in $AVAILABLE_DEMUXERS
do CONFIGURE_OPTIONS="$CONFIGURE_OPTIONS --enable-demuxer=$item"
done

AVAILABLE_BITSTREAMFILTERS="chomp dump_extradata h264_mp4toannexb mjpeg2jpeg mjpega_dump_header noise remove_extradata"
for item in $AVAILABLE_BITSTREAMFILTERS
do CONFIGURE_OPTIONS="$CONFIGURE_OPTIONS --enable-bsf=$item"
done

./configure --enable-cross-compile --arch=arm --target-os=linux --cc="$CC" --cxx="$CXX" --dep-cc="$CC" --ld="$CC" --as="$CC" --prefix=/usr $CONFIGURE_OPTIONS || exit 1

valve_make_clean
valve_make
valve_make_install
