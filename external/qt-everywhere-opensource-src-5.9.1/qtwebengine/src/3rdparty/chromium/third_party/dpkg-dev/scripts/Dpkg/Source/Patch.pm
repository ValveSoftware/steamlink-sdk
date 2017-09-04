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

package Dpkg::Source::Patch;

use strict;
use warnings;

our $VERSION = '0.01';

use Dpkg;
use Dpkg::Gettext;
use Dpkg::IPC;
use Dpkg::ErrorHandling;
use Dpkg::Source::Functions qw(fs_time);

use POSIX qw(:errno_h :sys_wait_h);
use File::Find;
use File::Basename;
use File::Spec;
use File::Path;
use File::Compare;
use Fcntl ':mode';
#XXX: Needed for sub-second timestamps, require recent perl
#use Time::HiRes qw(stat);

use parent qw(Dpkg::Compression::FileHandle);

sub create {
    my ($self, %opts) = @_;
    $self->ensure_open('w'); # Creates the file
    *$self->{errors} = 0;
    *$self->{empty} = 1;
    if ($opts{old} and $opts{new}) {
        $opts{old} = '/dev/null' unless -e $opts{old};
        $opts{new} = '/dev/null' unless -e $opts{new};
        if (-d $opts{old} and -d $opts{new}) {
            $self->add_diff_directory($opts{old}, $opts{new}, %opts);
        } elsif (-f $opts{old} and -f $opts{new}) {
            $self->add_diff_file($opts{old}, $opts{new}, %opts);
        } else {
            $self->_fail_not_same_type($opts{old}, $opts{new});
        }
        $self->finish() unless $opts{nofinish};
    }
}

sub set_header {
    my ($self, $header) = @_;
    *$self->{header} = $header;
}

sub add_diff_file {
    my ($self, $old, $new, %opts) = @_;
    $opts{include_timestamp} = 0 unless exists $opts{include_timestamp};
    my $handle_binary = $opts{handle_binary_func} || sub {
        my ($self, $old, $new) = @_;
        $self->_fail_with_msg($new, _g('binary file contents changed'));
    };
    # Optimization to avoid forking diff if unnecessary
    return 1 if compare($old, $new, 4096) == 0;
    # Default diff options
    my @options;
    if ($opts{options}) {
        push @options, @{$opts{options}};
    } else {
        push @options, '-p';
    }
    # Add labels
    if ($opts{label_old} and $opts{label_new}) {
	if ($opts{include_timestamp}) {
	    my $ts = (stat($old))[9];
	    my $t = POSIX::strftime('%Y-%m-%d %H:%M:%S', gmtime($ts));
	    $opts{label_old} .= sprintf("\t%s.%09d +0000", $t,
	                                ($ts - int($ts)) * 1_000_000_000);
	    $ts = (stat($new))[9];
	    $t = POSIX::strftime('%Y-%m-%d %H:%M:%S', gmtime($ts));
	    $opts{label_new} .= sprintf("\t%s.%09d +0000", $t,
	                                ($ts - int($ts)) * 1_000_000_000);
	} else {
	    # Space in filenames need special treatment
	    $opts{label_old} .= "\t" if $opts{label_old} =~ / /;
	    $opts{label_new} .= "\t" if $opts{label_new} =~ / /;
	}
        push @options, '-L', $opts{label_old},
                       '-L', $opts{label_new};
    }
    # Generate diff
    my $diffgen;
    my $diff_pid = spawn(
        exec => [ 'diff', '-u', @options, '--', $old, $new ],
        env => { LC_ALL => 'C', LANG => 'C', TZ => 'UTC0' },
        to_pipe => \$diffgen,
    );
    # Check diff and write it in patch file
    my $difflinefound = 0;
    my $binary = 0;
    while (<$diffgen>) {
        if (m/^(?:binary|[^-+\@ ].*\bdiffer\b)/i) {
            $binary = 1;
            &$handle_binary($self, $old, $new);
            last;
        } elsif (m/^[-+\@ ]/) {
            $difflinefound++;
        } elsif (m/^\\ /) {
            warning(_g('file %s has no final newline (either ' .
                       'original or modified version)'), $new);
        } else {
            chomp;
            error(_g("unknown line from diff -u on %s: `%s'"), $new, $_);
        }
	if (*$self->{empty} and defined(*$self->{header})) {
	    $self->print(*$self->{header}) or syserr(_g('failed to write'));
	    *$self->{empty} = 0;
	}
        print { $self } $_ or syserr(_g('failed to write'));
    }
    close($diffgen) or syserr('close on diff pipe');
    wait_child($diff_pid, nocheck => 1,
               cmdline => "diff -u @options -- $old $new");
    # Verify diff process ended successfully
    # Exit code of diff: 0 => no difference, 1 => diff ok, 2 => error
    # Ignore error if binary content detected
    my $exit = WEXITSTATUS($?);
    unless (WIFEXITED($?) && ($exit == 0 || $exit == 1 || $binary)) {
        subprocerr(_g('diff on %s'), $new);
    }
    return ($exit == 0 || $exit == 1);
}

