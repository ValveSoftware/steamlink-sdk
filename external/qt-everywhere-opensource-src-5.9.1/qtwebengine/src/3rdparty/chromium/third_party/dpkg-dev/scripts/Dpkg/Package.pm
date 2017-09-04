# Copyright © 2006 Frank Lichtenheld <djpig@debian.org>
# Copyright © 2007,2012 Guillem Jover <guillem@debian.org>
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

package Dpkg::Package;

use strict;
use warnings;

our $VERSION = '0.01';

use Dpkg::Gettext;

use Exporter qw(import);
our @EXPORT = qw(pkg_name_is_illegal);

sub pkg_name_is_illegal($) {
    my $name = shift || '';

    if ($name eq '') {
        return _g('may not be empty string');
    }
    if ($name =~ m/[^-+.0-9a-z]/o) {
        return sprintf(_g("character '%s' not allowed"), $&);
    }
    if ($name !~ m/^[0-9a-z]/o) {
        return _g('must start with an alphanumeric character');
    }

    return;
}

1;
