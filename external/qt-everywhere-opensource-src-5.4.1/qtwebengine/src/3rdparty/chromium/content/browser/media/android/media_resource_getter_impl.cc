// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/android/media_resource_getter_impl.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/path_service.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/fileapi/browser_file_system_helper.h"
#include "content/browser/fileapi/chrome_blob_storage_context.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "jni/MediaResourceGetter_jni.h"
#include "media/base/android/media_url_interceptor.h"
#include "net/base/auth.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"
#include "webkit/browser/blob/blob_data_handle.h"
#include "webkit/browser/blob/blob_storage_context.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace content {

static void ReturnResultOnUIThread(
    const base::Callback<void(const std::string&)>& callback,
    const std::string& result) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, base::Bind(callback, result));
}

static void RequestPlatformPathFromBlobURL(
    const GURL& url,
    BrowserContext* browser_context,
    const base::Callback<void(const std::string&)>& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ChromeBlobStorageContext* context =
      ChromeBlobStorageContext::GetFor(browser_context);
  scoped_ptr<webkit_blob::BlobDataHandle> handle =
      context->context()->GetBlobDataFromPublicURL(url);
  const std::vector<webkit_blob::BlobData::Item> items =
      handle->data()->items();

  // TODO(qinmin): handle the case when the blob data is not a single file.
  DLOG_IF(WARNING, items.size() != 1u)
      << "More than one blob data are present: " << items.size();
  ReturnResultOnUIThread(callback, items[0].path().value());
}

static void RequestPlaformPathFromFileSystemURL(
    const GURL& url,
    int render_process_id,
    scoped_refptr<fileapi::FileSystemContext> file_system_context,
    const base::Callback<void(const std::string&)>& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::FilePath platform_path;
  SyncGetPlatformPath(file_system_context.get(),
                      render_process_id,
                      url,
                      &platform_path);
  base::FilePath data_storage_path;
  PathService::Get(base::DIR_ANDROID_APP_DATA, &data_storage_path);
  if (data_storage_path.IsParent(platform_path))
    ReturnResultOnUIThread(callback, platform_path.value());
  else
    ReturnResultOnUIThread(callback, std::string());
}

// Posts a task to the UI thread to run the callback function.
static void PostMediaMetadataCallbackTask(
    const media::MediaResourceGetter::ExtractMediaMetadataCB& callback,
    JNIEnv* env, ScopedJavaLocalRef<jobject>& j_metadata) {
  BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(callback, base::TimeDelta::FromMilliseconds(
                       Java_MediaMetadata_getDurationInMilliseconds(
                           env, j_metadata.obj())),
                   Java_MediaMetadata_getWidth(env, j_metadata.obj()),
                   Java_MediaMetadata_getHeight(env, j_metadata.obj()),
                   Java_MediaMetadata_isSuccess(env, j_metadata.obj())));
}

// Gets the metadata from a media URL. When finished, a task is posted to the UI
// thread to run the callback function.
static void GetMediaMetadata(
    const std::string& url, const std::string& cookies,
    const std::string& user_agent,
    const media::MediaResourceGetter::ExtractMediaMetadataCB& callback) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jstring> j_url_string = ConvertUTF8ToJavaString(env, url);
  ScopedJavaLocalRef<jstring> j_cookies = ConvertUTF8ToJavaString(env, cookies);
  jobject j_context = base::android::GetApplicationContext();
  ScopedJavaLocalRef<jstring> j_user_agent = ConvertUTF8ToJavaString(
      env, user_agent);
  ScopedJavaLocalRef<jobject> j_metadata =
      Java_MediaResourceGetter_extractMediaMetadata(env,
                                                    j_context,
                                                    j_url_string.obj(),
                                                    j_cookies.obj(),
                                                    j_user_agent.obj());

  PostMediaMetadataCallbackTask(callback, env, j_metadata);
}

// Gets the metadata from a file descriptor. When finished, a task is posted to
// the UI thread to run the callback function.
static void GetMediaMetadataFromFd(
    const int fd, const int64 offset, const int64 size,
    const media::MediaResourceGetter::ExtractMediaMetadataCB& callback) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jobject> j_metadata =
      Java_MediaResourceGetter_extractMediaMetadataFromFd(
          env, fd, offset, size);

  PostMediaMetadataCallbackTask(callback, env, j_metadata);
}

// The task object that retrieves cookie on the IO thread.
// TODO(qinmin): refactor this class to make the code reusable by others as
// there are lots of duplicated functionalities elsewhere.
class CookieGetterTask
     : public base::RefCountedThreadSafe<CookieGetterTask> {
 public:
  CookieGetterTask(BrowserContext* browser_context,
                   int render_process_id, int render_frame_id);

  // Called by CookieGetterImpl to start getting cookies for a URL.
  void RequestCookies(
      const GURL& url, const GURL& first_party_for_cookies,
      const media::MediaResourceGetter::GetCookieCB& callback);

 private:
  friend class base::RefCountedThreadSafe<CookieGetterTask>;
  virtual ~CookieGetterTask();

  void CheckPolicyForCookies(
      const GURL& url, const GURL& first_party_for_cookies,
      const media::MediaResourceGetter::GetCookieCB& callback,
      const net::CookieList& cookie_list);

  // Context getter used to get the CookieStore.
  net::URLRequestContextGetter* context_getter_;

  // Resource context for checking cookie policies.
  ResourceContext* resource_context_;

  // Render process id, used to check whether the process can access cookies.
  int render_process_id_;

  // Render frame id, used to check tab specific cookie policy.
  int render_frame_id_;

  DISALLOW_COPY_AND_ASSIGN(CookieGetterTask);
};

