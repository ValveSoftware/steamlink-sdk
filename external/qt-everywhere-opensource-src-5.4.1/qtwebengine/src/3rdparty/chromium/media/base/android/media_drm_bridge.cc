// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_drm_bridge.h"

#include <algorithm>

#include "base/android/build_info.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/callback_helpers.h"
#include "base/containers/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/string_util.h"
#include "base/sys_byteorder.h"
#include "jni/MediaDrmBridge_jni.h"

#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertJavaStringToUTF8;
using base::android::JavaByteArrayToByteVector;
using base::android::ScopedJavaLocalRef;

namespace media {

static uint32 ReadUint32(const uint8_t* data) {
  uint32 value = 0;
  for (int i = 0; i < 4; ++i)
    value = (value << 8) | data[i];
  return value;
}

static uint64 ReadUint64(const uint8_t* data) {
  uint64 value = 0;
  for (int i = 0; i < 8; ++i)
    value = (value << 8) | data[i];
  return value;
}

// The structure of an ISO CENC Protection System Specific Header (PSSH) box is
// as follows. (See ISO/IEC FDIS 23001-7:2011(E).)
// Note: ISO boxes use big-endian values.
//
// PSSH {
//   uint32 Size
//   uint32 Type
//   uint64 LargeSize  # Field is only present if value(Size) == 1.
//   uint32 VersionAndFlags
//   uint8[16] SystemId
//   uint32 DataSize
//   uint8[DataSize] Data
// }
const int kBoxHeaderSize = 8;  // Box's header contains Size and Type.
const int kBoxLargeSizeSize = 8;
const int kPsshVersionFlagSize = 4;
const int kPsshSystemIdSize = 16;
const int kPsshDataSizeSize = 4;
const uint32 kTencType = 0x74656e63;
const uint32 kPsshType = 0x70737368;
const uint8 kWidevineUuid[16] = {
    0xED, 0xEF, 0x8B, 0xA9, 0x79, 0xD6, 0x4A, 0xCE,
    0xA3, 0xC8, 0x27, 0xDC, 0xD5, 0x1D, 0x21, 0xED };

typedef std::vector<uint8> UUID;

class KeySystemUuidManager {
 public:
  KeySystemUuidManager();
  UUID GetUUID(const std::string& key_system);
  void AddMapping(const std::string& key_system, const UUID& uuid);
  std::vector<std::string> GetPlatformKeySystemNames();

 private:
  typedef base::hash_map<std::string, UUID> KeySystemUuidMap;

  KeySystemUuidMap key_system_uuid_map_;

