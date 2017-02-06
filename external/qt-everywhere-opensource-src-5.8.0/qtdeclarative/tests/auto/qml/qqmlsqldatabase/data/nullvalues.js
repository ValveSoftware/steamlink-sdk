.import QtQuick.LocalStorage 2.0 as Sql

function test() {
    var db = Sql.LocalStorage.openDatabaseSync("QmlTestDB-nullvalues", "", "Test database from Qt autotests", 1000000);
    var r="transaction_not_finished";

    db.transaction(
        function(tx) {
            tx.executeSql('CREATE TABLE IF NOT EXISTS NullValues(salutation TEXT, salutee TEXT)');
            tx.executeSql('INSERT INTO NullValues VALUES(?, ?)', [ 'hello', null ]);
            var firstRow = tx.executeSql("SELECT * FROM NullValues").rows.item(0);
            if (firstRow.salutation !== "hello")
               return
            if (firstRow.salutee === "") {
                r = "wrong_data_type"
                return
            }
            if (firstRow.salutee === null)
                r = "passed";
        }
    );

    return r;
}
