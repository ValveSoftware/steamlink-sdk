import sys

if hasattr(sys, 'gettotalrefcount'):
    from _sysconfigdata_d import *
else:
    from _sysconfigdata_nd import *
