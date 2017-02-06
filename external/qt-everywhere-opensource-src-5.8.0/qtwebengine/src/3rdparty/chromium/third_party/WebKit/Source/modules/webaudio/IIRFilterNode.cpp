// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/IIRFilterNode.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "modules/webaudio/AbstractAudioContext.h"
#include "modules/webaudio/AudioBasicProcessorHandler.h"
#include "platform/Histogram.h"
#include "wtf/PtrUtil.h"

namespace blink {

IIRFilterNode::IIRFilterNode(AbstractAudioContext& context, const Vector<double> feedforwardCoef, const Vector<double> feedbackCoef)
    : AudioNode(context)
{
    setHandler(AudioBasicProcessorHandler::create(
        AudioHandler::NodeTypeIIRFilter, *this, context.sampleRate(),
        wrapUnique(new IIRProcessor(context.sampleRate(), 1, feedforwardCoef, feedbackCoef))));

    // Histogram of the IIRFilter order.  createIIRFilter ensures that the length of |feedbackCoef|
    // is in the range [1, IIRFilter::kMaxOrder + 1].  The order is one less than the length of this
    // vector.
    DEFINE_STATIC_LOCAL(SparseHistogram, filterOrderHistogram, ("WebAudio.IIRFilterNode.Order"));

    filterOrderHistogram.sample(feedbackCoef.size() - 1);
}

IIRFilterNode* IIRFilterNode::create(
    AbstractAudioContext& context,
    const Vector<double> feedforwardCoef,
    const Vector<double> feedbackCoef,
    ExceptionState& exceptionState)
{
    DCHECK(isMainThread());

    if (context.isContextClosed()) {
        context.throwExceptionForClosedState(exceptionState);
        return nullptr;
    }

    if (feedbackCoef.size() == 0 || (feedbackCoef.size() > IIRFilter::kMaxOrder + 1)) {
        exceptionState.throwDOMException(
            NotSupportedError,
            ExceptionMessages::indexOutsideRange<size_t>(
                "number of feedback coefficients",
                feedbackCoef.size(),
                1,
                ExceptionMessages::InclusiveBound,
                IIRFilter::kMaxOrder + 1,
                ExceptionMessages::InclusiveBound));
        return nullptr;
    }

    if (feedforwardCoef.size() == 0 || (feedforwardCoef.size() > IIRFilter::kMaxOrder + 1)) {
        exceptionState.throwDOMException(
            NotSupportedError,
            ExceptionMessages::indexOutsideRange<size_t>(
                "number of feedforward coefficients",
                feedforwardCoef.size(),
                1,
                ExceptionMessages::InclusiveBound,
                IIRFilter::kMaxOrder + 1,
                ExceptionMessages::InclusiveBound));
        return nullptr;
    }

    if (feedbackCoef[0] == 0) {
        exceptionState.throwDOMException(
            InvalidStateError,
            "First feedback coefficient cannot be zero.");
        return nullptr;
    }

    bool hasNonZeroCoef = false;

    for (size_t k = 0; k < feedforwardCoef.size(); ++k) {
        if (feedforwardCoef[k] != 0) {
            hasNonZeroCoef = true;
            break;
        }
    }

    if (!hasNonZeroCoef) {
        exceptionState.throwDOMException(
            InvalidStateError,
            "At least one feedforward coefficient must be non-zero.");
        return nullptr;
    }

    // Make sure all coefficents are finite.
    for (size_t k = 0; k < feedforwardCoef.size(); ++k) {
        double c = feedforwardCoef[k];
        if (!std::isfinite(c)) {
            String name = "feedforward coefficient " + String::number(k);
            exceptionState.throwDOMException(
                InvalidStateError,
                ExceptionMessages::notAFiniteNumber(c, name.ascii().data()));
            return nullptr;
        }
    }

    for (size_t k = 0; k < feedbackCoef.size(); ++k) {
        double c = feedbackCoef[k];
        if (!std::isfinite(c)) {
            String name = "feedback coefficient " + String::number(k);
            exceptionState.throwDOMException(
                InvalidStateError,
                ExceptionMessages::notAFiniteNumber(c, name.ascii().data()));
            return nullptr;
        }
    }

    return new IIRFilterNode(context, feedforwardCoef, feedbackCoef);
}

DEFINE_TRACE(IIRFilterNode)
{
    AudioNode::trace(visitor);
}

IIRProcessor* IIRFilterNode::iirProcessor() const
{
    return static_cast<IIRProcessor*>(static_cast<AudioBasicProcessorHandler&>(handler()).processor());
}

void IIRFilterNode::getFrequencyResponse(const DOMFloat32Array* frequencyHz, DOMFloat32Array* magResponse, DOMFloat32Array* phaseResponse, ExceptionState& exceptionState)
{
    if (!frequencyHz) {
        exceptionState.throwDOMException(
            NotSupportedError,
            "frequencyHz array cannot be null");
        return;
    }

    if (!magResponse) {
        exceptionState.throwDOMException(
            NotSupportedError,
            "magResponse array cannot be null");
        return;
    }

    if (!phaseResponse) {
        exceptionState.throwDOMException(
            NotSupportedError,
            "phaseResponse array cannot be null");
        return;
    }

    unsigned frequencyHzLength = frequencyHz->length();

    if (magResponse->length() < frequencyHzLength) {
        exceptionState.throwDOMException(
            NotSupportedError,
            ExceptionMessages::indexExceedsMinimumBound(
                "magResponse length",
                magResponse->length(),
                frequencyHzLength));
        return;
    }

    if (phaseResponse->length() < frequencyHzLength) {
        exceptionState.throwDOMException(
            NotSupportedError,
            ExceptionMessages::indexExceedsMinimumBound(
                "phaseResponse length",
                phaseResponse->length(),
                frequencyHzLength));
        return;
    }

    iirProcessor()->getFrequencyResponse(frequencyHzLength, frequencyHz->data(), magResponse->data(), phaseResponse->data());
}

} // namespace blink
