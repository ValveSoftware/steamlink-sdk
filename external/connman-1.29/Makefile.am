
AM_MAKEFLAGS = --no-print-directory

noinst_LTLIBRARIES =

includedir = @includedir@/connman

include_HEADERS = include/log.h include/plugin.h \
			include/notifier.h include/service.h \
			include/resolver.h include/ipconfig.h \
			include/device.h include/network.h include/inet.h \
			include/storage.h include/provision.h \
			include/session.h include/ipaddress.h include/agent.h \
			include/inotify.h include/peer.h include/machine.h

nodist_include_HEADERS = include/version.h

noinst_HEADERS = include/rtnl.h include/task.h \
			include/dbus.h include/option.h \
			include/provider.h include/vpn-dbus.h \
			include/utsname.h include/timeserver.h include/proxy.h \
			include/technology.h include/setting.h

local_headers = $(foreach file,$(include_HEADERS) $(nodist_include_HEADERS) \
			$(noinst_HEADERS), include/connman/$(notdir $(file)))


noinst_LTLIBRARIES += gdbus/libgdbus-internal.la

gdbus_libgdbus_internal_la_SOURCES = gdbus/gdbus.h \
				gdbus/mainloop.c gdbus/watch.c \
				gdbus/object.c gdbus/client.c gdbus/polkit.c

gdhcp_sources = gdhcp/gdhcp.h gdhcp/common.h gdhcp/common.c gdhcp/client.c \
		gdhcp/server.c gdhcp/ipv4ll.h gdhcp/ipv4ll.c gdhcp/unaligned.h

gweb_sources = gweb/gweb.h gweb/gweb.c gweb/gresolv.h gweb/gresolv.c

if WISPR
gweb_sources += gweb/giognutls.h gweb/giognutls.c
else
gweb_sources += gweb/giognutls.h gweb/gionotls.c
endif

shared_sources = src/shared/util.h src/shared/util.c \
		src/shared/netlink.h src/shared/netlink.c

if DATAFILES

if NMCOMPAT
nmcompat_conf = plugins/connman-nmcompat.conf
endif

dbusconfdir = @DBUS_CONFDIR@

dbusconf_DATA = src/connman.conf $(nmcompat_conf)

if VPN
dbusconf_DATA += vpn/connman-vpn-dbus.conf
dbusservicedir = @DBUS_DATADIR@
dbusservice_DATA = vpn/net.connman.vpn.service
endif

if SYSTEMD
systemdunitdir = @SYSTEMD_UNITDIR@

systemdunit_DATA = src/connman.service

if VPN
systemdunit_DATA += vpn/connman-vpn.service
endif
endif
endif

service_files_sources = src/connman.service.in src/net.connman.service.in \
				vpn/connman-vpn.service.in \
				vpn/net.connman.vpn.service.in
service_files = src/connman.service src/net.connman.service \
				vpn/connman-vpn.service \
				vpn/net.connman.vpn.service

plugin_LTLIBRARIES =

plugin_objects =

builtin_modules =
builtin_sources =
builtin_libadd =
builtin_cflags =

noinst_PROGRAMS =
bin_PROGRAMS =

unit_objects =

MANUAL_PAGES =

sbin_PROGRAMS = src/connmand

src_connmand_SOURCES = $(gdhcp_sources) $(gweb_sources) \
			$(builtin_sources) $(shared_sources) src/connman.ver \
			src/main.c src/connman.h src/log.c \
			src/error.c src/plugin.c src/task.c \
			src/device.c src/network.c src/connection.c \
			src/manager.c src/service.c \
			src/clock.c src/timezone.c src/agent-connman.c \
			src/agent.c src/notifier.c src/provider.c \
			src/resolver.c src/ipconfig.c src/detect.c src/inet.c \
			src/dhcp.c src/dhcpv6.c src/rtnl.c src/proxy.c \
			src/utsname.c src/timeserver.c src/rfkill.c \
			src/storage.c src/dbus.c src/config.c \
			src/technology.c src/counter.c src/ntp.c \
			src/session.c src/tethering.c src/wpad.c src/wispr.c \
			src/stats.c src/iptables.c src/dnsproxy.c src/6to4.c \
			src/ippool.c src/bridge.c src/nat.c src/ipaddress.c \
			src/inotify.c src/firewall.c src/ipv6pd.c src/peer.c \
			src/peer_service.c src/machine.c src/util.c

