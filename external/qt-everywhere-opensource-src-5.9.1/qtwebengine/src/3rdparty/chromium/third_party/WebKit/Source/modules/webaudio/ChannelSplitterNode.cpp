/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "modules/webaudio/ChannelSplitterNode.h"
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "modules/webaudio/AudioNodeInput.h"
#include "modules/webaudio/AudioNodeOutput.h"
#include "modules/webaudio/BaseAudioContext.h"
#include "modules/webaudio/ChannelSplitterOptions.h"

namespace blink {

ChannelSplitterHandler::ChannelSplitterHandler(AudioNode& node,
                                               float sampleRate,
                                               unsigned numberOfOutputs)
    : AudioHandler(NodeTypeChannelSplitter, node, sampleRate) {
  // These properties are fixed and cannot be changed by the user.
  m_channelCount = numberOfOutputs;
  setInternalChannelCountMode(Explicit);
  addInput();

  // Create a fixed number of outputs (able to handle the maximum number of
  // channels fed to an input).
  for (unsigned i = 0; i < numberOfOutputs; ++i)
    addOutput(1);

  initialize();
}

PassRefPtr<ChannelSplitterHandler> ChannelSplitterHandler::create(
    AudioNode& node,
    float sampleRate,
    unsigned numberOfOutputs) {
  return adoptRef(
      new ChannelSplitterHandler(node, sampleRate, numberOfOutputs));
}

void ChannelSplitterHandler::process(size_t framesToProcess) {
  AudioBus* source = input(0).bus();
  DCHECK(source);
  DCHECK_EQ(framesToProcess, source->length());

  unsigned numberOfSourceChannels = source->numberOfChannels();

  for (unsigned i = 0; i < numberOfOutputs(); ++i) {
    AudioBus* destination = output(i).bus();
    DCHECK(destination);

    if (i < numberOfSourceChannels) {
      // Split the channel out if it exists in the source.
      // It would be nice to avoid the copy and simply pass along pointers, but
      // this becomes extremely difficult with fanout and fanin.
      destination->channel(0)->copyFrom(source->channel(i));
    } else if (output(i).renderingFanOutCount() > 0) {
      // Only bother zeroing out the destination if it's connected to anything
      destination->zero();
    }
  }
}

void ChannelSplitterHandler::setChannelCount(unsigned long channelCount,
                                             ExceptionState& exceptionState) {
  DCHECK(isMainThread());
  BaseAudioContext::AutoLocker locker(context());

  // channelCount cannot be changed from the number of outputs.
  if (channelCount != numberOfOutputs()) {
    exceptionState.throwDOMException(
        InvalidStateError,
        "ChannelSplitter: channelCount cannot be changed from " +
            String::number(numberOfOutputs()));
  }
}

void ChannelSplitterHandler::setChannelCountMode(
    const String& mode,
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());
  BaseAudioContext::AutoLocker locker(context());

  // channcelCountMode must be 'explicit'.
  if (mode != "explicit") {
    exceptionState.throwDOMException(
        InvalidStateError,
        "ChannelSplitter: channelCountMode cannot be changed from 'explicit'");
  }
}

// ----------------------------------------------------------------

ChannelSplitterNode::ChannelSplitterNode(BaseAudioContext& context,
                                         unsigned numberOfOutputs)
    : AudioNode(context) {
  setHandler(ChannelSplitterHandler::create(*this, context.sampleRate(),
                                            numberOfOutputs));
}

ChannelSplitterNode* ChannelSplitterNode::create(
    BaseAudioContext& context,
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  // Default number of outputs for the splitter node is 6.
  return create(context, 6, exceptionState);
}

ChannelSplitterNode* ChannelSplitterNode::create(
    BaseAudioContext& context,
    unsigned numberOfOutputs,
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  if (context.isContextClosed()) {
    context.throwExceptionForClosedState(exceptionState);
    return nullptr;
  }

  if (!numberOfOutputs ||
      numberOfOutputs > BaseAudioContext::maxNumberOfChannels()) {
    exceptionState.throwDOMException(
        IndexSizeError, ExceptionMessages::indexOutsideRange<size_t>(
                            "number of outputs", numberOfOutputs, 1,
                            ExceptionMessages::InclusiveBound,
                            BaseAudioContext::maxNumberOfChannels(),
                            ExceptionMessages::InclusiveBound));
    return nullptr;
  }

  return new ChannelSplitterNode(context, numberOfOutputs);
}

ChannelSplitterNode* ChannelSplitterNode::create(
    BaseAudioContext* context,
    const ChannelSplitterOptions& options,
    ExceptionState& exceptionState) {
  ChannelSplitterNode* node =
      create(*context, options.numberOfOutputs(), exceptionState);

  if (!node)
    return nullptr;

  node->handleChannelOptions(options, exceptionState);

  return node;
}

}  // namespace blink
