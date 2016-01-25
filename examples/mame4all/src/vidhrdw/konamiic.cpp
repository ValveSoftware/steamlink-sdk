#define VERBOSE 0

/***************************************************************************

TODO:
- implement shadows properly
- in Aliens shadows should be disabled (tubes at the beginning of the game
  have a vertical line which is supposed to be white)
- understand global Y position for the 053247
- understand how the 051316 positioning works (probably just an external timing thing)



                      Emulated
                         |
                  board #|year    CPU      tiles        sprites  priority palette    other
                    -----|---- ------- ------------- ------------- ------ ------ ----------------
Hyper Crash         GX401 1985
Twinbee             GX412*1985   68000           GX400
Yie Ar Kung Fu      GX407*1985    6809
Gradius / Nemesis   GX456*1985   68000           GX400
Shao-lins Road      GX477*1985    6809
Jail Break          GX507*1986 KONAMI-1          005849
Finalizer           GX523*1985 KONAMI-1          005885
Konami's Ping Pong  GX555*1985     Z80
Iron Horse          GX560*1986    6809           005885
Konami GT           GX561*1985   68000           GX400
Green Beret         GX577*1985     Z80           005849
Galactic Warriors   GX578*1985   68000           GX400
Salamander          GX587*1986   68000           GX400
WEC Le Mans 24      GX602*1986 2x68000
BAW / Black Panther GX604 1987   68000           GX400                    007593
Combat School /     GX611*1987    6309           007121(x2)               007327
  Boot Camp
Rock 'n Rage /      GX620*1986    6309 007342        007420               007327
  Koi no Hotrock
Mr Kabuki/Mr Goemon GX621*1986     Z80           005849
Jackal              GX631*1986    6809?          005885(x2)
Contra / Gryzor     GX633*1987    6809?          007121(x2)               007593
Flak Attack         GX669*1987    6309           007121                   007327 007452
Devil World / Dark  GX687*1987 2x68000           TWIN16
  Adventure / Majuu no Oukoku
Double Dribble      GX690*1986  3x6809           005885(x2)               007327 007452
Kitten Kaboodle /   GX712+1988                   GX400                    007593 051550
  Nyan Nyan Panic
Chequered Flag      GX717*1988  052001               051960 051937(x2)           051316(x2) (zoom/rotation) 051733 (protection)
Fast Lane           GX752*1987    6309           007121                          051733 (protection) 007801
Hot Chase           GX763*1988 2x68000                                           051316(x3) (zoom/rotation) 007634 007635 007558 007557
Rack 'Em Up /       GX765*1987    6309 007342        007420               007327 007324
  The Hustler
Haunted Castle      GX768*1988  052001           007121(x2)               007327
Ajax / Typhoon      GX770*1987   6309+ 052109 051962 051960 051937  PROM  007327 051316 (zoom/rotation)
                                052001
Labyrinth Runner    GX771*1987    6309           007121                   007593 051733 (protection) 051550
Super Contra        GX775*1988  052001 052109 051962 051960 051937  PROM  007327
Battlantis          GX777*1987    6309 007342        007420               007327 007324
Vulcan Venture /    GX785*1988 2x68000           TWIN16
  Gradius 2
City Bomber         GX787+1987   68000           GX400                    007593 051550
Over Drive          GX789 1990
Hyper Crash         GX790 1987
Blades of Steel     GX797*1987    6309 007342        007420               007327 051733 (protection)
The Main Event      GX799*1988    6309 052109 051962 051960 051937  PROM
Missing in Action   GX808*1989   68000 052109 051962 051960 051937  PROM
Missing in Action J GX808*1989 2x68000           TWIN16
Crime Fighters      GX821*1989  052526 052109 051962 051960 051937  PROM
Special Project Y   GX857*1989    6309 052109 051962 051960 051937  PROM         052591 (protection)
'88 Games           GX861*1988  052001 052109 051962 051960 051937  PROM         051316 (zoom/rotation)
Final Round /       GX870*1988 1x68000           TWIN16?
  Hard Puncher
Thunder Cross       GX873*1988  052001 052109 051962 051960 051937  PROM  007327 052591 (protection)
Aliens              GX875*1990  052526 052109 051962 051960 051937  PROM
Gang Busters        GX878*1988  052526 052109 051962 051960 051937  PROM
Devastators         GX890*1988    6309 052109 051962 051960 051937  PROM         007324 051733 (protection)
Bottom of the Ninth GX891*1989    6809 052109 051962 051960 051937  PROM         051316 (zoom/rotation)
Cue Brick           GX903*1989 2x68000           TWIN16
Punk Shot           GX907*1990   68000 052109 051962 051960 051937 053251
Ultraman            GX910*1991   68000 ------ ------ 051960 051937  PROM         051316(x3) (zoom/rotation) 051550
Surprise Attack     GX911*1990  053248 052109 051962 053245 053244 053251
Lightning Fighters /GX939*1990   68000 052109 051962 053245 053244 053251
  Trigon
Gradius 3           GX945*1989 2x68000 052109 051962 051960 051937  PROM
Parodius            GX955*1990  053248 052109 051962 053245 053244 053251
TMNT                GX963*1989   68000 052109 051962 051960 051937  PROM
Block Hole          GX973*1989  052526 052109 051962 051960 051937  PROM
Escape Kids         GX975 1991  053248 052109 051962 053247 053246 053251        053252 - same board as Vendetta
Rollergames         GX999*1991  053248 ------ ------ 053245 053244               051316 (zoom/rotation) 053252
Bells & Whistles /  GX060*1991   68000 052109 051962 053245 053244 053251        054000 (collision)
  Detana!! Twin Bee
Golfing Greats      GX061*1991   68000 052109 051962 053245 053244 053251        053936 (3D)
TMNT 2              GX063*1991   68000 052109 051962 053245 053244 053251        053990
Sunset Riders       GX064*1991   68000 052109 051962 053245 053244 053251        054358
X-Men               GX065*1992   68000 052109 051962 053247 053246 053251
XEXEX               GX067*1991   68000 054157 054156 053247 053246 053251        054338 054539
Asterix             GX068+1992   68000 054157 054156 053245 053244 053251        054358
G.I. Joe            GX069+1992   68000 054157 054156 053247 053246 053251        054539
The Simpsons        GX072*1991  053248 052109 051962 053247 053246 053251
Thunder Cross 2     GX073*1991   68000 052109 051962 051960 051937 053251        054000 (collision)
Vendetta /          GX081*1991  053248 052109 051962 053247 053246 053251        054000 (collision)
  Crime Fighters 2
Premier Soccer      GX101 1993   68000 052109 051962 053245 053244 053251        053936 (3D)
Hexion              GX122+1992     Z80                                           052591 (protection) 053252
Entapous /          GX123+1993   68000 054157 054156 055673 053246               053252 054000 055555
  Gaiapolis
Mystic Warrior      GX128 1993
Cowboys of Moo Mesa GX151 1993   68000 054157 054156 053247 053246               053252 054338 053990
Violent Storm       GX168+1993   68000 054157 054156 055673 053246               054338 054539(x2) 055550 055555
Bucky 'O Hare       GX173+1992   68000 054157 054156 053247 053246 053251        054338 054539
Potrio              GX174 1992
Lethal Enforcers    GX191 1992    6309 054157(x2) 054156 053245 053244(x2)       054000 054539 054906
Metamorphic Force   GX224 1993
Martial Champion    GX234+1993   68000 054157 054156 055673 053246               053252 054338 054539 055555 053990 054986 054573
Run and Gun         GX247+1993   68000               055673 053246               053253(x2)
Polygonet CommandersGX305 1993   68020                                           056230?063936?054539?054986?


Notes:
- Old games use 051961 instead of 052109, it is an earlier version functionally
  equivalent (maybe 052109 had bugs fixed). The list always shows 052109 because
  the two are exchangeable and 052109's are found also on original boards whose
  schematics show a 051961.



Status of the ROM tests in the emulated games:

Ajax / Typhoon      pass
Super Contra        pass
The Main Event      pass
Missing in Action   pass
Crime Fighters      pass
Special Project Y   pass
Konami 88           pass
Thunder Cross       pass
Aliens              pass
Gang Busters        pass
Devastators         pass
Bottom of the Ninth pass
Punk Shot           pass
Surprise Attack     fails D05-6 (052109) because it uses mirror addresses to
                    select banks, and supporting those addresses breaks the
                    normal game ;-(
Lightning Fighters  pass
Gradius 3           pass
Parodius            pass
TMNT                pass
Block Hole          pass
Rollergames         pass
Bells & Whistles    pass
Golfing Greats      fails B05..B10 (053936)
TMNT 2              pass
Sunset Riders       pass
X-Men               fails 1F (054544)
The Simpsons        pass
Thunder Cross 2     pass
Vendetta            pass
Xexex               pass


THE FOLLOWING INFORMATION IS PRELIMINARY AND INACCURATE. DON'T RELY ON IT.


007121
------
This is an interesting beast. Many games use two of these in pair.
It manages sprites and two 32x32 tilemaps. The tilemaps can be joined to form
a single 64x32 one, or one of them can be moved to the side of screen, giving
a high score display suitable for vertical games.
The chip also generates clock and interrupt signals suitable for a 6809.
It uses 0x2000 bytes of RAM for the tilemaps and sprites, and an additional
0x100 bytes, maybe for scroll RAM and line buffers. The maximum addressable
ROM is 0x80000 bytes (addressed 16 bits at a time).
Two 256x4 lookup PROMs are also used to increase the color combinations.
All tilemap / sprite priority handling is done internally and the chip exports
7 bits of color code, composed of 2 bits of palette bank, 1 bit indicating tile
or sprite, and 4 bits of ROM data remapped through the PROM.

inputs:
- address lines (A0-A13)
- data lines (DB0-DB7)
- misc interface stuff
- data from the gfx ROMs (RDL0-RDL7, RDU0-RDU7)
- data from the tile lookup PROMs (VCD0-VCD3)
- data from the sprite lookup PROMs (OCD0-OCD3)

outputs:
- address lines for tilemap RAM (AX0-AX12)
- data lines for tilemap RAM (VO0-VO7)
- address lines for the small RAM (FA0-FA7)
- data lines for the small RAM (FD0-FD7)
- address lines for the gfx ROMs (R0-R17)
- address lines for the tile lookup PROMs (VCF0-VCF3, VCB0-VCB3)
- address lines for the sprite lookup PROMs (OCB0-OCB3, OCF0-OCF3)
- NNMI, NIRQ, NFIR, NE, NQ for the main CPU
- misc interface stuff
- color code to be output on screen (COA0-COA6)


control registers
000:          scroll x (low 8 bits)
001: -------x scroll x (high bit)
     ------x- enable rowscroll? (combasc)
     ----x--- this probably selects an alternate screen layout used in combat
              school where tilemap #2 is overlayed on front and doesn't scroll.
              The 32 lines of the front layer can be individually turned on or
              off using the second 32 bytes of scroll RAM.
002:          scroll y
003: -------x bit 13 of the tile code
     ------x- unknown (contra)
     -----x-- might be sprite / tilemap priority (0 = sprites have priority)
              (combat school, contra, haunted castle(0/1), labyrunr)
     ----x--- selects sprite buffer (and makes a copy to a private buffer?)
     ---x---- screen layout selector:
              when this is set, 5 columns are added on the left of the screen
              (that means 5 rows at the top for vertical games), and the
              rightmost 2 columns are chopped away.
              Tilemap #2 is used to display the 5 additional columns on the
              left. The rest of tilemap #2 is not used and can be used as work
              RAM by the program.
              The visible area becomes 280x224.
              Note that labyrunr changes this at runtime, setting it during
              gameplay and resetting it on the title screen and crosshatch.
     --x----- might be sprite / tilemap priority (0 = sprites have priority)
              (combat school, contra, haunted castle(0/1), labyrunr)
     -x------ Chops away the leftmost and rightmost columns, switching the
              visible area from 256 to 240 pixels. This is used by combasc on
              the scrolling stages, and by labyrunr on the title screen.
              At first I thought that this enabled an extra bank of 0x40
              sprites, needed by combasc, but labyrunr proves that this is not
              the case
     x------- unknown (contra)
004: ----xxxx bits 9-12 of the tile code. Only the bits enabled by the following
              mask are actually used, and replace the ones selected by register
			  005.
     xxxx---- mask enabling the above bits
005: selects where in the attribute byte to pick bits 9-12 of the tile code,
     output to pins R12-R15. The bit of the attribute byte to use is the
     specified bit (0-3) + 3, that is one of bits 3-6. Bit 7 is hardcoded as
     bit 8 of the code. Bits 0-2 are used for the color, however note that
     some games (combat school, flak attack, maybe fast lane) use bit 3 as well,
     and indeed there are 4 lines going to the color lookup PROM, so there has
     to be a way to select this.
     ------xx attribute bit to use for tile code bit  9
     ----xx-- attribute bit to use for tile code bit 10
     --xx---- attribute bit to use for tile code bit 11
     xx------ attribute bit to use for tile code bit 12
006: ----xxxx select additional effect for bits 3-6 of the tile attribute (the
              same ones indexed by register 005). Note that an attribute bit
              can therefore be used at the same time to be BOTH a tile code bit
              and an additional effect.
     -------x bit 3 of attribute is bit 3 of color (combasc, fastlane, flkatck)
	 ------x- bit 4 of attribute is tile flip X (assumption - no game uses this)
     -----x-- bit 5 of attribute is tile flip Y (flkatck)
     ----x--- bit 6 of attribute is tile priority over sprites (combasc, hcastle,
              labyrunr)
              Note that hcastle sets this bit for layer 0, and bit 6 of the
              attribute is also used as bit 12 of the tile code, however that
              bit is ALWAYS set thoughout the game.
              combasc uses the bit inthe "graduation" scene during attract mode,
              to place soldiers behind the stand.
              Use in labyrunr has not been investigated yet.
     --xx---- palette bank (both tiles and sprites, see contra)
007: -------x nmi enable
     ------x- irq enable
     -----x-- firq enable (probably)
     ----x--- flip screen
     ---x---- unknown (contra, labyrunr)



007342
------
The 007342 manages 2 64x32 scrolling tilemaps with 8x8 characters, and
optionally generates timing clocks and interrupt signals. It uses 0x2000
bytes of RAM, plus 0x0200 bytes for scrolling, and a variable amount of ROM.
It cannot read the ROMs.

control registers
000: ------x- INT control
     ---x---- flip screen (TODO: doesn't work with thehustl)
001: Used for banking in Rock'n'Rage
002: -------x MSB of x scroll 1
     ------x- MSB of x scroll 2
     ---xxx-- layer 1 row/column scroll control
              000 = disabled
			  010 = unknown (bladestl shootout between periods)
              011 = 32 columns (Blades of Steel)
              101 = 256 rows (Battlantis, Rock 'n Rage)
     x------- enable sprite wraparound from bottom to top (see Blades of Steel
              high score table)
003: x scroll 1
004: y scroll 1
005: x scroll 2
006: y scroll 2
007: not used


007420
------
Sprite generator. 8 bytes per sprite with zoom. It uses 0x200 bytes of RAM,
and a variable amount of ROM. Nothing is known about its external interface.



052109/051962
-------------
These work in pair.
The 052109 manages 3 64x32 scrolling tilemaps with 8x8 characters, and
optionally generates timing clocks and interrupt signals. It uses 0x4000
bytes of RAM, and a variable amount of ROM. It cannot read the ROMs:
instead, it exports 21 bits (16 from the tilemap RAM + 3 for the character
raster line + 2 additional ones for ROM banking) and these are externally
used to generate the address of the required data on the ROM; the output of
the ROMs is sent to the 051962, along with a color code. In theory you could
have any combination of bits in the tilemap RAM, as long as they add to 16.
In practice, all the games supported so far standardize on the same format
which uses 3 bits for the color code and 13 bits for the character code.
The 051962 multiplexes the data of the three layers and converts it into
palette indexes and transparency bits which will be mixed later in the video
chain.
Priority is handled externally: these chips only generate the tilemaps, they
don't mix them.
Both chips are interfaced with the main CPU. When the RMRD pin is asserted,
the CPU can read the gfx ROM data. This is done by telling the 052109 which
dword to read (this is a combination of some banking registers, and the CPU
address lines), and then reading it from the 051962.

052109 inputs:
- address lines (AB0-AB15, AB13-AB15 seem to have a different function)
- data lines (DB0-DB7)
- misc interface stuff

052109 outputs:
- address lines for the private RAM (RA0-RA12)
- data lines for the private RAM (VD0-VD15)
- NMI, IRQ, FIRQ for the main CPU
- misc interface stuff
- ROM bank selector (CAB1-CAB2)
- character "code" (VC0-VC10)
- character "color" (COL0-COL7); used foc color but also bank switching and tile
  flipping. Exact meaning depends on externl connections. All evidence indicates
  that COL2 and COL3 select the tile bank, and are replaced with the low 2 bits
  from the bank register. The top 2 bits of the register go to CAB1-CAB2.
  However, this DOES NOT WORK with Gradius III. "color" seems to pass through
  unaltered.
- layer A horizontal scroll (ZA1H-ZA4H)
- layer B horizontal scroll (ZB1H-ZB4H)
- ????? (BEN)

051962 inputs:
- gfx data from the ROMs (VC0-VC31)
- color code (COL0-COL7); only COL4-COL7 seem to really be used for color; COL0
  is tile flip X.
- layer A horizontal scroll (ZA1H-ZA4H)
- layer B horizontal scroll (ZB1H-ZB4H)
- let main CPU read the gfx ROMs (RMRD)
- address lines to be used with RMRD (AB0-AB1)
- data lines to be used with RMRD (DB0-DB7)
- ????? (BEN)
- misc interface stuff

051962 outputs:
- FIX layer palette index (DFI0-DFI7)
- FIX layer transparency (NFIC)
- A layer palette index (DSA0-DSAD); DSAA-DSAD seem to be unused
- A layer transparency (NSAC)
- B layer palette index (DSB0-DSBD); DSBA-DSBD seem to be unused
- B layer transparency (NSBC)
- misc interface stuff


052109 memory layout:
0000-07ff: layer FIX tilemap (attributes)
0800-0fff: layer A tilemap (attributes)
1000-1fff: layer B tilemap (attributes)
180c-1833: A y scroll
1a00-1bff: A x scroll
1c00     : ?
1c80     : row/column scroll control
           ------xx layer A row scroll
                    00 = disabled
                    01 = disabled? (gradius3, vendetta)
                    10 = 32 lines
                    11 = 256 lines
           -----x-- layer A column scroll
                    0 = disabled
                    1 = 64 (actually 40) columns
           ---xx--- layer B row scroll
           --x----- layer B column scroll
           surpratk sets this register to 70 during the second boss. There is
           nothing obviously wrong so it's not clear what should happen.
1d00     : bits 0 & 1 might enable NMI and FIRQ, not sure
         : bit 2 = IRQ enable
1d80     : ROM bank selector bits 0-3 = bank 0 bits 4-7 = bank 1
1e00     : ROM subbank selector for ROM testing
1e80     : bit 0 = flip screen (applies to tilemaps only, not sprites)
         : bit 1 = set by crimfght, mainevt, surpratk, xmen, mia, punkshot, thndrx2, spy
         :         it seems to enable tile flip X, however flip X is handled by the
         :         051962 and it is not hardwired to a specific tile attribute.
         :         Note that xmen, punkshot and thndrx2 set the bit but the current
         :         drivers don't use flip X and seem to work fine.
         : bit 2 = enables tile flip Y when bit 1 of the tile attribute is set
1f00     : ROM bank selector bits 0-3 = bank 2 bits 4-7 = bank 3
2000-27ff: layer FIX tilemap (code)
2800-2fff: layer A tilemap (code)
3000-37ff: layer B tilemap (code)
3800-3807: nothing here, so the chip can share address space with a 051937
380c-3833: B y scroll
3a00-3bff: B x scroll
3c00-3fff: nothing here, so the chip can share address space with a 051960
3d80     : mirror of 1d80, but ONLY during ROM test (surpratk)
3e00     : mirror of 1e00, but ONLY during ROM test (surpratk)
3f00     : mirror of 1f00, but ONLY during ROM test (surpratk)
EXTRA ADDRESSING SPACE USED BY X-MEN:
4000-47ff: layer FIX tilemap (code high bits)
4800-4fff: layer A tilemap (code high bits)
5000-57ff: layer B tilemap (code high bits)

The main CPU doesn't have direct acces to the RAM used by the 052109, it has
to through the chip.



051960/051937
-------------
Sprite generators. Designed to work in pair. The 051960 manages the sprite
list and produces and address that is fed to the gfx ROMs. The data from the
ROMs is sent to the 051937, along with color code and other stuff from the
051960. The 051937 outputs up to 12 bits of palette index, plus "shadow" and
transparency information.
Both chips are interfaced to the main CPU, through 8-bit data buses and 11
bits of address space. The 051937 sits in the range 000-007, while the 051960
in the range 400-7ff (all RAM). The main CPU can read the gfx ROM data though
the 051937 data bus, while the 051960 provides the address lines.
The 051960 is designed to directly address 1MB of ROM space, since it produces
18 address lines that go to two 16-bit wide ROMs (the 051937 has a 32-bit data
bus to the ROMs). However, the addressing space can be increased by using one
or more of the "color attribute" bits of the sprites as bank selectors.
Moreover a few games store the gfx data in the ROMs in a format different from
the one expected by the 051960, and use external logic to reorder the address
lines.
The 051960 can also genenrate IRQ, FIRQ and NMI signals.

memory map:
000-007 is for the 051937, but also seen by the 051960
400-7ff is 051960 only
000     R  bit 0 = unknown, looks like a status flag or something
                   aliens waits for it to be 0 before starting to copy sprite data
				   thndrx2 needs it to pulse for the startup checks to succeed
000     W  bit 0 = irq enable/acknowledge?
           bit 3 = flip screen (applies to sprites only, not tilemaps)
           bit 4 = unknown, used by Devastators, TMNT, Aliens, Chequered Flag, maybe others
                   aliens sets it just after checking bit 0, and before copying
                   the sprite data
           bit 5 = enable gfx ROM reading
001     W  Devastators sets bit 1, function unknown, could it be a global shadow enable?
           None of the other games I tested seem to set this register to other than 0.
002-003 W  selects the portion of the gfx ROMs to be read.
004     W  Aliens uses this to select the ROM bank to be read, but Punk Shot
           and TMNT don't, they use another bit of the registers above. Many
           other games write to this register before testing.
           It is possible that bits 2-7 of 003 go to OC0-OC5, and bits 0-1 of
           004 go to OC6-OC7.
004-007 R  reads data from the gfx ROMs (32 bits in total). The address of the
           data is determined by the register above and by the last address
           accessed on the 051960; plus bank switch bits for larger ROMs.
           It seems that the data can also be read directly from the 051960
           address space: 88 Games does this. First it reads 004 and discards
           the result, then it reads from the 051960 the data at the address
           it wants. The normal order is the opposite, read from the 051960 at
           the address you want, discard the result, and fetch the data from
           004-007.
400-7ff RW sprite RAM, 8 bytes per sprite



053245/053244
-------------
Sprite generators. The 053245 has a 16-bit data bus to the main CPU.

053244 memory map (but the 053245 sees and processes them too):
000-001 W  global X offset
002-003 W  global Y offset
004     W  unknown
005     W  bit 0 = flip screen X
           bit 1 = flip screen Y
           bit 2 = unknown, used by Parodius
           bit 4 = enable gfx ROM reading
           bit 5 = unknown, used by Rollergames
006     W  unknown
007     W  unknown
008-009 W  low 16 bits of the ROM address to read
00a-00b W  high 3 bits of the ROM address to read
00c-00f R  reads data from the gfx ROMs (32 bits in total). The address of the
           data is determined by the registers above; plus bank switch bits for
           larger ROMs.



053247/053246
-------------
Sprite generators. Nothing is known about their external interface.
The sprite RAM format is very similar to the 053245.

053246 memory map (but the 053247 sees and processes them too):
000-001 W  global X offset
002-003 W  global Y offset. TODO: it is not clear how this works, we use a hack
004     W  low 8 bits of the ROM address to read
005     W  bit 0 = flip screen X
           bit 1 = flip screen Y
           bit 2 = unknown
           bit 4 = interrupt enable
           bit 5 = unknown
006-007 W  high 16 bits of the ROM address to read

???-??? R  reads data from the gfx ROMs (16 bits in total). The address of the
           data is determined by the registers above



051316
------
Manages a 32x32 tilemap (16x16 tiles, 512x512 pixels) which can be zoomed,
distorted and rotated.
It uses two internal 24 bit counters which are incremented while scanning the
picture. The coordinates of the pixel in the tilemap that has to be drawn to
the current beam position are the counters / (2^11).
The chip doesn't directly generate the color information for the pixel, it
just generates a 24 bit address (whose top 16 bits are the contents of the
tilemap RAM), and a "visible" signal. It's up to external circuitry to convert
the address into a pixel color. Most games seem to use 4bpp graphics, but Ajax
uses 7bpp.
If the value in the internal counters is out of the visible range (0..511), it
is truncated and the corresponding address is still generated, but the "visible"
signal is not asserted. The external circuitry might ignore that signal and
still generate the pixel, therefore making the tilemap a continuous playfield
that wraps around instead of a large sprite.


control registers
000-001 X counter starting value / 256
002-003 amount to add to the X counter after each horizontal pixel
004-005 amount to add to the X counter after each line (0 = no rotation)
006-007 Y counter starting value / 256
008-009 amount to add to the Y counter after each horizontal pixel (0 = no rotation)
00a-00b amount to add to the Y counter after each line
00c-00d ROM bank to read, used during ROM testing
00e     bit 0 = enable ROM reading (active low). This only makes the chip output the
                requested address: the ROM is actually read externally, not through
                the chip's data bus.
        bit 1 = unknown
        bit 2 = unknown
00f     unused



053251
------
Priority encoder.

The chip has inputs for 5 layers (CI0-CI4); only 4 are used (CI1-CI4)
CI0-CI2 are 9(=5+4) bits inputs, CI3-CI4 8(=4+4) bits

The input connctions change from game to game. E.g. in Simpsons,
CI0 = grounded (background color)
CI1 = sprites
CI2 = FIX
CI3 = A
CI4 = B

in lgtnfght:
CI0 = grounded
CI1 = sprites
CI2 = FIX
CI3 = B
CI4 = A

there are three 6 bit priority inputs, PR0-PR2

simpsons:
PR0 = 111111
PR1 = xxxxx0 x bits coming from the sprite attributes
PR2 = 111111

lgtnfght:
PR0 = 111111
PR1 = 1xx000 x bits coming from the sprite attributes
PR2 = 111111

also two shadow inputs, SDI0 and SDI1 (from the sprite attributes)

the chip outputs the 11 bit palette index, CO0-CO10, and two shadow bits.

16 internal registers; registers are 6 bits wide (input is D0-D5)
For the most part, their meaning is unknown
All registers are write only.
There must be a way to enable/disable the three external PR inputs.
Some games initialize the priorities of the sprite & background layers,
others don't. It isn't clear whether the data written to those registers is
actually used, since the priority is taken from the external ports.

 0  priority of CI0 (higher = lower priority)
    punkshot: unused?
    lgtnfght: unused?
    simpsons: 3f = 111111
    xmen:     05 = 000101  default value
    xmen:     09 = 001001  used to swap CI0 and CI2
 1  priority of CI1 (higher = lower priority)
    punkshot: 28 = 101000
    lgtnfght: unused?
    simpsons: unused?
    xmen:     02 = 000010
 2  priority of CI2 (higher = lower priority)
    punkshot: 24 = 100100
    lgtnfght: 24 = 100100
    simpsons: 04 = 000100
    xmen:     09 = 001001  default value
    xmen:     05 = 000101  used to swap CI0 and CI2
 3  priority of CI3 (higher = lower priority)
    punkshot: 34 = 110100
    lgtnfght: 34 = 110100
    simpsons: 28 = 101000
    xmen:     00 = 000000
 4  priority of CI4 (higher = lower priority)
    punkshot: 2c = 101100  default value
    punkshot: 3c = 111100  used to swap CI3 and CI4
    punkshot: 26 = 100110  used to swap CI1 and CI4
    lgtnfght: 2c = 101100
    simpsons: 18 = 011000
    xmen:     fe = 111110
 5  unknown
    punkshot: unused?
    lgtnfght: 2a = 101010
    simpsons: unused?
    xmen: unused?
 6  unknown
    punkshot: 26 = 100110
    lgtnfght: 30 = 110000
    simpsons: 17 = 010111
    xmen:     03 = 000011 (written after initial tests)
 7  unknown
    punkshot: unused?
    lgtnfght: unused?
    simpsons: 27 = 100111
    xmen:     07 = 000111 (written after initial tests)
 8  unknown
    punkshot: unused?
    lgtnfght: unused?
    simpsons: 37 = 110111
    xmen:     ff = 111111 (written after initial tests)
 9  ----xx CI0 palette index base (CO9-CO10)
    --xx-- CI1 palette index base (CO9-CO10)
    xx---- CI2 palette index base (CO9-CO10)
10  ---xxx CI3 palette index base (CO8-CO10)
    xxx--- CI4 palette index base (CO8-CO10)
11  unknown
    punkshot: 00 = 000000
    lgtnfght: 00 = 000000
    simpsons: 00 = 000000
    xmen:     00 = 000000 (written after initial tests)
12  unknown
    punkshot: 04 = 000100
    lgtnfght: 04 = 000100
    simpsons: 05 = 000101
    xmen:     05 = 000101
13  unused
14  unused
15  unused


054000
------
Sort of a protection device, used for collision detection.
It is passed a few parameters, and returns a boolean telling if collision
happened. It has no access to gfx data, it only does arithmetical operations
on the parameters.

Memory map:
00      unused
01-03 W A center X
04    W unknown, needed by thndrx2 to pass the startup check, we use a hack
05      unused
06    W A semiaxis X
07    W A semiaxis Y
08      unused
09-0b W A center Y
0c    W unknown, needed by thndrx2 to pass the startup check, we use a hack
0d      unused
0e    W B semiaxis X
0f    W B semiaxis Y
10      unused
11-13 W B center Y
14      unused
15-17 W B center X
18    R 0 = collision, 1 = no collision


051733
------
Sort of a protection device, used for collision detection, and for
arithmetical operations.
It is passed a few parameters, and returns the result.

Memory map(preliminary):
------------------------
00-01 W operand 1
02-03 W operand 2

00-01 R operand 1/operand 2
02-03 R operand 1%operand 2?

06-07 W Radius
08-09 W Y pos of obj1
0a-0b W X pos of obj1
0c-0d W Y pos of obj2
0e-0f W X pos of obj2
13	  W unknown

07	  R collision (0x80 = no, 0x00 = yes)

Other addresses are unknown or unused.

Fast Lane:
----------
$9def:
This routine is called only after a collision.
(R) 0x0006:	unknown. Only bits 0-3 are used.

Blades of Steel:
----------------
$ac2f:
(R) 0x2f86: unknown. Only uses bit 0.

$a5de:
writes to 0x2f84-0x2f85, waits a little, and then reads from 0x2f84.

$7af3:
(R) 0x2f86: unknown. Only uses bit 0.


Devastators:
------------
$6ce8:
reads from 0x0006, and only uses bit 1.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/konamiic.h"


/*
	This recursive function doesn't use additional memory
	(it could be easily converted into an iterative one).
	It's called shuffle because it mimics the shuffling of a deck of cards.
*/
static void shuffle(UINT16 *buf,int len)
{
	int i;
	UINT16 t;

	if (len == 2) return;

	if (len % 4) exit(1);   /* must not happen */

	len /= 2;

	for (i = 0;i < len/2;i++)
	{
		t = buf[len/2 + i];
		buf[len/2 + i] = buf[len + i];
		buf[len + i] = t;
	}

	shuffle(buf,len);
	shuffle(buf + len,len);
}


