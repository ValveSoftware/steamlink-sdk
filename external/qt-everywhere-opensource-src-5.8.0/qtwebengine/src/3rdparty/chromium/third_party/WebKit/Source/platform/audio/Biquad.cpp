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

#include "platform/audio/Biquad.h"

#include "platform/audio/AudioUtilities.h"
#include "platform/audio/DenormalDisabler.h"
#include "wtf/MathExtras.h"

#include <algorithm>
#include <complex>
#include <stdio.h>
#if OS(MACOSX)
#include <Accelerate/Accelerate.h>
#endif

namespace blink {

#if OS(MACOSX)
const int kBufferSize = 1024;
#endif

Biquad::Biquad()
    : m_hasSampleAccurateValues(false)
{
#if OS(MACOSX)
    // Allocate two samples more for filter history
    m_inputBuffer.allocate(kBufferSize + 2);
    m_outputBuffer.allocate(kBufferSize + 2);
#endif

    // Allocate enough space for the a-rate filter coefficients to handle a rendering quantum of 128
    // frames.
    m_b0.allocate(AudioUtilities::kRenderQuantumFrames);
    m_b1.allocate(AudioUtilities::kRenderQuantumFrames);
    m_b2.allocate(AudioUtilities::kRenderQuantumFrames);
    m_a1.allocate(AudioUtilities::kRenderQuantumFrames);
    m_a2.allocate(AudioUtilities::kRenderQuantumFrames);

    // Initialize as pass-thru (straight-wire, no filter effect)
    setNormalizedCoefficients(0, 1, 0, 0, 1, 0, 0);

    reset(); // clear filter memory
}

Biquad::~Biquad()
{
}

void Biquad::process(const float* sourceP, float* destP, size_t framesToProcess)
{
    // WARNING: sourceP and destP may be pointing to the same area of memory!
    // Be sure to read from sourceP before writing to destP!
    if (hasSampleAccurateValues()) {
        int n = framesToProcess;

        // Create local copies of member variables
        double x1 = m_x1;
        double x2 = m_x2;
        double y1 = m_y1;
        double y2 = m_y2;

        const double* b0 = m_b0.data();
        const double* b1 = m_b1.data();
        const double* b2 = m_b2.data();
        const double* a1 = m_a1.data();
        const double* a2 = m_a2.data();

        for (int k = 0; k < n; ++k) {
            // FIXME: this can be optimized by pipelining the multiply adds...
            float x = *sourceP++;
            float y = b0[k]*x + b1[k]*x1 + b2[k]*x2 - a1[k]*y1 - a2[k]*y2;

            *destP++ = y;

            // Update state variables
            x2 = x1;
            x1 = x;
            y2 = y1;
            y1 = y;
        }

        // Local variables back to member. Flush denormals here so we
        // don't slow down the inner loop above.
        m_x1 = DenormalDisabler::flushDenormalFloatToZero(x1);
        m_x2 = DenormalDisabler::flushDenormalFloatToZero(x2);
        m_y1 = DenormalDisabler::flushDenormalFloatToZero(y1);
        m_y2 = DenormalDisabler::flushDenormalFloatToZero(y2);

        // There is an assumption here that once we have sample accurate values we can never go back
        // to not having sample accurate values.  This is currently true in the way
        // AudioParamTimline is implemented: once an event is inserted, sample accurate processing
        // is always enabled.
        //
        // If so, then we never have to update the state variables for the MACOSX path.  The
        // structure of the state variable in these cases aren't well documented so it's not clear
        // how to update them anyway.
    } else {
#if OS(MACOSX)
        double* inputP = m_inputBuffer.data();
        double* outputP = m_outputBuffer.data();

        // Set up filter state.  This is needed in case we're switching from
        // filtering with variable coefficients (i.e., with automations) to
        // fixed coefficients (without automations).
        inputP[0] = m_x2;
        inputP[1] = m_x1;
        outputP[0] = m_y2;
        outputP[1] = m_y1;

        // Use vecLib if available
        processFast(sourceP, destP, framesToProcess);

        // Copy the last inputs and outputs to the filter memory variables.
        // This is needed because the next rendering quantum might be an
        // automation which needs the history to continue correctly.  Because
        // sourceP and destP can be the same block of memory, we can't read from
        // sourceP to get the last inputs.  Fortunately, processFast has put the
        // last inputs in input[0] and input[1].
        m_x1 = inputP[1];
        m_x2 = inputP[0];
        m_y1 = destP[framesToProcess - 1];
        m_y2 = destP[framesToProcess - 2];

#else
        int n = framesToProcess;

        // Create local copies of member variables
        double x1 = m_x1;
        double x2 = m_x2;
        double y1 = m_y1;
        double y2 = m_y2;

        double b0 = m_b0[0];
        double b1 = m_b1[0];
        double b2 = m_b2[0];
        double a1 = m_a1[0];
        double a2 = m_a2[0];

        while (n--) {
            // FIXME: this can be optimized by pipelining the multiply adds...
            float x = *sourceP++;
            float y = b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2;

            *destP++ = y;

            // Update state variables
            x2 = x1;
            x1 = x;
            y2 = y1;
            y1 = y;
        }

        // Local variables back to member. Flush denormals here so we
        // don't slow down the inner loop above.
        m_x1 = DenormalDisabler::flushDenormalFloatToZero(x1);
        m_x2 = DenormalDisabler::flushDenormalFloatToZero(x2);
        m_y1 = DenormalDisabler::flushDenormalFloatToZero(y1);
        m_y2 = DenormalDisabler::flushDenormalFloatToZero(y2);
#endif
    }
}

#if OS(MACOSX)

// Here we have optimized version using Accelerate.framework

void Biquad::processFast(const float* sourceP, float* destP, size_t framesToProcess)
{
    double filterCoefficients[5];
    filterCoefficients[0] = m_b0[0];
    filterCoefficients[1] = m_b1[0];
    filterCoefficients[2] = m_b2[0];
    filterCoefficients[3] = m_a1[0];
    filterCoefficients[4] = m_a2[0];

    double* inputP = m_inputBuffer.data();
    double* outputP = m_outputBuffer.data();

    double* input2P = inputP + 2;
    double* output2P = outputP + 2;

    // Break up processing into smaller slices (kBufferSize) if necessary.

    int n = framesToProcess;

    while (n > 0) {
        int framesThisTime = n < kBufferSize ? n : kBufferSize;

        // Copy input to input buffer
        for (int i = 0; i < framesThisTime; ++i)
            input2P[i] = *sourceP++;

        processSliceFast(inputP, outputP, filterCoefficients, framesThisTime);

        // Copy output buffer to output (converts float -> double).
        for (int i = 0; i < framesThisTime; ++i)
            *destP++ = static_cast<float>(output2P[i]);

        n -= framesThisTime;
    }
}

void Biquad::processSliceFast(double* sourceP, double* destP, double* coefficientsP, size_t framesToProcess)
{
    // Use double-precision for filter stability
    vDSP_deq22D(sourceP, 1, coefficientsP, destP, 1, framesToProcess);

    // Save history.  Note that sourceP and destP reference m_inputBuffer and m_outputBuffer respectively.
    // These buffers are allocated (in the constructor) with space for two extra samples so it's OK to access
    // array values two beyond framesToProcess.
    sourceP[0] = sourceP[framesToProcess - 2 + 2];
    sourceP[1] = sourceP[framesToProcess - 1 + 2];
    destP[0] = destP[framesToProcess - 2 + 2];
    destP[1] = destP[framesToProcess - 1 + 2];
}

#endif // OS(MACOSX)


void Biquad::reset()
{
#if OS(MACOSX)
    // Two extra samples for filter history
    double* inputP = m_inputBuffer.data();
    inputP[0] = 0;
    inputP[1] = 0;

    double* outputP = m_outputBuffer.data();
    outputP[0] = 0;
    outputP[1] = 0;

#endif
    m_x1 = m_x2 = m_y1 = m_y2 = 0;
}

void Biquad::setLowpassParams(int index, double cutoff, double resonance)
{
    // Limit cutoff to 0 to 1.
    cutoff = clampTo(cutoff, 0.0, 1.0);

    if (cutoff == 1) {
        // When cutoff is 1, the z-transform is 1.
        setNormalizedCoefficients(index,
            1, 0, 0,
            1, 0, 0);
    } else if (cutoff > 0) {
        // Compute biquad coefficients for lowpass filter

        resonance = pow(10, resonance / 20);
        double theta = piDouble * cutoff;
        double alpha = sin(theta) / (2 * resonance);
        double cosw = cos(theta);
        double beta = (1 - cosw) / 2;

        double b0 = beta;
        double b1 = 2 * beta;
        double b2 = beta;

        double a0 = 1 + alpha;
        double a1 = -2 * cosw;
        double a2 = 1 - alpha;

        setNormalizedCoefficients(index, b0, b1, b2, a0, a1, a2);
    } else {
        // When cutoff is zero, nothing gets through the filter, so set
        // coefficients up correctly.
        setNormalizedCoefficients(index,
            0, 0, 0,
            1, 0, 0);
    }
}

void Biquad::setHighpassParams(int index, double cutoff, double resonance)
{
    // Limit cutoff to 0 to 1.
    cutoff = clampTo(cutoff, 0.0, 1.0);

    if (cutoff == 1) {
        // The z-transform is 0.
        setNormalizedCoefficients(index,
            0, 0, 0,
            1, 0, 0);
    } else if (cutoff > 0) {
        // Compute biquad coefficients for highpass filter

        resonance = pow(10, resonance / 20);
        double theta = piDouble * cutoff;
        double alpha = sin(theta) / (2 * resonance);
        double cosw = cos(theta);
        double beta = (1 + cosw) / 2;

        double b0 = beta;
        double b1 = -2 * beta;
        double b2 = beta;

        double a0 = 1 + alpha;
        double a1 = -2 * cosw;
        double a2 = 1 - alpha;

        setNormalizedCoefficients(index, b0, b1, b2, a0, a1, a2);
    } else {
        // When cutoff is zero, we need to be careful because the above
        // gives a quadratic divided by the same quadratic, with poles
        // and zeros on the unit circle in the same place. When cutoff
        // is zero, the z-transform is 1.
        setNormalizedCoefficients(index,
            1, 0, 0,
            1, 0, 0);
    }
}

void Biquad::setNormalizedCoefficients(int index, double b0, double b1, double b2, double a0, double a1, double a2)
{
    double a0Inverse = 1 / a0;

    m_b0[index] = b0 * a0Inverse;
    m_b1[index] = b1 * a0Inverse;
    m_b2[index] = b2 * a0Inverse;
    m_a1[index] = a1 * a0Inverse;
    m_a2[index] = a2 * a0Inverse;
}

void Biquad::setLowShelfParams(int index, double frequency, double dbGain)
{
    // Clip frequencies to between 0 and 1, inclusive.
    frequency = clampTo(frequency, 0.0, 1.0);

    double A = pow(10.0, dbGain / 40);

    if (frequency == 1) {
        // The z-transform is a constant gain.
        setNormalizedCoefficients(index,
            A * A, 0, 0,
            1, 0, 0);
    } else if (frequency > 0) {
        double w0 = piDouble * frequency;
        double S = 1; // filter slope (1 is max value)
        double alpha = 0.5 * sin(w0) * sqrt((A + 1 / A) * (1 / S - 1) + 2);
        double k = cos(w0);
        double k2 = 2 * sqrt(A) * alpha;
        double aPlusOne = A + 1;
        double aMinusOne = A - 1;

        double b0 = A * (aPlusOne - aMinusOne * k + k2);
        double b1 = 2 * A * (aMinusOne - aPlusOne * k);
        double b2 = A * (aPlusOne - aMinusOne * k - k2);
        double a0 = aPlusOne + aMinusOne * k + k2;
        double a1 = -2 * (aMinusOne + aPlusOne * k);
        double a2 = aPlusOne + aMinusOne * k - k2;

        setNormalizedCoefficients(index, b0, b1, b2, a0, a1, a2);
    } else {
        // When frequency is 0, the z-transform is 1.
        setNormalizedCoefficients(index,
            1, 0, 0,
            1, 0, 0);
    }
}

void Biquad::setHighShelfParams(int index, double frequency, double dbGain)
{
    // Clip frequencies to between 0 and 1, inclusive.
    frequency = clampTo(frequency, 0.0, 1.0);

    double A = pow(10.0, dbGain / 40);

    if (frequency == 1) {
        // The z-transform is 1.
        setNormalizedCoefficients(index,
            1, 0, 0,
            1, 0, 0);
    } else if (frequency > 0) {
        double w0 = piDouble * frequency;
        double S = 1; // filter slope (1 is max value)
        double alpha = 0.5 * sin(w0) * sqrt((A + 1 / A) * (1 / S - 1) + 2);
        double k = cos(w0);
        double k2 = 2 * sqrt(A) * alpha;
        double aPlusOne = A + 1;
        double aMinusOne = A - 1;

        double b0 = A * (aPlusOne + aMinusOne * k + k2);
        double b1 = -2 * A * (aMinusOne + aPlusOne * k);
        double b2 = A * (aPlusOne + aMinusOne * k - k2);
        double a0 = aPlusOne - aMinusOne * k + k2;
        double a1 = 2 * (aMinusOne - aPlusOne * k);
        double a2 = aPlusOne - aMinusOne * k - k2;

        setNormalizedCoefficients(index,
            b0, b1, b2,
            a0, a1, a2);
    } else {
        // When frequency = 0, the filter is just a gain, A^2.
        setNormalizedCoefficients(index,
            A * A, 0, 0,
            1, 0, 0);
    }
}

void Biquad::setPeakingParams(int index, double frequency, double Q, double dbGain)
{
    // Clip frequencies to between 0 and 1, inclusive.
    frequency = clampTo(frequency, 0.0, 1.0);

    // Don't let Q go negative, which causes an unstable filter.
    Q = std::max(0.0, Q);

    double A = pow(10.0, dbGain / 40);

    if (frequency > 0 && frequency < 1) {
        if (Q > 0) {
            double w0 = piDouble * frequency;
            double alpha = sin(w0) / (2 * Q);
            double k = cos(w0);

            double b0 = 1 + alpha * A;
            double b1 = -2 * k;
            double b2 = 1 - alpha * A;
            double a0 = 1 + alpha / A;
            double a1 = -2 * k;
            double a2 = 1 - alpha / A;

            setNormalizedCoefficients(index,
                b0, b1, b2,
                a0, a1, a2);
        } else {
            // When Q = 0, the above formulas have problems. If we look at
            // the z-transform, we can see that the limit as Q->0 is A^2, so
            // set the filter that way.
            setNormalizedCoefficients(index,
                A * A, 0, 0,
                1, 0, 0);
        }
    } else {
        // When frequency is 0 or 1, the z-transform is 1.
        setNormalizedCoefficients(index,
            1, 0, 0,
            1, 0, 0);
    }
}

void Biquad::setAllpassParams(int index, double frequency, double Q)
{
    // Clip frequencies to between 0 and 1, inclusive.
    frequency = clampTo(frequency, 0.0, 1.0);

    // Don't let Q go negative, which causes an unstable filter.
    Q = std::max(0.0, Q);

    if (frequency > 0 && frequency < 1) {
        if (Q > 0) {
            double w0 = piDouble * frequency;
            double alpha = sin(w0) / (2 * Q);
            double k = cos(w0);

            double b0 = 1 - alpha;
            double b1 = -2 * k;
            double b2 = 1 + alpha;
            double a0 = 1 + alpha;
            double a1 = -2 * k;
            double a2 = 1 - alpha;

            setNormalizedCoefficients(index,
                b0, b1, b2,
                a0, a1, a2);
        } else {
            // When Q = 0, the above formulas have problems. If we look at
            // the z-transform, we can see that the limit as Q->0 is -1, so
            // set the filter that way.
            setNormalizedCoefficients(index,
                -1, 0, 0,
                1, 0, 0);
        }
    } else {
        // When frequency is 0 or 1, the z-transform is 1.
        setNormalizedCoefficients(index,
            1, 0, 0,
            1, 0, 0);
    }
}

void Biquad::setNotchParams(int index, double frequency, double Q)
{
    // Clip frequencies to between 0 and 1, inclusive.
    frequency = clampTo(frequency, 0.0, 1.0);

    // Don't let Q go negative, which causes an unstable filter.
    Q = std::max(0.0, Q);

    if (frequency > 0 && frequency < 1) {
        if (Q > 0) {
            double w0 = piDouble * frequency;
            double alpha = sin(w0) / (2 * Q);
            double k = cos(w0);

            double b0 = 1;
            double b1 = -2 * k;
            double b2 = 1;
            double a0 = 1 + alpha;
            double a1 = -2 * k;
            double a2 = 1 - alpha;

            setNormalizedCoefficients(index,
                b0, b1, b2,
                a0, a1, a2);
        } else {
            // When Q = 0, the above formulas have problems. If we look at
            // the z-transform, we can see that the limit as Q->0 is 0, so
            // set the filter that way.
            setNormalizedCoefficients(index,
                0, 0, 0,
                1, 0, 0);
        }
    } else {
        // When frequency is 0 or 1, the z-transform is 1.
        setNormalizedCoefficients(index,
            1, 0, 0,
            1, 0, 0);
    }
}

void Biquad::setBandpassParams(int index, double frequency, double Q)
{
    // No negative frequencies allowed.
    frequency = std::max(0.0, frequency);

    // Don't let Q go negative, which causes an unstable filter.
    Q = std::max(0.0, Q);

    if (frequency > 0 && frequency < 1) {
        double w0 = piDouble * frequency;
        if (Q > 0) {
            double alpha = sin(w0) / (2 * Q);
            double k = cos(w0);

            double b0 = alpha;
            double b1 = 0;
            double b2 = -alpha;
            double a0 = 1 + alpha;
            double a1 = -2 * k;
            double a2 = 1 - alpha;

            setNormalizedCoefficients(index,
                b0, b1, b2,
                a0, a1, a2);
        } else {
            // When Q = 0, the above formulas have problems. If we look at
            // the z-transform, we can see that the limit as Q->0 is 1, so
            // set the filter that way.
            setNormalizedCoefficients(index,
                1, 0, 0,
                1, 0, 0);
        }
    } else {
        // When the cutoff is zero, the z-transform approaches 0, if Q
        // > 0. When both Q and cutoff are zero, the z-transform is
        // pretty much undefined. What should we do in this case?
        // For now, just make the filter 0. When the cutoff is 1, the
        // z-transform also approaches 0.
        setNormalizedCoefficients(index,
            0, 0, 0,
            1, 0, 0);
    }
}

void Biquad::getFrequencyResponse(int nFrequencies,
                                  const float* frequency,
                                  float* magResponse,
                                  float* phaseResponse)
{
    // Evaluate the Z-transform of the filter at given normalized
    // frequency from 0 to 1.  (1 corresponds to the Nyquist
    // frequency.)
    //
    // The z-transform of the filter is
    //
    // H(z) = (b0 + b1*z^(-1) + b2*z^(-2))/(1 + a1*z^(-1) + a2*z^(-2))
    //
    // Evaluate as
    //
    // b0 + (b1 + b2*z1)*z1
    // --------------------
    // 1 + (a1 + a2*z1)*z1
    //
    // with z1 = 1/z and z = exp(j*pi*frequency). Hence z1 = exp(-j*pi*frequency)

    // Make local copies of the coefficients as a micro-optimization.
    double b0 = m_b0[0];
    double b1 = m_b1[0];
    double b2 = m_b2[0];
    double a1 = m_a1[0];
    double a2 = m_a2[0];

    for (int k = 0; k < nFrequencies; ++k) {
        double omega = -piDouble * frequency[k];
        std::complex<double> z = std::complex<double> (cos(omega), sin(omega));
        std::complex<double> numerator = b0 + (b1 + b2 * z) * z;
        std::complex<double> denominator =
            std::complex<double>(1, 0) + (a1 + a2 * z) * z;
        std::complex<double> response = numerator / denominator;
        magResponse[k] = static_cast<float>(abs(response));
        phaseResponse[k] = static_cast<float>(atan2(imag(response), real(response)));
    }
}

} // namespace blink

