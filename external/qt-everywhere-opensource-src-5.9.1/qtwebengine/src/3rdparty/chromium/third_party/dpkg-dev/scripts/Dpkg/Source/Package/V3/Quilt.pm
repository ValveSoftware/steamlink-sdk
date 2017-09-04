# Copyright © 2008-2012 Raphaël Hertzog <hertzog@debian.org>
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

package Dpkg::Source::Package::V3::Quilt;

use strict;
use warnings;

our $VERSION = '0.01';

# Based on wig&pen implementation
use parent qw(Dpkg::Source::Package::V2);

use Dpkg;
use Dpkg::Gettext;
use Dpkg::ErrorHandling;
use Dpkg::Util qw(:list);
use Dpkg::Version;
use Dpkg::Source::Patch;
use Dpkg::Source::Functions qw(erasedir fs_time);
use Dpkg::Source::Quilt;
use Dpkg::Exit;

use File::Spec;
use File::Copy;

our $CURRENT_MINOR_VERSION = '0';

sub init_options {
    my ($self) = @_;
    $self->{options}{single_debian_patch} = 0
        unless exists $self->{options}{single_debian_patch};
    $self->{options}{allow_version_of_quilt_db} = []
        unless exists $self->{options}{allow_version_of_quilt_db};

    $self->SUPER::init_options();
}

sub parse_cmdline_option {
    my ($self, $opt) = @_;
    return 1 if $self->SUPER::parse_cmdline_option($opt);
    if ($opt =~ /^--single-debian-patch$/) {
        $self->{options}{single_debian_patch} = 1;
        # For backwards compatibility.
        $self->{options}{auto_commit} = 1;
        return 1;
    } elsif ($opt =~ /^--allow-version-of-quilt-db=(.*)$/) {
        push @{$self->{options}{allow_version_of_quilt_db}}, $1;
        return 1;
    }
    return 0;
}

sub build_quilt_object {
    my ($self, $dir) = @_;
    return $self->{quilt}{$dir} if exists $self->{quilt}{$dir};
    $self->{quilt}{$dir} = Dpkg::Source::Quilt->new($dir);
    return $self->{quilt}{$dir};
}

sub can_build {
    my ($self, $dir) = @_;
    my ($code, $msg) = $self->SUPER::can_build($dir);
    return ($code, $msg) if $code == 0;

    my $v = Dpkg::Version->new($self->{fields}->{'Version'});
    warning (_g('version does not contain a revision')) if $v->is_native();

    my $quilt = $self->build_quilt_object($dir);
    $msg = $quilt->find_problems();
    return (0, $msg) if $msg;
    return 1;
}

sub get_autopatch_name {
    my ($self) = @_;
    if ($self->{options}{single_debian_patch}) {
        return 'debian-changes';
    } else {
        return 'debian-changes-' . $self->{fields}{'Version'};
    }
}

sub apply_patches {
    my ($self, $dir, %opts) = @_;

    if ($opts{usage} eq 'unpack') {
        $opts{verbose} = 1;
    } elsif ($opts{usage} eq 'build') {
        $opts{warn_options} = 1;
        $opts{verbose} = 0;
    }

    my $quilt = $self->build_quilt_object($dir);
    $quilt->load_series(%opts) if $opts{warn_options}; # Trigger warnings

    # Always create the quilt db so that if the maintainer calls quilt to
    # create a patch, it's stored in the right directory
    $quilt->write_db();

    # Update debian/patches/series symlink if needed to allow quilt usage
    my $series = $quilt->get_series_file();
    my $basename = (File::Spec->splitpath($series))[2];
    if ($basename ne 'series') {
        my $dest = $quilt->get_patch_file('series');
        unlink($dest) if -l $dest;
        unless (-f _) { # Don't overwrite real files
            symlink($basename, $dest)
                or syserr(_g("can't create symlink %s"), $dest);
        }
    }

    return unless scalar($quilt->series());

    if ($opts{usage} eq 'preparation' and
        $self->{options}{unapply_patches} eq 'auto') {
        # We're applying the patches in --before-build, remember to unapply
        # them afterwards in --after-build
        my $pc_unapply = $quilt->get_db_file('.dpkg-source-unapply');
        open(my $unapply_fh, '>', $pc_unapply)
            or syserr(_g('cannot write %s'), $pc_unapply);
        close($unapply_fh);
    }

    # Apply patches
    my $pc_applied = $quilt->get_db_file('applied-patches');
    $opts{timestamp} = fs_time($pc_applied);
    if ($opts{skip_auto}) {
        my $auto_patch = $self->get_autopatch_name();
        $quilt->push(%opts) while ($quilt->next() and $quilt->next() ne $auto_patch);
    } else {
        $quilt->push(%opts) while $quilt->next();
    }
}

