.import QtQuick.LocalStorage 2.0 as Sql

function test() {
    var r="transaction_not_finished";

    var db = Sql.LocalStorage.openDatabaseSync("QmlTestDB-creation-a", "1.0", "Test database from Qt autotests", 1000000,
            function(db) {
                db.transaction(function(tx){
                    tx.executeSql('CREATE TABLE Greeting(salutation TEXT, salutee TEXT)');
                    r = "passed";
                })
            });

    var db = Sql.LocalStorage.openDatabaseSync("QmlTestDB-creation-a", "1.0", "Test database from Qt autotests", 1000000,
            function(db) {
                r = "FAILED: should have already been created";
            });

    return r;
}
