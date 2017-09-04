# Copyright © 2008 Raphaël Hertzog <hertzog@debian.org>
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

package Dpkg::Source::Archive;

use strict;
use warnings;

our $VERSION = '0.01';

use Dpkg::Source::Functions qw(erasedir fixperms);
use Dpkg::Gettext;
use Dpkg::IPC;
use Dpkg::ErrorHandling;

use Carp;
use File::Temp qw(tempdir);
use File::Basename qw(basename);
use File::Spec;
use Cwd;

use parent qw(Dpkg::Compression::FileHandle);

sub create {
    my ($self, %opts) = @_;
    $opts{options} ||= [];
    my %spawn_opts;
    # Possibly run tar from another directory
    if ($opts{chdir}) {
        $spawn_opts{chdir} = $opts{chdir};
        *$self->{chdir} = $opts{chdir};
    }
    # Redirect input/output appropriately
    $self->ensure_open('w');
    $spawn_opts{to_handle} = $self->get_filehandle();
    $spawn_opts{from_pipe} = \*$self->{tar_input};
    # Call tar creation process
    $spawn_opts{delete_env} = [ 'TAR_OPTIONS' ];
    $spawn_opts{exec} = [ 'tar', '--null', '-T', '-', '--numeric-owner',
                            '--owner', '0', '--group', '0',
                            @{$opts{options}}, '-cf', '-' ];
    *$self->{pid} = spawn(%spawn_opts);
    *$self->{cwd} = getcwd();
}

sub _add_entry {
    my ($self, $file) = @_;
    my $cwd = *$self->{cwd};
    croak 'call create() first' unless *$self->{tar_input};
    $file = $2 if ($file =~ /^\Q$cwd\E\/(.+)$/); # Relative names
    print({ *$self->{tar_input} } "$file\0")
        or syserr(_g('write on tar input'));
}

sub add_file {
    my ($self, $file) = @_;
    my $testfile = $file;
    if (*$self->{chdir}) {
        $testfile = File::Spec->catfile(*$self->{chdir}, $file);
    }
    croak 'add_file() does not handle directories'
        if not -l $testfile and -d _;
    $self->_add_entry($file);
}

sub add_directory {
    my ($self, $file) = @_;
    my $testfile = $file;
    if (*$self->{chdir}) {
        $testfile = File::Spec->catdir(*$self->{chdir}, $file);
    }
    croak 'add_directory() only handles directories'
        if -l $testfile or not -d _;
    $self->_add_entry($file);
}

sub finish {
    my ($self) = @_;
    close(*$self->{tar_input}) or syserr(_g('close on tar input'));
    wait_child(*$self->{pid}, cmdline => 'tar -cf -');
    delete *$self->{pid};
    delete *$self->{tar_input};
    delete *$self->{cwd};
    delete *$self->{chdir};
    $self->close();
}

sub extract {
    my ($self, $dest, %opts) = @_;
    $opts{options} ||= [];
    $opts{in_place} ||= 0;
    $opts{no_fixperms} ||= 0;
    my %spawn_opts = (wait_child => 1);

    # Prepare destination
    my $tmp;
    if ($opts{in_place}) {
        $spawn_opts{chdir} = $dest;
        $tmp = $dest; # So that fixperms call works
    } else {
        my $template = basename($self->get_filename()) .  '.tmp-extract.XXXXX';
        unless (-e $dest) {
            # Kludge so that realpath works
            mkdir($dest) or syserr(_g('cannot create directory %s'), $dest);
        }
        $tmp = tempdir($template, DIR => Cwd::realpath("$dest/.."), CLEANUP => 1);
        $spawn_opts{chdir} = $tmp;
    }

    # Prepare stuff that handles the input of tar
    $self->ensure_open('r');
    $spawn_opts{from_handle} = $self->get_filehandle();

    # Call tar extraction process
    $spawn_opts{delete_env} = [ 'TAR_OPTIONS' ];
    $spawn_opts{exec} = [ 'tar', '--no-same-owner', '--no-same-permissions',
                            @{$opts{options}}, '-xf', '-' ];
    spawn(%spawn_opts);
    $self->close();

    # Fix permissions on extracted files because tar insists on applying
    # our umask _to the original permissions_ rather than mostly-ignoring
    # the original permissions.
    # We still need --no-same-permissions because otherwise tar might
    # extract directory setgid (which we want inherited, not
    # extracted); we need --no-same-owner because putting the owner
    # back is tedious - in particular, correct group ownership would
    # have to be calculated using mount options and other madness.
    fixperms($tmp) unless $opts{no_fixperms};

    # Stop here if we extracted in-place as there's nothing to move around
    return if $opts{in_place};

    # Rename extracted directory
    opendir(my $dir_dh, $tmp) or syserr(_g('cannot opendir %s'), $tmp);
    my @entries = grep { $_ ne '.' && $_ ne '..' } readdir($dir_dh);
    closedir($dir_dh);
    my $done = 0;
    erasedir($dest);
    if (scalar(@entries) == 1 && ! -l "$tmp/$entries[0]" && -d _) {
	rename("$tmp/$entries[0]", $dest)
	    or syserr(_g('unable to rename %s to %s'),
	              "$tmp/$entries[0]", $dest);
    } else {
	rename($tmp, $dest)
	    or syserr(_g('unable to rename %s to %s'), $tmp, $dest);
    }
    erasedir($tmp);
}

1;
