CONFIG += java
load(sdk)

DESTDIR = $$[QT_INSTALL_PREFIX/get]/jar

PATHPREFIX = $$PWD/src/org/qtproject/qt5/android/purchasing/

!build_pass {
    isEmpty(SDK_ROOT): SDK_ROOT = $$(ANDROID_SDK_ROOT)
    isEmpty(SDK_ROOT): SDK_ROOT = $$DEFAULT_ANDROID_SDK_ROOT

    isEmpty(BUILD_TOOLS_REVISION) {
        BUILD_TOOLS_REVISION = $$(ANDROID_BUILD_TOOLS_REVISION)
        isEmpty(BUILD_TOOLS_REVISION) {
            BUILD_TOOLS_REVISIONS = $$files($$SDK_ROOT/build-tools/*)
            for (REVISION, BUILD_TOOLS_REVISIONS) {
                BASENAME = $$basename(REVISION)
                greaterThan(BASENAME, $$BUILD_TOOLS_REVISION): BUILD_TOOLS_REVISION = $$BASENAME
            }
        }
    }

    FRAMEWORK_AIDL_FILE = $$SDK_ROOT/platforms/$$API_VERSION_TO_USE/framework.aidl
    !exists($$FRAMEWORK_AIDL_FILE) {
        error("The Path $$FRAMEWORK_AIDL_FILE does not exist. Make sure the ANDROID_SDK_ROOT and ANDROID_API_VERSION environment variables are correctly set.")
    }

    AIDL_CMD = $$SDK_ROOT/platform-tools/aidl
    contains(QMAKE_HOST.os, Windows): AIDL_CMD = $$AIDL_CMD".exe"
    !exists($$AIDL_CMD) {
        AIDL_CMD = $$SDK_ROOT/build-tools/$$BUILD_TOOLS_REVISION/aidl
        contains(QMAKE_HOST.os, Windows): AIDL_CMD = $$AIDL_CMD".exe"
    }
    !exists($$AIDL_CMD): error("The path $$AIDL_CMD does not exist. Please set the environment variable ANDROID_BUILD_TOOLS_REVISION to the revision of the build tools installed in your Android SDK.")

    system($$AIDL_CMD -I$$PWD/src -p$$FRAMEWORK_AIDL_FILE $$PWD/src/com/android/vending/billing/IInAppBillingService.aidl $$PWD/src/com/android/vending/billing/IInAppBillingService.java)
}

JAVACLASSPATH += $$PWD/src/
JAVASOURCES += \
    $$PATHPREFIX/QtInAppPurchase.java \
    $$PATHPREFIX/Security.java \
    $$PATHPREFIX/Base64.java \
    $$PATHPREFIX/Base64DecoderException.java \
    $$PWD/src/com/android/vending/billing/IInAppBillingService.java



# install
target.path = $$[QT_INSTALL_PREFIX]/jar
INSTALLS += target

OTHER_FILES += $$JAVASOURCES
