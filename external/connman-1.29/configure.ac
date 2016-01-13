AC_PREREQ(2.60)
AC_INIT(connman, 1.29)

AM_INIT_AUTOMAKE([foreign subdir-objects color-tests])
AC_CONFIG_HEADERS([config.h])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AM_MAINTAINER_MODE

AC_PREFIX_DEFAULT(/usr/local)

PKG_PROG_PKG_CONFIG

COMPILER_FLAGS

AC_SUBST(abs_top_srcdir)
AC_SUBST(abs_top_builddir)

AC_LANG_C

AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CC_PIE
AC_PROG_INSTALL
AC_PROG_MKDIR_P

m4_define([_LT_AC_TAGCONFIG], [])
m4_ifdef([AC_LIBTOOL_TAGS], [AC_LIBTOOL_TAGS([])])

AC_DISABLE_STATIC
AC_PROG_LIBTOOL

AC_ARG_ENABLE(optimization, AC_HELP_STRING([--disable-optimization],
			[disable code optimization through compiler]), [
	if (test "${enableval}" = "no"); then
		CFLAGS="$CFLAGS -O0 -U_FORTIFY_SOURCE"
	fi
])

AC_ARG_ENABLE(debug, AC_HELP_STRING([--enable-debug],
			[enable compiling with debugging information]), [
	if (test "${enableval}" = "yes" &&
				test "${ac_cv_prog_cc_g}" = "yes"); then
		CFLAGS="$CFLAGS -g"
	fi
])

AC_ARG_ENABLE(pie, AC_HELP_STRING([--enable-pie],
			[enable position independent executables flag]), [
	if (test "${enableval}" = "yes" &&
				test "${ac_cv_prog_cc_pie}" = "yes"); then
		CFLAGS="$CFLAGS -fPIE"
		LDFLAGS="$LDFLAGS -pie"
	fi
])

AC_ARG_ENABLE(hh2serial-gps,
	AC_HELP_STRING([--enable-hh2serial-gps], [enable hh2serial GPS support]),
			[enable_hh2serial_gps=${enableval}], [enable_hh2serial_gps="no"])
AM_CONDITIONAL(HH2SERIAL_GPS, test "${enable_hh2serial_gps}" != "no")
AM_CONDITIONAL(HH2SERIAL_GPS_BUILTIN, test "${enable_hh2serial_gps}" = "builtin")

AC_ARG_WITH(openconnect, AC_HELP_STRING([--with-openconnect=PROGRAM],
        [specify location of openconnect binary]), [path_openconnect=${withval}])

AC_ARG_ENABLE(openconnect,
	AC_HELP_STRING([--enable-openconnect], [enable openconnect support]),
			[enable_openconnect=${enableval}], [enable_openconnect="no"])
if (test "${enable_openconnect}" != "no"); then
	if (test -z "${path_openconnect}"); then
		AC_PATH_PROG(OPENCONNECT, [openconnect], [], $PATH:/sbin:/usr/sbin)
		if (test -z "${OPENCONNECT}"); then
			AC_MSG_ERROR(openconnect binary not found)
		fi
	else
		OPENCONNECT="${path_openconnect}"
		AC_SUBST(OPENCONNECT)
	fi
fi
AM_CONDITIONAL(OPENCONNECT, test "${enable_openconnect}" != "no")
AM_CONDITIONAL(OPENCONNECT_BUILTIN, test "${enable_openconnect}" = "builtin")

AC_ARG_WITH(openvpn, AC_HELP_STRING([--with-openvpn=PROGRAM],
        [specify location of openvpn binary]), [path_openvpn=${withval}])

AC_ARG_ENABLE(openvpn,
	AC_HELP_STRING([--enable-openvpn], [enable openvpn support]),
			[enable_openvpn=${enableval}], [enable_openvpn="no"])
if (test "${enable_openvpn}" != "no"); then
	if (test -z "${path_openvpn}"); then
		AC_PATH_PROG(OPENVPN, [openvpn], [], $PATH:/sbin:/usr/sbin)
		if (test -z "${OPENVPN}"); then
			AC_MSG_ERROR(openvpn binary not found)
		fi
	else
		OPENVPN="${path_openvpn}"
		AC_SUBST(OPENVPN)
	fi
fi
AM_CONDITIONAL(OPENVPN, test "${enable_openvpn}" != "no")
AM_CONDITIONAL(OPENVPN_BUILTIN, test "${enable_openvpn}" = "builtin")

AC_ARG_WITH(vpnc, AC_HELP_STRING([--with-vpnc=PROGRAM],
	[specify location of vpnc binary]), [path_vpnc=${withval}])

AC_ARG_ENABLE(vpnc,
	AC_HELP_STRING([--enable-vpnc], [enable vpnc support]),
		[enable_vpnc=${enableval}], [enable_vpnc="no"])
