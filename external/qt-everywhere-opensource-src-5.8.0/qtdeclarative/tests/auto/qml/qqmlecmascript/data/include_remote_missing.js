function go(serverBaseUrl)
{
    var a = Qt.include(serverBaseUrl + "/missing.js",
                       function(o) {
                            test2 = o.status == o.NETWORK_ERROR
                            test3 = a.status == a.NETWORK_ERROR

                            done = true;
                       });

    test1 = a.status == a.LOADING
}

