msvc: DEFINES += JAS_WIN_MSVC_BUILD
INCLUDEPATH += $$PWD/jasper/src/libjasper/include $$PWD/libjasper/include
SOURCES += \
    $$PWD/jasper/src/libjasper/base/jas_cm.c \
    $$PWD/jasper/src/libjasper/base/jas_debug.c \
    $$PWD/jasper/src/libjasper/base/jas_getopt.c \
    $$PWD/jasper/src/libjasper/base/jas_icc.c \
    $$PWD/jasper/src/libjasper/base/jas_iccdata.c \
    $$PWD/jasper/src/libjasper/base/jas_image.c \
    $$PWD/jasper/src/libjasper/base/jas_init.c \
    $$PWD/jasper/src/libjasper/base/jas_malloc.c \
    $$PWD/jasper/src/libjasper/base/jas_seq.c \
    $$PWD/jasper/src/libjasper/base/jas_stream.c \
    $$PWD/jasper/src/libjasper/base/jas_string.c \
    $$PWD/jasper/src/libjasper/base/jas_tmr.c \
    $$PWD/jasper/src/libjasper/base/jas_tvp.c \
    $$PWD/jasper/src/libjasper/base/jas_version.c \
    $$PWD/jasper/src/libjasper/bmp/bmp_cod.c \
    $$PWD/jasper/src/libjasper/bmp/bmp_dec.c \
    $$PWD/jasper/src/libjasper/bmp/bmp_enc.c \
    $$PWD/jasper/src/libjasper/dummy.c \
    $$PWD/jasper/src/libjasper/jp2/jp2_cod.c \
    $$PWD/jasper/src/libjasper/jp2/jp2_dec.c \
    $$PWD/jasper/src/libjasper/jp2/jp2_enc.c \
    $$PWD/jasper/src/libjasper/jpc/jpc_bs.c \
    $$PWD/jasper/src/libjasper/jpc/jpc_cs.c \
    $$PWD/jasper/src/libjasper/jpc/jpc_dec.c \
    $$PWD/jasper/src/libjasper/jpc/jpc_enc.c \
    $$PWD/jasper/src/libjasper/jpc/jpc_math.c \
    $$PWD/jasper/src/libjasper/jpc/jpc_mct.c \
    $$PWD/jasper/src/libjasper/jpc/jpc_mqcod.c \
    $$PWD/jasper/src/libjasper/jpc/jpc_mqdec.c \
    $$PWD/jasper/src/libjasper/jpc/jpc_mqenc.c \
    $$PWD/jasper/src/libjasper/jpc/jpc_qmfb.c \
    $$PWD/jasper/src/libjasper/jpc/jpc_t1cod.c \
    $$PWD/jasper/src/libjasper/jpc/jpc_t1dec.c \
    $$PWD/jasper/src/libjasper/jpc/jpc_t1enc.c \
    $$PWD/jasper/src/libjasper/jpc/jpc_t2cod.c \
    $$PWD/jasper/src/libjasper/jpc/jpc_t2dec.c \
    $$PWD/jasper/src/libjasper/jpc/jpc_t2enc.c \
    $$PWD/jasper/src/libjasper/jpc/jpc_tagtree.c \
    $$PWD/jasper/src/libjasper/jpc/jpc_tsfb.c \
    $$PWD/jasper/src/libjasper/jpc/jpc_util.c \
    $$PWD/jasper/src/libjasper/jpg/jpg_val.c \
    $$PWD/jasper/src/libjasper/mif/mif_cod.c \
    $$PWD/jasper/src/libjasper/pgx/pgx_cod.c \
    $$PWD/jasper/src/libjasper/pgx/pgx_dec.c \
    $$PWD/jasper/src/libjasper/pgx/pgx_enc.c \
    $$PWD/jasper/src/libjasper/pnm/pnm_cod.c \
    $$PWD/jasper/src/libjasper/pnm/pnm_dec.c \
    $$PWD/jasper/src/libjasper/pnm/pnm_enc.c \
    $$PWD/jasper/src/libjasper/ras/ras_cod.c \
    $$PWD/jasper/src/libjasper/ras/ras_dec.c \
    $$PWD/jasper/src/libjasper/ras/ras_enc.c

LIBJPEG_DEP = $$PWD/../../../qtbase/src/3rdparty/libjpeg.pri
exists($${LIBJPEG_DEP}) {
    include($${LIBJPEG_DEP})
    SOURCES += \
        $$PWD/jasper/src/libjasper/jpg/jpg_dec.c \
        $$PWD/jasper/src/libjasper/jpg/jpg_enc.c
} else {
    SOURCES += \
        $$PWD/jasper/src/libjasper/jpg/jpg_dummy.c
}
