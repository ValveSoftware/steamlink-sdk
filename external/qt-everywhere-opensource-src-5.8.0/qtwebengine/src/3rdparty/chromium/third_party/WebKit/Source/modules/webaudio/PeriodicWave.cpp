/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "modules/webaudio/AbstractAudioContext.h"
#include "modules/webaudio/OscillatorNode.h"
#include "modules/webaudio/PeriodicWave.h"
#include "platform/audio/FFTFrame.h"
#include "platform/audio/VectorMath.h"
#include "wtf/PtrUtil.h"
#include <algorithm>
#include <memory>

namespace blink {

// The number of bands per octave.  Each octave will have this many entries in the wave tables.
const unsigned kNumberOfOctaveBands = 3;

// The max length of a periodic wave. This must be a power of two greater than or equal to 2048 and
// must be supported by the FFT routines.
const unsigned kMaxPeriodicWaveSize = 16384;

const float CentsPerRange = 1200 / kNumberOfOctaveBands;

using namespace VectorMath;

PeriodicWave* PeriodicWave::create(
    AbstractAudioContext& context,
    DOMFloat32Array* real,
    DOMFloat32Array* imag,
    bool disableNormalization,
    ExceptionState& exceptionState)
{
    DCHECK(isMainThread());

    if (context.isContextClosed()) {
        context.throwExceptionForClosedState(exceptionState);
        return nullptr;
    }

    if (real->length() != imag->length()) {
        exceptionState.throwDOMException(
            IndexSizeError,
            "length of real array (" + String::number(real->length())
            + ") and length of imaginary array (" +  String::number(imag->length())
            + ") must match.");
        return nullptr;
    }

    PeriodicWave* periodicWave = new PeriodicWave(context.sampleRate());
    size_t numberOfComponents = real->length();
    periodicWave->createBandLimitedTables(real->data(), imag->data(), numberOfComponents, disableNormalization);
    return periodicWave;
}

PeriodicWave* PeriodicWave::createSine(float sampleRate)
{
    PeriodicWave* periodicWave = new PeriodicWave(sampleRate);
    periodicWave->generateBasicWaveform(OscillatorHandler::SINE);
    return periodicWave;
}

PeriodicWave* PeriodicWave::createSquare(float sampleRate)
{
    PeriodicWave* periodicWave = new PeriodicWave(sampleRate);
    periodicWave->generateBasicWaveform(OscillatorHandler::SQUARE);
    return periodicWave;
}

PeriodicWave* PeriodicWave::createSawtooth(float sampleRate)
{
    PeriodicWave* periodicWave = new PeriodicWave(sampleRate);
    periodicWave->generateBasicWaveform(OscillatorHandler::SAWTOOTH);
    return periodicWave;
}

PeriodicWave* PeriodicWave::createTriangle(float sampleRate)
{
    PeriodicWave* periodicWave = new PeriodicWave(sampleRate);
    periodicWave->generateBasicWaveform(OscillatorHandler::TRIANGLE);
    return periodicWave;
}

PeriodicWave::PeriodicWave(float sampleRate)
    : m_v8ExternalMemory(0)
    , m_sampleRate(sampleRate)
    , m_centsPerRange(CentsPerRange)
{
    float nyquist = 0.5 * m_sampleRate;
    m_lowestFundamentalFrequency = nyquist / maxNumberOfPartials();
    m_rateScale = periodicWaveSize() / m_sampleRate;
    // Compute the number of ranges needed to cover the entire frequency range, assuming
    // kNumberOfOctaveBands per octave.
    m_numberOfRanges = 0.5 + kNumberOfOctaveBands * log2f(periodicWaveSize());
}

PeriodicWave::~PeriodicWave()
{
    adjustV8ExternalMemory(-static_cast<int64_t>(m_v8ExternalMemory));
}

unsigned PeriodicWave::periodicWaveSize() const
{
    // Choose an appropriate wave size for the given sample rate.  This allows us to use shorter
    // FFTs when possible to limit the complexity.  The breakpoints here are somewhat arbitrary, but
    // we want sample rates around 44.1 kHz or so to have a size of 4096 to preserve backward
    // compatibility.
    if (m_sampleRate <= 24000) {
        return 2048;
    }

    if (m_sampleRate <= 88200) {
        return 4096;
    }

    return kMaxPeriodicWaveSize;
}

unsigned PeriodicWave::maxNumberOfPartials() const
{
    return periodicWaveSize() / 2;
}

void PeriodicWave::waveDataForFundamentalFrequency(float fundamentalFrequency, float*& lowerWaveData, float*& higherWaveData, float& tableInterpolationFactor)
{
    // Negative frequencies are allowed, in which case we alias to the positive frequency.
    fundamentalFrequency = fabsf(fundamentalFrequency);

    // Calculate the pitch range.
    float ratio = fundamentalFrequency > 0 ? fundamentalFrequency / m_lowestFundamentalFrequency : 0.5;
    float centsAboveLowestFrequency = log2f(ratio) * 1200;

    // Add one to round-up to the next range just in time to truncate partials before aliasing occurs.
    float pitchRange = 1 + centsAboveLowestFrequency / m_centsPerRange;

    pitchRange = std::max(pitchRange, 0.0f);
    pitchRange = std::min(pitchRange, static_cast<float>(numberOfRanges() - 1));

    // The words "lower" and "higher" refer to the table data having the lower and higher numbers of partials.
    // It's a little confusing since the range index gets larger the more partials we cull out.
    // So the lower table data will have a larger range index.
    unsigned rangeIndex1 = static_cast<unsigned>(pitchRange);
    unsigned rangeIndex2 = rangeIndex1 < numberOfRanges() - 1 ? rangeIndex1 + 1 : rangeIndex1;

    lowerWaveData = m_bandLimitedTables[rangeIndex2]->data();
    higherWaveData = m_bandLimitedTables[rangeIndex1]->data();

    // Ranges from 0 -> 1 to interpolate between lower -> higher.
    tableInterpolationFactor = pitchRange - rangeIndex1;
}

unsigned PeriodicWave::numberOfPartialsForRange(unsigned rangeIndex) const
{
    // Number of cents below nyquist where we cull partials.
    float centsToCull = rangeIndex * m_centsPerRange;

    // A value from 0 -> 1 representing what fraction of the partials to keep.
    float cullingScale = pow(2, -centsToCull / 1200);

    // The very top range will have all the partials culled.
    unsigned numberOfPartials = cullingScale * maxNumberOfPartials();

    return numberOfPartials;
}

// Tell V8 about the memory we're using so it can properly schedule garbage collects.
void PeriodicWave::adjustV8ExternalMemory(int delta)
{
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(delta);
    m_v8ExternalMemory += delta;
}

// Convert into time-domain wave buffers.
// One table is created for each range for non-aliasing playback at different playback rates.
// Thus, higher ranges have more high-frequency partials culled out.
void PeriodicWave::createBandLimitedTables(const float* realData, const float* imagData, unsigned numberOfComponents, bool disableNormalization)
{
    // TODO(rtoy): Figure out why this needs to be 0.5 when normalization is disabled.
    float normalizationScale = 0.5;

    unsigned fftSize = periodicWaveSize();
    unsigned halfSize = fftSize / 2;
    unsigned i;

    numberOfComponents = std::min(numberOfComponents, halfSize);

    m_bandLimitedTables.reserveCapacity(numberOfRanges());

    FFTFrame frame(fftSize);
    for (unsigned rangeIndex = 0; rangeIndex < numberOfRanges(); ++rangeIndex) {
        // This FFTFrame is used to cull partials (represented by frequency bins).
        float* realP = frame.realData();
        float* imagP = frame.imagData();

        // Copy from loaded frequency data and generate the complex conjugate because of the way the
        // inverse FFT is defined versus the values in the arrays.  Need to scale the data by
        // fftSize to remove the scaling that the inverse IFFT would do.
        float scale = fftSize;
        vsmul(realData, 1, &scale, realP, 1, numberOfComponents);
        scale = -scale;
        vsmul(imagData, 1, &scale, imagP, 1, numberOfComponents);

        // Find the starting bin where we should start culling.  We need to clear out the highest
        // frequencies to band-limit the waveform.
        unsigned numberOfPartials = numberOfPartialsForRange(rangeIndex);

        // If fewer components were provided than 1/2 FFT size, then clear the remaining bins.
        // We also need to cull the aliasing partials for this pitch range.
        for (i = std::min(numberOfComponents, numberOfPartials + 1); i < halfSize; ++i) {
            realP[i] = 0;
            imagP[i] = 0;
        }

        // Clear packed-nyquist and any DC-offset.
        realP[0] = 0;
        imagP[0] = 0;

        // Create the band-limited table.
        unsigned waveSize = periodicWaveSize();
        std::unique_ptr<AudioFloatArray> table = wrapUnique(new AudioFloatArray(waveSize));
        adjustV8ExternalMemory(waveSize * sizeof(float));
        m_bandLimitedTables.append(std::move(table));

        // Apply an inverse FFT to generate the time-domain table data.
        float* data = m_bandLimitedTables[rangeIndex]->data();
        frame.doInverseFFT(data);

        // For the first range (which has the highest power), calculate its peak value then compute normalization scale.
        if (!disableNormalization) {
            if (!rangeIndex) {
                float maxValue;
                vmaxmgv(data, 1, &maxValue, fftSize);

                if (maxValue)
                    normalizationScale = 1.0f / maxValue;
            }
        }

        // Apply normalization scale.
        vsmul(data, 1, &normalizationScale, data, 1, fftSize);
    }
}

void PeriodicWave::generateBasicWaveform(int shape)
{
    unsigned fftSize = periodicWaveSize();
    unsigned halfSize = fftSize / 2;

    AudioFloatArray real(halfSize);
    AudioFloatArray imag(halfSize);
    float* realP = real.data();
    float* imagP = imag.data();

    // Clear DC and Nyquist.
    realP[0] = 0;
    imagP[0] = 0;

    for (unsigned n = 1; n < halfSize; ++n) {
        float piFactor = 2 / (n * piFloat);

        // All waveforms are odd functions with a positive slope at time 0. Hence the coefficients
        // for cos() are always 0.

        // Fourier coefficients according to standard definition:
        // b = 1/pi*integrate(f(x)*sin(n*x), x, -pi, pi)
        //   = 2/pi*integrate(f(x)*sin(n*x), x, 0, pi)
        // since f(x) is an odd function.

        float b; // Coefficient for sin().

        // Calculate Fourier coefficients depending on the shape. Note that the overall scaling
        // (magnitude) of the waveforms is normalized in createBandLimitedTables().
        switch (shape) {
        case OscillatorHandler::SINE:
            // Standard sine wave function.
            b = (n == 1) ? 1 : 0;
            break;
        case OscillatorHandler::SQUARE:
            // Square-shaped waveform with the first half its maximum value and the second half its
            // minimum value.
            //
            // See http://mathworld.wolfram.com/FourierSeriesSquareWave.html
            //
            // b[n] = 2/n/pi*(1-(-1)^n)
            //      = 4/n/pi for n odd and 0 otherwise.
            //      = 2*(2/(n*pi)) for n odd
            b = (n & 1) ? 2 * piFactor : 0;
            break;
        case OscillatorHandler::SAWTOOTH:
            // Sawtooth-shaped waveform with the first half ramping from zero to maximum and the
            // second half from minimum to zero.
            //
            // b[n] = -2*(-1)^n/pi/n
            //      = (2/(n*pi))*(-1)^(n+1)
            b = piFactor * ((n & 1) ? 1 : -1);
            break;
        case OscillatorHandler::TRIANGLE:
            // Triangle-shaped waveform going from 0 at time 0 to 1 at time pi/2 and back to 0 at
            // time pi.
            //
            // See http://mathworld.wolfram.com/FourierSeriesTriangleWave.html
            //
            // b[n] = 8*sin(pi*k/2)/(pi*k)^2
            //      = 8/pi^2/n^2*(-1)^((n-1)/2) for n odd and 0 otherwise
            //      = 2*(2/(n*pi))^2 * (-1)^((n-1)/2)
            if (n & 1) {
                b = 2 * (piFactor * piFactor) * ((((n - 1) >> 1) & 1) ? -1 : 1);
            } else {
                b = 0;
            }
            break;
        default:
            ASSERT_NOT_REACHED();
            b = 0;
            break;
        }

        realP[n] = 0;
        imagP[n] = b;
    }

    createBandLimitedTables(realP, imagP, halfSize, false);
}

} // namespace blink

