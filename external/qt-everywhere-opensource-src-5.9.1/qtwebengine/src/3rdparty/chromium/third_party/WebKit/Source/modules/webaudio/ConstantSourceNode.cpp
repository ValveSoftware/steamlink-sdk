// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/ConstantSourceNode.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "modules/webaudio/AudioNodeOutput.h"
#include "modules/webaudio/ConstantSourceOptions.h"
#include "platform/audio/AudioUtilities.h"
#include "wtf/MathExtras.h"
#include "wtf/StdLibExtras.h"
#include <algorithm>

namespace blink {

ConstantSourceHandler::ConstantSourceHandler(AudioNode& node,
                                             float sampleRate,
                                             AudioParamHandler& offset)
    : AudioScheduledSourceHandler(NodeTypeConstantSource, node, sampleRate),
      m_offset(offset),
      m_sampleAccurateValues(AudioUtilities::kRenderQuantumFrames) {
  // A ConstantSource is always mono.
  addOutput(1);

  initialize();
}

PassRefPtr<ConstantSourceHandler> ConstantSourceHandler::create(
    AudioNode& node,
    float sampleRate,
    AudioParamHandler& offset) {
  return adoptRef(new ConstantSourceHandler(node, sampleRate, offset));
}

ConstantSourceHandler::~ConstantSourceHandler() {
  uninitialize();
}

void ConstantSourceHandler::process(size_t framesToProcess) {
  AudioBus* outputBus = output(0).bus();
  DCHECK(outputBus);

  if (!isInitialized() || !outputBus->numberOfChannels()) {
    outputBus->zero();
    return;
  }

  // The audio thread can't block on this lock, so we call tryLock() instead.
  MutexTryLocker tryLocker(m_processLock);
  if (!tryLocker.locked()) {
    // Too bad - the tryLock() failed.
    outputBus->zero();
    return;
  }

  size_t quantumFrameOffset;
  size_t nonSilentFramesToProcess;

  // Figure out where in the current rendering quantum that the source is
  // active and for how many frames.
  updateSchedulingInfo(framesToProcess, outputBus, quantumFrameOffset,
                       nonSilentFramesToProcess);

  if (!nonSilentFramesToProcess) {
    outputBus->zero();
    return;
  }

  if (m_offset->hasSampleAccurateValues()) {
    DCHECK_LE(framesToProcess, m_sampleAccurateValues.size());
    if (framesToProcess <= m_sampleAccurateValues.size()) {
      float* offsets = m_sampleAccurateValues.data();
      m_offset->calculateSampleAccurateValues(offsets, framesToProcess);
      if (nonSilentFramesToProcess > 0) {
        memcpy(outputBus->channel(0)->mutableData() + quantumFrameOffset,
               offsets + quantumFrameOffset,
               nonSilentFramesToProcess * sizeof(*offsets));
        outputBus->clearSilentFlag();
      } else {
        outputBus->zero();
      }
    }
  } else {
    float value = m_offset->value();

    if (value == 0) {
      outputBus->zero();
    } else {
      float* dest = outputBus->channel(0)->mutableData();
      dest += quantumFrameOffset;
      for (unsigned k = 0; k < nonSilentFramesToProcess; ++k) {
        dest[k] = value;
      }
      outputBus->clearSilentFlag();
    }
  }
}

bool ConstantSourceHandler::propagatesSilence() const {
  return !isPlayingOrScheduled() || hasFinished();
}

// ----------------------------------------------------------------
ConstantSourceNode::ConstantSourceNode(BaseAudioContext& context)
    : AudioScheduledSourceNode(context),
      m_offset(AudioParam::create(context, ParamTypeConstantSourceValue, 1)) {
  setHandler(ConstantSourceHandler::create(*this, context.sampleRate(),
                                           m_offset->handler()));
}

ConstantSourceNode* ConstantSourceNode::create(BaseAudioContext& context,
                                               ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  if (context.isContextClosed()) {
    context.throwExceptionForClosedState(exceptionState);
    return nullptr;
  }

  return new ConstantSourceNode(context);
}

ConstantSourceNode* ConstantSourceNode::create(
    BaseAudioContext* context,
    const ConstantSourceOptions& options,
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  ConstantSourceNode* node = create(*context, exceptionState);

  if (!node)
    return nullptr;

  node->offset()->setValue(options.offset());

  return node;
}

DEFINE_TRACE(ConstantSourceNode) {
  visitor->trace(m_offset);
  AudioScheduledSourceNode::trace(visitor);
}

ConstantSourceHandler& ConstantSourceNode::constantSourceHandler() const {
  return static_cast<ConstantSourceHandler&>(handler());
}

AudioParam* ConstantSourceNode::offset() {
  return m_offset;
}

}  // namespace blink