if (test "${enable_vpnc}" != "no"); then
	if (test -z "${path_vpnc}"); then
		AC_PATH_PROG(VPNC, [vpnc], [], $PATH:/sbin:/usr/sbin)
		if (test -z "${VPNC}"); then
			AC_MSG_ERROR(vpnc binary not found)
		fi
	else
		VPNC="${path_vpnc}"
		AC_SUBST(VPNC)
	fi
fi
AM_CONDITIONAL(VPNC, test "${enable_vpnc}" != "no")
AM_CONDITIONAL(VPNC_BUILTIN, test "${enable_vpnc}" = "builtin")

AC_ARG_WITH(l2tp, AC_HELP_STRING([--with-l2tp=PROGRAM],
        [specify location of l2tp binary]), [path_l2tp=${withval}])

AC_ARG_ENABLE(l2tp,
	AC_HELP_STRING([--enable-l2tp], [enable l2tp support]),
			[enable_l2tp=${enableval}], [enable_l2tp="no"])
if (test "${enable_l2tp}" != "no"); then
	if (test -z "${path_pppd}"); then
		AC_PATH_PROG(PPPD, [pppd], [/usr/sbin/pppd], $PATH:/sbin:/usr/sbin)
	else
		PPPD="${path_pppd}"
		AC_SUBST(PPPD)
	fi
	AC_CHECK_HEADERS(pppd/pppd.h, dummy=yes,
			AC_MSG_ERROR(ppp header files are required))
	if (test -z "${path_l2tp}"); then
		AC_PATH_PROG(L2TP, [xl2tpd], [/usr/sbin/xl2tpd], $PATH:/sbin:/usr/sbin)
	else
		L2TP="${path_l2tp}"
		AC_SUBST(L2TP)
	fi
fi
AM_CONDITIONAL(L2TP, test "${enable_l2tp}" != "no")
AM_CONDITIONAL(L2TP_BUILTIN, test "${enable_l2tp}" = "builtin")

AC_ARG_WITH(pptp, AC_HELP_STRING([--with-pptp=PROGRAM],
        [specify location of pptp binary]), [path_pptp=${withval}])

AC_ARG_ENABLE(pptp,
	AC_HELP_STRING([--enable-pptp], [enable pptp support]),
			[enable_pptp=${enableval}], [enable_pptp="no"])
if (test "${enable_pptp}" != "no"); then
	if (test -z "${path_pppd}"); then
		AC_PATH_PROG(PPPD, [pppd], [/usr/sbin/pppd], $PATH:/sbin:/usr/sbin)
	else
		PPPD="${path_pppd}"
		AC_SUBST(PPPD)
	fi
	AC_CHECK_HEADERS(pppd/pppd.h, dummy=yes,
			AC_MSG_ERROR(ppp header files are required))
	if (test -z "${path_pptp}"); then
		AC_PATH_PROG(PPTP, [pptp], [/usr/sbin/pptp], $PATH:/sbin:/usr/sbin)
	else
		PPTP="${path_pptp}"
		AC_SUBST(PPTP)
	fi
fi
AM_CONDITIONAL(PPTP, test "${enable_pptp}" != "no")
AM_CONDITIONAL(PPTP_BUILTIN, test "${enable_pptp}" = "builtin")

AC_CHECK_HEADERS(resolv.h, dummy=yes,
	AC_MSG_ERROR(resolver header files are required))
AC_CHECK_LIB(resolv, ns_initparse, dummy=yes, [
	AC_CHECK_LIB(resolv, __ns_initparse, dummy=yes,
		AC_MSG_ERROR(resolver library support is required))
])

AC_CHECK_FUNC(signalfd, dummy=yes,
			AC_MSG_ERROR(signalfd support is required))

AC_CHECK_LIB(dl, dlopen, dummy=yes,
			AC_MSG_ERROR(dynamic linking loader is required))

AC_ARG_ENABLE(iospm, AC_HELP_STRING([--enable-iospm],
		[enable Intel OSPM support]), [enable_iospm=${enableval}])
AM_CONDITIONAL(IOSPM, test "${enable_iospm}" = "yes")

AC_ARG_ENABLE(tist,
	AC_HELP_STRING([--enable-tist], [enable TI Shared Transport support]),
			[enable_tist=${enableval}], [enable_tist="no"])
AM_CONDITIONAL(TIST, test "${enable_tist}" != "no")
AM_CONDITIONAL(TIST_BUILTIN, test "${enable_tist}" = "builtin")

AC_ARG_ENABLE(session-policy-local,
	AC_HELP_STRING([--enable-session-policy-local], [enable local file Session policy configuration support]),
			[enable_session_policy_local=${enableval}], [enable_session_policy_local="no"])
