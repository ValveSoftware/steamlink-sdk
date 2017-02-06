// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_drm_bridge.h"

#include <stddef.h>
#include <algorithm>
#include <utility>

#include "base/android/build_info.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/sys_byteorder.h"
#include "base/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "jni/MediaDrmBridge_jni.h"
#include "media/base/android/media_client_android.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/android/media_drm_bridge_delegate.h"
#include "media/base/android/provision_fetcher.h"
#include "media/base/cdm_key_information.h"
#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertJavaStringToUTF8;
using base::android::JavaByteArrayToByteVector;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace media {

namespace {

// These must be in sync with Android MediaDrm REQUEST_TYPE_XXX constants!
// https://developer.android.com/reference/android/media/MediaDrm.KeyRequest.html
enum class RequestType {
  REQUEST_TYPE_INITIAL = 0,
  REQUEST_TYPE_RENEWAL = 1,
  REQUEST_TYPE_RELEASE = 2,
};

// These must be in sync with Android MediaDrm KEY_STATUS_XXX constants:
// https://developer.android.com/reference/android/media/MediaDrm.KeyStatus.html
enum class KeyStatus {
  KEY_STATUS_USABLE = 0,
  KEY_STATUS_EXPIRED = 1,
  KEY_STATUS_OUTPUT_NOT_ALLOWED = 2,
  KEY_STATUS_PENDING = 3,
  KEY_STATUS_INTERNAL_ERROR = 4,
};

// Converts jbyteArray (byte[] in Java) into std::string.
std::string AsString(JNIEnv* env, jbyteArray j_byte_array) {
  std::vector<uint8_t> byte_vector;
  JavaByteArrayToByteVector(env, j_byte_array, &byte_vector);
  return std::string(byte_vector.begin(), byte_vector.end());
}

const uint8_t kWidevineUuid[16] = {
    0xED, 0xEF, 0x8B, 0xA9, 0x79, 0xD6, 0x4A, 0xCE,  //
    0xA3, 0xC8, 0x27, 0xDC, 0xD5, 0x1D, 0x21, 0xED};

// Convert |init_data_type| to a string supported by MediaDRM.
// "audio"/"video" does not matter, so use "video".
std::string ConvertInitDataType(media::EmeInitDataType init_data_type) {
  // TODO(jrummell/xhwang): EME init data types like "webm" and "cenc" are
  // supported in API level >=21 for Widevine key system. Switch to use those
  // strings when they are officially supported in Android for all key systems.
  switch (init_data_type) {
    case media::EmeInitDataType::WEBM:
      return "video/webm";
    case media::EmeInitDataType::CENC:
      return "video/mp4";
    case media::EmeInitDataType::KEYIDS:
      return "keyids";
    default:
      NOTREACHED();
      return "unknown";
  }
}

MediaKeys::MessageType GetMessageType(RequestType request_type) {
  switch (request_type) {
    case RequestType::REQUEST_TYPE_INITIAL:
      return MediaKeys::LICENSE_REQUEST;
    case RequestType::REQUEST_TYPE_RENEWAL:
      return MediaKeys::LICENSE_RENEWAL;
    case RequestType::REQUEST_TYPE_RELEASE:
      return MediaKeys::LICENSE_RELEASE;
  }

  NOTREACHED();
  return MediaKeys::LICENSE_REQUEST;
}

CdmKeyInformation::KeyStatus ConvertKeyStatus(KeyStatus key_status) {
  switch (key_status) {
    case KeyStatus::KEY_STATUS_USABLE:
      return CdmKeyInformation::USABLE;
    case KeyStatus::KEY_STATUS_EXPIRED:
      return CdmKeyInformation::EXPIRED;
    case KeyStatus::KEY_STATUS_OUTPUT_NOT_ALLOWED:
      return CdmKeyInformation::OUTPUT_RESTRICTED;
    case KeyStatus::KEY_STATUS_PENDING:
      // TODO(xhwang): This should probably be renamed to "PENDING".
      return CdmKeyInformation::KEY_STATUS_PENDING;
    case KeyStatus::KEY_STATUS_INTERNAL_ERROR:
      return CdmKeyInformation::INTERNAL_ERROR;
  }

  NOTREACHED();
  return CdmKeyInformation::INTERNAL_ERROR;
}

class KeySystemManager {
 public:
  KeySystemManager();
  UUID GetUUID(const std::string& key_system);
  std::vector<std::string> GetPlatformKeySystemNames();

 private:
  using KeySystemUuidMap = MediaClientAndroid::KeySystemUuidMap;

  KeySystemUuidMap key_system_uuid_map_;

  DISALLOW_COPY_AND_ASSIGN(KeySystemManager);
};

KeySystemManager::KeySystemManager() {
  // Widevine is always supported in Android.
  key_system_uuid_map_[kWidevineKeySystem] =
      UUID(kWidevineUuid, kWidevineUuid + arraysize(kWidevineUuid));
  MediaClientAndroid* client = GetMediaClientAndroid();
  if (client)
    client->AddKeySystemUUIDMappings(&key_system_uuid_map_);
}

UUID KeySystemManager::GetUUID(const std::string& key_system) {
  KeySystemUuidMap::iterator it = key_system_uuid_map_.find(key_system);
  if (it == key_system_uuid_map_.end())
    return UUID();
  return it->second;
}

std::vector<std::string> KeySystemManager::GetPlatformKeySystemNames() {
  std::vector<std::string> key_systems;
  for (KeySystemUuidMap::iterator it = key_system_uuid_map_.begin();
       it != key_system_uuid_map_.end(); ++it) {
    // Rule out the key system handled by Chrome explicitly.
    if (it->first != kWidevineKeySystem)
      key_systems.push_back(it->first);
  }
  return key_systems;
}

base::LazyInstance<KeySystemManager>::Leaky g_key_system_manager =
    LAZY_INSTANCE_INITIALIZER;

// Checks whether |key_system| is supported with |container_mime_type|. Only
// checks |key_system| support if |container_mime_type| is empty.
// TODO(xhwang): The |container_mime_type| is not the same as contentType in
// the EME spec. Revisit this once the spec issue with initData type is
// resolved.
bool IsKeySystemSupportedWithTypeImpl(const std::string& key_system,
                                      const std::string& container_mime_type) {
  DCHECK(MediaDrmBridge::IsAvailable());

  if (key_system.empty()) {
    NOTREACHED();
    return false;
  }

  UUID scheme_uuid = g_key_system_manager.Get().GetUUID(key_system);
  if (scheme_uuid.empty())
    return false;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_scheme_uuid =
      base::android::ToJavaByteArray(env, &scheme_uuid[0], scheme_uuid.size());
  ScopedJavaLocalRef<jstring> j_container_mime_type =
      ConvertUTF8ToJavaString(env, container_mime_type);
  return Java_MediaDrmBridge_isCryptoSchemeSupported(
      env, j_scheme_uuid.obj(), j_container_mime_type.obj());
}

MediaDrmBridge::SecurityLevel GetSecurityLevelFromString(
    const std::string& security_level_str) {
  if (0 == security_level_str.compare("L1"))
    return MediaDrmBridge::SECURITY_LEVEL_1;
  if (0 == security_level_str.compare("L3"))
    return MediaDrmBridge::SECURITY_LEVEL_3;
  DCHECK(security_level_str.empty());
  return MediaDrmBridge::SECURITY_LEVEL_DEFAULT;
}

// Do not change the return values as they are part of Android MediaDrm API for
// Widevine.
std::string GetSecurityLevelString(
    MediaDrmBridge::SecurityLevel security_level) {
  switch (security_level) {
    case MediaDrmBridge::SECURITY_LEVEL_DEFAULT:
      return "";
    case MediaDrmBridge::SECURITY_LEVEL_1:
      return "L1";
    case MediaDrmBridge::SECURITY_LEVEL_3:
      return "L3";
  }
  return "";
}

bool AreMediaDrmApisAvailable() {
  if (base::android::BuildInfo::GetInstance()->sdk_int() < 19)
    return false;

  int32_t os_major_version = 0;
  int32_t os_minor_version = 0;
  int32_t os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(
      &os_major_version, &os_minor_version, &os_bugfix_version);
  if (os_major_version == 4 && os_minor_version == 4 && os_bugfix_version == 0)
    return false;

  return true;
}

}  // namespace

// MediaDrm is not generally usable without MediaCodec. Thus, both the MediaDrm
// APIs and MediaCodec APIs must be enabled and not blacklisted.
// static
bool MediaDrmBridge::IsAvailable() {
  return AreMediaDrmApisAvailable() && MediaCodecUtil::IsMediaCodecAvailable();
}

// static
bool MediaDrmBridge::RegisterMediaDrmBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
bool MediaDrmBridge::IsKeySystemSupported(const std::string& key_system) {
  if (!MediaDrmBridge::IsAvailable())
    return false;

  return IsKeySystemSupportedWithTypeImpl(key_system, "");
}

// static
bool MediaDrmBridge::IsKeySystemSupportedWithType(
    const std::string& key_system,
    const std::string& container_mime_type) {
  DCHECK(!container_mime_type.empty()) << "Call IsKeySystemSupported instead";

  if (!MediaDrmBridge::IsAvailable())
    return false;

  return IsKeySystemSupportedWithTypeImpl(key_system, container_mime_type);
}

// static
std::vector<std::string> MediaDrmBridge::GetPlatformKeySystemNames() {
  if (!MediaDrmBridge::IsAvailable())
    return std::vector<std::string>();

  return g_key_system_manager.Get().GetPlatformKeySystemNames();
}

// static
scoped_refptr<MediaDrmBridge> MediaDrmBridge::CreateInternal(
    const std::string& key_system,
    SecurityLevel security_level,
    const CreateFetcherCB& create_fetcher_cb,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const LegacySessionErrorCB& legacy_session_error_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb) {
  // All paths requires the MediaDrmApis.
  DCHECK(AreMediaDrmApisAvailable());

  UUID scheme_uuid = g_key_system_manager.Get().GetUUID(key_system);
  if (scheme_uuid.empty())
    return nullptr;

  scoped_refptr<MediaDrmBridge> media_drm_bridge(new MediaDrmBridge(
      scheme_uuid, security_level, create_fetcher_cb, session_message_cb,
      session_closed_cb, legacy_session_error_cb, session_keys_change_cb,
      session_expiration_update_cb));

  if (media_drm_bridge->j_media_drm_.is_null())
    media_drm_bridge = nullptr;

  return media_drm_bridge;
}

// static
scoped_refptr<MediaDrmBridge> MediaDrmBridge::Create(
    const std::string& key_system,
    SecurityLevel security_level,
    const CreateFetcherCB& create_fetcher_cb,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const LegacySessionErrorCB& legacy_session_error_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb) {
  DVLOG(1) << __FUNCTION__;

  if (!IsAvailable())
    return nullptr;

  return CreateInternal(key_system, security_level, create_fetcher_cb,
                        session_message_cb, session_closed_cb,
                        legacy_session_error_cb, session_keys_change_cb,
                        session_expiration_update_cb);
}

// static
scoped_refptr<MediaDrmBridge> MediaDrmBridge::CreateWithoutSessionSupport(
    const std::string& key_system,
    SecurityLevel security_level,
    const CreateFetcherCB& create_fetcher_cb) {
  DVLOG(1) << __FUNCTION__;

  // Sessions won't be used so decoding capability is not required.
  if (!AreMediaDrmApisAvailable())
    return nullptr;

  return MediaDrmBridge::Create(key_system, security_level, create_fetcher_cb,
                                SessionMessageCB(), SessionClosedCB(),
                                LegacySessionErrorCB(), SessionKeysChangeCB(),
                                SessionExpirationUpdateCB());
}

void MediaDrmBridge::SetServerCertificate(
    const std::vector<uint8_t>& certificate,
    std::unique_ptr<media::SimpleCdmPromise> promise) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(2) << __FUNCTION__ << "(" << certificate.size() << " bytes)";

  DCHECK(!certificate.empty());

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_certificate = base::android::ToJavaByteArray(
      env, certificate.data(), certificate.size());
  if (Java_MediaDrmBridge_setServerCertificate(env, j_media_drm_.obj(),
                                               j_certificate.obj())) {
    promise->resolve();
  } else {
    promise->reject(INVALID_ACCESS_ERROR, 0, "Set server certificate failed.");
  }
}

