GYP_ARGS += "-D qt_os=\"desktop_linux\""

GYP_CONFIG += \
    desktop_linux=1 \
    toolkit_uses_gtk=0 \
    use_aura=1 \
    use_ash=0 \
    use_cairo=0 \
    use_clipboard_aurax11=0 \
    use_cups=0 \
    use_gconf=0 \
    use_gio=0 \
    use_gnome_keyring=0 \
    use_kerberos=0 \
    use_pango=0 \

!contains(QT_CONFIG, pulseaudio): GYP_CONFIG += use_pulseaudio=0
