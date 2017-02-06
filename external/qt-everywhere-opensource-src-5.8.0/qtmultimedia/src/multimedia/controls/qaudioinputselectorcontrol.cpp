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

#include "qaudioinputselectorcontrol.h"

QT_BEGIN_NAMESPACE

/*!
    \class QAudioInputSelectorControl

    \brief The QAudioInputSelectorControl class provides an audio input selector media control.
    \inmodule QtMultimedia
    \ingroup multimedia_control

    The QAudioInputSelectorControl class provides descriptions of the audio
    inputs available on a system and allows one to be selected as the audio
    input of a media service.

    The interface name of QAudioInputSelectorControl is \c org.qt-project.qt.audioinputselectorcontrol/5.0 as
    defined in QAudioInputSelectorControl_iid.

    \sa QMediaService::requestControl()
*/

/*!
    \macro QAudioInputSelectorControl_iid

    \c org.qt-project.qt.audioinputselectorcontrol/5.0

    Defines the interface name of the QAudioInputSelectorControl class.

    \relates QAudioInputSelectorControl
*/

/*!
    Constructs a new audio input selector control with the given \a parent.
*/
QAudioInputSelectorControl::QAudioInputSelectorControl(QObject *parent)
    :QMediaControl(parent)
{
}

/*!
    Destroys an audio input selector control.
*/
QAudioInputSelectorControl::~QAudioInputSelectorControl()
{
}

/*!
    \fn QList<QString> QAudioInputSelectorControl::availableInputs() const

    Returns a list of the names of the available audio inputs.
*/

/*!
    \fn QString QAudioInputSelectorControl::inputDescription(const QString& name) const

    Returns the description of the input \a name.
*/

/*!
    \fn QString QAudioInputSelectorControl::defaultInput() const

    Returns the name of the default audio input.
*/

/*!
    \fn QString QAudioInputSelectorControl::activeInput() const

    Returns the name of the currently selected audio input.
*/

/*!
    \fn QAudioInputSelectorControl::setActiveInput(const QString& name)

    Set the active audio input to \a name.
*/

/*!
    \fn QAudioInputSelectorControl::activeInputChanged(const QString& name)

    Signals that the audio input has changed to \a name.
*/

/*!
    \fn QAudioInputSelectorControl::availableInputsChanged()

    Signals that list of available inputs has changed.
*/

#include "moc_qaudioinputselectorcontrol.cpp"
QT_END_NAMESPACE

