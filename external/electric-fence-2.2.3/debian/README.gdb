This is a suggested ~/.gdbinit from Ray Dassen - it enables you to use 
Electric Fence from within gdb without having to relink the binary. See
the gdb docs for more information.

###############################################################################
# Electric Fence
#
# Debian's Electric Fence package provides efence as a shared library, which is
# very useful.
###############################################################################

define efence
        set environment EF_PROTECT_BELOW 0
        set environment LD_PRELOAD /usr/lib/libefence.so.0.0
        echo Enabled Electric Fence\n
end
document efence
Enable memory allocation debugging through Electric Fence (efence(3)).
        See also nofence and underfence.
end


define underfence
        set environment EF_PROTECT_BELOW 1
        set environment LD_PRELOAD /usr/lib/libefence.so.0.0
        echo Enabled Electric Fence for undeflow detection\n
end
document underfence
Enable memory allocation debugging for underflows through Electric Fence 
(efence(3)).
        See also nofence and efence.
end


define nofence
        unset environment LD_PRELOAD
        echo Disabled Electric Fence\n
end
document nofence
Disable memory allocation debugging through Electric Fence (efence(3)).
end