sub unapply_patches {
    my ($self, $dir, %opts) = @_;

    my $quilt = $self->build_quilt_object($dir);

    $opts{verbose} //= 1;

    my $pc_applied = $quilt->get_db_file('applied-patches');
    my @applied = $quilt->applied();
    $opts{timestamp} = fs_time($pc_applied) if @applied;

    $quilt->pop(%opts) while $quilt->top();

    erasedir($quilt->get_db_dir());
}

sub prepare_build {
    my ($self, $dir) = @_;
    $self->SUPER::prepare_build($dir);
    # Skip .pc directories of quilt by default and ignore difference
    # on debian/patches/series symlinks and d/p/.dpkg-source-applied
    # stamp file created by ourselves
    my $func = sub {
        return 1 if $_[0] =~ m{^debian/patches/series$} and -l $_[0];
        return 1 if $_[0] =~ /^\.pc(\/|$)/;
        return 1 if $_[0] =~ /$self->{options}{diff_ignore_regex}/;
        return 0;
    };
    $self->{diff_options}{diff_ignore_func} = $func;
}

sub do_build {
    my ($self, $dir) = @_;

    my $quilt = $self->build_quilt_object($dir);
    my $version = $quilt->get_db_version();

    if (defined($version) and $version != 2) {
        if (any { $version eq $_ }
            @{$self->{options}{allow_version_of_quilt_db}})
        {
            warning(_g('unsupported version of the quilt metadata: %s'), $version);
        } else {
            error(_g('unsupported version of the quilt metadata: %s'), $version);
        }
    }

    $self->SUPER::do_build($dir);
}

sub after_build {
    my ($self, $dir) = @_;
    my $quilt = $self->build_quilt_object($dir);
    my $pc_unapply = $quilt->get_db_file('.dpkg-source-unapply');
    my $opt_unapply = $self->{options}{unapply_patches};
    if (($opt_unapply eq 'auto' and -e $pc_unapply) or $opt_unapply eq 'yes') {
        unlink($pc_unapply);
        $self->unapply_patches($dir);
    }
}

sub check_patches_applied {
    my ($self, $dir) = @_;

    my $quilt = $self->build_quilt_object($dir);
    my $next = $quilt->next();
    return if not defined $next;

    my $first_patch = File::Spec->catfile($dir, 'debian', 'patches', $next);
    my $patch_obj = Dpkg::Source::Patch->new(filename => $first_patch);
    return unless $patch_obj->check_apply($dir);

    $self->apply_patches($dir, usage => 'preparation', verbose => 1);
}

sub _add_line {
    my ($file, $line) = @_;

    open(my $file_fh, '>>', $file) or syserr(_g('cannot write %s'), $file);
    print { $file_fh } "$line\n";
    close($file_fh);
}

sub _drop_line {
    my ($file, $re) = @_;

    open(my $file_fh, '<', $file) or syserr(_g('cannot read %s'), $file);
    my @lines = <$file_fh>;
    close($file_fh);
    open($file_fh, '>', $file) or syserr(_g('cannot write %s'), $file);
    print { $file_fh } $_ foreach grep { not /^\Q$re\E\s*$/ } @lines;
    close($file_fh);
}

sub register_patch {
    my ($self, $dir, $tmpdiff, $patch_name) = @_;

    my $quilt = $self->build_quilt_object($dir);

    my @patches = $quilt->series();
    my $has_patch = (grep { $_ eq $patch_name } @patches) ? 1 : 0;
    my $series = $quilt->get_series_file();
    my $applied = $quilt->get_db_file('applied-patches');
    my $patch = $quilt->get_patch_file($patch_name);

    if (-s $tmpdiff) {
        copy($tmpdiff, $patch)
            or syserr(_g('failed to copy %s to %s'), $tmpdiff, $patch);
        chmod(0666 & ~ umask(), $patch)
            or syserr(_g("unable to change permission of `%s'"), $patch);
    } elsif (-e $patch) {
        unlink($patch) or syserr(_g('cannot remove %s'), $patch);
    }

    if (-e $patch) {
        $quilt->setup_db();
        # Add patch to series file
        if (not $has_patch) {
            _add_line($series, $patch_name);
            _add_line($applied, $patch_name);
            $quilt->load_series();
            $quilt->load_db();
        }
        # Ensure quilt meta-data are created and in sync with some trickery:
        # reverse-apply the patch, drop .pc/$patch, re-apply it
        # with the correct options to recreate the backup files
        $quilt->pop(reverse_apply => 1);
        $quilt->push();
    } else {
        # Remove auto_patch from series
        if ($has_patch) {
            _drop_line($series, $patch_name);
            _drop_line($applied, $patch_name);
            erasedir($quilt->get_db_file($patch_name));
            $quilt->load_db();
            $quilt->load_series();
        }
        # Clean up empty series
        unlink($series) if -z $series;
    }
    return $patch;
}

1;
