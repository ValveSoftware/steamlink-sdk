__ZRELADDR	:= $(shell /bin/bash -c 'printf "0x%08x" \
		     $$[$(CONFIG_PHYS_OFFSET) + 0x8000]')
zreladdr-y	:= $(__ZRELADDR)