src_connmand_LDADD = gdbus/libgdbus-internal.la $(builtin_libadd) \
			@GLIB_LIBS@ @DBUS_LIBS@ @XTABLES_LIBS@ @GNUTLS_LIBS@ \
			-lresolv -ldl -lrt

src_connmand_LDFLAGS = -Wl,--export-dynamic \
				-Wl,--version-script=$(srcdir)/src/connman.ver

if VPN
vpn_plugin_LTLIBRARIES =

vpn_plugin_objects =

builtin_vpn_modules =
builtin_vpn_sources =
builtin_vpn_libadd =
builtin_vpn_cflags =

sbin_PROGRAMS += vpn/connman-vpnd

vpn_connman_vpnd_SOURCES = $(builtin_vpn_sources) \
			$(gweb_sources) vpn/vpn.ver vpn/main.c vpn/vpn.h \
			src/log.c src/error.c src/plugin.c src/task.c \
			vpn/vpn-manager.c vpn/vpn-provider.c \
			vpn/vpn-provider.h vpn/vpn-rtnl.h \
			vpn/vpn-ipconfig.c src/inet.c vpn/vpn-rtnl.c \
			src/dbus.c src/storage.c src/ipaddress.c src/agent.c \
			vpn/vpn-agent.c vpn/vpn-agent.h src/inotify.c \
			vpn/vpn-config.c

vpn_connman_vpnd_LDADD = gdbus/libgdbus-internal.la $(builtin_vpn_libadd) \
				@GLIB_LIBS@ @DBUS_LIBS@ @GNUTLS_LIBS@ \
				-lresolv -ldl

vpn_connman_vpnd_LDFLAGS = -Wl,--export-dynamic \
				-Wl,--version-script=$(srcdir)/vpn/vpn.ver
endif

BUILT_SOURCES = $(local_headers) src/builtin.h $(service_files) scripts/connman

if VPN
BUILT_SOURCES += vpn/builtin.h
endif

CLEANFILES = src/connman.conf $(BUILT_SOURCES) $(service_files)

statedir = $(localstatedir)/run/connman
vpn_statedir = $(localstatedir)/run/connman-vpn

if VPN
vpn_plugindir = $(libdir)/connman/plugins-vpn
endif

plugindir = $(libdir)/connman/plugins

scriptdir = $(libdir)/connman/scripts

storagedir = $(localstatedir)/lib/connman
vpn_storagedir = $(localstatedir)/lib/connman-vpn

configdir = ${sysconfdir}/connman

if MAINTAINER_MODE
if VPN
build_vpn_plugindir = $(abs_top_srcdir)/vpn/plugins/.libs
endif
build_plugindir = $(abs_top_srcdir)/plugins/.libs
build_scriptdir = $(abs_top_srcdir)/scripts
else
build_plugindir = $(plugindir)
build_scriptdir = $(scriptdir)
if VPN
build_vpn_plugindir = $(vpn_plugindir)
endif
endif

AM_CFLAGS = @DBUS_CFLAGS@ @GLIB_CFLAGS@ @XTABLES_CFLAGS@ \
				@GNUTLS_CFLAGS@ $(builtin_cflags) \
				-DCONNMAN_PLUGIN_BUILTIN \
				-DSTATEDIR=\""$(statedir)"\" \
				-DVPN_STATEDIR=\""$(vpn_statedir)"\" \
				-DPLUGINDIR=\""$(build_plugindir)"\" \
				-DSCRIPTDIR=\""$(build_scriptdir)"\" \
				-DSTORAGEDIR=\""$(storagedir)\"" \
				-DVPN_STORAGEDIR=\""$(vpn_storagedir)\"" \
				-DCONFIGDIR=\""$(configdir)\""