sub add_diff_directory {
    my ($self, $old, $new, %opts) = @_;
    # TODO: make this function more configurable
    # - offer to disable some checks
    my $basedir = $opts{basedirname} || basename($new);
    my $inc_removal = $opts{include_removal} || 0;
    my $diff_ignore;
    if ($opts{diff_ignore_func}) {
        $diff_ignore = $opts{diff_ignore_func};
    } elsif ($opts{diff_ignore_regex}) {
        $diff_ignore = sub { return $_[0] =~ /$opts{diff_ignore_regex}/o };
    } else {
        $diff_ignore = sub { return 0 };
    }

    my @diff_files;
    my %files_in_new;
    my $scan_new = sub {
        my $fn = (length > length($new)) ? substr($_, length($new) + 1) : '.';
        return if &$diff_ignore($fn);
        $files_in_new{$fn} = 1;
        lstat("$new/$fn") or syserr(_g('cannot stat file %s'), "$new/$fn");
        my $mode = S_IMODE((lstat(_))[2]);
        my $size = (lstat(_))[7];
        if (-l _) {
            unless (-l "$old/$fn") {
                $self->_fail_not_same_type("$old/$fn", "$new/$fn");
                return;
            }
            my $n = readlink("$new/$fn");
            unless (defined $n) {
                syserr(_g('cannot read link %s'), "$new/$fn");
            }
            my $n2 = readlink("$old/$fn");
            unless (defined $n2) {
                syserr(_g('cannot read link %s'), "$old/$fn");
            }
            unless ($n eq $n2) {
                $self->_fail_not_same_type("$old/$fn", "$new/$fn");
            }
        } elsif (-f _) {
            my $old_file = "$old/$fn";
            if (not lstat("$old/$fn")) {
                if ($! != ENOENT) {
                    syserr(_g('cannot stat file %s'), "$old/$fn");
                }
                $old_file = '/dev/null';
            } elsif (not -f _) {
                $self->_fail_not_same_type("$old/$fn", "$new/$fn");
                return;
            }

            my $label_old = "$basedir.orig/$fn";
            if ($opts{use_dev_null}) {
                $label_old = $old_file if $old_file eq '/dev/null';
            }
            push @diff_files, [$fn, $mode, $size, $old_file, "$new/$fn",
                               $label_old, "$basedir/$fn"];
        } elsif (-p _) {
            unless (-p "$old/$fn") {
                $self->_fail_not_same_type("$old/$fn", "$new/$fn");
            }
        } elsif (-b _ || -c _ || -S _) {
            $self->_fail_with_msg("$new/$fn",
                _g('device or socket is not allowed'));
        } elsif (-d _) {
            if (not lstat("$old/$fn")) {
                if ($! != ENOENT) {
                    syserr(_g('cannot stat file %s'), "$old/$fn");
                }
            } elsif (not -d _) {
                $self->_fail_not_same_type("$old/$fn", "$new/$fn");
            }
        } else {
            $self->_fail_with_msg("$new/$fn", _g('unknown file type'));
        }
    };
    my $scan_old = sub {
        my $fn = (length > length($old)) ? substr($_, length($old) + 1) : '.';
        return if &$diff_ignore($fn);
        return if $files_in_new{$fn};
        lstat("$old/$fn") or syserr(_g('cannot stat file %s'), "$old/$fn");
        if (-f _) {
            if ($inc_removal) {
                push @diff_files, [$fn, 0, 0, "$old/$fn", '/dev/null',
                                   "$basedir.orig/$fn", '/dev/null'];
            } else {
                warning(_g('ignoring deletion of file %s'), $fn);
            }
        } elsif (-d _) {
            warning(_g('ignoring deletion of directory %s'), $fn);
        } elsif (-l _) {
            warning(_g('ignoring deletion of symlink %s'), $fn);
        } else {
            $self->_fail_not_same_type("$old/$fn", "$new/$fn");
        }
    };

    find({ wanted => $scan_new, no_chdir => 1 }, $new);
    find({ wanted => $scan_old, no_chdir => 1 }, $old);

    if ($opts{order_from} and -e $opts{order_from}) {
        my $order_from = Dpkg::Source::Patch->new(
            filename => $opts{order_from});
        my $analysis = $order_from->analyze($basedir, verbose => 0);
        my %patchorder;
        my $i = 0;
        foreach my $fn (@{$analysis->{patchorder}}) {
            $fn =~ s{^[^/]+/}{};
            $patchorder{$fn} = $i++;
        }
        # 'quilt refresh' sorts files as follows:
        #   - Any files in the existing patch come first, in the order in
        #     which they appear in the existing patch.
        #   - New files follow, sorted lexicographically.
        # This seems a reasonable policy to follow, and avoids autopatches
        # being shuffled when they are regenerated.
        foreach my $diff_file (sort { $a->[0] cmp $b->[0] } @diff_files) {
            my $fn = $diff_file->[0];
            $patchorder{$fn} = $i++ unless exists $patchorder{$fn};
        }
        @diff_files = sort { $patchorder{$a->[0]} <=> $patchorder{$b->[0]} }
                      @diff_files;
    } else {
        @diff_files = sort { $a->[0] cmp $b->[0] } @diff_files;
    }

    foreach my $diff_file (@diff_files) {
        my ($fn, $mode, $size,
            $old_file, $new_file, $label_old, $label_new) = @$diff_file;
        my $success = $self->add_diff_file($old_file, $new_file,
                                           label_old => $label_old,
                                           label_new => $label_new, %opts);
        if ($success and
            $old_file eq '/dev/null' and $new_file ne '/dev/null') {
            if (not $size) {
                warning(_g("newly created empty file '%s' will not " .
                           'be represented in diff'), $fn);
            } else {
                if ($mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
                    warning(_g("executable mode %04o of '%s' will " .
                               'not be represented in diff'), $mode, $fn)
                        unless $fn eq 'debian/rules';
                }
                if ($mode & (S_ISUID | S_ISGID | S_ISVTX)) {
                    warning(_g("special mode %04o of '%s' will not " .
                               'be represented in diff'), $mode, $fn);
                }
            }
        }
    }
}

