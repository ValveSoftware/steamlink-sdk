/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
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

import QtQuick 2.1
import QtQuick.Window 2.1
import QtQuick.Layouts 1.0
import QtQuick.Controls 1.0
import QtQuick.Dialogs 1.0
import QtMultimedia 5.0
import QtWinExtras 1.0 as Win

Window {
    id: window

    title: qsTr("QtWinExtras Quick Player")

    width: 300
    height: 60
    minimumWidth: row.implicitWidth + 18
    minimumHeight: column.implicitHeight + 18

    //! [color]
    color: dwm.compositionEnabled ? "transparent" : dwm.realColorizationColor
    //! [color]

    //! [dwm]
    Win.DwmFeatures {
        id: dwm

        topGlassMargin: -1
        leftGlassMargin: -1
        rightGlassMargin: -1
        bottomGlassMargin: -1
    }
    //! [dwm]

    //! [taskbar]
    Win.TaskbarButton {
        id: taskbar

        progress.value: mediaPlayer.position
        progress.maximum: mediaPlayer.duration
        progress.visible: mediaPlayer.hasAudio
        progress.paused: mediaPlayer.playbackState === MediaPlayer.PausedState

        overlay.iconSource: mediaPlayer.playbackState === MediaPlayer.PlayingState ? "qrc:/play-32.png" :
                            mediaPlayer.playbackState === MediaPlayer.PausedState ? "qrc:/pause-32.png" : "qrc:/stop-32.png"
    }
    //! [taskbar]

    //! [thumbbar]
    Win.ThumbnailToolBar {
        id: thumbbar

        Win.ThumbnailToolButton {
            tooltip: qsTr("Rewind")
            iconSource: "qrc:/backward-32.png"

            enabled: mediaPlayer.position > 0
            onClicked: mediaPlayer.seek(mediaPlayer.position - mediaPlayer.duration / 10)
        }

        Win.ThumbnailToolButton {
            tooltip: mediaPlayer.playbackState === MediaPlayer.PlayingState ? qsTr("Pause") : qsTr("Play")
            iconSource: mediaPlayer.playbackState === MediaPlayer.PlayingState ? "qrc:/pause-32.png" : "qrc:/play-32.png"

            enabled: mediaPlayer.hasAudio
            onClicked: mediaPlayer.playbackState === MediaPlayer.PlayingState ? mediaPlayer.pause() : mediaPlayer.play()
        }

        Win.ThumbnailToolButton {
            tooltip: qsTr("Fast forward")
            iconSource: "qrc:/forward-32.png"

            enabled: mediaPlayer.position < mediaPlayer.duration
            onClicked: mediaPlayer.seek(mediaPlayer.position + mediaPlayer.duration / 10)
        }
    }
    //! [thumbbar]

    MediaPlayer {
        id: mediaPlayer
        autoPlay: true
        source : url
        readonly property string title: !!metaData.author && !!metaData.title
                                        ? qsTr("%1 - %2").arg(metaData.author).arg(metaData.title)
                                        : metaData.author || metaData.title || source
    }

    ColumnLayout {
        id: column

        anchors.margins: 9
        anchors.fill: parent

        Label {
            id: infoLabel

            elide: Qt.ElideLeft
            verticalAlignment: Qt.AlignVCenter
            text: mediaPlayer.errorString || mediaPlayer.title
            Layout.minimumHeight: infoLabel.implicitHeight
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        RowLayout {
            id: row

            Button {
                id: openButton

                text: qsTr("...")
                Layout.preferredWidth: openButton.implicitHeight
                onClicked: fileDialog.open()

                FileDialog {
                    id: fileDialog

                    folder : musicUrl
                    title: qsTr("Open file")
                    nameFilters: [qsTr("MP3 files (*.mp3)"), qsTr("All files (*.*)")]
                    onAccepted: mediaPlayer.source = fileDialog.fileUrl
                }
            }

            Button {
                id: playButton

                enabled: mediaPlayer.hasAudio
                Layout.preferredWidth: playButton.implicitHeight
                iconSource: mediaPlayer.playbackState === MediaPlayer.PlayingState ? "qrc:/pause-16.png" : "qrc:/play-16.png"
                onClicked: mediaPlayer.playbackState === MediaPlayer.PlayingState ? mediaPlayer.pause() : mediaPlayer.play()
            }

            Slider {
                id: positionSlider

                Layout.fillWidth: true
                maximumValue: mediaPlayer.duration

                property bool sync: false

                onValueChanged: {
                    if (!sync)
                        mediaPlayer.seek(value)
                }

                Connections {
                    target: mediaPlayer
                    onPositionChanged: {
                        positionSlider.sync = true
                        positionSlider.value = mediaPlayer.position
                        positionSlider.sync = false
                    }
                }
            }

            Label {
                id: positionLabel

                readonly property int minutes: Math.floor(mediaPlayer.position / 60000)
                readonly property int seconds: Math.round((mediaPlayer.position % 60000) / 1000)

                text: Qt.formatTime(new Date(0, 0, 0, 0, minutes, seconds), qsTr("mm:ss"))
            }
        }
    }
}
