/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Mobility Components.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtWidgets>

#include <qaudiorecorder.h>
#include <qmediaservice.h>

#include <QtMultimedia/qaudioformat.h>

#include "audiorecorder.h"

AudioRecorder::AudioRecorder()
{
//! [create-objs-1]
    capture = new QAudioRecorder();
//! [create-objs-1]

    // set a default file
    capture->setOutputLocation(QUrl("test.raw"));

    QWidget *window = new QWidget;
    QGridLayout* layout = new QGridLayout;

    QLabel* deviceLabel = new QLabel;
    deviceLabel->setText("Devices");
    deviceBox = new QComboBox(this);
    deviceBox->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);

    QLabel* codecLabel = new QLabel;
    codecLabel->setText("Codecs");
    codecsBox = new QComboBox(this);
    codecsBox->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);

    QLabel* qualityLabel = new QLabel;
    qualityLabel->setText("Quality");
    qualityBox = new QComboBox(this);
    qualityBox->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);

//! [device-list]
    for(int i = 0; i < audiosource->deviceCount(); i++)
        deviceBox->addItem(audiosource->name(i));
//! [device-list]

//! [codec-list]
    QStringList codecs = capture->supportedAudioCodecs();
    for(int i = 0; i < codecs.count(); i++)
        codecsBox->addItem(codecs.at(i));
//! [codec-list]

    qualityBox->addItem("Low");
    qualityBox->addItem("Medium");
    qualityBox->addItem("High");

    connect(capture, SIGNAL(durationChanged(qint64)), this, SLOT(updateProgress(qint64)));
    connect(capture, SIGNAL(stateChanged(QMediaRecorder::State)), this, SLOT(stateChanged(QMediaRecorder::State)));

    layout->addWidget(deviceLabel,0,0,Qt::AlignHCenter);
    connect(deviceBox,SIGNAL(activated(int)),SLOT(deviceChanged(int)));
    layout->addWidget(deviceBox,0,1,1,3,Qt::AlignLeft);

    layout->addWidget(codecLabel,1,0,Qt::AlignHCenter);
    connect(codecsBox,SIGNAL(activated(int)),SLOT(codecChanged(int)));
    layout->addWidget(codecsBox,1,1,Qt::AlignLeft);

    layout->addWidget(qualityLabel,1,2,Qt::AlignHCenter);
    connect(qualityBox,SIGNAL(activated(int)),SLOT(qualityChanged(int)));
    layout->addWidget(qualityBox,1,3,Qt::AlignLeft);

    fileButton = new QPushButton(this);
    fileButton->setText(tr("Output File"));
    connect(fileButton,SIGNAL(clicked()),SLOT(selectOutputFile()));
    layout->addWidget(fileButton,3,0,Qt::AlignHCenter);

    button = new QPushButton(this);
    button->setText(tr("Record"));
    connect(button,SIGNAL(clicked()),SLOT(toggleRecord()));
    layout->addWidget(button,3,3,Qt::AlignHCenter);

    recTime = new QLabel;
    recTime->setText("0 sec");
    layout->addWidget(recTime,4,0,Qt::AlignHCenter);

    window->setLayout(layout);
    setCentralWidget(window);
    window->show();

    active = false;
}

AudioRecorder::~AudioRecorder()
{
    delete capture;
    delete audiosource;
}

void AudioRecorder::updateProgress(qint64 pos)
{
    currentTime = pos;
    if(currentTime == 0) currentTime = 1;
    QString text = QString("%1 secs").arg(currentTime/1000);
    recTime->setText(text);
}

void AudioRecorder::stateChanged(QMediaRecorder::State state)
{
    qWarning()<<"stateChanged() "<<state;
}

void AudioRecorder::deviceChanged(int idx)
{
//! [get-device]
    for(int i = 0; i < audiosource->deviceCount(); i++) {
        if(deviceBox->itemText(idx).compare(audiosource->name(i)) == 0)
            audiosource->setSelectedDevice(i);
    }
//! [get-device]
}

void AudioRecorder::codecChanged(int idx)
{
    Q_UNUSED(idx);
    //capture->setAudioCodec(codecsBox->itemText(idx));
}

void AudioRecorder::qualityChanged(int idx)
{
    Q_UNUSED(idx);
    /*
    if(capture->audioCodec().compare("audio/pcm") == 0) {
        if(qualityBox->itemText(idx).compare("Low") == 0) {
            // 8000Hz mono is 8kbps
            capture->setAudioBitrate(8);
        } else if(qualityBox->itemText(idx).compare("Medium") == 0) {
            // 22050Hz mono is 44.1kbps
            capture->setAudioBitrate(44);
        } else if(qualityBox->itemText(idx).compare("High") == 0) {
            // 44100Hz mono is 88.2kbps
            capture->setAudioBitrate(88);
        }
    }
    */
}

//! [toggle-record]
void AudioRecorder::toggleRecord()
{
    if(!active) {
        recTime->setText("0 sec");
        currentTime = 0;
        capture->record();

        button->setText(tr("Stop"));
        active = true;
    } else {
        capture->stop();
        button->setText(tr("Record"));
        active = false;
    }
}
//! [toggle-record]

void AudioRecorder::selectOutputFile()
{
    QStringList fileNames;

    QFileDialog dialog(this);

    dialog.setFileMode(QFileDialog::AnyFile);
    if (dialog.exec())
        fileNames = dialog.selectedFiles();

    if(fileNames.size() > 0)
        capture->setOutputLocation(QUrl(fileNames.first()));
}
