// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/debug/trace_event.h"
#include "base/format_macros.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "crypto/sha2.h"
#include "net/cert/crl_set.h"
#include "third_party/zlib/zlib.h"

namespace net {

// Decompress zlib decompressed |in| into |out|. |out_len| is the number of
// bytes at |out| and must be exactly equal to the size of the decompressed
// data.
static bool DecompressZlib(uint8* out, int out_len, base::StringPiece in) {
  z_stream z;
  memset(&z, 0, sizeof(z));

  z.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(in.data()));
  z.avail_in = in.size();
  z.next_out = reinterpret_cast<Bytef*>(out);
  z.avail_out = out_len;

  if (inflateInit(&z) != Z_OK)
    return false;
  bool ret = false;
  int r = inflate(&z, Z_FINISH);
  if (r != Z_STREAM_END)
    goto err;
  if (z.avail_in || z.avail_out)
    goto err;
  ret = true;

 err:
  inflateEnd(&z);
  return ret;
}

CRLSet::CRLSet()
    : sequence_(0),
      not_after_(0) {
}

CRLSet::~CRLSet() {
}

// CRLSet format:
//
// uint16le header_len
// byte[header_len] header_bytes
// repeated {
//   byte[32] parent_spki_sha256
//   uint32le num_serials
//   [num_serials] {
//     uint8 serial_length;
//     byte[serial_length] serial;
//   }
//
// header_bytes consists of a JSON dictionary with the following keys:
//   Version (int): currently 0
//   ContentType (string): "CRLSet" or "CRLSetDelta" (magic value)
//   DeltaFrom (int32): if this is a delta update (see below), then this
//       contains the sequence number of the base CRLSet.
//   Sequence (int32): the monotonic sequence number of this CRL set.
//
// A delta CRLSet is similar to a CRLSet:
//
// struct CompressedChanges {
//    uint32le uncompressed_size
//    uint32le compressed_size
//    byte[compressed_size] zlib_data
// }
//
// uint16le header_len
// byte[header_len] header_bytes
// CompressedChanges crl_changes
// [crl_changes.uncompressed_size] {
//   switch (crl_changes[i]) {
//   case 0:
//     // CRL is the same
//   case 1:
//     // New CRL inserted
//     // See CRL structure from the non-delta format
//   case 2:
//     // CRL deleted
//   case 3:
//     // CRL changed
//     CompressedChanges serials_changes
//     [serials_changes.uncompressed_size] {
//       switch (serials_changes[i]) {
//       case 0:
//         // the serial is the same
//       case 1:
//         // serial inserted
//         uint8 serial_length
//         byte[serial_length] serial
//       case 2:
//         // serial deleted
//       }
//     }
//   }
// }
//
// A delta CRLSet applies to a specific CRL set as given in the
// header's "DeltaFrom" value. The delta describes the changes to each CRL
// in turn with a zlib compressed array of options: either the CRL is the same,
// a new CRL is inserted, the CRL is deleted or the CRL is updated. In the case
// of an update, the serials in the CRL are considered in the same fashion
// except there is no delta update of a serial number: they are either
// inserted, deleted or left the same.

// ReadHeader reads the header (including length prefix) from |data| and
// updates |data| to remove the header on return. Caller takes ownership of the
// returned pointer.
static base::DictionaryValue* ReadHeader(base::StringPiece* data) {
  if (data->size() < 2)
    return NULL;
  uint16 header_len;
  memcpy(&header_len, data->data(), 2);  // assumes little-endian.
  data->remove_prefix(2);

  if (data->size() < header_len)
    return NULL;

  const base::StringPiece header_bytes(data->data(), header_len);
  data->remove_prefix(header_len);

  scoped_ptr<base::Value> header(base::JSONReader::Read(
      header_bytes, base::JSON_ALLOW_TRAILING_COMMAS));
  if (header.get() == NULL)
    return NULL;

  if (!header->IsType(base::Value::TYPE_DICTIONARY))
    return NULL;
  return reinterpret_cast<base::DictionaryValue*>(header.release());
}

