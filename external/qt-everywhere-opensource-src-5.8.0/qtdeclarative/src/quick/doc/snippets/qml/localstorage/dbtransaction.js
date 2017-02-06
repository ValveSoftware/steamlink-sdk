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

//![0]
var db = LocalStorage.openDatabaseSync("ActivityTrackDB", "", "Database tracking sports activities", 1000000);
db.transaction(
    try {
        function(tx) {
            tx.executeSql('INSERT INTO trip_log VALUES(?, ?, ?)',
                          [ '01/10/2016','Sylling - Vikersund', '53' ]);
        }
    }   catch (err) {
            console.log("Error inserting into table Greeting: " + err);
        }
)
//![0]

//![1]
// Retrieve activity date, description and distance based on minimum
// distance parameter Pdistance
function db_distance_select(Pdistance)
{
var db = LocalStorage.openDatabaseSync("ActivityTrackDB", "", "Database tracking sports activities", 1000000);
db.transaction(
    function(tx) {
        var results = tx.executeSql('SELECT rowid,
                                            date,
                                            trip_desc,
                                            distance FROM trip_log
                                     where distance >= ?',[Pdistance]');
        for (var i = 0; i < results.rows.length; i++) {
            listModel.append({"id": results.rows.item(i).rowid,
                              "date": results.rows.item(i).date,
                              "trip_desc": results.rows.item(i).trip_desc,
                              "distance": results.rows.item(i).distance});
        }
    }
}
//![1]
//![2]
var db = LocalStorage.openDatabaseSync("ActivityTrackDB", "", "Database tracking sports activities", 1000000);
if (db.version == '0.1') {
    db.changeVersion('0.1', '0.2', function(tx) {
        tx.executeSql('INSERT INTO trip_log VALUES(?, ?, ?)',
                    [ '01/10/2016','Sylling - Vikersund', '53' ]);
    }
});
//![2]
