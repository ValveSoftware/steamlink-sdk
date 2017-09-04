# Copyright © 2007-2009 Raphaël Hertzog <hertzog@debian.org>
# Copyright © 2012 Guillem Jover <guillem@debian.org>
#
# This program is free software; you may redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#########################################################################
# Several parts are inspired by lib/Dep.pm from lintian (same license)
#
# Copyright © 1998 Richard Braakman
# Portions Copyright © 1999 Darren Benham
# Portions Copyright © 2000 Sean 'Shaleh' Perry
# Portions Copyright © 2004 Frank Lichtenheld
# Portions Copyright © 2006 Russ Allbery

package Dpkg::Deps;

=encoding utf8

=head1 NAME

Dpkg::Deps - parse and manipulate dependencies of Debian packages

=head1 DESCRIPTION

The Dpkg::Deps module provides objects implementing various types of
dependencies.

The most important function is deps_parse(), it turns a dependency line in
a set of Dpkg::Deps::{Simple,AND,OR,Union} objects depending on the case.

=head1 FUNCTIONS

All the deps_* functions are exported by default.

=over 4

=cut

use strict;
use warnings;

our $VERSION = '1.02';

use Dpkg::Version;
use Dpkg::Arch qw(get_host_arch get_build_arch);
use Dpkg::BuildProfiles qw(get_build_profiles);
use Dpkg::ErrorHandling;
use Dpkg::Gettext;

use Exporter qw(import);
our @EXPORT = qw(deps_concat deps_parse deps_eval_implication deps_compare);

=item deps_eval_implication($rel_p, $v_p, $rel_q, $v_q)

($rel_p, $v_p) and ($rel_q, $v_q) express two dependencies as (relation,
version). The relation variable can have the following values that are
exported by Dpkg::Version: REL_EQ, REL_LT, REL_LE, REL_GT, REL_GT.

This functions returns 1 if the "p" dependency implies the "q"
dependency. It returns 0 if the "p" dependency implies that "q" is
not satisfied. It returns undef when there's no implication.

The $v_p and $v_q parameter should be Dpkg::Version objects.

=cut

sub deps_eval_implication {
    my ($rel_p, $v_p, $rel_q, $v_q) = @_;

    # If versions are not valid, we can't decide of any implication
    return unless defined($v_p) and $v_p->is_valid();
    return unless defined($v_q) and $v_q->is_valid();

    # q wants an exact version, so p must provide that exact version.  p
    # disproves q if q's version is outside the range enforced by p.
    if ($rel_q eq REL_EQ) {
        if ($rel_p eq REL_LT) {
            return ($v_p <= $v_q) ? 0 : undef;
        } elsif ($rel_p eq REL_LE) {
            return ($v_p < $v_q) ? 0 : undef;
        } elsif ($rel_p eq REL_GT) {
            return ($v_p >= $v_q) ? 0 : undef;
        } elsif ($rel_p eq REL_GE) {
            return ($v_p > $v_q) ? 0 : undef;
        } elsif ($rel_p eq REL_EQ) {
            return ($v_p == $v_q);
        }
    }

    # A greater than clause may disprove a less than clause. An equal
    # cause might as well.  Otherwise, if
    # p's clause is <<, <=, or =, the version must be <= q's to imply q.
    if ($rel_q eq REL_LE) {
        if ($rel_p eq REL_GT) {
            return ($v_p >= $v_q) ? 0 : undef;
        } elsif ($rel_p eq REL_GE) {
            return ($v_p > $v_q) ? 0 : undef;
	} elsif ($rel_p eq REL_EQ) {
            return ($v_p <= $v_q) ? 1 : 0;
        } else { # <<, <=
            return ($v_p <= $v_q) ? 1 : undef;
        }
    }

    # Similar, but << is stronger than <= so p's version must be << q's
    # version if the p relation is <= or =.
    if ($rel_q eq REL_LT) {
        if ($rel_p eq REL_GT or $rel_p eq REL_GE) {
            return ($v_p >= $v_p) ? 0 : undef;
        } elsif ($rel_p eq REL_LT) {
            return ($v_p <= $v_q) ? 1 : undef;
	} elsif ($rel_p eq REL_EQ) {
            return ($v_p < $v_q) ? 1 : 0;
        } else { # <<, <=
            return ($v_p < $v_q) ? 1 : undef;
        }
    }

    # Same logic as above, only inverted.
    if ($rel_q eq REL_GE) {
        if ($rel_p eq REL_LT) {
            return ($v_p <= $v_q) ? 0 : undef;
        } elsif ($rel_p eq REL_LE) {
            return ($v_p < $v_q) ? 0 : undef;
	} elsif ($rel_p eq REL_EQ) {
            return ($v_p >= $v_q) ? 1 : 0;
        } else { # >>, >=
            return ($v_p >= $v_q) ? 1 : undef;
        }
    }
    if ($rel_q eq REL_GT) {
        if ($rel_p eq REL_LT or $rel_p eq REL_LE) {
            return ($v_p <= $v_q) ? 0 : undef;
        } elsif ($rel_p eq REL_GT) {
            return ($v_p >= $v_q) ? 1 : undef;
	} elsif ($rel_p eq REL_EQ) {
            return ($v_p > $v_q) ? 1 : 0;
        } else {
            return ($v_p > $v_q) ? 1 : undef;
        }
    }

    return;
}