// kCurrentFileVersion is the version of the CRLSet file format that we
// currently implement.
static const int kCurrentFileVersion = 0;

static bool ReadCRL(base::StringPiece* data, std::string* out_parent_spki_hash,
                    std::vector<std::string>* out_serials) {
  if (data->size() < crypto::kSHA256Length)
    return false;
  out_parent_spki_hash->assign(data->data(), crypto::kSHA256Length);
  data->remove_prefix(crypto::kSHA256Length);

  if (data->size() < sizeof(uint32))
    return false;
  uint32 num_serials;
  memcpy(&num_serials, data->data(), sizeof(uint32));  // assumes little endian
  if (num_serials > 32 * 1024 * 1024)  // Sanity check.
    return false;

  out_serials->reserve(num_serials);
  data->remove_prefix(sizeof(uint32));

  for (uint32 i = 0; i < num_serials; ++i) {
    if (data->size() < sizeof(uint8))
      return false;

    uint8 serial_length = data->data()[0];
    data->remove_prefix(sizeof(uint8));

    if (data->size() < serial_length)
      return false;

    out_serials->push_back(std::string());
    out_serials->back().assign(data->data(), serial_length);
    data->remove_prefix(serial_length);
  }

  return true;
}

bool CRLSet::CopyBlockedSPKIsFromHeader(base::DictionaryValue* header_dict) {
  base::ListValue* blocked_spkis_list = NULL;
  if (!header_dict->GetList("BlockedSPKIs", &blocked_spkis_list)) {
    // BlockedSPKIs is optional, so it's fine if we don't find it.
    return true;
  }

  blocked_spkis_.clear();
  blocked_spkis_.reserve(blocked_spkis_list->GetSize());

  std::string spki_sha256_base64;

  for (size_t i = 0; i < blocked_spkis_list->GetSize(); ++i) {
    spki_sha256_base64.clear();

    if (!blocked_spkis_list->GetString(i, &spki_sha256_base64))
      return false;

    blocked_spkis_.push_back(std::string());
    if (!base::Base64Decode(spki_sha256_base64, &blocked_spkis_.back())) {
      blocked_spkis_.pop_back();
      return false;
    }
  }

  return true;
}

// static
bool CRLSet::Parse(base::StringPiece data, scoped_refptr<CRLSet>* out_crl_set) {
  TRACE_EVENT0("CRLSet", "Parse");
  // Other parts of Chrome assume that we're little endian, so we don't lose
  // anything by doing this.
#if defined(__BYTE_ORDER)
  // Linux check
  COMPILE_ASSERT(__BYTE_ORDER == __LITTLE_ENDIAN, assumes_little_endian);
#elif defined(__BIG_ENDIAN__)
  // Mac check
  #error assumes little endian
#endif

  scoped_ptr<base::DictionaryValue> header_dict(ReadHeader(&data));
  if (!header_dict.get())
    return false;

  std::string contents;
  if (!header_dict->GetString("ContentType", &contents))
    return false;
  if (contents != "CRLSet")
    return false;

  int version;
  if (!header_dict->GetInteger("Version", &version) ||
      version != kCurrentFileVersion) {
    return false;
  }

  int sequence;
  if (!header_dict->GetInteger("Sequence", &sequence))
    return false;

  double not_after;
  if (!header_dict->GetDouble("NotAfter", &not_after)) {
    // NotAfter is optional for now.
    not_after = 0;
  }
  if (not_after < 0)
    return false;

  scoped_refptr<CRLSet> crl_set(new CRLSet());
  crl_set->sequence_ = static_cast<uint32>(sequence);
  crl_set->not_after_ = static_cast<uint64>(not_after);
  crl_set->crls_.reserve(64);  // Value observed experimentally.

  for (size_t crl_index = 0; !data.empty(); crl_index++) {
    // Speculatively push back a pair and pass it to ReadCRL() to avoid
    // unnecessary copies.
    crl_set->crls_.push_back(
        std::make_pair(std::string(), std::vector<std::string>()));
    std::pair<std::string, std::vector<std::string> >* const back_pair =
        &crl_set->crls_.back();

    if (!ReadCRL(&data, &back_pair->first, &back_pair->second)) {
      // Undo the speculative push_back() performed above.
      crl_set->crls_.pop_back();
      return false;
    }

    crl_set->crls_index_by_issuer_[back_pair->first] = crl_index;
  }

  if (!crl_set->CopyBlockedSPKIsFromHeader(header_dict.get()))
    return false;

  *out_crl_set = crl_set;
  return true;
}

