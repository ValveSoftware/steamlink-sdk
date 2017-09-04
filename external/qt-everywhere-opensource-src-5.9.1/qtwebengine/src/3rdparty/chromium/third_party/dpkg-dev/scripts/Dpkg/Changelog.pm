# Copyright © 2005, 2007 Frank Lichtenheld <frank@lichtenheld.de>
# Copyright © 2009       Raphaël Hertzog <hertzog@debian.org>
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

=encoding utf8

=head1 NAME

Dpkg::Changelog - base class to implement a changelog parser

=head1 DESCRIPTION

Dpkg::Changelog is a class representing a changelog file
as an array of changelog entries (Dpkg::Changelog::Entry).
By deriving this object and implementing its parse method, you
add the ability to fill this object with changelog entries.

=head2 FUNCTIONS

=cut

package Dpkg::Changelog;

use strict;
use warnings;

our $VERSION = '1.00';

use Dpkg;
use Dpkg::Gettext;
use Dpkg::ErrorHandling qw(:DEFAULT report);
use Dpkg::Control;
use Dpkg::Control::Changelog;
use Dpkg::Control::Fields;
use Dpkg::Index;
use Dpkg::Version;
use Dpkg::Vendor qw(run_vendor_hook);

use parent qw(Dpkg::Interface::Storable);

use overload
    '@{}' => sub { return $_[0]->{data} };

=over 4

=item my $c = Dpkg::Changelog->new(%options)

Creates a new changelog object.

=cut

sub new {
    my ($this, %opts) = @_;
    my $class = ref($this) || $this;
    my $self = {
	verbose => 1,
	parse_errors => []
    };
    bless $self, $class;
    $self->set_options(%opts);
    return $self;
}

=item $c->load($filename)

Parse $filename as a changelog.

=cut

=item $c->set_options(%opts)

Change the value of some options. "verbose" (defaults to 1) defines
whether parse errors are displayed as warnings by default. "reportfile"
is a string to use instead of the name of the file parsed, in particular
in error messages. "range" defines the range of entries that we want to
parse, the parser will stop as soon as it has parsed enough data to
satisfy $c->get_range($opts{range}).

=cut

sub set_options {
    my ($self, %opts) = @_;
    $self->{$_} = $opts{$_} foreach keys %opts;
}

=item $c->reset_parse_errors()

Can be used to delete all information about errors occurred during
previous L<parse> runs.

=cut

sub reset_parse_errors {
    my ($self) = @_;
    $self->{parse_errors} = [];
}

=item $c->parse_error($file, $line_nr, $error, [$line])

Record a new parse error in $file at line $line_nr. The error message is
specified with $error and a copy of the line can be recorded in $line.

=cut

sub parse_error {
    my ($self, $file, $line_nr, $error, $line) = @_;
    shift;

    push @{$self->{parse_errors}}, [ @_ ];

    if ($self->{verbose}) {
	if ($line) {
	    warning("%20s(l$line_nr): $error\nLINE: $line", $file);
	} else {
	    warning("%20s(l$line_nr): $error", $file);
	}
    }
}

=item $c->get_parse_errors()

Returns all error messages from the last L<parse> run.
If called in scalar context returns a human readable
string representation. If called in list context returns
an array of arrays. Each of these arrays contains

=over 4

=item 1.

a string describing the origin of the data (a filename usually). If the
reportfile configuration option was given, its value will be used instead.

=item 2.

the line number where the error occurred

=item 3.

an error description

=item 4.

the original line

=back

=cut

sub get_parse_errors {
    my ($self) = @_;

    if (wantarray) {
	return @{$self->{parse_errors}};
    } else {
	my $res = '';
	foreach my $e (@{$self->{parse_errors}}) {
	    if ($e->[3]) {
		$res .= report(_g('warning'),_g("%s(l%s): %s\nLINE: %s"), @$e );
	    } else {
		$res .= report(_g('warning'), _g('%s(l%s): %s'), @$e);
	    }
	}
	return $res;
    }
}

