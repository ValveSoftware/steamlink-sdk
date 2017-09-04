# Copyright © 2009-2010 Raphaël Hertzog <hertzog@debian.org>
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

package Dpkg::Conf;

use strict;
use warnings;

our $VERSION = '1.01';

use Dpkg::Gettext;
use Dpkg::ErrorHandling;

use parent qw(Dpkg::Interface::Storable);

use overload
    '@{}' => sub { return [ $_[0]->get_options() ] },
    fallback => 1;

=encoding utf8

=head1 NAME

Dpkg::Conf - parse dpkg configuration files

=head1 DESCRIPTION

The Dpkg::Conf object can be used to read options from a configuration
file. It can exports an array that can then be parsed exactly like @ARGV.

=head1 FUNCTIONS

=over 4

=item my $conf = Dpkg::Conf->new(%opts)

Create a new Dpkg::Conf object. Some options can be set through %opts:
if allow_short evaluates to true (it defaults to false), then short
options are allowed in the configuration file, they should be prepended
with a single hyphen.

=cut

sub new {
    my ($this, %opts) = @_;
    my $class = ref($this) || $this;

    my $self = {
	options => [],
	allow_short => 0,
    };
    foreach my $opt (keys %opts) {
	$self->{$opt} = $opts{$opt};
    }
    bless $self, $class;

    return $self;
}

=item @$conf

=item @options = $conf->get_options()

Returns the list of options that can be parsed like @ARGV.

=cut

sub get_options {
    my ($self) = @_;
    return @{$self->{options}};
}

=item $conf->load($file)

Read options from a file. Return the number of options parsed.

=item $conf->parse($fh)

Parse options from a file handle. Return the number of options parsed.

=cut

sub parse {
    my ($self, $fh, $desc) = @_;
    my $count = 0;
    while (<$fh>) {
	chomp;
	s/^\s+//; s/\s+$//;   # Strip leading/trailing spaces
	s/\s+=\s+/=/;         # Remove spaces around the first =
	s/\s+/=/ unless m/=/; # First spaces becomes = if no =
	next if /^#/ or /^$/; # Skip empty lines and comments
	if (/^-[^-]/ and not $self->{allow_short}) {
	    warning(_g('short option not allowed in %s, line %d'), $desc, $.);
	    next;
	}
	if (/^([^=]+)(?:=(.*))?$/) {
	    my ($name, $value) = ($1, $2);
	    $name = "--$name" unless $name =~ /^-/;
	    if (defined $value) {
		$value =~ s/^"(.*)"$/$1/ or $value =~ s/^'(.*)'$/$1/;
		push @{$self->{options}}, "$name=$value";
	    } else {
		push @{$self->{options}}, $name;
	    }
	    $count++;
	} else {
	    warning(_g('invalid syntax for option in %s, line %d'), $desc, $.);
	}
    }
    return $count;
}

=item $conf->filter(remove => $rmfunc)

=item $conf->filter(keep => $keepfunc)

Filter the list of options, either removing or keeping all those that
return true when &$rmfunc($option) or &keepfunc($option) is called.

=cut

sub filter {
    my ($self, %opts) = @_;
    if (defined($opts{remove})) {
	@{$self->{options}} = grep { not &{$opts{remove}}($_) }
				     @{$self->{options}};
    }
    if (defined($opts{keep})) {
	@{$self->{options}} = grep { &{$opts{keep}}($_) }
				     @{$self->{options}};
    }
}

=item $string = $conf->output($fh)

Write the options in the given filehandle (if defined) and return a string
representation of the content (that would be) written.

=item "$conf"

Return a string representation of the content.

=item $conf->save($file)

Save the options in a file.

=cut

sub output {
    my ($self, $fh) = @_;
    my $ret = '';
    foreach my $opt ($self->get_options()) {
	$opt =~ s/^--//;
	if ($opt =~ s/^([^=]+)=/$1 = "/) {
	    $opt .= '"';
	}
	$opt .= "\n";
	print { $fh } $opt if defined $fh;
	$ret .= $opt;
    }
    return $ret;
}

=back

=head1 AUTHOR

Raphaël Hertzog <hertzog@debian.org>.

=cut

1;
