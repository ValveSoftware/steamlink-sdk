MAME4ALL for Pi by Squid

INTRODUCTION

This is a MAME Raspberry Pi port based on Franxis MAME4ALL which is itself based on the MAME 0.37b5 emulator by Nicola Salmoria.
To see MAME license see the end of this document.
It emulates all arcade games supported by original MAME 0.37b5 plus some additional games from newer MAME versions.

This version emulates 2270 different romsets.

Although this is an old version of MAME it plays much faster than the newer versions and as the Pi is relatively CPU underpowered (yes even the RPi2) it was chosen to get as many games working at full speed as possible (full speed means 100% with no frame skip). It also plays most of the games I'm interested in playing!

This is a highly optimised version for the Raspberry Pi, using GLES2/dispmanx for graphics, ALSA for sound and SDL for input. It also uses the GPU for post-processing effects like scanlines.

Pi Store version is here:
http://store.raspberrypi.com/

Web page for downloads, news, source, additional information:
https://sourceforge.net/projects/mame4allpi/

(No asking for ROMS)


CONTROLS

These are the standard MAME key definitions as follows. All MAME keys are also available.
  * Keys LControl, LAlt, Space, LShift are the fire buttons
  * Cursors Keys for up, down, left and right
  * Keys 1 & 2 to start 1 or 2 player games
  * Keys 5 & 6 Insert credits for 1P or 2P
  * Key Escape to quit
  * Key TAB to bring up the MAME menu
  * Function Keys: F11 show fps, F10 toggle throttle, F5 cheats, Shift F11 show profiler
  * Key P to pause

NOTE: To type OK when MAME requires it with the joystick, press LEFT and then RIGHT.


INSTALLATION
For the Pi Store version place the ROMS in the directory:
/usr/local/bin/indiecity/InstalledApps/mame4all/Full/roms/

mame        -> MAME and frontend.
mame.cfg    -> MAME configuration file, limited support to only the options in the supplied file (not the full MAME settings).
cheat.dat   -> Cheats definition file
hiscore.dat -> High Scores definition file
artwork/    -> Artwork directory
cfg/        -> MAME configuration files directory
frontend/   -> Frontend configuration files
hi/         -> High Scores directory
inp/        -> Game recordings directory
memcard/    -> Memory card files directory
nvram/      -> NVRAM files directory
roms/       -> ROMs directory
samples/    -> Samples directory
skins/      -> Frontend skins directory
snap/       -> Screen snapshots directory
sta/        -> Save states directory

To run MAME simple run the "mame" executable. At the command line "./mame".
This runs the GUI frontend by default. To simply run MAME without the GUI
enter "./mame {gamerom}" where "{gamerom}" is the game to run.

It will work in X-Windows or in the Console (which is preferred).


Pi CONFIGURATION

I highly recommend overclocking your Raspberry Pi to gain maximum performance as MAME is very CPU intensive and overclocking will make most games run at full speed. The Pi 2 does not require overclocking.

Overclocking is supported by the Raspberry Foundation.

My overclocking settings which work well, (/boot/config.txt)
arm_freq=900
core_freq=300
sdram_freq=500

NOTE: Make sure overclocking is actually working by checking "cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor" should be "ondemand". Later kernels appear to set it to "powersave" by default. You will also need to make this permanent after a reboot.

I'd also recommend a minimum of 64MB for the GPU RAM allocation (gpu_mem=64).

If your sound is too quiet then do the following to fix that:
First get the playback device, type "amixer controls"
This will show the numid for the playback device, probably 3.
Now set the volume, type "amixer cset numid=3 90%".
Then reboot to make it permanent.

If you're having problems with HDMI audio then it is likely PulseAudio
is causing the issues as it has problems with the ALSA drivers. To fix
this simply remove PulseAudio:
sudo apt-get --purge remove pulseaudio
sudo apt-get autoremove

Additionally some TVs have problems with mono sound through HDMI, to fix this try setting the "force_stereo" to "yes" in mame.cfg.

