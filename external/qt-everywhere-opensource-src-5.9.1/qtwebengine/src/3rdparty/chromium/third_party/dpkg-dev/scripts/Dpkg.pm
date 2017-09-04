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

package Dpkg;

use strict;
use warnings;

our $VERSION = '1.01';

use Exporter qw(import);
our @EXPORT_OK = qw($PROGNAME $PROGVERSION $CONFDIR $ADMINDIR $LIBDIR $DATADIR);
our @EXPORT = qw($version $progname $admindir $dpkglibdir $pkgdatadir);

our ($PROGNAME) = $0 =~ m{(?:.*/)?([^/]*)};

# The following lines are automatically fixed at install time
our $PROGVERSION = '1.17.x';
our $CONFDIR = '/etc/dpkg';
our $ADMINDIR = '/var/lib/dpkg';
our $LIBDIR = '.';
our $DATADIR = '..';
$DATADIR = $ENV{DPKG_DATADIR} if defined $ENV{DPKG_DATADIR};

# XXX: Backwards compatibility, to be removed on VERSION 2.00.
## no critic (Variables::ProhibitPackageVars)
our $version = $PROGVERSION;
our $admindir = $ADMINDIR;
our $dpkglibdir = $LIBDIR;
our $pkgdatadir = $DATADIR;
## use critic

1;
