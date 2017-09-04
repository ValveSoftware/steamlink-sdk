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

package Dpkg::ErrorHandling;

use strict;
use warnings;

our $VERSION = '0.02';

use Dpkg ();
use Dpkg::Gettext;

use Exporter qw(import);
our @EXPORT = qw(report_options info warning error errormsg
                 syserr subprocerr usageerr);
our @EXPORT_OK = qw(report);

my $quiet_warnings = 0;
my $info_fh = \*STDOUT;

sub report_options
{
    my (%options) = @_;

    if (exists $options{quiet_warnings}) {
        $quiet_warnings = $options{quiet_warnings};
    }
    if (exists $options{info_fh}) {
        $info_fh = $options{info_fh};
    }
}

sub report(@)
{
    my ($type, $msg) = (shift, shift);

    $msg = sprintf($msg, @_) if (@_);
    return "$Dpkg::PROGNAME: $type: $msg\n";
}

sub info($;@)
{
    print { $info_fh } report(_g('info'), @_) if (!$quiet_warnings);
}

sub warning($;@)
{
    warn report(_g('warning'), @_) if (!$quiet_warnings);
}

sub syserr($;@)
{
    my $msg = shift;
    die report(_g('error'), "$msg: $!", @_);
}

sub error($;@)
{
    die report(_g('error'), @_);
}

sub errormsg($;@)
{
    print { *STDERR } report(_g('error'), @_);
}

sub subprocerr(@)
{
    my ($p) = (shift);

    $p = sprintf($p, @_) if (@_);

    require POSIX;

    if (POSIX::WIFEXITED($?)) {
	error(_g('%s gave error exit status %s'), $p, POSIX::WEXITSTATUS($?));
    } elsif (POSIX::WIFSIGNALED($?)) {
	error(_g('%s died from signal %s'), $p, POSIX::WTERMSIG($?));
    } else {
	error(_g('%s failed with unknown exit code %d'), $p, $?);
    }
}

my $printforhelp = _g('Use --help for program usage information.');

sub usageerr(@)
{
    my ($msg) = (shift);

    $msg = sprintf($msg, @_) if (@_);
    warn "$Dpkg::PROGNAME: $msg\n\n";
    warn "$printforhelp\n";
    exit(2);
}

1;
