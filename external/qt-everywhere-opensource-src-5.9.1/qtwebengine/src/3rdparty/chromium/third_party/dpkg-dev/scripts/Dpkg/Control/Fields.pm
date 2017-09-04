# Copyright © 2007-2009 Raphaël Hertzog <hertzog@debian.org>
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

package Dpkg::Control::Fields;

use strict;
use warnings;

our $VERSION = '1.00';

use Carp;
use Exporter qw(import);

use Dpkg::Control::FieldsCore;
use Dpkg::Vendor qw(run_vendor_hook);

our @EXPORT = @Dpkg::Control::FieldsCore::EXPORT;

# Register vendor specifics fields
foreach my $op (run_vendor_hook('register-custom-fields')) {
    next if not (defined $op and ref $op); # Skip when not implemented by vendor
    my $func = shift @$op;
    if ($func eq 'register') {
        &field_register(@$op);
    } elsif ($func eq 'insert_before') {
        &field_insert_before(@$op);
    } elsif ($func eq 'insert_after') {
        &field_insert_after(@$op);
    } else {
        croak "vendor hook register-custom-fields sent bad data: @$op";
    }
}

=encoding utf8

=head1 NAME

Dpkg::Control::Fields - manage (list of official) control fields

=head1 DESCRIPTION

The module contains a list of vendor-neutral and vendor-specific fieldnames
with associated meta-data explaining in which type of control information
they are allowed. The vendor-neutral fieldnames and all functions are
inherited from Dpkg::Control::FieldsCore.

=head1 AUTHOR

Raphaël Hertzog <hertzog@debian.org>.

=cut

1;
