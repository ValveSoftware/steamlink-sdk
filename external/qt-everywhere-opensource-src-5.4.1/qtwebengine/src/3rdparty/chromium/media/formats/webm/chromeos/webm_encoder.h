// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_WEBM_CHROMEOS_WEBM_ENCODER_H_
#define MEDIA_FORMATS_WEBM_CHROMEOS_WEBM_ENCODER_H_

#include <stdio.h>
#include <stack>

#include "base/files/file_path.h"
#include "media/base/media_export.h"
#include "media/formats/webm/chromeos/ebml_writer.h"

extern "C" {
#define VPX_CODEC_DISABLE_COMPAT 1
#include "third_party/libvpx/source/libvpx/vpx/vpx_encoder.h"
#include "third_party/libvpx/source/libvpx/vpx/vp8cx.h"
}

class SkBitmap;

namespace base {
class FilePath;
}

namespace media {

namespace chromeos {

// WebM encoder using libvpx. Currently only supports one-pass, constant bitrate
// encoding of short files consisting of a single video track. Seek info and
// cues are not supported, so generated .webm file does not strictly adhere to
// WebM standard (http://www.webmproject.org/code/specs/container/).
class MEDIA_EXPORT WebmEncoder {
 public:
  // Create new instance for writing to |output_path|. If |realtime| is |true|,
  // uses realtime deadline, otherwise - "good quality" deadline.
  WebmEncoder(const base::FilePath& output_path, int bitrate, bool realtime);
  ~WebmEncoder();

  // Encodes video from a Nx(N*M) sprite, having M frames of size NxN with FPS
  // |fps_n/fps_d|. Must be called on a thread that allows disk IO.
  // Returns |true| iff encoding and writing to file is successful.
  bool EncodeFromSprite(const SkBitmap& sprite, int fps_n, int fps_d);

 private:
  // Writes global WebM header and starts a single video track. Returns |false|
  // if there was an error opening file for writing.
  bool WriteWebmHeader();

  // Writes VPX packet to output file.
  void WriteWebmBlock(const vpx_codec_cx_pkt_t* packet);

  // Finishes video track and closes output file. Returns |false| if there were
  // any error during encoding/writing file.
  bool WriteWebmFooter();

  // Starts a new WebM sub-element of given type. Those can be nested.
  void StartSubElement(unsigned long class_id);

  // Closes current top-level sub-element.
  void EndSubElement();

  // libmkv callbacks.
  void EbmlWrite(const void* buffer, unsigned long len);
  void EbmlSerialize(const void* buffer, int buffer_size, unsigned long len);

  template <typename T>
  void EbmlSerializeHelper(const T* buffer, unsigned long len);

  // Video dimensions and FPS.
  size_t width_;
  size_t height_;
  vpx_rational_t fps_;

  // Number of frames in video.
  size_t frame_count_;

  // VPX config in use.
  vpx_codec_enc_cfg_t config_;

  // VPX parameters.
  int bitrate_;
  unsigned long deadline_;

  // EbmlWriter context.
  EbmlGlobal ebml_writer_;

  // Stack with start offsets of currently open sub-elements.
  std::stack<long int> ebml_sub_elements_;

  base::FilePath output_path_;
  FILE* output_;

  // True if an error occured while encoding/writing to file.
  bool has_errors_;

  DISALLOW_COPY_AND_ASSIGN(WebmEncoder);
};

}  // namespace chromeos

}  // namespace media

#endif  // MEDIA_FORMATS_WEBM_CHROMEOS_WEBM_ENCODER_H_
