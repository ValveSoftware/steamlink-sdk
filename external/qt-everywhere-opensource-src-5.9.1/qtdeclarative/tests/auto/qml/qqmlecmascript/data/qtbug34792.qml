import QtQuick 2.1
Rectangle {
    function foo()
    {
        for (var i = 0; i < 1; i++)
        {
            if (i >= 0)
                break

            return
        }

    }
}