if VPN
AM_CPPFLAGS = -I$(builddir)/include -I$(srcdir)/gdbus
else
AM_CPPFLAGS = -I$(builddir)/include -I$(builddir)/src -I$(srcdir)/gdbus
endif

src_connmand_CFLAGS = @DBUS_CFLAGS@ @GLIB_CFLAGS@ @XTABLES_CFLAGS@ \
				@GNUTLS_CFLAGS@ $(builtin_cflags) \
				-DCONNMAN_PLUGIN_BUILTIN \
				-DSTATEDIR=\""$(statedir)"\" \
				-DPLUGINDIR=\""$(build_plugindir)"\" \
				-DSCRIPTDIR=\""$(build_scriptdir)"\" \
				-DSTORAGEDIR=\""$(storagedir)\"" \
				-DVPN_STORAGEDIR=\""$(vpn_storagedir)\"" \
				-DCONFIGDIR=\""$(configdir)\"" \
				-I$(builddir)/src

EXTRA_DIST = src/genbuiltin src/connman-dbus.conf src/connman-polkit.conf \
				plugins/connman-nmcompat.conf \
				$(service_files_sources) scripts/connman.in

if VPN
vpn_connman_vpnd_CFLAGS = @DBUS_CFLAGS@ @GLIB_CFLAGS@ \
				$(builtin_vpn_cflags) \
				-DCONNMAN_PLUGIN_BUILTIN \
				-DVPN_STATEDIR=\""$(vpn_statedir)"\" \
				-DPLUGINDIR=\""$(build_vpn_plugindir)"\" \
				-DSCRIPTDIR=\""$(build_scriptdir)"\" \
				-DSTORAGEDIR=\""$(storagedir)\"" \
				-DVPN_STORAGEDIR=\""$(vpn_storagedir)\"" \
				-DCONFIGDIR=\""$(configdir)\"" \
				-I$(builddir)/vpn

endif

EXTRA_DIST += vpn/vpn-dbus.conf vpn/vpn-polkit.conf

script_DATA =
script_PROGRAMS =
script_LTLIBRARIES =

include Makefile.plugins

if CLIENT
bin_PROGRAMS += client/connmanctl

MANUAL_PAGES += doc/connmanctl.1

client_connmanctl_SOURCES = client/dbus_helpers.h client/dbus_helpers.c \
			client/services.h client/services.c \
			client/commands.h client/commands.c \
			client/input.h client/input.c \
			client/agent.h client/agent.c \
			client/peers.h client/peers.c \
			client/vpnconnections.h client/vpnconnections.c \
			client/main.c

client_connmanctl_LDADD = gdbus/libgdbus-internal.la @DBUS_LIBS@ @GLIB_LIBS@ \
				-lreadline -ldl
endif

noinst_PROGRAMS += unit/test-ippool

unit_test_ippool_SOURCES = src/log.c src/dbus.c src/error.c \
					src/ippool.c unit/test-ippool.c
unit_test_ippool_LDADD = gdbus/libgdbus-internal.la \
				@GLIB_LIBS@ @DBUS_LIBS@ -ldl

TESTS = unit/test-ippool

if WISPR
noinst_PROGRAMS += tools/wispr

tools_wispr_SOURCES = $(gweb_sources) tools/wispr.c
tools_wispr_LDADD = @GLIB_LIBS@ @GNUTLS_LIBS@ -lresolv
endif

if TOOLS
noinst_PROGRAMS += tools/supplicant-test \
			tools/dhcp-test tools/dhcp-server-test \
			tools/addr-test tools/web-test tools/resolv-test \
			tools/dbus-test tools/polkit-test \
			tools/iptables-test tools/tap-test tools/wpad-test \
			tools/stats-tool tools/private-network-test \
			tools/session-test tools/iptables-unit \
			tools/dnsproxy-test tools/netlink-test

