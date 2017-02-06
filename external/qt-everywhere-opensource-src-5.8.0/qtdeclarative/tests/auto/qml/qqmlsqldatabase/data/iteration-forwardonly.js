.import QtQuick.LocalStorage 2.0 as Sql

function test() {
    var db = Sql.LocalStorage.openDatabaseSync("QmlTestDB-iteration-forwardonly", "", "Test database from Qt autotests", 1000000);
    var r="transaction_not_finished";

    db.transaction(
        function(tx) {
            tx.executeSql('CREATE TABLE Greeting(salutation TEXT, salutee TEXT)');
            tx.executeSql('INSERT INTO Greeting VALUES ("Hello", "world")');
            tx.executeSql('INSERT INTO Greeting VALUES ("Goodbye", "cruel world")');
        }
    )

    db.transaction(
        function(tx) {
            var rs = tx.executeSql('SELECT * FROM Greeting');
            rs.forwardOnly = !rs.forwardOnly
            var r1=""
            for(var i = 0; i < rs.rows.length; i++)
                r1 += rs.rows.item(i).salutation + ", " + rs.rows.item(i).salutee + ";"
            if (r1 != "hello, world;hello, world;hello, world;hello, world;")
            if (r1 != "Hello, world;Goodbye, cruel world;")
                r = "SELECTED DATA WRONG: "+r1;
            else
                r = "passed";
        }
    );

    return r;
}
