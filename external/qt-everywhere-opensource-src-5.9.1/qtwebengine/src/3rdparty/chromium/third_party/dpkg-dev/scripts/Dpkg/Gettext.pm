# Copied from /usr/share/perl5/Debconf/Gettext.pm
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY AUTHORS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

package Dpkg::Gettext;

use strict;
use warnings;

our $VERSION = '1.00';

BEGIN {
    eval 'use Locale::gettext';
    if ($@) {
        eval q{
            sub _g {
                return shift;
            }
            sub textdomain {
            }
            sub ngettext {
                if ($_[2] == 1) {
                    return $_[0];
                } else {
                    return $_[1];
                }
            }
            sub P_ {
                return ngettext(@_);
            }
        };
    } else {
        eval q{
            sub _g {
                return gettext(shift);
            }
            sub P_ {
                return ngettext(@_);
            }
        };
    }
}

use Exporter qw(import);
our @EXPORT=qw(_g P_ textdomain ngettext);

1;
