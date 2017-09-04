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

package Dpkg::Control::FieldsCore;

use strict;
use warnings;

our $VERSION = '1.00';

use Exporter qw(import);
use Dpkg::Gettext;
use Dpkg::ErrorHandling;
use Dpkg::Control::Types;
use Dpkg::Checksums;

our @EXPORT = qw(field_capitalize field_is_official field_is_allowed_in
                 field_transfer_single field_transfer_all
                 field_list_src_dep field_list_pkg_dep field_get_dep_type
                 field_get_sep_type
                 field_ordered_list field_register
                 field_insert_after field_insert_before
                 FIELD_SEP_UNKNOWN FIELD_SEP_SPACE FIELD_SEP_COMMA
                 FIELD_SEP_LINE);

use constant {
    ALL_PKG => CTRL_INFO_PKG | CTRL_INDEX_PKG | CTRL_PKG_DEB | CTRL_FILE_STATUS,
    ALL_SRC => CTRL_INFO_SRC | CTRL_INDEX_SRC | CTRL_PKG_SRC,
    ALL_CHANGES => CTRL_FILE_CHANGES | CTRL_CHANGELOG,
};

use constant {
    FIELD_SEP_UNKNOWN => 0,
    FIELD_SEP_SPACE => 1,
    FIELD_SEP_COMMA => 2,
    FIELD_SEP_LINE => 4,
};

# The canonical list of fields

