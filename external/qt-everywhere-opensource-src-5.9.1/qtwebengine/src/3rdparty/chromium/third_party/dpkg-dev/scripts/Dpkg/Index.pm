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

package Dpkg::Index;

use strict;
use warnings;

our $VERSION = '1.00';

use Dpkg::Gettext;
use Dpkg::ErrorHandling;
use Dpkg::Control;
use Dpkg::Compression::FileHandle;

use parent qw(Dpkg::Interface::Storable);

use overload
    '@{}' => sub { return $_[0]->{order} },
    fallback => 1;

=encoding utf8

=head1 NAME

Dpkg::Index - generic index of control information

=head1 DESCRIPTION

This object represent a set of Dpkg::Control objects.

=head1 FUNCTIONS

=over 4

=item my $index = Dpkg::Index->new(%opts)

Creates a new empty index. See set_options() for more details.

=cut

sub new {
    my ($this, %opts) = @_;
    my $class = ref($this) || $this;

    my $self = {
	items => {},
	order => [],
	get_key_func => sub { return $_[0]->{Package} },
	type => CTRL_UNKNOWN,
    };
    bless $self, $class;
    $self->set_options(%opts);
    if (exists $opts{load}) {
	$self->load($opts{load});
    }

    return $self;
}

=item $index->set_options(%opts)

The "type" option is checked first to define default values for other
options. Here are the relevant options: "get_key_func" is a function
returning a key for the item passed in parameters. The index can only
contain one item with a given key. The function used depend on the
type: for CTRL_INFO_PKG, CTRL_INDEX_SRC, CTRL_INDEX_PKG and CTRL_PKG_DEB
it's simply the Package field; for CTRL_PKG_SRC and CTRL_INFO_SRC, it's
the Source field; for CTRL_CHANGELOG it's the Source and the Version
fields (concatenated with an intermediary "_"); for CTRL_FILE_CHANGES it's
the Source, Version and Architecture fields (concatenated with "_");
for CTRL_FILE_VENDOR it's the Vendor field; for CTRL_FILE_STATUS it's the
Package and Architecture fields (concatenated with "_"). Otherwise it's
the Package field by default.

=cut

sub set_options {
    my ($self, %opts) = @_;

    # Default values based on type
    if (exists $opts{type}) {
        my $t = $opts{type};
        if ($t == CTRL_INFO_PKG or $t == CTRL_INDEX_SRC or
	         $t == CTRL_INDEX_PKG or $t == CTRL_PKG_DEB) {
	    $self->{get_key_func} = sub { return $_[0]->{Package}; };
        } elsif ($t == CTRL_PKG_SRC or $t == CTRL_INFO_SRC) {
	    $self->{get_key_func} = sub { return $_[0]->{Source}; };
        } elsif ($t == CTRL_CHANGELOG) {
	    $self->{get_key_func} = sub {
		return $_[0]->{Source} . '_' . $_[0]->{Version};
	    };
        } elsif ($t == CTRL_FILE_CHANGES) {
	    $self->{get_key_func} = sub {
		return $_[0]->{Source} . '_' . $_[0]->{Version} . '_' .
		       $_[0]->{Architecture};
	    };
        } elsif ($t == CTRL_FILE_VENDOR) {
	    $self->{get_key_func} = sub { return $_[0]->{Vendor}; };
        } elsif ($t == CTRL_FILE_STATUS) {
	    $self->{get_key_func} = sub {
		return $_[0]->{Package} . '_' . $_[0]->{Architecture};
	    };
        }
    }

    # Options set by the user override default values
    $self->{$_} = $opts{$_} foreach keys %opts;
}

=item $index->get_type()

Returns the type of control information stored. See the type parameter
set during new().

=cut

sub get_type {
    my ($self) = @_;
    return $self->{type};
}

=item $index->add($item, [$key])

Add a new item in the index. If the $key parameter is omitted, the key
will be generated with the get_key_func function (see set_options() for
details).

=cut

sub add {
    my ($self, $item, $key) = @_;
    unless (defined $key) {
	$key = $self->{get_key_func}($item);
    }
    if (not exists $self->{items}{$key}) {
	push @{$self->{order}}, $key;
    }
    $self->{items}{$key} = $item;
}

=item $index->load($file)

Reads the file and creates all items parsed. Returns the number of items
parsed. Handles compressed files transparently based on their extensions.

=item $index->parse($fh, $desc)

Reads the filehandle and creates all items parsed. Returns the number of
items parsed.

=cut