If you're getting a black screen when running in Console mode with
Composite output, try removing/commenting out the "overscan_" parameters from "/boot/config.txt" as follows (using disable_overscan doesn't appear to fix it):
#overscan_left=16
#overscan_right=16
#overscan_top=16
#overscan_bottom=16

If the image goes off your TV screen then you can change the border width by setting "display_border" in mame.cfg. 

GRAPHICS CONFIGURATION

To switch off anti-aliasing drawing set the "display_smooth_stretch" to "no" in the mame.cfg. Performance can be impacted depending on the size of your monitor's resolution when switching off anti-aliasing. The "antialias" setting in mame.cfg is only for vector drawing.

Postprocessing scanline drawing is available, the "display_effect" in the mame.cfg controls this. Scanlines work best when "display_smooth_stretch" is set to "no".


SUPPORTED GAMES

Folder names or ZIP file names are listed on "gamelist.txt" file.
Romsets have to be MAME 0.37b5 ones (July 2000).
Additionaly there are additional romsets from newer MAME versions.

Please use "clrmame.dat" file to convert romsets from other MAME versions to the ones used by this version, using ClrMAME Pro utility, available in next webpage:

http://mamedev.emulab.it/clrmamepro/

NOTE: File and directory names in Linux are case-sensitive. Put all file and directory names using lower case!


SOUND SAMPLES

The sound samples are used to get complete sound in some of the oldest games.
They are placed into the 'samples' directory compressed into ZIP files.
The directory and the ZIP files are named using low case!

The sound samples collection can be downloaded in the following link:
http://dl.openhandhelds.org/cgi-bin/gp2x.cgi?0,0,0,0,5,2511

You can also use "clrmame.dat" file with ClrMAME Pro utility to get the samples pack.


ARTWORK

Artwork is used to improve the visualization for some of the oldest games. Download it here:
http://dl.openhandhelds.org/cgi-bin/gp2x.cgi?0,0,0,0,5,2512


ORIGINAL CREDITS

  * MAME 0.37b5 original version by Nicola Salmoria and the MAME Team (http://www.mame.net).
  * Z80 emulator Copyright (c) 1998 Juergen Buchmueller, all rights reserved.
  * M6502 emulator Copyright (c) 1998 Juergen Buchmueller, all rights reserved.
  * Hu6280 Copyright (c) 1999 Bryan McPhail, mish@tendril.force9.net
  * I86 emulator by David Hedley, modified by Fabrice Frances (frances@ensica.fr)
  * M6809 emulator by John Butler, based on L.C. Benschop's 6809 Simulator V09.
  * M6808 based on L.C. Benschop's 6809 Simulator V09.
  * M68000 emulator Copyright 1999 Karl Stenerud.  All rights reserved.
  * 80x86 M68000 emulator Copyright 1998, Mike Coates, Darren Olafson.
  * 8039 emulator by Mirko Buffoni, based on 8048 emulator by Dan Boris.
  * T-11 emulator Copyright (C) Aaron Giles 1998
  * TMS34010 emulator by Alex Pasadyn and Zsolt Vasvari.
  * TMS9900 emulator by Andy Jones, based on original code by Ton Brouwer.
  * Cinematronics CPU emulator by Jeff Mitchell, Zonn Moore, Neil Bradley.
  * Atari AVG/DVG emulation based on VECSIM by Hedley Rainnie, Eric Smith and Al Kossow.
  * TMS5220 emulator by Frank Palazzolo.
  * AY-3-8910 emulation based on various code snippets by Ville Hallik, Michael Cuddy, Tatsuyuki Satoh, Fabrice Frances, Nicola Salmoria.
  * YM-2203, YM-2151, YM3812 emulation by Tatsuyuki Satoh.
  * POKEY emulator by Ron Fries (rfries@aol.com). Many thanks to Eric Smith, Hedley Rainnie and Sean Trowbridge.
  * NES sound hardware info by Jeremy Chadwick and Hedley Rainne.
  * YM2610 emulation by Hiromitsu Shioya.


PORT CREDITS

  * Ported to Raspberry Pi by Squid.
  * Original MAM4ALL port to GP2X, WIZ and CAANOO by Franxis (franxism@gmail.com) based on source code MAME 0.37b5 (dated on july 2000).
  * ALSA sound code is based on code from RetroArch (http://themaister.net/retroarch.html)


CHANGE LOG

February 11, 2015:
  * Increased joystick axis detection.
  * Joystick controls default to XBOX 360 controller.

September 02, 2013:
  * Supports more joystick axis and up to 16 joystick buttons.
  * Frontend keys and joystick controls now configurable.
  * Joystick START+SELECT now quits frontend.
  * Vector games no longer display scanlines.
  * Flushes filesystem for hiscore and input saves.

June 07, 2013:
  * New graphics engine backend has options for non-antialised graphics, scanlines and 16bit colour support.
  * VSync support is much better eliminating stutter on full speed games.
  * New graphics backend may mean some 8bit games are faster, but 16bit games maybe slower.
  * Improvements to the sound should eliminate static on full speed games.

May 08, 2013:
  * Fix for "roms" configuration not read in the frontend.
  * Added graphics options to the config file for display border, display stretch disable.

April 10, 2013:
  * Added Mouse support (joysticks override mouse).
  * Fixed kiosk mode.
  * Fixed shinobi crashing (disabled drz80 audio for the rom).

April 06, 2013:
  * HDMI audio option to force stereo for TVs that don't support mono properly. Configured using the "force_stereo" in mame.cfg.

April 01, 2013:
  * Display ROM errors and invalid games messages.

March 26, 2013:
  * Added favourites selection, press Coin 1 (or joystick button 6) to toggle favourites. File Favorites.ini goes in folders directory.

March 20, 2013:
  * Reduced or eliminated sound static.
  * Fixed the sound in the galaxian group of games.
  * Fixed Neogeo crackling sound.
  * Workaround for SDL incompatible keyboards detected as joysticks.
  * Added key delay to frontend for easier selection.
  * Added "kiosk" mode option, stops MAME4ALL exiting from the game selection frontend.

March 16, 2013:
  * Fixed the funky colour palette problems.
  * Now supports ten (previously six) joystick buttons.
  * Added configuration file (mame.cfg) support.

March 12, 2013:
  * Added multiple USB joystick support.
  * Full keyboard support.
  * Added DrZ80 cpu core. Enabled by default for sound to improve performance for many games.
  * Fixed "slow" sound in dkong, williams and rampage games.
  * Frontend GUI now uses 16bit bitmaps. 
  * Slightly higher sound mixer quality.

March 01, 2013:
  * First release

TODO

  * Add better rotation support.

KNOWN PROBLEMS

  * Not perfect sound or incomplete in some games. Sometimes simply quiting a game and restarting can fix the sound - believe this is due to ALSA Pi driver bugs. Additionally try removing pulseaudio to improve sound problems.
  * Make sure nothing is running in the background. For best performance run in the console instead of X-Windows. But if experiencing black screen in the console, try running in X-Windows or use the console overscan settings above.
  * The SDL input drivers are a little buggy, if input suddenly stops, reboot the Pi.
  * If a game crashes either during play or on exit try disabling the DrZ80 core (-nodrz80_snd) on the command line.
  * Slow playability in modern games. Most games run at full speed on an overclocked Pi.
  * Memory leaks. In case of errors/crashes reboot your Pi.

THANKS TO

tecboy for testing and patience!

INTERESTING WEBPAGES ABOUT MAME

  * http://www.mame.net/
  * http://www.mameworld.net/

SKINS

The frontend graphic skin used in the emulator is located in these files:

  * skins/rpisplash16.bmp   -> Intro screen
  * skins/rpimenu16.bmp     -> Screen to select games

Bitmaps have to be 640x480 pixels 65536 colors (16 bit R5G6B5). 

MAME LICENSE

http://www.mame.net

http://www.mamedev.com

Copyright 1997-2015, Nicola Salmoria and the MAME team. All rights reserved.

Redistribution and use of this code or any derivative works are permitted provided
that the following conditions are met:

Redistributions may not be sold, nor may they be used in a commercial product or activity.

Redistributions that are modified from the original source must include the complete source code, including the source code for all components used by a binary built from the modified sources. However, as a special exception, the source code distributed need not include anything that is normally distributed (in either source or binary form) with the major components (compiler, kernel, and so on) of the operating system on which the executable runs, unless that component itself accompanies the executable.

Redistributions must reproduce the above copyright notice, this list of conditions and the
following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
