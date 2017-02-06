/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Mobility Components.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef TRACE_H
#define TRACE_H

#include <QDebug>

#define ENABLE_TRACE
//#define VERBOSE_TRACE

namespace Trace {

class NullDebug
{
public:
    template <typename T>
    NullDebug &operator<<(const T &) { return *this; }
};

inline NullDebug nullDebug() { return NullDebug(); }

template <typename T>
struct PtrWrapper
{
    PtrWrapper(const T *ptr) : m_ptr(ptr) { }
    const T *const m_ptr;
};

} // namespace Trace

template <typename T>
inline QDebug &operator<<(QDebug &debug, const Trace::PtrWrapper<T> &wrapper)
{
    QDebugStateSaver saver(debug);
    debug.nospace() << '[' << static_cast<const void *>(wrapper.m_ptr) << ']';
    return debug;
}

#ifdef ENABLE_TRACE
        inline QDebug qtTrace() { return qDebug() << "[qmlvideofx]"; }
#    ifdef VERBOSE_TRACE
        inline QDebug qtVerboseTrace() { return qtTrace(); }
#    else
        inline Trace::NullDebug qtVerboseTrace() { return Trace::nullDebug(); }
#    endif
#else
    inline Trace::NullDebug qtTrace() { return Trace::nullDebug(); }
    inline Trace::NullDebug qtVerboseTrace() { return Trace::nullDebug(); }
#endif

#endif // TRACE_H

