
When you add new fonts here, you should copy them to a running system and run this command:
	fc-cache -r -v

You should then copy the contents of /var/cache/fontconfig into rootfs_valve/var/cache/fontconfig

You should also add the fonts and cached fontconfig to the recovery image:
	MRVL/MV88DE3100_SDK/MV88DE3100_Tools/recovery-initramfs