// kMaxUncompressedChangesLength is the largest changes array that we'll
// accept. This bounds the number of CRLs in the CRLSet as well as the number
// of serial numbers in a given CRL.
static const unsigned kMaxUncompressedChangesLength = 1024 * 1024;

static bool ReadChanges(base::StringPiece* data,
                        std::vector<uint8>* out_changes) {
  uint32 uncompressed_size, compressed_size;
  if (data->size() < 2 * sizeof(uint32))
    return false;
  // assumes little endian.
  memcpy(&uncompressed_size, data->data(), sizeof(uint32));
  data->remove_prefix(4);
  memcpy(&compressed_size, data->data(), sizeof(uint32));
  data->remove_prefix(4);

  if (uncompressed_size > kMaxUncompressedChangesLength)
    return false;
  if (data->size() < compressed_size)
    return false;

  out_changes->clear();
  if (uncompressed_size == 0)
    return true;

  out_changes->resize(uncompressed_size);
  base::StringPiece compressed(data->data(), compressed_size);
  data->remove_prefix(compressed_size);
  return DecompressZlib(&(*out_changes)[0], uncompressed_size, compressed);
}

// These are the range coder symbols used in delta updates.
enum {
  SYMBOL_SAME = 0,
  SYMBOL_INSERT = 1,
  SYMBOL_DELETE = 2,
  SYMBOL_CHANGED = 3,
};

bool ReadDeltaCRL(base::StringPiece* data,
                  const std::vector<std::string>& old_serials,
                  std::vector<std::string>* out_serials) {
  std::vector<uint8> changes;
  if (!ReadChanges(data, &changes))
    return false;

  size_t i = 0;
  for (std::vector<uint8>::const_iterator k = changes.begin();
       k != changes.end(); ++k) {
    if (*k == SYMBOL_SAME) {
      if (i >= old_serials.size())
        return false;
      out_serials->push_back(old_serials[i]);
      i++;
    } else if (*k == SYMBOL_INSERT) {
      uint8 serial_length;
      if (data->size() < sizeof(uint8))
        return false;
      memcpy(&serial_length, data->data(), sizeof(uint8));
      data->remove_prefix(sizeof(uint8));

      if (data->size() < serial_length)
        return false;
      const std::string serial(data->data(), serial_length);
      data->remove_prefix(serial_length);

      out_serials->push_back(serial);
    } else if (*k == SYMBOL_DELETE) {
      if (i >= old_serials.size())
        return false;
      i++;
    } else {
      NOTREACHED();
      return false;
    }
  }

  if (i != old_serials.size())
    return false;
  return true;
}

