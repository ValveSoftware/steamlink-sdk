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

package Dpkg::Control::Types;

use strict;
use warnings;

our $VERSION = '0.01';

use Exporter qw(import);
our @EXPORT = qw(CTRL_UNKNOWN CTRL_INFO_SRC CTRL_INFO_PKG CTRL_INDEX_SRC
                 CTRL_INDEX_PKG CTRL_PKG_SRC CTRL_PKG_DEB CTRL_FILE_CHANGES
                 CTRL_FILE_VENDOR CTRL_FILE_STATUS CTRL_CHANGELOG);

=encoding utf8

=head1 NAME

Dpkg::Control::Types - export CTRL_* constants

=head1 DESCRIPTION

You should not use this module directly. Instead you more likely
want to use Dpkg::Control which also re-exports the same constants.

This module has been introduced solely to avoid a dependency loop
between Dpkg::Control and Dpkg::Control::Fields.

=cut

use constant {
    CTRL_UNKNOWN => 0,
    CTRL_INFO_SRC => 1,      # First control block in debian/control
    CTRL_INFO_PKG => 2,      # Subsequent control blocks in debian/control
    CTRL_INDEX_SRC => 4,     # Entry in repository's Packages files
    CTRL_INDEX_PKG => 8,     # Entry in repository's Sources files
    CTRL_PKG_SRC => 16,      # .dsc file of source package
    CTRL_PKG_DEB => 32,      # DEBIAN/control in binary packages
    CTRL_FILE_CHANGES => 64, # .changes file
    CTRL_FILE_VENDOR => 128, # File in $Dpkg::CONFDIR/origins
    CTRL_FILE_STATUS => 256, # $Dpkg::ADMINDIR/status
    CTRL_CHANGELOG => 512,   # Output of dpkg-parsechangelog
};

=head1 AUTHOR

Raphaël Hertzog <hertzog@debian.org>.

=cut

1;
