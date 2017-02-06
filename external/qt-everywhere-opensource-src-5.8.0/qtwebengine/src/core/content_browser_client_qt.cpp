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

#include "content_browser_client_qt.h"

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_restrictions.h"
#if defined(ENABLE_SPELLCHECK)
#include "chrome/browser/spellchecker/spellcheck_message_filter.h"
#if defined(USE_BROWSER_SPELLCHECKER)
#include "chrome/browser/spellchecker/spellcheck_message_filter_platform.h"
#endif
#endif
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/geolocation_delegate.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/media_observer.h"
#include "content/public/browser/quota_permission_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/url_constants.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/screen.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gpu_timing.h"

#include "access_token_store_qt.h"
#include "browser_context_adapter.h"
#include "browser_context_qt.h"
#include "browser_message_filter_qt.h"
#include "certificate_error_controller.h"
#include "certificate_error_controller_p.h"
#include "desktop_screen_qt.h"
#include "dev_tools_http_handler_delegate_qt.h"
#ifdef QT_USE_POSITIONING
#include "location_provider_qt.h"
#endif
#include "media_capture_devices_dispatcher.h"
#if defined(ENABLE_BASIC_PRINTING)
#include "printing_message_filter_qt.h"
#endif // defined(ENABLE_BASIC_PRINTING)
#include "qrc_protocol_handler_qt.h"
#include "renderer_host/resource_dispatcher_host_delegate_qt.h"
#include "renderer_host/user_resource_controller_host.h"
#include "web_contents_delegate_qt.h"
#include "web_engine_context.h"
#include "web_engine_library_info.h"

#if defined(Q_OS_LINUX)
#include "global_descriptors_qt.h"
#include "ui/base/resource/resource_bundle.h"
#endif

#if defined(ENABLE_PLUGINS)
#include "content/public/browser/browser_ppapi_host.h"
#include "ppapi/host/ppapi_host.h"
#include "renderer_host/pepper/pepper_host_factory_qt.h"
#endif

#include <QGuiApplication>
#include <QLocale>
#ifndef QT_NO_OPENGL
# include <QOpenGLContext>
#endif
#include <qpa/qplatformnativeinterface.h>

QT_BEGIN_NAMESPACE
Q_GUI_EXPORT QOpenGLContext *qt_gl_global_share_context();
QT_END_NAMESPACE

