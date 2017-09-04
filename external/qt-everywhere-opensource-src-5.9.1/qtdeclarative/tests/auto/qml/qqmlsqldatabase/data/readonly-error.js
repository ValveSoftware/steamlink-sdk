.import QtQuick.LocalStorage 2.0 as Sql

function test() {
    var r="transaction_not_finished";
    var db = Sql.LocalStorage.openDatabaseSync("QmlTestDB-readonly-error", "1.0", "Test database from Qt autotests", 1000000);

    db.transaction(
        function(tx) {
            tx.executeSql('CREATE TABLE IF NOT EXISTS Greeting(salutation TEXT, salutee TEXT)');
            tx.executeSql('INSERT INTO Greeting VALUES(?, ?)', [ 'hello', 'world' ]);
        }
    );

    try {
        db.readTransaction(
            function(tx) {
                tx.executeSql('INSERT INTO Greeting VALUES(?, ?)', [ 'hello', 'world' ]);
                r = "FAILED";
            }
        );
    } catch (err) {
        if (err.message == "Read-only Transaction")
            r = "passed";
        else
            r = "WRONG ERROR="+err.message;
    }

    return r;
}