tools_supplicant_test_SOURCES = tools/supplicant-test.c \
			tools/supplicant-dbus.h tools/supplicant-dbus.c \
			tools/supplicant.h tools/supplicant.c
tools_supplicant_test_LDADD = gdbus/libgdbus-internal.la \
				@GLIB_LIBS@ @DBUS_LIBS@

tools_web_test_SOURCES = $(gweb_sources) tools/web-test.c
tools_web_test_LDADD = @GLIB_LIBS@ @GNUTLS_LIBS@ -lresolv

tools_resolv_test_SOURCES = gweb/gresolv.h gweb/gresolv.c tools/resolv-test.c
tools_resolv_test_LDADD = @GLIB_LIBS@ -lresolv

tools_wpad_test_SOURCES = gweb/gresolv.h gweb/gresolv.c tools/wpad-test.c
tools_wpad_test_LDADD = @GLIB_LIBS@ -lresolv

tools_stats_tool_LDADD = @GLIB_LIBS@

tools_dhcp_test_SOURCES = $(gdhcp_sources) tools/dhcp-test.c
tools_dhcp_test_LDADD = @GLIB_LIBS@

tools_dhcp_server_test_SOURCES = $(gdhcp_sources) tools/dhcp-server-test.c
tools_dhcp_server_test_LDADD = @GLIB_LIBS@

tools_dbus_test_SOURCES = tools/dbus-test.c
tools_dbus_test_LDADD = gdbus/libgdbus-internal.la @GLIB_LIBS@ @DBUS_LIBS@

tools_polkit_test_LDADD = @DBUS_LIBS@

tools_iptables_test_SOURCES = src/log.c src/iptables.c tools/iptables-test.c
tools_iptables_test_LDADD = @GLIB_LIBS@ @XTABLES_LIBS@ -ldl

tools_private_network_test_LDADD = @GLIB_LIBS@ @DBUS_LIBS@

tools_session_test_SOURCES = src/log.c src/dbus.c src/error.c \
		tools/session-test.c tools/session-utils.c tools/manager-api.c \
		tools/session-api.c tools/session-test.h
tools_session_test_LDADD = gdbus/libgdbus-internal.la \
				@GLIB_LIBS@ @DBUS_LIBS@ -ldl

tools_iptables_unit_CFLAGS = @DBUS_CFLAGS@ @GLIB_CFLAGS@ @XTABLES_CFLAGS@ \
		-DIPTABLES_SAVE=\""${IPTABLES_SAVE}"\"
tools_iptables_unit_SOURCES = src/log.c \
		 src/iptables.c src/firewall.c src/nat.c tools/iptables-unit.c
tools_iptables_unit_LDADD = gdbus/libgdbus-internal.la \
				@GLIB_LIBS@ @DBUS_LIBS@ @XTABLES_LIBS@ -ldl

tools_dnsproxy_test_SOURCES = tools/dnsproxy-test.c
tools_dnsproxy_test_LDADD = @GLIB_LIBS@

tools_netlink_test_SOURCES =$(shared_sources) tools/netlink-test.c
tools_netlink_test_LDADD = @GLIB_LIBS@

endif

test_scripts = test/get-state test/list-services \
		test/monitor-services test/test-clock \
		test/simple-agent test/show-introspection test/test-compat \
		test/test-manager test/test-connman test/monitor-connman \
		test/connect-provider test/remove-provider \
		test/test-counter test/set-ipv4-method test/set-ipv6-method \
		test/get-services test/get-proxy-autoconfig test/set-proxy \
		test/enable-tethering test/disable-tethering test/backtrace \
		test/test-session test/p2p-on-supplicant \
		test/test-new-supplicant test/service-move-before \
		test/set-global-timeservers test/get-global-timeservers \
		test/set-nameservers test/set-domains test/set-timeservers \
		test/set-clock

test_scripts += test/vpn-connect test/vpn-disconnect test/vpn-get \
		test/monitor-vpn test/vpn-property

