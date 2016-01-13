/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "wtf/RefCounted.h"
#include "wtf/Functional.h"
#include <gtest/gtest.h>

namespace {

static int returnFortyTwo()
{
    return 42;
}

TEST(FunctionalTest, Basic)
{
    Function<int ()> emptyFunction;
    EXPECT_TRUE(emptyFunction.isNull());

    Function<int ()> returnFortyTwoFunction = bind(returnFortyTwo);
    EXPECT_FALSE(returnFortyTwoFunction.isNull());
    EXPECT_EQ(42, returnFortyTwoFunction());
}

static int multiplyByTwo(int n)
{
    return n * 2;
}

static double multiplyByOneAndAHalf(double d)
{
    return d * 1.5;
}

TEST(FunctionalTest, UnaryBind)
{
    Function<int ()> multiplyFourByTwoFunction = bind(multiplyByTwo, 4);
    EXPECT_EQ(8, multiplyFourByTwoFunction());

    Function<double ()> multiplyByOneAndAHalfFunction = bind(multiplyByOneAndAHalf, 3);
    EXPECT_EQ(4.5, multiplyByOneAndAHalfFunction());
}

static int multiply(int x, int y)
{
    return x * y;
}

static int subtract(int x, int y)
{
    return x - y;
}

TEST(FunctionalTest, BinaryBind)
{
    Function<int ()> multiplyFourByTwoFunction = bind(multiply, 4, 2);
    EXPECT_EQ(8, multiplyFourByTwoFunction());

    Function<int ()> subtractTwoFromFourFunction = bind(subtract, 4, 2);
    EXPECT_EQ(2, subtractTwoFromFourFunction());
}

class A {
public:
    explicit A(int i)
        : m_i(i)
    {
    }

    int f() { return m_i; }
    int addF(int j) { return m_i + j; }

private:
    int m_i;
};

TEST(FunctionalTest, MemberFunctionBind)
{
    A a(10);
    Function<int ()> function1 = bind(&A::f, &a);
    EXPECT_EQ(10, function1());

    Function<int ()> function2 = bind(&A::addF, &a, 15);
    EXPECT_EQ(25, function2());
}

class Number : public RefCounted<Number> {
public:
    static PassRefPtr<Number> create(int value)
    {
        return adoptRef(new Number(value));
    }

    ~Number()
    {
        m_value = 0;
    }

    int value() const { return m_value; }

private:
    explicit Number(int value)
        : m_value(value)
    {
    }

    int m_value;
};

static int multiplyNumberByTwo(Number* number)
{
    return number->value() * 2;
}

TEST(FunctionalTest, RefCountedStorage)
{
    RefPtr<Number> five = Number::create(5);
    Function<int ()> multiplyFiveByTwoFunction = bind(multiplyNumberByTwo, five);
    EXPECT_EQ(10, multiplyFiveByTwoFunction());

    Function<int ()> multiplyFourByTwoFunction = bind(multiplyNumberByTwo, Number::create(4));
    EXPECT_EQ(8, multiplyFourByTwoFunction());

    RefPtr<Number> six = Number::create(6);
    Function<int ()> multiplySixByTwoFunction = bind(multiplyNumberByTwo, six.release());
    EXPECT_FALSE(six);
    EXPECT_EQ(12, multiplySixByTwoFunction());
}

} // namespace
