/*
 * Copyright © 2010, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#include "commondbustypes.h"

// Marshall the ConnmanObject data into a D-Bus argument
QDBusArgument &operator<<(QDBusArgument &argument, const ConnmanObject &obj)
{
    argument.beginStructure();
    argument << obj.objpath << obj.properties;
    argument.endStructure();
    return argument;
}

// Retrieve the ConnmanObject data from the D-Bus argument
const QDBusArgument &operator>>(const QDBusArgument &argument, ConnmanObject &obj)
{
    argument.beginStructure();
    argument >> obj.objpath >> obj.properties;
    argument.endStructure();
    return argument;
}
