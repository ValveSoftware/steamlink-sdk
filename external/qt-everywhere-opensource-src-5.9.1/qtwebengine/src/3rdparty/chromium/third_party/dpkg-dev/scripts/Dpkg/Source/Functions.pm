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

package Dpkg::Source::Functions;

use strict;
use warnings;

our $VERSION = '0.01';

use Exporter qw(import);
our @EXPORT_OK = qw(erasedir fixperms fs_time is_binary);

use Dpkg::ErrorHandling;
use Dpkg::Gettext;
use Dpkg::IPC;

use POSIX qw(:errno_h);

sub erasedir {
    my ($dir) = @_;
    if (not lstat($dir)) {
        return if $! == ENOENT;
        syserr(_g('cannot stat directory %s (before removal)'), $dir);
    }
    system 'rm','-rf','--',$dir;
    subprocerr("rm -rf $dir") if $?;
    if (not stat($dir)) {
        return if $! == ENOENT;
        syserr(_g("unable to check for removal of dir `%s'"), $dir);
    }
    error(_g("rm -rf failed to remove `%s'"), $dir);
}

sub fixperms {
    my ($dir) = @_;
    my ($mode, $modes_set);
    # Unfortunately tar insists on applying our umask _to the original
    # permissions_ rather than mostly-ignoring the original
    # permissions.  We fix it up with chmod -R (which saves us some
    # work) but we have to construct a u+/- string which is a bit
    # of a palaver.  (Numeric doesn't work because we need [ugo]+X
    # and [ugo]=<stuff> doesn't work because that unsets sgid on dirs.)
    $mode = 0777 & ~umask;
    for my $i (0 .. 2) {
        $modes_set .= ',' if $i;
        $modes_set .= qw(u g o)[$i];
        for my $j (0 .. 2) {
            $modes_set .= $mode & (0400 >> ($i * 3 + $j)) ? '+' : '-';
            $modes_set .= qw(r w X)[$j];
        }
    }
    system('chmod', '-R', '--', $modes_set, $dir);
    subprocerr("chmod -R -- $modes_set $dir") if $?;
}

# Touch the file and read the resulting mtime.
#
# If the file doesn't exist, create it, read the mtime and unlink it.
#
# Use this instead of time() when the timestamp is going to be
# used to set file timestamps. This avoids confusion when an
# NFS server and NFS client disagree about what time it is.
sub fs_time($) {
    my ($file) = @_;
    my $is_temp = 0;
    if (not -e $file) {
	open(my $temp_fh, '>', $file) or syserr(_g('cannot write %s'));
	close($temp_fh);
	$is_temp = 1;
    } else {
	utime(undef, undef, $file) or
	    syserr(_g('cannot change timestamp for %s'), $file);
    }
    stat($file) or syserr(_g('cannot read timestamp from %s'), $file);
    my $mtime = (stat(_))[9];
    unlink($file) if $is_temp;
    return $mtime;
}

sub is_binary($) {
    my ($file) = @_;

    # TODO: might want to reimplement what diff does, aka checking if the
    # file contains \0 in the first 4Kb of data

    # Use diff to check if it's a binary file
    my $diffgen;
    my $diff_pid = spawn(
        exec => [ 'diff', '-u', '--', '/dev/null', $file ],
        env => { LC_ALL => 'C', LANG => 'C', TZ => 'UTC0' },
        to_pipe => \$diffgen,
    );
    my $result = 0;
    local $_;
    while (<$diffgen>) {
        if (m/^(?:binary|[^-+\@ ].*\bdiffer\b)/i) {
            $result = 1;
            last;
        } elsif (m/^[-+\@ ]/) {
            $result = 0;
            last;
        }
    }
    close($diffgen) or syserr('close on diff pipe');
    wait_child($diff_pid, nocheck => 1, cmdline => "diff -u -- /dev/null $file");
    return $result;
}

1;