namespace QtWebEngineCore {

namespace {

ContentBrowserClientQt* gBrowserClient = 0; // Owned by ContentMainDelegateQt.

// Return a timeout suitable for the glib loop, -1 to block forever,
// 0 to return right away, or a timeout in milliseconds from now.
int GetTimeIntervalMilliseconds(const base::TimeTicks& from) {
  if (from.is_null())
    return -1;

  // Be careful here.  TimeDelta has a precision of microseconds, but we want a
  // value in milliseconds.  If there are 5.5ms left, should the delay be 5 or
  // 6?  It should be 6 to avoid executing delayed work too early.
  int delay = static_cast<int>(
      ceil((from - base::TimeTicks::Now()).InMillisecondsF()));

  // If this value is negative, then we need to run delayed work soon.
  return delay < 0 ? 0 : delay;
}

class MessagePumpForUIQt : public QObject,
                           public base::MessagePump
{
public:
    MessagePumpForUIQt()
        // Usually this gets passed through Run, but since we have
        // our own event loop, attach it explicitly ourselves.
        : m_delegate(base::MessageLoopForUI::current())
        , m_explicitLoop(0)
        , m_timerId(0)
    {
    }

    virtual void Run(Delegate *delegate) Q_DECL_OVERRIDE
    {
        Q_ASSERT(delegate == m_delegate);
        // This is used only when MessagePumpForUIQt is used outside of the GUI thread.
        QEventLoop loop;
        m_explicitLoop = &loop;
        loop.exec();
        m_explicitLoop = 0;
    }

    virtual void Quit() Q_DECL_OVERRIDE
    {
        Q_ASSERT(m_explicitLoop);
        m_explicitLoop->quit();
    }

    virtual void ScheduleWork() Q_DECL_OVERRIDE
    {
        QCoreApplication::postEvent(this, new QEvent(QEvent::User));
    }

    virtual void ScheduleDelayedWork(const base::TimeTicks &delayed_work_time) Q_DECL_OVERRIDE
    {
        if (delayed_work_time.is_null()) {
            killTimer(m_timerId);
            m_timerId = 0;
            m_timerScheduledTime = base::TimeTicks();
        } else if (!m_timerId || delayed_work_time < m_timerScheduledTime) {
            killTimer(m_timerId);
            m_timerId = startTimer(GetTimeIntervalMilliseconds(delayed_work_time));
            m_timerScheduledTime = delayed_work_time;
        }
    }

protected:
    virtual void customEvent(QEvent *ev) Q_DECL_OVERRIDE
    {
        if (handleScheduledWork())
            QCoreApplication::postEvent(this, new QEvent(QEvent::User));
    }

    virtual void timerEvent(QTimerEvent *ev) Q_DECL_OVERRIDE
    {
        Q_ASSERT(m_timerId == ev->timerId());
        killTimer(m_timerId);
        m_timerId = 0;
        m_timerScheduledTime = base::TimeTicks();

        base::TimeTicks next_delayed_work_time;
        m_delegate->DoDelayedWork(&next_delayed_work_time);
        ScheduleDelayedWork(next_delayed_work_time);
    }

private:
    bool handleScheduledWork() {
        bool more_work_is_plausible = m_delegate->DoWork();

        base::TimeTicks delayed_work_time;
        more_work_is_plausible |= m_delegate->DoDelayedWork(&delayed_work_time);

        if (more_work_is_plausible)
            return true;

        more_work_is_plausible |= m_delegate->DoIdleWork();
        if (!more_work_is_plausible)
            ScheduleDelayedWork(delayed_work_time);

        return more_work_is_plausible;
    }

    Delegate *m_delegate;
    QEventLoop *m_explicitLoop;
    int m_timerId;
    base::TimeTicks m_timerScheduledTime;
};

std::unique_ptr<base::MessagePump> messagePumpFactory()
{
    return base::WrapUnique(new MessagePumpForUIQt);
}

} // namespace

class BrowserMainPartsQt : public content::BrowserMainParts
{
public:
    BrowserMainPartsQt()
        : content::BrowserMainParts()
    { }

    void PreMainMessageLoopStart() Q_DECL_OVERRIDE
    {
        base::MessageLoop::InitMessagePumpForUIFactory(messagePumpFactory);
    }

    void PreMainMessageLoopRun() Q_DECL_OVERRIDE
    {
    }

    void PostMainMessageLoopRun()
    {
        // The BrowserContext's destructor uses the MessageLoop so it should be deleted
        // right before the RenderProcessHostImpl's destructor destroys it.
        WebEngineContext::current()->destroyBrowserContext();
    }