void MediaDrmBridge::CreateSessionAndGenerateRequest(
    SessionType session_type,
    media::EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data,
    std::unique_ptr<media::NewSessionCdmPromise> promise) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(2) << __FUNCTION__;

  if (session_type != media::MediaKeys::TEMPORARY_SESSION) {
    NOTIMPLEMENTED() << "EME persistent sessions not yet supported on Android.";
    promise->reject(NOT_SUPPORTED_ERROR, 0,
                    "Only the temporary session type is supported.");
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_init_data;
  ScopedJavaLocalRef<jobjectArray> j_optional_parameters;

  MediaClientAndroid* client = GetMediaClientAndroid();
  if (client) {
    MediaDrmBridgeDelegate* delegate =
        client->GetMediaDrmBridgeDelegate(scheme_uuid_);
    if (delegate) {
      std::vector<uint8_t> init_data_from_delegate;
      std::vector<std::string> optional_parameters_from_delegate;
      if (!delegate->OnCreateSession(init_data_type, init_data,
                                     &init_data_from_delegate,
                                     &optional_parameters_from_delegate)) {
        promise->reject(INVALID_ACCESS_ERROR, 0, "Invalid init data.");
        return;
      }
      if (!init_data_from_delegate.empty()) {
        j_init_data =
            base::android::ToJavaByteArray(env, init_data_from_delegate.data(),
                                           init_data_from_delegate.size());
      }
      if (!optional_parameters_from_delegate.empty()) {
        j_optional_parameters = base::android::ToJavaArrayOfStrings(
            env, optional_parameters_from_delegate);
      }
    }
  }

  if (j_init_data.is_null()) {
    j_init_data =
        base::android::ToJavaByteArray(env, init_data.data(), init_data.size());
  }

  ScopedJavaLocalRef<jstring> j_mime =
      ConvertUTF8ToJavaString(env, ConvertInitDataType(init_data_type));
  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  Java_MediaDrmBridge_createSessionFromNative(
      env, j_media_drm_.obj(), j_init_data.obj(), j_mime.obj(),
      j_optional_parameters.obj(), promise_id);
}

