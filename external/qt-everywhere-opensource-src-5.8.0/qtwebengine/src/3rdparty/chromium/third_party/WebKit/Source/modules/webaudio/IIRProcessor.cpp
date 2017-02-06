// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/IIRProcessor.h"

#include "modules/webaudio/IIRDSPKernel.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

IIRProcessor::IIRProcessor(float sampleRate, size_t numberOfChannels, const Vector<double>& feedforwardCoef, const Vector<double>& feedbackCoef)
    : AudioDSPKernelProcessor(sampleRate, numberOfChannels)
{
    unsigned feedbackLength = feedbackCoef.size();
    unsigned feedforwardLength = feedforwardCoef.size();
    ASSERT(feedbackLength > 0);
    ASSERT(feedforwardLength > 0);

    m_feedforward.allocate(feedforwardLength);
    m_feedback.allocate(feedbackLength);
    m_feedforward.copyToRange(feedforwardCoef.data(), 0, feedforwardLength);
    m_feedback.copyToRange(feedbackCoef.data(), 0, feedbackLength);

    // Need to scale the feedback and feedforward coefficients appropriately. (It's up to the caller
    // to ensure feedbackCoef[0] is not 0!)
    ASSERT(feedbackCoef[0] != 0);

    if (feedbackCoef[0] != 1) {
        // The provided filter is:
        //
        //   a[0]*y(n) + a[1]*y(n-1) + ... = b[0]*x(n) + b[1]*x(n-1) + ...
        //
        // We want the leading coefficient of y(n) to be 1:
        //
        //   y(n) + a[1]/a[0]*y(n-1) + ... = b[0]/a[0]*x(n) + b[1]/a[0]*x(n-1) + ...
        //
        // Thus, the feedback and feedforward coefficients need to be scaled by 1/a[0].
        float scale = feedbackCoef[0];
        for (unsigned k = 1; k < feedbackLength; ++k)
            m_feedback[k] /= scale;

        for (unsigned k = 0; k < feedforwardLength; ++k)
            m_feedforward[k] /= scale;

        // The IIRFilter checks to make sure this coefficient is 1, so make it so.
        m_feedback[0] = 1;
    }

    m_responseKernel = wrapUnique(new IIRDSPKernel(this));
}

IIRProcessor::~IIRProcessor()
{
    if (isInitialized())
        uninitialize();
}

std::unique_ptr<AudioDSPKernel> IIRProcessor::createKernel()
{
    return wrapUnique(new IIRDSPKernel(this));
}

void IIRProcessor::process(const AudioBus* source, AudioBus* destination, size_t framesToProcess)
{
    if (!isInitialized()) {
        destination->zero();
        return;
    }

    // For each channel of our input, process using the corresponding IIRDSPKernel into the output
    // channel.
    for (unsigned i = 0; i < m_kernels.size(); ++i)
        m_kernels[i]->process(source->channel(i)->data(), destination->channel(i)->mutableData(), framesToProcess);
}

void IIRProcessor::getFrequencyResponse(int nFrequencies, const float* frequencyHz, float* magResponse, float* phaseResponse)
{
    m_responseKernel->getFrequencyResponse(nFrequencies, frequencyHz, magResponse, phaseResponse);
}

} // namespace blink
