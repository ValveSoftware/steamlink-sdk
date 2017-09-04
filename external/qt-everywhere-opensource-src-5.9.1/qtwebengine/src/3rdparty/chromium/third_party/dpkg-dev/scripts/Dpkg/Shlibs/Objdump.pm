# Copyright © 2007-2010 Raphaël Hertzog <hertzog@debian.org>
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

package Dpkg::Shlibs::Objdump;

use strict;
use warnings;

use Dpkg::Gettext;
use Dpkg::ErrorHandling;
use Dpkg::Path qw(find_command);
use Dpkg::Arch qw(debarch_to_gnutriplet get_build_arch get_host_arch);
use Dpkg::IPC;

our $VERSION = '0.01';

# Decide which objdump to call
our $OBJDUMP = 'objdump';
if (get_build_arch() ne get_host_arch()) {
    my $od = debarch_to_gnutriplet(get_host_arch()) . '-objdump';
    $OBJDUMP = $od if find_command($od);
}


sub new {
    my $this = shift;
    my $class = ref($this) || $this;
    my $self = { objects => {} };
    bless $self, $class;
    return $self;
}

sub add_object {
    my ($self, $obj) = @_;
    my $id = $obj->get_id;
    if ($id) {
	$self->{objects}{$id} = $obj;
    }
    return $id;
}

sub analyze {
    my ($self, $file) = @_;
    my $obj = Dpkg::Shlibs::Objdump::Object->new($file);

    return $self->add_object($obj);
}

sub locate_symbol {
    my ($self, $name) = @_;
    foreach my $obj (values %{$self->{objects}}) {
	my $sym = $obj->get_symbol($name);
	if (defined($sym) && $sym->{defined}) {
	    return $sym;
	}
    }
    return;
}

sub get_object {
    my ($self, $objid) = @_;
    if ($self->has_object($objid)) {
	return $self->{objects}{$objid};
    }
    return;
}

sub has_object {
    my ($self, $objid) = @_;
    return exists $self->{objects}{$objid};
}

sub is_armhf {
    my ($file) = @_;
    my ($output, %opts, $pid, $res);
    my $hf = 0;
    my $sf = 0;
    $pid = spawn(exec => [ "readelf", "-h", "--", $file ],
		 env => { "LC_ALL" => "C" },
		 to_pipe => \$output, %opts);
    while (<$output>) {
	chomp;
	if (/0x5000402/) {
	    $hf = 1;
	    last;
	}
	if (/0x5000202/) {
	    $sf = 1;
	    last;
	}
    }
    close($output);
    wait_child($pid, nocheck => 1);
    if ($?) {
	subprocerr("readelf");
    }
    if(($hf) || ($sf)) {
	return $hf;
    }
    undef $output;
    $pid = spawn(exec => [ "readelf", "-A", "--", $file ],
		 env => { "LC_ALL" => "C" },
		 to_pipe => \$output, %opts);
    while (<$output>) {
	chomp;
	if (/Tag_ABI_VFP_args: VFP registers/) {
	    $hf = 1;
	    last;
	}
    }
    close($output);
    wait_child($pid, nocheck => 1);
    if ($?) {
	subprocerr("readelf");
    }
    return $hf;
}

{
    my %format; # Cache of result
    sub get_format {
	my ($file, $objdump) = @_;

	$objdump //= $OBJDUMP;

	if (exists $format{$file}) {
	    return $format{$file};
	} else {
	    my ($output, %opts, $pid, $res);
	    if ($objdump ne 'objdump') {
		$opts{error_to_file} = '/dev/null';
	    }
	    $pid = spawn(exec => [ $objdump, '-a', '--', $file ],
			 env => { LC_ALL => 'C' },
			 to_pipe => \$output, %opts);
	    while (<$output>) {
		chomp;
		if (/^\s*\S+:\s*file\s+format\s+(\S+)\s*$/) {
		    $format{$file} = $1;
		    $res = $format{$file};
		    last;
		}
	    }
	    close($output);
	    wait_child($pid, nocheck => 1);
	    if ($?) {
		subprocerr('objdump') if $objdump eq 'objdump';
		$res = get_format($file, 'objdump');
	    }
	    if ($res eq "elf32-littlearm") {
		if (is_armhf($file)) {
		    $res = "elf32-littlearm-hfabi";
		} else {
		    $res = "elf32-littlearm-sfabi";
		}
		$format{$file} = $res;
	    }
	    return $res;
	}
    }
}

sub is_elf {
    my ($file) = @_;
    open(my $file_fh, '<', $file) or syserr(_g('cannot read %s'), $file);
    my ($header, $result) = ('', 0);
    if (read($file_fh, $header, 4) == 4) {
	$result = 1 if ($header =~ /^\177ELF$/);
    }
    close($file_fh);
    return $result;
}

package Dpkg::Shlibs::Objdump::Object;

use Dpkg::Gettext;
use Dpkg::ErrorHandling;

