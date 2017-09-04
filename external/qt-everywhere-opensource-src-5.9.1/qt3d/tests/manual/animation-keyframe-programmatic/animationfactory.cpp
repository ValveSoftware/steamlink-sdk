/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
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
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
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

#include "animationfactory.h"

using namespace Qt3DAnimation;

AnimationFactory::AnimationFactory(QObject *parent)
    : QObject(parent)
{
    updateClipData();
}

void AnimationFactory::updateClipData()
{
    m_clipData.clearChannels();

    // Add a channel for a Location animation
    QChannel location(QLatin1String("Location"));

    QChannelComponent locationX(QLatin1String("Location X"));
    locationX.appendKeyFrame(QKeyFrame({0.0f, 0.0f}, {-1.0f, 0.0f}, {1.0f, 0.0f}));
    locationX.appendKeyFrame(QKeyFrame({2.45f, 5.0f}, {1.45f, 5.0f}, {3.45f, 5.0f}));

    QChannelComponent locationY(QLatin1String("Location Y"));
    locationY.appendKeyFrame(QKeyFrame({0.0f, 0.0f}, {-1.0f, 0.0f}, {1.0f, 0.0f}));
    locationY.appendKeyFrame(QKeyFrame({2.45f, 0.0f}, {1.45f, 0.0f}, {3.45f, 0.0f}));

    QChannelComponent locationZ(QLatin1String("Location Z"));
    locationZ.appendKeyFrame(QKeyFrame({0.0f, 0.0f}, {-1.0f, 0.0f}, {1.0f, 0.0f}));
    locationZ.appendKeyFrame(QKeyFrame({2.45f, 0.0f}, {1.45f, 0.0f}, {3.45f, 0.0f}));

    location.appendChannelComponent(locationX);
    location.appendChannelComponent(locationY);
    location.appendChannelComponent(locationZ);

    m_clipData.appendChannel(location);
}