  DISALLOW_COPY_AND_ASSIGN(KeySystemUuidManager);
};

KeySystemUuidManager::KeySystemUuidManager() {
  // Widevine is always supported in Android.
  key_system_uuid_map_[kWidevineKeySystem] =
      UUID(kWidevineUuid, kWidevineUuid + arraysize(kWidevineUuid));
}

UUID KeySystemUuidManager::GetUUID(const std::string& key_system) {
  KeySystemUuidMap::iterator it = key_system_uuid_map_.find(key_system);
  if (it == key_system_uuid_map_.end())
    return UUID();
  return it->second;
}

void KeySystemUuidManager::AddMapping(const std::string& key_system,
                                      const UUID& uuid) {
  KeySystemUuidMap::iterator it = key_system_uuid_map_.find(key_system);
  DCHECK(it == key_system_uuid_map_.end())
      << "Shouldn't overwrite an existing key system.";
  if (it != key_system_uuid_map_.end())
    return;
  key_system_uuid_map_[key_system] = uuid;
}

std::vector<std::string> KeySystemUuidManager::GetPlatformKeySystemNames() {
  std::vector<std::string> key_systems;
  for (KeySystemUuidMap::iterator it = key_system_uuid_map_.begin();
       it != key_system_uuid_map_.end(); ++it) {
    // Rule out the key system handled by Chrome explicitly.
    if (it->first != kWidevineKeySystem)
      key_systems.push_back(it->first);
  }
  return key_systems;
}

base::LazyInstance<KeySystemUuidManager>::Leaky g_key_system_uuid_manager =
    LAZY_INSTANCE_INITIALIZER;

// Tries to find a PSSH box whose "SystemId" is |uuid| in |data|, parses the
// "Data" of the box and put it in |pssh_data|. Returns true if such a box is
// found and successfully parsed. Returns false otherwise.
// Notes:
// 1, If multiple PSSH boxes are found,the "Data" of the first matching PSSH box
// will be set in |pssh_data|.
// 2, Only PSSH and TENC boxes are allowed in |data|. TENC boxes are skipped.
static bool GetPsshData(const uint8* data, int data_size,
                        const UUID& uuid,
                        std::vector<uint8>* pssh_data) {
  const uint8* cur = data;
  const uint8* data_end = data + data_size;
  int bytes_left = data_size;

  while (bytes_left > 0) {
    const uint8* box_head = cur;

    if (bytes_left < kBoxHeaderSize)
      return false;

    uint64_t box_size = ReadUint32(cur);
    uint32 type = ReadUint32(cur + 4);
    cur += kBoxHeaderSize;
    bytes_left -= kBoxHeaderSize;

    if (box_size == 1) {  // LargeSize is present.
      if (bytes_left < kBoxLargeSizeSize)
        return false;

      box_size = ReadUint64(cur);
      cur += kBoxLargeSizeSize;
      bytes_left -= kBoxLargeSizeSize;
    } else if (box_size == 0) {
      box_size = bytes_left + kBoxHeaderSize;
    }

    const uint8* box_end = box_head + box_size;
    if (data_end < box_end)
      return false;

    if (type == kTencType) {
      // Skip 'tenc' box.
      cur = box_end;
      bytes_left = data_end - cur;
      continue;
    } else if (type != kPsshType) {
      return false;
    }

    const int kPsshBoxMinimumSize =
        kPsshVersionFlagSize + kPsshSystemIdSize + kPsshDataSizeSize;
    if (box_end < cur + kPsshBoxMinimumSize)
      return false;

    uint32 version_and_flags = ReadUint32(cur);
    cur += kPsshVersionFlagSize;
    bytes_left -= kPsshVersionFlagSize;
    if (version_and_flags != 0)
      return false;

    DCHECK_GE(bytes_left, kPsshSystemIdSize);
    if (!std::equal(uuid.begin(), uuid.end(), cur)) {
      cur = box_end;
      bytes_left = data_end - cur;
      continue;
    }

    cur += kPsshSystemIdSize;
    bytes_left -= kPsshSystemIdSize;

    uint32 data_size = ReadUint32(cur);
    cur += kPsshDataSizeSize;
    bytes_left -= kPsshDataSizeSize;

    if (box_end < cur + data_size)
      return false;

    pssh_data->assign(cur, cur + data_size);
    return true;
  }

  return false;
}

static MediaDrmBridge::SecurityLevel GetSecurityLevelFromString(
    const std::string& security_level_str) {
  if (0 == security_level_str.compare("L1"))
    return MediaDrmBridge::SECURITY_LEVEL_1;
  if (0 == security_level_str.compare("L3"))
    return MediaDrmBridge::SECURITY_LEVEL_3;
  DCHECK(security_level_str.empty());
  return MediaDrmBridge::SECURITY_LEVEL_NONE;
}

static std::string GetSecurityLevelString(
    MediaDrmBridge::SecurityLevel security_level) {
  switch (security_level) {
    case MediaDrmBridge::SECURITY_LEVEL_NONE:
      return "";
    case MediaDrmBridge::SECURITY_LEVEL_1:
      return "L1";
    case MediaDrmBridge::SECURITY_LEVEL_3:
      return "L3";
  }
  return "";
}

// Checks whether |key_system| is supported with |container_mime_type|. Only
// checks |key_system| support if |container_mime_type| is empty.
// TODO(xhwang): The |container_mime_type| is not the same as contentType in
// the EME spec. Revisit this once the spec issue with initData type is
// resolved.
static bool IsKeySystemSupportedWithTypeImpl(
    const std::string& key_system,
    const std::string& container_mime_type) {
  if (!MediaDrmBridge::IsAvailable())
    return false;

  UUID scheme_uuid = g_key_system_uuid_manager.Get().GetUUID(key_system);
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

// static
bool MediaDrmBridge::IsAvailable() {
  return base::android::BuildInfo::GetInstance()->sdk_int() >= 19;
}

// static
bool MediaDrmBridge::IsSecureDecoderRequired(SecurityLevel security_level) {
  DCHECK(IsAvailable());
  return SECURITY_LEVEL_1 == security_level;
}

// static
bool MediaDrmBridge::IsSecurityLevelSupported(const std::string& key_system,
                                              SecurityLevel security_level) {
  if (!IsAvailable())
    return false;

  scoped_ptr<MediaDrmBridge> media_drm_bridge =
      MediaDrmBridge::CreateSessionless(key_system);
  if (!media_drm_bridge)
    return false;

  return media_drm_bridge->SetSecurityLevel(security_level);
}

static void AddKeySystemUuidMapping(JNIEnv* env, jclass clazz,
                                    jstring j_key_system,
                                    jobject j_buffer) {
  std::string key_system = ConvertJavaStringToUTF8(env, j_key_system);
  uint8* buffer = static_cast<uint8*>(env->GetDirectBufferAddress(j_buffer));
  UUID uuid(buffer, buffer + 16);
  g_key_system_uuid_manager.Get().AddMapping(key_system, uuid);
}

// static
std::vector<std::string> MediaDrmBridge::GetPlatformKeySystemNames() {
  return g_key_system_uuid_manager.Get().GetPlatformKeySystemNames();
}

// static
bool MediaDrmBridge::IsKeySystemSupported(const std::string& key_system) {
  DCHECK(!key_system.empty());
  return IsKeySystemSupportedWithTypeImpl(key_system, "");
}

// static
bool MediaDrmBridge::IsKeySystemSupportedWithType(
    const std::string& key_system,
    const std::string& container_mime_type) {
  DCHECK(!key_system.empty() && !container_mime_type.empty());
  return IsKeySystemSupportedWithTypeImpl(key_system, container_mime_type);
}

bool MediaDrmBridge::RegisterMediaDrmBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

MediaDrmBridge::MediaDrmBridge(const std::vector<uint8>& scheme_uuid,
                               const SessionCreatedCB& session_created_cb,
                               const SessionMessageCB& session_message_cb,
                               const SessionReadyCB& session_ready_cb,
                               const SessionClosedCB& session_closed_cb,
                               const SessionErrorCB& session_error_cb)
    : scheme_uuid_(scheme_uuid),
      session_created_cb_(session_created_cb),
      session_message_cb_(session_message_cb),
      session_ready_cb_(session_ready_cb),
      session_closed_cb_(session_closed_cb),
      session_error_cb_(session_error_cb) {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  ScopedJavaLocalRef<jbyteArray> j_scheme_uuid =
      base::android::ToJavaByteArray(env, &scheme_uuid[0], scheme_uuid.size());
  j_media_drm_.Reset(Java_MediaDrmBridge_create(
      env, j_scheme_uuid.obj(), reinterpret_cast<intptr_t>(this)));
}

MediaDrmBridge::~MediaDrmBridge() {
  JNIEnv* env = AttachCurrentThread();
  player_tracker_.NotifyCdmUnset();
  if (!j_media_drm_.is_null())
    Java_MediaDrmBridge_release(env, j_media_drm_.obj());
}

// static
scoped_ptr<MediaDrmBridge> MediaDrmBridge::Create(
    const std::string& key_system,
    const SessionCreatedCB& session_created_cb,
    const SessionMessageCB& session_message_cb,
    const SessionReadyCB& session_ready_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionErrorCB& session_error_cb) {
  scoped_ptr<MediaDrmBridge> media_drm_bridge;
  if (!IsAvailable())
    return media_drm_bridge.Pass();

  UUID scheme_uuid = g_key_system_uuid_manager.Get().GetUUID(key_system);
  if (scheme_uuid.empty())
    return media_drm_bridge.Pass();

  media_drm_bridge.reset(new MediaDrmBridge(scheme_uuid,
                                            session_created_cb,
                                            session_message_cb,
                                            session_ready_cb,
                                            session_closed_cb,
                                            session_error_cb));

  if (media_drm_bridge->j_media_drm_.is_null())
    media_drm_bridge.reset();

  return media_drm_bridge.Pass();
}

// static
scoped_ptr<MediaDrmBridge> MediaDrmBridge::CreateSessionless(
    const std::string& key_system) {
  return MediaDrmBridge::Create(key_system,
                                SessionCreatedCB(),
                                SessionMessageCB(),
                                SessionReadyCB(),
                                SessionClosedCB(),
                                SessionErrorCB());
}

bool MediaDrmBridge::SetSecurityLevel(SecurityLevel security_level) {
  JNIEnv* env = AttachCurrentThread();

  std::string security_level_str = GetSecurityLevelString(security_level);
  if (security_level_str.empty())
    return false;

  ScopedJavaLocalRef<jstring> j_security_level =
      ConvertUTF8ToJavaString(env, security_level_str);
  return Java_MediaDrmBridge_setSecurityLevel(
      env, j_media_drm_.obj(), j_security_level.obj());
}

bool MediaDrmBridge::CreateSession(uint32 session_id,
                                   const std::string& content_type,
                                   const uint8* init_data,
                                   int init_data_length) {
  DVLOG(1) << __FUNCTION__;

  DCHECK(!session_created_cb_.is_null())
      << "CreateSession called on a sessionless MediaDrmBridge object.";

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_init_data;
  // Caller should always use "video/*" content types.
  DCHECK_EQ(0u, content_type.find("video/"));

  // Widevine MediaDrm plugin only accepts the "data" part of the PSSH box as
  // the init data when using MP4 container.
  if (std::equal(scheme_uuid_.begin(), scheme_uuid_.end(), kWidevineUuid) &&
      content_type == "video/mp4") {
    std::vector<uint8> pssh_data;
    if (!GetPsshData(init_data, init_data_length, scheme_uuid_, &pssh_data))
      return false;
    j_init_data =
        base::android::ToJavaByteArray(env, &pssh_data[0], pssh_data.size());
  } else {
    j_init_data =
        base::android::ToJavaByteArray(env, init_data, init_data_length);
  }

  ScopedJavaLocalRef<jstring> j_mime =
      ConvertUTF8ToJavaString(env, content_type);
  Java_MediaDrmBridge_createSession(
      env, j_media_drm_.obj(), session_id, j_init_data.obj(), j_mime.obj());
  return true;
}

void MediaDrmBridge::LoadSession(uint32 session_id,
                                 const std::string& web_session_id) {
  // MediaDrmBridge doesn't support loading sessions.
  NOTREACHED();
}

void MediaDrmBridge::UpdateSession(uint32 session_id,
                                   const uint8* response,
                                   int response_length) {
  DVLOG(1) << __FUNCTION__;

  DCHECK(!session_ready_cb_.is_null())
      << __FUNCTION__ << " called on a sessionless MediaDrmBridge object.";

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_response =
      base::android::ToJavaByteArray(env, response, response_length);
  Java_MediaDrmBridge_updateSession(
      env, j_media_drm_.obj(), session_id, j_response.obj());
}

void MediaDrmBridge::ReleaseSession(uint32 session_id) {
  DVLOG(1) << __FUNCTION__;

  DCHECK(!session_closed_cb_.is_null())
      << __FUNCTION__ << " called on a sessionless MediaDrmBridge object.";

  JNIEnv* env = AttachCurrentThread();
  Java_MediaDrmBridge_releaseSession(env, j_media_drm_.obj(), session_id);
}

int MediaDrmBridge::RegisterPlayer(const base::Closure& new_key_cb,
                                   const base::Closure& cdm_unset_cb) {
  return player_tracker_.RegisterPlayer(new_key_cb, cdm_unset_cb);
}

void MediaDrmBridge::UnregisterPlayer(int registration_id) {
  player_tracker_.UnregisterPlayer(registration_id);
}

void MediaDrmBridge::SetMediaCryptoReadyCB(const base::Closure& closure) {
  if (closure.is_null()) {
    media_crypto_ready_cb_.Reset();
    return;
  }

  DCHECK(media_crypto_ready_cb_.is_null());

  if (!GetMediaCrypto().is_null()) {
    base::MessageLoopProxy::current()->PostTask(FROM_HERE, closure);
    return;
  }

  media_crypto_ready_cb_ = closure;
}

void MediaDrmBridge::OnMediaCryptoReady(JNIEnv* env, jobject) {
  DCHECK(!GetMediaCrypto().is_null());
  if (!media_crypto_ready_cb_.is_null())
    base::ResetAndReturn(&media_crypto_ready_cb_).Run();
}

void MediaDrmBridge::OnSessionCreated(JNIEnv* env,
                                      jobject j_media_drm,
                                      jint j_session_id,
                                      jstring j_web_session_id) {
  uint32 session_id = j_session_id;
  std::string web_session_id = ConvertJavaStringToUTF8(env, j_web_session_id);
  session_created_cb_.Run(session_id, web_session_id);
}

void MediaDrmBridge::OnSessionMessage(JNIEnv* env,
                                      jobject j_media_drm,
                                      jint j_session_id,
                                      jbyteArray j_message,
                                      jstring j_destination_url) {
  uint32 session_id = j_session_id;
  std::vector<uint8> message;
  JavaByteArrayToByteVector(env, j_message, &message);
  GURL destination_gurl = GURL(ConvertJavaStringToUTF8(env, j_destination_url));
  if (!destination_gurl.is_valid() && !destination_gurl.is_empty()) {
    DLOG(WARNING) << "SessionMessage destination_url is invalid : "
                  << destination_gurl.possibly_invalid_spec();
    destination_gurl = GURL::EmptyGURL();  // Replace invalid destination_url.
  }
  session_message_cb_.Run(session_id, message, destination_gurl);
}

void MediaDrmBridge::OnSessionReady(JNIEnv* env,
                                    jobject j_media_drm,
                                    jint j_session_id) {
  uint32 session_id = j_session_id;
  session_ready_cb_.Run(session_id);
  // TODO(xhwang/jrummell): Move this when usableKeyIds/keyschange are
  // implemented.
  player_tracker_.NotifyNewKey();
}

void MediaDrmBridge::OnSessionClosed(JNIEnv* env,
                                     jobject j_media_drm,
                                     jint j_session_id) {
  uint32 session_id = j_session_id;
  session_closed_cb_.Run(session_id);
}

void MediaDrmBridge::OnSessionError(JNIEnv* env,
                                    jobject j_media_drm,
                                    jint j_session_id) {
  uint32 session_id = j_session_id;
  session_error_cb_.Run(session_id, MediaKeys::kUnknownError, 0);
}

ScopedJavaLocalRef<jobject> MediaDrmBridge::GetMediaCrypto() {
  JNIEnv* env = AttachCurrentThread();
  return Java_MediaDrmBridge_getMediaCrypto(env, j_media_drm_.obj());
}

MediaDrmBridge::SecurityLevel MediaDrmBridge::GetSecurityLevel() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_security_level =
      Java_MediaDrmBridge_getSecurityLevel(env, j_media_drm_.obj());
  std::string security_level_str =
      ConvertJavaStringToUTF8(env, j_security_level.obj());
  return GetSecurityLevelFromString(security_level_str);
}

bool MediaDrmBridge::IsProtectedSurfaceRequired() {
  return IsSecureDecoderRequired(GetSecurityLevel());
}

void MediaDrmBridge::ResetDeviceCredentials(
    const ResetCredentialsCB& callback) {
  DCHECK(reset_credentials_cb_.is_null());
  reset_credentials_cb_ = callback;
  JNIEnv* env = AttachCurrentThread();
  Java_MediaDrmBridge_resetDeviceCredentials(env, j_media_drm_.obj());
}

void MediaDrmBridge::OnResetDeviceCredentialsCompleted(
    JNIEnv* env, jobject, bool success) {
  base::ResetAndReturn(&reset_credentials_cb_).Run(success);
}

}  // namespace media