sub new {
    my $this = shift;
    my $file = shift || '';
    my $class = ref($this) || $this;
    my $self = {};
    bless $self, $class;

    $self->reset;
    if ($file) {
	$self->analyze($file);
    }

    return $self;
}

sub reset {
    my ($self) = @_;

    $self->{file} = '';
    $self->{id} = '';
    $self->{SONAME} = '';
    $self->{HASH} = '';
    $self->{GNU_HASH} = '';
    $self->{SONAME} = '';
    $self->{NEEDED} = [];
    $self->{RPATH} = [];
    $self->{dynsyms} = {};
    $self->{flags} = {};
    $self->{dynrelocs} = {};

    return $self;
}


sub analyze {
    my ($self, $file) = @_;

    $file ||= $self->{file};
    return unless $file;

    $self->reset;
    $self->{file} = $file;

    local $ENV{LC_ALL} = 'C';
    open(my $objdump, '-|', $OBJDUMP, '-w', '-f', '-p', '-T', '-R', $file)
        or syserr(_g('cannot fork for %s'), $OBJDUMP);
    my $ret = $self->parse_objdump_output($objdump);
    close($objdump);
    return $ret;
}

sub parse_objdump_output {
    my ($self, $fh) = @_;

    my $section = 'none';
    while (defined($_ = <$fh>)) {
	chomp;
	next if /^\s*$/;

	if (/^DYNAMIC SYMBOL TABLE:/) {
	    $section = 'dynsym';
	    next;
	} elsif (/^DYNAMIC RELOCATION RECORDS/) {
	    $section = 'dynreloc';
	    $_ = <$fh>; # Skip header
	    next;
	} elsif (/^Dynamic Section:/) {
	    $section = 'dyninfo';
	    next;
	} elsif (/^Program Header:/) {
	    $section = 'header';
	    next;
	} elsif (/^Version definitions:/) {
	    $section = 'verdef';
	    next;
	} elsif (/^Version References:/) {
	    $section = 'verref';
	    next;
	}

	if ($section eq 'dynsym') {
	    $self->parse_dynamic_symbol($_);
	} elsif ($section eq 'dynreloc') {
	    if (/^\S+\s+(\S+)\s+(\S+)\s*$/) {
		$self->{dynrelocs}{$2} = $1;
	    } else {
		warning(_g("couldn't parse dynamic relocation record: %s"), $_);
	    }
	} elsif ($section eq 'dyninfo') {
	    if (/^\s*NEEDED\s+(\S+)/) {
		push @{$self->{NEEDED}}, $1;
	    } elsif (/^\s*SONAME\s+(\S+)/) {
		$self->{SONAME} = $1;
	    } elsif (/^\s*HASH\s+(\S+)/) {
		$self->{HASH} = $1;
	    } elsif (/^\s*GNU_HASH\s+(\S+)/) {
		$self->{GNU_HASH} = $1;
	    } elsif (/^\s*RUNPATH\s+(\S+)/) {
                # RUNPATH takes precedence over RPATH but is
                # considered after LD_LIBRARY_PATH while RPATH
                # is considered before (if RUNPATH is not set).
                $self->{RPATH} = [ split (/:/, $1) ];
	    } elsif (/^\s*RPATH\s+(\S+)/) {
                unless (scalar(@{$self->{RPATH}})) {
                    $self->{RPATH} = [ split (/:/, $1) ];
                }
	    }
	} elsif ($section eq 'none') {
	    if (/^\s*.+:\s*file\s+format\s+(\S+)\s*$/) {
		$self->{format} = $1;
		if (($self->{format} eq "elf32-littlearm") && $self->{file}) {
		    if (Dpkg::Shlibs::Objdump::is_armhf($self->{file})) {
			$self->{format} = "elf32-littlearm-hfabi";
		    } else {
			$self->{format} = "elf32-littlearm-sfabi";
		    }
		}
	    } elsif (/^architecture:\s*\S+,\s*flags\s*\S+:\s*$/) {
		# Parse 2 lines of "-f"
		# architecture: i386, flags 0x00000112:
		# EXEC_P, HAS_SYMS, D_PAGED
		# start address 0x08049b50
		$_ = <$fh>;
		chomp;
		$self->{flags}{$_} = 1 foreach (split(/,\s*/));
	    }
	}
    }
    # Update status of dynamic symbols given the relocations that have
    # been parsed after the symbols...
    $self->apply_relocations();

    return $section ne 'none';
}