void MediaDrmBridge::LoadSession(
    SessionType session_type,
    const std::string& session_id,
    std::unique_ptr<media::NewSessionCdmPromise> promise) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(2) << __FUNCTION__;

  NOTIMPLEMENTED() << "EME persistent sessions not yet supported on Android.";
  promise->reject(NOT_SUPPORTED_ERROR, 0, "LoadSession() is not supported.");
}

void MediaDrmBridge::UpdateSession(
    const std::string& session_id,
    const std::vector<uint8_t>& response,
    std::unique_ptr<media::SimpleCdmPromise> promise) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(2) << __FUNCTION__;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_response =
      base::android::ToJavaByteArray(env, response.data(), response.size());
  ScopedJavaLocalRef<jbyteArray> j_session_id = base::android::ToJavaByteArray(
      env, reinterpret_cast<const uint8_t*>(session_id.data()),
      session_id.size());
  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  Java_MediaDrmBridge_updateSession(env, j_media_drm_.obj(), j_session_id.obj(),
                                    j_response.obj(), promise_id);
}

void MediaDrmBridge::CloseSession(
    const std::string& session_id,
    std::unique_ptr<media::SimpleCdmPromise> promise) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(2) << __FUNCTION__;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_session_id = base::android::ToJavaByteArray(
      env, reinterpret_cast<const uint8_t*>(session_id.data()),
      session_id.size());
  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  Java_MediaDrmBridge_closeSession(env, j_media_drm_.obj(), j_session_id.obj(),
                                   promise_id);
}

