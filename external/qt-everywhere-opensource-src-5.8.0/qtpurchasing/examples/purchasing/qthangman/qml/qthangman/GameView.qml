/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Purchasing module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3-COMM$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.2
import QtQuick.Controls 1.1

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
        font.family: "Helvetica Neue"
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
        font.family: "Helvetica Neue"
        font.weight: Font.Light
    }
}
