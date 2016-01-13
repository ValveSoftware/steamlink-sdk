// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/aes_decryptor.h"

#include <list>
#include <vector>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "crypto/encryptor.h"
#include "crypto/symmetric_key.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/cdm_promise.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/cdm/json_web_key.h"

namespace media {

// Keeps track of the session IDs and DecryptionKeys. The keys are ordered by
// insertion time (last insertion is first). It takes ownership of the
// DecryptionKeys.
class AesDecryptor::SessionIdDecryptionKeyMap {
  // Use a std::list to actually hold the data. Insertion is always done
  // at the front, so the "latest" decryption key is always the first one
  // in the list.
  typedef std::list<std::pair<std::string, DecryptionKey*> > KeyList;

 public:
  SessionIdDecryptionKeyMap() {}
  ~SessionIdDecryptionKeyMap() { STLDeleteValues(&key_list_); }

  // Replaces value if |session_id| is already present, or adds it if not.
  // This |decryption_key| becomes the latest until another insertion or
  // |session_id| is erased.
  void Insert(const std::string& web_session_id,
              scoped_ptr<DecryptionKey> decryption_key);

  // Deletes the entry for |session_id| if present.
  void Erase(const std::string& web_session_id);

  // Returns whether the list is empty
  bool Empty() const { return key_list_.empty(); }

  // Returns the last inserted DecryptionKey.
  DecryptionKey* LatestDecryptionKey() {
    DCHECK(!key_list_.empty());
    return key_list_.begin()->second;
  }

 private:
  // Searches the list for an element with |web_session_id|.
  KeyList::iterator Find(const std::string& web_session_id);

  // Deletes the entry pointed to by |position|.
  void Erase(KeyList::iterator position);

  KeyList key_list_;

