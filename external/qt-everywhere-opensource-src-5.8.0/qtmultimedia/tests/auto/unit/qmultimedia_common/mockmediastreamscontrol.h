/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef MOCKMEDIASTREAMSCONTROL_H
#define MOCKMEDIASTREAMSCONTROL_H

#include "qmediastreamscontrol.h"

class MockStreamsControl : public QMediaStreamsControl
{
public:
    MockStreamsControl(QObject *parent = 0) : QMediaStreamsControl(parent) {}

    int streamCount() { return _streams.count(); }
    void setStreamCount(int count) { _streams.resize(count); }

    StreamType streamType(int index) { return _streams.at(index).type; }
    void setStreamType(int index, StreamType type) { _streams[index].type = type; }

    QVariant metaData(int index, const QString &key) {
        return _streams.at(index).metaData.value(key); }
    void setMetaData(int index, const QString &key, const QVariant &value) {
        _streams[index].metaData.insert(key, value); }

    bool isActive(int index) { return _streams.at(index).active; }
    void setActive(int index, bool state) { _streams[index].active = state; }

private:
    struct Stream
    {
        Stream() : type(UnknownStream), active(false) {}
        StreamType type;
        QMap<QString, QVariant> metaData;
        bool active;
    };

    QVector<Stream> _streams;
};

#endif // MOCKMEDIASTREAMSCONTROL_H
