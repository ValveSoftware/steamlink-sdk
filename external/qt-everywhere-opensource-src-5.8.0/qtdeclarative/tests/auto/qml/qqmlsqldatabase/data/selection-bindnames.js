.import QtQuick.LocalStorage 2.0 as Sql

function test() {
    var db = Sql.LocalStorage.openDatabaseSync("QmlTestDB-bindnames", "", "Test database from Qt autotests", 1000000);
    var r="transaction_not_finished";

    db.transaction(
        function(tx) {
            tx.executeSql('CREATE TABLE IF NOT EXISTS Greeting(salutation TEXT, salutee TEXT)');
            tx.executeSql('INSERT INTO Greeting VALUES(?, ?)', [ 'hello', 'world' ]);
            tx.executeSql('INSERT INTO Greeting VALUES(?, ?)', [ 'goodbye', 'world' ]);
            tx.executeSql('INSERT INTO Greeting VALUES(?, ?)', [ 'hello', 'world' ]);
            tx.executeSql('INSERT INTO Greeting VALUES(?, ?)', [ 'hello', 'there' ]);
        }
    );

    db.transaction(
        function(tx) {
            var rs = tx.executeSql('SELECT * FROM Greeting WHERE salutation=:p2 AND salutee=:p1', {':p1':'world', ':p2':'hello'});
            if ( rs.rows.length != 2 )
                r = "SELECT RETURNED WRONG VALUE "+rs.rows.length+rs.rows.item(0)+rs.rows.item(1)
            else
                r = "passed";
        }
    );

    return r;
}
