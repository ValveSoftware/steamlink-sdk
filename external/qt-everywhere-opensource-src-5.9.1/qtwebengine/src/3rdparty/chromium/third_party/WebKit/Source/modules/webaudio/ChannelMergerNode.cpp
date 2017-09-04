/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/webaudio/ChannelMergerNode.h"
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "modules/webaudio/AudioNodeInput.h"
#include "modules/webaudio/AudioNodeOutput.h"
#include "modules/webaudio/BaseAudioContext.h"
#include "modules/webaudio/ChannelMergerOptions.h"

namespace blink {

ChannelMergerHandler::ChannelMergerHandler(AudioNode& node,
                                           float sampleRate,
                                           unsigned numberOfInputs)
    : AudioHandler(NodeTypeChannelMerger, node, sampleRate) {
  // These properties are fixed for the node and cannot be changed by user.
  m_channelCount = 1;
  setInternalChannelCountMode(Explicit);

  // Create the requested number of inputs.
  for (unsigned i = 0; i < numberOfInputs; ++i)
    addInput();

  // Create the output with the requested number of channels.
  addOutput(numberOfInputs);

  initialize();
}

PassRefPtr<ChannelMergerHandler> ChannelMergerHandler::create(
    AudioNode& node,
    float sampleRate,
    unsigned numberOfInputs) {
  return adoptRef(new ChannelMergerHandler(node, sampleRate, numberOfInputs));
}

void ChannelMergerHandler::process(size_t framesToProcess) {
  AudioNodeOutput& output = this->output(0);
  DCHECK_EQ(framesToProcess, output.bus()->length());

  unsigned numberOfOutputChannels = output.numberOfChannels();
  DCHECK_EQ(numberOfInputs(), numberOfOutputChannels);

  // Merge multiple inputs into one output.
  for (unsigned i = 0; i < numberOfOutputChannels; ++i) {
    AudioNodeInput& input = this->input(i);
    DCHECK_EQ(input.numberOfChannels(), 1u);
    AudioChannel* outputChannel = output.bus()->channel(i);
    if (input.isConnected()) {
      // The mixing rules will be applied so multiple channels are down-
      // mixed to mono (when the mixing rule is defined). Note that only
      // the first channel will be taken for the undefined input channel
      // layout.
      //
      // See:
      // http://webaudio.github.io/web-audio-api/#channel-up-mixing-and-down-mixing
      AudioChannel* inputChannel = input.bus()->channel(0);
      outputChannel->copyFrom(inputChannel);

    } else {
      // If input is unconnected, fill zeros in the channel.
      outputChannel->zero();
    }
  }
}

void ChannelMergerHandler::setChannelCount(unsigned long channelCount,
                                           ExceptionState& exceptionState) {
  DCHECK(isMainThread());
  BaseAudioContext::AutoLocker locker(context());

  // channelCount must be 1.
  if (channelCount != 1) {
    exceptionState.throwDOMException(
        InvalidStateError,
        "ChannelMerger: channelCount cannot be changed from 1");
  }
}

void ChannelMergerHandler::setChannelCountMode(const String& mode,
                                               ExceptionState& exceptionState) {
  DCHECK(isMainThread());
  BaseAudioContext::AutoLocker locker(context());

  // channcelCountMode must be 'explicit'.
  if (mode != "explicit") {
    exceptionState.throwDOMException(
        InvalidStateError,
        "ChannelMerger: channelCountMode cannot be changed from 'explicit'");
  }
}

// ----------------------------------------------------------------

ChannelMergerNode::ChannelMergerNode(BaseAudioContext& context,
                                     unsigned numberOfInputs)
    : AudioNode(context) {
  setHandler(ChannelMergerHandler::create(*this, context.sampleRate(),
                                          numberOfInputs));
}

ChannelMergerNode* ChannelMergerNode::create(BaseAudioContext& context,
                                             ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  // The default number of inputs for the merger node is 6.
  return create(context, 6, exceptionState);
}

ChannelMergerNode* ChannelMergerNode::create(BaseAudioContext& context,
                                             unsigned numberOfInputs,
                                             ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  if (context.isContextClosed()) {
    context.throwExceptionForClosedState(exceptionState);
    return nullptr;
  }

  if (!numberOfInputs ||
      numberOfInputs > BaseAudioContext::maxNumberOfChannels()) {
    exceptionState.throwDOMException(
        IndexSizeError, ExceptionMessages::indexOutsideRange<size_t>(
                            "number of inputs", numberOfInputs, 1,
                            ExceptionMessages::InclusiveBound,
                            BaseAudioContext::maxNumberOfChannels(),
                            ExceptionMessages::InclusiveBound));
    return nullptr;
  }

  return new ChannelMergerNode(context, numberOfInputs);
}

ChannelMergerNode* ChannelMergerNode::create(
    BaseAudioContext* context,
    const ChannelMergerOptions& options,
    ExceptionState& exceptionState) {
  ChannelMergerNode* node =
      create(*context, options.numberOfInputs(), exceptionState);

  if (!node)
    return nullptr;

  node->handleChannelOptions(options, exceptionState);

  return node;
}

}  // namespace blink