sub parse {
    my ($self, $fh, $desc) = @_;
    my $item = $self->new_item();
    my $i = 0;
    while ($item->parse($fh, $desc)) {
	$self->add($item);
	$item = $self->new_item();
	$i++;
    }
    return $i;
}

=item $index->save($file)

Writes the content of the index in a file. Auto-compresses files
based on their extensions.

=item my $item = $index->new_item()

Creates a new item. Mainly useful for derived objects that would want
to override this method to return something else than a Dpkg::Control
object.

=cut

sub new_item {
    my ($self) = @_;
    return Dpkg::Control->new(type => $self->{type});
}

=item my $item = $index->get_by_key($key)

Returns the item identified by $key or undef.

=cut

sub get_by_key {
    my ($self, $key) = @_;
    return $self->{items}{$key} if exists $self->{items}{$key};
    return;
}

=item my @keys = $index->get_keys(%criteria)

Returns the keys of items that matches all the criteria. The key of the
%criteria hash is a field name and the value is either a regex that needs
to match the field value, or a reference to a function that must return
true and that receives the field value as single parameter, or a scalar
that must be equal to the field value.

=cut

sub get_keys {
    my ($self, %crit) = @_;
    my @selected = @{$self->{order}};
    foreach my $s_crit (keys %crit) { # search criteria
	if (ref($crit{$s_crit}) eq 'Regexp') {
	    @selected = grep {
		$self->{items}{$_}{$s_crit} =~ $crit{$s_crit}
	    } @selected;
	} elsif (ref($crit{$s_crit}) eq 'CODE') {
	    @selected = grep {
		&{$crit{$s_crit}}($self->{items}{$_}{$s_crit});
	    } @selected;
	} else {
	    @selected = grep {
		$self->{items}{$_}{$s_crit} eq $crit{$s_crit}
	    } @selected;
	}
    }
    return @selected;
}

=item my @items = $index->get(%criteria)

Returns all the items that matches all the criteria.

=cut

sub get {
    my ($self, %crit) = @_;
    return map { $self->{items}{$_} } $self->get_keys(%crit);
}

=item $index->remove_by_key($key)

Remove the item identified by the given key.

=cut

sub remove_by_key {
    my ($self, $key) = @_;
    @{$self->{order}} = grep { $_ ne $key } @{$self->{order}};
    return delete $self->{items}{$key};
}

=item my @items = $index->remove(%criteria)

Returns and removes all the items that matches all the criteria.

=cut

sub remove {
    my ($self, %crit) = @_;
    my @keys = $self->get_keys(%crit);
    my (%keys, @ret);
    foreach my $key (@keys) {
	$keys{$key} = 1;
	push @ret, $self->{items}{$key} if defined wantarray;
	delete $self->{items}{$key};
    }
    @{$self->{order}} = grep { not exists $keys{$_} } @{$self->{order}};
    return @ret;
}

=item $index->merge($other_index, %opts)

Merge the entries of the other index. While merging, the keys of the merged
index are used, they are not re-computed (unless you have set the options
"keep_keys" to "0"). It's your responsibility to ensure that they have been
computed with the same function.

=cut

sub merge {
    my ($self, $other, %opts) = @_;
    $opts{keep_keys} = 1 unless exists $opts{keep_keys};
    foreach my $key ($other->get_keys()) {
	$self->add($other->get_by_key($key), $opts{keep_keys} ? $key : undef);
    }
}

=item $index->sort(\&sortfunc)

Sort the index with the given sort function. If no function is given, an
alphabetic sort is done based on the keys. The sort function receives the
items themselves as parameters and not the keys.

=cut

sub sort {
    my ($self, $func) = @_;
    if (defined $func) {
	@{$self->{order}} = sort {
	    &$func($self->{items}{$a}, $self->{items}{$b})
	} @{$self->{order}};
    } else {
	@{$self->{order}} = sort @{$self->{order}};
    }
}

=item my $str = $index->output()

=item "$index"

Get a string representation of the index. The Dpkg::Control objects are
output in the order which they have been read or added except if the order
hae been changed with sort().

=item $index->output($fh)

Print the string representation of the index to a filehandle.

=cut

sub output {
    my ($self, $fh) = @_;
    my $str = '';
    foreach my $key ($self->get_keys()) {
	if (defined $fh) {
	    print { $fh } $self->get_by_key($key) . "\n";
	}
	if (defined wantarray) {
	    $str .= $self->get_by_key($key) . "\n";
	}
    }
    return $str;
}

=back

=head1 AUTHOR

Raphaël Hertzog <hertzog@debian.org>.

=cut

1;