=item my $dep = deps_concat(@dep_list)

This function concatenates multiple dependency lines into a single line,
joining them with ", " if appropriate, and always returning a valid string.

=cut

sub deps_concat {
    my (@dep_list) = @_;

    return join(', ', grep { defined $_ } @dep_list);
}

=item my $dep = deps_parse($line, %options)

This function parses the dependency line and returns an object, either a
Dpkg::Deps::AND or a Dpkg::Deps::Union. Various options can alter the
behaviour of that function.

=over 4

=item use_arch (defaults to 1)

Take into account the architecture restriction part of the dependencies.
Set to 0 to completely ignore that information.

=item host_arch (defaults to the current architecture)

Define the host architecture. By default it uses
Dpkg::Arch::get_host_arch() to identify the proper architecture.

=item build_arch (defaults to the current architecture)

Define the build architecture. By default it uses
Dpkg::Arch::get_build_arch() to identify the proper architecture.

=item reduce_arch (defaults to 0)

If set to 1, ignore dependencies that do not concern the current host
architecture. This implicitely strips off the architecture restriction
list so that the resulting dependencies are directly applicable to the
current architecture.

=item use_profiles (defaults to 1)

Take into account the profile restriction part of the dependencies. Set
to 0 to completely ignore that information.

=item build_profiles (defaults to no profile)

Define the active build profiles. By default no profile is defined.

=item reduce_profiles (defaults to 0)

If set to 1, ignore dependencies that do not concern the current build
profile. This implicitly strips off the profile restriction list so
that the resulting dependencies are directly applicable to the current
profiles.

=item reduce_restrictions (defaults to 0)

If set to 1, ignore dependencies that do not concern the current set of
restrictions. This implicitly strips off any restriction list so that the
resulting dependencies are directly applicable to the current restriction.
This currently implies C<reduce_arch> and C<reduce_profiles>, and overrides
them if set.

=item union (defaults to 0)

If set to 1, returns a Dpkg::Deps::Union instead of a Dpkg::Deps::AND. Use
this when parsing non-dependency fields like Conflicts.

=item build_dep (defaults to 0)

If set to 1, allow build-dep only arch qualifiers, that is “:native”.
This should be set whenever working with build-deps.

=back

=cut

sub deps_parse {
    my $dep_line = shift;
    my %options = (@_);
    $options{use_arch} = 1 if not exists $options{use_arch};
    $options{reduce_arch} = 0 if not exists $options{reduce_arch};
    $options{host_arch} = get_host_arch() if not exists $options{host_arch};
    $options{build_arch} = get_build_arch() if not exists $options{build_arch};
    $options{use_profiles} = 1 if not exists $options{use_profiles};
    $options{reduce_profiles} = 0 if not exists $options{reduce_profiles};
    $options{build_profiles} = [ get_build_profiles() ]
        if not exists $options{build_profiles};
    $options{reduce_restrictions} = 0 if not exists $options{reduce_restrictions};
    $options{union} = 0 if not exists $options{union};
    $options{build_dep} = 0 if not exists $options{build_dep};

    if ($options{reduce_restrictions}) {
        $options{reduce_arch} = 1;
        $options{reduce_profiles} = 1;
    }

    # Strip trailing/leading spaces
    $dep_line =~ s/^\s+//;
    $dep_line =~ s/\s+$//;

    my @dep_list;
    foreach my $dep_and (split(/\s*,\s*/m, $dep_line)) {
        my @or_list = ();
        foreach my $dep_or (split(/\s*\|\s*/m, $dep_and)) {
	    my $dep_simple = Dpkg::Deps::Simple->new($dep_or, host_arch =>
	                                             $options{host_arch},
	                                             build_arch =>
	                                             $options{build_arch},
	                                             build_dep =>
	                                             $options{build_dep});
	    if (not defined $dep_simple->{package}) {
		warning(_g("can't parse dependency %s"), $dep_or);
		return;
	    }
	    $dep_simple->{arches} = undef if not $options{use_arch};
            if ($options{reduce_arch}) {
		$dep_simple->reduce_arch($options{host_arch});
		next if not $dep_simple->arch_is_concerned($options{host_arch});
	    }
	    $dep_simple->{restrictions} = undef if not $options{use_profiles};
	    if ($options{reduce_profiles}) {
		$dep_simple->reduce_profiles($options{build_profiles});
		next if not $dep_simple->profile_is_concerned($options{build_profiles});
	    }
	    push @or_list, $dep_simple;
        }
	next if not @or_list;
	if (scalar @or_list == 1) {
	    push @dep_list, $or_list[0];
	} else {
	    my $dep_or = Dpkg::Deps::OR->new();
	    $dep_or->add($_) foreach (@or_list);
	    push @dep_list, $dep_or;
	}
    }
    my $dep_and;
    if ($options{union}) {
	$dep_and = Dpkg::Deps::Union->new();
    } else {
	$dep_and = Dpkg::Deps::AND->new();
    }
    foreach my $dep (@dep_list) {
        if ($options{union} and not $dep->isa('Dpkg::Deps::Simple')) {
            warning(_g('an union dependency can only contain simple dependencies'));
            return;
        }
        $dep_and->add($dep);
    }
    return $dep_and;
}

