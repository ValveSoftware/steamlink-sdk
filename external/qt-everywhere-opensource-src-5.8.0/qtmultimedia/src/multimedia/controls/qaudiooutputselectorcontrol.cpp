/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

#include "qaudiooutputselectorcontrol.h"

QT_BEGIN_NAMESPACE

/*!
    \class QAudioOutputSelectorControl

    \brief The QAudioOutputSelectorControl class provides an audio output selector media control.
    \inmodule QtMultimedia
    \ingroup multimedia_control

    The QAudioOutputSelectorControl class provides descriptions of the audio
    outputs available on a system and allows one to be selected as the audio
    output of a media service.

    The interface name of QAudioOutputSelectorControl is \c org.qt-project.qt.audiooutputselectorcontrol/5.0 as
    defined in QAudioOutputSelectorControl_iid.

    \sa QMediaService::requestControl()
*/

/*!
    \macro QAudioOutputSelectorControl_iid

    \c org.qt-project.qt.audiooutputselectorcontrol/5.0

    Defines the interface name of the QAudioOutputSelectorControl class.

    \relates QAudioOutputSelectorControl
*/

/*!
    Constructs a new audio output selector control with the given \a parent.
*/
QAudioOutputSelectorControl::QAudioOutputSelectorControl(QObject *parent)
    :QMediaControl(parent)
{
}

/*!
    Destroys an audio output selector control.
*/
QAudioOutputSelectorControl::~QAudioOutputSelectorControl()
{
}

/*!
    \fn QList<QString> QAudioOutputSelectorControl::availableOutputs() const

    Returns a list of the names of the available audio outputs.
*/

/*!
    \fn QString QAudioOutputSelectorControl::outputDescription(const QString& name) const

    Returns the description of the output \a name.
*/

/*!
    \fn QString QAudioOutputSelectorControl::defaultOutput() const

    Returns the name of the default audio output.
*/

/*!
    \fn QString QAudioOutputSelectorControl::activeOutput() const

    Returns the name of the currently selected audio output.
*/

/*!
    \fn QAudioOutputSelectorControl::setActiveOutput(const QString& name)

    Set the active audio output to \a name.
*/

/*!
    \fn QAudioOutputSelectorControl::activeOutputChanged(const QString& name)

    Signals that the audio output has changed to \a name.
*/

/*!
    \fn QAudioOutputSelectorControl::availableOutputsChanged()

    Signals that list of available outputs has changed.
*/

#include "moc_qaudiooutputselectorcontrol.cpp"
QT_END_NAMESPACE