=item $c->set_unparsed_tail($tail)

Add a string representing unparsed lines after the changelog entries.
Use undef as $tail to remove the unparsed lines currently set.

=item $c->get_unparsed_tail()

Return a string representing the unparsed lines after the changelog
entries. Returns undef if there's no such thing.

=cut

sub set_unparsed_tail {
    my ($self, $tail) = @_;
    $self->{unparsed_tail} = $tail;
}

sub get_unparsed_tail {
    my ($self) = @_;
    return $self->{unparsed_tail};
}

=item @{$c}

Returns all the Dpkg::Changelog::Entry objects contained in this changelog
in the order in which they have been parsed.

=item $c->get_range($range)

Returns an array (if called in list context) or a reference to an array of
Dpkg::Changelog::Entry objects which each represent one entry of the
changelog. $range is a hash reference describing the range of entries
to return. See section L<"RANGE SELECTION">.

=cut

sub __sanity_check_range {
    my ($self, $r) = @_;
    my $data = $self->{data};

    if (defined($r->{offset}) and not defined($r->{count})) {
	warning(_g("'offset' without 'count' has no effect")) if $self->{verbose};
	delete $r->{offset};
    }

    ## no critic (ControlStructures::ProhibitUntilBlocks)
    if ((defined($r->{count}) || defined($r->{offset})) &&
        (defined($r->{from}) || defined($r->{since}) ||
	 defined($r->{to}) || defined($r->{until})))
    {
	warning(_g("you can't combine 'count' or 'offset' with any other " .
		   'range option')) if $self->{verbose};
	delete $r->{from};
	delete $r->{since};
	delete $r->{to};
	delete $r->{until};
    }
    if (defined($r->{from}) && defined($r->{since})) {
	warning(_g("you can only specify one of 'from' and 'since', using " .
		   "'since'")) if $self->{verbose};
	delete $r->{from};
    }
    if (defined($r->{to}) && defined($r->{until})) {
	warning(_g("you can only specify one of 'to' and 'until', using " .
		   "'until'")) if $self->{verbose};
	delete $r->{to};
    }

    # Handle non-existing versions
    my (%versions, @versions);
    foreach my $entry (@{$data}) {
        $versions{$entry->get_version()->as_string()} = 1;
        push @versions, $entry->get_version()->as_string();
    }
    if ((defined($r->{since}) and not exists $versions{$r->{since}})) {
        warning(_g("'%s' option specifies non-existing version"), 'since');
        warning(_g('use newest entry that is earlier than the one specified'));
        foreach my $v (@versions) {
            if (version_compare_relation($v, REL_LT, $r->{since})) {
                $r->{since} = $v;
                last;
            }
        }
        if (not exists $versions{$r->{since}}) {
            # No version was earlier, include all
            warning(_g('none found, starting from the oldest entry'));
            delete $r->{since};
            $r->{from} = $versions[-1];
        }
    }
    if ((defined($r->{from}) and not exists $versions{$r->{from}})) {
        warning(_g("'%s' option specifies non-existing version"), 'from');
        warning(_g('use oldest entry that is later than the one specified'));
        my $oldest;
        foreach my $v (@versions) {
            if (version_compare_relation($v, REL_GT, $r->{from})) {
                $oldest = $v;
            }
        }
        if (defined($oldest)) {
            $r->{from} = $oldest;
        } else {
            warning(_g("no such entry found, ignoring '%s' parameter"), 'from');
            delete $r->{from}; # No version was oldest
        }
    }
    if (defined($r->{until}) and not exists $versions{$r->{until}}) {
        warning(_g("'%s' option specifies non-existing version"), 'until');
        warning(_g('use oldest entry that is later than the one specified'));
        my $oldest;
        foreach my $v (@versions) {
            if (version_compare_relation($v, REL_GT, $r->{until})) {
                $oldest = $v;
            }
        }
        if (defined($oldest)) {
            $r->{until} = $oldest;
        } else {
            warning(_g("no such entry found, ignoring '%s' parameter"), 'until');
            delete $r->{until}; # No version was oldest
        }
    }
    if (defined($r->{to}) and not exists $versions{$r->{to}}) {
        warning(_g("'%s' option specifies non-existing version"), 'to');
        warning(_g('use newest entry that is earlier than the one specified'));
        foreach my $v (@versions) {
            if (version_compare_relation($v, REL_LT, $r->{to})) {
                $r->{to} = $v;
                last;
            }
        }
        if (not exists $versions{$r->{to}}) {
            # No version was earlier
            warning(_g("no such entry found, ignoring '%s' parameter"), 'to');
            delete $r->{to};
        }
    }

    if (defined($r->{since}) and $data->[0]->get_version() eq $r->{since}) {
	warning(_g("'since' option specifies most recent version, ignoring"));
	delete $r->{since};
    }
    if (defined($r->{until}) and $data->[-1]->get_version() eq $r->{until}) {
	warning(_g("'until' option specifies oldest version, ignoring"));
	delete $r->{until};
    }
    ## use critic
}

