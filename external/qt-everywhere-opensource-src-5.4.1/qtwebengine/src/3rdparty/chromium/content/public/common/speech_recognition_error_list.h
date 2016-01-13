// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file intentionally does not have header guards, it's included
// inside a macro to generate enum values.

#ifndef DEFINE_SPEECH_RECOGNITION_ERROR
#error "DEFINE_SPEECH_RECOGNITION_ERROR must be defined before including this."
#endif

// There was no error.
DEFINE_SPEECH_RECOGNITION_ERROR(NONE, 0)

// The user or a script aborted speech input.
DEFINE_SPEECH_RECOGNITION_ERROR(ABORTED, 1)

// There was an error with recording audio.
DEFINE_SPEECH_RECOGNITION_ERROR(AUDIO, 2)

// There was a network error.
DEFINE_SPEECH_RECOGNITION_ERROR(NETWORK, 3)

// Not allowed for privacy or security reasons.
DEFINE_SPEECH_RECOGNITION_ERROR(NOT_ALLOWED, 4)

// No speech heard before timeout.
DEFINE_SPEECH_RECOGNITION_ERROR(NO_SPEECH, 5)

// Speech was heard, but could not be interpreted.
DEFINE_SPEECH_RECOGNITION_ERROR(NO_MATCH, 6)

// There was an error in the speech recognition grammar.
DEFINE_SPEECH_RECOGNITION_ERROR(BAD_GRAMMAR, 7)

// Reports the highest value. Update when adding a new error code.
DEFINE_SPEECH_RECOGNITION_ERROR(LAST, 7)

