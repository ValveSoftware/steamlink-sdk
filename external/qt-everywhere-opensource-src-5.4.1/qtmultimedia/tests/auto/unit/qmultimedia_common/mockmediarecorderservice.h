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

#ifndef MOCKSERVICE_H
#define MOCKSERVICE_H

#include "qmediaservice.h"

#include "mockaudioencodercontrol.h"
#include "mockmediarecordercontrol.h"
#include "mockvideoencodercontrol.h"
#include "mockaudioinputselector.h"
#include "mockmediacontainercontrol.h"
#include "mockmetadatawritercontrol.h"
#include "mockavailabilitycontrol.h"
#include "mockaudioprobecontrol.h"

class MockMediaRecorderService : public QMediaService
{
    Q_OBJECT
public:
    MockMediaRecorderService(QObject *parent = 0, QMediaControl *control = 0, MockAvailabilityControl *availability = 0):
        QMediaService(parent),
        mockControl(control),
        mockAvailabilityControl(availability),
        hasControls(true)
    {
        mockAudioInputSelector = new MockAudioInputSelector(this);
        mockAudioEncoderControl = new MockAudioEncoderControl(this);
        mockFormatControl = new MockMediaContainerControl(this);
        mockVideoEncoderControl = new MockVideoEncoderControl(this);
        mockMetaDataControl = new MockMetaDataWriterControl(this);
        mockAudioProbeControl = new MockAudioProbeControl(this);
    }

    QMediaControl* requestControl(const char *name)
    {
        if (hasControls && qstrcmp(name,QAudioEncoderSettingsControl_iid) == 0)
            return mockAudioEncoderControl;
        if (hasControls && qstrcmp(name,QAudioInputSelectorControl_iid) == 0)
            return mockAudioInputSelector;
        if (hasControls && qstrcmp(name,QMediaRecorderControl_iid) == 0)
            return mockControl;
        if (hasControls && qstrcmp(name,QMediaContainerControl_iid) == 0)
            return mockFormatControl;
        if (hasControls && qstrcmp(name,QVideoEncoderSettingsControl_iid) == 0)
            return mockVideoEncoderControl;
        if (hasControls && qstrcmp(name, QMetaDataWriterControl_iid) == 0)
            return mockMetaDataControl;
        if (hasControls && qstrcmp(name, QMediaAvailabilityControl_iid) == 0)
            return mockAvailabilityControl;
        if (hasControls && qstrcmp(name, QMediaAudioProbeControl_iid) == 0)
            return mockAudioProbeControl;

        return 0;
    }

    void releaseControl(QMediaControl*)
    {
    }

    QMediaControl   *mockControl;
    QAudioInputSelectorControl  *mockAudioInputSelector;
    QAudioEncoderSettingsControl    *mockAudioEncoderControl;
    QMediaContainerControl     *mockFormatControl;
    QVideoEncoderSettingsControl    *mockVideoEncoderControl;
    MockMetaDataWriterControl *mockMetaDataControl;
    MockAvailabilityControl *mockAvailabilityControl;
    MockAudioProbeControl *mockAudioProbeControl;

    bool hasControls;
};

#endif // MOCKSERVICE_H
