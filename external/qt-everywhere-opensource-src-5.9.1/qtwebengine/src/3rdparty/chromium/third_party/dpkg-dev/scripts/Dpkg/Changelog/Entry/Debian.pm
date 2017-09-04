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

package Dpkg::Changelog::Entry::Debian;

use strict;
use warnings;

our $VERSION = '1.01';

use Exporter qw(import);
use Dpkg::Changelog::Entry;
use parent qw(Dpkg::Changelog::Entry);
our @EXPORT_OK = qw(match_header match_trailer find_closes
                    $regex_header $regex_trailer);

use Date::Parse;

use Dpkg::Gettext;
use Dpkg::Control::Fields;
use Dpkg::Control::Changelog;
use Dpkg::Version;

=encoding utf8

=head1 NAME

Dpkg::Changelog::Entry::Debian - represents a Debian changelog entry

=head1 DESCRIPTION

This object represents a Debian changelog entry. It implements the
generic interface Dpkg::Changelog::Entry. Only functions specific to this
implementation are described below.

=cut

my $name_chars = qr/[-+0-9a-z.]/i;

# XXX: Backwards compatibility, stop exporting on VERSION 2.00.
## no critic (Variables::ProhibitPackageVars)

# The matched content is the source package name ($1), the version ($2),
# the target distributions ($3) and the options on the rest of the line ($4).
our $regex_header = qr/^(\w$name_chars*) \(([^\(\) \t]+)\)((?:\s+$name_chars+)+)\;(.*?)\s*$/i;

# The matched content is the maintainer name ($1), its email ($2),
# some blanks ($3) and the timestamp ($4).
our $regex_trailer = qr/^ \-\- (.*) <(.*)>(  ?)((\w+\,\s*)?\d{1,2}\s+\w+\s+\d{4}\s+\d{1,2}:\d\d:\d\d\s+[-+]\d{4}(\s+\([^\\\(\)]\))?)\s*$/o;

## use critic

=head1 FUNCTIONS

=over 4

=item my @items = $entry->get_change_items()

Return a list of change items. Each item contains at least one line.
A change line starting with an asterisk denotes the start of a new item.
Any change line like "[ Raphaël Hertzog ]" is treated like an item of its
own even if it starts a set of items attributed to this person (the
following line necessarily starts a new item).

=cut

sub get_change_items {
    my ($self) = @_;
    my (@items, @blanks, $item);
    foreach my $line (@{$self->get_part('changes')}) {
	if ($line =~ /^\s*\*/) {
	    push @items, $item if defined $item;
	    $item = "$line\n";
	} elsif ($line =~ /^\s*\[\s[^\]]+\s\]\s*$/) {
	    push @items, $item if defined $item;
	    push @items, "$line\n";
	    $item = undef;
	    @blanks = ();
	} elsif ($line =~ /^\s*$/) {
	    push @blanks, "$line\n";
	} else {
	    if (defined $item) {
		$item .= "@blanks$line\n";
	    } else {
		$item = "$line\n";
	    }
	    @blanks = ();
	}
    }
    push @items, $item if defined $item;
    return @items;
}

=item my @errors = $entry->check_header()

=item my @errors = $entry->check_trailer()

Return a list of errors. Each item in the list is an error message
describing the problem. If the empty list is returned, no errors
have been found.

=cut

sub check_header {
    my ($self) = @_;
    my @errors;
    if (defined($self->{header}) and $self->{header} =~ $regex_header) {
	my ($version, $options) = ($2, $4);
	$options =~ s/^\s+//;
	my %optdone;
	foreach my $opt (split(/\s*,\s*/, $options)) {
	    unless ($opt =~ m/^([-0-9a-z]+)\=\s*(.*\S)$/i) {
		push @errors, sprintf(_g("bad key-value after \`;': \`%s'"), $opt);
		next;
	    }
	    my ($k, $v) = (field_capitalize($1), $2);
	    if ($optdone{$k}) {
		push @errors, sprintf(_g('repeated key-value %s'), $k);
	    }
	    $optdone{$k} = 1;
	    if ($k eq 'Urgency') {
		push @errors, sprintf(_g('badly formatted urgency value: %s'), $v)
		    unless ($v =~ m/^([-0-9a-z]+)((\s+.*)?)$/i);
	    } elsif ($k eq 'Binary-Only') {
		push @errors, sprintf(_g('bad binary-only value: %s'), $v)
		    unless ($v eq 'yes');
	    } elsif ($k =~ m/^X[BCS]+-/i) {
	    } else {
		push @errors, sprintf(_g('unknown key-value %s'), $k);
	    }
	}
	my ($ok, $msg) = version_check($version);
	unless ($ok) {
	    push @errors, sprintf(_g("version '%s' is invalid: %s"), $version, $msg);
	}
    } else {
	push @errors, _g("the header doesn't match the expected regex");
    }
    return @errors;
}

