// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IIRDSPKernel_h
#define IIRDSPKernel_h

#include "modules/webaudio/IIRProcessor.h"
#include "platform/audio/AudioDSPKernel.h"
#include "platform/audio/IIRFilter.h"

namespace blink {

class IIRProcessor;

class IIRDSPKernel final : public AudioDSPKernel {
public:
    explicit IIRDSPKernel(IIRProcessor* processor)
        : AudioDSPKernel(processor)
        , m_iir(processor->feedforward(), processor->feedback())
    {
    }

    // AudioDSPKernel
    void process(const float* source, float* dest, size_t framesToProcess) override;
    void reset() override { m_iir.reset(); }

    // Get the magnitude and phase response of the filter at the given
    // set of frequencies (in Hz). The phase response is in radians.
    void getFrequencyResponse(int nFrequencies, const float* frequencyHz, float* magResponse, float* phaseResponse);

    double tailTime() const override;
    double latencyTime() const override;

protected:
    IIRFilter m_iir;
};

} // namespace blink

#endif // IIRDSPKernel_h
