.import QtQuick.LocalStorage 2.0 as Sql

function test() {
    var db = Sql.LocalStorage.openDatabaseSync("QmlTestDB-data/error-notransaction", "1.0", "Test database from Qt autotests", 1000000);
    var r="transaction_not_finished";
    var v;

    try {
        db.transaction(function(tx) { v = tx });
        v.executeSql("SELECT 'bad'")
    } catch (err) {
        if (err.message == "executeSql called outside transaction()")
            r = "passed";
        else
            r = "WRONG ERROR="+err.message;
    }

    return r;
}