void MediaDrmBridge::RemoveSession(
    const std::string& session_id,
    std::unique_ptr<media::SimpleCdmPromise> promise) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(2) << __FUNCTION__;

  NOTIMPLEMENTED() << "EME persistent sessions not yet supported on Android.";
  promise->reject(NOT_SUPPORTED_ERROR, 0, "RemoveSession() is not supported.");
}

CdmContext* MediaDrmBridge::GetCdmContext() {
  DVLOG(2) << __FUNCTION__;

  return &media_drm_bridge_cdm_context_;
}

void MediaDrmBridge::DeleteOnCorrectThread() const {
  DVLOG(1) << __FUNCTION__;

  if (!task_runner_->BelongsToCurrentThread()) {
    // When DeleteSoon returns false, |this| will be leaked, which is okay.
    task_runner_->DeleteSoon(FROM_HERE, this);
  } else {
    delete this;
  }
}

int MediaDrmBridge::RegisterPlayer(const base::Closure& new_key_cb,
                                   const base::Closure& cdm_unset_cb) {
  // |player_tracker_| can be accessed from any thread.
  return player_tracker_.RegisterPlayer(new_key_cb, cdm_unset_cb);
}

void MediaDrmBridge::UnregisterPlayer(int registration_id) {
  // |player_tracker_| can be accessed from any thread.
  player_tracker_.UnregisterPlayer(registration_id);
}

