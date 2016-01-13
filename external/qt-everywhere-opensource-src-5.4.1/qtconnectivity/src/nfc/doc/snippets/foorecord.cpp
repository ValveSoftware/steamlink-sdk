/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the documentation of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
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
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
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

#include "foorecord.h"
#include <qndefrecord.h>

//! [Declare foo record]
Q_DECLARE_NDEFRECORD(QQmlNdefFooRecord, QNdefRecord::ExternalRtd, "com.example:f")
//! [Declare foo record]

//! [createFooRecord]
static inline QNdefRecord createFooRecord()
{
    QNdefRecord foo;
    foo.setTypeNameFormat(QNdefRecord::ExternalRtd);
    foo.setType("com.example:f");
    foo.setPayload(QByteArray(sizeof(int), char(0)));
    return foo;
}
//! [createFooRecord]

//! [copyFooRecord]
static inline QNdefRecord copyFooRecord(const QNdefRecord &record)
{
    if (record.typeNameFormat() != QNdefRecord::ExternalRtd)
        return createFooRecord();
    if (record.type() != "com.example:f")
        return createFooRecord();

    return record;
}
//! [copyFooRecord]

//! [Constructors]
QQmlNdefFooRecord::QQmlNdefFooRecord(QObject *parent)
:   QQmlNdefRecord(createFooRecord(), parent)
{
}

QQmlNdefFooRecord::QQmlNdefFooRecord(const QNdefRecord &record, QObject *parent)
:   QQmlNdefRecord(copyFooRecord(record), parent)
{
}
//! [Constructors]

QQmlNdefFooRecord::~QQmlNdefFooRecord()
{
}

int QQmlNdefFooRecord::foo() const
{
    QByteArray payload = record().payload();

    int value = payload.at(0) << 24 |
                payload.at(1) << 16 |
                payload.at(2) << 8 |
                payload.at(3) << 0;

    return value;
}

void QQmlNdefFooRecord::setFoo(int value)
{
    if (foo() == value)
        return;

    QByteArray payload;
    payload[0] = (value >> 24) & 0xff;
    payload[1] = (value >> 16) & 0xff;
    payload[2] = (value >> 8) & 0xff;
    payload[3] = (value >> 0) & 0xff;

    QNdefRecord r = record();
    r.setPayload(payload);
    setRecord(r);
    emit fooChanged();
}
