# Copyright © 2002 Adam Heath <doogie@debian.org>
# Copyright © 2012-2013 Guillem Jover <guillem@debian.org>
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

package Dpkg::Exit;

use strict;
use warnings;

our $VERSION = '1.01';

use Exporter qw(import);

our @EXPORT_OK = qw(push_exit_handler pop_exit_handler run_exit_handlers);

# XXX: Backwards compatibility, stop exporting on VERSION 2.00.
## no critic (Variables::ProhibitPackageVars)
our @handlers = ();
## use critic

=encoding utf8

=head1 NAME

Dpkg::Exit - program exit handlers

=head1 DESCRIPTION

The Dpkg::Exit module provides support functions to run handlers on exit.

=head1 FUNCTIONS

=over 4

=item push_exit_handler($func)

Register a code reference into the exit function handlers stack.

=cut

sub push_exit_handler {
    my ($func) = shift;
    push @handlers, $func;
}

=item pop_exit_handler()

Pop the last registered exit handler from the handlers stack.

=cut

sub pop_exit_handler {
    pop @handlers;
}

=item run_exit_handlers()

Run the registered exit handlers.

=cut

sub run_exit_handlers {
    &$_() foreach (reverse @handlers);
}

sub exit_handler {
    run_exit_handlers();
    exit(127);
}

$SIG{INT} = \&exit_handler;
$SIG{HUP} = \&exit_handler;
$SIG{QUIT} = \&exit_handler;

=back

=head1 CHANGES

=head2 Version 1.01

New functions: push_exit_handler(), pop_exit_handler(), run_exit_handlers()
Deprecated variable: @handlers

=cut

1;