sub finish {
    my ($self) = @_;
    close($self) or syserr(_g('cannot close %s'), $self->get_filename());
    return not *$self->{errors};
}

sub register_error {
    my ($self) = @_;
    *$self->{errors}++;
}
sub _fail_with_msg {
    my ($self, $file, $msg) = @_;
    errormsg(_g('cannot represent change to %s: %s'), $file, $msg);
    $self->register_error();
}
sub _fail_not_same_type {
    my ($self, $old, $new) = @_;
    my $old_type = get_type($old);
    my $new_type = get_type($new);
    errormsg(_g('cannot represent change to %s:'), $new);
    errormsg(_g('  new version is %s'), $new_type);
    errormsg(_g('  old version is %s'), $old_type);
    $self->register_error();
}

sub _getline {
    my $handle = shift;

    my $line = <$handle>;
    if (defined $line) {
        # Strip end-of-line chars
        chomp($line);
        $line =~ s/\r$//;
    }
    return $line;
}

sub _intuit_file_patched {
    my ($old, $new) = @_;

    return $new unless defined $old;
    return $old unless defined $new;
    return $new if -e $new and not -e $old;
    return $old if -e $old and not -e $new;

    # We don't consider the case where both files are non-existent and
    # where patch picks the one with the fewest directories to create
    # since dpkg-source will pre-create the required directories

    # Precalculate metrics used by patch
    my ($tmp_o, $tmp_n) = ($old, $new);
    my ($len_o, $len_n) = (length($old), length($new));
    $tmp_o =~ s{[/\\]+}{/}g;
    $tmp_n =~ s{[/\\]+}{/}g;
    my $nb_comp_o = ($tmp_o =~ tr{/}{/});
    my $nb_comp_n = ($tmp_n =~ tr{/}{/});
    $tmp_o =~ s{^.*/}{};
    $tmp_n =~ s{^.*/}{};
    my ($blen_o, $blen_n) = (length($tmp_o), length($tmp_n));

    # Decide like patch would
    if ($nb_comp_o != $nb_comp_n) {
        return ($nb_comp_o < $nb_comp_n) ? $old : $new;
    } elsif ($blen_o != $blen_n) {
        return ($blen_o < $blen_n) ? $old : $new;
    } elsif ($len_o != $len_n) {
        return ($len_o < $len_n) ? $old : $new;
    }
    return $old;
}

