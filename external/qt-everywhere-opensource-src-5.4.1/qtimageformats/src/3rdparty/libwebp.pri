CONFIG += object_parallel_to_source no_batch

INCLUDEPATH += \
    $$PWD/libwebp \
    $$PWD/libwebp/src \
    $$PWD/libwebp/src/dec \
    $$PWD/libwebp/src/enc \
    $$PWD/libwebp/src/dsp \
    $$PWD/libwebp/src/mux \
    $$PWD/libwebp/src/utils \
    $$PWD/libwebp/src/webp

SOURCES += \
    $$PWD/libwebp/src/dec/alpha.c \
    $$PWD/libwebp/src/dec/buffer.c \
    $$PWD/libwebp/src/dec/frame.c \
    $$PWD/libwebp/src/dec/idec.c \
    $$PWD/libwebp/src/dec/io.c \
    $$PWD/libwebp/src/dec/layer.c \
    $$PWD/libwebp/src/dec/quant.c \
    $$PWD/libwebp/src/dec/tree.c \
    $$PWD/libwebp/src/dec/vp8.c \
    $$PWD/libwebp/src/dec/vp8l.c \
    $$PWD/libwebp/src/dec/webp.c \
    $$PWD/libwebp/src/demux/demux.c \
    $$PWD/libwebp/src/dsp/cpu.c \
    $$PWD/libwebp/src/dsp/dec.c \
    $$PWD/libwebp/src/dsp/dec_neon.c \
    $$PWD/libwebp/src/dsp/dec_sse2.c \
    $$PWD/libwebp/src/dsp/enc.c \
    $$PWD/libwebp/src/dsp/enc_neon.c \
    $$PWD/libwebp/src/dsp/enc_sse2.c \
    $$PWD/libwebp/src/dsp/lossless.c \
    $$PWD/libwebp/src/dsp/upsampling.c \
    $$PWD/libwebp/src/dsp/upsampling_neon.c \
    $$PWD/libwebp/src/dsp/upsampling_sse2.c \
    $$PWD/libwebp/src/dsp/yuv.c \
    $$PWD/libwebp/src/enc/alpha.c \
    $$PWD/libwebp/src/enc/analysis.c \
    $$PWD/libwebp/src/enc/backward_references.c \
    $$PWD/libwebp/src/enc/config.c \
    $$PWD/libwebp/src/enc/cost.c \
    $$PWD/libwebp/src/enc/filter.c \
    $$PWD/libwebp/src/enc/frame.c \
    $$PWD/libwebp/src/enc/histogram.c \
    $$PWD/libwebp/src/enc/iterator.c \
    $$PWD/libwebp/src/enc/layer.c \
    $$PWD/libwebp/src/enc/picture.c \
    $$PWD/libwebp/src/enc/quant.c \
    $$PWD/libwebp/src/enc/syntax.c \
    $$PWD/libwebp/src/enc/token.c \
    $$PWD/libwebp/src/enc/tree.c \
    $$PWD/libwebp/src/enc/vp8l.c \
    $$PWD/libwebp/src/enc/webpenc.c \
    $$PWD/libwebp/src/mux/muxedit.c \
    $$PWD/libwebp/src/mux/muxinternal.c \
    $$PWD/libwebp/src/mux/muxread.c \
    $$PWD/libwebp/src/utils/alpha_processing.c \
    $$PWD/libwebp/src/utils/bit_reader.c \
    $$PWD/libwebp/src/utils/bit_writer.c \
    $$PWD/libwebp/src/utils/color_cache.c \
    $$PWD/libwebp/src/utils/filters.c \
    $$PWD/libwebp/src/utils/huffman.c \
    $$PWD/libwebp/src/utils/huffman_encode.c \
    $$PWD/libwebp/src/utils/quant_levels.c \
    $$PWD/libwebp/src/utils/quant_levels_dec.c \
    $$PWD/libwebp/src/utils/random.c \
    $$PWD/libwebp/src/utils/rescaler.c \
    $$PWD/libwebp/src/utils/thread.c \
    $$PWD/libwebp/src/utils/utils.c
