This test requires bttestui running on a remote machine, and for it to
be discoverable and connectable to the device under test. The bttestui
should run an rfcomm server (see UI option SListenUuid).

bttestui is available in tests/bttestui.

The unit test attempts to use service discovery to locate bttestui.
