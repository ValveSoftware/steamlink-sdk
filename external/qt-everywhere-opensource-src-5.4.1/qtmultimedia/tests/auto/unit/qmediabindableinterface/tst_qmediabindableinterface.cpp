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

//TESTED_COMPONENT=src/multimedia

#include <QtTest/QtTest>
#include <qmediaobject.h>
#include <qmediacontrol.h>
#include <qmediaservice.h>
#include <qmediarecordercontrol.h>
#include <qmediarecorder.h>
#include <qmetadatawritercontrol.h>
#include <qaudioinputselectorcontrol.h>
#include <qaudioencodersettingscontrol.h>
#include <qmediacontainercontrol.h>
#include <qvideoencodersettingscontrol.h>
#include <qaudioformat.h>

#include "mockmediacontainercontrol.h"
#include "mockmetadatawritercontrol.h"
#include "mockmediarecordercontrol.h"
#include "mockmediaobject.h"

QT_USE_NAMESPACE

class TestBindableService : public QMediaService
{
    Q_OBJECT
public:
    TestBindableService(QObject *parent, QMediaControl *control):
        QMediaService(parent),
        mockControl(control),
        hasControls(true)
    {
        mockContainerControl = new MockMediaContainerControl(parent); //Creating the object for Media
        mockMetaDataControl = new MockMetaDataWriterControl(parent); //Creating the object for MetaData
    }

    QMediaControl* requestControl(const char *name)
    {
        if (hasControls && qstrcmp(name,QMediaRecorderControl_iid) == 0)
            return mockControl;
        if (hasControls && qstrcmp(name,QMediaContainerControl_iid) == 0)
            return mockContainerControl;
        if (hasControls && qstrcmp(name, QMetaDataWriterControl_iid) == 0)
            return mockMetaDataControl;

        return 0;
    }

    void releaseControl(QMediaControl*) {}
    //Initialising the objects for the media
    QMediaControl *mockControl;
    QMediaContainerControl *mockContainerControl;
    MockMetaDataWriterControl *mockMetaDataControl;
    bool hasControls;
};

class tst_QMediaBindableInterface:public QObject
{
    Q_OBJECT
private slots:
    void init()
    {

    }

    void cleanup()
    {

    }

    void testMediaObject() //Verifying the mediaobject api
    {
        MockMediaRecorderControl recorderControl(0);
        TestBindableService service(0, &recorderControl);
        service.mockMetaDataControl->populateMetaData();
        MockMediaObject object(0, &service);
        QMediaRecorder recorder(&object);
        QMediaObject *obj = recorder.mediaObject();
        QVERIFY(obj != NULL);
        QVERIFY(obj->isAvailable());
    }

    void testDestructor() //Invoking the destructor
    {
        MockMediaRecorderControl recorderControl(0);
        TestBindableService service(0, &recorderControl);
        service.mockMetaDataControl->populateMetaData();
        MockMediaObject object(0, &service);
        QMediaRecorder *recorder = new QMediaRecorder(&object);
        QVERIFY(recorder->isAvailable());
        delete recorder;
        recorder = NULL;
    }
};

QTEST_MAIN(tst_QMediaBindableInterface)
#include "tst_qmediabindableinterface.moc"