/* helper function to join two 16-bit ROMs and form a 32-bit data stream */
void konami_rom_deinterleave_2(int mem_region)
{
	shuffle((UINT16 *)memory_region(mem_region),memory_region_length(mem_region)/2);
}

/* helper function to join four 16-bit ROMs and form a 64-bit data stream */
void konami_rom_deinterleave_4(int mem_region)
{
	konami_rom_deinterleave_2(mem_region);
	konami_rom_deinterleave_2(mem_region);
}








/*#define MAX_K007121 2*/

/*static*/ unsigned char K007121_ctrlram[MAX_K007121][8];
static int K007121_flipscreen[MAX_K007121];


void K007121_ctrl_w(int chip,int offset,int data)
{
	switch (offset)
	{
		case 6:
/* palette bank change */
if ((K007121_ctrlram[chip][offset] & 0x30) != (data & 0x30))
	tilemap_mark_all_tiles_dirty(ALL_TILEMAPS);
			break;
		case 7:
			K007121_flipscreen[chip] = data & 0x08;
			break;
	}

	K007121_ctrlram[chip][offset] = data;
}

WRITE_HANDLER( K007121_ctrl_0_w )
{
	K007121_ctrl_w(0,offset,data);
}

WRITE_HANDLER( K007121_ctrl_1_w )
{
	K007121_ctrl_w(1,offset,data);
}


/*
 * Sprite Format
 * ------------------
 *
 * There are 0x40 sprites, each one using 5 bytes. However the number of
 * sprites can be increased to 0x80 with a control register (Combat School
 * sets it on and off during the game).
 *
 * Byte | Bit(s)   | Use
 * -----+-76543210-+----------------
 *   0  | xxxxxxxx | sprite code
 *   1  | xxxx---- | color
 *   1  | ----xx-- | sprite code low 2 bits for 16x8/8x8 sprites
 *   1  | ------xx | sprite code bank bits 1/0
 *   2  | xxxxxxxx | y position
 *   3  | xxxxxxxx | x position (low 8 bits)
 *   4  | xx------ | sprite code bank bits 3/2
 *   4  | --x----- | flip y
 *   4  | ---x---- | flip x
 *   4  | ----xxx- | sprite size 000=16x16 001=16x8 010=8x16 011=8x8 100=32x32
 *   4  | -------x | x position (high bit)
 *
 * Flack Attack uses a different, "wider" layout with 32 bytes per sprites,
 * mapped as follows, and the priority order is reversed. Maybe it is a
 * compatibility mode with an older custom IC. It is not known how this
 * alternate layout is selected.
 *
 * 0 -> e
 * 1 -> f
 * 2 -> 6
 * 3 -> 4
 * 4 -> 8
 *
 */

