// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/icc_profile.h"

#include <list>

#include "base/containers/mru_cache.h"
#include "base/lazy_instance.h"
#include "base/synchronization/lock.h"
#include "ui/gfx/color_transform.h"

namespace gfx {

namespace {
const size_t kMinProfileLength = 128;
const size_t kMaxProfileLength = 4 * 1024 * 1024;

// Allow keeping around a maximum of 8 cached ICC profiles. Beware that
// we will do a linear search thorugh currently-cached ICC profiles,
// when creating a new ICC profile.
const size_t kMaxCachedICCProfiles = 8;

struct Cache {
  Cache() : id_to_icc_profile_mru(kMaxCachedICCProfiles) {}
  ~Cache() {}

  // Start from-ICC-data IDs at the end of the hard-coded list.
  uint64_t next_unused_id = 5;
  base::MRUCache<uint64_t, ICCProfile> id_to_icc_profile_mru;
  base::Lock lock;
};
static base::LazyInstance<Cache> g_cache;

}  // namespace

ICCProfile::ICCProfile() = default;
ICCProfile::ICCProfile(ICCProfile&& other) = default;
ICCProfile::ICCProfile(const ICCProfile& other) = default;
ICCProfile& ICCProfile::operator=(ICCProfile&& other) = default;
ICCProfile& ICCProfile::operator=(const ICCProfile& other) = default;
ICCProfile::~ICCProfile() = default;

bool ICCProfile::operator==(const ICCProfile& other) const {
  if (type_ != other.type_)
    return false;
  switch (type_) {
    case Type::INVALID:
      return true;
    case Type::FROM_COLOR_SPACE:
      return color_space_ == other.color_space_;
    case Type::FROM_DATA:
      return data_ == other.data_;
  }
  return false;
}

// static
ICCProfile ICCProfile::FromData(const char* data, size_t size) {
  ICCProfile icc_profile;
  if (IsValidProfileLength(size)) {
    icc_profile.type_ = Type::FROM_DATA;
    icc_profile.data_.insert(icc_profile.data_.begin(), data, data + size);
  } else {
    return ICCProfile();
  }

  Cache& cache = g_cache.Get();
  base::AutoLock lock(cache.lock);

  // Linearly search the cached ICC profiles to find one with the same data.
  // If it exists, re-use its id and touch it in the cache.
  for (auto iter = cache.id_to_icc_profile_mru.begin();
       iter != cache.id_to_icc_profile_mru.end(); ++iter) {
    if (icc_profile.data_ == iter->second.data_) {
      icc_profile.id_ = iter->second.id_;
      cache.id_to_icc_profile_mru.Get(icc_profile.id_);
      return icc_profile;
    }
  }

  // Create a new cached id and add it to the cache.
  icc_profile.id_ = cache.next_unused_id++;
  cache.id_to_icc_profile_mru.Put(icc_profile.id_, icc_profile);
  return icc_profile;
}

#if !defined(OS_WIN) && !defined(OS_MACOSX) && !defined(USE_X11)
// static
ICCProfile ICCProfile::FromBestMonitor() {
  return ICCProfile();
}
#endif

// static
ICCProfile ICCProfile::FromColorSpace(const gfx::ColorSpace& color_space) {
  if (color_space == gfx::ColorSpace())
    return ICCProfile();

  // If |color_space| was created from an ICC profile, retrieve that exact
  // profile.
  if (color_space.icc_profile_id_) {
    Cache& cache = g_cache.Get();
    base::AutoLock lock(cache.lock);

    auto found = cache.id_to_icc_profile_mru.Get(color_space.icc_profile_id_);
    if (found != cache.id_to_icc_profile_mru.end()) {
      return found->second;
    }
  }

  // TODO(ccameron): Support constructing ICC profiles from arbitrary ColorSpace
  // objects.
  ICCProfile icc_profile;
  icc_profile.type_ = gfx::ICCProfile::Type::FROM_COLOR_SPACE;
  icc_profile.color_space_ = color_space;
  return icc_profile;
}

const std::vector<char>& ICCProfile::GetData() const {
  return data_;
}

ColorSpace ICCProfile::GetColorSpace() const {
  if (type_ == Type::INVALID)
    return gfx::ColorSpace();
  if (type_ == Type::FROM_COLOR_SPACE)
    return color_space_;

  ColorSpace color_space(ColorSpace::PrimaryID::CUSTOM,
                         ColorSpace::TransferID::CUSTOM,
                         ColorSpace::MatrixID::RGB, ColorSpace::RangeID::FULL);
  color_space.icc_profile_id_ = id_;

  // Move this ICC profile to the most recently used end of the cache.
  {
    Cache& cache = g_cache.Get();
    base::AutoLock lock(cache.lock);

    auto found = cache.id_to_icc_profile_mru.Get(id_);
    if (found == cache.id_to_icc_profile_mru.end())
      cache.id_to_icc_profile_mru.Put(id_, *this);
  }

  ColorSpace unity_colorspace(
      ColorSpace::PrimaryID::CUSTOM, ColorSpace::TransferID::LINEAR,
      ColorSpace::MatrixID::RGB, ColorSpace::RangeID::FULL);
  unity_colorspace.custom_primary_matrix_[0] = 1.0f;
  unity_colorspace.custom_primary_matrix_[1] = 0.0f;
  unity_colorspace.custom_primary_matrix_[2] = 0.0f;
  unity_colorspace.custom_primary_matrix_[3] = 0.0f;

  unity_colorspace.custom_primary_matrix_[4] = 0.0f;
  unity_colorspace.custom_primary_matrix_[5] = 1.0f;
  unity_colorspace.custom_primary_matrix_[6] = 0.0f;
  unity_colorspace.custom_primary_matrix_[7] = 0.0f;

  unity_colorspace.custom_primary_matrix_[8] = 0.0f;
  unity_colorspace.custom_primary_matrix_[9] = 0.0f;
  unity_colorspace.custom_primary_matrix_[10] = 1.0f;
  unity_colorspace.custom_primary_matrix_[11] = 0.0f;

  // This will look up and use the ICC profile.
  std::unique_ptr<ColorTransform> transform(ColorTransform::NewColorTransform(
      color_space, unity_colorspace, ColorTransform::Intent::INTENT_ABSOLUTE));

  ColorTransform::TriStim tmp[4];
  tmp[0].set_x(1.0f);
  tmp[1].set_y(1.0f);
  tmp[2].set_z(1.0f);
  transform->transform(tmp, arraysize(tmp));

  color_space.custom_primary_matrix_[0] = tmp[0].x() - tmp[3].x();
  color_space.custom_primary_matrix_[1] = tmp[1].x() - tmp[3].x();
  color_space.custom_primary_matrix_[2] = tmp[2].x() - tmp[3].x();
  color_space.custom_primary_matrix_[3] = tmp[3].x();

  color_space.custom_primary_matrix_[4] = tmp[0].y() - tmp[3].y();
  color_space.custom_primary_matrix_[5] = tmp[1].y() - tmp[3].y();
  color_space.custom_primary_matrix_[6] = tmp[2].y() - tmp[3].y();
  color_space.custom_primary_matrix_[7] = tmp[3].y();

  color_space.custom_primary_matrix_[8] = tmp[0].z() - tmp[3].z();
  color_space.custom_primary_matrix_[9] = tmp[1].z() - tmp[3].z();
  color_space.custom_primary_matrix_[10] = tmp[2].z() - tmp[3].z();
  color_space.custom_primary_matrix_[11] = tmp[3].z();

  return color_space;
}

// static
bool ICCProfile::IsValidProfileLength(size_t length) {
  return length >= kMinProfileLength && length <= kMaxProfileLength;
}

}  // namespace gfx
