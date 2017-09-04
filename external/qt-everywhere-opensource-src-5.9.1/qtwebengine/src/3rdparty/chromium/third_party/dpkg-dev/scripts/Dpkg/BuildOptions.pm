# Copyright © 2007 Frank Lichtenheld <djpig@debian.org>
# Copyright © 2010 Raphaël Hertzog <hertzog@debian.org>
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

package Dpkg::BuildOptions;

use strict;
use warnings;

our $VERSION = '1.01';

use Dpkg::Gettext;
use Dpkg::ErrorHandling;
use Dpkg::BuildEnv;

=encoding utf8

=head1 NAME

Dpkg::BuildOptions - parse and update build options

=head1 DESCRIPTION

The Dpkg::BuildOptions object can be used to manipulate options stored
in environment variables like DEB_BUILD_OPTIONS and
DEB_BUILD_MAINT_OPTIONS.

=head1 FUNCTIONS

=over 4

=item my $bo = Dpkg::BuildOptions->new(%opts)

Create a new Dpkg::BuildOptions object. It will be initialized based
on the value of the environment variable named $opts{envvar} (or
DEB_BUILD_OPTIONS if that option is not set).

=cut

sub new {
    my ($this, %opts) = @_;
    my $class = ref($this) || $this;

    my $self = {
        options => {},
	source => {},
	envvar => $opts{envvar} // 'DEB_BUILD_OPTIONS',
    };
    bless $self, $class;
    $self->merge(Dpkg::BuildEnv::get($self->{envvar}), $self->{envvar});
    return $self;
}

=item $bo->reset()

Reset the object to not have any option (it's empty).

=cut

sub reset {
    my ($self) = @_;
    $self->{options} = {};
    $self->{source} = {};
}

=item $bo->merge($content, $source)

Merge the options set in $content and record that they come from the
source $source. $source is mainly used in warning messages currently
to indicate where invalid options have been detected.

$content is a space separated list of options with optional assigned
values like "nocheck parallel=2".

=cut

sub merge {
    my ($self, $content, $source) = @_;
    return 0 unless defined $content;
    my $count = 0;
    foreach (split(/\s+/, $content)) {
	unless (/^([a-z][a-z0-9_-]*)(?:=(\S*))?$/) {
            warning(_g('invalid flag in %s: %s'), $source, $_);
            next;
        }
	$count += $self->set($1, $2, $source);
    }
    return $count;
}

=item $bo->set($option, $value, [$source])

Store the given option in the objet with the given value. It's legitimate
for a value to be undefined if the option is a simple boolean (its
presence means true, its absence means false). The $source is optional
and indicates where the option comes from.

The known options have their values checked for sanity. Options without
values have their value removed and options with invalid values are
discarded.

=cut

sub set {
    my ($self, $key, $value, $source) = @_;

    # Sanity checks
    if ($key =~ /^(noopt|nostrip|nocheck)$/ && defined($value)) {
	$value = undef;
    } elsif ($key eq 'parallel')  {
	$value //= '';
	return 0 if $value !~ /^\d*$/;
    }

    $self->{options}{$key} = $value;
    $self->{source}{$key} = $source;

    return 1;
}

=item $bo->get($option)

Return the value associated to the option. It might be undef even if the
option exists. You might want to check with $bo->has($option) to verify if
the option is stored in the object.

=cut

sub get {
    my ($self, $key) = @_;
    return $self->{options}{$key};
}

=item $bo->has($option)

Returns a boolean indicating whether the option is stored in the object.

=cut

sub has {
    my ($self, $key) = @_;
    return exists $self->{options}{$key};
}

=item $string = $bo->output($fh)

Return a string representation of the build options suitable to be
assigned to an environment variable. Can optionnaly output that string to
the given filehandle.

=cut

sub output {
    my ($self, $fh) = @_;
    my $o = $self->{options};
    my $res = join(' ', map { defined($o->{$_}) ? $_ . '=' . $o->{$_} : $_ } sort keys %$o);
    print { $fh } $res if defined $fh;
    return $res;
}

=item $bo->export([$var])

Export the build options to the given environment variable. If omitted,
the environment variable defined at creation time is assumed. The value
set to the variable is also returned.

=cut

sub export {
    my ($self, $var) = @_;
    $var = $self->{envvar} unless defined $var;
    my $content = $self->output();
    Dpkg::BuildEnv::set($var, $content);
    return $content;
}

=back

=head1 CHANGES

=head2 Version 1.01

Enable to use another environment variable instead of DEB_BUILD_OPTIONS.
Thus add support for the "envvar" option at creation time.

=head1 AUTHOR

Raphaël Hertzog <hertzog@debian.org>

=cut

1;
