QMAKE_CLANG_DIR = "/usr"
QMAKE_CLANG_PATH = $$eval(QMAKE_MAC_SDK.macx-clang.$${QMAKE_MAC_SDK}.QMAKE_CXX)
!isEmpty(QMAKE_CLANG_PATH) {
    clang_dir = $$clean_path("$$dirname(QMAKE_CLANG_PATH)/../")
    exists($$clang_dir): QMAKE_CLANG_DIR = $$clang_dir
}

QMAKE_CLANG_PATH = "$${QMAKE_CLANG_DIR}/bin/clang++"
message("Using clang++ from $${QMAKE_CLANG_PATH}")
system("$${QMAKE_CLANG_PATH} --version")

GYP_CONFIG += \
    qt_os=\"mac\" \
    mac_sdk_min=\"10.7\" \
    mac_deployment_target=\"$${QMAKE_MACOSX_DEPLOYMENT_TARGET}\" \
    make_clang_dir=\"$${QMAKE_CLANG_DIR}\" \
    clang_use_chrome_plugins=0

QMAKE_MAC_SDK_PATH = "$$eval(QMAKE_MAC_SDK.$${QMAKE_MAC_SDK}.path)"
exists($$QMAKE_MAC_SDK_PATH): GYP_CONFIG += mac_sdk_path=\"$${QMAKE_MAC_SDK_PATH}\"