AM_CONDITIONAL(SESSION_POLICY_LOCAL, test "${enable_session_policy_local}" != "no")
AM_CONDITIONAL(SESSION_POLICY_LOCAL_BUILTIN, test "${enable_session_policy_local}" = "builtin")

AC_ARG_WITH(stats-max-file-size, AC_HELP_STRING([--with-stats-max-file-size=SIZE],
			[Maximal size of a statistics round robin file]),
			[stats_max_file_size=${withval}])

if (test -z "${stats_max_file_size}"); then
   # default size is 16 kByte
   stats_max_file_size="16 * 8 * 128"
fi

AC_DEFINE_UNQUOTED([STATS_MAX_FILE_SIZE], (${stats_max_file_size}), [Maximal size of a statistics round robin file])

PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.28, dummy=yes,
				AC_MSG_ERROR(GLib >= 2.28 is required))
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

PKG_CHECK_MODULES(DBUS, dbus-1 >= 1.4, dummy=yes,
				AC_MSG_ERROR(D-Bus >= 1.4 is required))
AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)

AC_ARG_WITH(dbusconfdir, AC_HELP_STRING([--with-dbusconfdir=PATH],
	[path to D-Bus config directory]), [path_dbusconf=${withval}],
		[path_dbusconf="`$PKG_CONFIG --variable=sysconfdir dbus-1`"])
if (test -z "${path_dbusconf}"); then
	DBUS_CONFDIR="${sysconfdir}/dbus-1/system.d"
else
	DBUS_CONFDIR="${path_dbusconf}/dbus-1/system.d"
fi
AC_SUBST(DBUS_CONFDIR)

AC_ARG_WITH(dbusdatadir, AC_HELP_STRING([--with-dbusdatadir=PATH],
	[path to D-Bus data directory]), [path_dbusdata=${withval}],
		[path_dbusdata="`$PKG_CONFIG --variable=datadir dbus-1`"])
if (test -z "${path_dbusdata}"); then
	DBUS_DATADIR="${datadir}/dbus-1/system-services"
else
	DBUS_DATADIR="${path_dbusdata}/dbus-1/system-services"
fi
AC_SUBST(DBUS_DATADIR)

AC_ARG_WITH([systemdunitdir], AC_HELP_STRING([--with-systemdunitdir=DIR],
	[path to systemd service directory]), [path_systemdunit=${withval}],
		[path_systemdunit="`$PKG_CONFIG --variable=systemdsystemunitdir systemd`"])
if (test -n "${path_systemdunit}"); then
	SYSTEMD_UNITDIR="${path_systemdunit}"
	AC_SUBST(SYSTEMD_UNITDIR)
fi
AM_CONDITIONAL(SYSTEMD, test -n "${path_systemdunit}")

PKG_CHECK_MODULES(XTABLES, xtables >= 1.4.11, dummy=yes,
				AC_MSG_ERROR(Xtables library is required))
AC_SUBST(XTABLES_CFLAGS)
AC_SUBST(XTABLES_LIBS)

AC_ARG_ENABLE(test, AC_HELP_STRING([--enable-test],
		[enable test/example scripts]), [enable_test=${enableval}])
AM_CONDITIONAL(TEST, test "${enable_test}" = "yes")

AC_ARG_ENABLE(nmcompat, AC_HELP_STRING([--enable-nmcompat],
				[enable Network Manager support]),
			[enable_nmcompat=${enableval}], [enable_nmcompat="no"])
AM_CONDITIONAL(NMCOMPAT, test "${enable_nmcompat}" != "no")

AC_ARG_ENABLE(polkit, AC_HELP_STRING([--enable-polkit],
				[enable PolicyKit support]),
			[enable_polkit=${enableval}], [enable_polkit="no"])
if (test "${enable_polkit}" != "no"); then
	POLKIT_DATADIR="`$PKG_CONFIG --variable=actiondir polkit`"
	POLKIT_DATADIR=""
	if (test -z "${POLKIT_DATADIR}"); then
		POLKIT_DATADIR="${datadir}/polkit-1/actions"
	fi
	AC_SUBST(POLKIT_DATADIR)
fi
AM_CONDITIONAL(POLKIT, test "${enable_polkit}" != "no")

AC_ARG_ENABLE(selinux, AC_HELP_STRING([--enable-selinux],
				[enable selinux support]),
			[enable_selinux=${enableval}], [enable_selinux="no"])
AM_CONDITIONAL(SELINUX, test "${enable_selinux}" != "no")

AC_ARG_ENABLE(loopback, AC_HELP_STRING([--disable-loopback],
				[disable loopback support]),
					[enable_loopback=${enableval}])
AM_CONDITIONAL(LOOPBACK, test "${enable_loopback}" != "no")

AC_ARG_ENABLE(ethernet, AC_HELP_STRING([--disable-ethernet],
				[disable Ethernet support]),
					[enable_ethernet=${enableval}])
