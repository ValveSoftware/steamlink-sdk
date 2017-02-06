// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wtf/Assertions.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/StringBuilder.h"
#include <stdio.h>

#if !LOG_DISABLED
namespace WTF {

static const int kPrinterBufferSize = 256;
static char gBuffer[kPrinterBufferSize];
static StringBuilder gBuilder;

static void vprint(const char* format, va_list args)
{
    int written = vsnprintf(gBuffer, kPrinterBufferSize, format, args);
    if (written > 0 && written < kPrinterBufferSize)
        gBuilder.append(gBuffer);
}

TEST(AssertionsTest, ScopedLogger)
{
    ScopedLogger::setPrintFuncForTests(vprint);
    {
        WTF_CREATE_SCOPED_LOGGER(a, "a1");
        {
            WTF_CREATE_SCOPED_LOGGER_IF(b, false, "b1");
            {
                WTF_CREATE_SCOPED_LOGGER(c, "c");
                {
                    WTF_CREATE_SCOPED_LOGGER(d, "d %d %s", -1, "hello");
                }
            }
            WTF_APPEND_SCOPED_LOGGER(b, "b2");
        }
        WTF_APPEND_SCOPED_LOGGER(a, "a2 %.1f", 0.5);
    }

    EXPECT_EQ(
        "( a1\n"
        "  ( c\n"
        "    ( d -1 hello )\n"
        "  )\n"
        "  a2 0.5\n"
        ")\n", gBuilder.toString());
};

} // namespace WTF
#endif // !LOG_DISABLED
