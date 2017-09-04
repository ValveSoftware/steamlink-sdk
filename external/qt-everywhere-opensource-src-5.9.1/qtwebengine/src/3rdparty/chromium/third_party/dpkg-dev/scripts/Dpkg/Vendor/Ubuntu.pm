# Copyright © 2008 Ian Jackson <ian@davenant.greenend.org.uk>
# Copyright © 2008 Canonical, Ltd.
#   written by Colin Watson <cjwatson@ubuntu.com>
# Copyright © 2008 James Westby <jw+debian@jameswestby.net>
# Copyright © 2009 Raphaël Hertzog <hertzog@debian.org>
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

package Dpkg::Vendor::Ubuntu;

use strict;
use warnings;

our $VERSION = '0.01';

use Dpkg::ErrorHandling;
use Dpkg::Gettext;
use Dpkg::Path qw(find_command);
use Dpkg::Control::Types;
use Dpkg::BuildOptions;
use Dpkg::Arch qw(debarch_eq get_host_arch);

use parent qw(Dpkg::Vendor::Debian);

=encoding utf8

=head1 NAME

Dpkg::Vendor::Ubuntu - Ubuntu vendor object

=head1 DESCRIPTION

This vendor object customize the behaviour of dpkg-source
to check that Maintainers have been modified if necessary.

=cut

sub run_hook {
    my ($self, $hook, @params) = @_;

    if ($hook eq 'before-source-build') {
        my $src = shift @params;
        my $fields = $src->{fields};

        # check that Maintainer/XSBC-Original-Maintainer comply to
        # https://wiki.ubuntu.com/DebianMaintainerField
        if (defined($fields->{'Version'}) and defined($fields->{'Maintainer'}) and
           $fields->{'Version'} =~ /ubuntu/) {
           if ($fields->{'Maintainer'} !~ /ubuntu/i) {
               if (defined ($ENV{DEBEMAIL}) and $ENV{DEBEMAIL} =~ /\@ubuntu\.com/) {
                   error(_g('Version number suggests Ubuntu changes, but Maintainer: does not have Ubuntu address'));
               } else {
                   warning(_g('Version number suggests Ubuntu changes, but Maintainer: does not have Ubuntu address'));
               }
           }
           unless ($fields->{'Original-Maintainer'}) {
               warning(_g('Version number suggests Ubuntu changes, but there is no XSBC-Original-Maintainer field'));
           }
        }

    } elsif ($hook eq 'keyrings') {
        my @keyrings = $self->SUPER::run_hook($hook);

        push(@keyrings, '/usr/share/keyrings/ubuntu-archive-keyring.gpg');
        return @keyrings;

    } elsif ($hook eq 'register-custom-fields') {
        my @field_ops = $self->SUPER::run_hook($hook);
        push @field_ops,
            [ 'register', 'Launchpad-Bugs-Fixed',
              CTRL_FILE_CHANGES | CTRL_CHANGELOG  ],
            [ 'insert_after', CTRL_FILE_CHANGES, 'Closes', 'Launchpad-Bugs-Fixed' ],
            [ 'insert_after', CTRL_CHANGELOG, 'Closes', 'Launchpad-Bugs-Fixed' ];
        return @field_ops;

    } elsif ($hook eq 'post-process-changelog-entry') {
        my $fields = shift @params;

        # Add Launchpad-Bugs-Fixed field
        my $bugs = find_launchpad_closes($fields->{'Changes'} || '');
        if (scalar(@$bugs)) {
            $fields->{'Launchpad-Bugs-Fixed'} = join(' ', @$bugs);
        }

    } elsif ($hook eq 'update-buildflags') {
	my $flags = shift @params;
	my $build_opts = Dpkg::BuildOptions->new();

	if (!$build_opts->has('noopt')) {
	    if (debarch_eq(get_host_arch(), 'ppc64el')) {
		for my $flag (qw(CFLAGS CXXFLAGS GCJFLAGS FFLAGS)) {
		    $flags->set($flag, '-g -O3', 'vendor');
		}
	    }
	}
	# Per https://wiki.ubuntu.com/DistCompilerFlags
	$flags->set('LDFLAGS', '-Wl,-Bsymbolic-functions', 'vendor');

	# Run the Debian hook to add hardening flags
	$self->SUPER::run_hook($hook, $flags);

	# Allow control of hardening-wrapper via dpkg-buildpackage DEB_BUILD_OPTIONS
	my $hardening;
	if ($build_opts->has('hardening')) {
	    $hardening = $build_opts->get('hardening') // 1;
	}
	if ($build_opts->has('nohardening')) {
	    $hardening = 0;
	}
	if (defined $hardening) {
	    my $flag = 'DEB_BUILD_HARDENING';
	    if ($hardening ne '0') {
		if (!find_command('hardened-cc')) {
		    syserr(_g("'hardening' flag found but 'hardening-wrapper' not installed"));
		}
		if ($hardening ne '1') {
		    my @options = split(/,\s*/, $hardening);
		    $hardening = 1;

		    my @hardopts = qw(format fortify stackprotector pie relro);
		    foreach my $item (@hardopts) {
			my $upitem = uc($item);
			foreach my $option (@options) {
			    if ($option =~ /^(no)?$item$/) {
				$flags->set($flag . '_' . $upitem,
				            not defined $1 or $1 eq '', 'env');
			    }
			}
		    }
		}
	    }
	    if (defined $ENV{$flag}) {
		info(_g('overriding %s in environment: %s'), $flag, $hardening);
	    }
	    $flags->set($flag, $hardening, 'env');
	}

    } else {
        return $self->SUPER::run_hook($hook, @params);
    }

}

=head1 PUBLIC FUNCTIONS

=over

=item $bugs = Dpkg::Vendor::Ubuntu::find_launchpad_closes($changes)

Takes one string as argument and finds "LP: #123456, #654321" statements,
which are references to bugs on Launchpad. Returns all closed bug
numbers in an array reference.

=cut

sub find_launchpad_closes {
    my ($changes) = @_;
    my %closes;

    while ($changes &&
          ($changes =~ /lp:\s+\#\d+(?:,\s*\#\d+)*/ig)) {
        $closes{$_} = 1 foreach($& =~ /\#?\s?(\d+)/g);
    }

    my @closes = sort { $a <=> $b } keys %closes;

    return \@closes;
}

=back

=cut

1;
