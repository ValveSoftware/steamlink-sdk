/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qtdiag.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QStyleHints>
#include <QtGui/QScreen>
#include <QtGui/QFont>
#include <QtGui/QFontDatabase>
#include <QtGui/QPalette>
#ifndef QT_NO_OPENGL
#  include <QtGui/QOpenGLContext>
#  include <QtGui/QOpenGLFunctions>
#endif // QT_NO_OPENGL
#include <QtGui/QWindow>
#include <QtGui/QTouchDevice>

#ifdef NETWORK_DIAG
#  include <QSslSocket>
#endif

#include <QtCore/QLibraryInfo>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtCore/QSysInfo>
#include <QtCore/QLibraryInfo>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QTextStream>
#include <QtCore/QStandardPaths>
#include <QtCore/QDir>
#include <QtCore/QFileSelector>
#include <QtCore/QDebug>

#include <private/qsimd_p.h>
#include <private/qguiapplication_p.h>
#include <qpa/qplatformintegration.h>
#include <qpa/qplatformscreen.h>
#include <qpa/qplatformtheme.h>
#include <qpa/qplatformnativeinterface.h>
#include <private/qhighdpiscaling_p.h>

#include <algorithm>

QT_BEGIN_NAMESPACE

QTextStream &operator<<(QTextStream &str, const QSize &s)
{
    str << s.width() << 'x' << s.height();
    return str;
}

QTextStream &operator<<(QTextStream &str, const QSizeF &s)
{
    str << s.width() << 'x' << s.height();
    return str;
}

QTextStream &operator<<(QTextStream &str, const QDpi &d)
{
    str << d.first << ',' << d.second;
    return str;
}

QTextStream &operator<<(QTextStream &str, const QRect &r)
{
    str << r.size() << forcesign << r.x() << r.y() << noforcesign;
    return str;
}

QTextStream &operator<<(QTextStream &str, const QStringList &l)
{
    for (int i = 0; i < l.size(); ++i) {
        if (i)
            str << ',';
        str << l.at(i);
    }
    return str;
}

QTextStream &operator<<(QTextStream &str, const QFont &f)
{
    str << '"' << f.family() << "\" "  << f.pointSize();
    return str;
}

QTextStream &operator<<(QTextStream &str, QPlatformScreen::SubpixelAntialiasingType st)
{
    static const char *enumValues[] = {
        "Subpixel_None", "Subpixel_RGB", "Subpixel_BGR", "Subpixel_VRGB", "Subpixel_VBGR"
    };
    str << (size_t(st) < sizeof(enumValues) / sizeof(enumValues[0])
            ? enumValues[st] : "<Unknown>");
    return str;
}

#ifndef QT_NO_OPENGL

QTextStream &operator<<(QTextStream &str, const QSurfaceFormat &format)
{
    str << "Version: " << format.majorVersion() << '.'
        << format.minorVersion() << " Profile: " << format.profile()
        << " Swap behavior: " << format.swapBehavior()
        << " Buffer size (RGB";
    if (format.hasAlpha())
        str << 'A';
    str << "): " << format.redBufferSize() << ',' << format.greenBufferSize()
        << ',' << format.blueBufferSize();
    if (format.hasAlpha())
        str << ',' << format.alphaBufferSize();
    if (const int dbs = format.depthBufferSize())
        str << " Depth buffer: " << dbs;
    if (const int sbs = format.stencilBufferSize())
        str << " Stencil buffer: " << sbs;
    const int samples = format.samples();
    if (samples > 0)
        str << " Samples: " << samples;
    return str;
}

void dumpGlInfo(QTextStream &str, bool listExtensions)
{
    QOpenGLContext context;
    if (context.create()) {
#  ifdef QT_OPENGL_DYNAMIC
        str << "Dynamic GL ";
#  endif
        switch (context.openGLModuleType()) {
        case QOpenGLContext::LibGL:
            str << "LibGL";
            break;
        case QOpenGLContext::LibGLES:
            str << "LibGLES";
            break;
        }
        QWindow window;
        window.setSurfaceType(QSurface::OpenGLSurface);
        window.create();
        context.makeCurrent(&window);
        QOpenGLFunctions functions(&context);

        str << " Vendor: " << reinterpret_cast<const char *>(functions.glGetString(GL_VENDOR))
            << "\nRenderer: " << reinterpret_cast<const char *>(functions.glGetString(GL_RENDERER))
            << "\nVersion: " << reinterpret_cast<const char *>(functions.glGetString(GL_VERSION))
            << "\nShading language: " << reinterpret_cast<const char *>(functions.glGetString(GL_SHADING_LANGUAGE_VERSION))
            <<  "\nFormat: " << context.format();

        if (listExtensions) {
            QList<QByteArray> extensionList = context.extensions().toList();
            std::sort(extensionList.begin(), extensionList.end());
            str << " \nFound " << extensionList.size() << " extensions:\n";
            foreach (const QByteArray &extension, extensionList)
                str << "  " << extension << '\n';
        }
    } else {
        str << "Unable to create an Open GL context.\n";
    }
}

