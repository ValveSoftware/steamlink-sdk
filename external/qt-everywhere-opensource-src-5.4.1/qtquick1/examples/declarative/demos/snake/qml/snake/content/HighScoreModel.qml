/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the demonstration applications of the Qt Toolkit.
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

import QtQuick 1.0

// Models a high score table.
//
// Use this component like this:
//
//  HighScoreModel {
//      id: highScores
//      game: "MyCoolGame"
//  }
//
// Then use either use the top-score properties:
//
//  Text { text: "HI: " + highScores.topScore }
//
// or, use the model in a view:
//
//  ListView {
//      model: highScore
//      delegate: Component {
//                    ... player ... score ...
//                }
//  }
//
// Add new scores via:
//
//  saveScore(newScore)
//
// or:
//
//  savePlayerScore(playerName,newScore)
//
// The best maxScore scores added by this method will be retained in an SQL database,
// and presented in the model and in the topScore/topPlayer properties.
//

ListModel {
    id: model
    property string game: ""
    property int topScore: 0
    property string topPlayer: ""
    property int maxScores: 10

    function __db()
    {
        return openDatabaseSync("HighScoreModel", "1.0", "Generic High Score Functionality for QML", 1000000);
    }
    function __ensureTables(tx)
    {
        tx.executeSql('CREATE TABLE IF NOT EXISTS HighScores(game TEXT, score INT, player TEXT)', []);
    }

    function fillModel() {
        __db().transaction(
            function(tx) {
                __ensureTables(tx);
                var rs = tx.executeSql("SELECT score,player FROM HighScores WHERE game=? ORDER BY score DESC", [game]);
                model.clear();
                if (rs.rows.length > 0) {
                    topScore = rs.rows.item(0).score
                    topPlayer = rs.rows.item(0).player
                    for (var i=0; i<rs.rows.length; ++i) {
                        if (i < maxScores)
                            model.append(rs.rows.item(i))
                    }
                    if (rs.rows.length > maxScores)
                        tx.executeSql("DELETE FROM HighScores WHERE game=? AND score <= ?",
                                            [game, rs.rows.item(maxScores).score]);
                }
            }
        )
    }

    function savePlayerScore(player,score) {
        __db().transaction(
            function(tx) {
                __ensureTables(tx);
                tx.executeSql("INSERT INTO HighScores VALUES(?,?,?)", [game,score,player]);
                fillModel();
            }
        )
    }

    function saveScore(score) {
        savePlayerScore("player",score);
    }

    function clearScores() {
        __db().transaction(
            function(tx) {
                tx.executeSql("DELETE FROM HighScores WHERE game=?", [game]);
                fillModel();
            }
        )
    }

    Component.onCompleted: { fillModel() }
}
