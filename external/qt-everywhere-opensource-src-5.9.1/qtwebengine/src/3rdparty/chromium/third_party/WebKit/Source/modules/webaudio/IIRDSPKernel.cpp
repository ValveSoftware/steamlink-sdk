// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/IIRDSPKernel.h"

#include "wtf/MathExtras.h"

namespace blink {

void IIRDSPKernel::process(const float* source,
                           float* destination,
                           size_t framesToProcess) {
  DCHECK(source);
  DCHECK(destination);

  m_iir.process(source, destination, framesToProcess);
}

void IIRDSPKernel::getFrequencyResponse(int nFrequencies,
                                        const float* frequencyHz,
                                        float* magResponse,
                                        float* phaseResponse) {
  bool isGood = nFrequencies > 0 && frequencyHz && magResponse && phaseResponse;
  DCHECK(isGood);
  if (!isGood)
    return;

  Vector<float> frequency(nFrequencies);

  double nyquist = this->nyquist();

  // Convert from frequency in Hz to normalized frequency (0 -> 1),
  // with 1 equal to the Nyquist frequency.
  for (int k = 0; k < nFrequencies; ++k)
    frequency[k] = clampTo<float>(frequencyHz[k] / nyquist);

  m_iir.getFrequencyResponse(nFrequencies, frequency.data(), magResponse,
                             phaseResponse);
}

double IIRDSPKernel::tailTime() const {
  // TODO(rtoy): This is true mathematically (infinite impulse response), but
  // perhaps it should be limited to a smaller value, possibly based on the
  // actual filter coefficients.  To do that, we would probably need to find the
  // pole, r, with largest magnitude and select some threshold, eps, such that
  // |r|^n < eps for all n >= N.  N is then the tailTime for the filter.  If the
  // the magnitude of r is greater than or equal to 1, the infinity is the right
  // answer. (There is no constraint on the IIR filter that it be stable.)
  return std::numeric_limits<double>::infinity();
}

double IIRDSPKernel::latencyTime() const {
  return 0;
}

}  // namespace blink