# Output format of objdump -w -T
#
# /lib/libc.so.6:     file format elf32-i386
#
# DYNAMIC SYMBOL TABLE:
# 00056ef0 g    DF .text  000000db  GLIBC_2.2   getwchar
# 00000000 g    DO *ABS*  00000000  GCC_3.0     GCC_3.0
# 00069960  w   DF .text  0000001e  GLIBC_2.0   bcmp
# 00000000  w   D  *UND*  00000000              _pthread_cleanup_pop_restore
# 0000b788 g    DF .text  0000008e  Base        .protected xine_close
# 0000b788 g    DF .text  0000008e              .hidden IA__g_free
# |        ||||||| |      |         |           |
# |        ||||||| |      |         Version str (.visibility) + Symbol name
# |        ||||||| |      Alignment
# |        ||||||| Section name (or *UND* for an undefined symbol)
# |        ||||||F=Function,f=file,O=object
# |        |||||d=debugging,D=dynamic
# |        ||||I=Indirect
# |        |||W=warning
# |        ||C=constructor
# |        |w=weak
# |        g=global,l=local,!=both global/local
# Size of the symbol
#
# GLIBC_2.2 is the version string associated to the symbol
# (GLIBC_2.2) is the same but the symbol is hidden, a newer version of the
# symbol exist

sub parse_dynamic_symbol {
    my ($self, $line) = @_;
    my $vis_re = '(\.protected|\.hidden|\.internal|0x\S+)';
    if ($line =~ /^[0-9a-f]+ (.{7})\s+(\S+)\s+[0-9a-f]+(?:\s+(\S+))?(?:\s+$vis_re)?\s+(\S+)/) {

	my ($flags, $sect, $ver, $vis, $name) = ($1, $2, $3, $4, $5);

	# Special case if version is missing but extra visibility
	# attribute replaces it in the match
	if (defined($ver) and $ver =~ /^$vis_re$/) {
	    $vis = $ver;
	    $ver = '';
	}

	# Cleanup visibility field
	$vis =~ s/^\.// if defined($vis);

	my $symbol = {
		name => $name,
		version => defined($ver) ? $ver : '',
		section => $sect,
		dynamic => substr($flags, 5, 1) eq 'D',
		debug => substr($flags, 5, 1) eq 'd',
		type => substr($flags, 6, 1),
		weak => substr($flags, 1, 1) eq 'w',
		local => substr($flags, 0, 1) eq 'l',
		global => substr($flags, 0, 1) eq 'g',
		visibility => defined($vis) ? $vis : '',
		hidden => '',
		defined => $sect ne '*UND*'
	    };

	# Handle hidden symbols
	if (defined($ver) and $ver =~ /^\((.*)\)$/) {
	    $ver = $1;
	    $symbol->{version} = $1;
	    $symbol->{hidden} = 1;
	}

	# Register symbol
	$self->add_dynamic_symbol($symbol);
    } elsif ($line =~ /^[0-9a-f]+ (.{7})\s+(\S+)\s+[0-9a-f]+/) {
	# Same start but no version and no symbol ... just ignore
    } elsif ($line =~ /^REG_G\d+\s+/) {
	# Ignore some s390-specific output like
	# REG_G6           g     R *UND*      0000000000000000              #scratch
    } else {
	warning(_g("couldn't parse dynamic symbol definition: %s"), $line);
    }
}

sub apply_relocations {
    my ($self) = @_;
    foreach my $sym (values %{$self->{dynsyms}}) {
	# We want to mark as undefined symbols those which are currently
	# defined but that depend on a copy relocation
	next if not $sym->{defined};
	next if not exists $self->{dynrelocs}{$sym->{name}};
	if ($self->{dynrelocs}{$sym->{name}} =~ /^R_.*_COPY$/) {
	    $sym->{defined} = 0;
	}
    }
}

sub add_dynamic_symbol {
    my ($self, $symbol) = @_;
    $symbol->{objid} = $symbol->{soname} = $self->get_id();
    $symbol->{soname} =~ s{^.*/}{} unless $self->{SONAME};
    if ($symbol->{version}) {
	$self->{dynsyms}{$symbol->{name} . '@' . $symbol->{version}} = $symbol;
    } else {
	$self->{dynsyms}{$symbol->{name} . '@Base'} = $symbol;
    }
}

sub get_id {
    my $self = shift;
    return $self->{SONAME} || $self->{file};
}

sub get_symbol {
    my ($self, $name) = @_;
    if (exists $self->{dynsyms}{$name}) {
	return $self->{dynsyms}{$name};
    }
    if ($name !~ /@/) {
        if (exists $self->{dynsyms}{$name . '@Base'}) {
            return $self->{dynsyms}{$name . '@Base'};
        }
    }
    return;
}

sub get_exported_dynamic_symbols {
    my ($self) = @_;
    return grep { $_->{defined} && $_->{dynamic} && !$_->{local} }
	    values %{$self->{dynsyms}};
}

sub get_undefined_dynamic_symbols {
    my ($self) = @_;
    return grep { (!$_->{defined}) && $_->{dynamic} }
	    values %{$self->{dynsyms}};
}

sub get_needed_libraries {
    my $self = shift;
    return @{$self->{NEEDED}};
}

sub is_executable {
    my $self = shift;
    return exists $self->{flags}{EXEC_P} && $self->{flags}{EXEC_P};
}

sub is_public_library {
    my $self = shift;
    return exists $self->{flags}{DYNAMIC} && $self->{flags}{DYNAMIC}
	&& exists $self->{SONAME} && $self->{SONAME};
}

1;