sub get_range {
    my ($self, $range) = @_;
    $range //= {};
    my $res = $self->_data_range($range);
    if (defined $res) {
	return @$res if wantarray;
	return $res;
    } else {
	return;
    }
}

sub _is_full_range {
    my ($self, $range) = @_;

    return 1 if $range->{all};

    # If no range delimiter is specified, we want everything.
    foreach (qw(since until from to count offset)) {
        return 0 if exists $range->{$_};
    }

    return 1;
}

sub _data_range {
    my ($self, $range) = @_;

    my $data = $self->{data} or return;

    return [ @$data ] if $self->_is_full_range($range);

    $self->__sanity_check_range($range);

    my ($start, $end);
    if (defined($range->{count})) {
	my $offset = $range->{offset} || 0;
	my $count = $range->{count};
	# Convert count/offset in start/end
	if ($offset > 0) {
	    $offset -= ($count < 0);
	} elsif ($offset < 0) {
	    $offset = $#$data + ($count > 0) + $offset;
	} else {
	    $offset = $#$data if $count < 0;
	}
	$start = $end = $offset;
	$start += $count+1 if $count < 0;
	$end += $count-1 if $count > 0;
	# Check limits
	$start = 0 if $start < 0;
	return if $start > $#$data;
	$end = $#$data if $end > $#$data;
	return if $end < 0;
	$end = $start if $end < $start;
	return [ @{$data}[$start .. $end] ];
    }

    ## no critic (ControlStructures::ProhibitUntilBlocks)
    my @result;
    my $include = 1;
    $include = 0 if defined($range->{to}) or defined($range->{until});
    foreach (@$data) {
	my $v = $_->get_version();
	$include = 1 if defined($range->{to}) and $v eq $range->{to};
	last if defined($range->{since}) and $v eq $range->{since};

	push @result, $_ if $include;

	$include = 1 if defined($range->{until}) and $v eq $range->{until};
	last if defined($range->{from}) and $v eq $range->{from};
    }
    ## use critic

    return \@result if scalar(@result);
    return;
}

=item $c->abort_early()

Returns true if enough data have been parsed to be able to return all
entries selected by the range set at creation (or with set_options).

=cut

sub abort_early {
    my ($self) = @_;

    my $data = $self->{data} or return;
    my $r = $self->{range} or return;
    my $count = $r->{count} || 0;
    my $offset = $r->{offset} || 0;

    return if $self->_is_full_range($r);
    return if $offset < 0 or $count < 0;
    if (defined($r->{count})) {
	if ($offset > 0) {
	    $offset -= ($count < 0);
	}
	my $start = my $end = $offset;
	$end += $count-1 if $count > 0;
	return ($start < @$data and $end < @$data);
    }

    return unless defined($r->{since}) or defined($r->{from});
    foreach (@$data) {
	my $v = $_->get_version();
	return 1 if defined($r->{since}) and $v eq $r->{since};
	return 1 if defined($r->{from}) and $v eq $r->{from};
    }

    return;
}