# Fetch the header filename ignoring the optional timestamp
sub _fetch_filename {
    my ($diff, $header) = @_;

    # Strip any leading spaces.
    $header =~ s/^\s+//;

    # Is it a C-style string?
    if ($header =~ m/^"/) {
        error(_g('diff %s patches file with C-style encoded filename'), $diff);
    } else {
        # Tab is the official separator, it's always used when
        # filename contain spaces. Try it first, otherwise strip on space
        # if there's no tab
        $header =~ s/\s.*// unless $header =~ s/\t.*//;
    }

    return $header;
}

# check diff for sanity, find directories to create as a side effect
sub analyze {
    my ($self, $destdir, %opts) = @_;

    $opts{verbose} //= 1;
    my $diff = $self->get_filename();
    my %filepatched;
    my %dirtocreate;
    my @patchorder;
    my $patch_header = '';
    my $diff_count = 0;

    $_ = _getline($self);

  HUNK:
    while (defined($_) or not eof($self)) {
	my (%path, %fn);
	# skip comments leading up to patch (if any)
	while (1) {
	    if (/^(?:--- |\+\+\+ |@@ -)/) {
		last;
	    } else {
		$patch_header .= "$_\n";
	    }
	    last HUNK if not defined($_ = _getline($self));
	}
	$diff_count++;
	# read file header (---/+++ pair)
	unless(s/^--- //) {
	    error(_g("expected ^--- in line %d of diff `%s'"), $., $diff);
	}
        $path{old} = $_ = _fetch_filename($diff, $_);
	$fn{old} = $_ if $_ ne '/dev/null' and s{^[^/]*/+}{$destdir/};
	if (/\.dpkg-orig$/) {
	    error(_g("diff `%s' patches file with name ending .dpkg-orig"), $diff);
	}

	unless (defined($_ = _getline($self))) {
	    error(_g("diff `%s' finishes in middle of ---/+++ (line %d)"), $diff, $.);
	}
	unless (s/^\+\+\+ //) {
	    error(_g("line after --- isn't as expected in diff `%s' (line %d)"), $diff, $.);
	}
        $path{new} = $_ = _fetch_filename($diff, $_);
	$fn{new} = $_ if $_ ne '/dev/null' and s{^[^/]*/+}{$destdir/};

	unless (defined $fn{old} or defined $fn{new}) {
	    error(_g("none of the filenames in ---/+++ are valid in diff '%s' (line %d)"),
		  $diff, $.);
	}

	# Safety checks on both filenames that patch could use
	foreach my $key ('old', 'new') {
	    next unless defined $fn{$key};
	    if ($path{$key} =~ m{/\.\./}) {
		error(_g('%s contains an insecure path: %s'), $diff, $path{$key});
	    }
	    my $path = $fn{$key};
	    while (1) {
		if (-l $path) {
		    error(_g('diff %s modifies file %s through a symlink: %s'),
			  $diff, $fn{$key}, $path);
		}
		last unless $path =~ s{/+[^/]*$}{};
		last if length($path) <= length($destdir); # $destdir is assumed safe
	    }
	}

        if ($path{old} eq '/dev/null' and $path{new} eq '/dev/null') {
            error(_g("original and modified files are /dev/null in diff `%s' (line %d)"),
                  $diff, $.);
        } elsif ($path{new} eq '/dev/null') {
            error(_g("file removal without proper filename in diff `%s' (line %d)"),
                  $diff, $. - 1) unless defined $fn{old};
            if ($opts{verbose}) {
                warning(_g('diff %s removes a non-existing file %s (line %d)'),
                        $diff, $fn{old}, $.) unless -e $fn{old};
            }
        }
	my $fn = _intuit_file_patched($fn{old}, $fn{new});

	my $dirname = $fn;
	if ($dirname =~ s{/[^/]+$}{} and not -d $dirname) {
	    $dirtocreate{$dirname} = 1;
	}

	if (-e $fn and not -f _) {
	    error(_g("diff `%s' patches something which is not a plain file"), $diff);
	}

	if ($filepatched{$fn}) {
	    warning(_g("diff `%s' patches file %s twice"), $diff, $fn)
		if $opts{verbose};
	} else {
	    $filepatched{$fn} = 1;
	    push @patchorder, $fn;
	}

	# read hunks
	my $hunk = 0;
	while (defined($_ = _getline($self))) {
	    # read hunk header (@@)
	    next if /^\\ /;
	    last unless (/^@@ -\d+(,(\d+))? \+\d+(,(\d+))? @\@( .*)?$/);
	    my ($olines, $nlines) = ($1 ? $2 : 1, $3 ? $4 : 1);
	    # read hunk
	    while ($olines || $nlines) {
		unless (defined($_ = _getline($self))) {
                    if (($olines == $nlines) and ($olines < 3)) {
                        warning(_g("unexpected end of diff `%s'"), $diff)
                            if $opts{verbose};
                        last;
                    } else {
                        error(_g("unexpected end of diff `%s'"), $diff);
                    }
		}
		next if /^\\ /;
		# Check stats
		if    (/^ / || /^$/)  { --$olines; --$nlines; }
		elsif (/^-/)  { --$olines; }
		elsif (/^\+/) { --$nlines; }
		else {
		    error(_g("expected [ +-] at start of line %d of diff `%s'"),
		          $., $diff);
		}
	    }
	    $hunk++;
	}
	unless($hunk) {
	    error(_g("expected ^\@\@ at line %d of diff `%s'"), $., $diff);
	}
    }
    close($self);
    unless ($diff_count) {
	warning(_g("diff `%s' doesn't contain any patch"), $diff)
	    if $opts{verbose};
    }
    *$self->{analysis}{$destdir}{dirtocreate} = \%dirtocreate;
    *$self->{analysis}{$destdir}{filepatched} = \%filepatched;
    *$self->{analysis}{$destdir}{patchorder} = \@patchorder;
    *$self->{analysis}{$destdir}{patchheader} = $patch_header;
    return *$self->{analysis}{$destdir};
}

