.import QtQuick.LocalStorage 2.0 as Sql

function test() {
    var r="transaction_not_finished";
    var db = Sql.LocalStorage.openDatabaseSync("QmlTestDB-creation", "1.0", "Test database from Qt autotests", 1000000);
    db.transaction(
        function(tx) {
            tx.executeSql('CREATE TABLE IF NOT EXISTS Greeting(salutation TEXT, salutee TEXT)');
            tx.executeSql('INSERT INTO Greeting VALUES(?, ?)', [ 'hello', 'world' ]);
            r = "passed";
        }
    );

    return r;
}
