// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/box_reader.h"

#include <stddef.h>
#include <string.h>

#include <algorithm>
#include <memory>
#include <set>

#include "media/formats/mp4/box_definitions.h"

namespace media {
namespace mp4 {

Box::~Box() {}

bool BufferReader::Read1(uint8_t* v) {
  RCHECK(HasBytes(1));
  *v = buf_[pos_++];
  return true;
}

// Internal implementation of multi-byte reads
template<typename T> bool BufferReader::Read(T* v) {
  RCHECK(HasBytes(sizeof(T)));

  T tmp = 0;
  for (size_t i = 0; i < sizeof(T); i++) {
    tmp <<= 8;
    tmp += buf_[pos_++];
  }
  *v = tmp;
  return true;
}

bool BufferReader::Read2(uint16_t* v) {
  return Read(v);
}
bool BufferReader::Read2s(int16_t* v) {
  return Read(v);
}
bool BufferReader::Read4(uint32_t* v) {
  return Read(v);
}
bool BufferReader::Read4s(int32_t* v) {
  return Read(v);
}
bool BufferReader::Read8(uint64_t* v) {
  return Read(v);
}
bool BufferReader::Read8s(int64_t* v) {
  return Read(v);
}

bool BufferReader::ReadFourCC(FourCC* v) {
  return Read4(reinterpret_cast<uint32_t*>(v));
}

bool BufferReader::ReadVec(std::vector<uint8_t>* vec, uint64_t count) {
  RCHECK(HasBytes(count));
  vec->clear();
  vec->insert(vec->end(), buf_ + pos_, buf_ + pos_ + count);
  pos_ += count;
  return true;
}

bool BufferReader::SkipBytes(uint64_t bytes) {
  RCHECK(HasBytes(bytes));
  pos_ += bytes;
  return true;
}

bool BufferReader::Read4Into8(uint64_t* v) {
  uint32_t tmp;
  RCHECK(Read4(&tmp));
  *v = tmp;
  return true;
}

bool BufferReader::Read4sInto8s(int64_t* v) {
  // Beware of the need for sign extension.
  int32_t tmp;
  RCHECK(Read4s(&tmp));
  *v = tmp;
  return true;
}

BoxReader::BoxReader(const uint8_t* buf,
                     const size_t buf_size,
                     const scoped_refptr<MediaLog>& media_log,
                     bool is_EOS)
    : BufferReader(buf, buf_size),
      media_log_(media_log),
      box_size_(0),
      box_size_known_(false),
      type_(FOURCC_NULL),
      version_(0),
      flags_(0),
      scanned_(false),
      is_EOS_(is_EOS) {}

BoxReader::BoxReader(const BoxReader& other) = default;

BoxReader::~BoxReader() {
  if (scanned_ && !children_.empty()) {
    for (ChildMap::iterator itr = children_.begin();
         itr != children_.end(); ++itr) {
      DVLOG(1) << "Skipping unknown box: " << FourCCToString(itr->first);
    }
  }
}

// static
BoxReader* BoxReader::ReadTopLevelBox(const uint8_t* buf,
                                      const size_t buf_size,
                                      const scoped_refptr<MediaLog>& media_log,
                                      bool* err) {
  std::unique_ptr<BoxReader> reader(
      new BoxReader(buf, buf_size, media_log, false));
  if (!reader->ReadHeader(err))
    return NULL;

  // BoxReader::ReadHeader is expected to return false if box_size > buf_size.
  // This may happen if a partial mp4 box is appended, or if the box header is
  // corrupt.
  CHECK(reader->box_size() <= static_cast<uint64_t>(buf_size));

  if (!IsValidTopLevelBox(reader->type(), media_log)) {
    *err = true;
    return NULL;
  }

  return reader.release();
}

// static
bool BoxReader::StartTopLevelBox(const uint8_t* buf,
                                 const size_t buf_size,
                                 const scoped_refptr<MediaLog>& media_log,
                                 FourCC* type,
                                 size_t* box_size,
                                 bool* err) {
  BoxReader reader(buf, buf_size, media_log, false);
  if (!reader.ReadHeader(err)) return false;
  if (!IsValidTopLevelBox(reader.type(), media_log)) {
    *err = true;
    return false;
  }
  *type = reader.type();
  *box_size = reader.box_size();
  return true;
}

// static
BoxReader* BoxReader::ReadConcatentatedBoxes(const uint8_t* buf,
                                             const size_t buf_size) {
  BoxReader* reader = new BoxReader(buf, buf_size, new MediaLog(), true);

  // Concatenated boxes are passed in without a wrapping parent box. Set
  // |box_size_| to the concatenated buffer length to mimic having already
  // parsed the parent box.
  reader->box_size_ = buf_size;
  reader->box_size_known_ = true;

  return reader;
}

// static
bool BoxReader::IsValidTopLevelBox(const FourCC& type,
                                   const scoped_refptr<MediaLog>& media_log) {
  switch (type) {
    case FOURCC_FTYP:
    case FOURCC_PDIN:
    case FOURCC_BLOC:
    case FOURCC_MOOV:
    case FOURCC_MOOF:
    case FOURCC_MFRA:
    case FOURCC_MDAT:
    case FOURCC_FREE:
    case FOURCC_SKIP:
    case FOURCC_META:
    case FOURCC_MECO:
    case FOURCC_STYP:
    case FOURCC_SIDX:
    case FOURCC_SSIX:
    case FOURCC_PRFT:
    case FOURCC_UUID:
    case FOURCC_EMSG:
      return true;
    default:
      // Hex is used to show nonprintable characters and aid in debugging
      MEDIA_LOG(DEBUG, media_log) << "Unrecognized top-level box type "
                                  << FourCCToString(type);
      return false;
  }
}

bool BoxReader::ScanChildren() {
  // Must be able to trust box_size_ below.
  RCHECK(box_size_known_);

  DCHECK(!scanned_);
  scanned_ = true;

  bool err = false;
  while (pos_ < box_size_) {
    BoxReader child(&buf_[pos_], box_size_ - pos_, media_log_, is_EOS_);
    if (!child.ReadHeader(&err)) break;

    children_.insert(std::pair<FourCC, BoxReader>(child.type(), child));
    pos_ += child.box_size();
  }

  return !err && pos_ == box_size_;
}

bool BoxReader::HasChild(Box* child) {
  DCHECK(scanned_);
  DCHECK(child);
  return children_.count(child->BoxType()) > 0;
}

bool BoxReader::ReadChild(Box* child) {
  DCHECK(scanned_);
  FourCC child_type = child->BoxType();

  ChildMap::iterator itr = children_.find(child_type);
  RCHECK(itr != children_.end());
  DVLOG(2) << "Found a " << FourCCToString(child_type) << " box.";
  RCHECK(child->Parse(&itr->second));
  children_.erase(itr);
  return true;
}

bool BoxReader::MaybeReadChild(Box* child) {
  if (!children_.count(child->BoxType())) return true;
  return ReadChild(child);
}

bool BoxReader::ReadFullBoxHeader() {
  uint32_t vflags;
  RCHECK(Read4(&vflags));
  version_ = vflags >> 24;
  flags_ = vflags & 0xffffff;
  return true;
}

bool BoxReader::ReadHeader(bool* err) {
  uint64_t box_size = 0;
  *err = false;

  if (!HasBytes(8)) {
    // If EOS is known, then this is an error. If not, additional data may be
    // appended later, so this is a soft error.
    *err = is_EOS_;
    return false;
  }
  CHECK(Read4Into8(&box_size) && ReadFourCC(&type_));

  if (box_size == 0) {
    if (is_EOS_) {
      // All the data bytes are expected to be provided.
      box_size = base::checked_cast<uint64_t>(buf_size_);
    } else {
      MEDIA_LOG(DEBUG, media_log_)
          << "ISO BMFF boxes that run to EOS are not supported";
      *err = true;
      return false;
    }
  } else if (box_size == 1) {
    if (!HasBytes(8)) {
      // If EOS is known, then this is an error. If not, it's a soft error.
      *err = is_EOS_;
      return false;
    }
    CHECK(Read8(&box_size));
  }

  // Implementation-specific: support for boxes larger than 2^31 has been
  // removed.
  if (box_size < static_cast<uint64_t>(pos_) ||
      box_size > static_cast<uint64_t>(std::numeric_limits<int32_t>::max())) {
    *err = true;
    return false;
  }

  // Make sure the buffer contains at least the expected number of bytes.
  // Since the data may be appended in pieces, this is only an error if EOS.
  if (box_size > base::checked_cast<size_t>(buf_size_)) {
    *err = is_EOS_;
    return false;
  }

  // Note that the pos_ head has advanced to the byte immediately after the
  // header, which is where we want it.
  box_size_ = base::checked_cast<size_t>(box_size);
  box_size_known_ = true;
  return true;
}

}  // namespace mp4
}  // namespace media
