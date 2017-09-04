// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ConstantSourceNode_h
#define ConstantSourceNode_h

#include "modules/webaudio/AudioParam.h"
#include "modules/webaudio/AudioScheduledSourceNode.h"
#include "platform/audio/AudioBus.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Threading.h"

namespace blink {

class BaseAudioContext;
class ConstantSourceOptions;
class ExceptionState;

// ConstantSourceNode is an audio generator for a constant source

class ConstantSourceHandler final : public AudioScheduledSourceHandler {
 public:
  static PassRefPtr<ConstantSourceHandler> create(AudioNode&,
                                                  float sampleRate,
                                                  AudioParamHandler& offset);
  ~ConstantSourceHandler() override;

  // AudioHandler
  void process(size_t framesToProcess) override;

 private:
  ConstantSourceHandler(AudioNode&,
                        float sampleRate,
                        AudioParamHandler& offset);

  // If we are no longer playing, propogate silence ahead to downstream nodes.
  bool propagatesSilence() const override;

  RefPtr<AudioParamHandler> m_offset;
  AudioFloatArray m_sampleAccurateValues;
};

class ConstantSourceNode final : public AudioScheduledSourceNode {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ConstantSourceNode* create(BaseAudioContext&, ExceptionState&);
  static ConstantSourceNode* create(BaseAudioContext*,
                                    const ConstantSourceOptions&,
                                    ExceptionState&);
  DECLARE_VIRTUAL_TRACE();

  AudioParam* offset();

 private:
  ConstantSourceNode(BaseAudioContext&);
  ConstantSourceHandler& constantSourceHandler() const;

  Member<AudioParam> m_offset;
};

}  // namespace blink

#endif  // ConstantSourceNode_h
