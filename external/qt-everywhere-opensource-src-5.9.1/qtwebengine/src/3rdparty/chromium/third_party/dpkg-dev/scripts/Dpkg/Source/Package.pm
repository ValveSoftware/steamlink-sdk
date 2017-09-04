# Copyright © 2008-2011 Raphaël Hertzog <hertzog@debian.org>
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

package Dpkg::Source::Package;

=encoding utf8

=head1 NAME

Dpkg::Source::Package - manipulate Debian source packages

=head1 DESCRIPTION

This module provides an object that can manipulate Debian source
packages. While it supports both the extraction and the creation
of source packages, the only API that is officially supported
is the one that supports the extraction of the source package.

=head1 FUNCTIONS

=cut

use strict;
use warnings;

our $VERSION = '1.01';

use Dpkg::Gettext;
use Dpkg::ErrorHandling;
use Dpkg::Control;
use Dpkg::Checksums;
use Dpkg::Version;
use Dpkg::Compression;
use Dpkg::Exit qw(run_exit_handlers);
use Dpkg::Path qw(check_files_are_the_same find_command);
use Dpkg::IPC;
use Dpkg::Vendor qw(run_vendor_hook);

use Carp;
use POSIX qw(:errno_h :sys_wait_h);
use File::Basename;

use Exporter qw(import);
our @EXPORT_OK = qw(get_default_diff_ignore_regex
                    set_default_diff_ignore_regex
                    get_default_tar_ignore_pattern);

my $diff_ignore_default_regex = '
# Ignore general backup files
(?:^|/).*~$|
# Ignore emacs recovery files
(?:^|/)\.#.*$|
# Ignore vi swap files
(?:^|/)\..*\.sw.$|
# Ignore baz-style junk files or directories
(?:^|/),,.*(?:$|/.*$)|
# File-names that should be ignored (never directories)
(?:^|/)(?:DEADJOE|\.arch-inventory|\.(?:bzr|cvs|hg|git)ignore)$|
# File or directory names that should be ignored
(?:^|/)(?:CVS|RCS|\.deps|\{arch\}|\.arch-ids|\.svn|
\.hg(?:tags|sigs)?|_darcs|\.git(?:attributes|modules)?|
\.shelf|_MTN|\.be|\.bzr(?:\.backup|tags)?)(?:$|/.*$)
';
# Take out comments and newlines
$diff_ignore_default_regex =~ s/^#.*$//mg;
$diff_ignore_default_regex =~ s/\n//sg;

# Public variables
# XXX: Backwards compatibility, stop exporting on VERSION 2.00.
## no critic (Variables::ProhibitPackageVars)
our $diff_ignore_default_regexp;
*diff_ignore_default_regexp = \$diff_ignore_default_regex;

