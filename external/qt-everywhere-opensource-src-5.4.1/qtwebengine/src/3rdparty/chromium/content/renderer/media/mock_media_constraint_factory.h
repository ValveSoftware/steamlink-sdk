// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_CONSTRAINT_H_
#define CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_CONSTRAINT_H_

#include <string>
#include <vector>

#include "third_party/WebKit/public/platform/WebMediaConstraints.h"

namespace content {

class MockMediaConstraintFactory {
 public:
  MockMediaConstraintFactory();
  ~MockMediaConstraintFactory();

  blink::WebMediaConstraints CreateWebMediaConstraints();
  void AddMandatory(const std::string& key, int value);
  void AddMandatory(const std::string& key, double value);
  void AddMandatory(const std::string& key, const std::string& value);
  void AddMandatory(const std::string& key, bool value);
  void AddOptional(const std::string& key, int value);
  void AddOptional(const std::string& key, double value);
  void AddOptional(const std::string& key, const std::string& value);
  void AddOptional(const std::string& key, bool value);
  void DisableDefaultAudioConstraints();

 private:
  std::vector<blink::WebMediaConstraint> mandatory_;
  std::vector<blink::WebMediaConstraint> optional_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_CONSTRAINT_H_
