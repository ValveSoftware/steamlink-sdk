// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/download_controller_android_impl.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "content/browser/android/content_view_core_impl.h"
#include "content/browser/download/download_item_impl.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/common/referrer.h"
#include "jni/DownloadController_jni.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_content_disposition.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace content {

// JNI methods
static void Init(JNIEnv* env, jobject obj) {
  DownloadControllerAndroidImpl::GetInstance()->Init(env, obj);
}

struct DownloadControllerAndroidImpl::JavaObject {
  ScopedJavaLocalRef<jobject> Controller(JNIEnv* env) {
    return GetRealObject(env, obj);
  }
  jweak obj;
};

// static
bool DownloadControllerAndroidImpl::RegisterDownloadController(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
DownloadControllerAndroid* DownloadControllerAndroid::Get() {
  return DownloadControllerAndroidImpl::GetInstance();
}

// static
DownloadControllerAndroidImpl* DownloadControllerAndroidImpl::GetInstance() {
  return Singleton<DownloadControllerAndroidImpl>::get();
}

DownloadControllerAndroidImpl::DownloadControllerAndroidImpl()
    : java_object_(NULL) {
}

DownloadControllerAndroidImpl::~DownloadControllerAndroidImpl() {
  if (java_object_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    env->DeleteWeakGlobalRef(java_object_->obj);
    delete java_object_;
    base::android::CheckException(env);
  }
}

// Initialize references to Java object.
void DownloadControllerAndroidImpl::Init(JNIEnv* env, jobject obj) {
  java_object_ = new JavaObject;
  java_object_->obj = env->NewWeakGlobalRef(obj);
}

void DownloadControllerAndroidImpl::CreateGETDownload(
    int render_process_id, int render_view_id, int request_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  GlobalRequestID global_id(render_process_id, request_id);

  // We are yielding the UI thread and render_view_host may go away by
  // the time we come back. Pass along render_process_id and render_view_id
  // to retrieve it later (if it still exists).
  GetDownloadInfoCB cb = base::Bind(
        &DownloadControllerAndroidImpl::StartAndroidDownload,
        base::Unretained(this), render_process_id,
        render_view_id);

  PrepareDownloadInfo(
      global_id,
      base::Bind(&DownloadControllerAndroidImpl::StartDownloadOnUIThread,
                 base::Unretained(this), cb));
}

void DownloadControllerAndroidImpl::PrepareDownloadInfo(
    const GlobalRequestID& global_id,
    const GetDownloadInfoCB& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  net::URLRequest* request =
      ResourceDispatcherHostImpl::Get()->GetURLRequest(global_id);
  if (!request) {
    LOG(ERROR) << "Request to download not found.";
    return;
  }

  DownloadInfoAndroid info_android(request);

  net::CookieStore* cookie_store = request->context()->cookie_store();
  if (cookie_store) {
    net::CookieMonster* cookie_monster = cookie_store->GetCookieMonster();
    if (cookie_monster) {
      cookie_monster->GetAllCookiesForURLAsync(
          request->url(),
          base::Bind(&DownloadControllerAndroidImpl::CheckPolicyAndLoadCookies,
                     base::Unretained(this), info_android, callback,
                     global_id));
    } else {
      DoLoadCookies(info_android, callback, global_id);
    }
  } else {
    // Can't get any cookies, start android download.
    callback.Run(info_android);
  }
}

void DownloadControllerAndroidImpl::CheckPolicyAndLoadCookies(
    const DownloadInfoAndroid& info, const GetDownloadInfoCB& callback,
    const GlobalRequestID& global_id, const net::CookieList& cookie_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  net::URLRequest* request =
      ResourceDispatcherHostImpl::Get()->GetURLRequest(global_id);
  if (!request) {
    LOG(ERROR) << "Request to download not found.";
    return;
  }

  if (request->context()->network_delegate()->CanGetCookies(
      *request, cookie_list)) {
    DoLoadCookies(info, callback, global_id);
  } else {
    callback.Run(info);
  }
}

void DownloadControllerAndroidImpl::DoLoadCookies(
    const DownloadInfoAndroid& info, const GetDownloadInfoCB& callback,
    const GlobalRequestID& global_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  net::CookieOptions options;
  options.set_include_httponly();

  net::URLRequest* request =
      ResourceDispatcherHostImpl::Get()->GetURLRequest(global_id);
  if (!request) {
    LOG(ERROR) << "Request to download not found.";
    return;
  }

  request->context()->cookie_store()->GetCookiesWithOptionsAsync(
      info.url, options,
      base::Bind(&DownloadControllerAndroidImpl::OnCookieResponse,
                 base::Unretained(this), info, callback));
}

void DownloadControllerAndroidImpl::OnCookieResponse(
    DownloadInfoAndroid download_info,
    const GetDownloadInfoCB& callback,
    const std::string& cookie) {
  download_info.cookie = cookie;

  // We have everything we need, start Android download.
  callback.Run(download_info);
}

void DownloadControllerAndroidImpl::StartDownloadOnUIThread(
    const GetDownloadInfoCB& callback,
    const DownloadInfoAndroid& info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, base::Bind(callback, info));
}

