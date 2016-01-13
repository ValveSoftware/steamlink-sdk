function go()
{
    var a = Qt.include("http://127.0.0.1:8111/missing.js",
                       function(o) {
                            test2 = o.status == o.NETWORK_ERROR
                            test3 = a.status == a.NETWORK_ERROR

                            done = true;
                       });

    test1 = a.status == a.LOADING
}

