CONFIG += object_parallel_to_source no_batch

INCLUDEPATH += \
    $$PWD/libwebp \
    $$PWD/libwebp/src \
    $$PWD/libwebp/src/dec \
    $$PWD/libwebp/src/enc \
    $$PWD/libwebp/src/extra \
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
    $$PWD/libwebp/src/dec/quant.c \
    $$PWD/libwebp/src/dec/tree.c \
    $$PWD/libwebp/src/dec/vp8.c \
    $$PWD/libwebp/src/dec/vp8l.c \
    $$PWD/libwebp/src/dec/webp.c \
    $$PWD/libwebp/src/demux/demux.c \
    $$PWD/libwebp/src/demux/anim_decode.c \
    $$PWD/libwebp/src/dsp/alpha_processing_mips_dsp_r2.c \
    $$PWD/libwebp/src/dsp/alpha_processing_sse41.c \
    $$PWD/libwebp/src/dsp/argb.c \
    $$PWD/libwebp/src/dsp/argb_mips_dsp_r2.c \
    $$PWD/libwebp/src/dsp/argb_sse2.c \
    $$PWD/libwebp/src/dsp/cost.c \
    $$PWD/libwebp/src/dsp/cost_mips32.c \
    $$PWD/libwebp/src/dsp/cost_mips_dsp_r2.c \
    $$PWD/libwebp/src/dsp/cost_sse2.c \
    $$PWD/libwebp/src/dsp/cpu.c \
    $$PWD/libwebp/src/dsp/dec.c \
    $$PWD/libwebp/src/dsp/dec_mips_dsp_r2.c \
    $$PWD/libwebp/src/dsp/dec_msa.c \
    $$PWD/libwebp/src/dsp/dec_sse2.c \
    $$PWD/libwebp/src/dsp/dec_sse41.c \
    $$PWD/libwebp/src/dsp/enc.c \
    $$PWD/libwebp/src/dsp/enc_mips_dsp_r2.c \
    $$PWD/libwebp/src/dsp/enc_sse2.c \
    $$PWD/libwebp/src/dsp/enc_sse41.c \
    $$PWD/libwebp/src/dsp/filters.c \
    $$PWD/libwebp/src/dsp/filters_mips_dsp_r2.c \
    $$PWD/libwebp/src/dsp/filters_sse2.c \
    $$PWD/libwebp/src/dsp/lossless.c \
    $$PWD/libwebp/src/dsp/lossless_enc.c \
    $$PWD/libwebp/src/dsp/lossless_enc_mips32.c \
    $$PWD/libwebp/src/dsp/lossless_enc_mips_dsp_r2.c \
    $$PWD/libwebp/src/dsp/lossless_enc_sse2.c \
    $$PWD/libwebp/src/dsp/lossless_enc_sse41.c \
    $$PWD/libwebp/src/dsp/lossless_mips_dsp_r2.c \
    $$PWD/libwebp/src/dsp/rescaler.c \
    $$PWD/libwebp/src/dsp/rescaler_mips32.c \
    $$PWD/libwebp/src/dsp/rescaler_mips_dsp_r2.c \
    $$PWD/libwebp/src/dsp/rescaler_sse2.c \
    $$PWD/libwebp/src/dsp/upsampling.c \
    $$PWD/libwebp/src/dsp/upsampling_mips_dsp_r2.c \
    $$PWD/libwebp/src/dsp/upsampling_sse2.c \
    $$PWD/libwebp/src/dsp/yuv.c \
    $$PWD/libwebp/src/dsp/yuv_mips_dsp_r2.c \
    $$PWD/libwebp/src/dsp/alpha_processing.c \
    $$PWD/libwebp/src/dsp/alpha_processing_sse2.c \
    $$PWD/libwebp/src/dsp/dec_clip_tables.c \
    $$PWD/libwebp/src/dsp/dec_mips32.c \
    $$PWD/libwebp/src/dsp/enc_avx2.c \
    $$PWD/libwebp/src/dsp/enc_mips32.c \
    $$PWD/libwebp/src/dsp/lossless_sse2.c \
    $$PWD/libwebp/src/dsp/yuv_mips32.c \
    $$PWD/libwebp/src/dsp/yuv_sse2.c \
    $$PWD/libwebp/src/enc/alpha.c \
    $$PWD/libwebp/src/enc/analysis.c \
    $$PWD/libwebp/src/enc/backward_references.c \
    $$PWD/libwebp/src/enc/config.c \
    $$PWD/libwebp/src/enc/cost.c \
    $$PWD/libwebp/src/enc/delta_palettization.c \
    $$PWD/libwebp/src/enc/filter.c \
    $$PWD/libwebp/src/enc/frame.c \
    $$PWD/libwebp/src/enc/histogram.c \
    $$PWD/libwebp/src/enc/iterator.c \
    $$PWD/libwebp/src/enc/near_lossless.c \
    $$PWD/libwebp/src/enc/picture.c \
    $$PWD/libwebp/src/enc/quant.c \
    $$PWD/libwebp/src/enc/syntax.c \
    $$PWD/libwebp/src/enc/token.c \
    $$PWD/libwebp/src/enc/tree.c \
    $$PWD/libwebp/src/enc/vp8l.c \
    $$PWD/libwebp/src/enc/webpenc.c \
    $$PWD/libwebp/src/enc/picture_csp.c \
    $$PWD/libwebp/src/enc/picture_psnr.c \
    $$PWD/libwebp/src/enc/picture_rescale.c \
    $$PWD/libwebp/src/enc/picture_tools.c \
    $$PWD/libwebp/src/extras/extras.c \
    $$PWD/libwebp/src/mux/anim_encode.c \
    $$PWD/libwebp/src/mux/muxedit.c \
    $$PWD/libwebp/src/mux/muxinternal.c \
    $$PWD/libwebp/src/mux/muxread.c \
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