# Note that fields used only in dpkg's available file are not listed
# Deprecated fields of dpkg's status file are also not listed
our %FIELDS = (
    'Architecture' => {
        allowed => (ALL_PKG | ALL_SRC | CTRL_FILE_CHANGES) & (~CTRL_INFO_SRC),
        separator => FIELD_SEP_SPACE,
    },
    'Binary' => {
        allowed => CTRL_PKG_SRC | CTRL_FILE_CHANGES,
        # XXX: This field values are separated either by space or comma
        # depending on the context.
        separator => FIELD_SEP_SPACE | FIELD_SEP_COMMA,
    },
    'Binary-Only' => {
        allowed => ALL_CHANGES,
    },
    'Breaks' => {
        allowed => ALL_PKG,
        separator => FIELD_SEP_COMMA,
        dependency => 'union',
        dep_order => 7,
    },
    'Bugs' => {
        allowed => (ALL_PKG | CTRL_INFO_SRC | CTRL_FILE_VENDOR) & (~CTRL_INFO_PKG),
    },
    'Build-Conflicts' => {
        allowed => ALL_SRC,
        separator => FIELD_SEP_COMMA,
        dependency => 'union',
        dep_order => 4,
    },
    'Build-Conflicts-Arch' => {
        allowed => ALL_SRC,
        separator => FIELD_SEP_COMMA,
        dependency => 'union',
        dep_order => 5,
    },
    'Build-Conflicts-Indep' => {
        allowed => ALL_SRC,
        separator => FIELD_SEP_COMMA,
        dependency => 'union',
        dep_order => 6,
    },
    'Build-Depends' => {
        allowed => ALL_SRC,
        separator => FIELD_SEP_COMMA,
        dependency => 'normal',
        dep_order => 1,
    },
    'Build-Depends-Arch' => {
        allowed => ALL_SRC,
        separator => FIELD_SEP_COMMA,
        dependency => 'normal',
        dep_order => 2,
    },
    'Build-Depends-Indep' => {
        allowed => ALL_SRC,
        separator => FIELD_SEP_COMMA,
        dependency => 'normal',
        dep_order => 3,
    },
    'Built-For-Profiles' => {
        allowed => ALL_PKG | CTRL_FILE_CHANGES,
        separator => FIELD_SEP_SPACE,
    },
    'Built-Using' => {
        allowed => ALL_PKG,
        separator => FIELD_SEP_COMMA,
        dependency => 'union',
        dep_order => 10,
    },
    'Changed-By' => {
        allowed => CTRL_FILE_CHANGES,
    },
    'Changes' => {
        allowed => ALL_CHANGES,
    },
    'Closes' => {
        allowed => ALL_CHANGES,
        separator => FIELD_SEP_SPACE,
    },
    'Conffiles' => {
        allowed => CTRL_FILE_STATUS,
        separator => FIELD_SEP_LINE | FIELD_SEP_SPACE,
    },
    'Config-Version' => {
        allowed => CTRL_FILE_STATUS,
    },
    'Conflicts' => {
        allowed => ALL_PKG,
        separator => FIELD_SEP_COMMA,
        dependency => 'union',
        dep_order => 6,
    },
    'Date' => {
        allowed => ALL_CHANGES,
    },
    'Depends' => {
        allowed => ALL_PKG,
        separator => FIELD_SEP_COMMA,
        dependency => 'normal',
        dep_order => 2,
    },
    'Description' => {
        allowed => ALL_PKG | CTRL_FILE_CHANGES,
    },
    'Directory' => {
        allowed => CTRL_INDEX_SRC,
    },
    'Distribution' => {
        allowed => ALL_CHANGES,
    },
    'Enhances' => {
        allowed => ALL_PKG,
        separator => FIELD_SEP_COMMA,
        dependency => 'union',
        dep_order => 5,
    },
    'Essential' => {
        allowed => ALL_PKG,
    },
    'Filename' => {
        allowed => CTRL_INDEX_PKG,
        separator => FIELD_SEP_LINE | FIELD_SEP_SPACE,
    },
    'Files' => {
        allowed => CTRL_PKG_SRC | CTRL_FILE_CHANGES,
        separator => FIELD_SEP_LINE | FIELD_SEP_SPACE,
    },
    'Format' => {
        allowed => CTRL_PKG_SRC | CTRL_FILE_CHANGES,
    },
    'Homepage' => {
        allowed => ALL_SRC | ALL_PKG,
    },
    'Installed-Size' => {
        allowed => ALL_PKG & ~CTRL_INFO_PKG,
    },
    'Installer-Menu-Item' => {
        allowed => ALL_PKG,
    },
    'Kernel-Version' => {
        allowed => ALL_PKG,
    },
    'Origin' => {
        allowed => (ALL_PKG | ALL_SRC) & (~CTRL_INFO_PKG),
    },
    'Maintainer' => {
        allowed => CTRL_PKG_DEB | ALL_SRC | ALL_CHANGES,
    },
    'Multi-Arch' => {
        allowed => ALL_PKG,
    },
    'Package' => {
        allowed => ALL_PKG,
    },
    'Package-List' => {
        allowed => ALL_SRC & ~CTRL_INFO_SRC,
        separator => FIELD_SEP_LINE | FIELD_SEP_SPACE,
    },
    'Package-Type' => {
        allowed => ALL_PKG,
    },
    'Parent' => {
        allowed => CTRL_FILE_VENDOR,
    },
    'Pre-Depends' => {
        allowed => ALL_PKG,
        separator => FIELD_SEP_COMMA,
        dependency => 'normal',
        dep_order => 1,
    },
    'Priority' => {
        allowed => CTRL_INFO_SRC | CTRL_INDEX_SRC | ALL_PKG,
    },
    'Provides' => {
        allowed => ALL_PKG,
        separator => FIELD_SEP_COMMA,
        dependency => 'union',
        dep_order => 9,
    },
    'Recommends' => {
        allowed => ALL_PKG,
        separator => FIELD_SEP_COMMA,
        dependency => 'normal',
        dep_order => 3,
    },
    'Replaces' => {
        allowed => ALL_PKG,
        separator => FIELD_SEP_COMMA,
        dependency => 'union',
        dep_order => 8,
    },
    'Section' => {
        allowed => CTRL_INFO_SRC | CTRL_INDEX_SRC | ALL_PKG,
    },
    'Size' => {
        allowed => CTRL_INDEX_PKG,
        separator => FIELD_SEP_LINE | FIELD_SEP_SPACE,
    },
    'Source' => {
        allowed => (ALL_PKG | ALL_SRC | ALL_CHANGES) &
                   (~(CTRL_INDEX_SRC | CTRL_INFO_PKG)),
    },
    'Standards-Version' => {
        allowed => ALL_SRC,
    },
    'Status' => {
        allowed => CTRL_FILE_STATUS,
        separator => FIELD_SEP_SPACE,
    },
    'Subarchitecture' => {
        allowed => ALL_PKG,
    },
    'Suggests' => {
        allowed => ALL_PKG,
        separator => FIELD_SEP_COMMA,
        dependency => 'normal',
        dep_order => 4,
    },
    'Tag' => {
        allowed => ALL_PKG,
        separator => FIELD_SEP_COMMA,
    },
    'Task' => {
        allowed => ALL_PKG,
    },
    'Triggers-Awaited' => {
        allowed => CTRL_FILE_STATUS,
        separator => FIELD_SEP_SPACE,
    },
    'Triggers-Pending' => {
        allowed => CTRL_FILE_STATUS,
        separator => FIELD_SEP_SPACE,
    },
    'Uploaders' => {
        allowed => ALL_SRC,
        separator => FIELD_SEP_COMMA,
    },
    'Urgency' => {
        allowed => ALL_CHANGES,
    },
    'Vcs-Browser' => {
        allowed => ALL_SRC,
    },
    'Vcs-Arch' => {
        allowed => ALL_SRC,
    },
    'Vcs-Bzr' => {
        allowed => ALL_SRC,
    },
    'Vcs-Cvs' => {
        allowed => ALL_SRC,
    },
    'Vcs-Darcs' => {
        allowed => ALL_SRC,
    },
    'Vcs-Git' => {
        allowed => ALL_SRC,
    },
    'Vcs-Hg' => {
        allowed => ALL_SRC,
    },
    'Vcs-Mtn' => {
        allowed => ALL_SRC,
    },
    'Vcs-Svn' => {
        allowed => ALL_SRC,
    },
    'Vendor' => {
        allowed => CTRL_FILE_VENDOR,
    },
    'Vendor-Url' => {
        allowed => CTRL_FILE_VENDOR,
    },
    'Version' => {
        allowed => (ALL_PKG | ALL_SRC | ALL_CHANGES) &
                    (~(CTRL_INFO_SRC | CTRL_INFO_PKG)),
    },
);

