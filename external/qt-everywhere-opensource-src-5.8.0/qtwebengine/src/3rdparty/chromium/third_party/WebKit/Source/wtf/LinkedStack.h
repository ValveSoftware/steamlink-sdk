/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LinkedStack_h
#define LinkedStack_h

#include "wtf/Allocator.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace WTF {

template <typename T>
class LinkedStack {
    USING_FAST_MALLOC(LinkedStack);
public:
    LinkedStack() : m_size(0) { }

    // Iterative cleanup to prevent stack overflow problems.
    ~LinkedStack()
    {
        std::unique_ptr<Node> ptr = m_head.release();
        while (ptr)
            ptr = ptr->m_next.release();
    }

    bool isEmpty();

    void push(const T&);
    const T& peek();
    void pop();

    size_t size();

private:
    class Node {
        USING_FAST_MALLOC(LinkedStack::Node);
    public:
        Node(const T&, std::unique_ptr<Node> next);

        T m_data;
        std::unique_ptr<Node> m_next;
    };

    std::unique_ptr<Node> m_head;
    size_t m_size;
};

template <typename T>
LinkedStack<T>::Node::Node(const T& data, std::unique_ptr<Node> next)
    : m_data(data)
    , m_next(next)
{
}

template <typename T>
inline bool LinkedStack<T>::isEmpty()
{
    return !m_head;
}

template <typename T>
inline void LinkedStack<T>::push(const T& data)
{
    m_head = wrapUnique(new Node(data, m_head.release()));
    ++m_size;
}

template <typename T>
inline const T& LinkedStack<T>::peek()
{
    return m_head->m_data;
}

template <typename T>
inline void LinkedStack<T>::pop()
{
    ASSERT(m_head && m_size);
    m_head = m_head->m_next.release();
    --m_size;
}

template <typename T>
inline size_t LinkedStack<T>::size()
{
    return m_size;
}

} // namespace WTF

using WTF::LinkedStack;

#endif
