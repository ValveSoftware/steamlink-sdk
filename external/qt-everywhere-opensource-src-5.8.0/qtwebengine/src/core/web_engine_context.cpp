/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "web_engine_context.h"

#include <math.h>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "cc/base/switches.h"
#if defined(ENABLE_BASIC_PRINTING)
#include "chrome/browser/printing/print_job_manager.h"
#endif // defined(ENABLE_BASIC_PRINTING)
#include "components/devtools_http_handler/devtools_http_handler.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/utility_process_host_impl.h"
#include "content/gpu/in_process_gpu_thread.h"
#include "content/public/app/content_main.h"
#include "content/public/app/content_main_runner.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/renderer/in_process_renderer_thread.h"
#include "content/utility/in_process_utility_thread.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "ui/events/event_switches.h"
#include "ui/native_theme/native_theme_switches.h"
#include "ui/gl/gl_switches.h"
#if defined(OS_WIN)
#include "sandbox/win/src/sandbox_types.h"
#include "content/public/app/sandbox_helper_win.h"
#endif // OS_WIN

#include "browser_context_adapter.h"
#include "content_browser_client_qt.h"
#include "content_client_qt.h"
#include "content_main_delegate_qt.h"
#include "dev_tools_http_handler_delegate_qt.h"
#include "gl_context_qt.h"
#include "media_capture_devices_dispatcher.h"
#include "type_conversion.h"
#include "surface_factory_qt.h"
#include "web_engine_library_info.h"
#include <QFileInfo>
#include <QGuiApplication>
#include <QOffscreenSurface>
#ifndef QT_NO_OPENGL
# include <QOpenGLContext>
#endif
#include <QStringList>
#include <QSurfaceFormat>
#include <QVector>
#include <qpa/qplatformnativeinterface.h>

using namespace QtWebEngineCore;

#ifndef QT_NO_OPENGL
QT_BEGIN_NAMESPACE
Q_GUI_EXPORT QOpenGLContext *qt_gl_global_share_context();
QT_END_NAMESPACE
#endif

namespace {

scoped_refptr<QtWebEngineCore::WebEngineContext> sContext;
static bool s_destroyed = false;

void destroyContext()
{
    // Destroy WebEngineContext before its static pointer is zeroed and destructor called.
    // Before destroying MessageLoop via destroying BrowserMainRunner destructor
    // WebEngineContext's pointer is used.
    sContext->destroy();
    sContext = 0;
    s_destroyed = true;
}

#ifndef QT_NO_OPENGL
bool usingANGLE()
{
#if defined(Q_OS_WIN)
    if (qt_gl_global_share_context())
        return qt_gl_global_share_context()->isOpenGLES();
    return QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES;
#else
    return false;
#endif
}

bool usingSoftwareDynamicGL()
{
#if defined(Q_OS_WIN)
    HMODULE handle = static_cast<HMODULE>(QOpenGLContext::openGLModuleHandle());
    wchar_t path[MAX_PATH];
    DWORD size = GetModuleFileName(handle, path, MAX_PATH);
    QFileInfo openGLModule(QString::fromWCharArray(path, size));
    return openGLModule.fileName() == QLatin1String("opengl32sw.dll");
#else
    return false;
#endif
}

bool usingQtQuick2DRenderer()
{
    const QStringList args = QGuiApplication::arguments();
    QString device;
    for (int index = 0; index < args.count(); ++index) {
        if (args.at(index).startsWith(QLatin1String("--device="))) {
            device = args.at(index).mid(9);
            break;
        }
    }

    if (device.isEmpty())
        device = QString::fromLocal8Bit(qgetenv("QT_QUICK_BACKEND"));
    if (device.isEmpty())
        device = QString::fromLocal8Bit(qgetenv("QMLSCENE_DEVICE"));
    if (device.isEmpty())
        device = QLatin1String("default");

    // Anything other than the default OpenGL device will need to render in 2D mode.
    return device != QLatin1String("default");
}
#endif //QT_NO_OPENGL
#if defined(ENABLE_PLUGINS)
void dummyGetPluginCallback(const std::vector<content::WebPluginInfo>&)
{
}
#endif

} // namespace