my @checksum_fields = map { &field_capitalize("Checksums-$_") } checksums_get_list();
my @sum_fields = map { $_ eq 'md5' ? 'MD5sum' : &field_capitalize($_) }
                 checksums_get_list();
&field_register($_, CTRL_PKG_SRC | CTRL_FILE_CHANGES) foreach @checksum_fields;
&field_register($_, CTRL_INDEX_PKG,
                separator => FIELD_SEP_LINE | FIELD_SEP_SPACE) foreach @sum_fields;

our %FIELD_ORDER = (
    CTRL_PKG_DEB() => [
        qw(Package Package-Type Source Version Built-Using Kernel-Version
        Built-For-Profiles Architecture Subarchitecture
        Installer-Menu-Item Essential Origin Bugs
        Maintainer Installed-Size), &field_list_pkg_dep(),
        qw(Section Priority Multi-Arch Homepage Description Tag Task)
    ],
    CTRL_PKG_SRC() => [
        qw(Format Source Binary Architecture Version Origin Maintainer
        Uploaders Homepage Standards-Version Vcs-Browser
        Vcs-Arch Vcs-Bzr Vcs-Cvs Vcs-Darcs Vcs-Git Vcs-Hg Vcs-Mtn
        Vcs-Svn), &field_list_src_dep(), qw(Package-List),
        @checksum_fields, qw(Files)
    ],
    CTRL_FILE_CHANGES() => [
        qw(Format Date Source Binary Binary-Only Built-For-Profiles Architecture
        Version Distribution Urgency Maintainer Changed-By Description
        Closes Changes),
        @checksum_fields, qw(Files)
    ],
    CTRL_CHANGELOG() => [
        qw(Source Binary-Only Version Distribution Urgency Maintainer
        Date Closes Changes)
    ],
    CTRL_FILE_STATUS() => [ # Same as fieldinfos in lib/dpkg/parse.c
        qw(Package Essential Status Priority Section Installed-Size Origin
        Maintainer Bugs Architecture Multi-Arch Source Version Config-Version
        Replaces Provides Depends Pre-Depends Recommends Suggests Breaks
        Conflicts Enhances Conffiles Description Triggers-Pending
        Triggers-Awaited)
    ],
);
# Order for CTRL_INDEX_PKG is derived from CTRL_PKG_DEB
$FIELD_ORDER{CTRL_INDEX_PKG()} = [ @{$FIELD_ORDER{CTRL_PKG_DEB()}} ];
&field_insert_before(CTRL_INDEX_PKG, 'Section', 'Filename', 'Size', @sum_fields);
# Order for CTRL_INDEX_SRC is derived from CTRL_PKG_SRC
$FIELD_ORDER{CTRL_INDEX_SRC()} = [ @{$FIELD_ORDER{CTRL_PKG_SRC()}} ];
@{$FIELD_ORDER{CTRL_INDEX_SRC()}} = map { $_ eq 'Source' ? 'Package' : $_ }
                                  @{$FIELD_ORDER{CTRL_PKG_SRC()}};