void K007121_sprites_draw(int chip,struct osd_bitmap *bitmap,
		const unsigned char *source,int base_color,int global_x_offset,int bank_base,
		UINT32 pri_mask)
{
	const struct GfxElement *gfx = Machine->gfx[chip];
	int flipscreen = K007121_flipscreen[chip];
	int i,num,inc,offs[5],trans;
	int is_flakatck = K007121_ctrlram[chip][0x06] & 0x04;	/* WRONG!!!! */

#if 0
usrintf_showmessage("%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x  %02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x",
	K007121_ctrlram[0][0x00],K007121_ctrlram[0][0x01],K007121_ctrlram[0][0x02],K007121_ctrlram[0][0x03],K007121_ctrlram[0][0x04],K007121_ctrlram[0][0x05],K007121_ctrlram[0][0x06],K007121_ctrlram[0][0x07],
	K007121_ctrlram[1][0x00],K007121_ctrlram[1][0x01],K007121_ctrlram[1][0x02],K007121_ctrlram[1][0x03],K007121_ctrlram[1][0x04],K007121_ctrlram[1][0x05],K007121_ctrlram[1][0x06],K007121_ctrlram[1][0x07]);
#endif
#if 0
if (keyboard_pressed(KEYCODE_D))
{
	FILE *fp;
	fp=fopen(chip?"SPRITE1.DMP":"SPRITE0.DMP", "w+b");
	if (fp)
	{
		fwrite(source, 0x800, 1, fp);
		usrintf_showmessage("saved");
		fclose(fp);
	}
}
#endif

	if (is_flakatck)
	{
		num = 0x40;
		inc = -0x20;
		source += 0x3f*0x20;
		offs[0] = 0x0e;
		offs[1] = 0x0f;
		offs[2] = 0x06;
		offs[3] = 0x04;
		offs[4] = 0x08;
		/* Flak Attack doesn't use a lookup PROM, it maps the color code directly */
		/* to a palette entry */
		trans = TRANSPARENCY_PEN;
	}
	else	/* all others */
	{
		num = (K007121_ctrlram[chip][0x03] & 0x40) ? 0x80 : 0x40;	/* WRONG!!! (needed by combasc)  */
		inc = 5;
		offs[0] = 0x00;
		offs[1] = 0x01;
		offs[2] = 0x02;
		offs[3] = 0x03;
		offs[4] = 0x04;
		trans = TRANSPARENCY_COLOR;
		/* when using priority buffer, draw front to back */
		if (pri_mask != -1)
		{
			source += (num-1)*inc;
			inc = -inc;
		}
	}

	for (i = 0;i < num;i++)
	{
		int number = source[offs[0]];				/* sprite number */
		int sprite_bank = source[offs[1]] & 0x0f;	/* sprite bank */
		int sx = source[offs[3]];					/* vertical position */
		int sy = source[offs[2]];					/* horizontal position */
		int attr = source[offs[4]];				/* attributes */
		int xflip = source[offs[4]] & 0x10;		/* flip x */
		int yflip = source[offs[4]] & 0x20;		/* flip y */
		int color = base_color + ((source[offs[1]] & 0xf0) >> 4);
		int width,height;
		static int x_offset[4] = {0x0,0x1,0x4,0x5};
		static int y_offset[4] = {0x0,0x2,0x8,0xa};
		int x,y, ex, ey;

		if (attr & 0x01) sx -= 256;
		if (sy >= 240) sy -= 256;

		number += ((sprite_bank & 0x3) << 8) + ((attr & 0xc0) << 4);
		number = number << 2;
		number += (sprite_bank >> 2) & 3;

		if (!is_flakatck || source[0x00])	/* Flak Attack needs this */
		{
			number += bank_base;

			switch( attr&0xe )
			{
				case 0x06: width = height = 1; break;
				case 0x04: width = 1; height = 2; number &= (~2); break;
				case 0x02: width = 2; height = 1; number &= (~1); break;
				case 0x00: width = height = 2; number &= (~3); break;
				case 0x08: width = height = 4; number &= (~3); break;
				default: width = 1; height = 1;
//					logerror("Unknown sprite size %02x\n",attr&0xe);
//					usrintf_showmessage("Unknown sprite size %02x\n",attr&0xe);
			}

			for (y = 0;y < height;y++)
			{
				for (x = 0;x < width;x++)
				{
					ex = xflip ? (width-1-x) : x;
					ey = yflip ? (height-1-y) : y;

					if (flipscreen)
					{
						if (pri_mask != -1)
							pdrawgfx(bitmap,gfx,
								number + x_offset[ex] + y_offset[ey],
								color,
								!xflip,!yflip,
								248-(sx+x*8),248-(sy+y*8),
								&Machine->visible_area,trans,0,
								pri_mask);
						else
							drawgfx(bitmap,gfx,
								number + x_offset[ex] + y_offset[ey],
								color,
								!xflip,!yflip,
								248-(sx+x*8),248-(sy+y*8),
								&Machine->visible_area,trans,0);
					}
					else
					{
						if (pri_mask != -1)
							pdrawgfx(bitmap,gfx,
								number + x_offset[ex] + y_offset[ey],
								color,
								xflip,yflip,
								global_x_offset+sx+x*8,sy+y*8,
								&Machine->visible_area,trans,0,
								pri_mask);
						else
							drawgfx(bitmap,gfx,
								number + x_offset[ex] + y_offset[ey],
								color,
								xflip,yflip,
								global_x_offset+sx+x*8,sy+y*8,
								&Machine->visible_area,trans,0);
					}
				}
			}
		}

		source += inc;
	}
}

void K007121_mark_sprites_colors(int chip,
		const unsigned char *source,int base_color,int bank_base)
{
	int i,num,inc,offs[5];
	int is_flakatck = K007121_ctrlram[chip][0x06] & 0x04;	/* WRONG!!!! */

	unsigned short palette_map[512];

	if (is_flakatck)
	{
		num = 0x40;
		inc = -0x20;
		source += 0x3f*0x20;
		offs[0] = 0x0e;
		offs[1] = 0x0f;
		offs[2] = 0x06;
		offs[3] = 0x04;
		offs[4] = 0x08;
	}
	else	/* all others */
	{
		num = (K007121_ctrlram[chip][0x03] & 0x40) ? 0x80 : 0x40;
		inc = 5;
		offs[0] = 0x00;
		offs[1] = 0x01;
		offs[2] = 0x02;
		offs[3] = 0x03;
		offs[4] = 0x04;
	}

	memset (palette_map, 0, sizeof (palette_map));

	/* sprites */
	for (i = 0;i < num;i++)
	{
		int color;

		color = base_color + ((source[offs[1]] & 0xf0) >> 4);
		palette_map[color] |= 0xffff;

		source += inc;
	}

	/* now build the final table */
	for (i = 0; i < 512; i++)
	{
		int usage = palette_map[i], j;
		if (usage)
		{
			for (j = 0; j < 16; j++)
				if (usage & (1 << j))
					palette_used_colors[i * 16 + j] |= PALETTE_COLOR_VISIBLE;
		}
	}
}







static unsigned char *K007342_ram,*K007342_scroll_ram;
static int K007342_gfxnum;
static int K007342_int_enabled;
static int K007342_flipscreen;
static int K007342_scrollx[2];
static int K007342_scrolly[2];
static unsigned char *K007342_videoram_0,*K007342_colorram_0;
static unsigned char *K007342_videoram_1,*K007342_colorram_1;
static int K007342_regs[8];
static void (*K007342_callback)(int tilemap, int bank, int *code, int *color);
static struct tilemap *K007342_tilemap[2];

/***************************************************************************

  Callbacks for the TileMap code

***************************************************************************/

/*
  data format:
  video RAM     xxxxxxxx    tile number (bits 0-7)
  color RAM     x-------    tiles with priority over the sprites
  color RAM     -x------    depends on external conections
  color RAM     --x-----    flip Y
  color RAM     ---x----    flip X
  color RAM     ----xxxx    depends on external connections (usually color and banking)
*/

static unsigned char *colorram,*videoram1,*videoram2;
static int layer;

static void tilemap_0_preupdate(void)
{
	colorram = K007342_colorram_0;
	videoram1 = K007342_videoram_0;
	layer = 0;
}

static void tilemap_1_preupdate(void)
{
	colorram = K007342_colorram_1;
	videoram1 = K007342_videoram_1;
	layer = 1;
}

static UINT32 K007342_scan(UINT32 col,UINT32 row,UINT32 num_cols,UINT32 num_rows)
{
	/* logical (col,row) -> memory offset */
	return (col & 0x1f) + ((row & 0x1f) << 5) + ((col & 0x20) << 5);
}

static void K007342_get_tile_info(int tile_index)
{
	int color, code;

	color = colorram[tile_index];
	code = videoram1[tile_index];

	tile_info.flags = TILE_FLIPYX((color & 0x30) >> 4);
	tile_info.priority = (color & 0x80) >> 7;

	(*K007342_callback)(layer, K007342_regs[1], &code, &color);

	SET_TILE_INFO(K007342_gfxnum,code,color);
}

int K007342_vh_start(int gfx_index, void (*callback)(int tilemap, int bank, int *code, int *color))
{
	K007342_gfxnum = gfx_index;
	K007342_callback = callback;

	K007342_tilemap[0] = tilemap_create(K007342_get_tile_info,K007342_scan,TILEMAP_TRANSPARENT,8,8,64,32);
	K007342_tilemap[1] = tilemap_create(K007342_get_tile_info,K007342_scan,TILEMAP_TRANSPARENT,8,8,64,32);

	K007342_ram = (unsigned char*)malloc(0x2000);
	K007342_scroll_ram = (unsigned char*)malloc(0x0200);

	if (!K007342_ram || !K007342_scroll_ram || !K007342_tilemap[0] || !K007342_tilemap[1])
	{
		K007342_vh_stop();
		return 1;
	}

	memset(K007342_ram,0,0x2000);

	K007342_colorram_0 = &K007342_ram[0x0000];
	K007342_colorram_1 = &K007342_ram[0x1000];
	K007342_videoram_0 = &K007342_ram[0x0800];
	K007342_videoram_1 = &K007342_ram[0x1800];

	K007342_tilemap[0]->transparent_pen = 0;
	K007342_tilemap[1]->transparent_pen = 0;

	return 0;
}

void K007342_vh_stop(void)
{
	free(K007342_ram);
	K007342_ram = 0;
	free(K007342_scroll_ram);
	K007342_scroll_ram = 0;
}

READ_HANDLER( K007342_r )
{
	return K007342_ram[offset];
}

WRITE_HANDLER( K007342_w )
{
	if (offset < 0x1000)
	{		/* layer 0 */
		if (K007342_ram[offset] != data)
		{
			K007342_ram[offset] = data;
			tilemap_mark_tile_dirty(K007342_tilemap[0],offset & 0x7ff);
		}
	}
	else
	{						/* layer 1 */
		if (K007342_ram[offset] != data)
		{
			K007342_ram[offset] = data;
			tilemap_mark_tile_dirty(K007342_tilemap[1],offset & 0x7ff);
		}
	}
}

READ_HANDLER( K007342_scroll_r )
{
	return K007342_scroll_ram[offset];
}

WRITE_HANDLER( K007342_scroll_w )
{
	K007342_scroll_ram[offset] = data;
}

WRITE_HANDLER( K007342_vreg_w )
{
	switch(offset)
	{
		case 0x00:
			/* bit 1: INT control */
			K007342_int_enabled = data & 0x02;
			K007342_flipscreen = data & 0x10;
			tilemap_set_flip(K007342_tilemap[0],K007342_flipscreen ? (TILEMAP_FLIPY | TILEMAP_FLIPX) : 0);
			tilemap_set_flip(K007342_tilemap[1],K007342_flipscreen ? (TILEMAP_FLIPY | TILEMAP_FLIPX) : 0);
			break;
		case 0x01:  /* used for banking in Rock'n'Rage */
			if (data != K007342_regs[1])
				tilemap_mark_all_tiles_dirty(ALL_TILEMAPS);
		case 0x02:
			K007342_scrollx[0] = (K007342_scrollx[0] & 0xff) | ((data & 0x01) << 8);
			K007342_scrollx[1] = (K007342_scrollx[1] & 0xff) | ((data & 0x02) << 7);
			break;
		case 0x03:  /* scroll x (register 0) */
			K007342_scrollx[0] = (K007342_scrollx[0] & 0x100) | data;
			break;
		case 0x04:  /* scroll y (register 0) */
			K007342_scrolly[0] = data;
			break;
		case 0x05:  /* scroll x (register 1) */
			K007342_scrollx[1] = (K007342_scrollx[1] & 0x100) | data;
			break;
		case 0x06:  /* scroll y (register 1) */
			K007342_scrolly[1] = data;
		case 0x07:  /* unused */
			break;
	}
	K007342_regs[offset] = data;
}

void K007342_tilemap_update(void)
{
	int offs;


	/* update scroll */
	switch (K007342_regs[2] & 0x1c)
	{
		case 0x00:
		case 0x08:	/* unknown, blades of steel shootout between periods */
			tilemap_set_scroll_rows(K007342_tilemap[0],1);
			tilemap_set_scroll_cols(K007342_tilemap[0],1);
			tilemap_set_scrollx(K007342_tilemap[0],0,K007342_scrollx[0]);
			tilemap_set_scrolly(K007342_tilemap[0],0,K007342_scrolly[0]);
			break;

		case 0x0c:	/* 32 columns */
			tilemap_set_scroll_rows(K007342_tilemap[0],1);
			tilemap_set_scroll_cols(K007342_tilemap[0],512);
			tilemap_set_scrollx(K007342_tilemap[0],0,K007342_scrollx[0]);
			for (offs = 0;offs < 256;offs++)
				tilemap_set_scrolly(K007342_tilemap[0],(offs + K007342_scrollx[0]) & 0x1ff,
						K007342_scroll_ram[2*(offs/8)] + 256 * K007342_scroll_ram[2*(offs/8)+1]);
			break;

		case 0x14:	/* 256 rows */
			tilemap_set_scroll_rows(K007342_tilemap[0],256);
			tilemap_set_scroll_cols(K007342_tilemap[0],1);
			tilemap_set_scrolly(K007342_tilemap[0],0,K007342_scrolly[0]);
			for (offs = 0;offs < 256;offs++)
				tilemap_set_scrollx(K007342_tilemap[0],(offs + K007342_scrolly[0]) & 0xff,
						K007342_scroll_ram[2*offs] + 256 * K007342_scroll_ram[2*offs+1]);
			break;

		default:
usrintf_showmessage("unknown scroll ctrl %02x",K007342_regs[2] & 0x1c);
			break;
	}

	tilemap_set_scrollx(K007342_tilemap[1],0,K007342_scrollx[1]);
	tilemap_set_scrolly(K007342_tilemap[1],0,K007342_scrolly[1]);

	/* update all layers */
	tilemap_0_preupdate(); tilemap_update(K007342_tilemap[0]);
	tilemap_1_preupdate(); tilemap_update(K007342_tilemap[1]);

#if 0
	{
		static int current_layer = 0;

		if (keyboard_pressed_memory(KEYCODE_Z)) current_layer = !current_layer;
		tilemap_set_enable(K007342_tilemap[current_layer], 1);
		tilemap_set_enable(K007342_tilemap[!current_layer], 0);

		usrintf_showmessage("regs:%02x %02x %02x %02x-%02x %02x %02x %02x:%02x",
			K007342_regs[0], K007342_regs[1], K007342_regs[2], K007342_regs[3],
			K007342_regs[4], K007342_regs[5], K007342_regs[6], K007342_regs[7],
			current_layer);
	}
#endif
}

void K007342_tilemap_set_enable(int tilemap, int enable)
{
	tilemap_set_enable(K007342_tilemap[tilemap], enable);
}

void K007342_tilemap_draw(struct osd_bitmap *bitmap,int num,int flags)
{
	tilemap_draw(bitmap,K007342_tilemap[num],flags);
}

int K007342_is_INT_enabled(void)
{
	return K007342_int_enabled;
}



static struct GfxElement *K007420_gfx;
static void (*K007420_callback)(int *code,int *color);
static unsigned char *K007420_ram;

int K007420_vh_start(int gfxnum, void (*callback)(int *code,int *color))
{
	K007420_gfx = Machine->gfx[gfxnum];
	K007420_callback = callback;
	K007420_ram = (unsigned char*)malloc(0x200);
	if (!K007420_ram) return 1;

	memset(K007420_ram,0,0x200);

	return 0;
}

void K007420_vh_stop(void)
{
	free(K007420_ram);
	K007420_ram = 0;
}

READ_HANDLER( K007420_r )
{
	return K007420_ram[offset];
}

WRITE_HANDLER( K007420_w )
{
	K007420_ram[offset] = data;
}

/*
 * Sprite Format
 * ------------------
 *
 * Byte | Bit(s)   | Use
 * -----+-76543210-+----------------
 *   0  | xxxxxxxx | y position
 *   1  | xxxxxxxx | sprite code (low 8 bits)
 *   2  | xxxxxxxx | depends on external conections. Usually banking
 *   3  | xxxxxxxx | x position (low 8 bits)
 *   4  | x------- | x position (high bit)
 *   4  | -xxx---- | sprite size 000=16x16 001=8x16 010=16x8 011=8x8 100=32x32
 *   4  | ----x--- | flip y
 *   4  | -----x-- | flip x
 *   4  | ------xx | zoom (bits 8 & 9)
 *   5  | xxxxxxxx | zoom (low 8 bits)  0x080 = normal, < 0x80 enlarge, > 0x80 reduce
 *   6  | xxxxxxxx | unused
 *   7  | xxxxxxxx | unused
 */

void K007420_sprites_draw(struct osd_bitmap *bitmap)
{
#define K007420_SPRITERAM_SIZE 0x200
	int offs;

	for (offs = K007420_SPRITERAM_SIZE - 8; offs >= 0; offs -= 8)
	{
		int ox,oy,code,color,flipx,flipy,zoom,w,h,x,y;
		static int xoffset[4] = { 0, 1, 4, 5 };
		static int yoffset[4] = { 0, 2, 8, 10 };

		code = K007420_ram[offs+1];
		color = K007420_ram[offs+2];
		ox = K007420_ram[offs+3] - ((K007420_ram[offs+4] & 0x80) << 1);
		oy = 256 - K007420_ram[offs+0];
		flipx = K007420_ram[offs+4] & 0x04;
		flipy = K007420_ram[offs+4] & 0x08;

		(*K007420_callback)(&code,&color);

		/* kludge for rock'n'rage */
		if ((K007420_ram[offs+4] == 0x40) && (K007420_ram[offs+1] == 0xff) &&
			(K007420_ram[offs+2] == 0x00) && (K007420_ram[offs+5] == 0xf0)) continue;

		/* 0x080 = normal scale, 0x040 = double size, 0x100 half size */
		zoom = K007420_ram[offs+5] | ((K007420_ram[offs+4] & 0x03) << 8);
		if (!zoom) continue;
		zoom = 0x10000 * 128 / zoom;

		switch (K007420_ram[offs+4] & 0x70)
		{
			case 0x30: w = h = 1; break;
			case 0x20: w = 2; h = 1; code &= (~1); break;
			case 0x10: w = 1; h = 2; code &= (~2); break;
			case 0x00: w = h = 2; code &= (~3); break;
			case 0x40: w = h = 4; code &= (~3); break;
			default: w = 1; h = 1;
//logerror("Unknown sprite size %02x\n",(K007420_ram[offs+4] & 0x70)>>4);
		}

		if (K007342_flipscreen)
		{
			ox = 256 - ox - ((zoom * w + (1<<12)) >> 13);
			oy = 256 - oy - ((zoom * h + (1<<12)) >> 13);
			flipx = !flipx;
			flipy = !flipy;
		}

		if (zoom == 0x10000)
		{
			int sx,sy;

			for (y = 0;y < h;y++)
			{
				sy = oy + 8 * y;

				for (x = 0;x < w;x++)
				{
					int c = code;

					sx = ox + 8 * x;
					if (flipx) c += xoffset[(w-1-x)];
					else c += xoffset[x];
					if (flipy) c += yoffset[(h-1-y)];
					else c += yoffset[y];

					drawgfx(bitmap,K007420_gfx,
						c,
						color,
						flipx,flipy,
						sx,sy,
						&Machine->visible_area,TRANSPARENCY_PEN,0);

					if (K007342_regs[2] & 0x80)
						drawgfx(bitmap,K007420_gfx,
							c,
							color,
							flipx,flipy,
							sx,sy-256,
							&Machine->visible_area,TRANSPARENCY_PEN,0);
				}
			}
		}
		else
		{
			int sx,sy,zw,zh;
			for (y = 0;y < h;y++)
			{
				sy = oy + ((zoom * y + (1<<12)) >> 13);
				zh = (oy + ((zoom * (y+1) + (1<<12)) >> 13)) - sy;

				for (x = 0;x < w;x++)
				{
					int c = code;

					sx = ox + ((zoom * x + (1<<12)) >> 13);
					zw = (ox + ((zoom * (x+1) + (1<<12)) >> 13)) - sx;
					if (flipx) c += xoffset[(w-1-x)];
					else c += xoffset[x];
					if (flipy) c += yoffset[(h-1-y)];
					else c += yoffset[y];

					drawgfxzoom(bitmap,K007420_gfx,
						c,
						color,
						flipx,flipy,
						sx,sy,
						&Machine->visible_area,TRANSPARENCY_PEN,0,
						(zw << 16) / 8,(zh << 16) / 8);

					if (K007342_regs[2] & 0x80)
						drawgfxzoom(bitmap,K007420_gfx,
							c,
							color,
							flipx,flipy,
							sx,sy-256,
							&Machine->visible_area,TRANSPARENCY_PEN,0,
							(zw << 16) / 8,(zh << 16) / 8);
				}
			}
		}
	}
#if 0
	{
		static int current_sprite = 0;

		if (keyboard_pressed_memory(KEYCODE_Z)) current_sprite = (current_sprite+1) & ((K007420_SPRITERAM_SIZE/8)-1);
		if (keyboard_pressed_memory(KEYCODE_X)) current_sprite = (current_sprite-1) & ((K007420_SPRITERAM_SIZE/8)-1);

		usrintf_showmessage("%02x:%02x %02x %02x %02x %02x %02x %02x %02x", current_sprite,
			K007420_ram[(current_sprite*8)+0], K007420_ram[(current_sprite*8)+1],
			K007420_ram[(current_sprite*8)+2], K007420_ram[(current_sprite*8)+3],
			K007420_ram[(current_sprite*8)+4], K007420_ram[(current_sprite*8)+5],
			K007420_ram[(current_sprite*8)+6], K007420_ram[(current_sprite*8)+7]);
	}
#endif
}