sub prepare_apply {
    my ($self, $analysis, %opts) = @_;
    if ($opts{create_dirs}) {
	foreach my $dir (keys %{$analysis->{dirtocreate}}) {
	    eval { mkpath($dir, 0, 0777); };
	    syserr(_g('cannot create directory %s'), $dir) if $@;
	}
    }
}

sub apply {
    my ($self, $destdir, %opts) = @_;
    # Set default values to options
    $opts{force_timestamp} = 1 unless exists $opts{force_timestamp};
    $opts{remove_backup} = 1 unless exists $opts{remove_backup};
    $opts{create_dirs} = 1 unless exists $opts{create_dirs};
    $opts{options} ||= [ '-t', '-F', '0', '-N', '-p1', '-u',
            '-V', 'never', '-g0', '-b', '-z', '.dpkg-orig'];
    $opts{add_options} ||= [];
    push @{$opts{options}}, @{$opts{add_options}};
    # Check the diff and create missing directories
    my $analysis = $self->analyze($destdir, %opts);
    $self->prepare_apply($analysis, %opts);
    # Apply the patch
    $self->ensure_open('r');
    my ($stdout, $stderr) = ('', '');
    spawn(
	exec => [ 'patch', @{$opts{options}} ],
	chdir => $destdir,
	env => { LC_ALL => 'C', LANG => 'C' },
	delete_env => [ 'POSIXLY_CORRECT' ], # ensure expected patch behaviour
	wait_child => 1,
	nocheck => 1,
	from_handle => $self->get_filehandle(),
	to_string => \$stdout,
	error_to_string => \$stderr,
    );
    if ($?) {
	print { *STDOUT } $stdout;
	print { *STDERR } $stderr;
	subprocerr('LC_ALL=C patch ' . join(' ', @{$opts{options}}) .
	           ' < ' . $self->get_filename());
    }
    $self->close();
    # Reset the timestamp of all the patched files
    # and remove .dpkg-orig files
    my @files = keys %{$analysis->{filepatched}};
    my $now = $opts{timestamp};
    $now ||= fs_time($files[0]) if $opts{force_timestamp} && scalar @files;
    foreach my $fn (@files) {
	if ($opts{force_timestamp}) {
	    utime($now, $now, $fn) or $! == ENOENT
		or syserr(_g('cannot change timestamp for %s'), $fn);
	}
	if ($opts{remove_backup}) {
	    $fn .= '.dpkg-orig';
	    unlink($fn) or syserr(_g('remove patch backup file %s'), $fn);
	}
    }
    return $analysis;
}