no warnings 'qw'; ## no critic (TestingAndDebugging::ProhibitNoWarnings)
our @tar_ignore_default_pattern = qw(
*.a
*.la
*.o
*.so
.*.sw?
*/*~
,,*
.[#~]*
.arch-ids
.arch-inventory
.be
.bzr
.bzr.backup
.bzr.tags
.bzrignore
.cvsignore
.deps
.git
.gitattributes
.gitignore
.gitmodules
.hg
.hgignore
.hgsigs
.hgtags
.shelf
.svn
CVS
DEADJOE
RCS
_MTN
_darcs
{arch}
);
## use critic

=over 4

=item my $string = get_default_diff_ignore_regex()

Returns the default diff ignore regex.

=cut

sub get_default_diff_ignore_regex {
    return $diff_ignore_default_regex;
}

=item set_default_diff_ignore_regex($string)

Set a regex as the new default diff ignore regex.

=cut

sub set_default_diff_ignore_regex {
    my ($regex) = @_;

    $diff_ignore_default_regex = $regex;
}

=item my @array = get_default_tar_ignore_pattern()

Returns the default tar ignore pattern, as an array.

=cut

sub get_default_tar_ignore_pattern {
    return @tar_ignore_default_pattern;
}

=item $p = Dpkg::Source::Package->new(filename => $dscfile, options => {})

Creates a new object corresponding to the source package described
by the file $dscfile.

The options hash supports the following options:

=over 8

=item skip_debianization

If set to 1, do not apply Debian changes on the extracted source package.

=item skip_patches

If set to 1, do not apply Debian-specific patches. This options is
specific for source packages using format "2.0" and "3.0 (quilt)".

=item require_valid_signature

If set to 1, the check_signature() method will be stricter and will error
out if the signature can't be verified.

=item copy_orig_tarballs

If set to 1, the extraction will copy the upstream tarballs next the
target directory. This is useful if you want to be able to rebuild the
source package after its extraction.

=back

=cut

# Object methods
sub new {
    my ($this, %args) = @_;
    my $class = ref($this) || $this;
    my $self = {
        fields => Dpkg::Control->new(type => CTRL_PKG_SRC),
        options => {},
        checksums => Dpkg::Checksums->new(),
    };
    bless $self, $class;
    if (exists $args{options}) {
        $self->{options} = $args{options};
    }
    if (exists $args{filename}) {
        $self->initialize($args{filename});
        $self->init_options();
    }
    return $self;
}

sub init_options {
    my ($self) = @_;
    # Use full ignore list by default
    # note: this function is not called by V1 packages
    $self->{options}{diff_ignore_regex} ||= $diff_ignore_default_regex;
    $self->{options}{diff_ignore_regex} .= '|(?:^|/)debian/source/local-.*$';
    if (defined $self->{options}{tar_ignore}) {
        $self->{options}{tar_ignore} = [ @tar_ignore_default_pattern ]
            unless @{$self->{options}{tar_ignore}};
    } else {
        $self->{options}{tar_ignore} = [ @tar_ignore_default_pattern ];
    }
    push @{$self->{options}{tar_ignore}}, 'debian/source/local-options',
         'debian/source/local-patch-header';
    # Skip debianization while specific to some formats has an impact
    # on code common to all formats
    $self->{options}{skip_debianization} ||= 0;
}

sub initialize {
    my ($self, $filename) = @_;
    my ($fn, $dir) = fileparse($filename);
    error(_g('%s is not the name of a file'), $filename) unless $fn;
    $self->{basedir} = $dir || './';
    $self->{filename} = $fn;

    # Read the fields
    my $fields = Dpkg::Control->new(type => CTRL_PKG_SRC);
    $fields->load($filename);
    $self->{fields} = $fields;
    $self->{is_signed} = $fields->get_option('is_pgp_signed');

    foreach my $f (qw(Source Version Files)) {
        unless (defined($fields->{$f})) {
            error(_g('missing critical source control field %s'), $f);
        }
    }

    $self->{checksums}->add_from_control($fields, use_files_for_md5 => 1);

    $self->upgrade_object_type(0);
}

sub upgrade_object_type {
    my ($self, $update_format) = @_;
    $update_format //= 1;
    $self->{fields}{'Format'} = '1.0'
        unless exists $self->{fields}{'Format'};
    my $format = $self->{fields}{'Format'};

    if ($format =~ /^([\d\.]+)(?:\s+\((.*)\))?$/) {
        my ($version, $variant, $major, $minor) = ($1, $2, $1, undef);

        if (defined $variant and $variant ne lc $variant) {
            error(_g("source package format '%s' is not supported: %s"),
                  $format, _g('format variant must be in lowercase'));
        }

        $major =~ s/\.[\d\.]+$//;
        my $module = "Dpkg::Source::Package::V$major";
        $module .= '::' . ucfirst $variant if defined $variant;
        eval "require $module; \$minor = \$${module}::CURRENT_MINOR_VERSION;";
        $minor //= 0;
        if ($update_format) {
            $self->{fields}{'Format'} = "$major.$minor";
            $self->{fields}{'Format'} .= " ($variant)" if defined $variant;
        }
        if ($@) {
            error(_g("source package format '%s' is not supported: %s"),
                  $format, $@);
        }
        bless $self, $module;
    } else {
        error(_g("invalid Format field `%s'"), $format);
    }
}

=item $p->get_filename()

Returns the filename of the DSC file.

=cut

sub get_filename {
    my ($self) = @_;
    return $self->{basedir} . $self->{filename};
}

=item $p->get_files()

Returns the list of files referenced by the source package. The filenames
usually do not have any path information.

=cut

sub get_files {
    my ($self) = @_;
    return $self->{checksums}->get_files();
}

=item $p->check_checksums()

Verify the checksums embedded in the DSC file. It requires the presence of
the other files constituting the source package. If any inconsistency is
discovered, it immediately errors out.

=cut

sub check_checksums {
    my ($self) = @_;
    my $checksums = $self->{checksums};
    # add_from_file verify the checksums if they are already existing
    foreach my $file ($checksums->get_files()) {
	$checksums->add_from_file($self->{basedir} . $file, key => $file);
    }
}

sub get_basename {
    my ($self, $with_revision) = @_;
    my $f = $self->{fields};
    unless (exists $f->{'Source'} and exists $f->{'Version'}) {
        error(_g('source and version are required to compute the source basename'));
    }
    my $v = Dpkg::Version->new($f->{'Version'});
    my $vs = $v->as_string(omit_epoch => 1, omit_revision => !$with_revision);
    return $f->{'Source'} . '_' . $vs;
}

sub find_original_tarballs {
    my ($self, %opts) = @_;
    $opts{extension} = compression_get_file_extension_regex()
        unless exists $opts{extension};
    $opts{include_main} = 1 unless exists $opts{include_main};
    $opts{include_supplementary} = 1 unless exists $opts{include_supplementary};
    my $basename = $self->get_basename();
    my @tar;
    foreach my $dir ('.', $self->{basedir}, $self->{options}{origtardir}) {
        next unless defined($dir) and -d $dir;
        opendir(my $dir_dh, $dir) or syserr(_g('cannot opendir %s'), $dir);
        push @tar, map { "$dir/$_" } grep {
		($opts{include_main} and
		 /^\Q$basename\E\.orig\.tar\.$opts{extension}$/) or
		($opts{include_supplementary} and
		 /^\Q$basename\E\.orig-[[:alnum:]-]+\.tar\.$opts{extension}$/)
	    } readdir($dir_dh);
        closedir($dir_dh);
    }
    return @tar;
}

=item $bool = $p->is_signed()

Returns 1 if the DSC files contains an embedded OpenPGP signature.
Otherwise returns 0.

=cut

sub is_signed {
    my $self = shift;
    return $self->{is_signed};
}

=item $p->check_signature()

Implement the same OpenPGP signature check that dpkg-source does.
In case of problems, it prints a warning or errors out.

If the object has been created with the "require_valid_signature" option,
then any problem will result in a fatal error.

=cut

sub check_signature {
    my ($self) = @_;
    my $dsc = $self->get_filename();
    my @exec;

    if (find_command('gpgv2')) {
        push @exec, 'gpgv2';
    } elsif (find_command('gpgv')) {
        push @exec, 'gpgv';
    } elsif (find_command('gpg2')) {
        push @exec, 'gpg2', '--no-default-keyring', '-q', '--verify';
    } elsif (find_command('gpg')) {
        push @exec, 'gpg', '--no-default-keyring', '-q', '--verify';
    }
    if (scalar(@exec)) {
        if (defined $ENV{HOME} and -r "$ENV{HOME}/.gnupg/trustedkeys.gpg") {
            push @exec, '--keyring', "$ENV{HOME}/.gnupg/trustedkeys.gpg";
        }
        foreach my $vendor_keyring (run_vendor_hook('keyrings')) {
            if (-r $vendor_keyring) {
                push @exec, '--keyring', $vendor_keyring;
            }
        }
        push @exec, $dsc;

        my ($stdout, $stderr);
        spawn(exec => \@exec, wait_child => 1, nocheck => 1,
              to_string => \$stdout, error_to_string => \$stderr,
              timeout => 10);
        if (WIFEXITED($?)) {
            my $gpg_status = WEXITSTATUS($?);
            print { *STDERR } "$stdout$stderr" if $gpg_status;
            if ($gpg_status == 1 or ($gpg_status &&
                $self->{options}{require_valid_signature}))
            {
                error(_g('failed to verify signature on %s'), $dsc);
            } elsif ($gpg_status) {
                warning(_g('failed to verify signature on %s'), $dsc);
            }
        } else {
            subprocerr("@exec");
        }
    } else {
        if ($self->{options}{require_valid_signature}) {
            error(_g("could not verify signature on %s since gpg isn't installed"), $dsc);
        } else {
            warning(_g("could not verify signature on %s since gpg isn't installed"), $dsc);
        }
    }
}

sub parse_cmdline_options {
    my ($self, @opts) = @_;
    foreach (@opts) {
        if (not $self->parse_cmdline_option($_)) {
            warning(_g('%s is not a valid option for %s'), $_, ref($self));
        }
    }
}

sub parse_cmdline_option {
    return 0;
}

=item $p->extract($targetdir)

Extracts the source package in the target directory $targetdir. Beware
that if $targetdir already exists, it will be erased.

=cut

sub extract {
    my $self = shift;
    my $newdirectory = $_[0];

    my ($ok, $error) = version_check($self->{fields}{'Version'});
    error($error) unless $ok;

    # Copy orig tarballs
    if ($self->{options}{copy_orig_tarballs}) {
        my $basename = $self->get_basename();
        my ($dirname, $destdir) = fileparse($newdirectory);
        $destdir ||= './';
	my $ext = compression_get_file_extension_regex();
        foreach my $orig (grep { /^\Q$basename\E\.orig(-[[:alnum:]-]+)?\.tar\.$ext$/ }
                          $self->get_files())
        {
            my $src = File::Spec->catfile($self->{basedir}, $orig);
            my $dst = File::Spec->catfile($destdir, $orig);
            if (not check_files_are_the_same($src, $dst, 1)) {
                system('cp', '--', $src, $dst);
                subprocerr("cp $src to $dst") if $?;
            }
        }
    }

    # Try extract
    eval { $self->do_extract(@_) };
    if ($@) {
        run_exit_handlers();
        die $@;
    }

    # Store format if non-standard so that next build keeps the same format
    if ($self->{fields}{'Format'} ne '1.0' and
        not $self->{options}{skip_debianization})
    {
        my $srcdir = File::Spec->catdir($newdirectory, 'debian', 'source');
        my $format_file = File::Spec->catfile($srcdir, 'format');
	unless (-e $format_file) {
	    mkdir($srcdir) unless -e $srcdir;
	    open(my $format_fh, '>', $format_file)
	        or syserr(_g('cannot write %s'), $format_file);
	    print { $format_fh } $self->{fields}{'Format'} . "\n";
	    close($format_fh);
	}
    }

    # Make sure debian/rules is executable
    my $rules = File::Spec->catfile($newdirectory, 'debian', 'rules');
    my @s = lstat($rules);
    if (not scalar(@s)) {
        unless ($! == ENOENT) {
            syserr(_g('cannot stat %s'), $rules);
        }
        warning(_g('%s does not exist'), $rules)
            unless $self->{options}{skip_debianization};
    } elsif (-f _) {
        chmod($s[2] | 0111, $rules)
            or syserr(_g('cannot make %s executable'), $rules);
    } else {
        warning(_g('%s is not a plain file'), $rules);
    }
}

sub do_extract {
    croak 'Dpkg::Source::Package does not know how to unpack a ' .
          'source package; use one of the subclasses';
}

# Function used specifically during creation of a source package

sub before_build {
    my ($self, $dir) = @_;
}

sub build {
    my $self = shift;
    eval { $self->do_build(@_) };
    if ($@) {
        run_exit_handlers();
        die $@;
    }
}

sub after_build {
    my ($self, $dir) = @_;
}

sub do_build {
    croak 'Dpkg::Source::Package does not know how to build a ' .
          'source package; use one of the subclasses';
}

sub can_build {
    my ($self, $dir) = @_;
    return (0, 'can_build() has not been overriden');
}

sub add_file {
    my ($self, $filename) = @_;
    my ($fn, $dir) = fileparse($filename);
    if ($self->{checksums}->has_file($fn)) {
        croak "tried to add file '$fn' twice";
    }
    $self->{checksums}->add_from_file($filename, key => $fn);
    $self->{checksums}->export_to_control($self->{fields},
					    use_files_for_md5 => 1);
}

sub commit {
    my $self = shift;
    eval { $self->do_commit(@_) };
    if ($@) {
        run_exit_handlers();
        die $@;
    }
}

sub do_commit {
    my ($self, $dir) = @_;
    info(_g("'%s' is not supported by the source format '%s'"),
         'dpkg-source --commit', $self->{fields}{'Format'});
}

sub write_dsc {
    my ($self, %opts) = @_;
    my $fields = $self->{fields};

    foreach my $f (keys %{$opts{override}}) {
	$fields->{$f} = $opts{override}{$f};
    }

    unless($opts{nocheck}) {
        foreach my $f (qw(Source Version)) {
            unless (defined($fields->{$f})) {
                error(_g('missing information for critical output field %s'), $f);
            }
        }
        foreach my $f (qw(Maintainer Architecture Standards-Version)) {
            unless (defined($fields->{$f})) {
                warning(_g('missing information for output field %s'), $f);
            }
        }
    }

    foreach my $f (keys %{$opts{remove}}) {
	delete $fields->{$f};
    }

    my $filename = $opts{filename};
    unless (defined $filename) {
        $filename = $self->get_basename(1) . '.dsc';
    }
    open(my $dsc_fh, '>', $filename)
        or syserr(_g('cannot write %s'), $filename);
    $fields->apply_substvars($opts{substvars});
    $fields->output($dsc_fh);
    close($dsc_fh);
}

=back

=head1 CHANGES

=head2 Version 1.01

New functions: get_default_diff_ignore_regex(), set_default_diff_ignore_regex(),
get_default_tar_ignore_pattern()

Deprecated variables: $diff_ignore_default_regexp, @tar_ignore_default_pattern

=head1 AUTHOR

Raphaël Hertzog, E<lt>hertzog@debian.orgE<gt>

=cut

1;