android {
    SOURCES += $$NDK_ROOT/sources/android/cpufeatures/cpu-features.c
    INCLUDEPATH += $$NDK_ROOT/sources/android/cpufeatures
}

equals(QT_ARCH, arm)|equals(QT_ARCH, arm64) {
    SOURCES_FOR_NEON += \
        $$PWD/libwebp/src/dsp/dec_neon.c \
        $$PWD/libwebp/src/dsp/enc_neon.c \
        $$PWD/libwebp/src/dsp/lossless_enc_neon.c \
        $$PWD/libwebp/src/dsp/lossless_neon.c \
        $$PWD/libwebp/src/dsp/rescaler_neon.c \
        $$PWD/libwebp/src/dsp/upsampling_neon.c

    contains(QT_CPU_FEATURES.$$QT_ARCH, neon) {
        # Default compiler settings include this feature, so just add to SOURCES
        SOURCES += $$SOURCES_FOR_NEON
    } else {
        neon_comp.commands = $$QMAKE_CC -c $(CFLAGS)
        neon_comp.commands += $$QMAKE_CFLAGS_NEON
        neon_comp.commands += $(INCPATH) ${QMAKE_FILE_IN}
        msvc: neon_comp.commands += -Fo${QMAKE_FILE_OUT}
        else: neon_comp.commands += -o ${QMAKE_FILE_OUT}
        neon_comp.dependency_type = TYPE_C
        neon_comp.output = ${QMAKE_VAR_OBJECTS_DIR}${QMAKE_FILE_BASE}$${first(QMAKE_EXT_OBJ)}
        neon_comp.input = SOURCES_FOR_NEON
        neon_comp.variable_out = OBJECTS
        neon_comp.name = compiling[neon] ${QMAKE_FILE_IN}
        silent: neon_comp.commands = @echo compiling[neon] ${QMAKE_FILE_IN} && $$neon_comp.commands
        QMAKE_EXTRA_COMPILERS += neon_comp
    }
}