bool CRLSet::ApplyDelta(const base::StringPiece& in_data,
                        scoped_refptr<CRLSet>* out_crl_set) {
  base::StringPiece data(in_data);
  scoped_ptr<base::DictionaryValue> header_dict(ReadHeader(&data));
  if (!header_dict.get())
    return false;

  std::string contents;
  if (!header_dict->GetString("ContentType", &contents))
    return false;
  if (contents != "CRLSetDelta")
    return false;

  int version;
  if (!header_dict->GetInteger("Version", &version) ||
      version != kCurrentFileVersion) {
    return false;
  }

  int sequence, delta_from;
  if (!header_dict->GetInteger("Sequence", &sequence) ||
      !header_dict->GetInteger("DeltaFrom", &delta_from) ||
      delta_from < 0 ||
      static_cast<uint32>(delta_from) != sequence_) {
    return false;
  }

  double not_after;
  if (!header_dict->GetDouble("NotAfter", &not_after)) {
    // NotAfter is optional for now.
    not_after = 0;
  }
  if (not_after < 0)
    return false;

  scoped_refptr<CRLSet> crl_set(new CRLSet);
  crl_set->sequence_ = static_cast<uint32>(sequence);
  crl_set->not_after_ = static_cast<uint64>(not_after);

  if (!crl_set->CopyBlockedSPKIsFromHeader(header_dict.get()))
    return false;

  std::vector<uint8> crl_changes;

  if (!ReadChanges(&data, &crl_changes))
    return false;

  size_t i = 0, j = 0;
  for (std::vector<uint8>::const_iterator k = crl_changes.begin();
       k != crl_changes.end(); ++k) {
    if (*k == SYMBOL_SAME) {
      if (i >= crls_.size())
        return false;
      crl_set->crls_.push_back(crls_[i]);
      crl_set->crls_index_by_issuer_[crls_[i].first] = j;
      i++;
      j++;
    } else if (*k == SYMBOL_INSERT) {
      std::string parent_spki_hash;
      std::vector<std::string> serials;
      if (!ReadCRL(&data, &parent_spki_hash, &serials))
        return false;
      crl_set->crls_.push_back(std::make_pair(parent_spki_hash, serials));
      crl_set->crls_index_by_issuer_[parent_spki_hash] = j;
      j++;
    } else if (*k == SYMBOL_DELETE) {
      if (i >= crls_.size())
        return false;
      i++;
    } else if (*k == SYMBOL_CHANGED) {
      if (i >= crls_.size())
        return false;
      std::vector<std::string> serials;
      if (!ReadDeltaCRL(&data, crls_[i].second, &serials))
        return false;
      crl_set->crls_.push_back(std::make_pair(crls_[i].first, serials));
      crl_set->crls_index_by_issuer_[crls_[i].first] = j;
      i++;
      j++;
    } else {
      NOTREACHED();
      return false;
    }
  }

  if (!data.empty())
    return false;
  if (i != crls_.size())
    return false;

  *out_crl_set = crl_set;
  return true;
}

// static
bool CRLSet::GetIsDeltaUpdate(const base::StringPiece& in_data,
                              bool* is_delta) {
  base::StringPiece data(in_data);
  scoped_ptr<base::DictionaryValue> header_dict(ReadHeader(&data));
  if (!header_dict.get())
    return false;

  std::string contents;
  if (!header_dict->GetString("ContentType", &contents))
    return false;

  if (contents == "CRLSet") {
    *is_delta = false;
  } else if (contents == "CRLSetDelta") {
    *is_delta = true;
  } else {
    return false;
  }

  return true;
}