CookieGetterTask::CookieGetterTask(
    BrowserContext* browser_context, int render_process_id, int render_frame_id)
    : context_getter_(browser_context->GetRequestContext()),
      resource_context_(browser_context->GetResourceContext()),
      render_process_id_(render_process_id),
      render_frame_id_(render_frame_id) {
}

CookieGetterTask::~CookieGetterTask() {}

void CookieGetterTask::RequestCookies(
    const GURL& url, const GURL& first_party_for_cookies,
    const media::MediaResourceGetter::GetCookieCB& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanAccessCookiesForOrigin(render_process_id_, url)) {
    callback.Run(std::string());
    return;
  }

  net::CookieStore* cookie_store =
      context_getter_->GetURLRequestContext()->cookie_store();
  if (!cookie_store) {
    callback.Run(std::string());
    return;
  }

  net::CookieMonster* cookie_monster = cookie_store->GetCookieMonster();
  if (cookie_monster) {
    cookie_monster->GetAllCookiesForURLAsync(url, base::Bind(
        &CookieGetterTask::CheckPolicyForCookies, this,
        url, first_party_for_cookies, callback));
  } else {
    callback.Run(std::string());
  }
}

void CookieGetterTask::CheckPolicyForCookies(
    const GURL& url, const GURL& first_party_for_cookies,
    const media::MediaResourceGetter::GetCookieCB& callback,
    const net::CookieList& cookie_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (GetContentClient()->browser()->AllowGetCookie(
      url, first_party_for_cookies, cookie_list,
      resource_context_, render_process_id_, render_frame_id_)) {
    net::CookieStore* cookie_store =
        context_getter_->GetURLRequestContext()->cookie_store();
    cookie_store->GetCookiesWithOptionsAsync(
        url, net::CookieOptions(), callback);
  } else {
    callback.Run(std::string());
  }
}

MediaResourceGetterImpl::MediaResourceGetterImpl(
    BrowserContext* browser_context,
    fileapi::FileSystemContext* file_system_context,
    int render_process_id,
    int render_frame_id)
    : browser_context_(browser_context),
      file_system_context_(file_system_context),
      render_process_id_(render_process_id),
      render_frame_id_(render_frame_id),
      weak_factory_(this) {}

MediaResourceGetterImpl::~MediaResourceGetterImpl() {}

void MediaResourceGetterImpl::GetCookies(
    const GURL& url, const GURL& first_party_for_cookies,
    const GetCookieCB& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_refptr<CookieGetterTask> task = new CookieGetterTask(
      browser_context_, render_process_id_, render_frame_id_);

  GetCookieCB cb = base::Bind(&MediaResourceGetterImpl::GetCookiesCallback,
                              weak_factory_.GetWeakPtr(),
                              callback);
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&CookieGetterTask::RequestCookies,
                 task, url, first_party_for_cookies,
                 base::Bind(&ReturnResultOnUIThread, cb)));
}

void MediaResourceGetterImpl::GetCookiesCallback(
    const GetCookieCB& callback, const std::string& cookies) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  callback.Run(cookies);
}

void MediaResourceGetterImpl::GetPlatformPathFromURL(
    const GURL& url, const GetPlatformPathCB& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(url.SchemeIsFileSystem() || url.SchemeIs(url::kBlobScheme));

  GetPlatformPathCB cb =
      base::Bind(&MediaResourceGetterImpl::GetPlatformPathCallback,
                 weak_factory_.GetWeakPtr(),
                 callback);

  if (url.SchemeIs(url::kBlobScheme)) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&RequestPlatformPathFromBlobURL, url, browser_context_, cb));
    return;
  }

  scoped_refptr<fileapi::FileSystemContext> context(file_system_context_);
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&RequestPlaformPathFromFileSystemURL, url, render_process_id_,
                 context, cb));
}

void MediaResourceGetterImpl::GetPlatformPathCallback(
    const GetPlatformPathCB& callback, const std::string& platform_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  callback.Run(platform_path);
}

void MediaResourceGetterImpl::ExtractMediaMetadata(
    const std::string& url, const std::string& cookies,
    const std::string& user_agent, const ExtractMediaMetadataCB& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::SequencedWorkerPool* pool = content::BrowserThread::GetBlockingPool();
  pool->PostWorkerTask(
      FROM_HERE,
      base::Bind(&GetMediaMetadata, url, cookies, user_agent, callback));
}

void MediaResourceGetterImpl::ExtractMediaMetadata(
    const int fd, const int64 offset, const int64 size,
    const ExtractMediaMetadataCB& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::SequencedWorkerPool* pool = content::BrowserThread::GetBlockingPool();
  pool->PostWorkerTask(
      FROM_HERE,
      base::Bind(&GetMediaMetadataFromFd, fd, offset, size, callback));
}

// static
bool MediaResourceGetterImpl::RegisterMediaResourceGetter(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