#endif // !QT_NO_OPENGL

#define DUMP_CAPABILITY(str, integration, capability) \
    if (platformIntegration->hasCapability(QPlatformIntegration::capability)) \
        str << ' ' << #capability;

// Dump values of QStandardPaths, indicate writable locations by asterisk.
static void dumpStandardLocation(QTextStream &str, QStandardPaths::StandardLocation location)
{
    str << '"' << QStandardPaths::displayName(location) << '"';
    const QStringList directories = QStandardPaths::standardLocations(location);
    const QString writableDirectory = QStandardPaths::writableLocation(location);
    const int writableIndex = writableDirectory.isEmpty() ? -1 : directories.indexOf(writableDirectory);
    for (int i = 0; i < directories.size(); ++i) {
        str << ' ';
        if (i == writableIndex)
            str << '*';
        str << QDir::toNativeSeparators(directories.at(i));
        if (i == writableIndex)
            str << '*';
    }
    if (!writableDirectory.isEmpty() && writableIndex < 0)
        str << " *" << QDir::toNativeSeparators(writableDirectory) << '*';
}

#define DUMP_CPU_FEATURE(feature, name)                 \
    if (qCpuHasFeature(feature))                        \
        str << " " name;

#define DUMP_STANDARDPATH(str, location) \
    str << "  " << #location << ": "; \
    dumpStandardLocation(str, QStandardPaths::location); \
    str << '\n';

#define DUMP_LIBRARYPATH(str, loc) \
    str << "  " << #loc << ": " << QDir::toNativeSeparators(QLibraryInfo::location(QLibraryInfo::loc)) << '\n';

// Helper to format a type via QDebug to be used for QFlags/Q_ENUM.
template <class T>
static QString formatQDebug(T t)
{
    QString result;
    QDebug(&result) << t;
    return result;
}

// Helper to format a type via QDebug, stripping the class name.
template <class T>
static QString formatValueQDebug(T t)
{
    QString result = formatQDebug(t).trimmed();
    if (result.endsWith(QLatin1Char(')'))) {
        result.chop(1);
        result.remove(0, result.indexOf(QLatin1Char('(')) + 1);
    }
    return result;
}

QTextStream &operator<<(QTextStream &str, const QPalette &palette)
{
    for (int r = 0; r < int(QPalette::NColorRoles); ++r) {
        const QPalette::ColorRole role = static_cast< QPalette::ColorRole>(r);
        const QColor color = palette.color(QPalette::Active, role);
        if (color.isValid())
            str << "  " << formatValueQDebug(role) << ": " << color.name(QColor::HexArgb) << '\n';
    }
    return str;
}

