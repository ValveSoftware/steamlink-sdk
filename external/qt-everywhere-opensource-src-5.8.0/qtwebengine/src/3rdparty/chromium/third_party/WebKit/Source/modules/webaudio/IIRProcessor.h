// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IIRProcessor_h
#define IIRProcessor_h

#include "modules/webaudio/AudioNode.h"
#include "platform/audio/AudioDSPKernel.h"
#include "platform/audio/AudioDSPKernelProcessor.h"
#include "platform/audio/IIRFilter.h"
#include <memory>

namespace blink {

class IIRDSPKernel;

class IIRProcessor final : public AudioDSPKernelProcessor {
public:
    IIRProcessor(float sampleRate, size_t numberOfChannels,
        const Vector<double>& feedforwardCoef, const Vector<double>& feedbackCoef);
    ~IIRProcessor() override;

    std::unique_ptr<AudioDSPKernel> createKernel() override;

    void process(const AudioBus* source, AudioBus* destination, size_t framesToProcess) override;

    // Get the magnitude and phase response of the filter at the given
    // set of frequencies (in Hz). The phase response is in radians.
    void getFrequencyResponse(int nFrequencies, const float* frequencyHz, float* magResponse, float* phaseResponse);

    AudioDoubleArray* feedback() { return &m_feedback; }
    AudioDoubleArray* feedforward() { return &m_feedforward; }

private:
    // The feedback and feedforward filter coefficients for the IIR filter.
    AudioDoubleArray m_feedback;
    AudioDoubleArray m_feedforward;
    // This holds the IIR kernel for computing the frequency response.
    std::unique_ptr<IIRDSPKernel> m_responseKernel;
};

} // namespace blink

#endif // IIRProcessor_h