    int PreCreateThreads() Q_DECL_OVERRIDE
    {
        base::ThreadRestrictions::SetIOAllowed(true);
        // Like ChromeBrowserMainExtraPartsViews::PreCreateThreads does.
        display::Screen::SetScreenInstance(new DesktopScreenQt);

        return 0;
    }

private:
    DISALLOW_COPY_AND_ASSIGN(BrowserMainPartsQt);
};

class QtShareGLContext : public gl::GLContext {
public:
    QtShareGLContext(QOpenGLContext *qtContext)
        : gl::GLContext(0)
        , m_handle(0)
    {
        QString platform = qApp->platformName().toLower();
        QPlatformNativeInterface *pni = QGuiApplication::platformNativeInterface();
        if (platform == QLatin1String("xcb")) {
            if (gl::GetGLImplementation() == gl::kGLImplementationEGLGLES2)
                m_handle = pni->nativeResourceForContext(QByteArrayLiteral("eglcontext"), qtContext);
            else
                m_handle = pni->nativeResourceForContext(QByteArrayLiteral("glxcontext"), qtContext);
        } else if (platform == QLatin1String("cocoa"))
            m_handle = pni->nativeResourceForContext(QByteArrayLiteral("cglcontextobj"), qtContext);
        else if (platform == QLatin1String("qnx"))
            m_handle = pni->nativeResourceForContext(QByteArrayLiteral("eglcontext"), qtContext);
        else if (platform == QLatin1String("eglfs") || platform == QLatin1String("wayland")
                 || platform == QLatin1String("wayland-egl"))
            m_handle = pni->nativeResourceForContext(QByteArrayLiteral("eglcontext"), qtContext);
        else if (platform == QLatin1String("windows")) {
            if (gl::GetGLImplementation() == gl::kGLImplementationEGLGLES2)
                m_handle = pni->nativeResourceForContext(QByteArrayLiteral("eglContext"), qtContext);
            else
                m_handle = pni->nativeResourceForContext(QByteArrayLiteral("renderingcontext"), qtContext);
        } else {
            qFatal("%s platform not yet supported", platform.toLatin1().constData());
            // Add missing platforms once they work.
            Q_UNREACHABLE();
        }
    }

    virtual void* GetHandle() Q_DECL_OVERRIDE { return m_handle; }
    // Qt currently never creates contexts using robustness attributes.
    virtual bool WasAllocatedUsingRobustnessExtension() { return false; }

