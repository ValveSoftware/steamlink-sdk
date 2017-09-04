.import QtQuick.LocalStorage 2.0 as Sql

function test() {
    var r="transaction_not_finished";
    var db = Sql.LocalStorage.openDatabaseSync("QmlTestDB-reopen", "1.0", "Test database from Qt autotests", 1000000);

    db.transaction(
        function(tx) {
            var rs = tx.executeSql('SELECT * FROM Greeting');
            if (rs.rows.item(0).salutation == 'hello')
                r = "passed";
            else
                r = "FAILED";
        }
    );

    return r;
}
