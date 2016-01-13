/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the tools applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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
#ifndef QT_NO_OPENGL
#  include <QtGui/QOpenGLContext>
#  include <QtGui/QOpenGLFunctions>
#endif // QT_NO_OPENGL
#include <QtGui/QWindow>

#ifdef NETWORK_DIAG
#  include <QSslSocket>
#endif

#include <QtCore/QLibraryInfo>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtCore/QSysInfo>
#include <QtCore/QLibraryInfo>
#include <QtCore/QTextStream>
#include <QtCore/QStandardPaths>
#include <QtCore/QDir>
#include <QtCore/QFileSelector>

#include <private/qsimd_p.h>
#include <private/qguiapplication_p.h>
#include <qpa/qplatformintegration.h>
#include <qpa/qplatformtheme.h>
#include <qpa/qplatformnativeinterface.h>

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

QTextStream &operator<<(QTextStream &str, const QRect &r)
{
    str << r.size() << '+' << r.x() << '+' << r.y();
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
    const int writableIndex = directories.indexOf(writableDirectory);
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
    DUMP_CPU_FEATURE(SSE2, "SSE2");
    DUMP_CPU_FEATURE(SSE3, "SSE3");
    DUMP_CPU_FEATURE(SSSE3, "SSSE3");
    DUMP_CPU_FEATURE(SSE4_1, "SSE4.1");
    DUMP_CPU_FEATURE(SSE4_2, "SSE4.2");
    DUMP_CPU_FEATURE(AVX, "AVX");
    DUMP_CPU_FEATURE(AVX2, "AVX2");
    DUMP_CPU_FEATURE(RTM, "RTM");
    DUMP_CPU_FEATURE(HLE, "HLE");
    DUMP_CPU_FEATURE(ARM_NEON, "Neon");
    str << '\n';

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
    DUMP_STANDARDPATH(str, DataLocation)
    DUMP_STANDARDPATH(str, CacheLocation)
    DUMP_STANDARDPATH(str, GenericDataLocation)
    DUMP_STANDARDPATH(str, RuntimeLocation)
    DUMP_STANDARDPATH(str, ConfigLocation)
    DUMP_STANDARDPATH(str, DownloadLocation)
    DUMP_STANDARDPATH(str, GenericCacheLocation)
    DUMP_STANDARDPATH(str, GenericConfigLocation)

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
        << "  passwordMaskDelay: " << styleHints->passwordMaskDelay() << '\n'
        << "  passwordMaskCharacter: ";
    if (passwordMaskCharacter.unicode() >= 32 && passwordMaskCharacter.unicode() < 128)
        str << '\'' << passwordMaskCharacter << '\'';
    else
        str << "U+" << qSetFieldWidth(4) << qSetPadChar('0') << uppercasedigits << hex << passwordMaskCharacter.unicode() << dec << qSetFieldWidth(0);
    str << '\n'
        << "  fontSmoothingGamma: " << styleHints->fontSmoothingGamma() << '\n'
        << "  useRtlExtensions: " << styleHints->useRtlExtensions() << '\n'
        << "  setFocusOnTouchRelease: " << styleHints->setFocusOnTouchRelease() << '\n';

    const QPlatformTheme *platformTheme = QGuiApplicationPrivate::platformTheme();
    str << "\nTheme:\n  Styles: " << platformTheme->themeHint(QPlatformTheme::StyleNames).toStringList();
    const QString iconTheme = platformTheme->themeHint(QPlatformTheme::SystemIconThemeName).toString();
    if (!iconTheme.isEmpty()) {
        str << "\n  Icon theme: " << iconTheme
            << ", " << platformTheme->themeHint(QPlatformTheme::SystemIconFallbackThemeName).toString()
            << " from " << platformTheme->themeHint(QPlatformTheme::IconThemeSearchPaths).toStringList() << '\n';
    }
    if (const QFont *systemFont = platformTheme->font())
        str << "  System font: " << *systemFont<< '\n';
    str << "  General font : " << QFontDatabase::systemFont(QFontDatabase::GeneralFont) << '\n'
              << "  Fixed font   : " << QFontDatabase::systemFont(QFontDatabase::FixedFont) << '\n'
              << "  Title font   : " << QFontDatabase::systemFont(QFontDatabase::TitleFont) << '\n'
              << "  Smallest font: " << QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont) << '\n';

    if (platformTheme->usePlatformNativeDialog(QPlatformTheme::FileDialog))
        str << "  Native file dialog\n";
    if (platformTheme->usePlatformNativeDialog(QPlatformTheme::ColorDialog))
        str << "  Native color dialog\n";
    if (platformTheme->usePlatformNativeDialog(QPlatformTheme::FontDialog))
        str << "  Native font dialog\n";

    const QList<QScreen*> screens = QGuiApplication::screens();
    const int screenCount = screens.size();
    str << "\nScreens: " << screenCount << '\n';
    for (int s = 0; s < screenCount; ++s) {
        const QScreen *screen = screens.at(s);
        str << '#' << ' ' << s << " \"" << screen->name() << '"'
                  << " Depth: " << screen->depth()
                  << " Primary: " <<  (screen == QGuiApplication::primaryScreen() ? "yes" : "no")
            << "\n  Geometry: " << screen->geometry() << " Available: " << screen->availableGeometry();
        if (screen->geometry() != screen->virtualGeometry())
            str << "\n  Virtual geometry: " << screen->virtualGeometry() << " Available: " << screen->availableVirtualGeometry();
        if (screen->virtualSiblings().size() > 1)
            str << "\n  " << screen->virtualSiblings().size() << " virtual siblings";
        str << "\n  Physical size: " << screen->physicalSize() << " mm"
                  << "  Refresh: " << screen->refreshRate() << " Hz"
            << "\n  Physical DPI: " << screen->physicalDotsPerInchX()
            << ',' << screen->physicalDotsPerInchY()
            << " Logical DPI: " << screen->logicalDotsPerInchX()
            << ',' << screen->logicalDotsPerInchY()
            << "\n  DevicePixelRatio: " << screen->devicePixelRatio()
            << " Primary orientation: " << screen->primaryOrientation()
            << "\n  Orientation: " << screen->orientation()
            << " Native orientation: " << screen->nativeOrientation()
            << " OrientationUpdateMask: " << screen->orientationUpdateMask()
            << "\n\n";
    }

#ifndef QT_NO_OPENGL
    dumpGlInfo(str, flags & QtDiagGlExtensions);
    str << "\n\n";
#else
    Q_UNUSED(flags)
#endif // !QT_NO_OPENGL

    // On Windows, this will provide addition GPU info similar to the output of dxdiag.
    const QVariant gpuInfoV = QGuiApplication::platformNativeInterface()->property("gpu");
    if (gpuInfoV.type() == QVariant::Map) {
        const QString description = gpuInfoV.toMap().value(QStringLiteral("printable")).toString();
        if (!description.isEmpty())
            str << "\nGPU:\n" << description;
    }
    return result;
}

QT_END_NAMESPACE
