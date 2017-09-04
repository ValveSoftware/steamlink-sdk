# Copyright © 2009-2011 Raphaël Hertzog <hertzog@debian.org>
#
# Hardening build flags handling derived from work of:
# Copyright © 2009-2011 Kees Cook <kees@debian.org>
# Copyright © 2007-2008 Canonical, Ltd.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

package Dpkg::Vendor::Debian;

use strict;
use warnings;

our $VERSION = '0.01';

use parent qw(Dpkg::Vendor::Default);

use Dpkg::Gettext;
use Dpkg::ErrorHandling;
use Dpkg::Control::Types;
use Dpkg::BuildOptions;
use Dpkg::Arch qw(get_host_arch debarch_to_debtriplet);

=encoding utf8

=head1 NAME

Dpkg::Vendor::Debian - Debian vendor object

=head1 DESCRIPTION

This vendor object customize the behaviour of dpkg scripts
for Debian specific actions.

=cut

sub run_hook {
    my ($self, $hook, @params) = @_;

    if ($hook eq 'keyrings') {
        return ('/usr/share/keyrings/debian-keyring.gpg',
                '/usr/share/keyrings/debian-maintainers.gpg');
    } elsif ($hook eq 'register-custom-fields') {
    } elsif ($hook eq 'extend-patch-header') {
        my ($textref, $ch_info) = @params;
	if ($ch_info->{'Closes'}) {
	    foreach my $bug (split(/\s+/, $ch_info->{'Closes'})) {
		$$textref .= "Bug-Debian: http://bugs.debian.org/$bug\n";
	    }
	}

	# XXX: Layer violation...
	require Dpkg::Vendor::Ubuntu;
	my $b = Dpkg::Vendor::Ubuntu::find_launchpad_closes($ch_info->{'Changes'});
	foreach my $bug (@$b) {
	    $$textref .= "Bug-Ubuntu: https://bugs.launchpad.net/bugs/$bug\n";
	}
    } elsif ($hook eq 'update-buildflags') {
	$self->add_hardening_flags(@params);
    } else {
        return $self->SUPER::run_hook($hook, @params);
    }
}

sub add_hardening_flags {
    my ($self, $flags) = @_;
    my $arch = get_host_arch();
    my ($abi, $os, $cpu) = debarch_to_debtriplet($arch);

    unless (defined $abi and defined $os and defined $cpu) {
        warning(_g("unknown host architecture '%s'"), $arch);
        ($abi, $os, $cpu) = ('', '', '');
    }

    # Features enabled by default for all builds.
    my %use_feature = (
	pie => 0,
	stackprotector => 1,
	fortify => 1,
	format => 1,
	relro => 1,
	bindnow => 0,
    );

    # Adjust features based on Maintainer's desires.
    my $opts = Dpkg::BuildOptions->new(envvar => 'DEB_BUILD_MAINT_OPTIONS');
    foreach my $feature (split(/,/, $opts->get('hardening') // '')) {
	$feature = lc($feature);
	if ($feature =~ s/^([+-])//) {
	    my $value = ($1 eq '+') ? 1 : 0;
	    if ($feature eq 'all') {
		$use_feature{$_} = $value foreach keys %use_feature;
	    } else {
		if (exists $use_feature{$feature}) {
		    $use_feature{$feature} = $value;
		} else {
		    warning(_g('unknown hardening feature: %s'), $feature);
		}
	    }
	} else {
	    warning(_g('incorrect value in hardening option of ' .
	               'DEB_BUILD_MAINT_OPTIONS: %s'), $feature);
	}
    }

    # Mask features that are not available on certain architectures.
    if ($os !~ /^(linux|knetbsd|hurd)$/ or
	$cpu =~ /^(hppa|mips|mipsel|avr32)$/) {
	# Disabled on non-linux/knetbsd/hurd (see #430455 and #586215).
	# Disabled on hppa, mips/mipsel (#532821), avr32
	#  (#574716).
	$use_feature{pie} = 0;
    }
    if ($cpu =~ /^(ia64|alpha|mips|mipsel|hppa)$/ or $arch eq 'arm') {
	# Stack protector disabled on ia64, alpha, mips, mipsel, hppa.
	#   "warning: -fstack-protector not supported for this target"
	# Stack protector disabled on arm (ok on armel).
	#   compiler supports it incorrectly (leads to SEGV)
	$use_feature{stackprotector} = 0;
    }
    if ($cpu =~ /^(ia64|hppa|avr32)$/) {
	# relro not implemented on ia64, hppa, avr32.
	$use_feature{relro} = 0;
    }

    # Mask features that might be influenced by other flags.
    if ($flags->{build_options}->has('noopt')) {
      # glibc 2.16 and later warn when using -O0 and _FORTIFY_SOURCE.
      $use_feature{fortify} = 0;
    }

    # Handle logical feature interactions.
    if ($use_feature{relro} == 0) {
	# Disable bindnow if relro is not enabled, since it has no
	# hardening ability without relro and may incur load penalties.
	$use_feature{bindnow} = 0;
    }

    # PIE
    if ($use_feature{pie}) {
	$flags->append('CFLAGS', '-fPIE');
	$flags->append('FFLAGS', '-fPIE');
	$flags->append('CXXFLAGS', '-fPIE');
	$flags->append('GCJFLAGS', '-fPIE');
	$flags->append('LDFLAGS', '-fPIE -pie');
    }

    # Stack protector
    if ($use_feature{stackprotector}) {
	$flags->append('CFLAGS', '-fstack-protector --param=ssp-buffer-size=4');
	$flags->append('FFLAGS', '-fstack-protector --param=ssp-buffer-size=4');
	$flags->append('CXXFLAGS', '-fstack-protector --param=ssp-buffer-size=4');
	$flags->append('GCJFLAGS', '-fstack-protector --param=ssp-buffer-size=4');
    }

    # Fortify Source
    if ($use_feature{fortify}) {
	$flags->append('CPPFLAGS', '-D_FORTIFY_SOURCE=2');
    }

    # Format Security
    if ($use_feature{format}) {
	$flags->append('CFLAGS', '-Wformat -Werror=format-security');
	$flags->append('CXXFLAGS', '-Wformat -Werror=format-security');
    }

    # Read-only Relocations
    if ($use_feature{relro}) {
	$flags->append('LDFLAGS', '-Wl,-z,relro');
    }

    # Bindnow
    if ($use_feature{bindnow}) {
	$flags->append('LDFLAGS', '-Wl,-z,now');
    }

    # Store the feature usage.
    while (my ($feature, $enabled) = each %use_feature) {
	$flags->set_feature('hardening', $feature, $enabled);
    }
}

1;
