import sys
import gdb

# Update module path.
dir = '/home/qwhuang/WORK/MRVL/MV88DE3100_SDK/Customization_Data/File_Systems/rootfs2012/usr/share/glib-2.0/gdb'
if not dir in sys.path:
    sys.path.insert(0, dir)

from glib import register
register (gdb.current_objfile ())