AM_CONDITIONAL(ETHERNET, test "${enable_ethernet}" != "no")

AC_ARG_ENABLE(gadget, AC_HELP_STRING([--disable-gadget],
				[disable USB Gadget support]),
					[enable_gadget=${enableval}])
AM_CONDITIONAL(GADGET, test "${enable_gadget}" != "no")

AC_ARG_ENABLE(wifi, AC_HELP_STRING([--disable-wifi],
				[disable WiFi support]),
					[enable_wifi=${enableval}])
AM_CONDITIONAL(WIFI, test "${enable_wifi}" != "no")

AC_ARG_ENABLE(bluetooth, AC_HELP_STRING([--disable-bluetooth],
				[disable Bluetooth support]),
					[enable_bluetooth=${enableval}])
AM_CONDITIONAL(BLUETOOTH, test "${enable_bluetooth}" != "no")

AC_ARG_ENABLE(ofono, AC_HELP_STRING([--disable-ofono],
				[disable oFono support]),
					[enable_ofono=${enableval}])
AM_CONDITIONAL(OFONO, test "${enable_ofono}" != "no")

AC_ARG_ENABLE(dundee, AC_HELP_STRING([--disable-dundee],
				[disable dundee support (Bluetooth DUN)]),
					[enable_dundee=${enableval}])
AM_CONDITIONAL(DUNDEE, test "${enable_dundee}" != "no")

AC_ARG_ENABLE(pacrunner, AC_HELP_STRING([--disable-pacrunner],
				[disable PACrunner support]),
					[enable_pacrunner=${enableval}])
AM_CONDITIONAL(PACRUNNER, test "${enable_pacrunner}" != "no")

AC_ARG_ENABLE(neard, AC_HELP_STRING([--disable-neard],
				[disable Neard support]),
					[enable_neard=${enableval}])
AM_CONDITIONAL(NEARD, test "${enable_neard}" != "no")

AC_ARG_ENABLE(wispr, AC_HELP_STRING([--disable-wispr],
				[disable WISPr support]),
					[enable_wispr=${enableval}])
AM_CONDITIONAL(WISPR, test "${enable_wispr}" != "no")

AC_ARG_ENABLE(tools, AC_HELP_STRING([--disable-tools],
				[disable testing tools]),
					[enable_tools=${enableval}])
AM_CONDITIONAL(TOOLS, test "${enable_tools}" != "no")

if (test "${enable_tools}" != "no"); then
	AC_PATH_PROGS(IPTABLES_SAVE, [iptables-save], [],
						$PATH:/sbin:/usr/sbin)
	IPTABLES_SAVE=$ac_cv_path_IPTABLES_SAVE
else
	IPTABLES_SAVE=""
fi
AC_SUBST(IPTABLES_SAVE)

AC_ARG_ENABLE(client, AC_HELP_STRING([--disable-client],
				[disable command line client]),
					[enable_client=${enableval}])
AM_CONDITIONAL(CLIENT, test "${enable_client}" != "no")

if (test "${enable_wispr}" != "no"); then
	PKG_CHECK_MODULES(GNUTLS, gnutls, dummy=yes,
				AC_MSG_ERROR(GnuTLS library is required))
else
	GNUTLS_CFLAGS=""
	GNUTLS_LIBS=""
fi
AC_SUBST(GNUTLS_CFLAGS)
AC_SUBST(GNUTLS_LIBS)

if (test "${enable_loopback}" != "no"); then
	AC_CHECK_HEADERS(sys/inotify.h, dummy=yes,
			AC_MSG_ERROR(inotify header files are required))

	AC_CHECK_LIB(c, inotify_init, dummy=yes,
			AC_MSG_ERROR(inotify library support is required))
fi

if (test "${enable_wifi}" != "no"); then
	AC_PATH_PROG(WPASUPPLICANT, [wpa_supplicant], [],
						$PATH:/sbin:/usr/sbin)
fi

AC_ARG_ENABLE(datafiles, AC_HELP_STRING([--disable-datafiles],
			[don't install configuration and data files]),
					[enable_datafiles=${enableval}])
AM_CONDITIONAL(DATAFILES, test "${enable_datafiles}" != "no")

if (test "${enable_client}" != "no"); then
	AC_CHECK_HEADERS(readline/readline.h, dummy=yes,
		AC_MSG_ERROR(readline header files are required))
fi

AM_CONDITIONAL(VPN, test "${enable_openconnect}" != "no" -o \
			"${enable_openvpn}" != "no" -o \
			"${enable_vpnc}" != "no" -o \
			"${enable_l2tp}" != "no" -o \
			"${enable_pptp}" != "no")

AC_OUTPUT(Makefile include/version.h connman.pc)
