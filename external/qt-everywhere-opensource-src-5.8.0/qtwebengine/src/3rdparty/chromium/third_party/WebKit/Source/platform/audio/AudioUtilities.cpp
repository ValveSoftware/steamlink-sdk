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

#include "platform/audio/AudioUtilities.h"
#include "wtf/Assertions.h"
#include "wtf/MathExtras.h"

namespace blink {

namespace AudioUtilities {

float decibelsToLinear(float decibels)
{
    return powf(10, 0.05f * decibels);
}

float linearToDecibels(float linear)
{
    ASSERT(linear >= 0);

    return 20 * log10f(linear);
}

double discreteTimeConstantForSampleRate(double timeConstant, double sampleRate)
{
    return 1 - exp(-1 / (sampleRate * timeConstant));
}

size_t timeToSampleFrame(double time, double sampleRate)
{
    ASSERT(time >= 0);
    double frame = round(time * sampleRate);

    // Just return the largest possible size_t value if necessary.
    if (frame >= std::numeric_limits<size_t>::max()) {
        return std::numeric_limits<size_t>::max();
    }

    return static_cast<size_t>(frame);
}

bool isValidAudioBufferSampleRate(float sampleRate)
{
    return sampleRate >= minAudioBufferSampleRate() && sampleRate <= maxAudioBufferSampleRate();
}

float minAudioBufferSampleRate()
{
    // crbug.com/344375
    return 3000;
}

float maxAudioBufferSampleRate()
{
    // Windows can support audio sampling rates this high, so allow AudioBuffer rates this high as well.
    return 192000;
}
} // namespace AudioUtilities

} // namespace blink

