/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef MOCKRADIODATACONTROL_H
#define MOCKRADIODATACONTROL_H

#include "qradiodatacontrol.h"

class MockRadioDataControl : public QRadioDataControl
{
    Q_OBJECT

public:
    MockRadioDataControl(QObject *parent):
        QRadioDataControl(parent), m_err(QRadioData::NoError),
        m_errstr("")
    {
    }

    using QRadioDataControl::error;

    QRadioData::Error error() const
    {
        return m_err;
    }

    QString errorString() const
    {
        return m_errstr;
    }

    QString stationId() const
    {
        return m_stationId;
    }

    QRadioData::ProgramType programType() const
    {
        return m_programType;
    }

    QString programTypeName() const
    {
        return m_programTypeName;
    }

    QString stationName() const
    {
        return m_stationName;
    }

    QString radioText() const
    {
        return m_radioText;
    }

    void setAlternativeFrequenciesEnabled(bool enabled)
    {
        m_alternativeFrequenciesEnabled = enabled;
        emit alternativeFrequenciesEnabledChanged(m_alternativeFrequenciesEnabled);
    }

    bool isAlternativeFrequenciesEnabled() const
    {
        return m_alternativeFrequenciesEnabled;
    }

    void forceRT( QString text )
    {
        m_radioText = text;
        emit radioTextChanged(m_radioText);
    }

    void forceProgramType( int pty )
    {
        m_programType = static_cast<QRadioData::ProgramType>(pty);
        emit programTypeChanged(m_programType);
    }

    void forcePTYN( QString ptyn )
    {
        m_programTypeName = ptyn;
        emit programTypeNameChanged(m_programTypeName);
    }

    void forcePI( QString pi )
    {
        m_stationId = pi;
        emit stationIdChanged(m_stationId);
    }

    void forcePS( QString ps )
    {
        m_stationName = ps;
        emit stationNameChanged(m_stationName);
    }

public:
    QString m_radioText;
    QRadioData::ProgramType m_programType;
    QString m_programTypeName;
    QString m_stationId;
    QString m_stationName;
    bool m_alternativeFrequenciesEnabled;

    QRadioData::Error m_err;
    QString m_errstr;
};

#endif // MOCKRADIODATACONTROL_H
