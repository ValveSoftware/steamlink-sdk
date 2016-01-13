// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebThreadedDataReceiver_h
#define WebThreadedDataReceiver_h

namespace blink {

class WebThread;

class WebThreadedDataReceiver {
public:
    virtual void acceptData(const char* data, int dataLength) = 0;
    virtual WebThread* backgroundThread() = 0;

    virtual ~WebThreadedDataReceiver() { }
};

} // namespace blink

#endif