  DISALLOW_COPY_AND_ASSIGN(SessionIdDecryptionKeyMap);
};

void AesDecryptor::SessionIdDecryptionKeyMap::Insert(
    const std::string& web_session_id,
    scoped_ptr<DecryptionKey> decryption_key) {
  KeyList::iterator it = Find(web_session_id);
  if (it != key_list_.end())
    Erase(it);
  DecryptionKey* raw_ptr = decryption_key.release();
  key_list_.push_front(std::make_pair(web_session_id, raw_ptr));
}

void AesDecryptor::SessionIdDecryptionKeyMap::Erase(
    const std::string& web_session_id) {
  KeyList::iterator it = Find(web_session_id);
  if (it == key_list_.end())
    return;
  Erase(it);
}

AesDecryptor::SessionIdDecryptionKeyMap::KeyList::iterator
AesDecryptor::SessionIdDecryptionKeyMap::Find(
    const std::string& web_session_id) {
  for (KeyList::iterator it = key_list_.begin(); it != key_list_.end(); ++it) {
    if (it->first == web_session_id)
      return it;
  }
  return key_list_.end();
}

void AesDecryptor::SessionIdDecryptionKeyMap::Erase(
    KeyList::iterator position) {
  DCHECK(position->second);
  delete position->second;
  key_list_.erase(position);
}

uint32 AesDecryptor::next_web_session_id_ = 1;

enum ClearBytesBufferSel {
  kSrcContainsClearBytes,
  kDstContainsClearBytes
};

static void CopySubsamples(const std::vector<SubsampleEntry>& subsamples,
                           const ClearBytesBufferSel sel,
                           const uint8* src,
                           uint8* dst) {
  for (size_t i = 0; i < subsamples.size(); i++) {
    const SubsampleEntry& subsample = subsamples[i];
    if (sel == kSrcContainsClearBytes) {
      src += subsample.clear_bytes;
    } else {
      dst += subsample.clear_bytes;
    }
    memcpy(dst, src, subsample.cypher_bytes);
    src += subsample.cypher_bytes;
    dst += subsample.cypher_bytes;
  }
}

// Decrypts |input| using |key|.  Returns a DecoderBuffer with the decrypted
// data if decryption succeeded or NULL if decryption failed.
static scoped_refptr<DecoderBuffer> DecryptData(const DecoderBuffer& input,
                                                crypto::SymmetricKey* key) {
  CHECK(input.data_size());
  CHECK(input.decrypt_config());
  CHECK(key);

  crypto::Encryptor encryptor;
  if (!encryptor.Init(key, crypto::Encryptor::CTR, "")) {
    DVLOG(1) << "Could not initialize decryptor.";
    return NULL;
  }

  DCHECK_EQ(input.decrypt_config()->iv().size(),
            static_cast<size_t>(DecryptConfig::kDecryptionKeySize));
  if (!encryptor.SetCounter(input.decrypt_config()->iv())) {
    DVLOG(1) << "Could not set counter block.";
    return NULL;
  }

  const char* sample = reinterpret_cast<const char*>(input.data());
  size_t sample_size = static_cast<size_t>(input.data_size());

  DCHECK_GT(sample_size, 0U) << "No sample data to be decrypted.";
  if (sample_size == 0)
    return NULL;

  if (input.decrypt_config()->subsamples().empty()) {
    std::string decrypted_text;
    base::StringPiece encrypted_text(sample, sample_size);
    if (!encryptor.Decrypt(encrypted_text, &decrypted_text)) {
      DVLOG(1) << "Could not decrypt data.";
      return NULL;
    }

    // TODO(xhwang): Find a way to avoid this data copy.
    return DecoderBuffer::CopyFrom(
        reinterpret_cast<const uint8*>(decrypted_text.data()),
        decrypted_text.size());
  }

  const std::vector<SubsampleEntry>& subsamples =
      input.decrypt_config()->subsamples();

  size_t total_clear_size = 0;
  size_t total_encrypted_size = 0;
  for (size_t i = 0; i < subsamples.size(); i++) {
    total_clear_size += subsamples[i].clear_bytes;
    total_encrypted_size += subsamples[i].cypher_bytes;
    // Check for overflow. This check is valid because *_size is unsigned.
    DCHECK(total_clear_size >= subsamples[i].clear_bytes);
    if (total_encrypted_size < subsamples[i].cypher_bytes)
      return NULL;
  }
  size_t total_size = total_clear_size + total_encrypted_size;
  if (total_size < total_clear_size || total_size != sample_size) {
    DVLOG(1) << "Subsample sizes do not equal input size";
    return NULL;
  }

  // No need to decrypt if there is no encrypted data.
  if (total_encrypted_size <= 0) {
    return DecoderBuffer::CopyFrom(reinterpret_cast<const uint8*>(sample),
                                   sample_size);
  }

  // The encrypted portions of all subsamples must form a contiguous block,
  // such that an encrypted subsample that ends away from a block boundary is
  // immediately followed by the start of the next encrypted subsample. We
  // copy all encrypted subsamples to a contiguous buffer, decrypt them, then
  // copy the decrypted bytes over the encrypted bytes in the output.
  // TODO(strobe): attempt to reduce number of memory copies
  scoped_ptr<uint8[]> encrypted_bytes(new uint8[total_encrypted_size]);
  CopySubsamples(subsamples, kSrcContainsClearBytes,
                 reinterpret_cast<const uint8*>(sample), encrypted_bytes.get());

  base::StringPiece encrypted_text(
      reinterpret_cast<const char*>(encrypted_bytes.get()),
      total_encrypted_size);
  std::string decrypted_text;
  if (!encryptor.Decrypt(encrypted_text, &decrypted_text)) {
    DVLOG(1) << "Could not decrypt data.";
    return NULL;
  }
  DCHECK_EQ(decrypted_text.size(), encrypted_text.size());

  scoped_refptr<DecoderBuffer> output = DecoderBuffer::CopyFrom(
      reinterpret_cast<const uint8*>(sample), sample_size);
  CopySubsamples(subsamples, kDstContainsClearBytes,
                 reinterpret_cast<const uint8*>(decrypted_text.data()),
                 output->writable_data());
  return output;
}

AesDecryptor::AesDecryptor(const SessionMessageCB& session_message_cb,
                           const SessionClosedCB& session_closed_cb)
    : session_message_cb_(session_message_cb),
      session_closed_cb_(session_closed_cb) {
  DCHECK(!session_message_cb_.is_null());
  DCHECK(!session_closed_cb_.is_null());
}

AesDecryptor::~AesDecryptor() {
  key_map_.clear();
}

void AesDecryptor::CreateSession(const std::string& init_data_type,
                                 const uint8* init_data,
                                 int init_data_length,
                                 SessionType session_type,
                                 scoped_ptr<NewSessionCdmPromise> promise) {
  std::string web_session_id(base::UintToString(next_web_session_id_++));
  valid_sessions_.insert(web_session_id);

  // For now, the AesDecryptor does not care about |init_data_type| or
  // |session_type|; just resolve the promise and then fire a message event
  // with the |init_data| as the request.
  // TODO(jrummell): Validate |init_data_type| and |session_type|.
  std::vector<uint8> message;
  if (init_data && init_data_length)
    message.assign(init_data, init_data + init_data_length);

  promise->resolve(web_session_id);

  session_message_cb_.Run(web_session_id, message, GURL());
}

void AesDecryptor::LoadSession(const std::string& web_session_id,
                               scoped_ptr<NewSessionCdmPromise> promise) {
  // TODO(xhwang): Change this to NOTREACHED() when blink checks for key systems
  // that do not support loadSession. See http://crbug.com/342481
  promise->reject(NOT_SUPPORTED_ERROR, 0, "LoadSession() is not supported.");
}

void AesDecryptor::UpdateSession(const std::string& web_session_id,
                                 const uint8* response,
                                 int response_length,
                                 scoped_ptr<SimpleCdmPromise> promise) {
  CHECK(response);
  CHECK_GT(response_length, 0);

  // TODO(jrummell): Convert back to a DCHECK once prefixed EME is removed.
  if (valid_sessions_.find(web_session_id) == valid_sessions_.end()) {
    promise->reject(INVALID_ACCESS_ERROR, 0, "Session does not exist.");
    return;
  }

  std::string key_string(reinterpret_cast<const char*>(response),
                         response_length);

  KeyIdAndKeyPairs keys;
  if (!ExtractKeysFromJWKSet(key_string, &keys)) {
    promise->reject(
        INVALID_ACCESS_ERROR, 0, "response is not a valid JSON Web Key Set.");
    return;
  }

  // Make sure that at least one key was extracted.
  if (keys.empty()) {
    promise->reject(
        INVALID_ACCESS_ERROR, 0, "response does not contain any keys.");
    return;
  }

  for (KeyIdAndKeyPairs::iterator it = keys.begin(); it != keys.end(); ++it) {
    if (it->second.length() !=
        static_cast<size_t>(DecryptConfig::kDecryptionKeySize)) {
      DVLOG(1) << "Invalid key length: " << key_string.length();
      promise->reject(INVALID_ACCESS_ERROR, 0, "Invalid key length.");
      return;
    }
    if (!AddDecryptionKey(web_session_id, it->first, it->second)) {
      promise->reject(INVALID_ACCESS_ERROR, 0, "Unable to add key.");
      return;
    }
  }

  {
    base::AutoLock auto_lock(new_key_cb_lock_);

    if (!new_audio_key_cb_.is_null())
      new_audio_key_cb_.Run();

    if (!new_video_key_cb_.is_null())
      new_video_key_cb_.Run();
  }

  promise->resolve();
}

void AesDecryptor::ReleaseSession(const std::string& web_session_id,
                                  scoped_ptr<SimpleCdmPromise> promise) {
  // Validate that this is a reference to an active session and then forget it.
  std::set<std::string>::iterator it = valid_sessions_.find(web_session_id);
  // TODO(jrummell): Convert back to a DCHECK once prefixed EME is removed.
  if (it == valid_sessions_.end()) {
    promise->reject(INVALID_ACCESS_ERROR, 0, "Session does not exist.");
    return;
  }

  valid_sessions_.erase(it);

  // Close the session.
  DeleteKeysForSession(web_session_id);
  promise->resolve();
  session_closed_cb_.Run(web_session_id);
}

Decryptor* AesDecryptor::GetDecryptor() {
  return this;
}

void AesDecryptor::RegisterNewKeyCB(StreamType stream_type,
                                    const NewKeyCB& new_key_cb) {
  base::AutoLock auto_lock(new_key_cb_lock_);

  switch (stream_type) {
    case kAudio:
      new_audio_key_cb_ = new_key_cb;
      break;
    case kVideo:
      new_video_key_cb_ = new_key_cb;
      break;
    default:
      NOTREACHED();
  }
}

void AesDecryptor::Decrypt(StreamType stream_type,
                           const scoped_refptr<DecoderBuffer>& encrypted,
                           const DecryptCB& decrypt_cb) {
  CHECK(encrypted->decrypt_config());

  scoped_refptr<DecoderBuffer> decrypted;
  // An empty iv string signals that the frame is unencrypted.
  if (encrypted->decrypt_config()->iv().empty()) {
    decrypted = DecoderBuffer::CopyFrom(encrypted->data(),
                                        encrypted->data_size());
  } else {
    const std::string& key_id = encrypted->decrypt_config()->key_id();
    DecryptionKey* key = GetKey(key_id);
    if (!key) {
      DVLOG(1) << "Could not find a matching key for the given key ID.";
      decrypt_cb.Run(kNoKey, NULL);
      return;
    }

    crypto::SymmetricKey* decryption_key = key->decryption_key();
    decrypted = DecryptData(*encrypted.get(), decryption_key);
    if (!decrypted.get()) {
      DVLOG(1) << "Decryption failed.";
      decrypt_cb.Run(kError, NULL);
      return;
    }
  }

  decrypted->set_timestamp(encrypted->timestamp());
  decrypted->set_duration(encrypted->duration());
  decrypt_cb.Run(kSuccess, decrypted);
}

void AesDecryptor::CancelDecrypt(StreamType stream_type) {
  // Decrypt() calls the DecryptCB synchronously so there's nothing to cancel.
}

void AesDecryptor::InitializeAudioDecoder(const AudioDecoderConfig& config,
                                          const DecoderInitCB& init_cb) {
  // AesDecryptor does not support audio decoding.
  init_cb.Run(false);
}

void AesDecryptor::InitializeVideoDecoder(const VideoDecoderConfig& config,
                                          const DecoderInitCB& init_cb) {
  // AesDecryptor does not support video decoding.
  init_cb.Run(false);
}

void AesDecryptor::DecryptAndDecodeAudio(
    const scoped_refptr<DecoderBuffer>& encrypted,
    const AudioDecodeCB& audio_decode_cb) {
  NOTREACHED() << "AesDecryptor does not support audio decoding";
}

void AesDecryptor::DecryptAndDecodeVideo(
    const scoped_refptr<DecoderBuffer>& encrypted,
    const VideoDecodeCB& video_decode_cb) {
  NOTREACHED() << "AesDecryptor does not support video decoding";
}

void AesDecryptor::ResetDecoder(StreamType stream_type) {
  NOTREACHED() << "AesDecryptor does not support audio/video decoding";
}

void AesDecryptor::DeinitializeDecoder(StreamType stream_type) {
  NOTREACHED() << "AesDecryptor does not support audio/video decoding";
}

bool AesDecryptor::AddDecryptionKey(const std::string& web_session_id,
                                    const std::string& key_id,
                                    const std::string& key_string) {
  scoped_ptr<DecryptionKey> decryption_key(new DecryptionKey(key_string));
  if (!decryption_key->Init()) {
    DVLOG(1) << "Could not initialize decryption key.";
    return false;
  }

  base::AutoLock auto_lock(key_map_lock_);
  KeyIdToSessionKeysMap::iterator key_id_entry = key_map_.find(key_id);
  if (key_id_entry != key_map_.end()) {
    key_id_entry->second->Insert(web_session_id, decryption_key.Pass());
    return true;
  }

  // |key_id| not found, so need to create new entry.
  scoped_ptr<SessionIdDecryptionKeyMap> inner_map(
      new SessionIdDecryptionKeyMap());
  inner_map->Insert(web_session_id, decryption_key.Pass());
  key_map_.add(key_id, inner_map.Pass());
  return true;
}

AesDecryptor::DecryptionKey* AesDecryptor::GetKey(
    const std::string& key_id) const {
  base::AutoLock auto_lock(key_map_lock_);
  KeyIdToSessionKeysMap::const_iterator key_id_found = key_map_.find(key_id);
  if (key_id_found == key_map_.end())
    return NULL;

  // Return the key from the "latest" session_id entry.
  return key_id_found->second->LatestDecryptionKey();
}

void AesDecryptor::DeleteKeysForSession(const std::string& web_session_id) {
  base::AutoLock auto_lock(key_map_lock_);

  // Remove all keys associated with |web_session_id|. Since the data is
  // optimized for access in GetKey(), we need to look at each entry in
  // |key_map_|.
  KeyIdToSessionKeysMap::iterator it = key_map_.begin();
  while (it != key_map_.end()) {
    it->second->Erase(web_session_id);
    if (it->second->Empty()) {
      // Need to get rid of the entry for this key_id. This will mess up the
      // iterator, so we need to increment it first.
      KeyIdToSessionKeysMap::iterator current = it;
      ++it;
      key_map_.erase(current);
    } else {
      ++it;
    }
  }
}

AesDecryptor::DecryptionKey::DecryptionKey(const std::string& secret)
    : secret_(secret) {
}

AesDecryptor::DecryptionKey::~DecryptionKey() {}

bool AesDecryptor::DecryptionKey::Init() {
  CHECK(!secret_.empty());
  decryption_key_.reset(crypto::SymmetricKey::Import(
      crypto::SymmetricKey::AES, secret_));
  if (!decryption_key_)
    return false;
  return true;
}

}  // namespace media