bool MediaDrmBridge::IsProtectedSurfaceRequired() {
  // For Widevine, this depends on the security level.
  if (std::equal(scheme_uuid_.begin(), scheme_uuid_.end(), kWidevineUuid))
    return IsSecureDecoderRequired(GetSecurityLevel());

  // For other key systems, assume true.
  return true;
}

void MediaDrmBridge::ResetDeviceCredentials(
    const ResetCredentialsCB& callback) {
  DVLOG(1) << __FUNCTION__;

  DCHECK(reset_credentials_cb_.is_null());
  reset_credentials_cb_ = callback;
  JNIEnv* env = AttachCurrentThread();
  Java_MediaDrmBridge_resetDeviceCredentials(env, j_media_drm_.obj());
}

void MediaDrmBridge::ResolvePromise(uint32_t promise_id) {
  DVLOG(2) << __FUNCTION__;
  cdm_promise_adapter_.ResolvePromise(promise_id);
}

void MediaDrmBridge::ResolvePromiseWithSession(uint32_t promise_id,
                                               const std::string& session_id) {
  DVLOG(2) << __FUNCTION__;
  cdm_promise_adapter_.ResolvePromise(promise_id, session_id);
}

void MediaDrmBridge::RejectPromise(uint32_t promise_id,
                                   const std::string& error_message) {
  DVLOG(2) << __FUNCTION__;
  cdm_promise_adapter_.RejectPromise(promise_id, MediaKeys::UNKNOWN_ERROR, 0,
                                     error_message);
}

jobject MediaDrmBridge::GetMediaCrypto() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return j_media_crypto_->obj();
}

void MediaDrmBridge::SetMediaCryptoReadyCB(
    const MediaCryptoReadyCB& media_crypto_ready_cb) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&MediaDrmBridge::SetMediaCryptoReadyCB,
                   weak_factory_.GetWeakPtr(), media_crypto_ready_cb));
    return;
  }

  DVLOG(1) << __FUNCTION__;

  if (media_crypto_ready_cb.is_null()) {
    media_crypto_ready_cb_.Reset();
    return;
  }

  DCHECK(media_crypto_ready_cb_.is_null());
  media_crypto_ready_cb_ = media_crypto_ready_cb;

  if (!j_media_crypto_)
    return;

  base::ResetAndReturn(&media_crypto_ready_cb_)
      .Run(CreateJavaObjectPtr(j_media_crypto_->obj()),
           IsProtectedSurfaceRequired());
}