std::string CRLSet::Serialize() const {
  std::string header = base::StringPrintf(
      "{"
      "\"Version\":0,"
      "\"ContentType\":\"CRLSet\","
      "\"Sequence\":%u,"
      "\"DeltaFrom\":0,"
      "\"NumParents\":%u,"
      "\"BlockedSPKIs\":[",
      static_cast<unsigned>(sequence_),
      static_cast<unsigned>(crls_.size()));

  for (std::vector<std::string>::const_iterator i = blocked_spkis_.begin();
       i != blocked_spkis_.end(); ++i) {
    std::string spki_hash_base64;
    base::Base64Encode(*i, &spki_hash_base64);

    if (i != blocked_spkis_.begin())
      header += ",";
    header += "\"" + spki_hash_base64 + "\"";
  }
  header += "]";
  if (not_after_ != 0)
    header += base::StringPrintf(",\"NotAfter\":%" PRIu64, not_after_);
  header += "}";

  size_t len = 2 /* header len */ + header.size();

  for (CRLList::const_iterator i = crls_.begin(); i != crls_.end(); ++i) {
    len += i->first.size() + 4 /* num serials */;
    for (std::vector<std::string>::const_iterator j = i->second.begin();
         j != i->second.end(); ++j) {
      len += 1 /* serial length */ + j->size();
    }
  }

  std::string ret;
  char* out = WriteInto(&ret, len + 1 /* to include final NUL */);
  size_t off = 0;
  out[off++] = header.size();
  out[off++] = header.size() >> 8;
  memcpy(out + off, header.data(), header.size());
  off += header.size();

  for (CRLList::const_iterator i = crls_.begin(); i != crls_.end(); ++i) {
    memcpy(out + off, i->first.data(), i->first.size());
    off += i->first.size();
    const uint32 num_serials = i->second.size();
    memcpy(out + off, &num_serials, sizeof(num_serials));
    off += sizeof(num_serials);

    for (std::vector<std::string>::const_iterator j = i->second.begin();
         j != i->second.end(); ++j) {
      out[off++] = j->size();
      memcpy(out + off, j->data(), j->size());
      off += j->size();
    }
  }

  CHECK_EQ(off, len);
  return ret;
}

CRLSet::Result CRLSet::CheckSPKI(const base::StringPiece& spki_hash) const {
  for (std::vector<std::string>::const_iterator i = blocked_spkis_.begin();
       i != blocked_spkis_.end(); ++i) {
    if (spki_hash.size() == i->size() &&
        memcmp(spki_hash.data(), i->data(), i->size()) == 0) {
      return REVOKED;
    }
  }

  return GOOD;
}

CRLSet::Result CRLSet::CheckSerial(
    const base::StringPiece& serial_number,
    const base::StringPiece& issuer_spki_hash) const {
  base::StringPiece serial(serial_number);

  if (!serial.empty() && (serial[0] & 0x80) != 0) {
    // This serial number is negative but the process which generates CRL sets
    // will reject any certificates with negative serial numbers as invalid.
    return UNKNOWN;
  }

  // Remove any leading zero bytes.
  while (serial.size() > 1 && serial[0] == 0x00)
    serial.remove_prefix(1);

  base::hash_map<std::string, size_t>::const_iterator i =
      crls_index_by_issuer_.find(issuer_spki_hash.as_string());
  if (i == crls_index_by_issuer_.end())
    return UNKNOWN;
  const std::vector<std::string>& serials = crls_[i->second].second;

  for (std::vector<std::string>::const_iterator i = serials.begin();
       i != serials.end(); ++i) {
    if (base::StringPiece(*i) == serial)
      return REVOKED;
  }

  return GOOD;
}

bool CRLSet::IsExpired() const {
  if (not_after_ == 0)
    return false;

  uint64 now = base::Time::Now().ToTimeT();
  return now > not_after_;
}

uint32 CRLSet::sequence() const {
  return sequence_;
}

const CRLSet::CRLList& CRLSet::crls() const {
  return crls_;
}

// static
CRLSet* CRLSet::EmptyCRLSetForTesting() {
  return ForTesting(false, NULL, "");
}

CRLSet* CRLSet::ExpiredCRLSetForTesting() {
  return ForTesting(true, NULL, "");
}

// static
CRLSet* CRLSet::ForTesting(bool is_expired,
                           const SHA256HashValue* issuer_spki,
                           const std::string& serial_number) {
  CRLSet* crl_set = new CRLSet;
  if (is_expired)
    crl_set->not_after_ = 1;
  if (issuer_spki != NULL) {
    const std::string spki(reinterpret_cast<const char*>(issuer_spki->data),
                           sizeof(issuer_spki->data));
    crl_set->crls_.push_back(make_pair(spki, std::vector<std::string>()));
    crl_set->crls_index_by_issuer_[spki] = 0;
  }

  if (!serial_number.empty())
    crl_set->crls_[0].second.push_back(serial_number);

  return crl_set;
}

}  // namespace net