&field_insert_after(CTRL_INDEX_SRC, 'Version', 'Priority', 'Section');
&field_insert_before(CTRL_INDEX_SRC, 'Checksums-Md5', 'Directory');

=encoding utf8

=head1 NAME

Dpkg::Control::FieldsCore - manage (list of official) control fields

=head1 DESCRIPTION

The modules contains a list of fieldnames with associated meta-data explaining
in which type of control information they are allowed. The types are the
CTRL_* constants exported by Dpkg::Control.

=head1 FUNCTIONS

=over 4

=item my $f = field_capitalize($field_name)

Returns the field name properly capitalized. All characters are lowercase,
except the first of each word (words are separated by a hyphen in field names).

=cut

sub field_capitalize($) {
    my $field = lc(shift);
    # Some special cases due to history
    return 'MD5sum' if $field eq 'md5sum';
    return uc($field) if checksums_is_supported($field);
    # Generic case
    return join '-', map { ucfirst } split /-/, $field;
}

=item field_is_official($fname)

Returns true if the field is official and known.

=cut

sub field_is_official($) {
    return exists $FIELDS{field_capitalize($_[0])};
}

=item field_is_allowed_in($fname, @types)

Returns true (1) if the field $fname is allowed in all the types listed in
the list. Note that you can use type sets instead of individual types (ex:
CTRL_FILE_CHANGES | CTRL_CHANGELOG).

field_allowed_in(A|B, C) returns true only if the field is allowed in C
and either A or B.

Undef is returned for non-official fields.

=cut

sub field_is_allowed_in($@) {
    my ($field, @types) = @_;
    $field = field_capitalize($field);
    return unless field_is_official($field);

    return 0 if not scalar(@types);
    foreach my $type (@types) {
        next if $type == CTRL_UNKNOWN; # Always allowed
        return 0 unless $FIELDS{$field}{allowed} & $type;
    }
    return 1;
}

=item field_transfer_single($from, $to, $field)

If appropriate, copy the value of the field named $field taken from the
$from Dpkg::Control object to the $to Dpkg::Control object.

Official fields are copied only if the field is allowed in both types of
objects. Custom fields are treated in a specific manner. When the target
is not among CTRL_PKG_SRC, CTRL_PKG_DEB or CTRL_FILE_CHANGES, then they
are alway copied as is (the X- prefix is kept). Otherwise they are not
copied except if the target object matches the target destination encoded
in the field name. The initial X denoting custom fields can be followed by
one or more letters among "S" (Source: corresponds to CTRL_PKG_SRC), "B"
(Binary: corresponds to CTRL_PKG_DEB) or "C" (Changes: corresponds to
CTRL_FILE_CHANGES).

Returns undef if nothing has been copied or the name of the new field
added to $to otherwise.

=cut

sub field_transfer_single($$;$) {
    my ($from, $to, $field) = @_;
    $field //= $_;
    my ($from_type, $to_type) = ($from->get_type(), $to->get_type());
    $field = field_capitalize($field);

    if (field_is_allowed_in($field, $from_type, $to_type)) {
        $to->{$field} = $from->{$field};
        return $field;
    } elsif ($field =~ /^X([SBC]*)-/i) {
        my $dest = $1;
        if (($dest =~ /B/i and $to_type == CTRL_PKG_DEB) or
            ($dest =~ /S/i and $to_type == CTRL_PKG_SRC) or
            ($dest =~ /C/i and $to_type == CTRL_FILE_CHANGES))
        {
            my $new = $field;
            $new =~ s/^X([SBC]*)-//i;
            $to->{$new} = $from->{$field};
            return $new;
        } elsif ($to_type != CTRL_PKG_DEB and
		 $to_type != CTRL_PKG_SRC and
		 $to_type != CTRL_FILE_CHANGES)
	{
	    $to->{$field} = $from->{$field};
	    return $field;
	}
    } elsif (not field_is_allowed_in($field, $from_type)) {
        warning(_g("unknown information field '%s' in input data in %s"),
                $field, $from->get_option('name') || _g('control information'));
    }
    return;
}