static inline QByteArrayList qtFeatures()
{
    QByteArrayList result;
#ifdef QT_NO_CLIPBOARD
    result.append("QT_NO_CLIPBOARD");
#endif
#ifdef QT_NO_CONTEXTMENU
    result.append("QT_NO_CONTEXTMENU");
#endif
#ifdef QT_NO_CURSOR
    result.append("QT_NO_CURSOR");
#endif
#ifdef QT_NO_DRAGANDDROP
    result.append("QT_NO_DRAGANDDROP");
#endif
#ifdef QT_NO_EXCEPTIONS
    result.append("QT_NO_EXCEPTIONS");
#endif
#ifdef QT_NO_LIBRARY
    result.append("QT_NO_LIBRARY");
#endif
#ifdef QT_NO_NETWORK
    result.append("QT_NO_NETWORK");
#endif
#ifdef QT_NO_OPENGL
    result.append("QT_NO_OPENGL");
#endif
#ifdef QT_NO_OPENSSL
    result.append("QT_NO_OPENSSL");
#endif
#ifdef QT_NO_PROCESS
    result.append("QT_NO_PROCESS");
#endif
#ifdef QT_NO_PRINTER
    result.append("QT_NO_PRINTER");
#endif
#ifdef QT_NO_SESSIONMANAGER
    result.append("QT_NO_SESSIONMANAGER");
#endif
#ifdef QT_NO_SETTINGS
    result.append("QT_NO_SETTINGS");
#endif
#ifdef QT_NO_SHORTCUT
    result.append("QT_NO_SHORTCUT");
#endif
#ifdef QT_NO_SYSTEMTRAYICON
    result.append("QT_NO_SYSTEMTRAYICON");
#endif
#ifdef QT_NO_QTHREAD
    result.append("QT_NO_QTHREAD");
#endif
#ifdef QT_NO_WHATSTHIS
    result.append("QT_NO_WHATSTHIS");
#endif
#ifdef QT_NO_WIDGETS
    result.append("QT_NO_WIDGETS");
#endif
#ifdef QT_NO_ZLIB
    result.append("QT_NO_ZLIB");
#endif
    return result;
}

