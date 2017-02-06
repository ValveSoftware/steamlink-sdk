.import QtQuick.LocalStorage 2.0 as Sql

function test() {
    var db = Sql.LocalStorage.openDatabaseSync("QmlTestDB-selection", "", "Test database from Qt autotests", 1000000);
    var r="transaction_not_finished";

    db.transaction(
        function(tx) {
            tx.executeSql('CREATE TABLE IF NOT EXISTS Greeting(salutation TEXT, salutee TEXT)');
            tx.executeSql('INSERT INTO Greeting VALUES(?, ?)', [ 'hello', 'world' ]);
            tx.executeSql('INSERT INTO Greeting VALUES(?, ?)', [ 'hello', 'world' ]);
            tx.executeSql('CREATE TABLE IF NOT EXISTS TypeTest(num INTEGER, txt1 TEXT, txt2 TEXT)');
            tx.executeSql("INSERT INTO TypeTest VALUES(1, null, 'hello')");
        }
    );

    db.transaction(
        function(tx) {
            tx.executeSql('INSERT INTO Greeting VALUES(?, ?)', [ 'hello', 'world' ]);
            tx.executeSql('INSERT INTO Greeting VALUES(?, ?)', [ 'hello', 'world' ]);
            var rs = tx.executeSql('SELECT * FROM Greeting');
            if ( rs.rows.length != 4 )
                r = "SELECT RETURNED WRONG VALUE "+rs.rows.length+rs.rows[0]+rs.rows[1]
            else
                r = "passed";
        }
    );
    if (r == "passed") {
        db.transaction(function (tx) {
            r = "";
            var firstRow = tx.executeSql("SELECT * FROM TypeTest").rows.item(0);
            if (typeof(firstRow.num) != "number")
                r += " num:" + firstRow.num+ "type:" + typeof(firstRow.num);
            if (typeof(firstRow.txt1) != "object" || firstRow.txt1 != null)
                r += " txt1:" + firstRow.txt1 + " type:" + typeof(firstRow.txt1);
            if (typeof(firstRow.txt2) != "string" || firstRow.txt2 != "hello")
                r += " txt2:" + firstRow.txt2 + " type:" + typeof(firstRow.txt2);
            if (r == "")
               r = "passed";
            else
               r = "SELECT RETURNED VALUES WITH WRONG TYPES " + r;
        });
    }

    return r;
}