=item deps_compare($a, $b)

Implements a comparison operator between two dependency objects.
This function is mainly used to implement the sort() method.

=back

=cut

my %relation_ordering = (
	undef => 0,
	REL_GE() => 1,
	REL_GT() => 2,
	REL_EQ() => 3,
	REL_LT() => 4,
	REL_LE() => 5,
);

sub deps_compare {
    my ($a, $b) = @_;
    return -1 if $a->is_empty();
    return 1 if $b->is_empty();
    while ($a->isa('Dpkg::Deps::Multiple')) {
	return -1 if $a->is_empty();
	my @deps = $a->get_deps();
	$a = $deps[0];
    }
    while ($b->isa('Dpkg::Deps::Multiple')) {
	return 1 if $b->is_empty();
	my @deps = $b->get_deps();
	$b = $deps[0];
    }
    my $ar = defined($a->{relation}) ? $a->{relation} : 'undef';
    my $br = defined($b->{relation}) ? $b->{relation} : 'undef';
    return (($a->{package} cmp $b->{package}) ||
	    ($relation_ordering{$ar} <=> $relation_ordering{$br}) ||
	    ($a->{version} cmp $b->{version}));
}


package Dpkg::Deps::Simple;

=head1 OBJECTS - Dpkg::Deps::*

There are several kind of dependencies. A Dpkg::Deps::Simple dependency
represents a single dependency statement (it relates to one package only).
Dpkg::Deps::Multiple dependencies are built on top of this object
and combine several dependencies in a different manners. Dpkg::Deps::AND
represents the logical "AND" between dependencies while Dpkg::Deps::OR
represents the logical "OR". Dpkg::Deps::Multiple objects can contain
Dpkg::Deps::Simple object as well as other Dpkg::Deps::Multiple objects.

In practice, the code is only meant to handle the realistic cases which,
given Debian's dependencies structure, imply those restrictions: AND can
contain Simple or OR objects, OR can only contain Simple objects.

Dpkg::Deps::KnownFacts is a special object that is used while evaluating
dependencies and while trying to simplify them. It represents a set of
installed packages along with the virtual packages that they might
provide.

=head2 COMMON FUNCTIONS

=over 4

=item $dep->is_empty()

Returns true if the dependency is empty and doesn't contain any useful
information. This is true when a Dpkg::Deps::Simple object has not yet
been initialized or when a (descendant of) Dpkg::Deps::Multiple contains
an empty list of dependencies.

=item $dep->get_deps()

Returns a list of sub-dependencies. For Dpkg::Deps::Simple it returns
itself.

=item $dep->output([$fh])

=item "$dep"

Returns a string representing the dependency. If $fh is set, it prints
the string to the filehandle.

=item $dep->implies($other_dep)

Returns 1 when $dep implies $other_dep. Returns 0 when $dep implies
NOT($other_dep). Returns undef when there's no implication. $dep and
$other_dep do not need to be of the same type.

=item $dep->sort()

Sorts alphabetically the internal list of dependencies. It's a no-op for
Dpkg::Deps::Simple objects.

=item $dep->arch_is_concerned($arch)

Returns true if the dependency applies to the indicated architecture. For
multiple dependencies, it returns true if at least one of the
sub-dependencies apply to this architecture.

=item $dep->reduce_arch($arch)

Simplifies the dependency to contain only information relevant to the given
architecture. A Dpkg::Deps::Simple object can be left empty after this
operation. For Dpkg::Deps::Multiple objects, the non-relevant
sub-dependencies are simply removed.

This trims off the architecture restriction list of Dpkg::Deps::Simple
objects.

=item $dep->get_evaluation($facts)

Evaluates the dependency given a list of installed packages and a list of
virtual packages provided. Those lists are part of the
Dpkg::Deps::KnownFacts object given as parameters.

Returns 1 when it's true, 0 when it's false, undef when some information
is lacking to conclude.

=item $dep->simplify_deps($facts, @assumed_deps)

Simplifies the dependency as much as possible given the list of facts (see
object Dpkg::Deps::KnownFacts) and a list of other dependencies that are
known to be true.

=item $dep->has_arch_restriction()

For a simple dependency, returns the package name if the dependency
applies only to a subset of architectures.  For multiple dependencies, it
returns the list of package names that have such a restriction.

=item $dep->reset()

Clears any dependency information stored in $dep so that $dep->is_empty()
returns true.

=back

=head2 Dpkg::Deps::Simple

Such an object has four interesting properties:

=over 4

=item package

The package name (can be undef if the dependency has not been initialized
or if the simplification of the dependency lead to its removal).

=item relation

The relational operator: "=", "<<", "<=", ">=" or ">>". It can be
undefined if the dependency had no version restriction. In that case the
following field is also undefined.

=item version

The version.

=item arches

The list of architectures where this dependency is applicable. It's
undefined when there's no restriction, otherwise it's an
array ref. It can contain an exclusion list, in that case each
architecture is prefixed with an exclamation mark.

=item archqual