//------------------------------------------------------------------------------
// The following OnXxx functions are called from Java. The implementation must
// only do minimal work and then post tasks to avoid reentrancy issues.

void MediaDrmBridge::OnMediaCryptoReady(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_media_drm,
    const JavaParamRef<jobject>& j_media_crypto) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__;

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&MediaDrmBridge::NotifyMediaCryptoReady,
                 weak_factory_.GetWeakPtr(),
                 base::Passed(CreateJavaObjectPtr(j_media_crypto.obj()))));
}

void MediaDrmBridge::OnStartProvisioning(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_media_drm,
    const JavaParamRef<jstring>& j_default_url,
    const JavaParamRef<jbyteArray>& j_request_data) {
  DVLOG(1) << __FUNCTION__;

  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&MediaDrmBridge::SendProvisioningRequest,
                                    weak_factory_.GetWeakPtr(),
                                    ConvertJavaStringToUTF8(env, j_default_url),
                                    AsString(env, j_request_data)));
}

void MediaDrmBridge::OnPromiseResolved(JNIEnv* env,
                                       const JavaParamRef<jobject>& j_media_drm,
                                       jint j_promise_id) {
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&MediaDrmBridge::ResolvePromise,
                                    weak_factory_.GetWeakPtr(), j_promise_id));
}

void MediaDrmBridge::OnPromiseResolvedWithSession(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_media_drm,
    jint j_promise_id,
    const JavaParamRef<jbyteArray>& j_session_id) {
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&MediaDrmBridge::ResolvePromiseWithSession,
                                    weak_factory_.GetWeakPtr(), j_promise_id,
                                    AsString(env, j_session_id)));
}

void MediaDrmBridge::OnPromiseRejected(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_media_drm,
    jint j_promise_id,
    const JavaParamRef<jstring>& j_error_message) {
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&MediaDrmBridge::RejectPromise, weak_factory_.GetWeakPtr(),
                 j_promise_id, ConvertJavaStringToUTF8(env, j_error_message)));
}

void MediaDrmBridge::OnSessionMessage(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_media_drm,
    const JavaParamRef<jbyteArray>& j_session_id,
    jint j_message_type,
    const JavaParamRef<jbyteArray>& j_message,
    const JavaParamRef<jstring>& j_legacy_destination_url) {
  DVLOG(2) << __FUNCTION__;

  std::vector<uint8_t> message;
  JavaByteArrayToByteVector(env, j_message, &message);
  GURL legacy_destination_url =
      GURL(ConvertJavaStringToUTF8(env, j_legacy_destination_url));
  MediaKeys::MessageType message_type =
      GetMessageType(static_cast<RequestType>(j_message_type));

  task_runner_->PostTask(
      FROM_HERE, base::Bind(session_message_cb_, AsString(env, j_session_id),
                            message_type, message, legacy_destination_url));
}

void MediaDrmBridge::OnSessionClosed(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_media_drm,
    const JavaParamRef<jbyteArray>& j_session_id) {
  DVLOG(2) << __FUNCTION__;
  std::string session_id = AsString(env, j_session_id);
  task_runner_->PostTask(FROM_HERE, base::Bind(session_closed_cb_, session_id));
}

