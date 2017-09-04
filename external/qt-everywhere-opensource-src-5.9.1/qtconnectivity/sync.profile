%modules = ( # path to module name map
    "QtBluetooth" => "$basedir/src/bluetooth",
    "QtNfc" => "$basedir/src/nfc",
);
%moduleheaders = ( # restrict the module headers to those found in relative path
);

@ignore_for_include_check = (

    # OBEX auto-generated headers
    "adapter1_bluez5_p.h", "adapter_p.h", "agent_p.h", "device1_bluez5_p.h",
    "device_p.h", "manager_p.h", "obex_agent_p.h", "obex_client1_bluez5_p.h",
    "obex_client_p.h", "obex_manager_p.h", "obex_objectpush1_bluez5_p.h",
    "obex_transfer1_bluez5_p.h", "obex_transfer_p.h", "objectmanager_p.h",
    "profile1_p.h", "properties_p.h", "service_p.h",
    # NFC auto-generated headers
    # Note: "adapter_p.h", "agent_p.h" and "manager_p.h" are duplicated here
    "dbusobjectmanager_p.h", "dbusproperties_p.h", "tag_p.h");