namespace QtWebEngineCore {

void WebEngineContext::destroyBrowserContext()
{
    m_defaultBrowserContext.reset();
}

void WebEngineContext::destroy()
{
    delete m_globalQObject;
    m_globalQObject = 0;
    base::MessagePump::Delegate *delegate = m_runLoop->loop_;
    // Flush the UI message loop before quitting.
    while (delegate->DoWork()) { }
    GLContextHelper::destroy();
    m_devtools.reset(0);
    m_runLoop->AfterRun();

    // Force to destroy RenderProcessHostImpl by destroying BrowserMainRunner.
    // RenderProcessHostImpl should be destroyed before WebEngineContext since
    // default BrowserContext might be used by the RenderprocessHostImpl's destructor.
    m_browserRunner.reset(0);

    // Drop the false reference.
    sContext->Release();
}

WebEngineContext::~WebEngineContext()
{
    // WebEngineContext::destroy() must be called before we are deleted
    Q_ASSERT(!m_globalQObject);
    Q_ASSERT(!m_devtools);
    Q_ASSERT(!m_browserRunner);
}

scoped_refptr<WebEngineContext> WebEngineContext::current()
{
    if (s_destroyed)
        return nullptr;
    if (!sContext.get()) {
        sContext = new WebEngineContext();
        // Make sure that we ramp down Chromium before QApplication destroys its X connection, etc.
        qAddPostRoutine(destroyContext);
        // Add a false reference so there is no race between unreferencing sContext and a global QApplication.
        sContext->AddRef();
    }
    return sContext;
}

QSharedPointer<BrowserContextAdapter> WebEngineContext::defaultBrowserContext()
{
    if (!m_defaultBrowserContext)
        m_defaultBrowserContext = QSharedPointer<BrowserContextAdapter>::create(QStringLiteral("Default"));
    return m_defaultBrowserContext;
}

QObject *WebEngineContext::globalQObject()
{
    return m_globalQObject;
}

#ifndef CHROMIUM_VERSION
#error Chromium version should be defined at gyp-time. Something must have gone wrong
#define CHROMIUM_VERSION // This is solely to keep Qt Creator happy.
#endif

const static char kChromiumFlagsEnv[] = "QTWEBENGINE_CHROMIUM_FLAGS";
const static char kDisableSandboxEnv[] = "QTWEBENGINE_DISABLE_SANDBOX";

WebEngineContext::WebEngineContext()
    : m_mainDelegate(new ContentMainDelegateQt)
    , m_contentRunner(content::ContentMainRunner::Create())
    , m_browserRunner(content::BrowserMainRunner::Create())
    , m_globalQObject(new QObject())
{

#ifdef Q_OS_LINUX
    // Call qputenv before BrowserMainRunnerImpl::Initialize is called.
    // http://crbug.com/245466
    qputenv("force_s3tc_enable", "true");
#endif

    // Allow us to inject javascript like any webview toolkit.
    content::RenderFrameHost::AllowInjectingJavaScriptForAndroidWebView();

    base::CommandLine::CreateEmpty();
    base::CommandLine* parsedCommandLine = base::CommandLine::ForCurrentProcess();
    QStringList appArgs = QCoreApplication::arguments();
    if (qEnvironmentVariableIsSet(kChromiumFlagsEnv)) {
        appArgs = appArgs.mid(0, 1); // Take application name and drop the rest
        appArgs.append(QString::fromLocal8Bit(qgetenv(kChromiumFlagsEnv)).split(' '));
    }

    bool useEmbeddedSwitches = false;
#if defined(QTWEBENGINE_EMBEDDED_SWITCHES)
    useEmbeddedSwitches = !appArgs.removeAll(QStringLiteral("--disable-embedded-switches"));
#else
    useEmbeddedSwitches = appArgs.removeAll(QStringLiteral("--enable-embedded-switches"));
#endif
    base::CommandLine::StringVector argv;
    argv.resize(appArgs.size());
#if defined(Q_OS_WIN)
    for (int i = 0; i < appArgs.size(); ++i)
        argv[i] = toString16(appArgs[i]);
#else
    for (int i = 0; i < appArgs.size(); ++i)
        argv[i] = appArgs[i].toStdString();
#endif
    parsedCommandLine->InitFromArgv(argv);

    parsedCommandLine->AppendSwitchPath(switches::kBrowserSubprocessPath, WebEngineLibraryInfo::getPath(content::CHILD_PROCESS_EXE));

    // Enable sandboxing on OS X and Linux (Desktop / Embedded) by default.
    bool disable_sandbox = qEnvironmentVariableIsSet(kDisableSandboxEnv);
    if (!disable_sandbox) {
#if defined(Q_OS_WIN)
        parsedCommandLine->AppendSwitch(switches::kNoSandbox);
#elif defined(Q_OS_LINUX)
        parsedCommandLine->AppendSwitch(switches::kDisableSetuidSandbox);
#endif
    } else {
        parsedCommandLine->AppendSwitch(switches::kNoSandbox);
        qInfo() << "Sandboxing disabled by user.";
    }

    parsedCommandLine->AppendSwitch(switches::kEnableThreadedCompositing);
    parsedCommandLine->AppendSwitch(switches::kInProcessGPU);
    // These are currently only default on OS X, and we don't support them:
    parsedCommandLine->AppendSwitch(switches::kDisableZeroCopy);
    parsedCommandLine->AppendSwitch(switches::kDisableGpuMemoryBufferCompositorResources);

    // Enabled on OS X and Linux but currently not working. It worked in 5.7 on OS X.
    parsedCommandLine->AppendSwitch(switches::kDisableGpuMemoryBufferVideoFrames);

    if (useEmbeddedSwitches) {
        // Inspired by the Android port's default switches
        if (!parsedCommandLine->HasSwitch(switches::kDisableOverlayScrollbar))
            parsedCommandLine->AppendSwitch(switches::kEnableOverlayScrollbar);
        if (!parsedCommandLine->HasSwitch(switches::kDisablePinch))
            parsedCommandLine->AppendSwitch(switches::kEnablePinch);
        parsedCommandLine->AppendSwitch(switches::kEnableViewport);
        parsedCommandLine->AppendSwitch(switches::kMainFrameResizesAreOrientationChanges);
        parsedCommandLine->AppendSwitch(switches::kDisableAcceleratedVideoDecode);
        parsedCommandLine->AppendSwitch(switches::kDisableGpuShaderDiskCache);
        parsedCommandLine->AppendSwitch(switches::kDisable2dCanvasAntialiasing);
        parsedCommandLine->AppendSwitch(cc::switches::kDisableCompositedAntialiasing);
        parsedCommandLine->AppendSwitchASCII(switches::kProfilerTiming, switches::kProfilerTimingDisabledValue);
    }

    GLContextHelper::initialize();

    const char *glType = 0;
#ifndef QT_NO_OPENGL
    if (!usingANGLE() && !usingSoftwareDynamicGL() && !usingQtQuick2DRenderer()) {
        if (qt_gl_global_share_context() && qt_gl_global_share_context()->isValid()) {
            // If the native handle is QEGLNativeContext try to use GL ES/2, if there is no native handle
            // assume we are using wayland and try GL ES/2, and finally Ozone demands GL ES/2 too.
            if (qt_gl_global_share_context()->nativeHandle().isNull()
#ifdef USE_OZONE
                || true
#endif
                || !strcmp(qt_gl_global_share_context()->nativeHandle().typeName(), "QEGLNativeContext"))
            {
                if (qt_gl_global_share_context()->isOpenGLES()) {
                    glType = gl::kGLImplementationEGLName;
                } else {
                    QOpenGLContext context;
                    QSurfaceFormat format;

                    format.setRenderableType(QSurfaceFormat::OpenGLES);
                    format.setVersion(2, 0);

                    context.setFormat(format);
                    context.setShareContext(qt_gl_global_share_context());
                    if (context.create()) {
                        QOffscreenSurface surface;

                        surface.setFormat(format);
                        surface.create();

                        if (context.makeCurrent(&surface)) {
                            if (context.hasExtension("GL_ARB_ES2_compatibility"))
                                glType = gl::kGLImplementationEGLName;

                            context.doneCurrent();
                        }

                        surface.destroy();
                    }
                }
            } else {
                if (!qt_gl_global_share_context()->isOpenGLES())
                    glType = gl::kGLImplementationDesktopName;
            }
        } else {
            qWarning("WebEngineContext used before QtWebEngine::initialize() or OpenGL context creation failed.");
        }
    }
#endif

    if (glType)
        parsedCommandLine->AppendSwitchASCII(switches::kUseGL, glType);
    else
        parsedCommandLine->AppendSwitch(switches::kDisableGpu);

    content::UtilityProcessHostImpl::RegisterUtilityMainThreadFactory(content::CreateInProcessUtilityThread);
    content::RenderProcessHostImpl::RegisterRendererMainThreadFactory(content::CreateInProcessRendererThread);
    content::GpuProcessHost::RegisterGpuMainThreadFactory(content::CreateInProcessGpuThread);

    content::ContentMainParams contentMainParams(m_mainDelegate.get());
    contentMainParams.setup_signal_handlers = false;
#if defined(OS_WIN)
    sandbox::SandboxInterfaceInfo sandbox_info = {0};
    content::InitializeSandboxInfo(&sandbox_info);
    contentMainParams.sandbox_info = &sandbox_info;
#endif
    m_contentRunner->Initialize(contentMainParams);
    m_browserRunner->Initialize(content::MainFunctionParams(*base::CommandLine::ForCurrentProcess()));

    // Once the MessageLoop has been created, attach a top-level RunLoop.
    m_runLoop.reset(new base::RunLoop);
    m_runLoop->BeforeRun();

    m_devtools = createDevToolsHttpHandler();
    // Force the initialization of MediaCaptureDevicesDispatcher on the UI
    // thread to avoid a thread check assertion in its constructor when it
    // first gets referenced on the IO thread.
    MediaCaptureDevicesDispatcher::GetInstance();

    base::ThreadRestrictions::SetIOAllowed(true);

#if defined(ENABLE_PLUGINS)
    // Creating pepper plugins from the page (which calls PluginService::GetPluginInfoArray)
    // might fail unless the page queried the list of available plugins at least once
    // (which ends up calling PluginService::GetPlugins). Since the plugins list can only
    // be created from the FILE thread, and that GetPluginInfoArray is synchronous, it
    // can't loads plugins synchronously from the IO thread to serve the render process' request
    // and we need to make sure that it happened beforehand.
    content::PluginService::GetInstance()->GetPlugins(base::Bind(&dummyGetPluginCallback));
#endif

#if defined(ENABLE_BASIC_PRINTING)
    m_printJobManager.reset(new printing::PrintJobManager());
#endif // defined(ENABLE_BASIC_PRINTING)
}

#if defined(ENABLE_BASIC_PRINTING)
printing::PrintJobManager* WebEngineContext::getPrintJobManager()
{
    return m_printJobManager.get();
}
#endif // defined(ENABLE_BASIC_PRINTING)
} // namespace