void MediaDrmBridge::OnSessionKeysChange(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_media_drm,
    const JavaParamRef<jbyteArray>& j_session_id,
    const JavaParamRef<jobjectArray>& j_keys_info,
    bool has_additional_usable_key) {
  DVLOG(2) << __FUNCTION__;

  CdmKeysInfo cdm_keys_info;

  size_t size = env->GetArrayLength(j_keys_info);
  DCHECK_GT(size, 0u);

  for (size_t i = 0; i < size; ++i) {
    ScopedJavaLocalRef<jobject> j_key_status(
        env, env->GetObjectArrayElement(j_keys_info, i));

    ScopedJavaLocalRef<jbyteArray> j_key_id =
        Java_KeyStatus_getKeyId(env, j_key_status.obj());
    std::vector<uint8_t> key_id;
    JavaByteArrayToByteVector(env, j_key_id.obj(), &key_id);
    DCHECK(!key_id.empty());

    jint j_status_code = Java_KeyStatus_getStatusCode(env, j_key_status.obj());
    CdmKeyInformation::KeyStatus key_status =
        ConvertKeyStatus(static_cast<KeyStatus>(j_status_code));

    DVLOG(2) << __FUNCTION__ << "Key status change: "
             << base::HexEncode(&key_id[0], key_id.size()) << ", "
             << key_status;

    cdm_keys_info.push_back(new CdmKeyInformation(key_id, key_status, 0));
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(session_keys_change_cb_, AsString(env, j_session_id),
                 has_additional_usable_key, base::Passed(&cdm_keys_info)));

  if (has_additional_usable_key) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&MediaDrmBridge::OnHasAdditionalUsableKey,
                                      weak_factory_.GetWeakPtr()));
  }
}

// According to MeidaDrm documentation [1], zero |expiry_time_ms| means the keys
// will never expire. This will be translated into a NULL base::Time() [2],
// which will then be mapped to a zero Java time [3]. The zero Java time is
// passed to Blink which will then be translated to NaN [4], which is what the
// spec uses to indicate that the license will never expire [5].
// [1]
// http://developer.android.com/reference/android/media/MediaDrm.OnExpirationUpdateListener.html
// [2] See base::Time::FromDoubleT()
// [3] See base::Time::ToJavaTime()
// [4] See MediaKeySession::expirationChanged()
// [5] https://github.com/w3c/encrypted-media/issues/58
void MediaDrmBridge::OnSessionExpirationUpdate(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_media_drm,
    const JavaParamRef<jbyteArray>& j_session_id,
    jlong expiry_time_ms) {
  DVLOG(2) << __FUNCTION__ << ": " << expiry_time_ms << " ms";
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(session_expiration_update_cb_, AsString(env, j_session_id),
                 base::Time::FromDoubleT(expiry_time_ms / 1000.0)));
}

void MediaDrmBridge::OnLegacySessionError(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_media_drm,
    const JavaParamRef<jbyteArray>& j_session_id,
    const JavaParamRef<jstring>& j_error_message) {
  std::string error_message = ConvertJavaStringToUTF8(env, j_error_message);

  DVLOG(2) << __FUNCTION__ << ": " << error_message;

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(legacy_session_error_cb_, AsString(env, j_session_id),
                 MediaKeys::UNKNOWN_ERROR, 0, error_message));
}

void MediaDrmBridge::OnResetDeviceCredentialsCompleted(
    JNIEnv* env,
    const JavaParamRef<jobject>&,
    bool success) {
  DVLOG(2) << __FUNCTION__ << ": success:" << success;
  DCHECK(!reset_credentials_cb_.is_null());
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(base::ResetAndReturn(&reset_credentials_cb_), success));
}

//------------------------------------------------------------------------------
// The following are private methods.

MediaDrmBridge::MediaDrmBridge(
    const std::vector<uint8_t>& scheme_uuid,
    SecurityLevel security_level,
    const CreateFetcherCB& create_fetcher_cb,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const LegacySessionErrorCB& legacy_session_error_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb)
    : scheme_uuid_(scheme_uuid),
      create_fetcher_cb_(create_fetcher_cb),
      session_message_cb_(session_message_cb),
      session_closed_cb_(session_closed_cb),
      legacy_session_error_cb_(legacy_session_error_cb),
      session_keys_change_cb_(session_keys_change_cb),
      session_expiration_update_cb_(session_expiration_update_cb),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      media_drm_bridge_cdm_context_(this),
      weak_factory_(this) {
  DVLOG(1) << __FUNCTION__;

  DCHECK(!create_fetcher_cb_.is_null());

  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  ScopedJavaLocalRef<jbyteArray> j_scheme_uuid =
      base::android::ToJavaByteArray(env, &scheme_uuid[0], scheme_uuid.size());

  std::string security_level_str = GetSecurityLevelString(security_level);
  ScopedJavaLocalRef<jstring> j_security_level =
      ConvertUTF8ToJavaString(env, security_level_str);

  // Note: OnMediaCryptoReady() could be called in this call.
  j_media_drm_.Reset(Java_MediaDrmBridge_create(
      env, j_scheme_uuid.obj(), j_security_level.obj(),
      reinterpret_cast<intptr_t>(this)));
}