=item $c->save($filename)

Save the changelog in the given file.

=item $c->output()

=item "$c"

Returns a string representation of the changelog (it's a concatenation of
the string representation of the individual changelog entries).

=item $c->output($fh)

Output the changelog to the given filehandle.

=cut

sub output {
    my ($self, $fh) = @_;
    my $str = '';
    foreach my $entry (@{$self}) {
	my $text = $entry->output();
	print { $fh } $text if defined $fh;
	$str .= $text if defined wantarray;
    }
    my $text = $self->get_unparsed_tail();
    if (defined $text) {
	print { $fh } $text if defined $fh;
	$str .= $text if defined wantarray;
    }
    return $str;
}

=item my $control = $c->dpkg($range)

Returns a Dpkg::Control::Changelog object representing the entries selected
by the optional range specifier (see L<"RANGE SELECTION"> for details).
Returns undef in no entries are matched.

The following fields are contained in the object:

=over 4

=item Source

package name (in the first entry)

=item Version

packages' version (from first entry)

=item Distribution

target distribution (from first entry)

=item Urgency

urgency (highest of all printed entries)

=item Maintainer

person that created the (first) entry

=item Date

date of the (first) entry

=item Closes

bugs closed by the entry/entries, sorted by bug number

=item Changes

content of the the entry/entries

=back

=cut

our ( @URGENCIES, %URGENCIES );
BEGIN {
    @URGENCIES = qw(low medium high critical emergency);
    my $i = 1;
    %URGENCIES = map { $_ => $i++ } @URGENCIES;
}

sub dpkg {
    my ($self, $range) = @_;

    my @data = $self->get_range($range) or return;
    my $src = shift @data;

    my $f = Dpkg::Control::Changelog->new();
    $f->{Urgency} = $src->get_urgency() || 'unknown';
    $f->{Source} = $src->get_source() || 'unknown';
    $f->{Version} = $src->get_version() // 'unknown';
    $f->{Distribution} = join(' ', $src->get_distributions());
    $f->{Maintainer} = $src->get_maintainer() || '';
    $f->{Date} = $src->get_timestamp() || '';
    $f->{Changes} = $src->get_dpkg_changes();

    # handle optional fields
    my $opts = $src->get_optional_fields();
    my %closes;
    foreach (keys %$opts) {
	if (/^Urgency$/i) { # Already dealt
	} elsif (/^Closes$/i) {
	    $closes{$_} = 1 foreach (split(/\s+/, $opts->{Closes}));
	} else {
	    field_transfer_single($opts, $f);
	}
    }

    foreach my $bin (@data) {
	my $oldurg = $f->{Urgency} || '';
	my $oldurgn = $URGENCIES{$f->{Urgency}} || -1;
	my $newurg = $bin->get_urgency() || '';
	my $newurgn = $URGENCIES{$newurg} || -1;
	$f->{Urgency} = ($newurgn > $oldurgn) ? $newurg : $oldurg;
	$f->{Changes} .= "\n" . $bin->get_dpkg_changes();

	# handle optional fields
	$opts = $bin->get_optional_fields();
	foreach (keys %$opts) {
	    if (/^Closes$/i) {
		$closes{$_} = 1 foreach (split(/\s+/, $opts->{Closes}));
	    } elsif (not exists $f->{$_}) { # Don't overwrite an existing field
		field_transfer_single($opts, $f);
	    }
	}
    }

    if (scalar keys %closes) {
	$f->{Closes} = join ' ', sort { $a <=> $b } keys %closes;
    }
    run_vendor_hook('post-process-changelog-entry', $f);

    return $f;
}

