/*
 * Copyright (C) 2013, Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#if ENABLE(WEB_AUDIO)
#include "bindings/modules/v8/V8AudioNode.h"

#include "bindings/modules/v8/V8AnalyserNode.h"
#include "bindings/modules/v8/V8AudioBufferSourceNode.h"
#include "bindings/modules/v8/V8AudioDestinationNode.h"
#include "bindings/modules/v8/V8BiquadFilterNode.h"
#include "bindings/modules/v8/V8ChannelMergerNode.h"
#include "bindings/modules/v8/V8ChannelSplitterNode.h"
#include "bindings/modules/v8/V8ConvolverNode.h"
#include "bindings/modules/v8/V8DelayNode.h"
#include "bindings/modules/v8/V8DynamicsCompressorNode.h"
#include "bindings/modules/v8/V8GainNode.h"
#include "bindings/modules/v8/V8MediaElementAudioSourceNode.h"
#include "bindings/modules/v8/V8MediaStreamAudioDestinationNode.h"
#include "bindings/modules/v8/V8MediaStreamAudioSourceNode.h"
#include "bindings/modules/v8/V8OscillatorNode.h"
#include "bindings/modules/v8/V8PannerNode.h"
#include "bindings/modules/v8/V8ScriptProcessorNode.h"
#include "bindings/modules/v8/V8WaveShaperNode.h"
#include "bindings/v8/V8Binding.h"
#include "modules/webaudio/AnalyserNode.h"
#include "modules/webaudio/AudioBufferSourceNode.h"
#include "modules/webaudio/AudioDestinationNode.h"
#include "modules/webaudio/AudioNode.h"
#include "modules/webaudio/BiquadFilterNode.h"
#include "modules/webaudio/ChannelMergerNode.h"
#include "modules/webaudio/ChannelSplitterNode.h"
#include "modules/webaudio/ConvolverNode.h"
#include "modules/webaudio/DelayNode.h"
#include "modules/webaudio/DynamicsCompressorNode.h"
#include "modules/webaudio/GainNode.h"
#include "modules/webaudio/MediaElementAudioSourceNode.h"
#include "modules/webaudio/MediaStreamAudioDestinationNode.h"
#include "modules/webaudio/MediaStreamAudioSourceNode.h"
#include "modules/webaudio/OscillatorNode.h"
#include "modules/webaudio/PannerNode.h"
#include "modules/webaudio/ScriptProcessorNode.h"
#include "modules/webaudio/WaveShaperNode.h"

namespace WebCore {

v8::Handle<v8::Object> wrap(AudioNode* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(impl);
    switch (impl->nodeType()) {
    case AudioNode::NodeTypeDestination:
        return wrap(static_cast<AudioDestinationNode*>(impl), creationContext, isolate);
    case AudioNode::NodeTypeOscillator:
        return wrap(static_cast<OscillatorNode*>(impl), creationContext, isolate);
    case AudioNode::NodeTypeAudioBufferSource:
        return wrap(static_cast<AudioBufferSourceNode*>(impl), creationContext, isolate);
    case AudioNode::NodeTypeMediaElementAudioSource:
        return wrap(static_cast<MediaElementAudioSourceNode*>(impl), creationContext, isolate);
    case AudioNode::NodeTypeMediaStreamAudioDestination:
        return wrap(static_cast<MediaStreamAudioDestinationNode*>(impl), creationContext, isolate);
    case AudioNode::NodeTypeMediaStreamAudioSource:
        return wrap(static_cast<MediaStreamAudioSourceNode*>(impl), creationContext, isolate);
    case AudioNode::NodeTypeJavaScript:
        return wrap(static_cast<ScriptProcessorNode*>(impl), creationContext, isolate);
    case AudioNode::NodeTypeBiquadFilter:
        return wrap(static_cast<BiquadFilterNode*>(impl), creationContext, isolate);
    case AudioNode::NodeTypePanner:
        return wrap(static_cast<PannerNode*>(impl), creationContext, isolate);
    case AudioNode::NodeTypeConvolver:
        return wrap(static_cast<ConvolverNode*>(impl), creationContext, isolate);
    case AudioNode::NodeTypeDelay:
        return wrap(static_cast<DelayNode*>(impl), creationContext, isolate);
    case AudioNode::NodeTypeGain:
        return wrap(static_cast<GainNode*>(impl), creationContext, isolate);
    case AudioNode::NodeTypeChannelSplitter:
        return wrap(static_cast<ChannelSplitterNode*>(impl), creationContext, isolate);
    case AudioNode::NodeTypeChannelMerger:
        return wrap(static_cast<ChannelMergerNode*>(impl), creationContext, isolate);
    case AudioNode::NodeTypeAnalyser:
        return wrap(static_cast<AnalyserNode*>(impl), creationContext, isolate);
    case AudioNode::NodeTypeDynamicsCompressor:
        return wrap(static_cast<DynamicsCompressorNode*>(impl), creationContext, isolate);
    case AudioNode::NodeTypeWaveShaper:
        return wrap(static_cast<WaveShaperNode*>(impl), creationContext, isolate);
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    return V8AudioNode::createWrapper(impl, creationContext, isolate);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