QString qtDiag(unsigned flags)
{
    QString result;
    QTextStream str(&result);

    const QPlatformIntegration *platformIntegration = QGuiApplicationPrivate::platformIntegration();
    str << QLibraryInfo::build() << " on \"" << QGuiApplication::platformName() << "\" "
              << '\n'
        << "OS: " << QSysInfo::prettyProductName()
              << " [" << QSysInfo::kernelType() << " version " << QSysInfo::kernelVersion() << "]\n";

    str << "\nArchitecture: " << QSysInfo::currentCpuArchitecture() << "; features:";
#if defined(Q_PROCESSOR_X86)
    DUMP_CPU_FEATURE(SSE2, "SSE2");
    DUMP_CPU_FEATURE(SSE3, "SSE3");
    DUMP_CPU_FEATURE(SSSE3, "SSSE3");
    DUMP_CPU_FEATURE(SSE4_1, "SSE4.1");
    DUMP_CPU_FEATURE(SSE4_2, "SSE4.2");
    DUMP_CPU_FEATURE(AVX, "AVX");
    DUMP_CPU_FEATURE(AVX2, "AVX2");
    DUMP_CPU_FEATURE(RTM, "RTM");
    DUMP_CPU_FEATURE(HLE, "HLE");
#elif defined(Q_PROCESSOR_ARM)
    DUMP_CPU_FEATURE(ARM_NEON, "Neon");
#elif defined(Q_PROCESSOR_MIPS)
    DUMP_CPU_FEATURE(DSP, "DSP");
    DUMP_CPU_FEATURE(DSPR2, "DSPR2");
#endif
    str << '\n';

#ifndef QT_NO_PROCESS
    const QProcessEnvironment systemEnvironment = QProcessEnvironment::systemEnvironment();
    str << "\nEnvironment:\n";
    foreach (const QString &key, systemEnvironment.keys()) {
        if (key.startsWith(QLatin1Char('Q')))
           str << "  " << key << "=\"" << systemEnvironment.value(key) << "\"\n";
    }
#endif // !QT_NO_PROCESS

    const QByteArrayList features = qtFeatures();
    if (!features.isEmpty())
        str << "\nFeatures: " << features.join(' ') << '\n';

    str << "\nLibrary info:\n";
    DUMP_LIBRARYPATH(str, PrefixPath)
    DUMP_LIBRARYPATH(str, DocumentationPath)
    DUMP_LIBRARYPATH(str, HeadersPath)
    DUMP_LIBRARYPATH(str, LibrariesPath)
    DUMP_LIBRARYPATH(str, LibraryExecutablesPath)
    DUMP_LIBRARYPATH(str, BinariesPath)
    DUMP_LIBRARYPATH(str, PluginsPath)
    DUMP_LIBRARYPATH(str, ImportsPath)
    DUMP_LIBRARYPATH(str, Qml2ImportsPath)
    DUMP_LIBRARYPATH(str, ArchDataPath)
    DUMP_LIBRARYPATH(str, DataPath)
    DUMP_LIBRARYPATH(str, TranslationsPath)
    DUMP_LIBRARYPATH(str, ExamplesPath)
    DUMP_LIBRARYPATH(str, TestsPath)
    DUMP_LIBRARYPATH(str, SettingsPath)

    str << "\nStandard paths [*...* denote writable entry]:\n";
    DUMP_STANDARDPATH(str, DesktopLocation)
    DUMP_STANDARDPATH(str, DocumentsLocation)
    DUMP_STANDARDPATH(str, FontsLocation)
    DUMP_STANDARDPATH(str, ApplicationsLocation)
    DUMP_STANDARDPATH(str, MusicLocation)
    DUMP_STANDARDPATH(str, MoviesLocation)
    DUMP_STANDARDPATH(str, PicturesLocation)
    DUMP_STANDARDPATH(str, TempLocation)
    DUMP_STANDARDPATH(str, HomeLocation)
    DUMP_STANDARDPATH(str, AppLocalDataLocation)
    DUMP_STANDARDPATH(str, CacheLocation)
    DUMP_STANDARDPATH(str, GenericDataLocation)
    DUMP_STANDARDPATH(str, RuntimeLocation)
    DUMP_STANDARDPATH(str, ConfigLocation)
    DUMP_STANDARDPATH(str, DownloadLocation)
    DUMP_STANDARDPATH(str, GenericCacheLocation)
    DUMP_STANDARDPATH(str, GenericConfigLocation)
    DUMP_STANDARDPATH(str, AppDataLocation)
    DUMP_STANDARDPATH(str, AppConfigLocation)

    str << "\nFile selectors (increasing order of precedence):\n ";
    foreach (const QString &s, QFileSelector().allSelectors())
        str << ' ' << s;

    str << "\n\nNetwork:\n  ";
#ifdef NETWORK_DIAG
#  ifndef QT_NO_SSL
    if (QSslSocket::supportsSsl()) {
        str << "Using \"" << QSslSocket::sslLibraryVersionString() << "\", version: 0x"
            << hex << QSslSocket::sslLibraryVersionNumber() << dec;
    } else {
        str << "\nSSL is not supported.";
    }
#  else // !QT_NO_SSL
    str << "SSL is not available.";
#  endif // QT_NO_SSL
#else
    str << "Qt Network module is not available.";
#endif // NETWORK_DIAG

    str << "\n\nPlatform capabilities:";
    DUMP_CAPABILITY(str, platformIntegration, ThreadedPixmaps)
    DUMP_CAPABILITY(str, platformIntegration, OpenGL)
    DUMP_CAPABILITY(str, platformIntegration, ThreadedOpenGL)
    DUMP_CAPABILITY(str, platformIntegration, SharedGraphicsCache)
    DUMP_CAPABILITY(str, platformIntegration, BufferQueueingOpenGL)
    DUMP_CAPABILITY(str, platformIntegration, WindowMasks)
    DUMP_CAPABILITY(str, platformIntegration, MultipleWindows)
    DUMP_CAPABILITY(str, platformIntegration, ApplicationState)
    DUMP_CAPABILITY(str, platformIntegration, ForeignWindows)
    DUMP_CAPABILITY(str, platformIntegration, NonFullScreenWindows)
    DUMP_CAPABILITY(str, platformIntegration, NativeWidgets)
    DUMP_CAPABILITY(str, platformIntegration, WindowManagement)
    DUMP_CAPABILITY(str, platformIntegration, SyncState)
    DUMP_CAPABILITY(str, platformIntegration, RasterGLSurface)
    DUMP_CAPABILITY(str, platformIntegration, AllGLFunctionsQueryable)
    DUMP_CAPABILITY(str, platformIntegration, ApplicationIcon)
    DUMP_CAPABILITY(str, platformIntegration, SwitchableWidgetComposition)
    str << '\n';

    const QStyleHints *styleHints = QGuiApplication::styleHints();
    const QChar passwordMaskCharacter = styleHints->passwordMaskCharacter();
    str << "\nStyle hints:\n  mouseDoubleClickInterval: " << styleHints->mouseDoubleClickInterval() << '\n'
        << "  mousePressAndHoldInterval: " << styleHints->mousePressAndHoldInterval() << '\n'
        << "  startDragDistance: " << styleHints->startDragDistance() << '\n'
        << "  startDragTime: " << styleHints->startDragTime() << '\n'
        << "  startDragVelocity: " << styleHints->startDragVelocity() << '\n'
        << "  keyboardInputInterval: " << styleHints->keyboardInputInterval() << '\n'
        << "  keyboardAutoRepeatRate: " << styleHints->keyboardAutoRepeatRate() << '\n'
        << "  cursorFlashTime: " << styleHints->cursorFlashTime() << '\n'
        << "  showIsFullScreen: " << styleHints->showIsFullScreen() << '\n'
        << "  showIsMaximized: " << styleHints->showIsMaximized() << '\n'
        << "  passwordMaskDelay: " << styleHints->passwordMaskDelay() << '\n'
        << "  passwordMaskCharacter: ";
    if (passwordMaskCharacter.unicode() >= 32 && passwordMaskCharacter.unicode() < 128)
        str << '\'' << passwordMaskCharacter << '\'';
    else
        str << "U+" << qSetFieldWidth(4) << qSetPadChar('0') << uppercasedigits << hex << passwordMaskCharacter.unicode() << dec << qSetFieldWidth(0);
    str << '\n'
        << "  fontSmoothingGamma: " << styleHints->fontSmoothingGamma() << '\n'
        << "  useRtlExtensions: " << styleHints->useRtlExtensions() << '\n'
        << "  setFocusOnTouchRelease: " << styleHints->setFocusOnTouchRelease() << '\n'
        << "  tabFocusBehavior: " << formatQDebug(styleHints->tabFocusBehavior()) << '\n'
        << "  singleClickActivation: " << styleHints->singleClickActivation() << '\n';
    str << "\nAdditional style hints (QPlatformIntegration):\n"
        << "  ReplayMousePressOutsidePopup: "
        << platformIntegration->styleHint(QPlatformIntegration::ReplayMousePressOutsidePopup).toBool() << '\n';

    const QPlatformTheme *platformTheme = QGuiApplicationPrivate::platformTheme();
    str << "\nTheme:"
           "\n  Available    : " << platformIntegration->themeNames()
        << "\n  Styles       : " << platformTheme->themeHint(QPlatformTheme::StyleNames).toStringList();
    const QString iconTheme = platformTheme->themeHint(QPlatformTheme::SystemIconThemeName).toString();
    if (!iconTheme.isEmpty()) {
        str << "\n  Icon theme   : " << iconTheme
            << ", " << platformTheme->themeHint(QPlatformTheme::SystemIconFallbackThemeName).toString()
            << " from " << platformTheme->themeHint(QPlatformTheme::IconThemeSearchPaths).toStringList() << '\n';
    }
    if (const QFont *systemFont = platformTheme->font())
        str << "\n  System font  : " << *systemFont<< '\n';

    if (platformTheme->usePlatformNativeDialog(QPlatformTheme::FileDialog))
        str << "  Native file dialog\n";
    if (platformTheme->usePlatformNativeDialog(QPlatformTheme::ColorDialog))
        str << "  Native color dialog\n";
    if (platformTheme->usePlatformNativeDialog(QPlatformTheme::FontDialog))
        str << "  Native font dialog\n";
    if (platformTheme->usePlatformNativeDialog(QPlatformTheme::MessageDialog))
        str << "  Native message dialog\n";

    str << "\nFonts:\n  General font : " << QFontDatabase::systemFont(QFontDatabase::GeneralFont) << '\n'
              << "  Fixed font   : " << QFontDatabase::systemFont(QFontDatabase::FixedFont) << '\n'
              << "  Title font   : " << QFontDatabase::systemFont(QFontDatabase::TitleFont) << '\n'
              << "  Smallest font: " << QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont) << '\n';
    if (flags & QtDiagFonts) {
        QFontDatabase fontDatabase;
        const QStringList families = fontDatabase.families();
        str << "\n  Families (" << families.size() << "):\n";
        for (int i = 0, count = families.size(); i < count; ++i)
            str << "    " << families.at(i) << '\n';

        const QList<int> standardSizes = QFontDatabase::standardSizes();
        str << "\n  Standard Sizes:";
        for (int i = 0, count = standardSizes.size(); i < count; ++i)
            str << ' ' << standardSizes.at(i);
        QList<QFontDatabase::WritingSystem> writingSystems = fontDatabase.writingSystems();
        str << "\n\n  Writing systems:\n";
        for (int i = 0, count = writingSystems.size(); i < count; ++i)
            str << "    " << formatValueQDebug(writingSystems.at(i)) << '\n';
    }

    str << "\nPalette:\n" << QGuiApplication::palette();

    const QList<QScreen*> screens = QGuiApplication::screens();
    const int screenCount = screens.size();
    str << "\nScreens: " << screenCount << ", High DPI scaling: "
        << (QHighDpiScaling::isActive() ? "active" : "inactive") << '\n';
    for (int s = 0; s < screenCount; ++s) {
        const QScreen *screen = screens.at(s);
        const QPlatformScreen *platformScreen = screen->handle();
        const QRect geometry = screen->geometry();
        const QDpi dpi(screen->logicalDotsPerInchX(), screen->logicalDotsPerInchY());
        const QDpi nativeDpi = platformScreen->logicalDpi();
        const QRect nativeGeometry = platformScreen->geometry();
        str << '#' << ' ' << s << " \"" << screen->name() << '"'
                  << " Depth: " << screen->depth()
                  << " Primary: " <<  (screen == QGuiApplication::primaryScreen() ? "yes" : "no")
            << "\n  Geometry: " << geometry;
        if (geometry != nativeGeometry)
            str << " (native: " << nativeGeometry << ')';
        str << " Available: " << screen->availableGeometry();
        if (geometry != screen->virtualGeometry())
            str << "\n  Virtual geometry: " << screen->virtualGeometry() << " Available: " << screen->availableVirtualGeometry();
        if (screen->virtualSiblings().size() > 1)
            str << "\n  " << screen->virtualSiblings().size() << " virtual siblings";
        str << "\n  Physical size: " << screen->physicalSize() << " mm"
            << "  Refresh: " << screen->refreshRate() << " Hz"
            << " Power state: " << platformScreen->powerState();
        str << "\n  Physical DPI: " << screen->physicalDotsPerInchX()
            << ',' << screen->physicalDotsPerInchY()
            << " Logical DPI: " << dpi;
        if (dpi != nativeDpi)
            str << " (native: " << nativeDpi << ')';
        str << ' ' << platformScreen->subpixelAntialiasingTypeHint() << "\n  ";
        if (QHighDpiScaling::isActive())
            str << "High DPI scaling factor: " << QHighDpiScaling::factor(screen) << ' ';
        str << "DevicePixelRatio: " << screen->devicePixelRatio()
            << " Pixel density: " << platformScreen->pixelDensity();
        str << "\n  Primary orientation: " << screen->primaryOrientation()
            << " Orientation: " << screen->orientation()
            << " Native orientation: " << screen->nativeOrientation()
            << " OrientationUpdateMask: " << screen->orientationUpdateMask()
            << "\n\n";
    }

    const QList<const QTouchDevice *> touchDevices = QTouchDevice::devices();
    if (!touchDevices.isEmpty()) {
        str << "Touch devices: " << touchDevices.size() << '\n';
        foreach (const QTouchDevice *device, touchDevices) {
            str << "  " << (device->type() == QTouchDevice::TouchScreen ? "TouchScreen" : "TouchPad")
                << " \"" << device->name() << "\", max " << device->maximumTouchPoints()
                << " touch points, capabilities:";
            const QTouchDevice::Capabilities capabilities = device->capabilities();
            if (capabilities & QTouchDevice::Position)
                str << " Position";
            if (capabilities & QTouchDevice::Area)
                str << " Area";
            if (capabilities & QTouchDevice::Pressure)
                str << " Pressure";
            if (capabilities & QTouchDevice::Velocity)
                str << " Velocity";
            if (capabilities & QTouchDevice::RawPositions)
                str << " RawPositions";
            if (capabilities & QTouchDevice::NormalizedPosition)
                str << " NormalizedPosition";
            if (capabilities & QTouchDevice::MouseEmulation)
                str << " MouseEmulation";
            str << '\n';
        }
        str << "\n\n";
    }

#ifndef QT_NO_OPENGL
    if (flags & QtDiagGl) {
        dumpGlInfo(str, flags & QtDiagGlExtensions);
        str << "\n\n";
    }
#else
    Q_UNUSED(flags)
#endif // !QT_NO_OPENGL

    // On Windows, this will provide addition GPU info similar to the output of dxdiag.
    if (const QPlatformNativeInterface *ni = QGuiApplication::platformNativeInterface()) {
        const QVariant gpuInfoV = ni->property("gpu");
        if (gpuInfoV.type() == QVariant::Map) {
            const QString description = gpuInfoV.toMap().value(QStringLiteral("printable")).toString();
            if (!description.isEmpty())
                str << "\nGPU:\n" << description;
        }
    }
    return result;
}

QT_END_NAMESPACE
