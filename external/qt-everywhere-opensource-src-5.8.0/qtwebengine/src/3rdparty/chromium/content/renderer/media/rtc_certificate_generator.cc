// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_certificate_generator.h"

#include <string>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "content/renderer/media/rtc_certificate.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/webrtc/base/rtccertificate.h"
#include "third_party/webrtc/base/rtccertificategenerator.h"
#include "third_party/webrtc/base/scoped_ref_ptr.h"
#include "url/gurl.h"

namespace content {
namespace {

rtc::KeyParams WebRTCKeyParamsToKeyParams(
    const blink::WebRTCKeyParams& key_params) {
  switch (key_params.keyType()) {
    case blink::WebRTCKeyTypeRSA:
      return rtc::KeyParams::RSA(key_params.rsaParams().modLength,
                                 key_params.rsaParams().pubExp);
    case blink::WebRTCKeyTypeECDSA:
      return rtc::KeyParams::ECDSA(
          static_cast<rtc::ECCurve>(key_params.ecCurve()));
    default:
      NOTREACHED();
      return rtc::KeyParams();
  }
}

// A certificate generation request spawned by
// |RTCCertificateGenerator::generateCertificateWithOptionalExpiration|. This
// is handled by a separate class so that reference counting can keep the
// request alive independently of the |RTCCertificateGenerator| that spawned it.
class RTCCertificateGeneratorRequest
    : public base::RefCountedThreadSafe<RTCCertificateGeneratorRequest> {
 public:
  RTCCertificateGeneratorRequest(
      const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
      const scoped_refptr<base::SingleThreadTaskRunner>& worker_thread)
      : main_thread_(main_thread),
        worker_thread_(worker_thread) {
    DCHECK(main_thread_);
    DCHECK(worker_thread_);
  }

  void GenerateCertificateAsync(
      const blink::WebRTCKeyParams& key_params,
      const rtc::Optional<uint64_t>& expires_ms,
      std::unique_ptr<blink::WebRTCCertificateCallback> observer) {
    DCHECK(main_thread_->BelongsToCurrentThread());
    DCHECK(observer);
    worker_thread_->PostTask(FROM_HERE, base::Bind(
        &RTCCertificateGeneratorRequest::GenerateCertificateOnWorkerThread,
        this,
        key_params,
        expires_ms,
        base::Passed(std::move(observer))));
  }

 private:
  friend class base::RefCountedThreadSafe<RTCCertificateGeneratorRequest>;
  ~RTCCertificateGeneratorRequest() {}

  void GenerateCertificateOnWorkerThread(
      const blink::WebRTCKeyParams key_params,
      const rtc::Optional<uint64_t> expires_ms,
      std::unique_ptr<blink::WebRTCCertificateCallback> observer) {
    DCHECK(worker_thread_->BelongsToCurrentThread());

    rtc::scoped_refptr<rtc::RTCCertificate> certificate =
        rtc::RTCCertificateGenerator::GenerateCertificate(
            WebRTCKeyParamsToKeyParams(key_params), expires_ms);

    main_thread_->PostTask(FROM_HERE, base::Bind(
        &RTCCertificateGeneratorRequest::DoCallbackOnMainThread,
        this,
        base::Passed(std::move(observer)),
        base::Passed(base::WrapUnique(new RTCCertificate(certificate)))));
  }

  void DoCallbackOnMainThread(
      std::unique_ptr<blink::WebRTCCertificateCallback> observer,
      std::unique_ptr<blink::WebRTCCertificate> certificate) {
    DCHECK(main_thread_->BelongsToCurrentThread());
    DCHECK(observer);
    if (certificate)
      observer->onSuccess(std::move(certificate));
    else
      observer->onError();
  }

  // The main thread is the renderer thread.
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  // The WebRTC worker thread.
  const scoped_refptr<base::SingleThreadTaskRunner> worker_thread_;
};

}  // namespace

void RTCCertificateGenerator::generateCertificate(
    const blink::WebRTCKeyParams& key_params,
    std::unique_ptr<blink::WebRTCCertificateCallback> observer) {
  generateCertificateWithOptionalExpiration(
      key_params, rtc::Optional<uint64_t>(), std::move(observer));
}

void RTCCertificateGenerator::generateCertificateWithExpiration(
    const blink::WebRTCKeyParams& key_params,
    uint64_t expires_ms,
    std::unique_ptr<blink::WebRTCCertificateCallback> observer) {
  generateCertificateWithOptionalExpiration(
    key_params, rtc::Optional<uint64_t>(expires_ms), std::move(observer));
}

void RTCCertificateGenerator::generateCertificateWithOptionalExpiration(
    const blink::WebRTCKeyParams& key_params,
    const rtc::Optional<uint64_t>& expires_ms,
    std::unique_ptr<blink::WebRTCCertificateCallback> observer) {
  DCHECK(isSupportedKeyParams(key_params));
#if defined(ENABLE_WEBRTC)
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread =
      base::ThreadTaskRunnerHandle::Get();
  PeerConnectionDependencyFactory* pc_dependency_factory =
      RenderThreadImpl::current()->GetPeerConnectionDependencyFactory();
  pc_dependency_factory->EnsureInitialized();

  scoped_refptr<RTCCertificateGeneratorRequest> request =
      new RTCCertificateGeneratorRequest(
          main_thread,
          pc_dependency_factory->GetWebRtcWorkerThread());
  request->GenerateCertificateAsync(
      key_params, expires_ms, std::move(observer));
#else
  observer->onError();
#endif
}

bool RTCCertificateGenerator::isSupportedKeyParams(
    const blink::WebRTCKeyParams& key_params) {
  return WebRTCKeyParamsToKeyParams(key_params).IsValid();
}

std::unique_ptr<blink::WebRTCCertificate> RTCCertificateGenerator::fromPEM(
    const std::string& pem_private_key,
    const std::string& pem_certificate) {
  rtc::scoped_refptr<rtc::RTCCertificate> certificate =
      rtc::RTCCertificate::FromPEM(
          rtc::RTCCertificatePEM(pem_private_key, pem_certificate));
  return std::unique_ptr<blink::WebRTCCertificate>(
      new RTCCertificate(certificate));
}

}  // namespace content
