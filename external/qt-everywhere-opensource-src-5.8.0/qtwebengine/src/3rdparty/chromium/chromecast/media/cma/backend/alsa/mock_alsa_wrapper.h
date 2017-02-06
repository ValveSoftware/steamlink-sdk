// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ALSA_MOCK_ALSA_WRAPPER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ALSA_MOCK_ALSA_WRAPPER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "chromecast/media/cma/backend/alsa/alsa_wrapper.h"
#include "media/audio/alsa/alsa_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromecast {
namespace media {

class MockAlsaWrapper : public AlsaWrapper {
 public:
  MockAlsaWrapper();
  ~MockAlsaWrapper() override;

  MOCK_METHOD3(DeviceNameHint, int(int card, const char* iface, void*** hints));
  MOCK_METHOD2(DeviceNameGetHint, char*(const void* hint, const char* id));
  MOCK_METHOD1(DeviceNameFreeHint, int(void** hints));

  MOCK_METHOD4(PcmOpen,
               int(snd_pcm_t** handle,
                   const char* name,
                   snd_pcm_stream_t stream,
                   int mode));
  MOCK_METHOD1(PcmClose, int(snd_pcm_t* handle));
  MOCK_METHOD1(PcmPrepare, int(snd_pcm_t* handle));
  MOCK_METHOD1(PcmDrain, int(snd_pcm_t* handle));
  MOCK_METHOD1(PcmDrop, int(snd_pcm_t* handle));
  MOCK_METHOD2(PcmDelay, int(snd_pcm_t* handle, snd_pcm_sframes_t* delay));
  MOCK_METHOD3(PcmWritei,
               snd_pcm_sframes_t(snd_pcm_t* handle,
                                 const void* buffer,
                                 snd_pcm_uframes_t size));
  MOCK_METHOD3(PcmReadi,
               snd_pcm_sframes_t(snd_pcm_t* handle,
                                 void* buffer,
                                 snd_pcm_uframes_t size));
  MOCK_METHOD3(PcmRecover, int(snd_pcm_t* handle, int err, int silent));
  MOCK_METHOD7(PcmSetParams,
               int(snd_pcm_t* handle,
                   snd_pcm_format_t format,
                   snd_pcm_access_t access,
                   unsigned int channels,
                   unsigned int rate,
                   int soft_resample,
                   unsigned int latency));
  MOCK_METHOD3(PcmGetParams,
               int(snd_pcm_t* handle,
                   snd_pcm_uframes_t* buffer_size,
                   snd_pcm_uframes_t* period_size));
  MOCK_METHOD1(PcmName, const char*(snd_pcm_t* handle));
  MOCK_METHOD1(PcmAvailUpdate, snd_pcm_sframes_t(snd_pcm_t* handle));
  MOCK_METHOD1(PcmState, snd_pcm_state_t(snd_pcm_t* handle));
  MOCK_METHOD1(PcmStart, int(snd_pcm_t* handle));
  MOCK_METHOD2(PcmFormatSize, ssize_t(snd_pcm_format_t format, size_t samples));
  MOCK_METHOD1(StrError, const char*(int errnum));

  MOCK_METHOD2(PcmPause, int(snd_pcm_t* handle, int enable));

  MOCK_METHOD1(PcmStatusMalloc, int(snd_pcm_status_t** ptr));
  MOCK_METHOD1(PcmStatusFree, void(snd_pcm_status_t* obj));
  MOCK_METHOD2(PcmStatus, int(snd_pcm_t* handle, snd_pcm_status_t* status));
  MOCK_METHOD1(PcmStatusGetDelay,
               snd_pcm_sframes_t(const snd_pcm_status_t* obj));
  MOCK_METHOD2(PcmStatusGetHtstamp,
               void(const snd_pcm_status_t* obj, snd_htimestamp_t* ptr));

