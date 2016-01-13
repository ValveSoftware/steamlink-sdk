// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RTC_MEDIA_CONSTRAINTS_H_
#define CONTENT_RENDERER_MEDIA_RTC_MEDIA_CONSTRAINTS_H_

#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediaconstraintsinterface.h"

namespace blink {
class WebMediaConstraints;
}

namespace content {

// RTCMediaConstraints acts as a glue layer between WebKits MediaConstraints and
// libjingle webrtc::MediaConstraintsInterface.
// Constraints are used by PeerConnection and getUserMedia API calls.
class CONTENT_EXPORT RTCMediaConstraints
    : public NON_EXPORTED_BASE(webrtc::MediaConstraintsInterface) {
 public:
  RTCMediaConstraints();
  explicit RTCMediaConstraints(
      const blink::WebMediaConstraints& constraints);
  virtual ~RTCMediaConstraints();
  virtual const Constraints& GetMandatory() const OVERRIDE;
  virtual const Constraints& GetOptional() const OVERRIDE;
  // Adds a mandatory constraint, optionally overriding an existing one.
  // If the constraint is already set and |override_if_exists| is false,
  // the function will return false, otherwise true.
  bool AddMandatory(const std::string& key, const std::string& value,
                    bool override_if_exists);
  // As above, but against the optional constraints.
  bool AddOptional(const std::string& key, const std::string& value,
                   bool override_if_exists);

 protected:
  bool AddConstraint(Constraints* constraints,
                     const std::string& key,
                     const std::string& value,
                     bool override_if_exists);
  Constraints mandatory_;
  Constraints optional_;
};

}  // namespace content


#endif  // CONTENT_RENDERER_MEDIA_RTC_MEDIA_CONSTRAINTS_H_
