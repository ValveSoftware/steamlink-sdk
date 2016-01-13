// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP4_AVC_H_
#define MEDIA_FORMATS_MP4_AVC_H_

#include <vector>

#include "base/basictypes.h"
#include "media/base/media_export.h"

namespace media {

struct SubsampleEntry;

namespace mp4 {

struct AVCDecoderConfigurationRecord;

class MEDIA_EXPORT AVC {
 public:
  static bool ConvertFrameToAnnexB(int length_size, std::vector<uint8>* buffer);

  // Inserts the SPS & PPS data from |avc_config| into |buffer|.
  // |buffer| is expected to contain AnnexB conformant data.
  // |subsamples| contains the SubsampleEntry info if |buffer| contains
  // encrypted data.
  // Returns true if the param sets were successfully inserted.
  static bool InsertParamSetsAnnexB(
      const AVCDecoderConfigurationRecord& avc_config,
      std::vector<uint8>* buffer,
      std::vector<SubsampleEntry>* subsamples);

  static bool ConvertConfigToAnnexB(
      const AVCDecoderConfigurationRecord& avc_config,
      std::vector<uint8>* buffer,
      std::vector<SubsampleEntry>* subsamples);

  // Verifies that the contents of |buffer| conform to
  // Section 7.4.1.2.3 of ISO/IEC 14496-10.
  // Returns true if |buffer| contains conformant Annex B data
  // TODO(acolwell): Remove the std::vector version when we can use,
  // C++11's std::vector<T>::data() method.
  static bool IsValidAnnexB(const std::vector<uint8>& buffer);
  static bool IsValidAnnexB(const uint8* buffer, size_t size);
};

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_FORMATS_MP4_AVC_H_