    // We don't care about the rest, this context shouldn't be used except for its handle.
    virtual bool Initialize(gl::GLSurface *, gl::GpuPreference) Q_DECL_OVERRIDE { Q_UNREACHABLE(); return false; }
    virtual bool MakeCurrent(gl::GLSurface *) Q_DECL_OVERRIDE { Q_UNREACHABLE(); return false; }
    virtual void ReleaseCurrent(gl::GLSurface *) Q_DECL_OVERRIDE { Q_UNREACHABLE(); }
    virtual bool IsCurrent(gl::GLSurface *) Q_DECL_OVERRIDE { Q_UNREACHABLE(); return false; }
    virtual void OnSetSwapInterval(int) Q_DECL_OVERRIDE { Q_UNREACHABLE(); }
    virtual scoped_refptr<gl::GPUTimingClient> CreateGPUTimingClient() Q_DECL_OVERRIDE
    {
        return nullptr;
    }

private:
    void *m_handle;
};

class ShareGroupQtQuick : public gl::GLShareGroup {
public:
    virtual gl::GLContext* GetContext() Q_DECL_OVERRIDE { return m_shareContextQtQuick.get(); }
    virtual void AboutToAddFirstContext() Q_DECL_OVERRIDE;

private:
    scoped_refptr<QtShareGLContext> m_shareContextQtQuick;
};

void ShareGroupQtQuick::AboutToAddFirstContext()
{
#ifndef QT_NO_OPENGL
    // This currently has to be setup by ::main in all applications using QQuickWebEngineView with delegated rendering.
    QOpenGLContext *shareContext = qt_gl_global_share_context();
    if (!shareContext) {
        qFatal("QWebEngine: OpenGL resource sharing is not set up in QtQuick. Please make sure to call QtWebEngine::initialize() in your main() function.");
    }
    m_shareContextQtQuick = make_scoped_refptr(new QtShareGLContext(shareContext));
#endif
}

class QuotaPermissionContextQt : public content::QuotaPermissionContext {
public:
    virtual void RequestQuotaPermission(const content::StorageQuotaParams &params, int render_process_id, const PermissionCallback &callback) Q_DECL_OVERRIDE
    {
        Q_UNUSED(params);
        Q_UNUSED(render_process_id);
        callback.Run(QUOTA_PERMISSION_RESPONSE_DISALLOW);
    }
};

ContentBrowserClientQt::ContentBrowserClientQt()
    : m_browserMainParts(0)
{
    Q_ASSERT(!gBrowserClient);
    gBrowserClient = this;
}

ContentBrowserClientQt::~ContentBrowserClientQt()
{
    gBrowserClient = 0;
}

ContentBrowserClientQt *ContentBrowserClientQt::Get()
{
    return gBrowserClient;
}

content::BrowserMainParts *ContentBrowserClientQt::CreateBrowserMainParts(const content::MainFunctionParams&)
{
    m_browserMainParts = new BrowserMainPartsQt();
    return m_browserMainParts;
}

void ContentBrowserClientQt::RenderProcessWillLaunch(content::RenderProcessHost* host)
{
    // FIXME: Add a settings variable to enable/disable the file scheme.
    const int id = host->GetID();
    content::ChildProcessSecurityPolicy::GetInstance()->GrantScheme(id, url::kFileScheme);
    static_cast<BrowserContextQt*>(host->GetBrowserContext())->m_adapter->userResourceController()->renderProcessStartedWithHost(host);
#if defined(ENABLE_PEPPER_CDMS)
    host->AddFilter(new BrowserMessageFilterQt(id));
#endif
#if defined(ENABLE_SPELLCHECK)
    // SpellCheckMessageFilter is required for both Hunspell and Native configurations.
    host->AddFilter(new SpellCheckMessageFilter(id));
#endif
#if defined(Q_OS_MACOS) && defined(USE_BROWSER_SPELLCHECKER)
  host->AddFilter(new SpellCheckMessageFilterPlatform(id));
#endif
#if defined(ENABLE_BASIC_PRINTING)
    host->AddFilter(new PrintingMessageFilterQt(host->GetID()));
#endif // defined(ENABLE_BASIC_PRINTING)
}

void ContentBrowserClientQt::ResourceDispatcherHostCreated()
{
    m_resourceDispatcherHostDelegate.reset(new ResourceDispatcherHostDelegateQt);
    content::ResourceDispatcherHost::Get()->SetDelegate(m_resourceDispatcherHostDelegate.get());
}

gl::GLShareGroup *ContentBrowserClientQt::GetInProcessGpuShareGroup()
{
    if (!m_shareGroupQtQuick.get())
        m_shareGroupQtQuick = new ShareGroupQtQuick;
    return m_shareGroupQtQuick.get();
}

content::MediaObserver *ContentBrowserClientQt::GetMediaObserver()
{
    return MediaCaptureDevicesDispatcher::GetInstance();
}

void ContentBrowserClientQt::OverrideWebkitPrefs(content::RenderViewHost *rvh, content::WebPreferences *web_prefs)
{
    if (content::WebContents *webContents = rvh->GetDelegate()->GetAsWebContents())
        static_cast<WebContentsDelegateQt*>(webContents->GetDelegate())->overrideWebPreferences(webContents, web_prefs);
}

namespace {

// A provider of services needed by Geolocation.
class GeolocationDelegateQt : public content::GeolocationDelegate {
public:
    GeolocationDelegateQt() {}
    content::AccessTokenStore* CreateAccessTokenStore() final
    {
        return new AccessTokenStoreQt;
    }

    content::LocationProvider* OverrideSystemLocationProvider() final
    {
#ifdef QT_USE_POSITIONING
        if (!m_location_provider)
            m_location_provider = base::WrapUnique(new LocationProviderQt);
        return m_location_provider.get();
#else
        return nullptr;
#endif
    }

private:
#ifdef QT_USE_POSITIONING
    std::unique_ptr<LocationProviderQt> m_location_provider;
#endif