MediaDrmBridge::~MediaDrmBridge() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__;

  JNIEnv* env = AttachCurrentThread();

  // After the call to Java_MediaDrmBridge_destroy() Java won't call native
  // methods anymore, this is ensured by MediaDrmBridge.java.
  if (!j_media_drm_.is_null())
    Java_MediaDrmBridge_destroy(env, j_media_drm_.obj());

  player_tracker_.NotifyCdmUnset();

  if (!media_crypto_ready_cb_.is_null()) {
    base::ResetAndReturn(&media_crypto_ready_cb_)
        .Run(CreateJavaObjectPtr(nullptr), IsProtectedSurfaceRequired());
  }

  // Rejects all pending promises.
  cdm_promise_adapter_.Clear();
}

// TODO(ddorwin): This is specific to Widevine. http://crbug.com/459400
// static
bool MediaDrmBridge::IsSecureDecoderRequired(SecurityLevel security_level) {
  DCHECK(IsAvailable());
  return SECURITY_LEVEL_1 == security_level;
}

MediaDrmBridge::SecurityLevel MediaDrmBridge::GetSecurityLevel() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_security_level =
      Java_MediaDrmBridge_getSecurityLevel(env, j_media_drm_.obj());
  std::string security_level_str =
      ConvertJavaStringToUTF8(env, j_security_level.obj());
  return GetSecurityLevelFromString(security_level_str);
}

// We have to use scoped_ptr to pass ScopedJavaGlobalRef with a callback.
// TODO(timav): Check whether we can simply pass j_media_crypto_->obj() in the
// callback.
MediaDrmBridge::JavaObjectPtr MediaDrmBridge::CreateJavaObjectPtr(
    jobject object) {
  JavaObjectPtr j_object_ptr(new ScopedJavaGlobalRef<jobject>());
  j_object_ptr->Reset(AttachCurrentThread(), object);
  return j_object_ptr;
}

void MediaDrmBridge::NotifyMediaCryptoReady(JavaObjectPtr j_media_crypto) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(j_media_crypto);
  DCHECK(!j_media_crypto_);

  j_media_crypto_ = std::move(j_media_crypto);

  if (media_crypto_ready_cb_.is_null())
    return;

  base::ResetAndReturn(&media_crypto_ready_cb_)
      .Run(CreateJavaObjectPtr(j_media_crypto_->obj()),
           IsProtectedSurfaceRequired());
}

void MediaDrmBridge::SendProvisioningRequest(const std::string& default_url,
                                             const std::string& request_data) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__;

  DCHECK(!provision_fetcher_) << "At most one provision request at any time.";
  provision_fetcher_ = create_fetcher_cb_.Run();

  provision_fetcher_->Retrieve(
      default_url, request_data,
      base::Bind(&MediaDrmBridge::ProcessProvisionResponse,
                 weak_factory_.GetWeakPtr()));
}

void MediaDrmBridge::ProcessProvisionResponse(bool success,
                                              const std::string& response) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__;

  DCHECK(provision_fetcher_) << "No provision request pending.";
  provision_fetcher_.reset();

  if (!success)
    VLOG(1) << "Device provision failure: can't get server response";

  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jbyteArray> j_response = base::android::ToJavaByteArray(
      env, reinterpret_cast<const uint8_t*>(response.data()), response.size());

  Java_MediaDrmBridge_processProvisionResponse(env, j_media_drm_.obj(), success,
                                               j_response.obj());
}

void MediaDrmBridge::OnHasAdditionalUsableKey() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__;

  player_tracker_.NotifyNewKey();
}

}  // namespace media
