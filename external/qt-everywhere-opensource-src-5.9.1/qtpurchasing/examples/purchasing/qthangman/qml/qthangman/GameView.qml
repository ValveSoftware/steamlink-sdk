/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Purchasing module of the Qt Toolkit.
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

import QtQuick 2.2
import QtQuick.Controls 1.1
import "."

Item {
    id: gameView

    function allContained(owned, word)
    {
        for (var i=0; i<word.length; ++i) {
            if (owned.indexOf(word.charAt(i)) < 0)
                return false
        }
        return true
    }

    property bool gameOver: applicationData.errorCount > 8
    property bool success: applicationData.word.length > 0 && !gameOver && allContained(applicationData.lettersOwned, applicationData.word)

    property alias globalButtonHeight: letterSelector.keyHeight

    onGameOverChanged: {
        if (gameOver)
            applicationData.gameOverReveal();
    }

    onSuccessChanged: {
        if (success === true)
            applicationData.wordsGuessedCorrectly += 1;
    }

    Connections {
        target: applicationData
        onWordChanged: {
            applicationData.wordsGiven += 1;
        }
    }

    SimpleButton {
        id: vowelsAvailableText
        height: globalButtonHeight
        width: parent.width * 0.25
        visible: !applicationData.vowelsUnlocked
        text: "Vowels: " + applicationData.vowelsAvailable
        anchors.left: parent.left
        anchors.verticalCenter: helpButton.verticalCenter
        anchors.margins: topLevel.globalMargin
        onClicked: {
            pageStack.push(Qt.resolvedUrl("StoreView.qml"));
        }
    }

    ScoreItem {
        anchors.margins: topLevel.globalMargin
        anchors.right: helpButton.left
        anchors.verticalCenter: helpButton.verticalCenter
        height: globalButtonHeight
    }

    SimpleButton {
        id: helpButton
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: topLevel.globalMargin
        height: globalButtonHeight
        width: globalButtonHeight
        text: "?"
        onClicked: {
            pageStack.push(Qt.resolvedUrl("HowToView.qml"))
        }
    }

    Item {
        anchors.top: helpButton.bottom
        anchors.bottom: word.top
        anchors.left: parent.left
        anchors.right: parent.right

        Hangman {
            game: gameView
            anchors.centerIn: parent
            width: Math.min(parent.width, parent.height) * 0.75
            height: width
        }
    }

    Word {
        id: word
        text: applicationData.word
        anchors.bottom: letterSelector.top
        anchors.bottomMargin: parent.height * 0.05
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width * 0.8
        height: parent.height * 0.1
    }

    LetterSelector {
        id: letterSelector
        locked: gameOver || success
        hideVowels: applicationData.vowelsAvailable < 1 && !applicationData.vowelsUnlocked
        anchors.margins: topLevel.globalMargin
        anchors.bottom: guessButton.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: parent.height * 0.23
        onLetterSelected: {
            if (applicationData.isVowel(letter) && !applicationData.vowelsUnlocked) {
                applicationData.vowelsAvailable -= 1;
            }
            applicationData.requestLetter(letter.charAt(0));
        }
    }

    SimpleButton {
        id: revealButton
        anchors.margins: topLevel.globalMargin
        visible: !(gameOver || success)
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        height: globalButtonHeight
        width : letterSelector.keyWidth * 3
        text: "Reveal"
        onClicked: {
            applicationData.reveal();
        }
    }
    SimpleButton {
        id: guessButton
        anchors.margins: topLevel.globalMargin
        visible: !(gameOver || success)
        anchors.bottom: parent.bottom
        anchors.left: revealButton.right
        anchors.right: parent.right
        height: globalButtonHeight
        text: "Guess"
        onClicked: {
            pageStack.push(Qt.resolvedUrl("GuessWordView.qml"));
        }
    }
    SimpleButton {
        id: resetButton
        anchors.margins: topLevel.globalMargin
        visible: gameOver || success
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: globalButtonHeight
        text: "Play Again?"
        onClicked: {
            letterSelector.reset();
            applicationData.reset();
        }
    }

    Text {
        id: gameOverText
        visible: gameOver
        anchors.fill: letterSelector
        text: "Game Over"
        fontSizeMode: Text.Fit
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        minimumPointSize: 8
        font.pointSize: 64
        color: "white"
        font.family: Settings.fontFamily
        font.weight: Font.Light
    }

    Text {
        id: successText
        visible: success
        anchors.fill: letterSelector
        text: "Success"
        fontSizeMode: Text.Fit
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        minimumPointSize: 8
        font.pointSize: 64
        color: "white"
        font.family: Settings.fontFamily
        font.weight: Font.Light
    }
}
