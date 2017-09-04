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

#ifndef MOCKMETADATAWRITERCONTROL_H
#define MOCKMETADATAWRITERCONTROL_H

#include <QObject>
#include <QMap>

#include "qmetadatawritercontrol.h"

class MockMetaDataWriterControl : public QMetaDataWriterControl
{
    Q_OBJECT
public:
    MockMetaDataWriterControl(QObject *parent = 0)
        : QMetaDataWriterControl(parent)
        , m_available(false)
        , m_writable(false)
    {
    }

    bool isMetaDataAvailable() const { return m_available; }
    void setMetaDataAvailable(bool available)
    {
        if (m_available != available)
            emit metaDataAvailableChanged(m_available = available);
    }
    QStringList availableMetaData() const { return m_data.keys(); }

    bool isWritable() const { return m_writable; }
    void setWritable(bool writable) { emit writableChanged(m_writable = writable); }

    QVariant metaData(const QString &key) const { return m_data.value(key); }//Getting the metadata from Multimediakit
    void setMetaData(const QString &key, const QVariant &value)
    {
        if (m_data[key] != value) {
            if (value.isNull())
                m_data.remove(key);
            else
                m_data[key] = value;

            emit metaDataChanged(key, value);
            emit metaDataChanged();
        }
    }

    using QMetaDataWriterControl::metaDataChanged;

    void populateMetaData()
    {
        m_available = true;
    }
    void setWritable()
    {
        emit writableChanged(true);
    }
    void setMetaDataAvailable()
    {
        emit metaDataAvailableChanged(true);
    }

    bool m_available;
    bool m_writable;
    QMap<QString, QVariant> m_data;
};

#endif // MOCKMETADATAWRITERCONTROL_H