sub check_trailer {
    my ($self) = @_;
    my @errors;
    if (defined($self->{trailer}) and $self->{trailer} =~ $regex_trailer) {
	if ($3 ne '  ') {
	    push @errors, _g('badly formatted trailer line');
	}
	unless (defined str2time($4)) {
	    push @errors, sprintf(_g("couldn't parse date %s"), $4);
	}
    } else {
	push @errors, _g("the trailer doesn't match the expected regex");
    }
    return @errors;
}

=item $entry->normalize()

Normalize the content. Strip whitespaces at end of lines, use a single
empty line to separate each part.

=cut

sub normalize {
    my ($self) = @_;
    $self->SUPER::normalize();
    #XXX: recreate header/trailer
}

sub get_source {
    my ($self) = @_;
    if (defined($self->{header}) and $self->{header} =~ $regex_header) {
	return $1;
    }
    return;
}

sub get_version {
    my ($self) = @_;
    if (defined($self->{header}) and $self->{header} =~ $regex_header) {
	return Dpkg::Version->new($2);
    }
    return;
}

sub get_distributions {
    my ($self) = @_;
    if (defined($self->{header}) and $self->{header} =~ $regex_header) {
	my $value = $3;
	$value =~ s/^\s+//;
	my @dists = split(/\s+/, $value);
	return @dists if wantarray;
	return $dists[0];
    }
    return;
}

sub get_optional_fields {
    my ($self) = @_;
    my $f = Dpkg::Control::Changelog->new();
    if (defined($self->{header}) and $self->{header} =~ $regex_header) {
	my $options = $4;
	$options =~ s/^\s+//;
	foreach my $opt (split(/\s*,\s*/, $options)) {
	    if ($opt =~ m/^([-0-9a-z]+)\=\s*(.*\S)$/i) {
		$f->{$1} = $2;
	    }
	}
    }
    my @closes = find_closes(join("\n", @{$self->{changes}}));
    if (@closes) {
	$f->{Closes} = join(' ', @closes);
    }
    return $f;
}

sub get_urgency {
    my ($self) = @_;
    my $f = $self->get_optional_fields();
    if (exists $f->{Urgency}) {
	$f->{Urgency} =~ s/\s.*$//;
	return lc($f->{Urgency});
    }
    return;
}

sub get_maintainer {
    my ($self) = @_;
    if (defined($self->{trailer}) and $self->{trailer} =~ $regex_trailer) {
	return "$1 <$2>";
    }
    return;
}

sub get_timestamp {
    my ($self) = @_;
    if (defined($self->{trailer}) and $self->{trailer} =~ $regex_trailer) {
	return $4;
    }
    return;
}

=back

=head1 UTILITY FUNCTIONS

=over 4

=item my $bool = match_header($line)

Checks if the line matches a valid changelog header line.

=cut

sub match_header {
    my ($line) = @_;

    return $line =~ /$regex_header/;
}

=item my $bool = match_trailer($line)

Checks if the line matches a valid changelog trailing line.

=cut

sub match_trailer {
    my ($line) = @_;

    return $line =~ /$regex_trailer/;
}

=item my @closed_bugs = find_closes($changes)

Takes one string as argument and finds "Closes: #123456, #654321" statements
as supported by the Debian Archive software in it. Returns all closed bug
numbers in an array.

=cut

sub find_closes {
    my $changes = shift;
    my %closes;

    while ($changes &&
           ($changes =~ /closes:\s*(?:bug)?\#?\s?\d+(?:,\s*(?:bug)?\#?\s?\d+)*/ig)) {
        $closes{$_} = 1 foreach($& =~ /\#?\s?(\d+)/g);
    }

    my @closes = sort { $a <=> $b } keys %closes;
    return @closes;
}

=back

=head1 CHANGES

=head2 Version 1.01

New functions: match_header(), match_trailer()
Deprecated variables: $regex_header, $regex_trailer

=head1 AUTHOR

Raphaël Hertzog <hertzog@debian.org>.

=cut

1;
