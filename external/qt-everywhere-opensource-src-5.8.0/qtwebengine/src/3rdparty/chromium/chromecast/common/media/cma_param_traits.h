// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_COMMON_MEDIA_CMA_PARAM_TRAITS_H_
#define CHROMECAST_COMMON_MEDIA_CMA_PARAM_TRAITS_H_

#include "ipc/ipc_message_utils.h"

namespace media {
class AudioDecoderConfig;
class VideoDecoderConfig;
class EncryptionScheme;
}

namespace IPC {

template <>
struct ParamTraits<media::AudioDecoderConfig> {
  typedef media::AudioDecoderConfig param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<media::VideoDecoderConfig> {
  typedef media::VideoDecoderConfig param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<media::EncryptionScheme> {
  typedef media::EncryptionScheme param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m, base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CHROMECAST_COMMON_MEDIA_CMA_PARAM_TRAITS_H_
