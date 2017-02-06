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

Item {
    PageHeader {
        id: header
        title: "How to Play"
    }

    Flickable {
        id: helpFlickable
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.left: parent.left
        anchors.margins: topLevel.globalMargin
        clip: true

        contentHeight: helpContent.height + (topLevel.globalMargin * 2)

        Item {
            id: helpContent
            width: parent.width
            height: contentColumn.height
            Column {
                id: contentColumn
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.left: parent.left
                anchors.margins: topLevel.globalMargin
                spacing: topLevel.globalMargin
                Text {
                    height: contentHeight
                    width: parent.width
                    wrapMode: Text.Wrap
                    font.family: ".Helvetica Neue Interface -M3"
                    color: "white"
                    font.pixelSize: helpFlickable.height * 0.04
                    text: "\
Hangman is a classic word game where the objective is to guess a given word \
before you make too many mistakes and the hangman gets hung.\n"
                }

                Word {
                    anchors.margins: topLevel.globalMargin
                    height: topLevel.buttonHeight
                    width: parent.width * .8
                    text: "HANGMAN"
                }

                Text {
                    height: contentHeight
                    width: parent.width
                    wrapMode: Text.Wrap
                    font.family: ".Helvetica Neue Interface -M3"
                    color: "white"
                    font.pixelSize: helpFlickable.height * 0.04
                    text: "\
\nYou play by guessing letters. If you guess a letter that is part of the word \
it will be shown in any locations in the word it is located. If however it is \
not part of the word, another piece will be added and the hangman will be one \
step closer to death. \n"
                }

                Hangman {
                    height: width
                    width: parent.width / 2
                    errorCount: 9
                }

                Text {
                    height: contentHeight
                    width: parent.width
                    wrapMode: Text.Wrap
                    font.family: ".Helvetica Neue Interface -M3"
                    color: "white"
                    font.pixelSize: helpFlickable.height * 0.04
                    text: "\
\nVowels must be purchased, unlocked or earned to be used.  If you guess a word, \
any vowels that have not been guess already will be added to your vowel pool."
                }

                ScoreItem {
                    anchors.margins: topLevel.globalMargin
                    height: topLevel.buttonHeight
                }

                Text {
                    height: contentHeight
                    width: parent.width
                    wrapMode: Text.Wrap
                    font.family: ".Helvetica Neue Interface -M3"
                    color: "white"
                    font.pixelSize: helpFlickable.height * 0.04
                    text: "\
When you guess a word you are rewarded points.  You receive a point for each \
consonant that was guessed as well as a point for any remaining parts of the \
hangman.  You can not do anything with points, they just show how awesome you are.
"
                }
            }
        }
    }
}