    DISALLOW_COPY_AND_ASSIGN(GeolocationDelegateQt);
};

}  // anonymous namespace

content::GeolocationDelegate *ContentBrowserClientQt::CreateGeolocationDelegate()
{
    return new GeolocationDelegateQt;
}

content::QuotaPermissionContext *ContentBrowserClientQt::CreateQuotaPermissionContext()
{
    return new QuotaPermissionContextQt;
}


void ContentBrowserClientQt::AllowCertificateError(content::WebContents *webContents,
                                   int cert_error,
                                   const net::SSLInfo& ssl_info,
                                   const GURL& request_url,
                                   content::ResourceType resource_type,
                                   bool overridable,
                                   bool strict_enforcement,
                                   bool expired_previous_decision,
                                   const base::Callback<void(bool)>& callback,
                                   content::CertificateRequestResultType* result)
{
    WebContentsDelegateQt* contentsDelegate = static_cast<WebContentsDelegateQt*>(webContents->GetDelegate());

    QSharedPointer<CertificateErrorController> errorController(new CertificateErrorController(new CertificateErrorControllerPrivate(cert_error, ssl_info, request_url, resource_type, overridable, strict_enforcement, callback)));
    contentsDelegate->allowCertificateError(errorController);

    // If we don't give the user a chance to allow it, we can reject it right away.
    if (result && (!overridable || strict_enforcement))
        *result = content::CERTIFICATE_REQUEST_RESULT_TYPE_DENY;
}

void ContentBrowserClientQt::SelectClientCertificate(content::WebContents * /*webContents*/,
                                                     net::SSLCertRequestInfo * /*certRequestInfo*/,
                                                     std::unique_ptr<content::ClientCertificateDelegate> delegate)
{
    delegate->ContinueWithCertificate(nullptr);
}

std::string ContentBrowserClientQt::GetApplicationLocale()
{
    return WebEngineLibraryInfo::getApplicationLocale();
}

std::string ContentBrowserClientQt::GetAcceptLangs(content::BrowserContext *context)
{
    return static_cast<BrowserContextQt*>(context)->adapter()->httpAcceptLanguage().toStdString();
}

void ContentBrowserClientQt::AppendExtraCommandLineSwitches(base::CommandLine* command_line, int child_process_id)
{
    Q_UNUSED(child_process_id);

    std::string processType = command_line->GetSwitchValueASCII(switches::kProcessType);
    if (processType == switches::kZygoteProcess)
        command_line->AppendSwitchASCII(switches::kLang, GetApplicationLocale());
}

void ContentBrowserClientQt::GetAdditionalWebUISchemes(std::vector<std::string>* additional_schemes)
{
    additional_schemes->push_back(kQrcSchemeQt);
}

#if defined(Q_OS_LINUX)
void ContentBrowserClientQt::GetAdditionalMappedFilesForChildProcess(const base::CommandLine& command_line, int child_process_id, content::FileDescriptorInfo* mappings)
{
    const std::string &locale = GetApplicationLocale();
    const base::FilePath &locale_file_path = ui::ResourceBundle::GetSharedInstance().GetLocaleFilePath(locale, true);
    if (locale_file_path.empty())
        return;

    // Open pak file of the current locale in the Browser process and pass its file descriptor to the sandboxed
    // Renderer Process. FileDescriptorInfo is responsible for closing the file descriptor.
    int flags = base::File::FLAG_OPEN | base::File::FLAG_READ;
    base::File locale_file = base::File(locale_file_path, flags);
    mappings->Transfer(kWebEngineLocale, base::ScopedFD(locale_file.TakePlatformFile()));
}
#endif

#if defined(ENABLE_PLUGINS)
void ContentBrowserClientQt::DidCreatePpapiPlugin(content::BrowserPpapiHost* browser_host)
{
    browser_host->GetPpapiHost()->AddHostFactoryFilter(
                base::WrapUnique(new QtWebEngineCore::PepperHostFactoryQt(browser_host)));
}
#endif

content::DevToolsManagerDelegate* ContentBrowserClientQt::GetDevToolsManagerDelegate()
{
    return new DevToolsManagerDelegateQt;
}

} // namespace QtWebEngineCore