# Verify if check will work...
sub check_apply {
    my ($self, $destdir, %opts) = @_;
    # Set default values to options
    $opts{create_dirs} = 1 unless exists $opts{create_dirs};
    $opts{options} ||= [ '--dry-run', '-s', '-t', '-F', '0', '-N', '-p1', '-u',
            '-V', 'never', '-g0', '-b', '-z', '.dpkg-orig'];
    $opts{add_options} ||= [];
    push @{$opts{options}}, @{$opts{add_options}};
    # Check the diff and create missing directories
    my $analysis = $self->analyze($destdir, %opts);
    $self->prepare_apply($analysis, %opts);
    # Apply the patch
    $self->ensure_open('r');
    my $patch_pid = spawn(
	exec => [ 'patch', @{$opts{options}} ],
	chdir => $destdir,
	env => { LC_ALL => 'C', LANG => 'C' },
	delete_env => [ 'POSIXLY_CORRECT' ], # ensure expected patch behaviour
	from_handle => $self->get_filehandle(),
	to_file => '/dev/null',
	error_to_file => '/dev/null',
    );
    wait_child($patch_pid, nocheck => 1);
    my $exit = WEXITSTATUS($?);
    subprocerr('patch --dry-run') unless WIFEXITED($?);
    $self->close();
    return ($exit == 0);
}

# Helper functions
sub get_type {
    my $file = shift;
    if (not lstat($file)) {
        return _g('nonexistent') if $! == ENOENT;
        syserr(_g('cannot stat %s'), $file);
    } else {
        -f _ && return _g('plain file');
        -d _ && return _g('directory');
        -l _ && return sprintf(_g('symlink to %s'), readlink($file));
        -b _ && return _g('block device');
        -c _ && return _g('character device');
        -p _ && return _g('named pipe');
        -S _ && return _g('named socket');
    }
}

1;