=item field_transfer_all($from, $to)

Transfer all appropriate fields from $from to $to. Calls
field_transfer_single() on all fields available in $from.

Returns the list of fields that have been added to $to.

=cut

sub field_transfer_all($$) {
    my ($from, $to) = @_;
    my (@res, $res);
    foreach my $k (keys %$from) {
        $res = field_transfer_single($from, $to, $k);
        push @res, $res if $res and defined wantarray;
    }
    return @res;
}

=item field_ordered_list($type)

Returns an ordered list of fields for a given type of control information.
This list can be used to output the fields in a predictable order.
The list might be empty for types where the order does not matter much.

=cut

sub field_ordered_list($) {
    my ($type) = @_;
    return @{$FIELD_ORDER{$type}} if exists $FIELD_ORDER{$type};
    return ();
}

=item field_list_src_dep()

List of fields that contains dependencies-like information in a source
Debian package.

=cut

sub field_list_src_dep() {
    my @list = sort {
        $FIELDS{$a}{dep_order} <=> $FIELDS{$b}{dep_order}
    } grep {
        field_is_allowed_in($_, CTRL_PKG_SRC) and
        exists $FIELDS{$_}{dependency}
    } keys %FIELDS;
    return @list;
}

=item field_list_pkg_dep()

List of fields that contains dependencies-like information in a binary
Debian package. The fields that express real dependencies are sorted from
the stronger to the weaker.

=cut

sub field_list_pkg_dep() {
    my @keys = keys %FIELDS;
    my @list = sort {
        $FIELDS{$a}{dep_order} <=> $FIELDS{$b}{dep_order}
    } grep {
        field_is_allowed_in($_, CTRL_PKG_DEB) and
        exists $FIELDS{$_}{dependency}
    } @keys;
    return @list;
}

=item field_get_dep_type($field)

Return the type of the dependency expressed by the given field. Can
either be "normal" for a real dependency field (Pre-Depends, Depends, ...)
or "union" for other relation fields sharing the same syntax (Conflicts,
Breaks, ...). Returns undef for fields which are not dependencies.

=cut

sub field_get_dep_type($) {
    my $field = field_capitalize($_[0]);
    return unless field_is_official($field);
    return $FIELDS{$field}{dependency} if exists $FIELDS{$field}{dependency};
    return;
}

=item field_get_sep_type($field)

Return the type of the field value separator. Can be one of FIELD_SEP_UNKNOWN,
FIELD_SEP_SPACE, FIELD_SEP_COMMA or FIELD_SEP_LINE.

=cut

sub field_get_sep_type($) {
    my $field = field_capitalize($_[0]);

    return $FIELDS{$field}{separator} if exists $FIELDS{$field}{separator};
    return FIELD_SEP_UNKNOWN;
}

=item field_register($field, $allowed_types, %opts)

Register a new field as being allowed in control information of specified
types. %opts is optional

=cut

sub field_register($$;@) {
    my ($field, $types, %opts) = @_;
    $field = field_capitalize($field);
    $FIELDS{$field} = {
        allowed => $types,
        %opts
    };
}

=item field_insert_after($type, $ref, @fields)

Place field after another one ($ref) in output of control information of
type $type.

=cut
sub field_insert_after($$@) {
    my ($type, $field, @fields) = @_;
    return 0 if not exists $FIELD_ORDER{$type};
    ($field, @fields) = map { field_capitalize($_) } ($field, @fields);
    @{$FIELD_ORDER{$type}} = map {
        ($_ eq $field) ? ($_, @fields) : $_
    } @{$FIELD_ORDER{$type}};
    return 1;
}

=item field_insert_before($type, $ref, @fields)

Place field before another one ($ref) in output of control information of
type $type.

=cut
sub field_insert_before($$@) {
    my ($type, $field, @fields) = @_;
    return 0 if not exists $FIELD_ORDER{$type};
    ($field, @fields) = map { field_capitalize($_) } ($field, @fields);
    @{$FIELD_ORDER{$type}} = map {
        ($_ eq $field) ? (@fields, $_) : $_
    } @{$FIELD_ORDER{$type}};
    return 1;
}

=back

=head1 AUTHOR

Raphaël Hertzog <hertzog@debian.org>.

=cut

1;
