CONFIG += marvell
CONFIG += debug

QT += core gui widgets

QMAKE_CXXFLAGS += -Wno-unused-parameter
QMAKE_LFLAGS += '-Wl,-rpath,\'/home/steam/lib\''

RESOURCES += example.qrc

SOURCES += \
	controllermanager.cpp \
	hello.cpp \
	main.cpp \

HEADERS += \
	controllermanager.h \
	hello.h \

LIBS += -lSDL2

marvell {
	INCLUDEPATH += \
		$(MARVELL_ROOTFS)/usr/include/SDL2 \
}

TRANSLATIONS += \
	translations/example_bg_BG.ts \
	translations/example_cs_CZ.ts \
	translations/example_da_DK.ts \
	translations/example_de_DE.ts \
	translations/example_el_GR.ts \
	translations/example_en_US.ts \
	translations/example_es_ES.ts \
	translations/example_fi_FI.ts \
	translations/example_fr_FR.ts \
	translations/example_hu_HU.ts \
	translations/example_it_IT.ts \
	translations/example_ja_JP.ts \
	translations/example_ko_KR.ts \
	translations/example_nb_NO.ts \
	translations/example_nl_NL.ts \
	translations/example_pl_PL.ts \
	translations/example_pt_BR.ts \
	translations/example_pt_PT.ts \
	translations/example_ro_RO.ts \
	translations/example_ru_RU.ts \
	translations/example_sv_SE.ts \
	translations/example_th_TH.ts \
	translations/example_tr_TR.ts \
	translations/example_uk_UA.ts \
	translations/example_zh_CN.ts \
	translations/example_zh_TW.ts \