=item my @controls = $c->rfc822($range)

Returns a Dpkg::Index containing Dpkg::Control::Changelog objects where
each object represents one entry in the changelog that is part of the
range requested (see L<"RANGE SELECTION"> for details). For the format of
such an object see the description of the L<"dpkg"> method (while ignoring
the remarks about which values are taken from the first entry).

=cut

sub rfc822 {
    my ($self, $range) = @_;

    my @data = $self->get_range($range) or return;
    my $index = Dpkg::Index->new(type => CTRL_CHANGELOG);

    foreach my $entry (@data) {
	my $f = Dpkg::Control::Changelog->new();
	$f->{Urgency} = $entry->get_urgency() || 'unknown';
	$f->{Source} = $entry->get_source() || 'unknown';
	$f->{Version} = $entry->get_version() // 'unknown';
	$f->{Distribution} = join(' ', $entry->get_distributions());
	$f->{Maintainer} = $entry->get_maintainer() || '';
	$f->{Date} = $entry->get_timestamp() || '';
	$f->{Changes} = $entry->get_dpkg_changes();

	# handle optional fields
	my $opts = $entry->get_optional_fields();
	foreach (keys %$opts) {
	    field_transfer_single($opts, $f) unless exists $f->{$_};
	}

        run_vendor_hook('post-process-changelog-entry', $f);

	$index->add($f);
    }
    return $index;
}

=back

=head1 RANGE SELECTION

A range selection is described by a hash reference where
the allowed keys and values are described below.

The following options take a version number as value.

=over 4

=item since

Causes changelog information from all versions strictly
later than B<version> to be used.

=item until

Causes changelog information from all versions strictly
earlier than B<version> to be used.

=item from

Similar to C<since> but also includes the information for the
specified B<version> itself.

=item to

Similar to C<until> but also includes the information for the
specified B<version> itself.

=back

The following options don't take version numbers as values:

=over 4

=item all

If set to a true value, all entries of the changelog are returned,
this overrides all other options.

=item count

Expects a signed integer as value. Returns C<value> entries from the
top of the changelog if set to a positive integer, and C<abs(value)>
entries from the tail if set to a negative integer.

=item offset

Expects a signed integer as value. Changes the starting point for
C<count>, either counted from the top (positive integer) or from
the tail (negative integer). C<offset> has no effect if C<count>
wasn't given as well.

=back

Some examples for the above options. Imagine an example changelog with
entries for the versions 1.2, 1.3, 2.0, 2.1, 2.2, 3.0 and 3.1.

            Range                           Included entries
 C<{ since =E<gt> '2.0' }>                  3.1, 3.0, 2.2
 C<{ until =E<gt> '2.0' }>                  1.3, 1.2
 C<{ from =E<gt> '2.0' }>                   3.1, 3.0, 2.2, 2.1, 2.0
 C<{ to =E<gt> '2.0' }>                     2.0, 1.3, 1.2
 C<{ count =E<gt> 2 }>                      3.1, 3.0
 C<{ count =E<gt> -2 }>	                    1.3, 1.2
 C<{ count =E<gt> 3, offset=E<gt> 2 }>      2.2, 2.1, 2.0
 C<{ count =E<gt> 2, offset=E<gt> -3 }>     2.0, 1.3
 C<{ count =E<gt> -2, offset=E<gt> 3 }>     3.0, 2.2
 C<{ count =E<gt> -2, offset=E<gt> -3 }>    2.2, 2.1

Any combination of one option of C<since> and C<from> and one of
C<until> and C<to> returns the intersection of the two results
with only one of the options specified.

=head1 AUTHOR

Frank Lichtenheld, E<lt>frank@lichtenheld.deE<gt>
Raphaël Hertzog, E<lt>hertzog@debian.orgE<gt>

=cut
1;
