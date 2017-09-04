.import QtQuick.LocalStorage 2.0 as Sql

function test() {
    var r="transaction_not_finished";
    try {
        var db = Sql.LocalStorage.openDatabaseSync("QmlTestDB-creation", "2.0", "Test database from Qt autotests", 1000000);
    } catch (err) {
        if (err.code != SQLException.VERSION_ERR)
            r = "WRONG ERROR CODE="+err.code;
        else if (err.message != "SQL: database version mismatch")
            r = "WRONG ERROR="+err.message;
        else
            r = "passed";
    }
    return r;
}