The arch qualifier of the dependency (can be undef if there's none).
In the dependency "python:any (>= 2.6)", the arch qualifier is "any".

=back

=head3 METHODS

=over 4

=item $simple_dep->parse_string('dpkg-dev (>= 1.14.8) [!hurd-i386]')

Parses the dependency and modifies internal properties to match the parsed
dependency.

=item $simple_dep->merge_union($other_dep)

Returns true if $simple_dep could be modified to represent the union of
both dependencies. Otherwise returns false.

=back

=cut

use strict;
use warnings;

use Carp;

use Dpkg::Arch qw(debarch_is);
use Dpkg::Version;
use Dpkg::ErrorHandling;
use Dpkg::Gettext;
use Dpkg::Util qw(:list);

use parent qw(Dpkg::Interface::Storable);

sub new {
    my ($this, $arg, %opts) = @_;
    my $class = ref($this) || $this;
    my $self = {};
    bless $self, $class;
    $self->reset();
    $self->{host_arch} = $opts{host_arch} || Dpkg::Arch::get_host_arch();
    $self->{build_arch} = $opts{build_arch} || Dpkg::Arch::get_build_arch();
    $self->{build_dep} = $opts{build_dep} || 0;
    $self->parse_string($arg) if defined($arg);
    return $self;
}

sub reset {
    my ($self) = @_;
    $self->{package} = undef;
    $self->{relation} = undef;
    $self->{version} = undef;
    $self->{arches} = undef;
    $self->{archqual} = undef;
    $self->{restrictions} = undef;
}

sub parse {
    my ($self, $fh, $desc) = @_;
    my $line = <$fh>;
    chomp($line);
    return $self->parse_string($line);
}

sub parse_string {
    my ($self, $dep) = @_;
    return if not $dep =~
           m{^\s*                           # skip leading whitespace
              ([a-zA-Z0-9][a-zA-Z0-9+.-]*)  # package name
              (?:                           # start of optional part
                :                           # colon for architecture
                ([a-zA-Z0-9][a-zA-Z0-9-]*)  # architecture name
              )?                            # end of optional part
              (?:                           # start of optional part
                \s* \(                      # open parenthesis for version part
                \s* (<<|<=|=|>=|>>|<|>)     # relation part
                \s* (.*?)                   # do not attempt to parse version
                \s* \)                      # closing parenthesis
              )?                            # end of optional part
              (?:                           # start of optional architecture
                \s* \[                      # open bracket for architecture
                \s* (.*?)                   # don't parse architectures now
                \s* \]                      # closing bracket
              )?                            # end of optional architecture
              (?:                           # start of optional restriction
                \s* <                       # open bracket for restriction
                \s* (.*?)                   # don't parse restrictions now
                \s* >                       # closing bracket
              )?                            # end of optional restriction
              \s*$                          # trailing spaces at end
            }x;
    if (defined($2)) {
	return if $2 eq 'native' and not $self->{build_dep};
	$self->{archqual} = $2;
    }
    $self->{package} = $1;
    $self->{relation} = version_normalize_relation($3) if defined($3);
    if (defined($4)) {
	$self->{version} = Dpkg::Version->new($4);
    }
    if (defined($5)) {
	$self->{arches} = [ split(/\s+/, $5) ];
    }
    if (defined($6)) {
	$self->{restrictions} = [ map { lc } split /\s+/, $6 ];
    }
}

sub output {
    my ($self, $fh) = @_;
    my $res = $self->{package};
    if (defined($self->{archqual})) {
	$res .= ':' . $self->{archqual};
    }
    if (defined($self->{relation})) {
	$res .= ' (' . $self->{relation} . ' ' . $self->{version} .  ')';
    }
    if (defined($self->{arches})) {
	$res .= ' [' . join(' ', @{$self->{arches}}) . ']';
    }
    if (defined($self->{restrictions})) {
	$res .= ' <' . join(' ', @{$self->{restrictions}}) . '>';
    }
    if (defined($fh)) {
	print { $fh } $res;
    }
    return $res;
}

# _arch_is_superset(\@p, \@q)
#
# Returns true if the arch list @p is a superset of arch list @q.
# The arguments can also be undef in case there's no explicit architecture
# restriction.
sub _arch_is_superset {
    my ($p, $q) = @_;
    my $p_arch_neg = defined($p) && $p->[0] =~ /^!/;
    my $q_arch_neg = defined($q) && $q->[0] =~ /^!/;

    # If "p" has no arches, it is a superset of q and we should fall through
    # to the version check.
    if (not defined $p) {
	return 1;
    }

    # If q has no arches, it is a superset of p and there are no useful
    # implications.
    elsif (not defined $q) {
	return 0;
    }

    # Both have arches.  If neither are negated, we know nothing useful
    # unless q is a subset of p.
    elsif (not $p_arch_neg and not $q_arch_neg) {
	my %p_arches = map { $_ => 1 } @{$p};
	my $subset = 1;
	for my $arch (@{$q}) {
	    $subset = 0 unless $p_arches{$arch};
	}
	return 0 unless $subset;
    }

    # If both are negated, we know nothing useful unless p is a subset of
    # q (and therefore has fewer things excluded, and therefore is more
    # general).
    elsif ($p_arch_neg and $q_arch_neg) {
	my %q_arches = map { $_ => 1 } @{$q};
	my $subset = 1;
	for my $arch (@{$p}) {
	    $subset = 0 unless $q_arches{$arch};
	}
	return 0 unless $subset;
    }

    # If q is negated and p isn't, we'd need to know the full list of
    # arches to know if there's any relationship, so bail.
    elsif (not $p_arch_neg and $q_arch_neg) {
	return 0;
    }

    # If p is negated and q isn't, q is a subset of p if none of the
    # negated arches in p are present in q.
    elsif ($p_arch_neg and not $q_arch_neg) {
	my %q_arches = map { $_ => 1 } @{$q};
	my $subset = 1;
	for my $arch (@{$p}) {
	    $subset = 0 if $q_arches{substr($arch, 1)};
	}
	return 0 unless $subset;
    }
    return 1;
}

# _arch_qualifier_allows_implication($p, $q)
#
# Returns true if the arch qualifier $p and $q are compatible with the
# implication $p -> $q, false otherwise. $p/$q can be
# undef/"any"/"native" or an architecture string.
sub _arch_qualifier_allows_implication {
    my ($p, $q) = @_;
    if (defined $p and $p eq 'any') {
	return 1 if defined $q and $q eq 'any';
	return 0;
    } elsif (defined $p and $p eq 'native') {
	return 1 if defined $q and ($q eq 'any' or $q eq 'native');
	return 0;
    } elsif (defined $p) {
	return 1 if defined $q and ($p eq $q or $q eq 'any');
	return 0;
    } else {
	return 0 if defined $q and $q ne 'any' and $q ne 'native';
	return 1;
    }
}

# Returns true if the dependency in parameter can deduced from the current
# dependency. Returns false if it can be negated. Returns undef if nothing
# can be concluded.
sub implies {
    my ($self, $o) = @_;
    if ($o->isa('Dpkg::Deps::Simple')) {
	# An implication is only possible on the same package
	return if $self->{package} ne $o->{package};

	# Our architecture set must be a superset of the architectures for
	# o, otherwise we can't conclude anything.
	return unless _arch_is_superset($self->{arches}, $o->{arches});

	# The arch qualifier must not forbid an implication
	return unless _arch_qualifier_allows_implication($self->{archqual},
	                                                 $o->{archqual});

	# If o has no version clause, then our dependency is stronger
	return 1 if not defined $o->{relation};
	# If o has a version clause, we must also have one, otherwise there
	# can't be an implication
	return if not defined $self->{relation};

	return Dpkg::Deps::deps_eval_implication($self->{relation},
		$self->{version}, $o->{relation}, $o->{version});

    } elsif ($o->isa('Dpkg::Deps::AND')) {
	# TRUE: Need to imply all individual elements
	# FALSE: Need to NOT imply at least one individual element
	my $res = 1;
	foreach my $dep ($o->get_deps()) {
	    my $implication = $self->implies($dep);
	    unless (defined($implication) && $implication == 1) {
		$res = $implication;
		last if defined $res;
	    }
	}
	return $res;
    } elsif ($o->isa('Dpkg::Deps::OR')) {
	# TRUE: Need to imply at least one individual element
	# FALSE: Need to not apply all individual elements
	# UNDEF: The rest
	my $res = undef;
	foreach my $dep ($o->get_deps()) {
	    my $implication = $self->implies($dep);
	    if (defined($implication)) {
		if (not defined $res) {
		    $res = $implication;
		} else {
		    if ($implication) {
			$res = 1;
		    } else {
			$res = 0;
		    }
		}
		last if defined($res) && $res == 1;
	    }
	}
	return $res;
    } else {
	croak 'Dpkg::Deps::Simple cannot evaluate implication with a ' .
	      ref($o);
    }
}

sub get_deps {
    my $self = shift;
    return $self;
}

sub sort {
    # Nothing to sort
}

sub arch_is_concerned {
    my ($self, $host_arch) = @_;

    return 0 if not defined $self->{package}; # Empty dep
    return 1 if not defined $self->{arches};  # Dep without arch spec

    my $seen_arch = 0;
    foreach my $arch (@{$self->{arches}}) {
	$arch=lc($arch);

	if ($arch =~ /^!/) {
	    my $not_arch = $arch;
	    $not_arch =~ s/^!//;

	    if (debarch_is($host_arch, $not_arch)) {
		$seen_arch = 0;
		last;
	    } else {
		# !arch includes by default all other arches
		# unless they also appear in a !otherarch
		$seen_arch = 1;
	    }
	} elsif (debarch_is($host_arch, $arch)) {
	    $seen_arch = 1;
	    last;
	}
    }
    return $seen_arch;
}

sub reduce_arch {
    my ($self, $host_arch) = @_;
    if (not $self->arch_is_concerned($host_arch)) {
	$self->reset();
    } else {
	$self->{arches} = undef;
    }
}

sub has_arch_restriction {
    my ($self) = @_;
    if (defined $self->{arches}) {
	return $self->{package};
    } else {
	return ();
    }
}

sub profile_is_concerned {
    my ($self, $build_profiles) = @_;

    return 0 if not defined $self->{package}; # Empty dep
    return 1 if not defined $self->{restrictions}; # Dep without restrictions

    my $seen_profile = 0;
    foreach my $restriction (@{$self->{restrictions}}) {
        # Determine if this restriction is negated, and within the "profile"
        # namespace, otherwise it does not concern this check.
        next if $restriction !~ m/^(!)?profile\.(.*)/;

        my $negated = defined $1 && $1 eq '!';
        my $profile = $2;

        # Determine if the restriction matches any of the specified profiles.
        my $found = any { $_ eq $profile } @{$build_profiles};

        if ($negated) {
            if ($found) {
                $seen_profile = 0;
                last;
            } else {
                # "!profile.this" includes by default all other profiles
                # unless they also appear in a "!profile.other".
                $seen_profile = 1;
            }
        } elsif ($found) {
            $seen_profile = 1;
            last;
        }
    }
    return $seen_profile;
}

sub reduce_profiles {
    my ($self, $build_profiles) = @_;

    if (not $self->profile_is_concerned($build_profiles)) {
        $self->reset();
    } else {
        $self->{restrictions} = undef;
    }
}

sub get_evaluation {
    my ($self, $facts) = @_;
    return if not defined $self->{package};
    return $facts->_evaluate_simple_dep($self);
}

sub simplify_deps {
    my ($self, $facts) = @_;
    my $eval = $self->get_evaluation($facts);
    $self->reset() if defined $eval and $eval == 1;
}

sub is_empty {
    my $self = shift;
    return not defined $self->{package};
}

sub merge_union {
    my ($self, $o) = @_;
    return 0 if not $o->isa('Dpkg::Deps::Simple');
    return 0 if $self->is_empty() or $o->is_empty();
    return 0 if $self->{package} ne $o->{package};
    return 0 if defined $self->{arches} or defined $o->{arches};

    if (not defined $o->{relation} and defined $self->{relation}) {
	# Union is the non-versioned dependency
	$self->{relation} = undef;
	$self->{version} = undef;
	return 1;
    }

    my $implication = $self->implies($o);
    my $rev_implication = $o->implies($self);
    if (defined($implication)) {
	if ($implication) {
	    $self->{relation} = $o->{relation};
	    $self->{version} = $o->{version};
	    return 1;
	} else {
	    return 0;
	}
    }
    if (defined($rev_implication)) {
	if ($rev_implication) {
	    # Already merged...
	    return 1;
	} else {
	    return 0;
	}
    }
    return 0;
}

package Dpkg::Deps::Multiple;

=head2 Dpkg::Deps::Multiple

This is the base class for Dpkg::Deps::{AND,OR,Union}. It implements
the following methods:

=over 4

=item $mul->add($dep)

Adds a new dependency object at the end of the list.

=back

=cut

use strict;
use warnings;

use Carp;

use Dpkg::ErrorHandling;

use parent qw(Dpkg::Interface::Storable);

sub new {
    my $this = shift;
    my $class = ref($this) || $this;
    my $self = { list => [ @_ ] };
    bless $self, $class;
    return $self;
}

sub reset {
    my ($self) = @_;
    $self->{list} = [];
}

sub add {
    my $self = shift;
    push @{$self->{list}}, @_;
}

sub get_deps {
    my $self = shift;
    return grep { not $_->is_empty() } @{$self->{list}};
}

sub sort {
    my $self = shift;
    my @res = ();
    @res = sort { Dpkg::Deps::deps_compare($a, $b) } @{$self->{list}};
    $self->{list} = [ @res ];
}

sub arch_is_concerned {
    my ($self, $host_arch) = @_;
    my $res = 0;
    foreach my $dep (@{$self->{list}}) {
	$res = 1 if $dep->arch_is_concerned($host_arch);
    }
    return $res;
}

sub reduce_arch {
    my ($self, $host_arch) = @_;
    my @new;
    foreach my $dep (@{$self->{list}}) {
	$dep->reduce_arch($host_arch);
	push @new, $dep if $dep->arch_is_concerned($host_arch);
    }
    $self->{list} = [ @new ];
}

sub has_arch_restriction {
    my ($self) = @_;
    my @res;
    foreach my $dep (@{$self->{list}}) {
	push @res, $dep->has_arch_restriction();
    }
    return @res;
}


sub is_empty {
    my $self = shift;
    return scalar @{$self->{list}} == 0;
}

sub merge_union {
    croak 'method merge_union() is only valid for Dpkg::Deps::Simple';
}

package Dpkg::Deps::AND;

=head2 Dpkg::Deps::AND

This object represents a list of dependencies who must be met at the same
time.

=over 4

=item $and->output([$fh])

The output method uses ", " to join the list of sub-dependencies.

=back

=cut

use strict;
use warnings;

use parent -norequire, qw(Dpkg::Deps::Multiple);

sub output {
    my ($self, $fh) = @_;
    my $res = join(', ', map { $_->output() } grep { not $_->is_empty() } $self->get_deps());
    if (defined($fh)) {
	print { $fh } $res;
    }
    return $res;
}

sub implies {
    my ($self, $o) = @_;
    # If any individual member can imply $o or NOT $o, we're fine
    foreach my $dep ($self->get_deps()) {
	my $implication = $dep->implies($o);
	return 1 if defined($implication) && $implication == 1;
	return 0 if defined($implication) && $implication == 0;
    }
    # If o is an AND, we might have an implication, if we find an
    # implication within us for each predicate in o
    if ($o->isa('Dpkg::Deps::AND')) {
	my $subset = 1;
	foreach my $odep ($o->get_deps()) {
	    my $found = 0;
	    foreach my $dep ($self->get_deps()) {
		$found = 1 if $dep->implies($odep);
	    }
	    $subset = 0 if not $found;
	}
	return 1 if $subset;
    }
    return;
}

sub get_evaluation {
    my ($self, $facts) = @_;
    # Return 1 only if all members evaluates to true
    # Return 0 if at least one member evaluates to false
    # Return undef otherwise
    my $result = 1;
    foreach my $dep ($self->get_deps()) {
	my $eval = $dep->get_evaluation($facts);
	if (not defined $eval) {
	    $result = undef;
	} elsif ($eval == 0) {
	    $result = 0;
	    last;
	} elsif ($eval == 1) {
	    # Still possible
	}
    }
    return $result;
}

sub simplify_deps {
    my ($self, $facts, @knowndeps) = @_;
    my @new;

WHILELOOP:
    while (@{$self->{list}}) {
	my $dep = shift @{$self->{list}};
	my $eval = $dep->get_evaluation($facts);
	next if defined($eval) and $eval == 1;
	foreach my $odep (@knowndeps, @new) {
	    next WHILELOOP if $odep->implies($dep);
	}
        # When a dependency is implied by another dependency that
        # follows, then invert them
        # "a | b, c, a"  becomes "a, c" and not "c, a"
        my $i = 0;
	foreach my $odep (@{$self->{list}}) {
            if (defined $odep and $odep->implies($dep)) {
                splice @{$self->{list}}, $i, 1;
                unshift @{$self->{list}}, $odep;
                next WHILELOOP;
            }
            $i++;
	}
	push @new, $dep;
    }
    $self->{list} = [ @new ];
}


package Dpkg::Deps::OR;

=head2 Dpkg::Deps::OR

This object represents a list of dependencies of which only one must be met
for the dependency to be true.

=over 4

=item $or->output([$fh])

The output method uses " | " to join the list of sub-dependencies.

=back

=cut

use strict;
use warnings;

use parent -norequire, qw(Dpkg::Deps::Multiple);

sub output {
    my ($self, $fh) = @_;
    my $res = join(' | ', map { $_->output() } grep { not $_->is_empty() } $self->get_deps());
    if (defined($fh)) {
	print { $fh } $res;
    }
    return $res;
}

sub implies {
    my ($self, $o) = @_;

    # Special case for AND with a single member, replace it by its member
    if ($o->isa('Dpkg::Deps::AND')) {
	my @subdeps = $o->get_deps();
	if (scalar(@subdeps) == 1) {
	    $o = $subdeps[0];
	}
    }

    # In general, an OR dependency can't imply anything except if each
    # of its member implies a member in the other OR dependency
    if ($o->isa('Dpkg::Deps::OR')) {
	my $subset = 1;
	foreach my $dep ($self->get_deps()) {
	    my $found = 0;
	    foreach my $odep ($o->get_deps()) {
		$found = 1 if $dep->implies($odep);
	    }
	    $subset = 0 if not $found;
	}
	return 1 if $subset;
    }
    return;
}

sub get_evaluation {
    my ($self, $facts) = @_;
    # Returns false if all members evaluates to 0
    # Returns true if at least one member evaluates to true
    # Returns undef otherwise
    my $result = 0;
    foreach my $dep ($self->get_deps()) {
	my $eval = $dep->get_evaluation($facts);
	if (not defined $eval) {
	    $result = undef;
	} elsif ($eval == 1) {
	    $result = 1;
	    last;
	} elsif ($eval == 0) {
	    # Still possible to have a false evaluation
	}
    }
    return $result;
}

sub simplify_deps {
    my ($self, $facts) = @_;
    my @new;

WHILELOOP:
    while (@{$self->{list}}) {
	my $dep = shift @{$self->{list}};
	my $eval = $dep->get_evaluation($facts);
	if (defined($eval) and $eval == 1) {
	    $self->{list} = [];
	    return;
	}
	foreach my $odep (@new, @{$self->{list}}) {
	    next WHILELOOP if $odep->implies($dep);
	}
	push @new, $dep;
    }
    $self->{list} = [ @new ];
}

package Dpkg::Deps::Union;

=head2 Dpkg::Deps::Union

This object represents a list of relationships.

=over 4

=item $union->output([$fh])

The output method uses ", " to join the list of relationships.

=item $union->implies($other_dep)

=item $union->get_evaluation($other_dep)

Those methods are not meaningful for this object and always return undef.

=item $union->simplify_deps($facts)

The simplication is done to generate an union of all the relationships.
It uses $simple_dep->merge_union($other_dep) to get its job done.

=back

=cut

use strict;
use warnings;

use parent -norequire, qw(Dpkg::Deps::Multiple);

sub output {
    my ($self, $fh) = @_;
    my $res = join(', ', map { $_->output() } grep { not $_->is_empty() } $self->get_deps());
    if (defined($fh)) {
	print { $fh } $res;
    }
    return $res;
}

sub implies {
    # Implication test are not useful on Union
    return;
}

sub get_evaluation {
    # Evaluation are not useful on Union
    return;
}

sub simplify_deps {
    my ($self, $facts) = @_;
    my @new;

WHILELOOP:
    while (@{$self->{list}}) {
	my $odep = shift @{$self->{list}};
	foreach my $dep (@new) {
	    next WHILELOOP if $dep->merge_union($odep);
	}
	push @new, $odep;
    }
    $self->{list} = [ @new ];
}

package Dpkg::Deps::KnownFacts;

=head2 Dpkg::Deps::KnownFacts

This object represents a list of installed packages and a list of virtual
packages provided (by the set of installed packages).

=over 4

=item my $facts = Dpkg::Deps::KnownFacts->new();

Creates a new object.

=cut

use strict;
use warnings;

use Dpkg::Version;

sub new {
    my $this = shift;
    my $class = ref($this) || $this;
    my $self = {
	pkg => {},
	virtualpkg => {},
    };
    bless $self, $class;
    return $self;
}

=item $facts->add_installed_package($package, $version, $arch, $multiarch)

Records that the given version of the package is installed. If
$version/$arch is undefined we know that the package is installed but we
don't know which version/architecture it is. $multiarch is the Multi-Arch
field of the package. If $multiarch is undef, it will be equivalent to
"Multi-Arch: no".

Note that $multiarch is only used if $arch is provided.

=cut

sub add_installed_package {
    my ($self, $pkg, $ver, $arch, $multiarch) = @_;
    my $p = {
	package => $pkg,
	version => $ver,
	architecture => $arch,
	multiarch => $multiarch || 'no',
    };
    $self->{pkg}{"$pkg:$arch"} = $p if defined $arch;
    push @{$self->{pkg}{$pkg}}, $p;
}

=item $facts->add_provided_package($virtual, $relation, $version, $by)

Records that the "$by" package provides the $virtual package. $relation
and $version correspond to the associated relation given in the Provides
field. This might be used in the future for versioned provides.

=cut

sub add_provided_package {
    my ($self, $pkg, $rel, $ver, $by) = @_;
    if (not exists $self->{virtualpkg}{$pkg}) {
	$self->{virtualpkg}{$pkg} = [];
    }
    push @{$self->{virtualpkg}{$pkg}}, [ $by, $rel, $ver ];
}

=item my ($check, $param) = $facts->check_package($package)

$check is one when the package is found. For a real package, $param
contains the version. For a virtual package, $param contains an array
reference containing the list of packages that provide it (each package is
listed as [ $provider, $relation, $version ]).

This function is obsolete and should not be used. Dpkg::Deps::KnownFacts
is only meant to be filled with data and then passed to Dpkg::Deps
methods where appropriate, but it should not be directly queried.

=back

=cut

sub check_package {
    my ($self, $pkg) = @_;
    if (exists $self->{pkg}{$pkg}) {
	return (1, $self->{pkg}{$pkg}[0]{version});
    }
    if (exists $self->{virtualpkg}{$pkg}) {
	return (1, $self->{virtualpkg}{$pkg});
    }
    return (0, undef);
}

## The functions below are private to Dpkg::Deps

sub _find_package {
    my ($self, $dep, $lackinfos) = @_;
    my ($pkg, $archqual) = ($dep->{package}, $dep->{archqual});
    return if not exists $self->{pkg}{$pkg};
    my $host_arch = $dep->{host_arch};
    my $build_arch = $dep->{build_arch};
    foreach my $p (@{$self->{pkg}{$pkg}}) {
	my $a = $p->{architecture};
	my $ma = $p->{multiarch};
	if (not defined $a) {
	    $$lackinfos = 1;
	    next;
	}
	if (not defined $archqual) {
	    return $p if $ma eq 'foreign';
	    return $p if $a eq $host_arch or $a eq 'all';
	} elsif ($archqual eq 'any') {
	    return $p if $ma eq 'allowed';
	} elsif ($archqual eq 'native') {
	    return $p if $a eq $build_arch and $ma ne 'foreign';
	} else {
	    return $p if $a eq $archqual;
	}
    }
    return;
}

sub _find_virtual_packages {
    my ($self, $pkg) = @_;
    return () if not exists $self->{virtualpkg}{$pkg};
    return @{$self->{virtualpkg}{$pkg}};
}

sub _evaluate_simple_dep {
    my ($self, $dep) = @_;
    my ($lackinfos, $pkg) = (0, $dep->{package});
    my $p = $self->_find_package($dep, \$lackinfos);
    if ($p) {
	if (defined $dep->{relation}) {
	    if (defined $p->{version}) {
		return 1 if version_compare_relation($p->{version},
		                           $dep->{relation}, $dep->{version});
	    } else {
		$lackinfos = 1;
	    }
	} else {
	    return 1;
	}
    }
    foreach my $virtpkg ($self->_find_virtual_packages($pkg)) {
	# XXX: Adapt when versioned provides are allowed
	next if defined $virtpkg->[1];
	next if defined $dep->{relation}; # Provides don't satisfy versioned deps
	return 1;
    }
    return if $lackinfos;
    return 0;
}

=head1 CHANGES

=head2 Version 1.02

=over

=item * Add new Dpkg::deps_concat() function.

=back

=head2 Version 1.01

=over

=item * Add new $dep->reset() method that all dependency objects support.

=item * Dpkg::Deps::Simple now recognizes the arch qualifier "any" and
stores it in the "archqual" property when present.

=item * Dpkg::Deps::KnownFacts->add_installed_package() now accepts 2
supplementary parameters ($arch and $multiarch).

=item * Dpkg::Deps::KnownFacts->check_package() is obsolete, it should
not have been part of the public API.

=back

=cut

1;
