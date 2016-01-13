.import QtQuick.LocalStorage 2.0 as Sql

function test() {
    var db = Sql.LocalStorage.openDatabaseSync("QmlTestDB-error-b", "1.0", "Test database from Qt autotests", 1000000);
    var r="transaction_not_finished";

    db.transaction(
        function(tx) {
            tx.executeSql('INSERT INTO Greeting VALUES("junk","junk")');
            notexist[123] = "oops"
        }
    );

    return r;
}
