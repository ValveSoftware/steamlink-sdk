
MAME4ALL 2.7 - Source Code (30/april/2012)
------------------------------------------

http://franxis.zxq.net/

Development Environment:
- GCC 4.0.2: DevKitGP2X rc2 (http://sourceforge.net/project/showfiles.php?group_id=114505)

This port is based on MAME 0.37b5.

Thanks and regards.

- Franxis (franxism@gmail.com)

COMPILE EXECUTABLES FOR THE GP2X
--------------------------------

make -f makefile.gp2x
make -f makefile.gp2x TARGET=neomame
make -f makefile.gp2x mame.gpe

COMPILE EXECUTABLES FOR THE WIZ
-------------------------------

make -f makefile.wiz
make -f makefile.wiz TARGET=neomame
make -f makefile.wiz mame.gpe

To be able to compile MAME4ALL for the WIZ, two files of the DevKitGP2X distribution have to be modified manually:

devkitGP2X\sysroot\usr\lib\libm.so:
/* GNU ld script
   Use the shared library, but some functions are only in
   the static library, so try that secondarily.  */
OUTPUT_FORMAT(elf32-littlearm)
GROUP ( /lib/libm.so.6 )

devkitGP2X\sysroot\usr\lib\libpthread.so:
/* GNU ld script
   Use the shared library, but some functions are only in
   the static library, so try that secondarily.  */
OUTPUT_FORMAT(elf32-littlearm)
GROUP ( /lib/libpthread.so.0 )

COMPILE EXECUTABLES FOR THE CAANOO
----------------------------------

Use Sourcery G++ Lite 2006q1-6 (http://www.codesourcery.com/sgpp/lite/arm/portal/release293)

make -f makefile.caanoo
make -f makefile.caanoo TARGET=neomame
make -f makefile.caanoo mame.gpe

MAME LICENSE
------------

http://www.mame.net
http://www.mamedev.com

Copyright © 1997-2009, Nicola Salmoria and the MAME team. All rights reserved. 

Redistribution and use of this code or any derivative works are permitted provided
that the following conditions are met: 

* Redistributions may not be sold, nor may they be used in a commercial product or activity. 

* Redistributions that are modified from the original source must include the complete source
code, including the source code for all components used by a binary built from the modified
sources. However, as a special exception, the source code distributed need not include 
anything that is normally distributed (in either source or binary form) with the major 
components (compiler, kernel, and so on) of the operating system on which the executable
runs, unless that component itself accompanies the executable. 

* Redistributions must reproduce the above copyright notice, this list of conditions and the
following disclaimer in the documentation and/or other materials provided with the distribution. 

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR 
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY 
AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR 
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT 
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
