.import QtQuick.LocalStorage 2.0 as Sql

function test() {
    var r="transaction_not_finished";

    var db = Sql.LocalStorage.openDatabaseSync("QmlTestDB-changeversion", "", "Test database from Qt autotests", 1000000,
            function(db) {
                db.changeVersion("","1.0")
                db.transaction(function(tx){
                    tx.executeSql('CREATE TABLE Greeting(salutation TEXT, salutee TEXT)');
                })
            });

    db.transaction(function(tx){
        tx.executeSql('INSERT INTO Greeting VALUES ("Hello", "world")');
        tx.executeSql('INSERT INTO Greeting VALUES ("Goodbye", "cruel world")');
    });


    db = Sql.LocalStorage.openDatabaseSync("QmlTestDB-changeversion", "", "Test database from Qt autotests", 1000000);

    if (db.version == "1.0")
        db.changeVersion("1.0","2.0",function(tx)
            {
                tx.executeSql('CREATE TABLE Utterance(type TEXT, phrase TEXT)')
                var rs = tx.executeSql('SELECT * FROM Greeting');
                for (var i=0; i<rs.rows.length; ++i) {
                    var type = "Greeting";
                    var phrase = rs.rows.item(i).salutation + ", " + rs.rows.item(i).salutee;
                    if (rs.rows.item(i).salutation == "Goodbye"
                     || rs.rows.item(i).salutation == "Farewell"
                     || rs.rows.item(i).salutation == "Good-bye") type = "Valediction";
                    var ins = tx.executeSql('INSERT INTO Utterance VALUES(?,?)',[type,phrase]);
                }
                tx.executeSql('DROP TABLE Greeting');
            });
    else
        return "db.version should be 1.0, but is " + db.version;

    var db = Sql.LocalStorage.openDatabaseSync("QmlTestDB-changeversion", "2.0", "Test database from Qt autotests", 1000000);

    db.transaction(function(tx){
        var rs = tx.executeSql('SELECT * FROM Utterance');
        r = ""
        for (var i=0; i<rs.rows.length; ++i) {
            r += "(" + rs.rows.item(i).type + ": " + rs.rows.item(i).phrase + ")";
        }
        if (r == "(Greeting: Hello, world)(Valediction: Goodbye, cruel world)")
            r = "passed"
        else
            r = "WRONG DATA: " + r;
    })

    return r;
}