static int K052109_memory_region;
static int K052109_gfxnum;
static void (*K052109_callback)(int tilemap,int bank,int *code,int *color);
static unsigned char *K052109_ram;
static unsigned char *K052109_videoram_F,*K052109_videoram2_F,*K052109_colorram_F;
static unsigned char *K052109_videoram_A,*K052109_videoram2_A,*K052109_colorram_A;
static unsigned char *K052109_videoram_B,*K052109_videoram2_B,*K052109_colorram_B;
static unsigned char K052109_charrombank[4];
static int has_extra_video_ram;
static int K052109_RMRD_line;
static int K052109_tileflip_enable;
static int K052109_irq_enabled;
static unsigned char K052109_romsubbank,K052109_scrollctrl;
static struct tilemap *K052109_tilemap[3];



/***************************************************************************

  Callbacks for the TileMap code

***************************************************************************/

/*
  data format:
  video RAM    xxxxxxxx  tile number (low 8 bits)
  color RAM    xxxx----  depends on external connections (usually color and banking)
  color RAM    ----xx--  bank select (0-3): these bits are replaced with the 2
                         bottom bits of the bank register before being placed on
                         the output pins. The other two bits of the bank register are
                         placed on the CAB1 and CAB2 output pins.
  color RAM    ------xx  depends on external connections (usually banking, flip)
*/

static void tilemap0_preupdate(void)
{
	colorram = K052109_colorram_F;
	videoram1 = K052109_videoram_F;
	videoram2 = K052109_videoram2_F;
	layer = 0;
}

static void tilemap1_preupdate(void)
{
	colorram = K052109_colorram_A;
	videoram1 = K052109_videoram_A;
	videoram2 = K052109_videoram2_A;
	layer = 1;
}

static void tilemap2_preupdate(void)
{
	colorram = K052109_colorram_B;
	videoram1 = K052109_videoram_B;
	videoram2 = K052109_videoram2_B;
	layer = 2;
}

static void K052109_get_tile_info(int tile_index)
{
	int flipy = 0;
	int code = videoram1[tile_index] + 256 * videoram2[tile_index];
	int color = colorram[tile_index];
	int bank = K052109_charrombank[(color & 0x0c) >> 2];
if (has_extra_video_ram) bank = (color & 0x0c) >> 2;	/* kludge for X-Men */
	color = (color & 0xf3) | ((bank & 0x03) << 2);
	bank >>= 2;

	flipy = color & 0x02;

	tile_info.flags = 0;

	(*K052109_callback)(layer,bank,&code,&color);

	SET_TILE_INFO(K052109_gfxnum,code,color);

	/* if the callback set flip X but it is not enabled, turn it off */
	if (!(K052109_tileflip_enable & 1)) tile_info.flags &= ~TILE_FLIPX;

	/* if flip Y is enabled and the attribute but is set, turn it on */
	if (flipy && (K052109_tileflip_enable & 2)) tile_info.flags |= TILE_FLIPY;
}



