# Copyright © 1996 Ian Jackson
# Copyright © 2005 Frank Lichtenheld <frank@lichtenheld.de>
# Copyright © 2009 Raphaël Hertzog <hertzog@debian.org>
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

=encoding utf8

=head1 NAME

Dpkg::Changelog::Debian - parse Debian changelogs

=head1 DESCRIPTION

Dpkg::Changelog::Debian parses Debian changelogs as described in the Debian
policy (version 3.6.2.1 at the time of this writing). See section
L<"SEE ALSO"> for locations where to find this definition.

The parser tries to ignore most cruft like # or /* */ style comments,
CVS comments, vim variables, emacs local variables and stuff from
older changelogs with other formats at the end of the file.
NOTE: most of these are ignored silently currently, there is no
parser error issued for them. This should become configurable in the
future.

=head2 METHODS

=cut

package Dpkg::Changelog::Debian;

use strict;
use warnings;

our $VERSION = '1.00';

use Dpkg::Gettext;
use Dpkg::File;
use Dpkg::Changelog qw(:util);
use parent qw(Dpkg::Changelog);
use Dpkg::Changelog::Entry::Debian qw(match_header match_trailer);

use constant {
    FIRST_HEADING => _g('first heading'),
    NEXT_OR_EOF => _g('next heading or eof'),
    START_CHANGES => _g('start of change data'),
    CHANGES_OR_TRAILER => _g('more change data or trailer'),
};

=over 4

=item $c->parse($fh, $description)

Read the filehandle and parse a Debian changelog in it. Returns the number
of changelog entries that have been parsed with success.

=cut

sub parse {
    my ($self, $fh, $file) = @_;
    $file = $self->{reportfile} if exists $self->{reportfile};

    $self->reset_parse_errors;

    $self->{data} = [];
    $self->set_unparsed_tail(undef);

    my $expect = FIRST_HEADING;
    my $entry = Dpkg::Changelog::Entry::Debian->new();
    my @blanklines = ();
    my $unknowncounter = 1; # to make version unique, e.g. for using as id

    while (<$fh>) {
	chomp;
	if (match_header($_)) {
	    unless ($expect eq FIRST_HEADING || $expect eq NEXT_OR_EOF) {
		$self->parse_error($file, $.,
		    sprintf(_g('found start of entry where expected %s'),
		    $expect), "$_");
	    }
	    unless ($entry->is_empty) {
		push @{$self->{data}}, $entry;
		$entry = Dpkg::Changelog::Entry::Debian->new();
		last if $self->abort_early();
	    }
	    $entry->set_part('header', $_);
	    foreach my $error ($entry->check_header()) {
		$self->parse_error($file, $., $error, $_);
	    }
	    $expect= START_CHANGES;
	    @blanklines = ();
	} elsif (m/^(;;\s*)?Local variables:/io) {
	    last; # skip Emacs variables at end of file
	} elsif (m/^vim:/io) {
	    last; # skip vim variables at end of file
	} elsif (m/^\$\w+:.*\$/o) {
	    next; # skip stuff that look like a CVS keyword
	} elsif (m/^\# /o) {
	    next; # skip comments, even that's not supported
	} elsif (m{^/\*.*\*/}o) {
	    next; # more comments
	} elsif (m/^(\w+\s+\w+\s+\d{1,2} \d{1,2}:\d{1,2}:\d{1,2}\s+[\w\s]*\d{4})\s+(.*)\s+(<|\()(.*)(\)|>)/o
		 || m/^(\w+\s+\w+\s+\d{1,2},?\s*\d{4})\s+(.*)\s+(<|\()(.*)(\)|>)/o
		 || m/^(\w[-+0-9a-z.]*) \(([^\(\) \t]+)\)\;?/io
		 || m/^([\w.+-]+)(-| )(\S+) Debian (\S+)/io
		 || m/^Changes from version (.*) to (.*):/io
		 || m/^Changes for [\w.+-]+-[\w.+-]+:?\s*$/io
		 || m/^Old Changelog:\s*$/io
		 || m/^(?:\d+:)?\w[\w.+~-]*:?\s*$/o) {
	    # save entries on old changelog format verbatim
	    # we assume the rest of the file will be in old format once we
	    # hit it for the first time
	    $self->set_unparsed_tail("$_\n" . file_slurp($fh));
	} elsif (m/^\S/) {
	    $self->parse_error($file, $., _g('badly formatted heading line'), "$_");
	} elsif (match_trailer($_)) {
	    unless ($expect eq CHANGES_OR_TRAILER) {
		$self->parse_error($file, $.,
		    sprintf(_g('found trailer where expected %s'), $expect), "$_");
	    }
	    $entry->set_part('trailer', $_);
	    $entry->extend_part('blank_after_changes', [ @blanklines ]);
	    @blanklines = ();
	    foreach my $error ($entry->check_trailer()) {
		$self->parse_error($file, $., $error, $_);
	    }
	    $expect = NEXT_OR_EOF;
	} elsif (m/^ \-\-/) {
	    $self->parse_error($file, $., _g('badly formatted trailer line'), "$_");
	} elsif (m/^\s{2,}(\S)/) {
	    unless ($expect eq START_CHANGES or $expect eq CHANGES_OR_TRAILER) {
		$self->parse_error($file, $., sprintf(_g('found change data' .
		    ' where expected %s'), $expect), "$_");
		if ($expect eq NEXT_OR_EOF and not $entry->is_empty) {
		    # lets assume we have missed the actual header line
		    push @{$self->{data}}, $entry;
		    $entry = Dpkg::Changelog::Entry::Debian->new();
		    $entry->set_part('header', 'unknown (unknown' . ($unknowncounter++) . ') unknown; urgency=unknown');
		}
	    }
	    # Keep raw changes
	    $entry->extend_part('changes', [ @blanklines, $_ ]);
	    @blanklines = ();
	    $expect = CHANGES_OR_TRAILER;
	} elsif (!m/\S/) {
	    if ($expect eq START_CHANGES) {
		$entry->extend_part('blank_after_header', $_);
		next;
	    } elsif ($expect eq NEXT_OR_EOF) {
		$entry->extend_part('blank_after_trailer', $_);
		next;
	    } elsif ($expect ne CHANGES_OR_TRAILER) {
		$self->parse_error($file, $.,
		    sprintf(_g('found blank line where expected %s'), $expect));
	    }
	    push @blanklines, $_;
	} else {
	    $self->parse_error($file, $., _g('unrecognized line'), "$_");
	    unless ($expect eq START_CHANGES or $expect eq CHANGES_OR_TRAILER) {
		# lets assume change data if we expected it
		$entry->extend_part('changes', [ @blanklines, $_]);
		@blanklines = ();
		$expect = CHANGES_OR_TRAILER;
	    }
	}
    }

    unless ($expect eq NEXT_OR_EOF) {
	$self->parse_error($file, $., sprintf(_g('found eof where expected %s'),
					      $expect));
    }
    unless ($entry->is_empty) {
	push @{$self->{data}}, $entry;
    }

    return scalar @{$self->{data}};
}

1;
__END__

=back

=head1 SEE ALSO

Dpkg::Changelog

Description of the Debian changelog format in the Debian policy:
L<http://www.debian.org/doc/debian-policy/ch-source.html#s-dpkgchangelog>.

=head1 AUTHORS

Frank Lichtenheld, E<lt>frank@lichtenheld.deE<gt>
Raphaël Hertzog, E<lt>hertzog@debian.orgE<gt>

=cut
