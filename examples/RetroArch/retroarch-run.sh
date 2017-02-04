#!/bin/sh
if [ -f "config.copied" ]; then
	./retroarch
else
	mkdir -p ~/.config/retroarch
	cp retroarch_steamlink.cfg ~/.config/retroarch/retroarch.cfg
	touch config.copied
	./retroarch
fi