void DownloadControllerAndroidImpl::StartAndroidDownload(
    int render_process_id, int render_view_id,
    const DownloadInfoAndroid& info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JNIEnv* env = base::android::AttachCurrentThread();

  // Call newHttpGetDownload
  ScopedJavaLocalRef<jobject> view = GetContentView(render_process_id,
                                                    render_view_id);
  if (view.is_null()) {
    // The view went away. Can't proceed.
    LOG(ERROR) << "Download failed on URL:" << info.url.spec();
    return;
  }

  ScopedJavaLocalRef<jstring> jurl =
      ConvertUTF8ToJavaString(env, info.url.spec());
  ScopedJavaLocalRef<jstring> juser_agent =
      ConvertUTF8ToJavaString(env, info.user_agent);
  ScopedJavaLocalRef<jstring> jcontent_disposition =
      ConvertUTF8ToJavaString(env, info.content_disposition);
  ScopedJavaLocalRef<jstring> jmime_type =
      ConvertUTF8ToJavaString(env, info.original_mime_type);
  ScopedJavaLocalRef<jstring> jcookie =
      ConvertUTF8ToJavaString(env, info.cookie);
  ScopedJavaLocalRef<jstring> jreferer =
      ConvertUTF8ToJavaString(env, info.referer);

  // Try parsing the content disposition header to get a
  // explicitly specified filename if available.
  net::HttpContentDisposition header(info.content_disposition, "");
  ScopedJavaLocalRef<jstring> jfilename =
      ConvertUTF8ToJavaString(env, header.filename());

  Java_DownloadController_newHttpGetDownload(
      env, GetJavaObject()->Controller(env).obj(), view.obj(), jurl.obj(),
      juser_agent.obj(), jcontent_disposition.obj(), jmime_type.obj(),
      jcookie.obj(), jreferer.obj(), jfilename.obj(), info.total_bytes);
}

void DownloadControllerAndroidImpl::OnDownloadStarted(
    DownloadItem* download_item) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!download_item->GetWebContents())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();

  // Register for updates to the DownloadItem.
  download_item->AddObserver(this);

  ScopedJavaLocalRef<jobject> view =
      GetContentViewCoreFromWebContents(download_item->GetWebContents());
  // The view went away. Can't proceed.
  if (view.is_null())
    return;

  ScopedJavaLocalRef<jstring> jmime_type =
      ConvertUTF8ToJavaString(env, download_item->GetMimeType());
  ScopedJavaLocalRef<jstring> jfilename = ConvertUTF8ToJavaString(
      env, download_item->GetTargetFilePath().BaseName().value());
  Java_DownloadController_onDownloadStarted(
      env, GetJavaObject()->Controller(env).obj(), view.obj(), jfilename.obj(),
      jmime_type.obj());
}

void DownloadControllerAndroidImpl::OnDownloadUpdated(DownloadItem* item) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (item->IsDangerous() && (item->GetState() != DownloadItem::CANCELLED))
    OnDangerousDownload(item);

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jurl =
      ConvertUTF8ToJavaString(env, item->GetURL().spec());
  ScopedJavaLocalRef<jstring> jmime_type =
      ConvertUTF8ToJavaString(env, item->GetMimeType());
  ScopedJavaLocalRef<jstring> jpath =
      ConvertUTF8ToJavaString(env, item->GetTargetFilePath().value());
  ScopedJavaLocalRef<jstring> jfilename = ConvertUTF8ToJavaString(
      env, item->GetTargetFilePath().BaseName().value());

  switch (item->GetState()) {
    case DownloadItem::IN_PROGRESS: {
      base::TimeDelta time_delta;
      item->TimeRemaining(&time_delta);
      Java_DownloadController_onDownloadUpdated(
          env, GetJavaObject()->Controller(env).obj(),
          base::android::GetApplicationContext(), jurl.obj(), jmime_type.obj(),
          jfilename.obj(), jpath.obj(), item->GetReceivedBytes(), true,
          item->GetId(), item->PercentComplete(), time_delta.InMilliseconds());
      break;
    }
    case DownloadItem::COMPLETE:
      // Multiple OnDownloadUpdated() notifications may be issued while the
      // download is in the COMPLETE state. Only handle one.
      item->RemoveObserver(this);

      // Call onDownloadCompleted
      Java_DownloadController_onDownloadCompleted(
          env, GetJavaObject()->Controller(env).obj(),
          base::android::GetApplicationContext(), jurl.obj(), jmime_type.obj(),
          jfilename.obj(), jpath.obj(), item->GetReceivedBytes(), true,
          item->GetId());
      break;
    case DownloadItem::CANCELLED:
    // TODO(shashishekhar): An interrupted download can be resumed. Android
    // currently does not support resumable downloads. Add handling for
    // interrupted case based on item->CanResume().
    case DownloadItem::INTERRUPTED:
      // Call onDownloadCompleted with success = false.
      Java_DownloadController_onDownloadCompleted(
          env, GetJavaObject()->Controller(env).obj(),
          base::android::GetApplicationContext(), jurl.obj(), jmime_type.obj(),
          jfilename.obj(), jpath.obj(), item->GetReceivedBytes(), false,
          item->GetId());
      break;
    case DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED();
  }
}