int K052109_vh_start(int gfx_memory_region,int plane0,int plane1,int plane2,int plane3,
		void (*callback)(int tilemap,int bank,int *code,int *color))
{
	int gfx_index;
	static struct GfxLayout charlayout =
	{
		8,8,
		0,				/* filled in later */
		4,
		{ 0, 0, 0, 0 },	/* filled in later */
		{ 0, 1, 2, 3, 4, 5, 6, 7 },
		{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
		32*8
	};


	/* find first empty slot to decode gfx */
	for (gfx_index = 0; gfx_index < MAX_GFX_ELEMENTS; gfx_index++)
		if (Machine->gfx[gfx_index] == 0)
			break;
	if (gfx_index == MAX_GFX_ELEMENTS)
		return 1;

	/* tweak the structure for the number of tiles we have */
	charlayout.total = memory_region_length(gfx_memory_region) / 32;
	charlayout.planeoffset[0] = plane3 * 8;
	charlayout.planeoffset[1] = plane2 * 8;
	charlayout.planeoffset[2] = plane1 * 8;
	charlayout.planeoffset[3] = plane0 * 8;

	/* decode the graphics */
	Machine->gfx[gfx_index] = decodegfx(memory_region(gfx_memory_region),&charlayout);
	if (!Machine->gfx[gfx_index])
		return 1;

	/* set the color information */
	Machine->gfx[gfx_index]->colortable = Machine->remapped_colortable;
	Machine->gfx[gfx_index]->total_colors = Machine->drv->color_table_len / 16;

	K052109_memory_region = gfx_memory_region;
	K052109_gfxnum = gfx_index;
	K052109_callback = callback;
	K052109_RMRD_line = CLEAR_LINE;

	has_extra_video_ram = 0;

	K052109_tilemap[0] = tilemap_create(K052109_get_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,32);
	K052109_tilemap[1] = tilemap_create(K052109_get_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,32);
	K052109_tilemap[2] = tilemap_create(K052109_get_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,32);

	K052109_ram = (unsigned char*)malloc(0x6000);

	if (!K052109_ram || !K052109_tilemap[0] || !K052109_tilemap[1] || !K052109_tilemap[2])
	{
		K052109_vh_stop();
		return 1;
	}

	memset(K052109_ram,0,0x6000);

	K052109_colorram_F = &K052109_ram[0x0000];
	K052109_colorram_A = &K052109_ram[0x0800];
	K052109_colorram_B = &K052109_ram[0x1000];
	K052109_videoram_F = &K052109_ram[0x2000];
	K052109_videoram_A = &K052109_ram[0x2800];
	K052109_videoram_B = &K052109_ram[0x3000];
	K052109_videoram2_F = &K052109_ram[0x4000];
	K052109_videoram2_A = &K052109_ram[0x4800];
	K052109_videoram2_B = &K052109_ram[0x5000];

	K052109_tilemap[0]->transparent_pen = 0;
	K052109_tilemap[1]->transparent_pen = 0;
	K052109_tilemap[2]->transparent_pen = 0;

	return 0;
}

void K052109_vh_stop(void)
{
	free(K052109_ram);
	K052109_ram = 0;
}



READ_HANDLER( K052109_r )
{
	if (K052109_RMRD_line == CLEAR_LINE)
	{
//		if ((offset & 0x1fff) >= 0x1800)
//		{
//			if (offset >= 0x180c && offset < 0x1834)
//			{	/* A y scroll */	}
//			else if (offset >= 0x1a00 && offset < 0x1c00)
//			{	/* A x scroll */	}
//			else if (offset == 0x1d00)
//			{	/* read for bitwise operations before writing */	}
//			else if (offset >= 0x380c && offset < 0x3834)
//			{	/* B y scroll */	}
//			else if (offset >= 0x3a00 && offset < 0x3c00)
//			{	/* B x scroll */	}
//			else
//logerror("%04x: read from unknown 052109 address %04x\n",cpu_get_pc(),offset);
//		}

		return K052109_ram[offset];
	}
	else	/* Punk Shot and TMNT read from 0000-1fff, Aliens from 2000-3fff */
	{
		int code = (offset & 0x1fff) >> 5;
		int color = K052109_romsubbank;
		int bank = K052109_charrombank[(color & 0x0c) >> 2] >> 2;   /* discard low bits (TMNT) */
		int addr;

if (has_extra_video_ram) code |= color << 8;	/* kludge for X-Men */
else
		(*K052109_callback)(0,bank,&code,&color);

		addr = (code << 5) + (offset & 0x1f);
		addr &= memory_region_length(K052109_memory_region)-1;

#if 0
	usrintf_showmessage("%04x: off%04x sub%02x (bnk%x) adr%06x",cpu_get_pc(),offset,K052109_romsubbank,bank,addr);
#endif

		return memory_region(K052109_memory_region)[addr];
	}
}

WRITE_HANDLER( K052109_w )
{
	if ((offset & 0x1fff) < 0x1800) /* tilemap RAM */
	{
		if (K052109_ram[offset] != data)
		{
			if (offset >= 0x4000) has_extra_video_ram = 1;  /* kludge for X-Men */
			K052109_ram[offset] = data;
			tilemap_mark_tile_dirty(K052109_tilemap[(offset & 0x1800) >> 11],offset & 0x7ff);
		}
	}
	else	/* control registers */
	{
		K052109_ram[offset] = data;

		if (offset >= 0x180c && offset < 0x1834)
		{	/* A y scroll */	}
		else if (offset >= 0x1a00 && offset < 0x1c00)
		{	/* A x scroll */	}
		else if (offset == 0x1c80)
		{
if (K052109_scrollctrl != data)
{
#if 0
usrintf_showmessage("scrollcontrol = %02x",data);
#endif
//logerror("%04x: rowscrollcontrol = %02x\n",cpu_get_pc(),data);
			K052109_scrollctrl = data;
}
		}
		else if (offset == 0x1d00)
		{
#if VERBOSE
logerror("%04x: 052109 register 1d00 = %02x\n",cpu_get_pc(),data);
#endif
			/* bit 2 = irq enable */
			/* the custom chip can also generate NMI and FIRQ, for use with a 6809 */
			K052109_irq_enabled = data & 0x04;
		}
		else if (offset == 0x1d80)
		{
			int dirty = 0;

			if (K052109_charrombank[0] != (data & 0x0f)) dirty |= 1;
			if (K052109_charrombank[1] != ((data >> 4) & 0x0f)) dirty |= 2;
			if (dirty)
			{
				int i;

				K052109_charrombank[0] = data & 0x0f;
				K052109_charrombank[1] = (data >> 4) & 0x0f;

				for (i = 0;i < 0x1800;i++)
				{
					int bank = (K052109_ram[i]&0x0c) >> 2;
					if ((bank == 0 && (dirty & 1)) || (bank == 1 && dirty & 2))
					{
						tilemap_mark_tile_dirty(K052109_tilemap[(i & 0x1800) >> 11],i & 0x7ff);
					}
				}
			}
		}
		else if (offset == 0x1e00)
		{
//logerror("%04x: 052109 register 1e00 = %02x\n",cpu_get_pc(),data);
			K052109_romsubbank = data;
		}
		else if (offset == 0x1e80)
		{
//if ((data & 0xfe)) logerror("%04x: 052109 register 1e80 = %02x\n",cpu_get_pc(),data);
			tilemap_set_flip(K052109_tilemap[0],(data & 1) ? (TILEMAP_FLIPY | TILEMAP_FLIPX) : 0);
			tilemap_set_flip(K052109_tilemap[1],(data & 1) ? (TILEMAP_FLIPY | TILEMAP_FLIPX) : 0);
			tilemap_set_flip(K052109_tilemap[2],(data & 1) ? (TILEMAP_FLIPY | TILEMAP_FLIPX) : 0);
			if (K052109_tileflip_enable != ((data & 0x06) >> 1))
			{
				K052109_tileflip_enable = ((data & 0x06) >> 1);

				tilemap_mark_all_tiles_dirty(K052109_tilemap[0]);
				tilemap_mark_all_tiles_dirty(K052109_tilemap[1]);
				tilemap_mark_all_tiles_dirty(K052109_tilemap[2]);
			}
		}
		else if (offset == 0x1f00)
		{
			int dirty = 0;

			if (K052109_charrombank[2] != (data & 0x0f)) dirty |= 1;
			if (K052109_charrombank[3] != ((data >> 4) & 0x0f)) dirty |= 2;
			if (dirty)
			{
				int i;

				K052109_charrombank[2] = data & 0x0f;
				K052109_charrombank[3] = (data >> 4) & 0x0f;

				for (i = 0;i < 0x1800;i++)
				{
					int bank = (K052109_ram[i] & 0x0c) >> 2;
					if ((bank == 2 && (dirty & 1)) || (bank == 3 && dirty & 2))
						tilemap_mark_tile_dirty(K052109_tilemap[(i & 0x1800) >> 11],i & 0x7ff);
				}
			}
		}
		else if (offset >= 0x380c && offset < 0x3834)
		{	/* B y scroll */	}
		else if (offset >= 0x3a00 && offset < 0x3c00)
		{	/* B x scroll */	}
//		else
//logerror("%04x: write %02x to unknown 052109 address %04x\n",cpu_get_pc(),data,offset);
	}
}

void K052109_set_RMRD_line(int state)
{
	K052109_RMRD_line = state;
}


void K052109_tilemap_update(void)
{
#if 0
{
usrintf_showmessage("%x %x %x %x",
	K052109_charrombank[0],
	K052109_charrombank[1],
	K052109_charrombank[2],
	K052109_charrombank[3]);
}
#endif
	if ((K052109_scrollctrl & 0x03) == 0x02)
	{
		int xscroll,yscroll,offs;
		unsigned char *scrollram = &K052109_ram[0x1a00];


		tilemap_set_scroll_rows(K052109_tilemap[1],256);
		tilemap_set_scroll_cols(K052109_tilemap[1],1);
		yscroll = K052109_ram[0x180c];
		tilemap_set_scrolly(K052109_tilemap[1],0,yscroll);
		for (offs = 0;offs < 256;offs++)
		{
			xscroll = scrollram[2*(offs&0xfff8)+0] + 256 * scrollram[2*(offs&0xfff8)+1];
			xscroll -= 6;
			tilemap_set_scrollx(K052109_tilemap[1],(offs+yscroll)&0xff,xscroll);
		}
	}
	else if ((K052109_scrollctrl & 0x03) == 0x03)
	{
		int xscroll,yscroll,offs;
		unsigned char *scrollram = &K052109_ram[0x1a00];


		tilemap_set_scroll_rows(K052109_tilemap[1],256);
		tilemap_set_scroll_cols(K052109_tilemap[1],1);
		yscroll = K052109_ram[0x180c];
		tilemap_set_scrolly(K052109_tilemap[1],0,yscroll);
		for (offs = 0;offs < 256;offs++)
		{
			xscroll = scrollram[2*offs+0] + 256 * scrollram[2*offs+1];
			xscroll -= 6;
			tilemap_set_scrollx(K052109_tilemap[1],(offs+yscroll)&0xff,xscroll);
		}
	}
	else if ((K052109_scrollctrl & 0x04) == 0x04)
	{
		int xscroll,yscroll,offs;
		unsigned char *scrollram = &K052109_ram[0x1800];


		tilemap_set_scroll_rows(K052109_tilemap[1],1);
		tilemap_set_scroll_cols(K052109_tilemap[1],512);
		xscroll = K052109_ram[0x1a00] + 256 * K052109_ram[0x1a01];
		xscroll -= 6;
		tilemap_set_scrollx(K052109_tilemap[1],0,xscroll);
		for (offs = 0;offs < 512;offs++)
		{
			yscroll = scrollram[offs/8];
			tilemap_set_scrolly(K052109_tilemap[1],(offs+xscroll)&0x1ff,yscroll);
		}
	}
	else
	{
		int xscroll,yscroll;
		unsigned char *scrollram = &K052109_ram[0x1a00];


		tilemap_set_scroll_rows(K052109_tilemap[1],1);
		tilemap_set_scroll_cols(K052109_tilemap[1],1);
		xscroll = scrollram[0] + 256 * scrollram[1];
		xscroll -= 6;
		yscroll = K052109_ram[0x180c];
		tilemap_set_scrollx(K052109_tilemap[1],0,xscroll);
		tilemap_set_scrolly(K052109_tilemap[1],0,yscroll);
	}

	if ((K052109_scrollctrl & 0x18) == 0x10)
	{
		int xscroll,yscroll,offs;
		unsigned char *scrollram = &K052109_ram[0x3a00];


		tilemap_set_scroll_rows(K052109_tilemap[2],256);
		tilemap_set_scroll_cols(K052109_tilemap[2],1);
		yscroll = K052109_ram[0x380c];
		tilemap_set_scrolly(K052109_tilemap[2],0,yscroll);
		for (offs = 0;offs < 256;offs++)
		{
			xscroll = scrollram[2*(offs&0xfff8)+0] + 256 * scrollram[2*(offs&0xfff8)+1];
			xscroll -= 6;
			tilemap_set_scrollx(K052109_tilemap[2],(offs+yscroll)&0xff,xscroll);
		}
	}
	else if ((K052109_scrollctrl & 0x18) == 0x18)
	{
		int xscroll,yscroll,offs;
		unsigned char *scrollram = &K052109_ram[0x3a00];


		tilemap_set_scroll_rows(K052109_tilemap[2],256);
		tilemap_set_scroll_cols(K052109_tilemap[2],1);
		yscroll = K052109_ram[0x380c];
		tilemap_set_scrolly(K052109_tilemap[2],0,yscroll);
		for (offs = 0;offs < 256;offs++)
		{
			xscroll = scrollram[2*offs+0] + 256 * scrollram[2*offs+1];
			xscroll -= 6;
			tilemap_set_scrollx(K052109_tilemap[2],(offs+yscroll)&0xff,xscroll);
		}
	}
	else if ((K052109_scrollctrl & 0x20) == 0x20)
	{
		int xscroll,yscroll,offs;
		unsigned char *scrollram = &K052109_ram[0x3800];


		tilemap_set_scroll_rows(K052109_tilemap[2],1);
		tilemap_set_scroll_cols(K052109_tilemap[2],512);
		xscroll = K052109_ram[0x3a00] + 256 * K052109_ram[0x3a01];
		xscroll -= 6;
		tilemap_set_scrollx(K052109_tilemap[2],0,xscroll);
		for (offs = 0;offs < 512;offs++)
		{
			yscroll = scrollram[offs/8];
			tilemap_set_scrolly(K052109_tilemap[2],(offs+xscroll)&0x1ff,yscroll);
		}
	}
	else
	{
		int xscroll,yscroll;
		unsigned char *scrollram = &K052109_ram[0x3a00];


		tilemap_set_scroll_rows(K052109_tilemap[2],1);
		tilemap_set_scroll_cols(K052109_tilemap[2],1);
		xscroll = scrollram[0] + 256 * scrollram[1];
		xscroll -= 6;
		yscroll = K052109_ram[0x380c];
		tilemap_set_scrollx(K052109_tilemap[2],0,xscroll);
		tilemap_set_scrolly(K052109_tilemap[2],0,yscroll);
	}

	tilemap0_preupdate(); tilemap_update(K052109_tilemap[0]);
	tilemap1_preupdate(); tilemap_update(K052109_tilemap[1]);
	tilemap2_preupdate(); tilemap_update(K052109_tilemap[2]);
}

void K052109_tilemap_draw(struct osd_bitmap *bitmap,int num,int flags)
{
	tilemap_draw(bitmap,K052109_tilemap[num],flags);
}

int K052109_is_IRQ_enabled(void)
{
	return K052109_irq_enabled;
}







static int K051960_memory_region;
static struct GfxElement *K051960_gfx;
static void (*K051960_callback)(int *code,int *color,int *priority,int *shadow);
static int K051960_romoffset;
static int K051960_spriteflip,K051960_readroms,K051960_force_shadows;
static unsigned char K051960_spriterombank[3];
static unsigned char *K051960_ram;
static int K051960_irq_enabled, K051960_nmi_enabled;


int K051960_vh_start(int gfx_memory_region,int plane0,int plane1,int plane2,int plane3,
		void (*callback)(int *code,int *color,int *priority,int *shadow))
{
	int gfx_index;
	static struct GfxLayout spritelayout =
	{
		16,16,
		0,				/* filled in later */
		4,
		{ 0, 0, 0, 0 },	/* filled in later */
		{ 0, 1, 2, 3, 4, 5, 6, 7,
				8*32+0, 8*32+1, 8*32+2, 8*32+3, 8*32+4, 8*32+5, 8*32+6, 8*32+7 },
		{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32,
				16*32, 17*32, 18*32, 19*32, 20*32, 21*32, 22*32, 23*32 },
		128*8
	};


	/* find first empty slot to decode gfx */
	for (gfx_index = 0; gfx_index < MAX_GFX_ELEMENTS; gfx_index++)
		if (Machine->gfx[gfx_index] == 0)
			break;
	if (gfx_index == MAX_GFX_ELEMENTS)
		return 1;

	/* tweak the structure for the number of tiles we have */
	spritelayout.total = memory_region_length(gfx_memory_region) / 128;
	spritelayout.planeoffset[0] = plane0 * 8;
	spritelayout.planeoffset[1] = plane1 * 8;
	spritelayout.planeoffset[2] = plane2 * 8;
	spritelayout.planeoffset[3] = plane3 * 8;

	/* decode the graphics */
	Machine->gfx[gfx_index] = decodegfx(memory_region(gfx_memory_region),&spritelayout);
	if (!Machine->gfx[gfx_index])
		return 1;

	/* set the color information */
	Machine->gfx[gfx_index]->colortable = Machine->remapped_colortable;
	Machine->gfx[gfx_index]->total_colors = Machine->drv->color_table_len / 16;

	K051960_memory_region = gfx_memory_region;
	K051960_gfx = Machine->gfx[gfx_index];
	K051960_callback = callback;
	K051960_ram = (unsigned char*)malloc(0x400);
	if (!K051960_ram) return 1;
	memset(K051960_ram,0,0x400);
	K051960_force_shadows = 0;

	return 0;
}

void K051960_vh_stop(void)
{
	free(K051960_ram);
	K051960_ram = 0;
}


static int K051960_fetchromdata(int byte)
{
	int code,color,pri,shadow,off1,addr;


	addr = K051960_romoffset + (K051960_spriterombank[0] << 8) +
			((K051960_spriterombank[1] & 0x03) << 16);
	code = (addr & 0x3ffe0) >> 5;
	off1 = addr & 0x1f;
	color = ((K051960_spriterombank[1] & 0xfc) >> 2) + ((K051960_spriterombank[2] & 0x03) << 6);
	pri = 0;
	shadow = color & 0x80;
	(*K051960_callback)(&code,&color,&pri,&shadow);

	addr = (code << 7) | (off1 << 2) | byte;
	addr &= memory_region_length(K051960_memory_region)-1;

#if 0
	usrintf_showmessage("%04x: addr %06x",cpu_get_pc(),addr);
#endif

	return memory_region(K051960_memory_region)[addr];
}

READ_HANDLER( K051960_r )
{
	if (K051960_readroms)
	{
		/* the 051960 remembers the last address read and uses it when reading the sprite ROMs */
		K051960_romoffset = (offset & 0x3fc) >> 2;
		return K051960_fetchromdata(offset & 3);	/* only 88 Games reads the ROMs from here */
	}
	else
		return K051960_ram[offset];
}

WRITE_HANDLER( K051960_w )
{
	K051960_ram[offset] = data;
}

READ_HANDLER( K051960_word_r )
{
	return K051960_r(offset + 1) | (K051960_r(offset) << 8);
}

WRITE_HANDLER( K051960_word_w )
{
	if ((data & 0xff000000) == 0)
		K051960_w(offset,(data >> 8) & 0xff);
	if ((data & 0x00ff0000) == 0)
		K051960_w(offset + 1,data & 0xff);
}

READ_HANDLER( K051937_r )
{
	if (K051960_readroms && offset >= 4 && offset < 8)
	{
		return K051960_fetchromdata(offset & 3);
	}
	else
	{
		if (offset == 0)
		{
			static int counter;

			/* some games need bit 0 to pulse */
			return (counter++) & 1;
		}
//logerror("%04x: read unknown 051937 address %x\n",cpu_get_pc(),offset);
		return 0;
	}
}

WRITE_HANDLER( K051937_w )
{
	if (offset == 0)
	{
		/* bit 0 is IRQ enable */
		K051960_irq_enabled = (data & 0x01);

		/* bit 1: probably FIRQ enable */

		/* bit 2 is NMI enable */
		K051960_nmi_enabled = (data & 0x04);

		/* bit 3 = flip screen */
		K051960_spriteflip = data & 0x08;

		/* bit 4 used by Devastators and TMNT, unknown */

		/* bit 5 = enable gfx ROM reading */
		K051960_readroms = data & 0x20;
#if VERBOSE
logerror("%04x: write %02x to 051937 address %x\n",cpu_get_pc(),data,offset);
#endif
	}
	else if (offset == 1)
	{
#if 0
	usrintf_showmessage("%04x: write %02x to 051937 address %x",cpu_get_pc(),data,offset);
#endif
//logerror("%04x: write %02x to unknown 051937 address %x\n",cpu_get_pc(),data,offset);
		K051960_force_shadows = data & 0x02;
	}
	else if (offset >= 2 && offset < 5)
	{
		K051960_spriterombank[offset - 2] = data;
	}
	else
	{
#if 0
	usrintf_showmessage("%04x: write %02x to 051937 address %x",cpu_get_pc(),data,offset);
#endif
//logerror("%04x: write %02x to unknown 051937 address %x\n",cpu_get_pc(),data,offset);
	}
}

READ_HANDLER( K051937_word_r )
{
	return K051937_r(offset + 1) | (K051937_r(offset) << 8);
}

WRITE_HANDLER( K051937_word_w )
{
	if ((data & 0xff000000) == 0)
		K051937_w(offset,(data >> 8) & 0xff);
	if ((data & 0x00ff0000) == 0)
		K051937_w(offset + 1,data & 0xff);
}


/*
 * Sprite Format
 * ------------------
 *
 * Byte | Bit(s)   | Use
 * -----+-76543210-+----------------
 *   0  | x------- | active (show this sprite)
 *   0  | -xxxxxxx | priority order
 *   1  | xxx----- | sprite size (see below)
 *   1  | ---xxxxx | sprite code (high 5 bits)
 *   2  | xxxxxxxx | sprite code (low 8 bits)
 *   3  | xxxxxxxx | "color", but depends on external connections (see below)
 *   4  | xxxxxx-- | zoom y (0 = normal, >0 = shrink)
 *   4  | ------x- | flip y
 *   4  | -------x | y position (high bit)
 *   5  | xxxxxxxx | y position (low 8 bits)
 *   6  | xxxxxx-- | zoom x (0 = normal, >0 = shrink)
 *   6  | ------x- | flip x
 *   6  | -------x | x position (high bit)
 *   7  | xxxxxxxx | x position (low 8 bits)
 *
 * Example of "color" field for Punk Shot:
 *   3  | x------- | shadow
 *   3  | -xx----- | priority
 *   3  | ---x---- | use second gfx ROM bank
 *   3  | ----xxxx | color code
 *
 * shadow enables transparent shadows. Note that it applies to pen 0x0f ONLY.
 * The rest of the sprite remains normal.
 * Note that Aliens also uses the shadow bit to select the second sprite bank.
 */

void K051960_sprites_draw(struct osd_bitmap *bitmap,int min_priority,int max_priority)
{
#define NUM_SPRITES 128
	int offs,pri_code;
	int sortedlist[NUM_SPRITES];

	for (offs = 0;offs < NUM_SPRITES;offs++)
		sortedlist[offs] = -1;

	/* prebuild a sorted table */
	for (offs = 0;offs < 0x400;offs += 8)
	{
		if (K051960_ram[offs] & 0x80)
		{
			if (max_priority == -1)	/* draw front to back when using priority buffer */
				sortedlist[(K051960_ram[offs] & 0x7f) ^ 0x7f] = offs;
			else
				sortedlist[K051960_ram[offs] & 0x7f] = offs;
		}
	}

	for (pri_code = 0;pri_code < NUM_SPRITES;pri_code++)
	{
		int ox,oy,code,color,pri,shadow,size,w,h,x,y,flipx,flipy,zoomx,zoomy;
		/* sprites can be grouped up to 8x8. The draw order is
			 0  1  4  5 16 17 20 21
			 2  3  6  7 18 19 22 23
			 8  9 12 13 24 25 28 29
			10 11 14 15 26 27 30 31
			32 33 36 37 48 49 52 53
			34 35 38 39 50 51 54 55
			40 41 44 45 56 57 60 61
			42 43 46 47 58 59 62 63
		*/
		static int xoffset[8] = { 0, 1, 4, 5, 16, 17, 20, 21 };
		static int yoffset[8] = { 0, 2, 8, 10, 32, 34, 40, 42 };
		static int width[8] =  { 1, 2, 1, 2, 4, 2, 4, 8 };
		static int height[8] = { 1, 1, 2, 2, 2, 4, 4, 8 };


		offs = sortedlist[pri_code];
		if (offs == -1) continue;

		code = K051960_ram[offs+2] + ((K051960_ram[offs+1] & 0x1f) << 8);
		color = K051960_ram[offs+3] & 0xff;
		pri = 0;
		shadow = color & 0x80;
		(*K051960_callback)(&code,&color,&pri,&shadow);

		if (max_priority != -1)
			if (pri < min_priority || pri > max_priority) continue;

		size = (K051960_ram[offs+1] & 0xe0) >> 5;
		w = width[size];
		h = height[size];

		if (w >= 2) code &= ~0x01;
		if (h >= 2) code &= ~0x02;
		if (w >= 4) code &= ~0x04;
		if (h >= 4) code &= ~0x08;
		if (w >= 8) code &= ~0x10;
		if (h >= 8) code &= ~0x20;

		ox = (256 * K051960_ram[offs+6] + K051960_ram[offs+7]) & 0x01ff;
		oy = 256 - ((256 * K051960_ram[offs+4] + K051960_ram[offs+5]) & 0x01ff);
		flipx = K051960_ram[offs+6] & 0x02;
		flipy = K051960_ram[offs+4] & 0x02;
		zoomx = (K051960_ram[offs+6] & 0xfc) >> 2;
		zoomy = (K051960_ram[offs+4] & 0xfc) >> 2;
		zoomx = 0x10000 / 128 * (128 - zoomx);
		zoomy = 0x10000 / 128 * (128 - zoomy);

		if (K051960_spriteflip)
		{
			ox = 512 - (zoomx * w >> 12) - ox;
			oy = 256 - (zoomy * h >> 12) - oy;
			flipx = !flipx;
			flipy = !flipy;
		}

		if (zoomx == 0x10000 && zoomy == 0x10000)
		{
			int sx,sy;

			for (y = 0;y < h;y++)
			{
				sy = oy + 16 * y;

				for (x = 0;x < w;x++)
				{
					int c = code;

					sx = ox + 16 * x;
					if (flipx) c += xoffset[(w-1-x)];
					else c += xoffset[x];
					if (flipy) c += yoffset[(h-1-y)];
					else c += yoffset[y];

					/* hack to simulate shadow */
					if (K051960_force_shadows || shadow)
					{
						int o = K051960_gfx->colortable[16*color+15];
						K051960_gfx->colortable[16*color+15] = palette_transparent_pen;
						if (max_priority == -1)
							pdrawgfx(bitmap,K051960_gfx,
									c,
									color,
									flipx,flipy,
									sx & 0x1ff,sy,
									&Machine->visible_area,TRANSPARENCY_PENS,(cpu_getcurrentframe() & 1) ? 0x8001 : 0x0001,pri);
						else
							drawgfx(bitmap,K051960_gfx,
									c,
									color,
									flipx,flipy,
									sx & 0x1ff,sy,
									&Machine->visible_area,TRANSPARENCY_PENS,(cpu_getcurrentframe() & 1) ? 0x8001 : 0x0001);
						K051960_gfx->colortable[16*color+15] = o;
					}
					else
					{
						if (max_priority == -1)
							pdrawgfx(bitmap,K051960_gfx,
									c,
									color,
									flipx,flipy,
									sx & 0x1ff,sy,
									&Machine->visible_area,TRANSPARENCY_PEN,0,pri);
						else
							drawgfx(bitmap,K051960_gfx,
									c,
									color,
									flipx,flipy,
									sx & 0x1ff,sy,
									&Machine->visible_area,TRANSPARENCY_PEN,0);
					}
				}
			}
		}
		else
		{
			int sx,sy,zw,zh;

			for (y = 0;y < h;y++)
			{
				sy = oy + ((zoomy * y + (1<<11)) >> 12);
				zh = (oy + ((zoomy * (y+1) + (1<<11)) >> 12)) - sy;

				for (x = 0;x < w;x++)
				{
					int c = code;

					sx = ox + ((zoomx * x + (1<<11)) >> 12);
					zw = (ox + ((zoomx * (x+1) + (1<<11)) >> 12)) - sx;
					if (flipx) c += xoffset[(w-1-x)];
					else c += xoffset[x];
					if (flipy) c += yoffset[(h-1-y)];
					else c += yoffset[y];

					/* hack to simulate shadow */
					if (K051960_force_shadows || shadow)
					{
						int o = K051960_gfx->colortable[16*color+15];
						K051960_gfx->colortable[16*color+15] = palette_transparent_pen;
						if (max_priority == -1)
							pdrawgfxzoom(bitmap,K051960_gfx,
									c,
									color,
									flipx,flipy,
									sx & 0x1ff,sy,
									&Machine->visible_area,TRANSPARENCY_PENS,(cpu_getcurrentframe() & 1) ? 0x8001 : 0x0001,
									(zw << 16) / 16,(zh << 16) / 16,pri);
						else
							drawgfxzoom(bitmap,K051960_gfx,
									c,
									color,
									flipx,flipy,
									sx & 0x1ff,sy,
									&Machine->visible_area,TRANSPARENCY_PENS,(cpu_getcurrentframe() & 1) ? 0x8001 : 0x0001,
									(zw << 16) / 16,(zh << 16) / 16);
						K051960_gfx->colortable[16*color+15] = o;
					}
					else
					{
						if (max_priority == -1)
							pdrawgfxzoom(bitmap,K051960_gfx,
									c,
									color,
									flipx,flipy,
									sx & 0x1ff,sy,
									&Machine->visible_area,TRANSPARENCY_PEN,0,
									(zw << 16) / 16,(zh << 16) / 16,pri);
						else
							drawgfxzoom(bitmap,K051960_gfx,
									c,
									color,
									flipx,flipy,
									sx & 0x1ff,sy,
									&Machine->visible_area,TRANSPARENCY_PEN,0,
									(zw << 16) / 16,(zh << 16) / 16);
					}
				}
			}
		}
	}
#if 0
if (keyboard_pressed(KEYCODE_D))
{
	FILE *fp;
	fp=fopen("SPRITE.DMP", "w+b");
	if (fp)
	{
		fwrite(K051960_ram, 0x400, 1, fp);
		usrintf_showmessage("saved");
		fclose(fp);
	}
}
#endif
#undef NUM_SPRITES
}

void K051960_mark_sprites_colors(void)
{
	int offs,i;

	unsigned short palette_map[512];

	memset (palette_map, 0, sizeof (palette_map));

	/* sprites */
	for (offs = 0x400-8;offs >= 0;offs -= 8)
	{
		if (K051960_ram[offs] & 0x80)
		{
			int code,color,pri,shadow;

			code = K051960_ram[offs+2] + ((K051960_ram[offs+1] & 0x1f) << 8);
			color = (K051960_ram[offs+3] & 0xff);
			pri = 0;
			shadow = color & 0x80;
			(*K051960_callback)(&code,&color,&pri,&shadow);
			palette_map[color] |= 0xffff;
		}
	}

	/* now build the final table */
	for (i = 0; i < 512; i++)
	{
		int usage = palette_map[i], j;
		if (usage)
		{
			for (j = 1; j < 16; j++)
				if (usage & (1 << j))
					palette_used_colors[i * 16 + j] |= PALETTE_COLOR_VISIBLE;
		}
	}
}

int K051960_is_IRQ_enabled(void)
{
	return K051960_irq_enabled;
}

int K051960_is_NMI_enabled(void)
{
	return K051960_nmi_enabled;
}




READ_HANDLER( K052109_051960_r )
{
	if (K052109_RMRD_line == CLEAR_LINE)
	{
		if (offset >= 0x3800 && offset < 0x3808)
			return K051937_r(offset - 0x3800);
		else if (offset < 0x3c00)
			return K052109_r(offset);
		else
			return K051960_r(offset - 0x3c00);
	}
	else return K052109_r(offset);
}

WRITE_HANDLER( K052109_051960_w )
{
	if (offset >= 0x3800 && offset < 0x3808)
		K051937_w(offset - 0x3800,data);
	else if (offset < 0x3c00)
		K052109_w(offset,data);
	else
		K051960_w(offset - 0x3c00,data);
}





static int K053245_memory_region=2;
static struct GfxElement *K053245_gfx;
static void (*K053245_callback)(int *code,int *color,int *priority);
static int K053244_romoffset,K053244_rombank;
static int K053244_readroms;
static int K053245_flipscreenX,K053245_flipscreenY;
static int K053245_spriteoffsX,K053245_spriteoffsY;
static unsigned char *K053245_ram;

int K053245_vh_start(int gfx_memory_region,int plane0,int plane1,int plane2,int plane3,
		void (*callback)(int *code,int *color,int *priority))
{
	int gfx_index;
	static struct GfxLayout spritelayout =
	{
		16,16,
		0,				/* filled in later */
		4,
		{ 0, 0, 0, 0 },	/* filled in later */
		{ 0, 1, 2, 3, 4, 5, 6, 7,
				8*32+0, 8*32+1, 8*32+2, 8*32+3, 8*32+4, 8*32+5, 8*32+6, 8*32+7 },
		{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32,
				16*32, 17*32, 18*32, 19*32, 20*32, 21*32, 22*32, 23*32 },
		128*8
	};


	/* find first empty slot to decode gfx */
	for (gfx_index = 0; gfx_index < MAX_GFX_ELEMENTS; gfx_index++)
		if (Machine->gfx[gfx_index] == 0)
			break;
	if (gfx_index == MAX_GFX_ELEMENTS)
		return 1;

	/* tweak the structure for the number of tiles we have */
	spritelayout.total = memory_region_length(gfx_memory_region) / 128;
	spritelayout.planeoffset[0] = plane3 * 8;
	spritelayout.planeoffset[1] = plane2 * 8;
	spritelayout.planeoffset[2] = plane1 * 8;
	spritelayout.planeoffset[3] = plane0 * 8;

	/* decode the graphics */
	Machine->gfx[gfx_index] = decodegfx(memory_region(gfx_memory_region),&spritelayout);
	if (!Machine->gfx[gfx_index])
		return 1;

	/* set the color information */
	Machine->gfx[gfx_index]->colortable = Machine->remapped_colortable;
	Machine->gfx[gfx_index]->total_colors = Machine->drv->color_table_len / 16;

	K053245_memory_region = gfx_memory_region;
	K053245_gfx = Machine->gfx[gfx_index];
	K053245_callback = callback;
	K053244_rombank = 0;
	K053245_ram = (unsigned char*)malloc(0x800);
	if (!K053245_ram) return 1;

	memset(K053245_ram,0,0x800);

	return 0;
}

void K053245_vh_stop(void)
{
	free(K053245_ram);
	K053245_ram = 0;
}

READ_HANDLER( K053245_word_r )
{
	return READ_WORD(&K053245_ram[offset]);
}

WRITE_HANDLER( K053245_word_w )
{
	COMBINE_WORD_MEM(&K053245_ram[offset],data);
}

READ_HANDLER( K053245_r )
{
	int shift = ((offset & 1) ^ 1) << 3;
	return (READ_WORD(&K053245_ram[offset & ~1]) >> shift) & 0xff;
}

WRITE_HANDLER( K053245_w )
{
	int shift = ((offset & 1) ^ 1) << 3;
	offset &= ~1;
	COMBINE_WORD_MEM(&K053245_ram[offset],(0xff000000 >> shift) | ((data & 0xff) << shift));
}

READ_HANDLER( K053244_r )
{
	if (K053244_readroms && offset >= 0x0c && offset < 0x10)
	{
		int addr;


		addr = 0x200000 * K053244_rombank + 4 * (K053244_romoffset & 0x7ffff) + ((offset & 3) ^ 1);
		addr &= memory_region_length(K053245_memory_region)-1;

#if 0
	usrintf_showmessage("%04x: offset %02x addr %06x",cpu_get_pc(),offset&3,addr);
#endif

		return memory_region(K053245_memory_region)[addr];
	}
	else
	{
//logerror("%04x: read from unknown 053244 address %x\n",cpu_get_pc(),offset);
		return 0;
	}
}

WRITE_HANDLER( K053244_w )
{
	if (offset == 0x00)
		K053245_spriteoffsX = (K053245_spriteoffsX & 0x00ff) | (data << 8);
	else if (offset == 0x01)
		K053245_spriteoffsX = (K053245_spriteoffsX & 0xff00) | data;
	else if (offset == 0x02)
		K053245_spriteoffsY = (K053245_spriteoffsY & 0x00ff) | (data << 8);
	else if (offset == 0x03)
		K053245_spriteoffsY = (K053245_spriteoffsY & 0xff00) | data;
	else if (offset == 0x05)
	{
		/* bit 0/1 = flip screen */
		K053245_flipscreenX = data & 0x01;
		K053245_flipscreenY = data & 0x02;

		/* bit 2 = unknown, Parodius uses it */

		/* bit 4 = enable gfx ROM reading */
		K053244_readroms = data & 0x10;

		/* bit 5 = unknown, Rollergames uses it */
#if VERBOSE
logerror("%04x: write %02x to 053244 address 5\n",cpu_get_pc(),data);
#endif
	}
	else if (offset >= 0x08 && offset < 0x0c)
	{
		offset = 8*((offset & 0x03) ^ 0x01);
		K053244_romoffset = (K053244_romoffset & ~(0xff << offset)) | (data << offset);
		return;
	}
//	else
//logerror("%04x: write %02x to unknown 053244 address %x\n",cpu_get_pc(),data,offset);
}

void K053244_bankselect(int bank)   /* used by TMNT2 for ROM testing */
{
	K053244_rombank = bank;
}

/*
 * Sprite Format
 * ------------------
 *
 * Word | Bit(s)           | Use
 * -----+-fedcba9876543210-+----------------
 *   0  | x--------------- | active (show this sprite)
 *   0  | -x-------------- | maintain aspect ratio (when set, zoom y acts on both axis)
 *   0  | --x------------- | flip y
 *   0  | ---x------------ | flip x
 *   0  | ----xxxx-------- | sprite size (see below)
 *   0  | ---------xxxxxxx | priority order
 *   1  | --xxxxxxxxxxxxxx | sprite code. We use an additional bit in TMNT2, but this is
 *                           probably not accurate (protection related so we can't verify)
 *   2  | ------xxxxxxxxxx | y position
 *   3  | ------xxxxxxxxxx | x position
 *   4  | xxxxxxxxxxxxxxxx | zoom y (0x40 = normal, <0x40 = enlarge, >0x40 = reduce)
 *   5  | xxxxxxxxxxxxxxxx | zoom x (0x40 = normal, <0x40 = enlarge, >0x40 = reduce)
 *   6  | ------x--------- | mirror y (top half is drawn as mirror image of the bottom)
 *   6  | -------x-------- | mirror x (right half is drawn as mirror image of the left)
 *   6  | --------x------- | shadow
 *   6  | ---------xxxxxxx | "color", but depends on external connections
 *   7  | ---------------- |
 *
 * shadow enables transparent shadows. Note that it applies to pen 0x0f ONLY.
 * The rest of the sprite remains normal.
 */

void K053245_sprites_draw(struct osd_bitmap *bitmap)
{
#define NUM_SPRITES 128
	int offs,pri_code;
	int sortedlist[NUM_SPRITES];

	for (offs = 0;offs < NUM_SPRITES;offs++)
		sortedlist[offs] = -1;

	/* prebuild a sorted table */
	for (offs = 0;offs < 0x800;offs += 16)
	{
		if (READ_WORD(&K053245_ram[offs]) & 0x8000)
		{
			sortedlist[READ_WORD(&K053245_ram[offs]) & 0x007f] = offs;
		}
	}

	for (pri_code = NUM_SPRITES-1;pri_code >= 0;pri_code--)
	{
		int ox,oy,color,code,size,w,h,x,y,flipx,flipy,mirrorx,mirrory,zoomx,zoomy,pri;


		offs = sortedlist[pri_code];
		if (offs == -1) continue;

		/* the following changes the sprite draw order from
			 0  1  4  5 16 17 20 21
			 2  3  6  7 18 19 22 23
			 8  9 12 13 24 25 28 29
			10 11 14 15 26 27 30 31
			32 33 36 37 48 49 52 53
			34 35 38 39 50 51 54 55
			40 41 44 45 56 57 60 61
			42 43 46 47 58 59 62 63

			to

			 0  1  2  3  4  5  6  7
			 8  9 10 11 12 13 14 15
			16 17 18 19 20 21 22 23
			24 25 26 27 28 29 30 31
			32 33 34 35 36 37 38 39
			40 41 42 43 44 45 46 47
			48 49 50 51 52 53 54 55
			56 57 58 59 60 61 62 63
		*/

		/* NOTE: from the schematics, it looks like the top 2 bits should be ignored */
		/* (there are not output pins for them), and probably taken from the "color" */
		/* field to do bank switching. However this applies only to TMNT2, with its */
		/* protection mcu creating the sprite table, so we don't know where to fetch */
		/* the bits from. */
		code = READ_WORD(&K053245_ram[offs+0x02]);
		code = ((code & 0xffe1) + ((code & 0x0010) >> 2) + ((code & 0x0008) << 1)
				 + ((code & 0x0004) >> 1) + ((code & 0x0002) << 2));
		color = READ_WORD(&K053245_ram[offs+0x0c]) & 0x00ff;
		pri = 0;

		(*K053245_callback)(&code,&color,&pri);

		size = (READ_WORD(&K053245_ram[offs]) & 0x0f00) >> 8;

		w = 1 << (size & 0x03);
		h = 1 << ((size >> 2) & 0x03);

		/* zoom control:
		   0x40 = normal scale
		  <0x40 enlarge (0x20 = double size)
		  >0x40 reduce (0x80 = half size)
		*/
		zoomy = READ_WORD(&K053245_ram[offs+0x08]);
		if (zoomy > 0x2000) continue;
		if (zoomy) zoomy = (0x400000+zoomy/2) / zoomy;
		else zoomy = 2 * 0x400000;
		if ((READ_WORD(&K053245_ram[offs]) & 0x4000) == 0)
		{
			zoomx = READ_WORD(&K053245_ram[offs+0x0a]);
			if (zoomx > 0x2000) continue;
			if (zoomx) zoomx = (0x400000+zoomx/2) / zoomx;
//			else zoomx = 2 * 0x400000;
else zoomx = zoomy; /* workaround for TMNT2 */
		}
		else zoomx = zoomy;

		ox = READ_WORD(&K053245_ram[offs+0x06]) + K053245_spriteoffsX;
		oy = READ_WORD(&K053245_ram[offs+0x04]);

		flipx = READ_WORD(&K053245_ram[offs]) & 0x1000;
		flipy = READ_WORD(&K053245_ram[offs]) & 0x2000;
		mirrorx = READ_WORD(&K053245_ram[offs+0x0c]) & 0x0100;
		mirrory = READ_WORD(&K053245_ram[offs+0x0c]) & 0x0200;

		if (K053245_flipscreenX)
		{
			ox = 512 - ox;
			if (!mirrorx) flipx = !flipx;
		}
		if (K053245_flipscreenY)
		{
			oy = -oy;
			if (!mirrory) flipy = !flipy;
		}

		ox = (ox + 0x5d) & 0x3ff;
		if (ox >= 768) ox -= 1024;
		oy = (-(oy + K053245_spriteoffsY + 0x07)) & 0x3ff;
		if (oy >= 640) oy -= 1024;

		/* the coordinates given are for the *center* of the sprite */
		ox -= (zoomx * w) >> 13;
		oy -= (zoomy * h) >> 13;

		for (y = 0;y < h;y++)
		{
			int sx,sy,zw,zh;

			sy = oy + ((zoomy * y + (1<<11)) >> 12);
			zh = (oy + ((zoomy * (y+1) + (1<<11)) >> 12)) - sy;

			for (x = 0;x < w;x++)
			{
				int c,fx,fy;

				sx = ox + ((zoomx * x + (1<<11)) >> 12);
				zw = (ox + ((zoomx * (x+1) + (1<<11)) >> 12)) - sx;
				c = code;
				if (mirrorx)
				{
					if ((flipx == 0) ^ (2*x < w))
					{
						/* mirror left/right */
						c += (w-x-1);
						fx = 1;
					}
					else
					{
						c += x;
						fx = 0;
					}
				}
				else
				{
					if (flipx) c += w-1-x;
					else c += x;
					fx = flipx;
				}
				if (mirrory)
				{
					if ((flipy == 0) ^ (2*y >= h))
					{
						/* mirror top/bottom */
						c += 8*(h-y-1);
						fy = 1;
					}
					else
					{
						c += 8*y;
						fy = 0;
					}
				}
				else
				{
					if (flipy) c += 8*(h-1-y);
					else c += 8*y;
					fy = flipy;
				}

				/* the sprite can start at any point in the 8x8 grid, but it must stay */
				/* in a 64 entries window, wrapping around at the edges. The animation */
				/* at the end of the saloon level in SUnset Riders breaks otherwise. */
				c = (c & 0x3f) | (code & ~0x3f);

				if (zoomx == 0x10000 && zoomy == 0x10000)
				{
					/* hack to simulate shadow */
					if (READ_WORD(&K053245_ram[offs+0x0c]) & 0x0080)
					{
						int o = K053245_gfx->colortable[16*color+15];
						K053245_gfx->colortable[16*color+15] = palette_transparent_pen;
						pdrawgfx(bitmap,K053245_gfx,
								c,
								color,
								fx,fy,
								sx,sy,
								&Machine->visible_area,TRANSPARENCY_PENS,(cpu_getcurrentframe() & 1) ? 0x8001 : 0x0001,pri);
						K053245_gfx->colortable[16*color+15] = o;
					}
					else
					{
						pdrawgfx(bitmap,K053245_gfx,
								c,
								color,
								fx,fy,
								sx,sy,
								&Machine->visible_area,TRANSPARENCY_PEN,0,pri);
					}
				}
				else
				{
					/* hack to simulate shadow */
					if (READ_WORD(&K053245_ram[offs+0x0c]) & 0x0080)
					{
						int o = K053245_gfx->colortable[16*color+15];
						K053245_gfx->colortable[16*color+15] = palette_transparent_pen;
						pdrawgfxzoom(bitmap,K053245_gfx,
								c,
								color,
								fx,fy,
								sx,sy,
								&Machine->visible_area,TRANSPARENCY_PENS,(cpu_getcurrentframe() & 1) ? 0x8001 : 0x0001,
								(zw << 16) / 16,(zh << 16) / 16,pri);
						K053245_gfx->colortable[16*color+15] = o;
					}
					else
					{
						pdrawgfxzoom(bitmap,K053245_gfx,
								c,
								color,
								fx,fy,
								sx,sy,
								&Machine->visible_area,TRANSPARENCY_PEN,0,
								(zw << 16) / 16,(zh << 16) / 16,pri);
					}
				}
			}
		}
	}
#if 0
if (keyboard_pressed(KEYCODE_D))
{
	FILE *fp;
	fp=fopen("SPRITE.DMP", "w+b");
	if (fp)
	{
		fwrite(K053245_ram, 0x800, 1, fp);
		usrintf_showmessage("saved");
		fclose(fp);
	}
}
#endif
#undef NUM_SPRITES
}

void K053245_mark_sprites_colors(void)
{
	int offs,i;

	unsigned short palette_map[512];

	memset (palette_map, 0, sizeof (palette_map));

	/* sprites */
	for (offs = 0x800-16;offs >= 0;offs -= 16)
	{
		if (READ_WORD(&K053245_ram[offs]) & 0x8000)
		{
			int code,color,pri;

			code = READ_WORD(&K053245_ram[offs+0x02]);
			code = ((code & 0xffe1) + ((code & 0x0010) >> 2) + ((code & 0x0008) << 1)
					 + ((code & 0x0004) >> 1) + ((code & 0x0002) << 2));
			color = READ_WORD(&K053245_ram[offs+0x0c]) & 0x00ff;
			pri = 0;
			(*K053245_callback)(&code,&color,&pri);
			palette_map[color] |= 0xffff;
		}
	}

	/* now build the final table */
	for (i = 0; i < 512; i++)
	{
		int usage = palette_map[i], j;
		if (usage)
		{
			for (j = 1; j < 16; j++)
				if (usage & (1 << j))
					palette_used_colors[i * 16 + j] |= PALETTE_COLOR_VISIBLE;
		}
	}
}




static int K053247_memory_region;
static struct GfxElement *K053247_gfx;
static void (*K053247_callback)(int *code,int *color,int *priority);
static int K053246_OBJCHA_line;
static int K053246_romoffset;
static int K053247_flipscreenX,K053247_flipscreenY;
static int K053247_spriteoffsX,K053247_spriteoffsY;
static unsigned char *K053247_ram;
static int K053247_irq_enabled;


int K053247_vh_start(int gfx_memory_region,int plane0,int plane1,int plane2,int plane3,
		void (*callback)(int *code,int *color,int *priority))
{
	int gfx_index;
	static struct GfxLayout spritelayout =
	{
		16,16,
		0,				/* filled in later */
		4,
		{ 0, 0, 0, 0 },	/* filled in later */
		{ 2*4, 3*4, 0*4, 1*4, 6*4, 7*4, 4*4, 5*4,
				10*4, 11*4, 8*4, 9*4, 14*4, 15*4, 12*4, 13*4 },
		{ 0*64, 1*64, 2*64, 3*64, 4*64, 5*64, 6*64, 7*64,
				8*64, 9*64, 10*64, 11*64, 12*64, 13*64, 14*64, 15*64 },
		128*8
	};


	/* find first empty slot to decode gfx */
	for (gfx_index = 0; gfx_index < MAX_GFX_ELEMENTS; gfx_index++)
		if (Machine->gfx[gfx_index] == 0)
			break;
	if (gfx_index == MAX_GFX_ELEMENTS)
		return 1;

	/* tweak the structure for the number of tiles we have */
	spritelayout.total = memory_region_length(gfx_memory_region) / 128;
	spritelayout.planeoffset[0] = plane0;
	spritelayout.planeoffset[1] = plane1;
	spritelayout.planeoffset[2] = plane2;
	spritelayout.planeoffset[3] = plane3;

	/* decode the graphics */
	Machine->gfx[gfx_index] = decodegfx(memory_region(gfx_memory_region),&spritelayout);
	if (!Machine->gfx[gfx_index])
		return 1;

	/* set the color information */
	Machine->gfx[gfx_index]->colortable = Machine->remapped_colortable;
	Machine->gfx[gfx_index]->total_colors = Machine->drv->color_table_len / 16;

	K053247_memory_region = gfx_memory_region;
	K053247_gfx = Machine->gfx[gfx_index];
	K053247_callback = callback;
	K053246_OBJCHA_line = CLEAR_LINE;
	K053247_ram = (unsigned char*)malloc(0x1000);
	if (!K053247_ram) return 1;

	memset(K053247_ram,0,0x1000);

	return 0;
}

void K053247_vh_stop(void)
{
	free(K053247_ram);
	K053247_ram = 0;
}

READ_HANDLER( K053247_word_r )
{
	return READ_WORD(&K053247_ram[offset]);
}

WRITE_HANDLER( K053247_word_w )
{
	COMBINE_WORD_MEM(&K053247_ram[offset],data);
}

READ_HANDLER( K053247_r )
{
	int shift = ((offset & 1) ^ 1) << 3;
	return (READ_WORD(&K053247_ram[offset & ~1]) >> shift) & 0xff;
}

WRITE_HANDLER( K053247_w )
{
	int shift = ((offset & 1) ^ 1) << 3;
	offset &= ~1;
	COMBINE_WORD_MEM(&K053247_ram[offset],(0xff000000 >> shift) | ((data & 0xff) << shift));
}

READ_HANDLER( K053246_r )
{
	if (K053246_OBJCHA_line == ASSERT_LINE)
	{
		int addr;


		addr = 2 * K053246_romoffset + ((offset & 1) ^ 1);
		addr &= memory_region_length(K053247_memory_region)-1;

#if 0
	usrintf_showmessage("%04x: offset %02x addr %06x",cpu_get_pc(),offset,addr);
#endif

		return memory_region(K053247_memory_region)[addr];
	}
	else
	{
//logerror("%04x: read from unknown 053244 address %x\n",cpu_get_pc(),offset);
		return 0;
	}
}

WRITE_HANDLER( K053246_w )
{
	if (offset == 0x00)
		K053247_spriteoffsX = (K053247_spriteoffsX & 0x00ff) | (data << 8);
	else if (offset == 0x01)
		K053247_spriteoffsX = (K053247_spriteoffsX & 0xff00) | data;
	else if (offset == 0x02)
		K053247_spriteoffsY = (K053247_spriteoffsY & 0x00ff) | (data << 8);
	else if (offset == 0x03)
		K053247_spriteoffsY = (K053247_spriteoffsY & 0xff00) | data;
	else if (offset == 0x05)
	{
		/* bit 0/1 = flip screen */
		K053247_flipscreenX = data & 0x01;
		K053247_flipscreenY = data & 0x02;

		/* bit 2 = unknown */

		/* bit 4 = interrupt enable */
		K053247_irq_enabled = data & 0x10;

		/* bit 5 = unknown */

//logerror("%04x: write %02x to 053246 address 5\n",cpu_get_pc(),data);
	}
	else if (offset >= 0x04 && offset < 0x08)   /* only 4,6,7 - 5 is handled above */
	{
		offset = 8*(((offset & 0x03) ^ 0x01) - 1);
		K053246_romoffset = (K053246_romoffset & ~(0xff << offset)) | (data << offset);
		return;
	}
//	else
//logerror("%04x: write %02x to unknown 053246 address %x\n",cpu_get_pc(),data,offset);
}

READ_HANDLER( K053246_word_r )
{
	return K053246_r(offset + 1) | (K053246_r(offset) << 8);
}

WRITE_HANDLER( K053246_word_w )
{
	if ((data & 0xff000000) == 0)
		K053246_w(offset,(data >> 8) & 0xff);
	if ((data & 0x00ff0000) == 0)
		K053246_w(offset + 1,data & 0xff);
}

void K053246_set_OBJCHA_line(int state)
{
	K053246_OBJCHA_line = state;
}

/*
 * Sprite Format
 * ------------------
 *
 * Word | Bit(s)           | Use
 * -----+-fedcba9876543210-+----------------
 *   0  | x--------------- | active (show this sprite)
 *   0  | -x-------------- | maintain aspect ratio (when set, zoom y acts on both axis)
 *   0  | --x------------- | flip y
 *   0  | ---x------------ | flip x
 *   0  | ----xxxx-------- | sprite size (see below)
 *   0  | ---------xxxxxxx | priority order
 *   1  | xxxxxxxxxxxxxxxx | sprite code
 *   2  | ------xxxxxxxxxx | y position
 *   3  | ------xxxxxxxxxx | x position
 *   4  | xxxxxxxxxxxxxxxx | zoom y (0x40 = normal, <0x40 = enlarge, >0x40 = reduce)
 *   5  | xxxxxxxxxxxxxxxx | zoom x (0x40 = normal, <0x40 = enlarge, >0x40 = reduce)
 *   6  | x--------------- | mirror y (top half is drawn as mirror image of the bottom)
 *   6  | -x-------------- | mirror x (right half is drawn as mirror image of the left)
 *   6  | -----x---------- | shadow
 *   6  | xxxxxxxxxxxxxxxx | "color", but depends on external connections
 *   7  | ---------------- |
 *
 * shadow enables transparent shadows. Note that it applies to pen 0x0f ONLY.
 * The rest of the sprite remains normal.
 */

void K053247_sprites_draw(struct osd_bitmap *bitmap)
{
#define NUM_SPRITES 256
	int offs,pri_code;
	int sortedlist[NUM_SPRITES];

	for (offs = 0;offs < NUM_SPRITES;offs++)
		sortedlist[offs] = -1;

	/* prebuild a sorted table */
	for (offs = 0;offs < 0x1000;offs += 16)
	{
//		if (READ_WORD(&K053247_ram[offs]) & 0x8000)
		sortedlist[READ_WORD(&K053247_ram[offs]) & 0x00ff] = offs;
	}

	for (pri_code = 0;pri_code < NUM_SPRITES;pri_code++)
	{
		int ox,oy,color,code,size,w,h,x,y,xa,ya,flipx,flipy,mirrorx,mirrory,zoomx,zoomy,pri;
		/* sprites can be grouped up to 8x8. The draw order is
			 0  1  4  5 16 17 20 21
			 2  3  6  7 18 19 22 23
			 8  9 12 13 24 25 28 29
			10 11 14 15 26 27 30 31
			32 33 36 37 48 49 52 53
			34 35 38 39 50 51 54 55
			40 41 44 45 56 57 60 61
			42 43 46 47 58 59 62 63
		*/
		static int xoffset[8] = { 0, 1, 4, 5, 16, 17, 20, 21 };
		static int yoffset[8] = { 0, 2, 8, 10, 32, 34, 40, 42 };
		static int offsetkludge;


		offs = sortedlist[pri_code];
		if (offs == -1) continue;

		if ((READ_WORD(&K053247_ram[offs]) & 0x8000) == 0) continue;

		code = READ_WORD(&K053247_ram[offs+0x02]);
		color = READ_WORD(&K053247_ram[offs+0x0c]);
		pri = 0;

		(*K053247_callback)(&code,&color,&pri);

		size = (READ_WORD(&K053247_ram[offs]) & 0x0f00) >> 8;

		w = 1 << (size & 0x03);
		h = 1 << ((size >> 2) & 0x03);

		/* the sprite can start at any point in the 8x8 grid. We have to */
		/* adjust the offsets to draw it correctly. Simpsons does this all the time. */
		xa = 0;
		ya = 0;
		if (code & 0x01) xa += 1;
		if (code & 0x02) ya += 1;
		if (code & 0x04) xa += 2;
		if (code & 0x08) ya += 2;
		if (code & 0x10) xa += 4;
		if (code & 0x20) ya += 4;
		code &= ~0x3f;


		/* zoom control:
		   0x40 = normal scale
		  <0x40 enlarge (0x20 = double size)
		  >0x40 reduce (0x80 = half size)
		*/
		zoomy = READ_WORD(&K053247_ram[offs+0x08]);
		if (zoomy > 0x2000) continue;
		if (zoomy) zoomy = (0x400000+zoomy/2) / zoomy;
		else zoomy = 2 * 0x400000;
		if ((READ_WORD(&K053247_ram[offs]) & 0x4000) == 0)
		{
			zoomx = READ_WORD(&K053247_ram[offs+0x0a]);
			if (zoomx > 0x2000) continue;
			if (zoomx) zoomx = (0x400000+zoomx/2) / zoomx;
			else zoomx = 2 * 0x400000;
		}
		else zoomx = zoomy;

		ox = READ_WORD(&K053247_ram[offs+0x06]);
		oy = READ_WORD(&K053247_ram[offs+0x04]);

/* TODO: it is not known how the global Y offset works */
switch (K053247_spriteoffsY)
{
	case 0x0261:	/* simpsons */
	case 0x0262:	/* simpsons (dreamland) */
	case 0x0263:	/* simpsons (dreamland) */
	case 0x0264:	/* simpsons (dreamland) */
	case 0x0265:	/* simpsons (dreamland) */
	case 0x006d:	/* simpsons flip (dreamland) */
	case 0x006e:	/* simpsons flip (dreamland) */
	case 0x006f:	/* simpsons flip (dreamland) */
	case 0x0070:	/* simpsons flip (dreamland) */
	case 0x0071:	/* simpsons flip */
		offsetkludge = 0x017;
		break;
	case 0x02f7:	/* vendetta (level 4 boss) */
	case 0x02f8:	/* vendetta (level 4 boss) */
	case 0x02f9:	/* vendetta (level 4 boss) */
	case 0x02fa:	/* vendetta */
	case 0x02fb:	/* vendetta (fat guy jumping) */
	case 0x02fc:	/* vendetta (fat guy jumping) */
	case 0x02fd:	/* vendetta (fat guy jumping) */
	case 0x02fe:	/* vendetta (fat guy jumping) */
	case 0x02ff:	/* vendetta (fat guy jumping) */
	case 0x03f7:	/* vendetta flip (level 4 boss) */
	case 0x03f8:	/* vendetta flip (level 4 boss) */
	case 0x03f9:	/* vendetta flip (level 4 boss) */
	case 0x03fa:	/* vendetta flip */
	case 0x03fb:	/* vendetta flip (fat guy jumping) */
	case 0x03fc:	/* vendetta flip (fat guy jumping) */
	case 0x03fd:	/* vendetta flip (fat guy jumping) */
	case 0x03fe:	/* vendetta flip (fat guy jumping) */
	case 0x03ff:	/* vendetta flip (fat guy jumping) */
		offsetkludge = 0x006;
		break;
	case 0x0292:	/* xmen */
	case 0x0072:	/* xmen flip */
		offsetkludge = -0x002;
		break;
	default:
		offsetkludge = 0;
			usrintf_showmessage("unknown spriteoffsY %04x",K053247_spriteoffsY);
		break;
}

		flipx = READ_WORD(&K053247_ram[offs]) & 0x1000;
		flipy = READ_WORD(&K053247_ram[offs]) & 0x2000;
		mirrorx = READ_WORD(&K053247_ram[offs+0x0c]) & 0x4000;
		mirrory = READ_WORD(&K053247_ram[offs+0x0c]) & 0x8000;

		if (K053247_flipscreenX)
		{
			ox = -ox;
			if (!mirrorx) flipx = !flipx;
		}
		if (K053247_flipscreenY)
		{
			oy = -oy;
			if (!mirrory) flipy = !flipy;
		}

		ox = (ox + 0x35 - K053247_spriteoffsX) & 0x3ff;
		if (ox >= 768) ox -= 1024;
		oy = (-(oy + K053247_spriteoffsY + offsetkludge)) & 0x3ff;
		if (oy >= 640) oy -= 1024;

		/* the coordinates given are for the *center* of the sprite */
		ox -= (zoomx * w) >> 13;
		oy -= (zoomy * h) >> 13;

		for (y = 0;y < h;y++)
		{
			int sx,sy,zw,zh;

			sy = oy + ((zoomy * y + (1<<11)) >> 12);
			zh = (oy + ((zoomy * (y+1) + (1<<11)) >> 12)) - sy;

			for (x = 0;x < w;x++)
			{
				int c,fx,fy;

				sx = ox + ((zoomx * x + (1<<11)) >> 12);
				zw = (ox + ((zoomx * (x+1) + (1<<11)) >> 12)) - sx;
				c = code;
				if (mirrorx)
				{
					if ((flipx == 0) ^ (2*x < w))
					{
						/* mirror left/right */
						c += xoffset[(w-1-x+xa)&7];
						fx = 1;
					}
					else
					{
						c += xoffset[(x+xa)&7];
						fx = 0;
					}
				}
				else
				{
					if (flipx) c += xoffset[(w-1-x+xa)&7];
					else c += xoffset[(x+xa)&7];
					fx = flipx;
				}
				if (mirrory)
				{
					if ((flipy == 0) ^ (2*y >= h))
					{
						/* mirror top/bottom */
						c += yoffset[(h-1-y+ya)&7];
						fy = 1;
					}
					else
					{
						c += yoffset[(y+ya)&7];
						fy = 0;
					}
				}
				else
				{
					if (flipy) c += yoffset[(h-1-y+ya)&7];
					else c += yoffset[(y+ya)&7];
					fy = flipy;
				}

				if (zoomx == 0x10000 && zoomy == 0x10000)
				{
					/* hack to simulate shadow */
					if (READ_WORD(&K053247_ram[offs+0x0c]) & 0x0400)
					{
						int o = K053247_gfx->colortable[16*color+15];
						K053247_gfx->colortable[16*color+15] = palette_transparent_pen;
						pdrawgfx(bitmap,K053247_gfx,
								c,
								color,
								fx,fy,
								sx,sy,
								&Machine->visible_area,TRANSPARENCY_PENS,(cpu_getcurrentframe() & 1) ? 0x8001 : 0x0001,pri);
						K053247_gfx->colortable[16*color+15] = o;
					}
					else
					{
						pdrawgfx(bitmap,K053247_gfx,
								c,
								color,
								fx,fy,
								sx,sy,
								&Machine->visible_area,TRANSPARENCY_PEN,0,pri);
					}
				}
				else
				{
					/* hack to simulate shadow */
					if (READ_WORD(&K053247_ram[offs+0x0c]) & 0x0400)
					{
						int o = K053247_gfx->colortable[16*color+15];
						K053247_gfx->colortable[16*color+15] = palette_transparent_pen;
						pdrawgfxzoom(bitmap,K053247_gfx,
								c,
								color,
								fx,fy,
								sx,sy,
								&Machine->visible_area,TRANSPARENCY_PENS,(cpu_getcurrentframe() & 1) ? 0x8001 : 0x0001,
								(zw << 16) / 16,(zh << 16) / 16,pri);
						K053247_gfx->colortable[16*color+15] = o;
					}
					else
					{
						pdrawgfxzoom(bitmap,K053247_gfx,
								c,
								color,
								fx,fy,
								sx,sy,
								&Machine->visible_area,TRANSPARENCY_PEN,0,
								(zw << 16) / 16,(zh << 16) / 16,pri);
					}
				}

				if (mirrory && h == 1)  /* Simpsons shadows */
				{
					if (zoomx == 0x10000 && zoomy == 0x10000)
					{
						/* hack to simulate shadow */
						if (READ_WORD(&K053247_ram[offs+0x0c]) & 0x0400)
						{
							int o = K053247_gfx->colortable[16*color+15];
							K053247_gfx->colortable[16*color+15] = palette_transparent_pen;
							pdrawgfx(bitmap,K053247_gfx,
									c,
									color,
									fx,!fy,
									sx,sy,
									&Machine->visible_area,TRANSPARENCY_PENS,(cpu_getcurrentframe() & 1) ? 0x8001 : 0x0001,pri);
							K053247_gfx->colortable[16*color+15] = o;
						}
						else
						{
							pdrawgfx(bitmap,K053247_gfx,
									c,
									color,
									fx,!fy,
									sx,sy,
									&Machine->visible_area,TRANSPARENCY_PEN,0,pri);
						}
					}
					else
					{
						/* hack to simulate shadow */
						if (READ_WORD(&K053247_ram[offs+0x0c]) & 0x0400)
						{
							int o = K053247_gfx->colortable[16*color+15];
							K053247_gfx->colortable[16*color+15] = palette_transparent_pen;
							pdrawgfxzoom(bitmap,K053247_gfx,
									c,
									color,
									fx,!fy,
									sx,sy,
									&Machine->visible_area,TRANSPARENCY_PENS,(cpu_getcurrentframe() & 1) ? 0x8001 : 0x0001,
									(zw << 16) / 16,(zh << 16) / 16,pri);
							K053247_gfx->colortable[16*color+15] = o;
						}
						else
						{
							pdrawgfxzoom(bitmap,K053247_gfx,
									c,
									color,
									fx,!fy,
									sx,sy,
									&Machine->visible_area,TRANSPARENCY_PEN,0,
									(zw << 16) / 16,(zh << 16) / 16,pri);
						}
					}
				}
			}
		}
	}
#if 0
if (keyboard_pressed(KEYCODE_D))
{
	FILE *fp;
	fp=fopen("SPRITE.DMP", "w+b");
	if (fp)
	{
		fwrite(K053247_ram, 0x1000, 1, fp);
		usrintf_showmessage("saved");
		fclose(fp);
	}
}
#endif
#undef NUM_SPRITES
}

void K053247_mark_sprites_colors(void)
{
	int offs,i;

	unsigned short palette_map[512];

	memset (palette_map, 0, sizeof (palette_map));

	/* sprites */
	for (offs = 0x1000-16;offs >= 0;offs -= 16)
	{
		if (READ_WORD(&K053247_ram[offs]) & 0x8000)
		{
			int code,color,pri;

			code = READ_WORD(&K053247_ram[offs+0x02]);
			color = READ_WORD(&K053247_ram[offs+0x0c]);
			pri = 0;
			(*K053247_callback)(&code,&color,&pri);
			palette_map[color] |= 0xffff;
		}
	}

	/* now build the final table */
	for (i = 0; i < 512; i++)
	{
		int usage = palette_map[i], j;
		if (usage)
		{
			for (j = 1; j < 16; j++)
				if (usage & (1 << j))
					palette_used_colors[i * 16 + j] |= PALETTE_COLOR_VISIBLE;
		}
	}
}

int K053247_is_IRQ_enabled(void)
{
	return K053247_irq_enabled;
}


#define MAX_K051316 3

static int K051316_memory_region[MAX_K051316];
static int K051316_gfxnum[MAX_K051316];
static int K051316_wraparound[MAX_K051316];
static int K051316_offset[MAX_K051316][2];
static int K051316_bpp[MAX_K051316];
static void (*K051316_callback[MAX_K051316])(int *code,int *color);
static unsigned char *K051316_ram[MAX_K051316];
static unsigned char K051316_ctrlram[MAX_K051316][16];
static struct tilemap *K051316_tilemap[MAX_K051316];
static int K051316_chip_selected;

void K051316_vh_stop(int chip);

/***************************************************************************

  Callbacks for the TileMap code

***************************************************************************/

static void K051316_preupdate(int chip)
{
	K051316_chip_selected = chip;
}

static void K051316_get_tile_info(int tile_index)
{
	int code = K051316_ram[K051316_chip_selected][tile_index];
	int color = K051316_ram[K051316_chip_selected][tile_index + 0x400];

	(*K051316_callback[K051316_chip_selected])(&code,&color);

	SET_TILE_INFO(K051316_gfxnum[K051316_chip_selected],code,color);
}


int K051316_vh_start(int chip, int gfx_memory_region,int bpp,
		void (*callback)(int *code,int *color))
{
	int gfx_index;


	/* find first empty slot to decode gfx */
	for (gfx_index = 0; gfx_index < MAX_GFX_ELEMENTS; gfx_index++)
		if (Machine->gfx[gfx_index] == 0)
			break;
	if (gfx_index == MAX_GFX_ELEMENTS)
		return 1;

	if (bpp == 4)
	{
		static struct GfxLayout charlayout =
		{
			16,16,
			0,				/* filled in later */
			4,
			{ 0, 1, 2, 3 },
			{ 0*4, 1*4, 2*4, 3*4, 4*4, 5*4, 6*4, 7*4,
					8*4, 9*4, 10*4, 11*4, 12*4, 13*4, 14*4, 15*4 },
			{ 0*64, 1*64, 2*64, 3*64, 4*64, 5*64, 6*64, 7*64,
					8*64, 9*64, 10*64, 11*64, 12*64, 13*64, 14*64, 15*64 },
			128*8
		};


		/* tweak the structure for the number of tiles we have */
		charlayout.total = memory_region_length(gfx_memory_region) / 128;

		/* decode the graphics */
		Machine->gfx[gfx_index] = decodegfx(memory_region(gfx_memory_region),&charlayout);
	}
	else if (bpp == 7)
	{
		static struct GfxLayout charlayout =
		{
			16,16,
			0,				/* filled in later */
			7,
			{ 1, 2, 3, 4, 5, 6, 7 },
			{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
					8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
			{ 0*128, 1*128, 2*128, 3*128, 4*128, 5*128, 6*128, 7*128,
					8*128, 9*128, 10*128, 11*128, 12*128, 13*128, 14*128, 15*128 },
			256*8
		};


		/* tweak the structure for the number of tiles we have */
		charlayout.total = memory_region_length(gfx_memory_region) / 256;

		/* decode the graphics */
		Machine->gfx[gfx_index] = decodegfx(memory_region(gfx_memory_region),&charlayout);
	}
	else
	{
//logerror("K051316_vh_start supports only 4 or 7 bpp\n");
		return 1;
	}

	if (!Machine->gfx[gfx_index])
		return 1;

	/* set the color information */
	Machine->gfx[gfx_index]->colortable = Machine->remapped_colortable;
	Machine->gfx[gfx_index]->total_colors = Machine->drv->color_table_len / (1 << bpp);

	K051316_memory_region[chip] = gfx_memory_region;
	K051316_gfxnum[chip] = gfx_index;
	K051316_bpp[chip] = bpp;
	K051316_callback[chip] = callback;

	K051316_tilemap[chip] = tilemap_create(K051316_get_tile_info,tilemap_scan_rows,TILEMAP_OPAQUE,16,16,32,32);

	K051316_ram[chip] = (unsigned char*)malloc(0x800);

	if (!K051316_ram[chip] || !K051316_tilemap[chip])
	{
		K051316_vh_stop(chip);
		return 1;
	}

	tilemap_set_clip(K051316_tilemap[chip],0);

	K051316_wraparound[chip] = 0;	/* default = no wraparound */
	K051316_offset[chip][0] = K051316_offset[chip][1] = 0;

	return 0;
}

int K051316_vh_start_0(int gfx_memory_region,int bpp,
		void (*callback)(int *code,int *color))
{
	return K051316_vh_start(0,gfx_memory_region,bpp,callback);
}

int K051316_vh_start_1(int gfx_memory_region,int bpp,
		void (*callback)(int *code,int *color))
{
	return K051316_vh_start(1,gfx_memory_region,bpp,callback);
}

int K051316_vh_start_2(int gfx_memory_region,int bpp,
		void (*callback)(int *code,int *color))
{
	return K051316_vh_start(2,gfx_memory_region,bpp,callback);
}


void K051316_vh_stop(int chip)
{
	free(K051316_ram[chip]);
	K051316_ram[chip] = 0;
}

void K051316_vh_stop_0(void)
{
	K051316_vh_stop(0);
}

void K051316_vh_stop_1(void)
{
	K051316_vh_stop(1);
}

void K051316_vh_stop_2(void)
{
	K051316_vh_stop(2);
}

int K051316_r(int chip, int offset)
{
	return K051316_ram[chip][offset];
}

READ_HANDLER( K051316_0_r )
{
	return K051316_r(0, offset);
}

READ_HANDLER( K051316_1_r )
{
	return K051316_r(1, offset);
}

READ_HANDLER( K051316_2_r )
{
	return K051316_r(2, offset);
}


void K051316_w(int chip,int offset,int data)
{
	if (K051316_ram[chip][offset] != data)
	{
		K051316_ram[chip][offset] = data;
		tilemap_mark_tile_dirty(K051316_tilemap[chip],offset & 0x3ff);
	}
}

WRITE_HANDLER( K051316_0_w )
{
	K051316_w(0,offset,data);
}

WRITE_HANDLER( K051316_1_w )
{
	K051316_w(1,offset,data);
}

WRITE_HANDLER( K051316_2_w )
{
	K051316_w(2,offset,data);
}


int K051316_rom_r(int chip, int offset)
{
	if ((K051316_ctrlram[chip][0x0e] & 0x01) == 0)
	{
		int addr;

		addr = offset + (K051316_ctrlram[chip][0x0c] << 11) + (K051316_ctrlram[chip][0x0d] << 19);
		if (K051316_bpp[chip] <= 4) addr /= 2;
		addr &= memory_region_length(K051316_memory_region[chip])-1;

#if 0
	usrintf_showmessage("%04x: offset %04x addr %04x",cpu_get_pc(),offset,addr);
#endif

		return memory_region(K051316_memory_region[chip])[addr];
	}
	else
	{
//logerror("%04x: read 051316 ROM offset %04x but reg 0x0c bit 0 not clear\n",cpu_get_pc(),offset);
		return 0;
	}
}

READ_HANDLER( K051316_rom_0_r )
{
	return K051316_rom_r(0,offset);
}

READ_HANDLER( K051316_rom_1_r )
{
	return K051316_rom_r(1,offset);
}

READ_HANDLER( K051316_rom_2_r )
{
	return K051316_rom_r(2,offset);
}



void K051316_ctrl_w(int chip,int offset,int data)
{
	K051316_ctrlram[chip][offset] = data;
//if (offset >= 0x0c) logerror("%04x: write %02x to 051316 reg %x\n",cpu_get_pc(),data,offset);
}

WRITE_HANDLER( K051316_ctrl_0_w )
{
	K051316_ctrl_w(0,offset,data);
}

WRITE_HANDLER( K051316_ctrl_1_w )
{
	K051316_ctrl_w(1,offset,data);
}

WRITE_HANDLER( K051316_ctrl_2_w )
{
	K051316_ctrl_w(2,offset,data);
}

void K051316_wraparound_enable(int chip, int status)
{
	K051316_wraparound[chip] = status;
}

void K051316_set_offset(int chip, int xoffs, int yoffs)
{
	K051316_offset[chip][0] = xoffs;
	K051316_offset[chip][1] = yoffs;
}

void K051316_tilemap_update(int chip)
{
	K051316_preupdate(chip);
	tilemap_update(K051316_tilemap[chip]);
}

void K051316_tilemap_update_0(void)
{
	K051316_tilemap_update(0);
}

void K051316_tilemap_update_1(void)
{
	K051316_tilemap_update(1);
}

void K051316_tilemap_update_2(void)
{
	K051316_tilemap_update(2);
}


void K051316_zoom_draw(int chip, struct osd_bitmap *bitmap,UINT32 priority)
{
	UINT32 startx,starty;
	int incxx,incxy,incyx,incyy;
	struct osd_bitmap *srcbitmap = K051316_tilemap[chip]->pixmap;

	startx = 256 * ((INT16)(256 * K051316_ctrlram[chip][0x00] + K051316_ctrlram[chip][0x01]));
	incxx  =        (INT16)(256 * K051316_ctrlram[chip][0x02] + K051316_ctrlram[chip][0x03]);
	incyx  =        (INT16)(256 * K051316_ctrlram[chip][0x04] + K051316_ctrlram[chip][0x05]);
	starty = 256 * ((INT16)(256 * K051316_ctrlram[chip][0x06] + K051316_ctrlram[chip][0x07]));
	incxy  =        (INT16)(256 * K051316_ctrlram[chip][0x08] + K051316_ctrlram[chip][0x09]);
	incyy  =        (INT16)(256 * K051316_ctrlram[chip][0x0a] + K051316_ctrlram[chip][0x0b]);

	startx -= (16 + K051316_offset[chip][1]) * incyx;
	starty -= (16 + K051316_offset[chip][1]) * incyy;

	startx -= (89 + K051316_offset[chip][0]) * incxx;
	starty -= (89 + K051316_offset[chip][0]) * incxy;

	copyrozbitmap(bitmap,srcbitmap,startx << 5,starty << 5,
			incxx << 5,incxy << 5,incyx << 5,incyy << 5,K051316_wraparound[chip],
			&Machine->visible_area,TRANSPARENCY_PEN,palette_transparent_pen,priority);
#if 0
	usrintf_showmessage("%02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x",
			K051316_ctrlram[chip][0x00],
			K051316_ctrlram[chip][0x01],
			K051316_ctrlram[chip][0x02],
			K051316_ctrlram[chip][0x03],
			K051316_ctrlram[chip][0x04],
			K051316_ctrlram[chip][0x05],
			K051316_ctrlram[chip][0x06],
			K051316_ctrlram[chip][0x07],
			K051316_ctrlram[chip][0x08],
			K051316_ctrlram[chip][0x09],
			K051316_ctrlram[chip][0x0a],
			K051316_ctrlram[chip][0x0b],
			K051316_ctrlram[chip][0x0c],	/* bank for ROM testing */
			K051316_ctrlram[chip][0x0d],
			K051316_ctrlram[chip][0x0e],	/* 0 = test ROMs */
			K051316_ctrlram[chip][0x0f]);
#endif
}

void K051316_zoom_draw_0(struct osd_bitmap *bitmap,UINT32 priority)
{
	K051316_zoom_draw(0,bitmap,priority);
}

void K051316_zoom_draw_1(struct osd_bitmap *bitmap,UINT32 priority)
{
	K051316_zoom_draw(1,bitmap,priority);
}

void K051316_zoom_draw_2(struct osd_bitmap *bitmap,UINT32 priority)
{
	K051316_zoom_draw(2,bitmap,priority);
}




static unsigned char K053251_ram[16];
static int K053251_palette_index[5];

WRITE_HANDLER( K053251_w )
{
	data &= 0x3f;

	if (K053251_ram[offset] != data)
	{
		K053251_ram[offset] = data;
		if (offset == 9)
		{
			/* palette base index */
			K053251_palette_index[0] = 32 * ((data >> 0) & 0x03);
			K053251_palette_index[1] = 32 * ((data >> 2) & 0x03);
			K053251_palette_index[2] = 32 * ((data >> 4) & 0x03);
			tilemap_mark_all_tiles_dirty(ALL_TILEMAPS);
		}
		else if (offset == 10)
		{
			/* palette base index */
			K053251_palette_index[3] = 16 * ((data >> 0) & 0x07);
			K053251_palette_index[4] = 16 * ((data >> 3) & 0x07);
			tilemap_mark_all_tiles_dirty(ALL_TILEMAPS);
		}
#if 0
else
{
logerror("%04x: write %02x to K053251 register %04x\n",cpu_get_pc(),data&0xff,offset);
usrintf_showmessage("pri = %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x",
	K053251_ram[0],K053251_ram[1],K053251_ram[2],K053251_ram[3],
	K053251_ram[4],K053251_ram[5],K053251_ram[6],K053251_ram[7],
	K053251_ram[8],K053251_ram[9],K053251_ram[10],K053251_ram[11],
	K053251_ram[12],K053251_ram[13],K053251_ram[14],K053251_ram[15]
	);
}
#endif
	}
}

int K053251_get_priority(int ci)
{
	return K053251_ram[ci];
}

int K053251_get_palette_index(int ci)
{
	return K053251_palette_index[ci];
}



static unsigned char K054000_ram[0x20];

WRITE_HANDLER( K054000_w )
{
#if VERBOSE
logerror("%04x: write %02x to 054000 address %02x\n",cpu_get_pc(),data,offset);
#endif

	K054000_ram[offset] = data;
}

READ_HANDLER( K054000_r )
{
	int Acx,Acy,Aax,Aay;
	int Bcx,Bcy,Bax,Bay;


#if VERBOSE
logerror("%04x: read 054000 address %02x\n",cpu_get_pc(),offset);
#endif

	if (offset != 0x18) return 0;


	Acx = (K054000_ram[0x01] << 16) | (K054000_ram[0x02] << 8) | K054000_ram[0x03];
	Acy = (K054000_ram[0x09] << 16) | (K054000_ram[0x0a] << 8) | K054000_ram[0x0b];
/* TODO: this is a hack to make thndrx2 pass the startup check. It is certainly wrong. */
if (K054000_ram[0x04] == 0xff) Acx+=3;
if (K054000_ram[0x0c] == 0xff) Acy+=3;
	Aax = K054000_ram[0x06] + 1;
	Aay = K054000_ram[0x07] + 1;

	Bcx = (K054000_ram[0x15] << 16) | (K054000_ram[0x16] << 8) | K054000_ram[0x17];
	Bcy = (K054000_ram[0x11] << 16) | (K054000_ram[0x12] << 8) | K054000_ram[0x13];
	Bax = K054000_ram[0x0e] + 1;
	Bay = K054000_ram[0x0f] + 1;

	if (Acx + Aax < Bcx - Bax)
		return 1;

	if (Bcx + Bax < Acx - Aax)
		return 1;

	if (Acy + Aay < Bcy - Bay)
		return 1;

	if (Bcy + Bay < Acy - Aay)
		return 1;

	return 0;
}



static unsigned char K051733_ram[0x20];

WRITE_HANDLER( K051733_w )
{
#if VERBOSE
logerror("%04x: write %02x to 051733 address %02x\n",cpu_get_pc(),data,offset);
#endif

	K051733_ram[offset] = data;
}

READ_HANDLER( K051733_r )
{
	int op1 = (K051733_ram[0x00] << 8) | K051733_ram[0x01];
	int op2 = (K051733_ram[0x02] << 8) | K051733_ram[0x03];

	int rad = (K051733_ram[0x06] << 8) | K051733_ram[0x07];
	int yobj1c = (K051733_ram[0x08] << 8) | K051733_ram[0x09];
	int xobj1c = (K051733_ram[0x0a] << 8) | K051733_ram[0x0b];
	int yobj2c = (K051733_ram[0x0c] << 8) | K051733_ram[0x0d];
	int xobj2c = (K051733_ram[0x0e] << 8) | K051733_ram[0x0f];

#if VERBOSE
logerror("%04x: read 051733 address %02x\n",cpu_get_pc(),offset);
#endif

	switch(offset){
		case 0x00:
			if (op2) return	((op1/op2) >> 8);
			else return 0xff;
		case 0x01:
			if (op2) return	op1/op2;
			else return 0xff;

		/* this is completely unverified */
		case 0x02:
			if (op2) return	((op1%op2) >> 8);
			else return 0xff;
		case 0x03:
			if (op2) return	op1%op2;
			else return 0xff;

		case 0x07:{
			if (xobj1c + rad < xobj2c - rad)
				return 0x80;

			if (xobj2c + rad < xobj1c - rad)
				return 0x80;

			if (yobj1c + rad < yobj2c - rad)
				return 0x80;

			if (yobj2c + rad < yobj1c - rad)
				return 0x80;

			return 0;
		}
		default:
			return K051733_ram[offset];
	}
}
