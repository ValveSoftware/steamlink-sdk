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

#ifndef MOCKAUDIOINPUTSELECTOR_H
#define MOCKAUDIOINPUTSELECTOR_H

#include "qaudioinputselectorcontrol.h"

class MockAudioInputSelector : public QAudioInputSelectorControl
{
    Q_OBJECT
public:
    MockAudioInputSelector(QObject *parent):
        QAudioInputSelectorControl(parent)
    {
        m_names << "device1" << "device2" << "device3";
        m_descriptions << "dev1 comment" << "dev2 comment" << "dev3 comment";
        m_audioInput = "device1";
        emit availableInputsChanged();
    }
    ~MockAudioInputSelector() {}

    QList<QString> availableInputs() const
    {
        return m_names;
    }

    QString inputDescription(const QString& name) const
    {
        QString desc;

        for (int i = 0; i < m_names.count(); i++) {
            if (m_names.at(i).compare(name) == 0) {
                desc = m_descriptions.at(i);
                break;
            }
        }
        return desc;
    }

    QString defaultInput() const
    {
        return m_names.at(0);
    }

    QString activeInput() const
    {
        return m_audioInput;
    }

public Q_SLOTS:

    void setActiveInput(const QString& name)
    {
        m_audioInput = name;
        emit activeInputChanged(name);
    }

    void addInputs()
    {
        m_names << "device4";
        emit availableInputsChanged();
    }

    void removeInputs()
    {
        m_names.clear();
        emit availableInputsChanged();
    }

private:
    QString         m_audioInput;
    QList<QString>  m_names;
    QList<QString>  m_descriptions;
};



#endif // MOCKAUDIOINPUTSELECTOR_H