void DownloadControllerAndroidImpl::OnDangerousDownload(DownloadItem* item) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jfilename = ConvertUTF8ToJavaString(
      env, item->GetTargetFilePath().BaseName().value());
  ScopedJavaLocalRef<jobject> view_core = GetContentViewCoreFromWebContents(
      item->GetWebContents());
  if (!view_core.is_null()) {
    Java_DownloadController_onDangerousDownload(
        env, GetJavaObject()->Controller(env).obj(), view_core.obj(),
        jfilename.obj(), item->GetId());
  }
}

ScopedJavaLocalRef<jobject> DownloadControllerAndroidImpl::GetContentView(
    int render_process_id, int render_view_id) {
  RenderViewHost* render_view_host =
      RenderViewHost::FromID(render_process_id, render_view_id);

  if (!render_view_host)
    return ScopedJavaLocalRef<jobject>();

  WebContents* web_contents =
      render_view_host->GetDelegate()->GetAsWebContents();

  return GetContentViewCoreFromWebContents(web_contents);
}

ScopedJavaLocalRef<jobject>
    DownloadControllerAndroidImpl::GetContentViewCoreFromWebContents(
    WebContents* web_contents) {
  if (!web_contents)
    return ScopedJavaLocalRef<jobject>();

  ContentViewCore* view_core = ContentViewCore::FromWebContents(web_contents);
  return view_core ? view_core->GetJavaObject() :
      ScopedJavaLocalRef<jobject>();
}

DownloadControllerAndroidImpl::JavaObject*
    DownloadControllerAndroidImpl::GetJavaObject() {
  if (!java_object_) {
    // Initialize Java DownloadController by calling
    // DownloadController.getInstance(), which will call Init()
    // if Java DownloadController is not instantiated already.
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_DownloadController_getInstance(env);
  }

  DCHECK(java_object_);
  return java_object_;
}

void DownloadControllerAndroidImpl::StartContextMenuDownload(
    const ContextMenuParams& params, WebContents* web_contents, bool is_link) {
  const GURL& url = is_link ? params.link_url : params.src_url;
  const GURL& referrer = params.frame_url.is_empty() ?
      params.page_url : params.frame_url;
  DownloadManagerImpl* dlm = static_cast<DownloadManagerImpl*>(
      BrowserContext::GetDownloadManager(web_contents->GetBrowserContext()));
  scoped_ptr<DownloadUrlParameters> dl_params(
      DownloadUrlParameters::FromWebContents(web_contents, url));
  dl_params->set_referrer(
      Referrer(referrer, params.referrer_policy));
  if (is_link)
    dl_params->set_referrer_encoding(params.frame_charset);
  else
    dl_params->set_prefer_cache(true);
  dl_params->set_prompt(false);
  dlm->DownloadUrl(dl_params.Pass());
}

void DownloadControllerAndroidImpl::DangerousDownloadValidated(
    WebContents* web_contents, int download_id, bool accept) {
  if (!web_contents)
    return;
  DownloadManagerImpl* dlm = static_cast<DownloadManagerImpl*>(
      BrowserContext::GetDownloadManager(web_contents->GetBrowserContext()));
  DownloadItem* item = dlm->GetDownload(download_id);
  if (!item)
    return;
  if (accept)
    item->ValidateDangerousDownload();
  else
    item->Remove();
}

DownloadControllerAndroidImpl::DownloadInfoAndroid::DownloadInfoAndroid(
    net::URLRequest* request) {
  request->GetResponseHeaderByName("content-disposition", &content_disposition);

  if (request->response_headers())
    request->response_headers()->GetMimeType(&original_mime_type);

  request->extra_request_headers().GetHeader(
      net::HttpRequestHeaders::kUserAgent, &user_agent);
  GURL referer_url(request->referrer());
  if (referer_url.is_valid())
    referer = referer_url.spec();
  if (!request->url_chain().empty()) {
    original_url = request->url_chain().front();
    url = request->url_chain().back();
  }
}

DownloadControllerAndroidImpl::DownloadInfoAndroid::~DownloadInfoAndroid() {}

}  // namespace content