if TEST
testdir = $(pkglibdir)/test
test_SCRIPTS = $(test_scripts)
endif

EXTRA_DIST += $(test_scripts)

EXTRA_DIST += doc/overview-api.txt doc/behavior-api.txt \
				doc/coding-style.txt doc/wifi-p2p-overview.txt \
				doc/vpn-agent-api.txt doc/peer-api.txt \
				doc/ipconfig-api.txt doc/plugin-api.txt \
				doc/manager-api.txt doc/agent-api.txt \
				doc/service-api.txt doc/technology-api.txt \
				doc/counter-api.txt doc/config-format.txt \
				doc/clock-api.txt doc/session-api.txt \
				doc/session-overview.txt doc/backtrace.txt \
				doc/advanced-configuration.txt \
				doc/vpn-config-format.txt \
				doc/vpn-connection-api.txt \
				doc/vpn-manager-api.txt doc/vpn-overview.txt \
				doc/session-policy-format.txt

EXTRA_DIST += src/main.conf \
		src/eduroam.config

MANUAL_PAGES += doc/connman.8 doc/connman.conf.5

dist_man_MANS = $(MANUAL_PAGES)

pkgconfigdir = $(libdir)/pkgconfig

pkgconfig_DATA = connman.pc

DISTCHECK_CONFIGURE_FLAGS = --disable-datafiles \
				--enable-hh2serial-gps \
				--enable-openconnect \
				--enable-openvpn \
				--enable-vpnc \
				--enable-session-policy-local \
				--enable-nmcompat \
				--enable-polkit

DISTCLEANFILES = $(pkgconfig_DATA)

MAINTAINERCLEANFILES = Makefile.in \
	aclocal.m4 configure config.h.in config.sub config.guess \
	ltmain.sh depcomp compile missing install-sh mkinstalldirs test-driver


src/builtin.h: src/genbuiltin $(builtin_sources)
	$(AM_V_GEN)$(srcdir)/src/genbuiltin $(builtin_modules) > $@

vpn/builtin.h: src/genbuiltin $(builtin_vpn_sources)
	$(AM_V_GEN)$(srcdir)/src/genbuiltin $(builtin_vpn_modules) > $@

src/connman.conf: src/connman-dbus.conf src/connman-polkit.conf
if POLKIT
	$(AM_V_GEN)cp $(srcdir)/src/connman-polkit.conf $@
else
	$(AM_V_GEN)cp $(srcdir)/src/connman-dbus.conf $@
endif

if VPN
vpn/connman-vpn-dbus.conf: vpn/vpn-dbus.conf vpn/vpn-polkit.conf
if POLKIT
	$(AM_V_GEN)cp $(srcdir)/vpn/vpn-polkit.conf $@
else
	$(AM_V_GEN)cp $(srcdir)/vpn/vpn-dbus.conf $@
endif
endif

if SELINUX
if VPN
EXTRA_DIST += connman-task.pp
CLEANFILES += connman-task.pp
endif

connman-task.pp: vpn/connman-task.te
	make -f /usr/share/selinux/devel/Makefile
endif

EXTRA_DIST += vpn/connman-task.te

do_subst = $(AM_V_GEN)$(SED) \
		-e 's,[@]prefix[@],$(prefix),g' \
		-e 's,[@]sbindir[@],$(sbindir),g' \
		-e 's,[@]sysconfdir[@],$(sysconfdir),g'

%.service: %.service.in Makefile
	$(do_subst) < $< > $@

scripts/connman: scripts/connman.in Makefile
	$(do_subst) < $< > $@

include/connman/version.h: include/version.h
	$(AM_V_at)$(MKDIR_P) include/connman
	$(AM_V_GEN)$(LN_S) $(abs_top_builddir)/$< $@

include/connman/%.h: $(abs_top_srcdir)/include/%.h
	$(AM_V_at)$(MKDIR_P) include/connman
	$(AM_V_GEN)$(LN_S) $< $@

clean-local:
	@$(RM) -rf include/connman