  MOCK_METHOD1(PcmHwParamsMalloc, int(snd_pcm_hw_params_t** ptr));
  MOCK_METHOD1(PcmHwParamsFree, void(snd_pcm_hw_params_t* obj));
  MOCK_METHOD2(PcmHwParamsCurrent,
               int(snd_pcm_t* handle, snd_pcm_hw_params_t* params));
  MOCK_METHOD1(PcmHwParamsCanPause, int(const snd_pcm_hw_params_t* params));
  MOCK_METHOD2(PcmHwParamsAny,
               int(snd_pcm_t* handle, snd_pcm_hw_params_t* params));
  MOCK_METHOD3(PcmHwParamsSetRateResample,
               int(snd_pcm_t* handle, snd_pcm_hw_params_t* params, bool val));
  MOCK_METHOD3(PcmHwParamsSetAccess,
               int(snd_pcm_t* handle,
                   snd_pcm_hw_params_t* params,
                   snd_pcm_access_t access));
  MOCK_METHOD3(PcmHwParamsSetFormat,
               int(snd_pcm_t* handle,
                   snd_pcm_hw_params_t* params,
                   snd_pcm_format_t format));
  MOCK_METHOD3(PcmHwParamsSetChannels,
               int(snd_pcm_t* handle,
                   snd_pcm_hw_params_t* params,
                   unsigned int num_channels));
  MOCK_METHOD4(PcmHwParamsSetRateNear,
               int(snd_pcm_t* handle,
                   snd_pcm_hw_params_t* params,
                   unsigned int* rate,
                   int* dir));
  MOCK_METHOD3(PcmHwParamsSetBufferSizeNear,
               int(snd_pcm_t* handle,
                   snd_pcm_hw_params_t* params,
                   snd_pcm_uframes_t* val));
  MOCK_METHOD4(PcmHwParamsSetPeriodSizeNear,
               int(snd_pcm_t* handle,
                   snd_pcm_hw_params_t* params,
                   snd_pcm_uframes_t* val,
                   int* dir));
  MOCK_METHOD3(PcmHwParamsTestFormat,
               int(snd_pcm_t* handle,
                   snd_pcm_hw_params_t* params,
                   snd_pcm_format_t format));
  MOCK_METHOD4(PcmHwParamsTestRate,
               int(snd_pcm_t* handle,
                   snd_pcm_hw_params_t* params,
                   unsigned int rate,
                   int dir));
  MOCK_METHOD2(PcmHwParams,
               int(snd_pcm_t* handle, snd_pcm_hw_params_t* params));

  MOCK_METHOD1(PcmSwParamsMalloc, int(snd_pcm_sw_params_t** params));
  MOCK_METHOD1(PcmSwParamsFree, void(snd_pcm_sw_params_t* params));
  MOCK_METHOD2(PcmSwParamsCurrent,
               int(snd_pcm_t* handle, snd_pcm_sw_params_t* params));
  MOCK_METHOD3(PcmSwParamsSetStartThreshold,
               int(snd_pcm_t* handle,
                   snd_pcm_sw_params_t* params,
                   snd_pcm_uframes_t val));
  MOCK_METHOD3(PcmSwParamsSetAvailMin,
               int(snd_pcm_t* handle,
                   snd_pcm_sw_params_t* params,
                   snd_pcm_uframes_t val));
  MOCK_METHOD3(PcmSwParamsSetTstampMode,
               int(snd_pcm_t* handle,
                   snd_pcm_sw_params_t* obj,
                   snd_pcm_tstamp_t val));
  MOCK_METHOD3(PcmSwParamsSetTstampType,
               int(snd_pcm_t* handle, snd_pcm_sw_params_t* obj, int val));
  MOCK_METHOD2(PcmSwParams, int(snd_pcm_t* handle, snd_pcm_sw_params_t* obj));

  // Getters for test control.
  snd_pcm_state_t state();
  const std::vector<uint8_t>& data();

 private:
  class FakeAlsaWrapper;

  // Certain calls will be delegated to this class.
  std::unique_ptr<FakeAlsaWrapper> fake_;

  DISALLOW_COPY_AND_ASSIGN(MockAlsaWrapper);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ALSA_MOCK_ALSA_WRAPPER_H_
