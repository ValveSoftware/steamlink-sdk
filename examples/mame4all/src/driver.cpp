/******************************************************************************

  driver.c

  The list of all available drivers. Drivers have to be included here to be
  recognized by the executable.

  To save some typing, we use a hack here. This file is recursively #included
  twice, with different definitions of the DRIVER() macro. The first one
  declares external references to the drivers; the second one builds an array
  storing all the drivers.

******************************************************************************/

#include "driver.h"


#ifndef DRIVER_RECURSIVE

/* The "root" driver, defined so we can have &driver_##NAME in macros. */
struct GameDriver driver_0 =
{
	__FILE__,
	0,
	"",
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	NOT_A_DRIVER
};

#endif

#ifdef TINY_COMPILE
extern struct GameDriver TINY_NAME;

const struct GameDriver *drivers[] =
{
	&TINY_NAME,
	0	/* end of array */
};

#else

#ifndef DRIVER_RECURSIVE

#define DRIVER_RECURSIVE

/* step 1: declare all external references */
#define DRIVER(NAME) extern struct GameDriver driver_##NAME;
#define TESTDRIVER(NAME) extern struct GameDriver driver_##NAME;
#include "driver.cpp"

/* step 2: define the drivers[] array */
#undef DRIVER
#undef TESTDRIVER
#define DRIVER(NAME) &driver_##NAME,
#define TESTDRIVER(NAME)
const struct GameDriver *drivers[] =
{
#include "driver.cpp"
	0	/* end of array */
};

#else	/* DRIVER_RECURSIVE */

	/* "Pacman hardware" games */
	DRIVER( pacman )	/* (c) 1980 Namco */
	DRIVER( pacmanjp )	/* (c) 1980 Namco */
	DRIVER( pacmanm )	/* (c) 1980 Midway */
	DRIVER( npacmod )	/* (c) 1981 Namco */
	DRIVER( pacmod )	/* (c) 1981 Midway */
	DRIVER( hangly )	/* hack */
	DRIVER( hangly2 )	/* hack */
	DRIVER( puckman )	/* hack */
	DRIVER( pacheart )	/* hack */
	DRIVER( piranha )	/* hack */
	DRIVER( pacplus )
	DRIVER( mspacman )	/* (c) 1981 Midway (but it's a bootleg) */	/* made by Gencomp */
	DRIVER( mspacatk )	/* hack */
	DRIVER( pacgal )	/* hack */
	DRIVER( maketrax )	/* (c) 1981 Williams, high score table says KRL (fur Kural) */
	DRIVER( crush )		/* (c) 1981 Kural Samno Electric Ltd */
	DRIVER( crush2 )	/* (c) 1981 Kural Esco Electric Ltd - bootleg? */
	DRIVER( crush3 )	/* Kural Electric Ltd - bootleg? */
	DRIVER( mbrush )	/* 1981 bootleg */
	DRIVER( paintrlr )	/* 1981 bootleg */
	DRIVER( eyes )		/* (c) 1982 Digitrex Techstar + "Rockola presents" */
	DRIVER( eyes2 )		/* (c) 1982 Techstar + "Rockola presents" */
	DRIVER( mrtnt )		/* (c) 1983 Telko */
	DRIVER( ponpoko )	/* (c) 1982 Sigma Ent. Inc. */
	DRIVER( ponpokov )	/* (c) 1982 Sigma Ent. Inc. + Venture Line license */
	DRIVER( lizwiz )	/* (c) 1985 Techstar + "Sunn presents" */
	DRIVER( theglob )	/* (c) 1983 Epos Corporation */
	DRIVER( beastf )	/* (c) 1984 Epos Corporation */
	DRIVER( jumpshot )
	DRIVER( dremshpr )	/* (c) 1982 Sanritsu */
	DRIVER( vanvan )	/* (c) 1983 Karateco (bootleg?) */
	DRIVER( vanvans )	/* (c) 1983 Sanritsu */
	DRIVER( alibaba )	/* (c) 1982 Sega */
	DRIVER( pengo )		/* 834-0386 (c) 1982 Sega */
	DRIVER( pengo2 )	/* 834-0386 (c) 1982 Sega */
	DRIVER( pengo2u )	/* 834-0386 (c) 1982 Sega */
	DRIVER( penta )		/* bootleg */
	DRIVER( jrpacman )	/* (c) 1983 Midway */

	/* "Galaxian hardware" games */
	DRIVER( galaxian )	/* (c) Namco */
	DRIVER( galmidw )	/* (c) Midway */
	DRIVER( superg )	/* hack */
	DRIVER( galaxb )	/* bootleg */
	DRIVER( galapx )	/* hack */
	DRIVER( galap1 )	/* hack */
	DRIVER( galap4 )	/* hack */
	DRIVER( galturbo )	/* hack */
	DRIVER( swarm )		/* hack */
	DRIVER( zerotime )	/* hack */
	DRIVER( pisces )	/* ? */
	DRIVER( uniwars )	/* (c) Irem */
	DRIVER( gteikoku )	/* (c) Irem */
	DRIVER( spacbatt )	/* bootleg */
	DRIVER( warofbug )	/* (c) 1981 Armenia */
	DRIVER( redufo )	/* bootleg - original should be (c) Artic */
	DRIVER( exodus )	/* Subelectro - bootleg? */
	DRIVER( pacmanbl )	/* bootleg */
	DRIVER( devilfsg )	/* (c) 1984 Vision / Artic (bootleg?) */
	DRIVER( zigzag )	/* (c) 1982 LAX */
	DRIVER( zigzag2 )	/* (c) 1982 LAX */
	DRIVER( jumpbug )	/* (c) 1981 Rock-ola */
	DRIVER( jumpbugb )	/* (c) 1981 Sega */
	DRIVER( levers )	/* (c) 1983 Rock-ola */
	DRIVER( azurian )	/* (c) 1982 Rait Electronics Ltd */
	DRIVER( orbitron )	/* Signatron USA */
	DRIVER( mooncrgx )	/* bootleg */
	DRIVER( mooncrst )	/* (c) 1980 Nichibutsu */
	DRIVER( mooncrsg )	/* (c) 1980 Gremlin */
	DRIVER( smooncrs )	/* Gremlin */
	DRIVER( mooncrsb )	/* bootleg */
	DRIVER( mooncrs2 )	/* bootleg */
	DRIVER( fantazia )	/* bootleg */
	DRIVER( eagle )		/* (c) Centuri */
	DRIVER( eagle2 )	/* (c) Centuri */
	DRIVER( moonqsr )	/* (c) 1980 Nichibutsu */
	DRIVER( checkman )	/* (c) 1982 Zilec-Zenitone */
	DRIVER( checkmaj )	/* (c) 1982 Jaleco (Zenitone/Zilec in ROM CM4, and the programmer names) */
	DRIVER( streakng )	/* [1980] Shoei */
	DRIVER( blkhole )	/* TDS (Tokyo Denshi Sekkei) */
	DRIVER( moonal2 )	/* Nichibutsu */
	DRIVER( moonal2b )	/* Nichibutsu */
	DRIVER( kingball )	/* (c) 1980 Namco */
	DRIVER( kingbalj )	/* (c) 1980 Namco */

	/* "Scramble hardware" (and variations) games */
	DRIVER( scramble )	/* GX387 (c) 1981 Konami */
	DRIVER( scrambls )	/* GX387 (c) 1981 Stern */
	DRIVER( scramblb )	/* bootleg */
	DRIVER( atlantis )	/* (c) 1981 Comsoft */
	DRIVER( atlants2 )	/* (c) 1981 Comsoft */
	DRIVER( theend )	/* (c) 1980 Konami */
	DRIVER( theends )	/* (c) 1980 Stern */
	DRIVER( ckongs )	/* bootleg */
	DRIVER( froggers )	/* bootleg */
	DRIVER( amidars )	/* (c) 1982 Konami */
	DRIVER( triplep )	/* (c) 1982 KKI */
	DRIVER( knockout )	/* (c) 1982 KKK */
	DRIVER( mariner )	/* (c) 1981 Amenip */
	DRIVER( 800fath )	/* (c) 1981 Amenip + U.S. Billiards license */
	DRIVER( mars )		/* (c) 1981 Artic */
	DRIVER( devilfsh )	/* (c) 1982 Artic */
	DRIVER( newsin7 )	/* (c) 1983 ATW USA, Inc. */
	DRIVER( hotshock )	/* (c) 1982 E.G. Felaco */
	DRIVER( hunchbks )	/* (c) 1983 Century */
	DRIVER( scobra )	/* GX316 (c) 1981 Konami */
	DRIVER( scobras )	/* GX316 (c) 1981 Stern */
	DRIVER( scobrab )	/* GX316 (c) 1981 Karateco (bootleg?) */
	DRIVER( stratgyx )	/* GX306 (c) 1981 Konami */
	DRIVER( stratgys )	/* GX306 (c) 1981 Stern */
	DRIVER( armorcar )	/* (c) 1981 Stern */
	DRIVER( armorca2 )	/* (c) 1981 Stern */
	DRIVER( moonwar2 )	/* (c) 1981 Stern */
	DRIVER( monwar2a )	/* (c) 1981 Stern */
	DRIVER( spdcoin )	/* (c) 1984 Stern */
	DRIVER( darkplnt )	/* (c) 1982 Stern */
	DRIVER( tazmania )	/* (c) 1982 Stern */
	DRIVER( tazmani2 )	/* (c) 1982 Stern */
	DRIVER( calipso )	/* (c) 1982 Tago */
	DRIVER( anteater )	/* (c) 1982 Tago */
	DRIVER( rescue )	/* (c) 1982 Stern */
	DRIVER( minefld )	/* (c) 1983 Stern */
	DRIVER( losttomb )	/* (c) 1982 Stern */
	DRIVER( losttmbh )	/* (c) 1982 Stern */
	DRIVER( superbon )	/* bootleg */
	DRIVER( hustler )	/* GX343 (c) 1981 Konami */
	DRIVER( billiard )	/* bootleg */
	DRIVER( hustlerb )	/* bootleg */
	DRIVER( frogger )	/* GX392 (c) 1981 Konami */
	DRIVER( frogseg1 )	/* (c) 1981 Sega */
	DRIVER( frogseg2 )	/* 834-0068 (c) 1981 Sega */
	DRIVER( froggrmc )	/* 800-3110 (c) 1981 Sega */
	DRIVER( amidar )	/* GX337 (c) 1981 Konami */
	DRIVER( amidaru )	/* GX337 (c) 1982 Konami + Stern license */
	DRIVER( amidaro )	/* GX337 (c) 1982 Konami + Olympia license */
	DRIVER( amigo )		/* bootleg */
	DRIVER( turtles )	/* (c) 1981 Stern */
	DRIVER( turpin )	/* (c) 1981 Sega */
	DRIVER( 600 )		/* GX353 (c) 1981 Konami */
	DRIVER( flyboy )	/* (c) 1982 Kaneko */
	DRIVER( flyboyb )	/* bootleg */
	DRIVER( fastfred )	/* (c) 1982 Atari */
	DRIVER( jumpcoas )	/* (c) 1983 Kaneko */

	/* "Crazy Climber hardware" games */
	DRIVER( cclimber )	/* (c) 1980 Nichibutsu */
	DRIVER( cclimbrj )	/* (c) 1980 Nichibutsu */
	DRIVER( ccboot )	/* bootleg */
	DRIVER( ccboot2 )	/* bootleg */
	DRIVER( ckong )		/* (c) 1981 Falcon */
	DRIVER( ckonga )	/* (c) 1981 Falcon */
	DRIVER( ckongjeu )	/* bootleg */
	DRIVER( ckongo )	/* bootleg */
	DRIVER( ckongalc )	/* bootleg */
	DRIVER( monkeyd )	/* bootleg */
	DRIVER( rpatrolb )	/* bootleg */
	DRIVER( silvland )	/* Falcon */
	DRIVER( yamato )	/* (c) 1983 Sega */
	DRIVER( yamato2 )	/* (c) 1983 Sega */
	DRIVER( swimmer )	/* (c) 1982 Tehkan */
	DRIVER( swimmera )	/* (c) 1982 Tehkan */
	DRIVER( guzzler )	/* (c) 1983 Tehkan */

	/* Nichibutsu games */
	DRIVER( friskyt )	/* (c) 1981 */
	DRIVER( radrad )	/* (c) 1982 Nichibutsu USA */
	DRIVER( seicross )	/* (c) 1984 + Alice */
	DRIVER( sectrzon )	/* (c) 1984 + Alice */
	DRIVER( wiping )	/* (c) 1982 */
	DRIVER( rugrats )	/* (c) 1983 */
	DRIVER( cop01 )		/* (c) 1985 */
	DRIVER( cop01a )	/* (c) 1985 */
	DRIVER( terracre )	/* (c) 1985 */
	DRIVER( terracrb )	/* (c) 1985 */
	DRIVER( terracra )	/* (c) 1985 */
	DRIVER( galivan )	/* (c) 1985 */
	DRIVER( galivan2 )	/* (c) 1985 */
	DRIVER( dangar )	/* (c) 1986 */
	DRIVER( dangar2 )	/* (c) 1986 */
	DRIVER( dangarb )	/* bootleg */
	DRIVER( ninjemak )	/* (c) 1986 (US?) */
	DRIVER( youma )		/* (c) 1986 (Japan) */
	DRIVER( terraf )	/* (c) 1987 */
	DRIVER( terrafu )	/* (c) 1987 Nichibutsu USA */
	DRIVER( kodure )	/* (c) 1987 (Japan) */
	DRIVER( armedf )	/* (c) 1988 */
	DRIVER( cclimbr2 )	/* (c) 1988 (Japan) */

	/* "Phoenix hardware" (and variations) games */
	DRIVER( safarir )	/* Shin Nihon Kikaku (SNK) */
	DRIVER( phoenix )	/* (c) 1980 Amstar */
	DRIVER( phoenixa )	/* (c) 1980 Amstar + Centuri license */
	DRIVER( phoenixt )	/* (c) 1980 Taito */
	DRIVER( phoenix3 )	/* bootleg */
	DRIVER( phoenixc )	/* bootleg */
	DRIVER( pleiads )	/* (c) 1981 Tehkan */
	DRIVER( pleiadbl )	/* bootleg */
	DRIVER( pleiadce )	/* (c) 1981 Centuri + Tehkan */
	DRIVER( naughtyb )	/* (c) 1982 Jaleco */
	DRIVER( naughtya )	/* bootleg */
	DRIVER( naughtyc )	/* (c) 1982 Jaleco + Cinematronics */
	DRIVER( popflame )	/* (c) 1982 Jaleco */
	DRIVER( popflama )	/* (c) 1982 Jaleco */
TESTDRIVER( popflamb )

	/* Namco games (plus some intruders on similar hardware) */
	DRIVER( geebee )	/* [1978] Namco */
	DRIVER( geebeeg )	/* [1978] Gremlin */
	DRIVER( bombbee )	/* [1979] Namco */
	DRIVER( cutieq )	/* (c) 1979 Namco */
	DRIVER( navalone )	/* (c) 1980 Namco */
	DRIVER( kaitei )	/* [1980] K.K. Tokki */
	DRIVER( kaitein )	/* [1980] Namco */
	DRIVER( sos )		/* [1980] Namco */
	DRIVER( tankbatt )	/* (c) 1980 Namco */
	DRIVER( warpwarp )	/* (c) 1981 Namco */
	DRIVER( warpwarr )	/* (c) 1981 Rock-ola - the high score table says "NAMCO" */
	DRIVER( warpwar2 )	/* (c) 1981 Rock-ola - the high score table says "NAMCO" */
	DRIVER( rallyx )	/* (c) 1980 Namco */
	DRIVER( rallyxm )	/* (c) 1980 Midway */
	DRIVER( nrallyx )	/* (c) 1981 Namco */
	DRIVER( jungler )	/* GX327 (c) 1981 Konami */
	DRIVER( junglers )	/* GX327 (c) 1981 Stern */
	DRIVER( locomotn )	/* GX359 (c) 1982 Konami + Centuri license */
	DRIVER( gutangtn )	/* GX359 (c) 1982 Konami + Sega license */
	DRIVER( cottong )	/* bootleg */
	DRIVER( commsega )	/* (c) 1983 Sega */
	/* the following ones all have a custom I/O chip */
	DRIVER( bosco )		/* (c) 1981 */
	DRIVER( boscoo )	/* (c) 1981 */
	DRIVER( boscoo2 )	/* (c) 1981 */
	DRIVER( boscomd )	/* (c) 1981 Midway */
	DRIVER( boscomdo )	/* (c) 1981 Midway */
	DRIVER( galaga )	/* (c) 1981 */
	DRIVER( galagamw )	/* (c) 1981 Midway */
	DRIVER( galagads )	/* hack */
	DRIVER( gallag )	/* bootleg */
	DRIVER( galagab2 )	/* bootleg */
	DRIVER( galaga84 )	/* hack */
	DRIVER( nebulbee )	/* hack */
	DRIVER( digdug )	/* (c) 1982 */
	DRIVER( digdugb )	/* (c) 1982 */
	DRIVER( digdugat )	/* (c) 1982 Atari */
	DRIVER( dzigzag )	/* bootleg */
	DRIVER( xevious )	/* (c) 1982 */
	DRIVER( xeviousa )	/* (c) 1982 + Atari license */
	DRIVER( xevios )	/* bootleg */
	DRIVER( sxevious )	/* (c) 1984 */
	DRIVER( superpac )	/* (c) 1982 */
	DRIVER( superpcm )	/* (c) 1982 Midway */
	DRIVER( pacnpal )	/* (c) 1983 */
	DRIVER( pacnpal2 )	/* (c) 1983 */
	DRIVER( pacnchmp )	/* (c) 1983 */
	DRIVER( phozon )	/* (c) 1983 */
	DRIVER( mappy )		/* (c) 1983 */
	DRIVER( mappyjp )	/* (c) 1983 */
	DRIVER( digdug2 )	/* (c) 1985 */
	DRIVER( digdug2a )	/* (c) 1985 */
	DRIVER( todruaga )	/* (c) 1984 */
	DRIVER( todruagb )	/* (c) 1984 */
	DRIVER( motos )		/* (c) 1985 */
	DRIVER( grobda )	/* (c) 1984 */
	DRIVER( grobda2 )	/* (c) 1984 */
	DRIVER( grobda3 )	/* (c) 1984 */
	DRIVER( gaplus )	/* (c) 1984 */
	DRIVER( gaplusa )	/* (c) 1984 */
	DRIVER( galaga3 )	/* (c) 1984 */
	DRIVER( galaga3a )	/* (c) 1984 */
	/* Libble Rabble board (first Japanese game using a 68000) */
TESTDRIVER( liblrabl )	/* (c) 1983 */
	DRIVER( toypop )	/* (c) 1986 */
	/* Z8000 games */
	DRIVER( polepos )	/* (c) 1982  */
	DRIVER( poleposa )	/* (c) 1982 + Atari license */
	DRIVER( polepos1 )	/* (c) 1982 Atari */
	DRIVER( topracer )	/* bootleg */
	DRIVER( polepos2 )	/* (c) 1983 */
	DRIVER( poleps2a )	/* (c) 1983 + Atari license */
	DRIVER( poleps2b )	/* bootleg */
	DRIVER( poleps2c )	/* bootleg */
	/* no custom I/O in the following, HD63701 (or compatible) microcontroller instead */
	DRIVER( pacland )	/* (c) 1984 */
	DRIVER( pacland2 )	/* (c) 1984 */
	DRIVER( pacland3 )	/* (c) 1984 */
	DRIVER( paclandm )	/* (c) 1984 Midway */
	DRIVER( drgnbstr )	/* (c) 1984 */
	DRIVER( skykid )	/* (c) 1985 */
	DRIVER( baraduke )	/* (c) 1985 */
	DRIVER( metrocrs )	/* (c) 1985 */

	/* Namco System 86 games */
	DRIVER( hopmappy )	/* (c) 1986 */
	DRIVER( skykiddx )	/* (c) 1986 */
	DRIVER( skykiddo )	/* (c) 1986 */
	DRIVER( roishtar )	/* (c) 1986 */
	DRIVER( genpeitd )	/* (c) 1986 */
	DRIVER( rthunder )	/* (c) 1986 new version */
	DRIVER( rthundro )	/* (c) 1986 old version */
	DRIVER( wndrmomo )	/* (c) 1987 */

	/* Namco System 1 games */
	DRIVER( shadowld )	/* (c) 1987 */
	DRIVER( youkaidk )	/* (c) 1987 (Japan new version) */
	DRIVER( yokaidko )	/* (c) 1987 (Japan old version) */
	DRIVER( dspirit )	/* (c) 1987 new version */
	DRIVER( dspirito )	/* (c) 1987 old version */
	DRIVER( blazer )	/* (c) 1987 (Japan) */
	DRIVER( quester )	/* (c) 1987 (Japan) */
	DRIVER( pacmania )	/* (c) 1987 */
	DRIVER( pacmanij )	/* (c) 1987 (Japan) */
	DRIVER( galaga88 )	/* (c) 1987 */
	DRIVER( galag88b )	/* (c) 1987 */
	DRIVER( galag88j )	/* (c) 1987 (Japan) */
	DRIVER( ws )		/* (c) 1988 (Japan) */
	DRIVER( berabohm )	/* (c) 1988 (Japan) */
	/* 1988 Alice in Wonderland (English version of Marchen maze) */
	DRIVER( mmaze )		/* (c) 1988 (Japan) */
TESTDRIVER( bakutotu )	/* (c) 1988 */
	DRIVER( wldcourt )	/* (c) 1988 (Japan) */
	DRIVER( splatter )	/* (c) 1988 (Japan) */
	DRIVER( faceoff )	/* (c) 1988 (Japan) */
	DRIVER( rompers )	/* (c) 1989 (Japan) */
	DRIVER( romperso )	/* (c) 1989 (Japan) */
	DRIVER( blastoff )	/* (c) 1989 (Japan) */
	DRIVER( ws89 )		/* (c) 1989 (Japan) */
	DRIVER( dangseed )	/* (c) 1989 (Japan) */
	DRIVER( ws90 )		/* (c) 1990 (Japan) */
	DRIVER( pistoldm )	/* (c) 1990 (Japan) */
	DRIVER( soukobdx )	/* (c) 1990 (Japan) */
	DRIVER( puzlclub )	/* (c) 1990 (Japan) */
	DRIVER( tankfrce )	/* (c) 1991 (US) */
	DRIVER( tankfrcj )	/* (c) 1991 (Japan) */

	/* Namco System 2 games */
TESTDRIVER( finallap )	/* 87.12 Final Lap */
TESTDRIVER( finalapd )	/* 87.12 Final Lap */
TESTDRIVER( finalapc )	/* 87.12 Final Lap */
TESTDRIVER( finlapjc )	/* 87.12 Final Lap */
TESTDRIVER( finlapjb )	/* 87.12 Final Lap */
	DRIVER( assault )	/* (c) 1988 */
	DRIVER( assaultj )	/* (c) 1988 (Japan) */
	DRIVER( assaultp )	/* (c) 1988 (Japan) */
TESTDRIVER( metlhawk )	/* (c) 1988 */
	DRIVER( mirninja )	/* (c) 1988 (Japan) */
	DRIVER( ordyne )	/* (c) 1988 */
	DRIVER( phelios )	/* (c) 1988 (Japan) */
	DRIVER( burnforc )	/* (c) 1989 (Japan) */
TESTDRIVER( dirtfoxj )	/* (c) 1989 (Japan) */
	DRIVER( finehour )	/* (c) 1989 (Japan) */
TESTDRIVER( fourtrax )	/* 89.11 */
	DRIVER( marvland )	/* (c) 1989 (US) */
	DRIVER( marvlanj )	/* (c) 1989 (Japan) */
	DRIVER( valkyrie )	/* (c) 1989 (Japan) */
	DRIVER ( kyukaidk )	/* (c) 1990 (Japan) */
	DRIVER ( kyukaido )	/* (c) 1990 (Japan) */
	DRIVER( dsaber )	/* (c) 1990 */
	DRIVER( dsaberj )	/* (c) 1990 (Japan) */
	DRIVER( rthun2 )	/* (c) 1990 */
	DRIVER( rthun2j )	/* (c) 1990 (Japan) */
TESTDRIVER( finalap2 )	/* 90.8  Final Lap 2 */
TESTDRIVER( finalp2j )	/* 90.8  Final Lap 2 (Japan) */
	/* 91.3  Steel Gunner */
	/* 91.7  Golly Ghost */
	/* 91.9  Super World Stadium */
TESTDRIVER( sgunner2 )	/* (c) 1991 (Japan) */
	DRIVER( cosmogng )	/* (c) 1991 (US) */
	DRIVER( cosmognj )	/* (c) 1991 (Japan) */
TESTDRIVER( finalap3 )	/* 92.9  Final Lap 3 */
TESTDRIVER( suzuka8h )
	/* 92.8  Bubble Trouble */
	DRIVER( sws92 )		/* (c) 1992 (Japan) */
	/* 93.4  Lucky & Wild */
TESTDRIVER( suzuk8h2 )
	DRIVER( sws93 )		/* (c) 1993 (Japan) */
	/* 93.6  Super World Stadium '93 */

	/* Universal games */
	DRIVER( cosmicg )	/* 7907 (c) 1979 */
	DRIVER( cosmica )	/* 7910 (c) [1979] */
	DRIVER( cosmica2 )	/* 7910 (c) 1979 */
	DRIVER( panic )		/* (c) 1980 */
	DRIVER( panica )	/* (c) 1980 */
	DRIVER( panicger )	/* (c) 1980 */
	DRIVER( magspot2 )	/* 8013 (c) [1980] */
	DRIVER( devzone )	/* 8022 (c) [1980] */
	DRIVER( nomnlnd )	/* (c) [1980?] */
	DRIVER( nomnlndg )	/* (c) [1980?] + Gottlieb */
	DRIVER( cheekyms )	/* (c) [1980?] */
	DRIVER( ladybug )	/* (c) 1981 */
	DRIVER( ladybugb )	/* bootleg */
	DRIVER( snapjack )	/* (c) */
	DRIVER( cavenger )	/* (c) 1981 */
	DRIVER( mrdo )		/* (c) 1982 */
	DRIVER( mrdot )		/* (c) 1982 + Taito license */
	DRIVER( mrdofix )	/* (c) 1982 + Taito license */
	DRIVER( mrlo )		/* bootleg */
	DRIVER( mrdu )		/* bootleg */
	DRIVER( mrdoy )		/* bootleg */
	DRIVER( yankeedo )	/* bootleg */
	DRIVER( docastle )	/* (c) 1983 */
	DRIVER( docastl2 )	/* (c) 1983 */
	DRIVER( douni )		/* (c) 1983 */
	DRIVER( dorunrun )	/* (c) 1984 */
	DRIVER( dorunru2 )	/* (c) 1984 */
	DRIVER( dorunruc )	/* (c) 1984 */
	DRIVER( spiero )	/* (c) 1987 */
	DRIVER( dowild )	/* (c) 1984 */
	DRIVER( jjack )		/* (c) 1984 */
	DRIVER( kickridr )	/* (c) 1984 */

	/* Nintendo games */
	DRIVER( radarscp )	/* (c) 1980 Nintendo */
	DRIVER( dkong )		/* (c) 1981 Nintendo of America */
	DRIVER( dkongjp )	/* (c) 1981 Nintendo */
	DRIVER( dkongjpo )	/* (c) 1981 Nintendo */
	DRIVER( dkongjr )	/* (c) 1982 Nintendo of America */
	DRIVER( dkngjrjp )	/* no copyright notice */
	DRIVER( dkjrjp )	/* (c) 1982 Nintendo */
	DRIVER( dkjrbl )	/* (c) 1982 Nintendo of America */
	DRIVER( dkong3 )	/* (c) 1983 Nintendo of America */
	DRIVER( dkong3j )	/* (c) 1983 Nintendo */
	DRIVER( mario )		/* (c) 1983 Nintendo of America */
	DRIVER( mariojp )	/* (c) 1983 Nintendo */
	DRIVER( masao )		/* bootleg */
	DRIVER( hunchbkd )	/* (c) 1983 Century */
	DRIVER( herbiedk )	/* (c) 1984 CVS */
TESTDRIVER( herocast )
	DRIVER( popeye )
	DRIVER( popeye2 )
	DRIVER( popeyebl )	/* bootleg */
	DRIVER( punchout )	/* (c) 1984 */
	DRIVER( spnchout )	/* (c) 1984 */
	DRIVER( spnchotj )	/* (c) 1984 (Japan) */
	DRIVER( armwrest )	/* (c) 1985 */

	/* Midway 8080 b/w games */
	DRIVER( seawolf )	/* 596 [1976] */
	DRIVER( gunfight )	/* 597 [1975] */
	/* 603 - Top Gun [1976] */
	DRIVER( tornbase )	/* 605 [1976] */
	DRIVER( 280zzzap )	/* 610 [1976] */
	DRIVER( maze )		/* 611 [1976] */
	DRIVER( boothill )	/* 612 [1977] */
	DRIVER( checkmat )	/* 615 [1977] */
	DRIVER( desertgu )	/* 618 [1977] */
	DRIVER( dplay )		/* 619 [1977] */
	DRIVER( lagunar )	/* 622 [1977] */
	DRIVER( gmissile )	/* 623 [1977] */
	DRIVER( m4 )		/* 626 [1977] */
	DRIVER( clowns )	/* 630 [1978] */
	/* 640 - Space Walk [1978] */
	DRIVER( einnings )	/* 642 [1978] Midway */
	DRIVER( shuffle )	/* 643 [1978] */
	DRIVER( dogpatch )	/* 644 [1977] */
	DRIVER( spcenctr )	/* 645 (c) 1980 Midway */
	DRIVER( phantom2 )	/* 652 [1979] */
	DRIVER( bowler )	/* 730 [1978] Midway */
	DRIVER( invaders )	/* 739 [1979] */
	DRIVER( blueshrk )	/* 742 [1978] */
	DRIVER( invad2ct )	/* 851 (c) 1980 Midway */
	DRIVER( invadpt2 )	/* 852 [1980] Taito */
	DRIVER( invaddlx )	/* 852 [1980] Midway */
	DRIVER( moonbase )	/* Zeta - Nichibutsu */
	/* 870 - Space Invaders Deluxe cocktail */
	DRIVER( earthinv )
	DRIVER( spaceatt )
	DRIVER( sinvzen )
	DRIVER( superinv )
	DRIVER( sinvemag )
	DRIVER( jspecter )
	DRIVER( invrvnge )
	DRIVER( invrvnga )
	DRIVER( galxwars )
	DRIVER( starw )
	DRIVER( lrescue )	/* (c) 1979 Taito */
	DRIVER( grescue )	/* bootleg? */
	DRIVER( desterth )	/* bootleg */
	DRIVER( cosmicmo )	/* Universal */
	DRIVER( rollingc )	/* Nichibutsu */
	DRIVER( sheriff )	/* (c) Nintendo */
	DRIVER( bandido )	/* (c) Exidy */
	DRIVER( ozmawars )	/* Shin Nihon Kikaku (SNK) */
	DRIVER( solfight )	/* bootleg */
	DRIVER( spaceph )	/* Zilec Games */
	DRIVER( schaser )	/* Taito */
	DRIVER( schasrcv )	/* Taito */
	DRIVER( lupin3 )	/* (c) 1980 Taito */
	DRIVER( helifire )	/* (c) Nintendo */
	DRIVER( helifira )	/* (c) Nintendo */
	DRIVER( spacefev )
	DRIVER( sfeverbw )
	DRIVER( spclaser )
	DRIVER( laser )
	DRIVER( spcewarl )
	DRIVER( polaris )	/* (c) 1980 Taito */
	DRIVER( polarisa )	/* (c) 1980 Taito */
	DRIVER( ballbomb )	/* (c) 1980 Taito */
	DRIVER( m79amb )
	DRIVER( alieninv )
	DRIVER( sitv )
	DRIVER( sicv )
	DRIVER( sisv )
	DRIVER( sisv2 )
	DRIVER( spacewr3 )
	DRIVER( invaderl )
	DRIVER( yosakdon )
	DRIVER( spceking )
	DRIVER( spcewars )

	/* "Midway" Z80 b/w games */
	DRIVER( astinvad )	/* (c) 1980 Stern */
	DRIVER( kamikaze )	/* Leijac Corporation */
	DRIVER( spaceint )	/* [1980] Shoei */

	/* Meadows S2650 games */
	DRIVER( lazercmd )	/* [1976?] */
	DRIVER( deadeye )	/* [1978?] */
	DRIVER( gypsyjug )	/* [1978?] */
	DRIVER( medlanes )	/* [1977?] */

	/* Midway "Astrocade" games */
	DRIVER( seawolf2 )
	DRIVER( spacezap )	/* (c) 1980 */
	DRIVER( ebases )
	DRIVER( wow )		/* (c) 1980 */
	DRIVER( gorf )		/* (c) 1981 */
	DRIVER( gorfpgm1 )	/* (c) 1981 */
	DRIVER( robby )		/* (c) 1981 Bally Midway */
TESTDRIVER( profpac )	/* (c) 1983 Bally Midway */

	/* Bally Midway MCR games */
	/* MCR1 */
	DRIVER( solarfox )	/* (c) 1981 */
	DRIVER( kick )		/* (c) 1981 */
	DRIVER( kicka )		/* bootleg? */
	/* MCR2 */
	DRIVER( shollow )	/* (c) 1981 */
	DRIVER( shollow2 )	/* (c) 1981 */
	DRIVER( tron )		/* (c) 1982 */
	DRIVER( tron2 )		/* (c) 1982 */
	DRIVER( kroozr )	/* (c) 1982 */
	DRIVER( domino )	/* (c) 1982 */
	DRIVER( wacko )		/* (c) 1982 */
	DRIVER( twotiger )	/* (c) 1984 */
	/* MCR2 + MCR3 sprites */
	DRIVER( journey )	/* (c) 1983 */
	/* MCR3 */
	DRIVER( tapper )	/* (c) 1983 */
	DRIVER( tappera )	/* (c) 1983 */
	DRIVER( sutapper )	/* (c) 1983 */
	DRIVER( rbtapper )	/* (c) 1984 */
	DRIVER( timber )	/* (c) 1984 */
	DRIVER( dotron )	/* (c) 1983 */
	DRIVER( dotrone )	/* (c) 1983 */
	DRIVER( destderb )	/* (c) 1984 */
	DRIVER( destderm )	/* (c) 1984 */
	DRIVER( sarge )		/* (c) 1985 */
	DRIVER( rampage )	/* (c) 1986 */
	DRIVER( rampage2 )	/* (c) 1986 */
	DRIVER( powerdrv )	/* (c) 1986 */
	DRIVER( maxrpm )	/* (c) 1986 */
	DRIVER( spyhunt )	/* (c) 1983 */
	DRIVER( turbotag )	/* (c) 1985 */
	DRIVER( crater )	/* (c) 1984 */
	/* MCR 68000 */
	DRIVER( zwackery )	/* (c) 1984 */
	DRIVER( xenophob )	/* (c) 1987 */
	DRIVER( spyhunt2 )	/* (c) 1987 */
	DRIVER( spyhnt2a )	/* (c) 1987 */
	DRIVER( blasted )	/* (c) 1988 */
	DRIVER( archrivl )	/* (c) 1989 */
	DRIVER( archriv2 )	/* (c) 1989 */
	DRIVER( trisport )	/* (c) 1989 */
	DRIVER( pigskin )	/* (c) 1990 */
/* other possible MCR games:
Black Belt
Shoot the Bull
Special Force
MotorDome
Six Flags (?)
*/

	/* Bally / Sente games */
	DRIVER( sentetst )
	DRIVER( cshift )	/* (c) 1984 */
	DRIVER( gghost )	/* (c) 1984 */
	DRIVER( hattrick )	/* (c) 1984 */
	DRIVER( otwalls )	/* (c) 1984 */
	DRIVER( snakepit )	/* (c) 1984 */
	DRIVER( snakjack )	/* (c) 1984 */
	DRIVER( stocker )	/* (c) 1984 */
	DRIVER( triviag1 )	/* (c) 1984 */
	DRIVER( triviag2 )	/* (c) 1984 */
	DRIVER( triviasp )	/* (c) 1984 */
	DRIVER( triviayp )	/* (c) 1984 */
	DRIVER( triviabb )	/* (c) 1984 */
	DRIVER( gimeabrk )	/* (c) 1985 */
	DRIVER( minigolf )	/* (c) 1985 */
	DRIVER( minigol2 )	/* (c) 1985 */
	DRIVER( toggle )	/* (c) 1985 */
	DRIVER( nametune )	/* (c) 1986 */
	DRIVER( nstocker )	/* (c) 1986 */
	DRIVER( sfootbal )	/* (c) 1986 */
	DRIVER( spiker )	/* (c) 1986 */
	DRIVER( rescraid )	/* (c) 1987 */
	DRIVER( rescrdsa )	/* (c) 1987 */

	/* Irem games */
	/* trivia: IREM means "International Rental Electronics Machines" */
	DRIVER( skychut )	/* (c) [1980] */
	DRIVER( redalert )	/* (c) 1981 + "GDI presents" */
	DRIVER( olibochu )	/* M47 (c) 1981 + "GDI presents" */
	DRIVER( mpatrol )	/* M52 (c) 1982 */
	DRIVER( mpatrolw )	/* M52 (c) 1982 + Williams license */
	DRIVER( mranger )	/* bootleg */
	DRIVER( troangel )	/* (c) 1983 */
	DRIVER( yard )		/* (c) 1983 */
	DRIVER( vsyard )	/* (c) 1983/1984 */
	DRIVER( vsyard2 )	/* (c) 1983/1984 */
	DRIVER( travrusa )	/* (c) 1983 */
	DRIVER( motorace )	/* (c) 1983 Williams license */
	/* M62 */
	DRIVER( kungfum )	/* (c) 1984 */
	DRIVER( kungfud )	/* (c) 1984 + Data East license */
	DRIVER( spartanx )	/* (c) 1984 */
	DRIVER( kungfub )	/* bootleg */
	DRIVER( kungfub2 )	/* bootleg */
	DRIVER( battroad )	/* (c) 1984 */
	DRIVER( ldrun )		/* (c) 1984 licensed from Broderbund */
	DRIVER( ldruna )	/* (c) 1984 licensed from Broderbund */
	DRIVER( ldrun2 )	/* (c) 1984 licensed from Broderbund */
	DRIVER( ldrun3 )	/* (c) 1985 licensed from Broderbund */
	DRIVER( ldrun4 )	/* (c) 1986 licensed from Broderbund */
	DRIVER( lotlot )	/* (c) 1985 licensed from Tokuma Shoten */
	DRIVER( kidniki )	/* (c) 1986 + Data East USA license */
	DRIVER( yanchamr )	/* (c) 1986 (Japan) */
	DRIVER( spelunkr )	/* (c) 1985 licensed from Broderbund */
	DRIVER( spelunk2 )	/* (c) 1986 licensed from Broderbund */

	DRIVER( vigilant )	/* (c) 1988 (World) */
	DRIVER( vigilntu )	/* (c) 1988 (US) */
	DRIVER( vigilntj )	/* (c) 1988 (Japan) */
	DRIVER( kikcubic )	/* (c) 1988 (Japan) */
	/* M72 (and derivatives) */
	DRIVER( rtype )		/* (c) 1987 (Japan) */
	DRIVER( rtypepj )	/* (c) 1987 (Japan) */
	DRIVER( rtypeu )	/* (c) 1987 + Nintendo USA license (US) */
	DRIVER( bchopper )	/* (c) 1987 */
	DRIVER( mrheli )	/* (c) 1987 (Japan) */
	DRIVER( nspirit )	/* (c) 1988 */
	DRIVER( nspiritj )	/* (c) 1988 (Japan) */
	DRIVER( imgfight )	/* (c) 1988 (Japan) */
	DRIVER( loht )		/* (c) 1989 */
	DRIVER( xmultipl )	/* (c) 1989 (Japan) */
	DRIVER( dbreed )	/* (c) 1989 */
	DRIVER( rtype2 )	/* (c) 1989 */
	DRIVER( rtype2j )	/* (c) 1989 (Japan) */
	DRIVER( majtitle )	/* (c) 1990 (Japan) */
	DRIVER( hharry )	/* (c) 1990 (World) */
	DRIVER( hharryu )	/* (c) 1990 Irem America (US) */
	DRIVER( dkgensan )	/* (c) 1990 (Japan) */
TESTDRIVER( kengo )
	DRIVER( poundfor )	/* (c) 1990 (World) */
	DRIVER( poundfou )	/* (c) 1990 Irem America (US) */
	DRIVER( airduel )	/* (c) 1990 (Japan) */
	DRIVER( gallop )	/* (c) 1991 (Japan) */
	/* not M72, but same sound hardware */
	DRIVER( sichuan2 )	/* (c) 1989 Tamtex */
	DRIVER( sichuana )	/* (c) 1989 Tamtex */
	DRIVER( shisen )	/* (c) 1989 Tamtex */
	/* M92 */
	DRIVER( bmaster )	/* (c) 1991 Irem */
	DRIVER( gunforce )	/* (c) 1991 Irem (World) */
	DRIVER( gunforcu )	/* (c) 1991 Irem America (US) */
	DRIVER( hook )		/* (c) 1992 Irem (World) */
	DRIVER( hooku )		/* (c) 1992 Irem America (US) */
	DRIVER( mysticri )	/* (c) 1992 Irem (World) */
	DRIVER( gunhohki )	/* (c) 1992 Irem (Japan) */
	DRIVER( uccops )	/* (c) 1992 Irem (World) */
	DRIVER( uccopsj )	/* (c) 1992 Irem (Japan) */
	DRIVER( rtypeleo )	/* (c) 1992 Irem (Japan) */
	DRIVER( majtitl2 )	/* (c) 1992 Irem (World) */
	DRIVER( skingame )	/* (c) 1992 Irem America (US) */
	DRIVER( skingam2 )	/* (c) 1992 Irem America (US) */
	DRIVER( inthunt )	/* (c) 1993 Irem (World) */
	DRIVER( inthuntu )	/* (c) 1993 Irem (US) */
	DRIVER( kaiteids )	/* (c) 1993 Irem (Japan) */
TESTDRIVER( nbbatman )	/* (c) 1993 Irem America (US) */
TESTDRIVER( leaguemn )	/* (c) 1993 Irem (Japan) */
	DRIVER( lethalth )	/* (c) 1991 Irem (World) */
	DRIVER( thndblst )	/* (c) 1991 Irem (Japan) */
	DRIVER( psoldier )	/* (c) 1993 Irem (Japan) */
	/* M97 */
TESTDRIVER( riskchal )
TESTDRIVER( gussun )
TESTDRIVER( shisen2 )
TESTDRIVER( quizf1 )
TESTDRIVER( atompunk )
TESTDRIVER( bbmanw )
	/* M107 */
TESTDRIVER( firebarr )	/* (c) 1993 Irem (Japan) */
	DRIVER( dsoccr94 )	/* (c) 1994 Irem (Data East Corporation license) */

	/* Gottlieb/Mylstar games (Gottlieb became Mylstar in 1983) */
	DRIVER( reactor )	/* GV-100 (c) 1982 Gottlieb */
	DRIVER( mplanets )	/* GV-102 (c) 1983 Gottlieb */
	DRIVER( qbert )		/* GV-103 (c) 1982 Gottlieb */
	DRIVER( qbertjp )	/* GV-103 (c) 1982 Gottlieb + Konami license */
	DRIVER( insector )	/* GV-??? (c) 1982 Gottlieb - never released */
	DRIVER( krull )		/* GV-105 (c) 1983 Gottlieb */
	DRIVER( sqbert )	/* GV-??? (c) 1983 Mylstar - never released */
	DRIVER( mach3 )		/* GV-109 (c) 1983 Mylstar */
	DRIVER( usvsthem )	/* GV-??? (c) 198? Mylstar */
	DRIVER( 3stooges )	/* GV-113 (c) 1984 Mylstar */
	DRIVER( qbertqub )	/* GV-119 (c) 1983 Mylstar */
	DRIVER( screwloo )	/* GV-123 (c) 1983 Mylstar - never released */
	DRIVER( curvebal )	/* GV-134 (c) 1984 Mylstar */

	/* older Taito games */
	DRIVER( crbaloon )	/* (c) 1980 Taito Corporation */
	DRIVER( crbalon2 )	/* (c) 1980 Taito Corporation */

	/* Taito "Qix hardware" games */
	DRIVER( qix )		/* (c) 1981 Taito America Corporation */
	DRIVER( qixa )		/* (c) 1981 Taito America Corporation */
	DRIVER( qixb )		/* (c) 1981 Taito America Corporation */
	DRIVER( qix2 )		/* (c) 1981 Taito America Corporation */
	DRIVER( sdungeon )	/* (c) 1981 Taito America Corporation */
	DRIVER( elecyoyo )	/* (c) 1982 Taito America Corporation */
	DRIVER( elecyoy2 )	/* (c) 1982 Taito America Corporation */
	DRIVER( kram )		/* (c) 1982 Taito America Corporation */
	DRIVER( kram2 )		/* (c) 1982 Taito America Corporation */
	DRIVER( zookeep )	/* (c) 1982 Taito America Corporation */
	DRIVER( zookeep2 )	/* (c) 1982 Taito America Corporation */
	DRIVER( zookeep3 )	/* (c) 1982 Taito America Corporation */

	/* Taito SJ System games */
	DRIVER( spaceskr )	/* (c) 1981 Taito Corporation */
	DRIVER( junglek )	/* (c) 1982 Taito Corporation */
	DRIVER( junglkj2 )	/* (c) 1982 Taito Corporation */
	DRIVER( jungleh )	/* (c) 1982 Taito America Corporation */
	DRIVER( alpine )	/* (c) 1982 Taito Corporation */
	DRIVER( alpinea )	/* (c) 1982 Taito Corporation */
	DRIVER( timetunl )	/* (c) 1982 Taito Corporation */
	DRIVER( wwestern )	/* (c) 1982 Taito Corporation */
	DRIVER( wwester1 )	/* (c) 1982 Taito Corporation */
	DRIVER( frontlin )	/* (c) 1982 Taito Corporation */
	DRIVER( elevator )	/* (c) 1983 Taito Corporation */
	DRIVER( elevatob )	/* bootleg */
	DRIVER( tinstar )	/* (c) 1983 Taito Corporation */
	DRIVER( waterski )	/* (c) 1983 Taito Corporation */
	DRIVER( bioatack )	/* (c) 1983 Taito Corporation + Fox Video Games license */
	DRIVER( hwrace )	/* (c) 1983 Taito Corporation */
	DRIVER( sfposeid )	/* 1984 */
	DRIVER( kikstart )

	/* other Taito games */
	DRIVER( bking2 )	/* (c) 1983 Taito Corporation */
	DRIVER( gsword )	/* (c) 1984 Taito Corporation */
	DRIVER( lkage )		/* A54 (c) 1984 Taito Corporation */
	DRIVER( lkageb )	/* bootleg */
	DRIVER( lkageb2 )	/* bootleg */
	DRIVER( lkageb3 )	/* bootleg */
	DRIVER( retofinv )	/* (c) 1985 Taito Corporation */
	DRIVER( retofin1 )	/* bootleg */
	DRIVER( retofin2 )	/* bootleg */
	DRIVER( tsamurai )	/* (c) 1985 Taito */
	DRIVER( tsamura2 )	/* (c) 1985 Taito */
	DRIVER( nunchaku )	/* (c) 1985 Taito */
	DRIVER( yamagchi )	/* (c) 1985 Taito */
	DRIVER( flstory )	/* A45 (c) 1985 Taito Corporation */
	DRIVER( flstoryj )	/* A45 (c) 1985 Taito Corporation (Japan) */
TESTDRIVER( onna34ro )	/* A52 */
	DRIVER( gladiatr )	/* (c) 1986 Taito America Corporation (US) */
	DRIVER( ogonsiro )	/* (c) 1986 Taito Corporation (Japan) */
	DRIVER( lsasquad )	/* A64 (c) 1986 Taito Corporation / Taito America (dip switch) */
	DRIVER( storming )	/* A64 (c) 1986 Taito Corporation */
	DRIVER( tokio )		/* A71 1986 */
	DRIVER( tokiob )	/* bootleg */
	DRIVER( bublbobl )	/* A78 (c) 1986 Taito Corporation */
	DRIVER( bublbobr )	/* A78 (c) 1986 Taito America Corporation + Romstar license */
	DRIVER( bubbobr1 )	/* A78 (c) 1986 Taito America Corporation + Romstar license */
	DRIVER( boblbobl )	/* bootleg */
	DRIVER( sboblbob )	/* bootleg */
	DRIVER( kicknrun )	/* (c) 1986 Taito Corporation */
	DRIVER( mexico86 )	/* bootleg (Micro Research) */
	DRIVER( kikikai )	/* (c) 1986 Taito Corporation */
	DRIVER( rastan )	/* (c) 1987 Taito Corporation Japan (World) */
	DRIVER( rastanu )	/* (c) 1987 Taito America Corporation (US) */
	DRIVER( rastanu2 )	/* (c) 1987 Taito America Corporation (US) */
	DRIVER( rastsaga )	/* (c) 1987 Taito Corporation (Japan)*/
	DRIVER( rainbow )	/* (c) 1987 Taito Corporation */
	DRIVER( rainbowe )	/* (c) 1988 Taito Corporation */
	DRIVER( jumping )	/* bootleg */
	DRIVER( arkanoid )	/* A75 (c) 1986 Taito Corporation Japan (World) */
	DRIVER( arknoidu )	/* A75 (c) 1986 Taito America Corporation + Romstar license (US) */
	DRIVER( arknoidj )	/* A75 (c) 1986 Taito Corporation (Japan) */
	DRIVER( arkbl2 )	/* bootleg */
TESTDRIVER( arkbl3 )	/* bootleg */
	DRIVER( arkatayt )	/* bootleg */
TESTDRIVER( arkblock )	/* bootleg */
	DRIVER( arkbloc2 )	/* bootleg */
	DRIVER( arkangc )	/* bootleg */
	DRIVER( arkatour )	/* (c) 1987 Taito America Corporation + Romstar license (US) */
	DRIVER( superqix )	/* 1987 */
	DRIVER( sqixbl )	/* bootleg? but (c) 1987 */
	DRIVER( superman )	/* (c) 1988 Taito Corporation */
	DRIVER( minivadr )	/* cabinet test board */

	/* Taito "tnzs" hardware */
	DRIVER( extrmatn )	/* (c) 1987 World Games */
	DRIVER( arkanoi2 )	/* (c) 1987 Taito Corporation Japan (World) */
	DRIVER( ark2us )	/* (c) 1987 Taito America Corporation + Romstar license (US) */
	DRIVER( ark2jp )	/* (c) 1987 Taito Corporation (Japan) */
	DRIVER( plumppop )	/* (c) 1987 Taito Corporation (Japan) */
	DRIVER( drtoppel )	/* (c) 1987 Taito Corporation (Japan) */
	DRIVER( chukatai )	/* (c) 1988 Taito Corporation (Japan) */
	DRIVER( tnzs )		/* (c) 1988 Taito Corporation (Japan) (new logo) */
	DRIVER( tnzsb )		/* bootleg but Taito Corporation Japan (World) (new logo) */
	DRIVER( tnzs2 )		/* (c) 1988 Taito Corporation Japan (World) (old logo) */
	DRIVER( insectx )	/* (c) 1989 Taito Corporation Japan (World) */
	DRIVER( kageki )	/* (c) 1988 Taito America Corporation + Romstar license (US) */
	DRIVER( kagekij )	/* (c) 1988 Taito Corporation (Japan) */

	/* Taito L-System games */
	DRIVER( raimais )	/* (c) 1988 Taito Corporation (Japan) */
	DRIVER( fhawk )		/* (c) 1988 Taito Corporation (Japan) */
	DRIVER( champwr )	/* (c) 1989 Taito Corporation Japan (World) */
	DRIVER( champwru )	/* (c) 1989 Taito America Corporation (US) */
	DRIVER( champwrj )	/* (c) 1989 Taito Corporation (Japan) */
	DRIVER( kurikint )      /* (c) 1989 Taito Corporation (Japan) */
	DRIVER( kurikina )	/* (c) 1989 Taito Corporation (Japan) */
	DRIVER( plotting )	/* (c) 1989 Taito Corporation Japan (World) */
	DRIVER( puzznic )	/* (c) 1989 Taito Corporation (Japan) */
	DRIVER( horshoes )	/* (c) 1990 Taito America Corporation (US) */
	DRIVER( palamed )	/* (c) 1990 Taito Corporation (Japan) */
	DRIVER( cachat )	/* (c) 1993 Taito Corporation (Japan) */
	DRIVER( tubeit )	/* no copyright message */
	DRIVER( cubybop )	/* no copyright message */
	DRIVER( plgirls )	/* (c) 1992 Hot-B. */
	DRIVER( plgirls2 )	/* (c) 1993 Hot-B. */

	/* Taito B-System games */
	DRIVER( masterw )	/* B72 (c) 1989 Taito Corporation Japan (World) */
	DRIVER( nastar )	/* B81 (c) 1988 Taito Corporation Japan (World) */
	DRIVER( nastarw )	/* B81 (c) 1988 Taito America Corporation (US) */
	DRIVER( rastsag2 )	/* B81 (c) 1988 Taito Corporation (Japan) */
	DRIVER( crimec )	/* B99 (c) 1989 Taito Corporation Japan (World) */
	DRIVER( crimecu )	/* B99 (c) 1989 Taito America Corporation (US) */
	DRIVER( crimecj )	/* B99 (c) 1989 Taito Corporation (Japan) */
	DRIVER( tetrist )	/* C12 (c) 1989 Sega Enterprises,Ltd. (Japan) */
	DRIVER( viofight )	/* C16 (c) 1989 Taito Corporation Japan (World) */
	DRIVER( ashura )	/* C43 (c) 1990 Taito Corporation (Japan) */
	DRIVER( ashurau )	/* C43 (c) 1990 Taito America Corporation (US) */
	DRIVER( hitice )	/* C59 (c) 1990 Williams (US) */
	DRIVER( rambo3 )	/* ??? (c) 1989 Taito Europe Corporation (Europe) */
	DRIVER( rambo3a )	/* ??? (c) 1989 Taito America Corporation (US) */
	DRIVER( puzbobb )	/* ??? (c) 1994 Taito Corporation (Japan) */
	DRIVER( qzshowby )	/* D72 (c) 1994 Taito Corporation (Japan) */
	DRIVER( spacedx )	/* D89 (c) 1994 Taito Corporation (Japan) */
	DRIVER( silentd )	/* ??? (c) 1992 Taito Corporation Japan (World) */

	/* Taito F2 games */
	DRIVER( finalb )	/* B82 (c) 1988 Taito Corporation Japan (World) */
	DRIVER( finalbj )	/* B82 (c) 1988 Taito Corporation (Japan) */
	DRIVER( dondokod )	/* B95 (c) 1989 Taito Corporation (Japan) */
TESTDRIVER( megab )		/* C11 (c) 1989 Taito Corporation Japan (World) */
TESTDRIVER( megabj )	/* C11 (c) 1989 Taito Corporation (Japan) */
	DRIVER( thundfox )	/* C28 (c) 1990 Taito Corporation (Japan) */
	DRIVER( cameltry )	/* C38 (c) 1989 Taito Corporation (Japan) */
	DRIVER( cameltru )	/* C38 (c) 1989 Taito America Corporation (US) */
	DRIVER( qtorimon )	/* C41 (c) 1990 Taito Corporation (Japan) */
	DRIVER( liquidk )	/* C49 (c) 1990 Taito Corporation Japan (World) */
	DRIVER( liquidku )	/* C49 (c) 1990 Taito America Corporation (US) */
	DRIVER( mizubaku )	/* C49 (c) 1990 Taito Corporation (Japan) */
	DRIVER( quizhq )	/* C53 (c) 1990 Taito Corporation (Japan) */
	DRIVER( ssi )		/* C64 (c) 1990 Taito Corporation Japan (World) */
	DRIVER( majest12 )	/* C64 (c) 1990 Taito Corporation (Japan) */
	DRIVER( gunfront )	/* C71 (c) 1990 Taito Corporation Japan (World) */
	DRIVER( gunfronj )	/* C71 (c) 1990 Taito Corporation (Japan) */
	DRIVER( growl )		/* C74 (c) 1990 Taito Corporation Japan (World) */
	DRIVER( growlu )	/* C74 (c) 1990 Taito America Corporation (US) */
	DRIVER( runark )	/* C74 (c) 1990 Taito Corporation (Japan) */
	DRIVER( mjnquest )	/* C77 (c) 1990 Taito Corporation (Japan) */
	DRIVER( mjnquesb )	/* C77 (c) 1990 Taito Corporation (Japan) */
	DRIVER( footchmp )	/* C80 (c) 1990 Taito Corporation Japan (World) */
	DRIVER( hthero )	/* C80 (c) 1990 Taito Corporation (Japan) */
	DRIVER( euroch92 )	/*     (c) 1992 Taito Corporation Japan (World) */
	DRIVER( koshien )	/* C81 (c) 1990 Taito Corporation (Japan) */
	DRIVER( yuyugogo )	/* C83 (c) 1990 Taito Corporation (Japan) */
	DRIVER( ninjak )	/* C85 (c) 1990 Taito Corporation Japan (World) */
	DRIVER( ninjakj )	/* C85 (c) 1990 Taito Corporation (Japan) */
	DRIVER( solfigtr )	/* C91 (c) 1991 Taito Corporation Japan (World) */
	DRIVER( qzquest )	/* C92 (c) 1991 Taito Corporation (Japan) */
	DRIVER( pulirula )	/* C98 (c) 1991 Taito Corporation Japan (World) */
	DRIVER( pulirulj )	/* C98 (c) 1991 Taito Corporation (Japan) */
TESTDRIVER( metalb )	/* D16? (c) 1991 Taito Corporation Japan (World) */
TESTDRIVER( metalbj )	/* D12 (c) 1991 Taito Corporation (Japan) */
	DRIVER( qzchikyu )	/* D19 (c) 1991 Taito Corporation (Japan) */
	DRIVER( yesnoj )	/* D20 (c) 1992 Taito Corporation (Japan) */
	DRIVER( deadconx )	/* D28 (c) 1992 Taito Corporation Japan (World) */
	DRIVER( deadconj )	/* D28 (c) 1992 Taito Corporation (Japan) */
	DRIVER( dinorex )	/* D39 (c) 1992 Taito Corporation Japan (World) */
	DRIVER( dinorexj )	/* D39 (c) 1992 Taito Corporation (Japan) */
	DRIVER( dinorexu )	/* D39 (c) 1992 Taito America Corporation (US) */
	DRIVER( qjinsei )	/* D48 (c) 1992 Taito Corporation (Japan) */
	DRIVER( qcrayon )	/* D55 (c) 1993 Taito Corporation (Japan) */
	DRIVER( qcrayon2 )	/* D63 (c) 1993 Taito Corporation (Japan) */
	DRIVER( driftout )	/* (c) 1991 Visco */
	DRIVER( driveout )	/* bootleg */

	/* Toaplan games */
	DRIVER( tigerh )	/* GX-551 [not a Konami board!] */
	DRIVER( tigerh2 )	/* GX-551 [not a Konami board!] */
	DRIVER( tigerhj )	/* GX-551 [not a Konami board!] */
	DRIVER( tigerhb1 )	/* bootleg but (c) 1985 Taito Corporation */
	DRIVER( tigerhb2 )	/* bootleg but (c) 1985 Taito Corporation */
	DRIVER( slapfigh )	/* TP-??? */
	DRIVER( slapbtjp )	/* bootleg but (c) 1986 Taito Corporation */
	DRIVER( slapbtuk )	/* bootleg but (c) 1986 Taito Corporation */
	DRIVER( alcon )		/* TP-??? */
	DRIVER( getstar )
	DRIVER( getstarj )
	DRIVER( getstarb )	/* GX-006 bootleg but (c) 1986 Taito Corporation */

	DRIVER( fshark )	/* TP-007 (c) 1987 Taito Corporation (World) */
	DRIVER( skyshark )	/* TP-007 (c) 1987 Taito America Corporation + Romstar license (US) */
	DRIVER( hishouza )	/* TP-007 (c) 1987 Taito Corporation (Japan) */
	DRIVER( fsharkbt )	/* bootleg */
	DRIVER( wardner )	/* TP-009 (c) 1987 Taito Corporation Japan (World) */
	DRIVER( pyros )		/* TP-009 (c) 1987 Taito America Corporation (US) */
	DRIVER( wardnerj )	/* TP-009 (c) 1987 Taito Corporation (Japan) */
	DRIVER( twincobr )	/* TP-011 (c) 1987 Taito Corporation (World) */
	DRIVER( twincobu )	/* TP-011 (c) 1987 Taito America Corporation + Romstar license (US) */
	DRIVER( ktiger )	/* TP-011 (c) 1987 Taito Corporation (Japan) */

	DRIVER( rallybik )	/* TP-012 (c) 1988 Taito */
	DRIVER( truxton )	/* TP-013B (c) 1988 Taito */
	DRIVER( hellfire )	/* TP-??? (c) 1989 Toaplan + Taito license */
	DRIVER( zerowing )	/* TP-015 (c) 1989 Toaplan */
	DRIVER( demonwld )	/* TP-016 (c) 1989 Toaplan + Taito license */
	DRIVER( fireshrk )	/* TP-017 (c) 1990 Toaplan */
	DRIVER( samesame )	/* TP-017 (c) 1989 Toaplan */
	DRIVER( outzone )	/* TP-018 (c) 1990 Toaplan */
	DRIVER( outzonep )	/* bootleg */
	DRIVER( vimana )	/* TP-019 (c) 1991 Toaplan (+ Tecmo license when set to Japan) */
	DRIVER( vimana2 )	/* TP-019 (c) 1991 Toaplan (+ Tecmo license when set to Japan)  */
	DRIVER( vimanan )	/* TP-019 (c) 1991 Toaplan (+ Nova Apparate GMBH & Co license) */
	DRIVER( snowbros )	/* MIN16-02 (c) 1990 Toaplan + Romstar license */
	DRIVER( snowbroa )	/* MIN16-02 (c) 1990 Toaplan + Romstar license */
	DRIVER( snowbrob )	/* MIN16-02 (c) 1990 Toaplan + Romstar license */
	DRIVER( snowbroj )	/* MIN16-02 (c) 1990 Toaplan */

	DRIVER( tekipaki )	/* TP-020 (c) 1991 Toaplan */
	DRIVER( ghox )		/* TP-021 (c) 1991 Toaplan */
	DRIVER( dogyuun )	/* TP-022 (c) 1992 Toaplan */
	DRIVER( kbash )		/* TP-023 (c) 1993 Toaplan */
	DRIVER( tatsujn2 )	/* TP-024 (c) 1992 Toaplan */
	DRIVER( pipibibs )	/* TP-025 */
	DRIVER( pipibibi )	/* (c) 1991 Ryouta Kikaku (bootleg?) */
	DRIVER( whoopee )	/* TP-025 */
TESTDRIVER( fixeight )	/* TP-026 (c) 1992 + Taito license */
	DRIVER( vfive )		/* TP-027 (c) 1993 Toaplan (Japan) */
	DRIVER( grindstm )	/* TP-027 (c) 1993 Toaplan + Unite Trading license (Korea) */
	DRIVER( batsugun )	/* TP-030 (c) 1993 Toaplan */
	DRIVER( batugnsp )	/* TP-??? (c) 1993 Toaplan */
	DRIVER( snowbro2 )	/* TP-??? (c) 1994 Hanafram */
	DRIVER( mahoudai )	/* (c) 1993 Raizing + Able license */
	DRIVER( shippumd )	/* (c) 1994 Raizing */

/*
Toa Plan's board list
(translated from http://www.aianet.ne.jp/~eisetu/rom/rom_toha.html)

Title              ROMno.   Remark(1)   Remark(2)
--------------------------------------------------
Tiger Heli           A47      GX-551
Hishouzame           B02      TP-007
Kyukyoku Tiger       B30      TP-011
Dash Yarou           B45      TP-012
Tatsujin             B65      TP-013B   M6100649A
Zero Wing            O15      TP-015
Horror Story         O16      TP-016
Same!Same!Same!      O17      TP-017
Out Zone                      TP-018
Vimana                        TP-019
Teki Paki            O20      TP-020
Ghox               TP-21      TP-021
Dogyuun                       TP-022
Tatsujin Oh                   TP-024    *1
Fixeight                      TP-026
V-V                           TP-027

*1 There is a doubt this game uses TP-024 board and TP-025 romsets.

   86 Mahjong Sisters                                 Kit 2P 8W+2B     HC    Mahjong TP-
   88 Dash                                            Kit 2P 8W+2B                   TP-
   89 Fire Shark                                      Kit 2P 8W+2B     VC    Shooter TP-017
   89 Twin Hawk                                       Kit 2P 8W+2B     VC    Shooter TP-
   91 Whoopie                                         Kit 2P 8W+2B     HC    Action
   92 Teki Paki                                       Kit 2P                         TP-020
   92 Ghox                                            Kit 2P Paddle+1B VC    Action  TP-021
10/92 Dogyuun                                         Kit 2P 8W+2B     VC    Shooter TP-022
92/93 Knuckle Bash                 Atari Games        Kit 2P 8W+2B     HC    Action  TP-023
10/92 Tatsujin II/Truxton II       Taito              Kit 2P 8W+2B     VC    Shooter TP-024
10/92 Truxton II/Tatsujin II       Taito              Kit 2P 8W+2B     VC    Shooter TP-024
      Pipi & Bipi                                                                    TP-025
   92 Fix Eight                                       Kit 2P 8W+2B     VC    Action  TP-026
12/92 V  -  V (5)/Grind Stormer                       Kit 2P 8W+2B     VC    Shooter TP-027
 1/93 Grind Stormer/V - V (Five)                      Kit 2P 8W+2B     VC    Shooter TP-027
 2/94 Batsugun                                        Kit 2P 8W+2B     VC            TP-
 4/94 Snow Bros. 2                                    Kit 2P 8W+2B     HC    Action  TP-
*/

	/* Cave games */
	/* Cave was formed in 1994 from the ruins of Toaplan, like Raizing was. */
/* Donpachi (c) 1995 */
	DRIVER( ddonpach )	/* (c) 1997 Atlus/Cave */
	DRIVER( dfeveron )	/* (c) 1998 Cave + Nihon System license */
	DRIVER( esprade )	/* (c) 1998 Atlus/Cave */
	DRIVER( uopoko )	/* (c) 1998 Cave + Jaleco license */
TESTDRIVER( guwange )	/* (c) 1999 Atlus/Cave */

	/* Kyugo games */
	/* Kyugo only made four games: Repulse, Flash Gal, SRD Mission and Air Wolf. */
	/* Gyrodine was made by Crux. Crux was antecedent of Toa Plan, and spin-off from Orca. */
	DRIVER( gyrodine )	/* (c) 1984 Taito Corporation */
	DRIVER( sonofphx )	/* (c) 1985 Associated Overseas MFR */
	DRIVER( repulse )	/* (c) 1985 Sega */
	DRIVER( 99lstwar )	/* (c) 1985 Proma */
	DRIVER( 99lstwra )	/* (c) 1985 Proma */
	DRIVER( flashgal )	/* (c) 1985 Sega */
	DRIVER( srdmissn )	/* (c) 1986 Taito Corporation */
	DRIVER( airwolf )	/* (c) 1987 Kyugo */
	DRIVER( skywolf )	/* bootleg */
	DRIVER( skywolf2 )	/* bootleg */

	/* Williams games */
	DRIVER( defender )	/* (c) 1980 */
	DRIVER( defendg )	/* (c) 1980 */
	DRIVER( defendw )	/* (c) 1980 */
TESTDRIVER( defndjeu )	/* bootleg */
	DRIVER( defcmnd )	/* bootleg */
	DRIVER( defence )	/* bootleg */
	DRIVER( mayday )
	DRIVER( maydaya )
	DRIVER( colony7 )	/* (c) 1981 Taito */
	DRIVER( colony7a )	/* (c) 1981 Taito */
	DRIVER( stargate )	/* (c) 1981 */
	DRIVER( robotron )	/* (c) 1982 */
	DRIVER( robotryo )	/* (c) 1982 */
	DRIVER( joust )		/* (c) 1982 */
	DRIVER( joustr )	/* (c) 1982 */
	DRIVER( joustwr )	/* (c) 1982 */
	DRIVER( bubbles )	/* (c) 1982 */
	DRIVER( bubblesr )	/* (c) 1982 */
	DRIVER( splat )		/* (c) 1982 */
	DRIVER( sinistar )	/* (c) 1982 */
	DRIVER( sinista1 )	/* (c) 1982 */
	DRIVER( sinista2 )	/* (c) 1982 */
	DRIVER( blaster )	/* (c) 1983 */
	DRIVER( mysticm )	/* (c) 1983 */
	DRIVER( tshoot )	/* (c) 1984 */
	DRIVER( inferno )	/* (c) 1984 */
	DRIVER( joust2 )	/* (c) 1986 */

	/* Capcom games */
	/* The following is a COMPLETE list of the Capcom games up to 1997, as shown on */
	/* their web site. The list is sorted by production date. */
	DRIVER( vulgus )	/*  5/1984 (c) 1984 */
	DRIVER( vulgus2 )	/*  5/1984 (c) 1984 */
	DRIVER( vulgusj )	/*  5/1984 (c) 1984 */
	DRIVER( sonson )	/*  7/1984 (c) 1984 */
	DRIVER( higemaru )	/*  9/1984 (c) 1984 */
	DRIVER( 1942 )		/* 12/1984 (c) 1984 */
	DRIVER( 1942a )		/* 12/1984 (c) 1984 */
	DRIVER( 1942b )		/* 12/1984 (c) 1984 */
	DRIVER( exedexes )	/*  2/1985 (c) 1985 */
	DRIVER( savgbees )	/*  2/1985 (c) 1985 + Memetron license */
	DRIVER( commando )	/*  5/1985 (c) 1985 (World) */
	DRIVER( commandu )	/*  5/1985 (c) 1985 + Data East license (US) */
	DRIVER( commandj )	/*  5/1985 (c) 1985 (Japan) */
	DRIVER( spaceinv )	/* bootleg */
	DRIVER( gng )		/*  9/1985 (c) 1985 */
	DRIVER( gnga )		/*  9/1985 (c) 1985 */
	DRIVER( gngt )		/*  9/1985 (c) 1985 */
	DRIVER( makaimur )	/*  9/1985 (c) 1985 */
	DRIVER( makaimuc )	/*  9/1985 (c) 1985 */
	DRIVER( makaimug )	/*  9/1985 (c) 1985 */
	DRIVER( diamond )	/* (c) 1989 KH Video (NOT A CAPCOM GAME but runs on GnG hardware) */
	DRIVER( gunsmoke )	/* 11/1985 (c) 1985 (World) */
	DRIVER( gunsmrom )	/* 11/1985 (c) 1985 + Romstar (US) */
	DRIVER( gunsmoka )	/* 11/1985 (c) 1985 (US) */
	DRIVER( gunsmokj )	/* 11/1985 (c) 1985 (Japan) */
	DRIVER( sectionz )	/* 12/1985 (c) 1985 */
	DRIVER( sctionza )	/* 12/1985 (c) 1985 */
	DRIVER( trojan )	/*  4/1986 (c) 1986 (US) */
	DRIVER( trojanr )	/*  4/1986 (c) 1986 + Romstar */
	DRIVER( trojanj )	/*  4/1986 (c) 1986 (Japan) */
	DRIVER( srumbler )	/*  9/1986 (c) 1986 */
	DRIVER( srumblr2 )	/*  9/1986 (c) 1986 */
	DRIVER( rushcrsh )	/*  9/1986 (c) 1986 */
	DRIVER( lwings )	/* 11/1986 (c) 1986 */
	DRIVER( lwings2 )	/* 11/1986 (c) 1986 */
	DRIVER( lwingsjp )	/* 11/1986 (c) 1986 */
	DRIVER( sidearms )	/* 12/1986 (c) 1986 (World) */
	DRIVER( sidearmr )	/* 12/1986 (c) 1986 + Romstar license (US) */
	DRIVER( sidearjp )	/* 12/1986 (c) 1986 (Japan) */
	DRIVER( turtship )	/* (c) 1988 Philco (NOT A CAPCOM GAME but runs on modified Sidearms hardware) */
	DRIVER( dyger )		/* (c) 1989 Philco (NOT A CAPCOM GAME but runs on modified Sidearms hardware) */
	DRIVER( dygera )	/* (c) 1989 Philco (NOT A CAPCOM GAME but runs on modified Sidearms hardware) */
	DRIVER( avengers )	/*  2/1987 (c) 1987 (US) */
	DRIVER( avenger2 )	/*  2/1987 (c) 1987 (US) */
	DRIVER( bionicc )	/*  3/1987 (c) 1987 (US) */
	DRIVER( bionicc2 )	/*  3/1987 (c) 1987 (US) */
	DRIVER( topsecrt )	/*  3/1987 (c) 1987 (Japan) */
	DRIVER( 1943 )		/*  6/1987 (c) 1987 (US) */
	DRIVER( 1943j )		/*  6/1987 (c) 1987 (Japan) */
	DRIVER( blktiger )	/*  8/1987 (c) 1987 (US) */
	DRIVER( bktigerb )	/* bootleg */
	DRIVER( blkdrgon )	/*  8/1987 (c) 1987 (Japan) */
	DRIVER( blkdrgnb )	/* bootleg, hacked to say Black Tiger */
	DRIVER( sf1 )		/*  8/1987 (c) 1987 (World) */
	DRIVER( sf1us )		/*  8/1987 (c) 1987 (US) */
	DRIVER( sf1jp )		/*  8/1987 (c) 1987 (Japan) */
	DRIVER( tigeroad )	/* 11/1987 (c) 1987 + Romstar (US) */
	DRIVER( toramich )	/* 11/1987 (c) 1987 (Japan) */
	DRIVER( f1dream )	/*  4/1988 (c) 1988 + Romstar */
	DRIVER( f1dreamb )	/* bootleg */
	DRIVER( 1943kai )	/*  6/1988 (c) 1987 (Japan) */
	DRIVER( lastduel )	/*  7/1988 (c) 1988 (US) */
	DRIVER( lstduela )	/*  7/1988 (c) 1988 (US) */
	DRIVER( lstduelb )	/* bootleg */
	DRIVER( madgear )	/*  2/1989 (c) 1989 (US) */
	DRIVER( madgearj )	/*  2/1989 (c) 1989 (Japan) */
	DRIVER( ledstorm )	/*  2/1989 (c) 1989 (US) */
	/*  3/1989 Dokaben (baseball) - see below among "Mitchell" games */
	/*  8/1989 Dokaben 2 (baseball) - see below among "Mitchell" games */
	/* 10/1989 Capcom Baseball - see below among "Mitchell" games */
	/* 11/1989 Capcom World - see below among "Mitchell" games */
	/*  3/1990 Adventure Quiz 2 Hatena no Dai-Bouken - see below among "Mitchell" games */
	/*  1/1991 Quiz Tonosama no Yabou - see below among "Mitchell" games */
	/*  4/1991 Ashita Tenki ni Naare (golf) - see below among "Mitchell" games */
	/*  5/1991 Ataxx - see below among "Leland" games */
	/*  6/1991 Quiz Sangokushi - see below among "Mitchell" games */
	/* 10/1991 Block Block - see below among "Mitchell" games */
	/*  6/1995 Street Fighter - the Movie - see below among "Incredible Technologies" games */

	/* Capcom CPS1 games */
	DRIVER( forgottn )	/*  7/1988 (c) 1988 (US) */
	DRIVER( lostwrld )	/*  7/1988 (c) 1988 (Japan) */
	DRIVER( ghouls )	/* 12/1988 (c) 1988 (World) */
	DRIVER( ghoulsu )	/* 12/1988 (c) 1988 (US) */
	DRIVER( ghoulsj )	/* 12/1988 (c) 1988 (Japan) */
	DRIVER( strider )	/*  3/1989 (c) 1989 */
	DRIVER( striderj )	/*  3/1989 (c) 1989 */
	DRIVER( stridrja )	/*  3/1989 (c) 1989 */
	DRIVER( dwj )		/*  4/1989 (c) 1989 (Japan) */
	DRIVER( willow )	/*  6/1989 (c) 1989 (Japan) */
	DRIVER( willowj )	/*  6/1989 (c) 1989 (Japan) */
	DRIVER( unsquad )	/*  8/1989 (c) 1989 */
	DRIVER( area88 )	/*  8/1989 (c) 1989 */
	DRIVER( ffight )	/* 12/1989 (c) (World) */
	DRIVER( ffightu )	/* 12/1989 (c) (US)    */
	DRIVER( ffightj )	/* 12/1989 (c) (Japan) */
	DRIVER( 1941 )		/*  2/1990 (c) 1990 (World) */
	DRIVER( 1941j )		/*  2/1990 (c) 1990 (Japan) */
	DRIVER( mercs )		/*  3/ 2/1990 (c) 1990 (World) */
	DRIVER( mercsu )	/*  3/ 2/1990 (c) 1990 (US)    */
	DRIVER( mercsj )	/*  3/ 2/1990 (c) 1990 (Japan) */
	DRIVER( mtwins )	/*  6/19/1990 (c) 1990 (World) */
	DRIVER( chikij )	/*  6/19/1990 (c) 1990 (Japan) */
	DRIVER( msword )	/*  7/25/1990 (c) 1990 (World) */
	DRIVER( mswordu )	/*  7/25/1990 (c) 1990 (US)    */
	DRIVER( mswordj )	/*  6/23/1990 (c) 1990 (Japan) */
	DRIVER( cawing )	/* 10/12/1990 (c) 1990 (World) */
	DRIVER( cawingj )	/* 10/12/1990 (c) 1990 (Japan) */
	DRIVER( nemo )		/* 11/30/1990 (c) 1990 (World) */
	DRIVER( nemoj )		/* 11/20/1990 (c) 1990 (Japan) */
	DRIVER( sf2 )		/*  2/14/1991 (c) 1991 (World) */
	DRIVER( sf2a )		/*  2/ 6/1991 (c) 1991 (US)    */
	DRIVER( sf2b )		/*  2/14/1991 (c) 1991 (US)    */
	DRIVER( sf2e )		/*  2/28/1991 (c) 1991 (US)    */
	DRIVER( sf2j )		/* 12/10/1991 (c) 1991 (Japan) */
	DRIVER( sf2jb )		/*  2/14/1991 (c) 1991 (Japan) */
	DRIVER( 3wonders )	/*  5/20/1991 (c) 1991 (US) */
	DRIVER( wonder3 )	/*  5/20/1991 (c) 1991 (Japan) */
	DRIVER( kod )		/*  7/11/1991 (c) 1991 (World) */
	DRIVER( kodj )		/*  8/ 5/1991 (c) 1991 (Japan) */
	DRIVER( kodb )		/* bootleg */
	DRIVER( captcomm )	/* 10/14/1991 (c) 1991 (World) */
	DRIVER( captcomu )	/*  9/28/1991 (c) 1991 (US)    */
	DRIVER( captcomj )	/* 12/ 2/1991 (c) 1991 (Japan) */
	DRIVER( knights )	/* 11/27/1991 (c) 1991 (World) */
	DRIVER( knightsj )	/* 11/27/1991 (c) 1991 (Japan) */
	DRIVER( sf2ce )		/*  3/13/1992 (c) 1992 (World) */
	DRIVER( sf2cea )	/*  3/13/1992 (c) 1992 (US)    */
	DRIVER( sf2ceb )	/*  5/13/1992 (c) 1992 (US)    */
	DRIVER( sf2cej )	/*  5/13/1992 (c) 1992 (Japan) */
	DRIVER( sf2rb )		/* hack */
	DRIVER( sf2red )	/* hack */
	DRIVER( sf2accp2 )	/* hack */
	DRIVER( varth )		/*  6/12/1992 (c) 1992 (World) */
	DRIVER( varthu )	/*  6/12/1992 (c) 1992 (US) */
	DRIVER( varthj )	/*  7/14/1992 (c) 1992 (Japan) */
	DRIVER( cworld2j )	/*  6/11/1992 (QUIZ 5) (c) 1992 (Japan) */
	DRIVER( wof )		/* 10/ 2/1992 (c) 1992 (World) (CPS1 + QSound) */
	DRIVER( wofa )		/* 10/ 5/1992 (c) 1992 (Asia)  (CPS1 + QSound) */
	DRIVER( wofj )		/* 10/31/1992 (c) 1992 (Japan) (CPS1 + QSound) */
	DRIVER( sf2t )		/* 12/ 9/1992 (c) 1992 (US)    */
	DRIVER( sf2tj )		/* 12/ 9/1992 (c) 1992 (Japan) */
	DRIVER( dino )		/*  2/ 1/1993 (c) 1993 (World) (CPS1 + QSound) */
	DRIVER( dinoj )		/*  2/ 1/1993 (c) 1993 (Japan) (CPS1 + QSound) */
	DRIVER( punisher )	/*  4/22/1993 (c) 1993 (World) (CPS1 + QSound) */
	DRIVER( punishru )	/*  4/22/1993 (c) 1993 (US)    (CPS1 + QSound) */
	DRIVER( punishrj )	/*  4/22/1993 (c) 1993 (Japan) (CPS1 + QSound) */
	DRIVER( slammast )	/*  7/13/1993 (c) 1993 (World) (CPS1 + QSound) */
	DRIVER( mbomberj )	/*  7/13/1993 (c) 1993 (Japan) (CPS1 + QSound) */
	DRIVER( mbombrd )	/* 12/ 6/1993 (c) 1993 (World) (CPS1 + QSound) */
	DRIVER( mbombrdj )	/* 12/ 6/1993 (c) 1993 (Japan) (CPS1 + QSound) */
	DRIVER( pnickj )	/*  6/ 8/1994 (c) 1994 + Compile license (Japan) not listed on Capcom's site */
	DRIVER( qad )		/*  7/ 1/1992 (c) 1992 (US)    */
	DRIVER( qadj )		/*  9/21/1994 (c) 1994 (Japan) */
	DRIVER( qtono2 )	/*  1/23/1995 (c) 1995 (Japan) */
	DRIVER( pang3 )		/*  5/11/1995 (c) 1995 Mitchell (Euro) not listed on Capcom's site */
	DRIVER( pang3j )	/*  5/11/1995 (c) 1995 Mitchell (Japan) not listed on Capcom's site */
	DRIVER( megaman )	/* 10/ 6/1995 (c) 1995 (Asia)  */
	DRIVER( rockmanj )	/*  9/22/1995 (c) 1995 (Japan) */
//	DRIVER( sfzch )		/* 10/20/1995 (c) 1995 (Japan) (CPS Changer) */

	/* Capcom CPS2 games */
	/* list completed by CPS2Shock */
	/* http://cps2shock.retrogames.com */
TESTDRIVER( ssf2 )		/* Super Street Fighter 2: The New Challengers (USA 930911) */
TESTDRIVER( ssf2a )		/* Super Street Fighter 2: The New Challengers (Asia 930911) */
TESTDRIVER( ssf2j )		/* Super Street Fighter 2: The New Challengers (Japan 930910) */
TESTDRIVER( ecofe )		/* Eco Fighters (Etc 931203) */
TESTDRIVER( ddtod )		/* Dungeons & Dragons: Tower of Doom (USA 940113) */
TESTDRIVER( ddtoda )	/* Dungeons & Dragons: Tower of Doom (Asia 940113) */
TESTDRIVER( ddtodr1 )	/* Dungeons & Dragons: Tower of Doom (USA 940125) */
TESTDRIVER( ssf2t )		/* Super Street Fighter 2 Turbo (USA 940223) */
TESTDRIVER( ssf2xj )	/* Super Street Fighter 2 X: Grand Master Challenge (Japan 940223) */
TESTDRIVER( avsp )		/* Aliens Vs. Predator (USA 940520) */
TESTDRIVER( vampj )		/* Vampire: The Night Warriors (Japan 940705) */
TESTDRIVER( vampa )		/* Vampire: The Night Warriors (Asia 940705) */
TESTDRIVER( dstlk )		/* DarkStalkers: The Night Warriors (USA 940818) */
TESTDRIVER( slam2e )	/* Saturday Night Slammasters II: Ring of Destruction (Euro 940902) */
TESTDRIVER( armwara )	/* Armoured Warriors (Asia 940920) */
TESTDRIVER( xmcotaj )	/* X-Men: Children of the Atom (Japan 941219) */
TESTDRIVER( xmcota )	/* X-Men: Children of the Atom (USA 950105) */
TESTDRIVER( vhuntj )	/* Vampire Hunter: Darkstalkers 2 (Japan 950302) */
TESTDRIVER( nwarr )		/* Night Warriors: DarkStalkers Revenge (USA 950406) */
TESTDRIVER( cybotsj )	/* Cyberbots: Full Metal Madness (Japan 950420) */
TESTDRIVER( sfa )		/* Street Fighter Alpha: The Warriors Dream (USA 950627) */
TESTDRIVER( sfar1 )		/* Street Fighter Alpha: The Warriors Dream (USA 950727) */
TESTDRIVER( sfzj )		/* Street Fighter Zero (Japan 950627) */
TESTDRIVER( sfzjr1 )	/* Street Fighter Zero (Japan 950727) */
TESTDRIVER( msh )		/* Marvel Super Heroes (USA 951024) */
TESTDRIVER( 19xx )		/* 19XX: The Battle Against Destiny (USA 951207) */
TESTDRIVER( ddsom )		/* Dungeons & Dragons 2: Shadow over Mystara (USA 960209) */
TESTDRIVER( sfz2j )		/* Street Fighter Zero 2 (Japan 960227) */
TESTDRIVER( spf2xj )	/* Super Puzzle Fighter 2 X (Japan 960531) */
TESTDRIVER( spf2t )		/* Super Puzzle Fighter 2 Turbo (USA 960620) */
TESTDRIVER( rckman2j )	/* Rockman 2: The Power Fighters (Japan 960708) */
TESTDRIVER( sfz2a )		/* Street Fighter Zero 2 Alpha (Japan 960805) */
						/*  9/1996 Quiz Naneiro Dreams */
TESTDRIVER( xmvsf )		/* X-Men Vs. Street Fighter (USA 961004) */
TESTDRIVER( batcirj )	/* Battle Circuit (Japan 970319) */
TESTDRIVER( batcira )	/* Battle Circuit (Asia 970319) */
TESTDRIVER( vsav )		/* Vampire Savior: The Lord of Vampire (USA 970519) */
TESTDRIVER( vsavj )		/* Vampire Savior: The Lord of Vampire (Japan 970519) */
TESTDRIVER( mshvsf )	/* Marvel Super Heroes Vs. Street Fighter (USA 970625) */
TESTDRIVER( mshvsfj )	/* Marvel Super Heroes Vs. Street Fighter (Japan 970707) */
TESTDRIVER( vhunt2 )	/* Vampire Hunter 2: Darkstalkers Revenge (Japan 970828) */
TESTDRIVER( sgemf )		/* Super Gem Fighter Mini Mix (USA 970904) */
TESTDRIVER( pfghtj )	/* Pocket Fighter (Japan 970904) */
TESTDRIVER( vsav2 )		/* Vampire Savior 2: The Lord of Vampire (Japan 970913) */
TESTDRIVER( mvsc )		/* Marvel Super Heroes vs. Capcom: Clash of Super Heroes (USA 980123) */
TESTDRIVER( sfa3 )		/* Street Fighter Alpha 3 (USA 980629) */
						/* 1999 Giga Wing */
						/* Gulum Pa! */

	/* Capcom CPS3 games */
	/* 10/1996 Warzard */
	/*  2/1997 Street Fighter III - New Generation */
	/* ???? Jojo's Bizarre Adventure */
	/* ???? Street Fighter 3: Second Impact ~giant attack~ */
	/* ???? Street Fighter 3: Third Strike ~fight to the finish~ */

	/* Capcom ZN1/ZN2 games */
TESTDRIVER( ts2j )		/*  Battle Arena Toshinden 2 (JAPAN 951124) */
						/*  7/1996 Star Gladiator */
TESTDRIVER( sfex )		/*  Street Fighter EX (ASIA 961219) */
TESTDRIVER( sfexj )		/*  Street Fighter EX (JAPAN 961130) */
TESTDRIVER( sfexp )		/*  Street Fighter EX Plus (USA 970311) */
TESTDRIVER( sfexpj )	/*  Street Fighter EX Plus (JAPAN 970311) */
TESTDRIVER( rvschool )	/*  Rival Schools (ASIA 971117) */
TESTDRIVER( jgakuen )	/*  Justice Gakuen (JAPAN 971117) */
TESTDRIVER( sfex2 )		/*  Street Fighter EX 2 (JAPAN 980312) */
TESTDRIVER( tgmj )		/*  Tetris The Grand Master (JAPAN 980710) */
TESTDRIVER( sfex2p )	/*  Street Fighter EX 2 Plus (JAPAN 990611) */
						/*  Star Gladiator 2 */
						/*  Rival Schools 2 */

	/* Mitchell games */
	DRIVER( mgakuen )	/* (c) 1988 Yuga */
	DRIVER( mgakuen2 )	/* (c) 1989 Face */
	DRIVER( pkladies )	/* (c) 1989 Mitchell */
	DRIVER( dokaben )	/*  3/1989 (c) 1989 Capcom (Japan) */
	/*  8/1989 Dokaben 2 (baseball) */
	DRIVER( pang )		/* (c) 1989 Mitchell (World) */
	DRIVER( pangb )		/* bootleg */
	DRIVER( bbros )		/* (c) 1989 Capcom (US) not listed on Capcom's site */
	DRIVER( pompingw )	/* (c) 1989 Mitchell (Japan) */
	DRIVER( cbasebal )	/* 10/1989 (c) 1989 Capcom (Japan) (different hardware) */
	DRIVER( cworld )	/* 11/1989 (QUIZ 1) (c) 1989 Capcom */
	DRIVER( hatena )	/*  2/28/1990 (QUIZ 2) (c) 1990 Capcom (Japan) */
	DRIVER( spang )		/*  9/14/1990 (c) 1990 Mitchell (World) */
	DRIVER( sbbros )	/* 10/ 1/1990 (c) 1990 Mitchell + Capcom (US) not listed on Capcom's site */
	DRIVER( marukin )	/* 10/17/1990 (c) 1990 Yuga (Japan) */
	DRIVER( qtono1 )	/* 12/25/1990 (QUIZ 3) (c) 1991 Capcom (Japan) */
	/*  4/1991 Ashita Tenki ni Naare (golf) */
	DRIVER( qsangoku )	/*  6/ 7/1991 (QUIZ 4) (c) 1991 Capcom (Japan) */
	DRIVER( block )		/*  9/10/1991 (c) 1991 Capcom (World) */
	DRIVER( blockj )	/*  9/10/1991 (c) 1991 Capcom (Japan) */
	DRIVER( blockbl )	/* bootleg */

	/* Incredible Technologies games */
	DRIVER( capbowl )	/* (c) 1988 Incredible Technologies */
	DRIVER( capbowl2 )	/* (c) 1988 Incredible Technologies */
	DRIVER( clbowl )	/* (c) 1989 Incredible Technologies */
	DRIVER( bowlrama )	/* (c) 1991 P & P Marketing */
/*
The Incredible Technologies game list
http://www.itsgames.com/it/CorporateProfile/corporateprofile_main.htm

ShuffleShot - (Incredible Technologies, Inc.)
Peter Jacobsen's Golden Tee '97 - (Incredible Technologies, Inc.)
World Class Bowling - (Incredible Technologies, Inc.)
Peter Jacobsen's Golden Tee 3D Golf - (Incredible Technologies, Inc.)
Street Fighter - "The Movie" (Capcom)
PAIRS - (Strata)
BloodStorm - (Strata)
Driver's Edge - (Strata)
NFL Hard Yardage - (Strata)
Time Killers - (Strata)
Neck 'n' Neck - (Bundra Games)
Ninja Clowns - (Strata)
Rim Rockin' Basketball - (Strata)
Arlington Horse Racing - (Strata)
Dyno Bop - (Grand Products)
Poker Dice - (Strata)
Peggle - (Strata)
Slick Shot - (Grand Products)
Golden Tee Golf II - (Strata)
Hot Shots Tennis - (Strata)
Strata Bowling - (Strata)
Golden Tee Golf I - (Strata)
Capcom Bowling - (Strata)
*/

	/* Leland games */
	DRIVER( cerberus )	/* (c) 1985 Cinematronics */
	DRIVER( mayhem )	/* (c) 1985 Cinematronics */
	DRIVER( wseries )	/* (c) 1985 Cinematronics */
	DRIVER( alleymas )	/* (c) 1986 Cinematronics */
	DRIVER( dangerz )	/* (c) 1986 Cinematronics USA */
	DRIVER( basebal2 )	/* (c) 1987 Cinematronics */
	DRIVER( dblplay )	/* (c) 1987 Tradewest / Leland */
	DRIVER( strkzone )	/* (c) 1988 Leland */
	DRIVER( redlin2p )	/* (c) 1987 Cinematronics + Tradewest license */
	DRIVER( quarterb )	/* (c) 1987 Leland */
	DRIVER( quartrba )	/* (c) 1987 Leland */
	DRIVER( viper )		/* (c) 1988 Leland */
	DRIVER( teamqb )	/* (c) 1988 Leland */
	DRIVER( teamqb2 )	/* (c) 1988 Leland */
	DRIVER( aafb )		/* (c) 1989 Leland */
	DRIVER( aafbd2p )	/* (c) 1989 Leland */
	DRIVER( aafbb )		/* (c) 1989 Leland */
	DRIVER( offroad )	/* (c) 1989 Leland */
	DRIVER( offroadt )	/* (c) 1989 Leland */
	DRIVER( pigout )	/* (c) 1990 Leland */
	DRIVER( pigouta )	/* (c) 1990 Leland */
	DRIVER( ataxx )		/* (c) 1990 Leland */
	DRIVER( ataxxa )	/* (c) 1990 Leland */
	DRIVER( ataxxj )	/* (c) 1990 Leland */
	DRIVER( wsf )		/* (c) 1990 Leland */
	DRIVER( indyheat )	/* (c) 1991 Leland */
	DRIVER( brutforc )	/* (c) 1991 Leland */

	/* Gremlin 8080 games */
	/* the numbers listed are the range of ROM part numbers */
	DRIVER( blockade )	/* 1-4 [1977 Gremlin] */
	DRIVER( comotion )	/* 5-7 [1977 Gremlin] */
	DRIVER( hustle )	/* 16-21 [1977 Gremlin] */
	DRIVER( blasto )	/* [1978 Gremlin] */

	/* Gremlin/Sega "VIC dual game board" games */
	/* the numbers listed are the range of ROM part numbers */
	DRIVER( depthch )	/* 50-55 [1977 Gremlin?] */
	DRIVER( safari )	/* 57-66 [1977 Gremlin?] */
	DRIVER( frogs )		/* 112-119 [1978 Gremlin?] */
	DRIVER( sspaceat )	/* 155-162 (c) */
	DRIVER( sspacat2 )
	DRIVER( sspacatc )	/* 139-146 (c) */
	DRIVER( headon )	/* 163-167/192-193 (c) Gremlin */
	DRIVER( headonb )	/* 163-167/192-193 (c) Gremlin */
	DRIVER( headon2 )	/* ???-??? (c) 1979 Sega */
	/* ???-??? Fortress */
	/* ???-??? Gee Bee */
	/* 255-270  Head On 2 / Deep Scan */
	DRIVER( invho2 )	/* 271-286 (c) 1979 Sega */
	DRIVER( samurai )	/* 289-302 + upgrades (c) 1980 Sega */
	DRIVER( invinco )	/* 310-318 (c) 1979 Sega */
	DRIVER( invds )		/* 367-382 (c) 1979 Sega */
	DRIVER( tranqgun )	/* 413-428 (c) 1980 Sega */
	/* 450-465  Tranquilizer Gun (different version?) */
	/* ???-??? Car Hunt / Deep Scan */
	DRIVER( spacetrk )	/* 630-645 (c) 1980 Sega */
	DRIVER( sptrekct )	/* (c) 1980 Sega */
	DRIVER( carnival )	/* 651-666 (c) 1980 Sega */
	DRIVER( carnvckt )	/* 501-516 (c) 1980 Sega */
	DRIVER( digger )	/* 684-691 no copyright notice */
	DRIVER( pulsar )	/* 790-805 (c) 1981 Sega */
	DRIVER( heiankyo )	/* (c) [1979?] Denki Onkyo */

	/* Sega G-80 vector games */
	DRIVER( spacfury )	/* (c) 1981 */
	DRIVER( spacfura )	/* no copyright notice */
	DRIVER( zektor )	/* (c) 1982 */
	DRIVER( tacscan )	/* (c) */
	DRIVER( elim2 )		/* (c) 1981 Gremlin */
	DRIVER( elim2a )	/* (c) 1981 Gremlin */
	DRIVER( elim4 )		/* (c) 1981 Gremlin */
	DRIVER( startrek )	/* (c) 1982 */

	/* Sega G-80 raster games */
	DRIVER( astrob )	/* (c) 1981 */
	DRIVER( astrob2 )	/* (c) 1981 */
	DRIVER( astrob1 )	/* (c) 1981 */
	DRIVER( 005 )		/* (c) 1981 */
	DRIVER( monsterb )	/* (c) 1982 */
	DRIVER( spaceod )	/* (c) 1981 */
	DRIVER( pignewt )	/* (c) 1983 */
	DRIVER( pignewta )	/* (c) 1983 */
	DRIVER( sindbadm )	/* 834-5244 (c) 1983 Sega */

	/* Sega "Zaxxon hardware" games */
	DRIVER( zaxxon )	/* (c) 1982 */
	DRIVER( zaxxon2 )	/* (c) 1982 */
	DRIVER( zaxxonb )	/* bootleg */
	DRIVER( szaxxon )	/* (c) 1982 */
	DRIVER( futspy )	/* (c) 1984 */
	DRIVER( razmataz )	/* modified 834-0213, 834-0214 (c) 1983 */
	DRIVER( congo )		/* 605-5167 (c) 1983 */
	DRIVER( tiptop )	/* 605-5167 (c) 1983 */

	/* Sega System 1 / System 2 games */
	DRIVER( starjack )	/* 834-5191 (c) 1983 (S1) */
	DRIVER( starjacs )	/* (c) 1983 Stern (S1) */
	DRIVER( regulus )	/* 834-5328 (c) 1983 (S1) */
	DRIVER( regulusu )	/* 834-5328 (c) 1983 (S1) */
	DRIVER( upndown )	/* (c) 1983 (S1) */
	DRIVER( mrviking )	/* 834-5383 (c) 1984 (S1) */
	DRIVER( mrvikinj )	/* 834-5383 (c) 1984 (S1) */
	DRIVER( swat )		/* 834-5388 (c) 1984 Coreland / Sega (S1) */
	DRIVER( flicky )	/* (c) 1984 (S1) */
	DRIVER( flicky2 )	/* (c) 1984 (S1) */
	/* Water Match (S1) */
	DRIVER( bullfgtj )	/* 834-5478 (c) 1984 Sega / Coreland (S1) */
	DRIVER( pitfall2 )	/* 834-5627 [1985?] reprogrammed, (c) 1984 Activision (S1) */
	DRIVER( pitfallu )	/* 834-5627 [1985?] reprogrammed, (c) 1984 Activision (S1) */
	DRIVER( seganinj )	/* 834-5677 (c) 1985 (S1) */
	DRIVER( seganinu )	/* 834-5677 (c) 1985 (S1) */
	DRIVER( nprinces )	/* 834-5677 (c) 1985 (S1) */
	DRIVER( nprincsu )	/* 834-5677 (c) 1985 (S1) */
	DRIVER( nprincsb )	/* bootleg? (S1) */
	DRIVER( imsorry )	/* 834-5707 (c) 1985 Coreland / Sega (S1) */
	DRIVER( imsorryj )	/* 834-5707 (c) 1985 Coreland / Sega (S1) */
	DRIVER( teddybb )	/* 834-5712 (c) 1985 (S1) */
	DRIVER( hvymetal )	/* 834-5745 (c) 1985 (S2?) */
	DRIVER( myhero )	/* 834-5755 (c) 1985 (S1) */
	DRIVER( myheroj )	/* 834-5755 (c) 1985 Coreland / Sega (S1) */
	DRIVER( myherok )	/* 834-5755 (c) 1985 Coreland / Sega (S1) */
	DRIVER( shtngmst )	/* 834-5719/5720 (c) 1985 (S2) */
	DRIVER( chplft )	/* 834-5795 (c) 1985, (c) 1982 Dan Gorlin (S2) */
	DRIVER( chplftb )	/* 834-5795 (c) 1985, (c) 1982 Dan Gorlin (S2) */
	DRIVER( chplftbl )	/* bootleg (S2) */
	DRIVER( 4dwarrio )	/* 834-5918 (c) 1985 Coreland / Sega (S1) */
	DRIVER( brain )		/* (c) 1986 Coreland / Sega (S2?) */
	DRIVER( wboy )		/* 834-5984 (c) 1986 + Escape license (S1) */
	DRIVER( wboy2 )		/* 834-5984 (c) 1986 + Escape license (S1) */
	DRIVER( wboy3 )
	DRIVER( wboy4 )		/* 834-5984 (c) 1986 + Escape license (S1) */
	DRIVER( wboyu )		/* 834-5753 (? maybe a conversion) (c) 1986 + Escape license (S1) */
	DRIVER( wboy4u )	/* 834-5984 (c) 1986 + Escape license (S1) */
	DRIVER( wbdeluxe )	/* (c) 1986 + Escape license (S1) */
	DRIVER( gardia )	/* 834-6119 (S2?) */
	DRIVER( gardiab )	/* bootleg */
	DRIVER( blockgal )	/* 834-6303 (S1) */
	DRIVER( blckgalb )	/* bootleg */
	DRIVER( tokisens )	/* (c) 1987 (from a bootleg board) (S2) */
	DRIVER( wbml )		/* bootleg (S2) */
	DRIVER( wbmlj )		/* (c) 1987 Sega/Westone (S2) */
	DRIVER( wbmlj2 )	/* (c) 1987 Sega/Westone (S2) */
	DRIVER( wbmlju )	/* bootleg? (S2) */
	DRIVER( dakkochn )	/* 836-6483? (S2) */
	DRIVER( ufosensi )	/* 834-6659 (S2) */
/*
other System 1 / System 2 games:

WarBall
Rafflesia
Sanrin Sanchan
DokiDoki Penguin Land *not confirmed
*/

	/* Sega System E games (Master System hardware) */
/*
???          834-5492 (??? not sure it's System E)
Transformer  834-5803 (c) 1986
Opa Opa
Fantasy Zone 2
Hang-On Jr.
(more?)
*/

	/* other Sega 8-bit games */
	DRIVER( turbo )		/* (c) 1981 Sega */
	DRIVER( turboa )	/* (c) 1981 Sega */
	DRIVER( turbob )	/* (c) 1981 Sega */
TESTDRIVER( kopunch )	/* 834-0103 (c) 1981 Sega */
	DRIVER( suprloco )	/* (c) 1982 Sega */
	DRIVER( appoooh )	/* (c) 1984 Sega */
	DRIVER( bankp )		/* (c) 1984 Sega */
	DRIVER( dotrikun )	/* cabinet test board */
	DRIVER( dotriku2 )	/* cabinet test board */

	/* Sega System 16 games */
	// Not working
	DRIVER( alexkidd )	/* (c) 1986 (protected) */
	DRIVER( aliensya )	/* (c) 1987 (protected) */
	DRIVER( aliensyb )	/* (c) 1987 (protected) */
	DRIVER( aliensyj )	/* (c) 1987 (protected. Japan) */
	DRIVER( astorm )	/* (c) 1990 (protected) */
	DRIVER( astorm2p )	/* (c) 1990 (protected 2 Players) */
	DRIVER( auraila )	/* (c) 1990 Sega / Westone (protected) */
	DRIVER( bayrouta )	/* (c) 1989 (protected) */
	DRIVER( bayrtbl1 )	/* (c) 1989 (protected) (bootleg) */
	DRIVER( bayrtbl2 )	/* (c) 1989 (protected) (bootleg) */
	DRIVER( enduror )	/* (c) 1985 (protected) */
	DRIVER( eswat )		/* (c) 1989 (protected) */
	DRIVER( fpoint )	/* (c) 1989 (protected) */
	DRIVER( goldnaxb )	/* (c) 1989 (protected) */
	DRIVER( goldnaxc )	/* (c) 1989 (protected) */
	DRIVER( goldnaxj )	/* (c) 1989 (protected. Japan) */
	DRIVER( jyuohki )	/* (c) 1988 (protected. Altered Beast Japan) */
	DRIVER( moonwalk )	/* (c) 1990 (protected) */
	DRIVER( moonwlka )	/* (c) 1990 (protected) */
	DRIVER( passsht )	/* (protected) */
	DRIVER( sdioj )		/* (c) 1987 (protected. Japan) */
	DRIVER( shangon )	/* (c) 1992 (protected) */
	DRIVER( shinobia )	/* (c) 1987 (protected) */
	DRIVER( shinobib )	/* (c) 1987 (protected) */
	DRIVER( tetris )	/* (c) 1988 (protected) */
	DRIVER( tetrisa )	/* (c) 1988 (protected) */
	DRIVER( wb3a )		/* (c) 1988 Sega / Westone (protected) */

TESTDRIVER( aceattac )	/* (protected) */
TESTDRIVER( aburner )	/* */
TESTDRIVER( aburner2 )  /* */
TESTDRIVER( afighter )	/* (protected) */
TESTDRIVER( bloxeed )	/* (protected) */
TESTDRIVER( cltchitr )	/* (protected) */
TESTDRIVER( cotton )	/* (protected) */
TESTDRIVER( cottona )	/* (protected) */
TESTDRIVER( ddcrew )	/* (protected) */
TESTDRIVER( dunkshot )	/* (protected) */
TESTDRIVER( exctleag )  /* (protected) */
TESTDRIVER( lghost )	/* (protected) */
TESTDRIVER( loffire )	/* (protected) */
TESTDRIVER( mvp )		/* (protected) */
TESTDRIVER( ryukyu )	/* (protected) */
TESTDRIVER( suprleag )  /* (protected) */
TESTDRIVER( thndrbld )	/* (protected) */
TESTDRIVER( thndrbdj )  /* (protected?) */
TESTDRIVER( toutrun )	/* (protected) */
TESTDRIVER( toutruna )	/* (protected) */

	// Working
	DRIVER( alexkida )	/* (c) 1986 */
	DRIVER( aliensyn )	/* (c) 1987 */
	DRIVER( altbeas2 )	/* (c) 1988 */
	DRIVER( altbeast )	/* (c) 1988 */
	DRIVER( astormbl )	/* bootleg */
	DRIVER( atomicp )	/* (c) 1990 Philko */
	DRIVER( aurail )	/* (c) 1990 Sega / Westone */
	DRIVER( bayroute )	/* (c) 1989 */
	DRIVER( bodyslam )	/* (c) 1986 */
	DRIVER( dduxbl )	/* (c) 1989 (Datsu bootleg) */
	DRIVER( dumpmtmt )	/* (c) 1986 (Japan) */
	DRIVER( endurob2 )	/* (c) 1985 (Beta bootleg) */
	DRIVER( endurobl )	/* (c) 1985 (Herb bootleg) */
	DRIVER( eswatbl )	/* (c) 1989 (but bootleg) */
	DRIVER( fantzone )	/* (c) 1986 */
	DRIVER( fantzono )	/* (c) 1986 */
	DRIVER( fpointbl )	/* (c) 1989 (Datsu bootleg) */
	DRIVER( goldnabl )	/* (c) 1989 (bootleg) */
	DRIVER( goldnaxa )	/* (c) 1989 */
	DRIVER( goldnaxe )	/* (c) 1989 */
	DRIVER( hangon )	/* (c) 1985 */
	DRIVER( hwchamp )	/* (c) 1987 */
	DRIVER( mjleague )	/* (c) 1985 */
	DRIVER( moonwlkb )	/* bootleg */
	DRIVER( outrun )	/* (c) 1986 (bootleg)*/
	DRIVER( outruna )	/* (c) 1986 (bootleg) */
	DRIVER( outrunb )	/* (c) 1986 (protected beta bootleg) */
	DRIVER( passht4b )	/* bootleg */
	DRIVER( passshtb )	/* bootleg */
	DRIVER( quartet )	/* (c) 1986 */
	DRIVER( quartet2 )	/* (c) 1986 */
	DRIVER( quartetj )	/* (c) 1986 */
	DRIVER( riotcity )	/* (c) 1991 Sega / Westone */
	DRIVER( sdi )		/* (c) 1987 */
	DRIVER( shangonb )	/* (c) 1992 (but bootleg) */
	DRIVER( sharrier )	/* (c) 1985 */
	DRIVER( shdancbl )	/* (c) 1989 (but bootleg) */
	DRIVER( shdancer )	/* (c) 1989 */
	DRIVER( shdancrj )	/* (c) 1989 */
	DRIVER( shinobi )	/* (c) 1987 */
	DRIVER( shinobl )	/* (c) 1987 (but bootleg) */
	DRIVER( tetrisbl )	/* (c) 1988 (but bootleg) */
	DRIVER( timscanr )	/* (c) 1987 */
	DRIVER( toryumon )	/* (c) 1995 */
	DRIVER( tturf )		/* (c) 1989 Sega / Sunsoft */
	DRIVER( tturfbl )	/* (c) 1989 (Datsu bootleg) */
	DRIVER( tturfu )	/* (c) 1989 Sega / Sunsoft */
	DRIVER( wb3 )		/* (c) 1988 Sega / Westone */
	DRIVER( wb3bl )		/* (c) 1988 Sega / Westone (but bootleg) */
	DRIVER( wrestwar )	/* (c) 1989 */
	/* Deniam games */
	/* they run on Sega System 16 video hardware */
	DRIVER( logicpro )      /* (c) 1996 Deniam */
	DRIVER( karianx )       /* (c) 1996 Deniam */
	DRIVER( logicpr2 )      /* (c) 1997 Deniam (Japan) */
/*
Deniam is a Korean company (http://deniam.co.kr).

Game list:
Title            System     Date
---------------- ---------- ----------
GO!GO!           deniam-16b 1995/10/11
Logic Pro        deniam-16b 1996/10/20
Karian Cross     deniam-16b 1997/04/17
LOTTERY GAME     deniam-16c 1997/05/21
Logic Pro 2      deniam-16c 1997/06/20
Propose          deniam-16c 1997/06/21
BOMULEUL CHAJARA SEGA ST-V  1997/04/11
*/

	/* Data East "Burger Time hardware" games */
	DRIVER( lnc )		/* (c) 1981 */
	DRIVER( zoar )		/* (c) 1982 */
	DRIVER( btime )		/* (c) 1982 */
	DRIVER( btime2 )	/* (c) 1982 */
	DRIVER( btimem )	/* (c) 1982 + Midway */
	DRIVER( wtennis )	/* bootleg 1982 */
	DRIVER( brubber )	/* (c) 1982 */
	DRIVER( bnj )		/* (c) 1982 + Midway */
	DRIVER( caractn )	/* bootleg */
	DRIVER( disco )		/* (c) 1982 */
	DRIVER( mmonkey )	/* (c) 1982 Technos Japan + Roller Tron */
	/* cassette system */
TESTDRIVER( decocass )
	DRIVER( cookrace )	/* bootleg */

	/* other Data East games */
	DRIVER( astrof )	/* (c) [1980?] */
	DRIVER( astrof2 )	/* (c) [1980?] */
	DRIVER( astrof3 )	/* (c) [1980?] */
	DRIVER( tomahawk )	/* (c) [1980?] */
	DRIVER( tomahaw5 )	/* (c) [1980?] */
	DRIVER( kchamp )	/* (c) 1984 Data East USA (US) */
	DRIVER( karatedo )	/* (c) 1984 Data East Corporation (Japan) */
	DRIVER( kchampvs )	/* (c) 1984 Data East USA (US) */
	DRIVER( karatevs )	/* (c) 1984 Data East Corporation (Japan) */
	DRIVER( firetrap )	/* (c) 1986 */
	DRIVER( firetpbl )	/* bootleg */
	DRIVER( brkthru )	/* (c) 1986 Data East USA (US) */
	DRIVER( brkthruj )	/* (c) 1986 Data East Corporation (Japan) */
	DRIVER( darwin )	/* (c) 1986 Data East Corporation (Japan) */
	DRIVER( shootout )	/* (c) 1985 Data East USA (US) */
	DRIVER( shootouj )	/* (c) 1985 Data East USA (Japan) */
	DRIVER( shootoub )	/* bootleg */
	DRIVER( sidepckt )	/* (c) 1986 Data East Corporation */
	DRIVER( sidepctj )	/* (c) 1986 Data East Corporation */
	DRIVER( sidepctb )	/* bootleg */
	DRIVER( exprraid )	/* (c) 1986 Data East USA (US) */
	DRIVER( wexpress )	/* (c) 1986 Data East Corporation (World?) */
	DRIVER( wexpresb )	/* bootleg */
	DRIVER( pcktgal )	/* (c) 1987 Data East Corporation (Japan) */
	DRIVER( pcktgalb )	/* bootleg */
	DRIVER( pcktgal2 )	/* (c) 1989 Data East Corporation (World?) */
	DRIVER( spool3 )	/* (c) 1989 Data East Corporation (World?) */
	DRIVER( spool3i )	/* (c) 1990 Data East Corporation + I-Vics license */
	DRIVER( battlera )	/* (c) 1988 Data East Corporation (World) */
	DRIVER( bldwolf )	/* (c) 1988 Data East USA (US) */
	DRIVER( actfancr )	/* (c) 1989 Data East Corporation (World) */
	DRIVER( actfanc1 )	/* (c) 1989 Data East Corporation (World) */
	DRIVER( actfancj )	/* (c) 1989 Data East Corporation (Japan) */
	DRIVER( triothep )	/* (c) 1989 Data East Corporation (Japan) */

	/* Data East 8-bit games */
	DRIVER( lastmiss )	/* (c) 1986 Data East USA (US) */
	DRIVER( lastmss2 )	/* (c) 1986 Data East USA (US) */
	DRIVER( shackled )	/* (c) 1986 Data East USA (US) */
	DRIVER( breywood )	/* (c) 1986 Data East Corporation (Japan) */
	DRIVER( csilver )	/* (c) 1987 Data East Corporation (Japan) */
	DRIVER( ghostb )	/* (c) 1987 Data East USA (US) */
	DRIVER( ghostb3 )	/* (c) 1987 Data East USA (US) */
	DRIVER( meikyuh )	/* (c) 1987 Data East Corporation (Japan) */
	DRIVER( srdarwin )	/* (c) 1987 Data East Corporation (Japan) */
	DRIVER( gondo )		/* (c) 1987 Data East USA (US) */
	DRIVER( makyosen )	/* (c) 1987 Data East Corporation (Japan) */
	DRIVER( garyoret )	/* (c) 1987 Data East Corporation (Japan) */
	DRIVER( cobracom )	/* (c) 1988 Data East Corporation (World) */
	DRIVER( cobracmj )	/* (c) 1988 Data East Corporation (Japan) */
	DRIVER( oscar )		/* (c) 1988 Data East USA (US) */
	DRIVER( oscarj )	/* (c) 1987 Data East Corporation (Japan) */
	DRIVER( oscarj1 )	/* (c) 1987 Data East Corporation (Japan) */
	DRIVER( oscarj0 )	/* (c) 1987 Data East Corporation (Japan) */

	/* Data East 16-bit games */
	DRIVER( karnov )	/* (c) 1987 Data East USA (US) */
	DRIVER( karnovj )	/* (c) 1987 Data East Corporation (Japan) */
TESTDRIVER( wndrplnt )	/* (c) 1987 Data East Corporation (Japan) */
	DRIVER( chelnov )	/* (c) 1988 Data East USA (US) */
	DRIVER( chelnovj )	/* (c) 1988 Data East Corporation (Japan) */
/* the following ones all run on similar hardware */
	DRIVER( hbarrel )	/* (c) 1987 Data East USA (US) */
	DRIVER( hbarrelw )	/* (c) 1987 Data East Corporation (World) */
	DRIVER( baddudes )	/* (c) 1988 Data East USA (US) */
	DRIVER( drgninja )	/* (c) 1988 Data East Corporation (Japan) */
TESTDRIVER( birdtry )	/* (c) 1988 Data East Corporation (Japan) */
	DRIVER( robocop )	/* (c) 1988 Data East Corporation (World) */
	DRIVER( robocopu )	/* (c) 1988 Data East USA (US) */
	DRIVER( robocpu0 )	/* (c) 1988 Data East USA (US) */
	DRIVER( robocopb )	/* bootleg */
	DRIVER( hippodrm )	/* (c) 1989 Data East USA (US) */
	DRIVER( ffantasy )	/* (c) 1989 Data East Corporation (Japan) */
	DRIVER( slyspy )	/* (c) 1989 Data East USA (US) */
	DRIVER( slyspy2 )	/* (c) 1989 Data East USA (US) */
	DRIVER( secretag )	/* (c) 1989 Data East Corporation (World) */
TESTDRIVER( secretab )	/* bootleg */
	DRIVER( midres )	/* (c) 1989 Data East Corporation (World) */
	DRIVER( midresu )	/* (c) 1989 Data East USA (US) */
	DRIVER( midresj )	/* (c) 1989 Data East Corporation (Japan) */
	DRIVER( bouldash )	/* (c) 1990 Data East Corporation */
/* end of similar hardware */
	DRIVER( stadhero )	/* (c) 1988 Data East Corporation (Japan) */
	DRIVER( madmotor )	/* (c) [1989] Mitchell */
	/* All these games have a unique code stamped on the mask roms */
	DRIVER( vaportra )	/* MAA (c) 1989 Data East Corporation (World) */
	DRIVER( vaportru )	/* MAA (c) 1989 Data East Corporation (US) */
	DRIVER( kuhga )		/* MAA (c) 1989 Data East Corporation (Japan) */
	DRIVER( cbuster )	/* MAB (c) 1990 Data East Corporation (World) */
	DRIVER( cbusterw )	/* MAB (c) 1990 Data East Corporation (World) */
	DRIVER( cbusterj )	/* MAB (c) 1990 Data East Corporation (Japan) */
	DRIVER( twocrude )	/* MAB (c) 1990 Data East USA (US) */
	DRIVER( darkseal )	/* MAC (c) 1990 Data East Corporation (World) */
	DRIVER( darksea1 )	/* MAC (c) 1990 Data East Corporation (World) */
	DRIVER( darkseaj )	/* MAC (c) 1990 Data East Corporation (Japan) */
	DRIVER( gatedoom )	/* MAC (c) 1990 Data East Corporation (US) */
	DRIVER( gatedom1 )	/* MAC (c) 1990 Data East Corporation (US) */
TESTDRIVER( edrandy )	/* MAD (c) 1990 Data East Corporation (World) */
TESTDRIVER( edrandyj )	/* MAD (c) 1990 Data East Corporation (Japan) */
	DRIVER( supbtime )	/* MAE (c) 1990 Data East Corporation (World) */
	DRIVER( supbtimj )	/* MAE (c) 1990 Data East Corporation (Japan) */
	/* Mutant Fighter/Death Brade MAF (c) 1991 */
	DRIVER( cninja )	/* MAG (c) 1991 Data East Corporation (World) */
	DRIVER( cninja0 )	/* MAG (c) 1991 Data East Corporation (World) */
	DRIVER( cninjau )	/* MAG (c) 1991 Data East Corporation (US) */
	DRIVER( joemac )	/* MAG (c) 1991 Data East Corporation (Japan) */
	DRIVER( stoneage )	/* bootleg */
	/* Robocop 2           MAH (c) 1991 */
	/* Desert Assault/Thunderzone MAJ (c) 1991 */
	/* Rohga Armour Attack/Wolf Fang MAM (c) 1991 */
	/* Captain America     MAN (c) 1991 */
	DRIVER( tumblep )	/* MAP (c) 1991 Data East Corporation (World) */
	DRIVER( tumblepj )	/* MAP (c) 1991 Data East Corporation (Japan) */
	DRIVER( tumblepb )	/* bootleg */
	DRIVER( tumblep2 )	/* bootleg */
	/* Dragon Gun/Dragoon  MAR (c) 1992 */
	/* Wizard's Fire       MAS (c) 1992 */
TESTDRIVER( funkyjet )	/* MAT (c) 1992 Mitchell */
	/* Diet GoGo      	   MAY (c) 1993 */
 	/* Fighter's History   MBF (c) 1993 */
	/* Joe & Mac Return    MBN (c) 1994 */
	/* Chain Reaction      MCC (c) 1994 */

	/* Tehkan / Tecmo games (Tehkan became Tecmo in 1986) */
	DRIVER( senjyo )	/* (c) 1983 Tehkan */
	DRIVER( starforc )	/* (c) 1984 Tehkan */
	DRIVER( starfore )	/* (c) 1984 Tehkan */
	DRIVER( megaforc )	/* (c) 1985 Tehkan + Video Ware license */
	DRIVER( baluba )	/* (c) 1986 Able Corp. */
	DRIVER( bombjack )	/* (c) 1984 Tehkan */
	DRIVER( bombjac2 )	/* (c) 1984 Tehkan */
	DRIVER( pbaction )	/* (c) 1985 Tehkan */
	DRIVER( pbactio2 )	/* (c) 1985 Tehkan */
	/* 6009 Tank Busters */
	/* 6011 Pontoon (c) 1985 Tehkan is a gambling game - removed */
	DRIVER( tehkanwc )	/* (c) 1985 Tehkan */
	DRIVER( gridiron )	/* (c) 1985 Tehkan */
	DRIVER( teedoff )	/* 6102 - (c) 1986 Tecmo */
	DRIVER( solomon )	/* (c) 1986 Tecmo */
	DRIVER( rygar )		/* 6002 - (c) 1986 Tecmo */
	DRIVER( rygar2 )	/* 6002 - (c) 1986 Tecmo */
	DRIVER( rygarj )	/* 6002 - (c) 1986 Tecmo */
	DRIVER( gemini )	/* (c) 1987 Tecmo */
	DRIVER( silkworm )	/* 6217 - (c) 1988 Tecmo */
	DRIVER( silkwrm2 )	/* 6217 - (c) 1988 Tecmo */
	DRIVER( gaiden )	/* 6215 - (c) 1988 Tecmo (World) */
	DRIVER( shadoww )	/* 6215 - (c) 1988 Tecmo (US) */
	DRIVER( ryukendn )	/* 6215 - (c) 1989 Tecmo (Japan) */
	DRIVER( tknight )	/* (c) 1989 Tecmo */
	DRIVER( wildfang )	/* (c) 1989 Tecmo */
	DRIVER( wc90 )		/* (c) 1989 Tecmo */
	DRIVER( wc90b )		/* bootleg */
	DRIVER( fstarfrc )	/* (c) 1992 Tecmo */
	DRIVER( ginkun )	/* (c) 1995 Tecmo */

	/* Konami bitmap games */
	DRIVER( tutankhm )	/* GX350 (c) 1982 Konami */
	DRIVER( tutankst )	/* GX350 (c) 1982 Stern */
	DRIVER( junofrst )	/* GX310 (c) 1983 Konami */
	DRIVER( junofstg )	/* GX310 (c) 1983 Konami + Gottlieb license */

	/* Konami games */
	DRIVER( pooyan )	/* GX320 (c) 1982 */
	DRIVER( pooyans )	/* GX320 (c) 1982 Stern */
	DRIVER( pootan )	/* bootleg */
	DRIVER( timeplt )	/* GX393 (c) 1982 */
	DRIVER( timepltc )	/* GX393 (c) 1982 + Centuri license*/
	DRIVER( spaceplt )	/* bootleg */
	DRIVER( psurge )	/* (c) 1988 unknown (NOT Konami) */
	DRIVER( megazone )	/* GX319 (c) 1983 */
	DRIVER( megaznik )	/* GX319 (c) 1983 + Interlogic / Kosuka */
	DRIVER( pandoras )	/* GX328 (c) 1984 + Interlogic */
	DRIVER( gyruss )	/* GX347 (c) 1983 */
	DRIVER( gyrussce )	/* GX347 (c) 1983 + Centuri license */
	DRIVER( venus )		/* bootleg */
	DRIVER( trackfld )	/* GX361 (c) 1983 */
	DRIVER( trackflc )	/* GX361 (c) 1983 + Centuri license */
	DRIVER( hyprolym )	/* GX361 (c) 1983 */
	DRIVER( hyprolyb )	/* bootleg */
	DRIVER( rocnrope )	/* GX364 (c) 1983 */
	DRIVER( rocnropk )	/* GX364 (c) 1983 + Kosuka */
	DRIVER( circusc )	/* GX380 (c) 1984 */
	DRIVER( circusc2 )	/* GX380 (c) 1984 */
	DRIVER( circuscc )	/* GX380 (c) 1984 + Centuri license */
	DRIVER( circusce )	/* GX380 (c) 1984 + Centuri license */
	DRIVER( tp84 )		/* GX388 (c) 1984 */
	DRIVER( tp84a )		/* GX388 (c) 1984 */
	DRIVER( hyperspt )	/* GX330 (c) 1984 + Centuri */
	DRIVER( hpolym84 )	/* GX330 (c) 1984 */
	DRIVER( sbasketb )	/* GX405 (c) 1984 */
	DRIVER( mikie )		/* GX469 (c) 1984 */
	DRIVER( mikiej )	/* GX469 (c) 1984 */
	DRIVER( mikiehs )	/* GX469 (c) 1984 */
	DRIVER( roadf )		/* GX461 (c) 1984 */
	DRIVER( roadf2 )	/* GX461 (c) 1984 */
	DRIVER( yiear )		/* GX407 (c) 1985 */
	DRIVER( yiear2 )	/* GX407 (c) 1985 */
	DRIVER( kicker )	/* GX477 (c) 1985 */
	DRIVER( shaolins )	/* GX477 (c) 1985 */
	DRIVER( pingpong )	/* GX555 (c) 1985 */
	DRIVER( gberet )	/* GX577 (c) 1985 */
	DRIVER( rushatck )	/* GX577 (c) 1985 */
	DRIVER( gberetb )	/* bootleg on different hardware */
	DRIVER( mrgoemon )	/* GX621 (c) 1986 (Japan) */
	DRIVER( jailbrek )	/* GX507 (c) 1986 */
	DRIVER( finalizr )	/* GX523 (c) 1985 */
	DRIVER( finalizb )	/* bootleg */
	DRIVER( ironhors )	/* GX560 (c) 1986 */
	DRIVER( dairesya )	/* GX560 (c) 1986 (Japan) */
	DRIVER( farwest )
	DRIVER( jackal )	/* GX631 (c) 1986 (World) */
	DRIVER( topgunr )	/* GX631 (c) 1986 (US) */
	DRIVER( jackalj )	/* GX631 (c) 1986 (Japan) */
	DRIVER( topgunbl )	/* bootleg */
	DRIVER( ddribble )	/* GX690 (c) 1986 */
	DRIVER( contra )	/* GX633 (c) 1987 */
	DRIVER( contrab )	/* bootleg */
	DRIVER( contraj )	/* GX633 (c) 1987 (Japan) */
	DRIVER( contrajb )	/* bootleg */
	DRIVER( gryzor )	/* GX633 (c) 1987 */
	DRIVER( combasc )	/* GX611 (c) 1988 */
	DRIVER( combasct )	/* GX611 (c) 1987 */
	DRIVER( combascj )	/* GX611 (c) 1987 (Japan) */
	DRIVER( bootcamp )	/* GX611 (c) 1987 */
	DRIVER( combascb )	/* bootleg */
	DRIVER( rockrage )	/* GX620 (c) 1986 (World?) */
	DRIVER( rockragj )	/* GX620 (c) 1986 (Japan) */
	DRIVER( mx5000 )	/* GX669 (c) 1987 */
	DRIVER( flkatck )	/* GX669 (c) 1987 (Japan) */
	DRIVER( fastlane )	/* GX752 (c) 1987 */
	DRIVER( labyrunr )	/* GX771 (c) 1987 (Japan) */
	DRIVER( thehustl )	/* GX765 (c) 1987 (Japan) */
	DRIVER( thehustj )	/* GX765 (c) 1987 (Japan) */
	DRIVER( rackemup )	/* GX765 (c) 1987 */
	DRIVER( battlnts )	/* GX777 (c) 1987 */
	DRIVER( battlntj )	/* GX777 (c) 1987 (Japan) */
	DRIVER( bladestl )	/* GX797 (c) 1987 */
	DRIVER( bladstle )	/* GX797 (c) 1987 */
	DRIVER( hcastle )	/* GX768 (c) 1988 */
	DRIVER( hcastlea )	/* GX768 (c) 1988 */
	DRIVER( hcastlej )	/* GX768 (c) 1988 (Japan) */
	DRIVER( ajax )		/* GX770 (c) 1987 */
	DRIVER( ajaxj )		/* GX770 (c) 1987 (Japan) */
	DRIVER( scontra )	/* GX775 (c) 1988 */
	DRIVER( scontraj )	/* GX775 (c) 1988 (Japan) */
	DRIVER( thunderx )	/* GX873 (c) 1988 */
	DRIVER( thnderxj )	/* GX873 (c) 1988 (Japan) */
	DRIVER( mainevt )	/* GX799 (c) 1988 */
	DRIVER( mainevt2 )	/* GX799 (c) 1988 */
	DRIVER( ringohja )	/* GX799 (c) 1988 (Japan) */
	DRIVER( devstors )	/* GX890 (c) 1988 */
	DRIVER( devstor2 )	/* GX890 (c) 1988 */
	DRIVER( devstor3 )	/* GX890 (c) 1988 */
	DRIVER( garuka )	/* GX890 (c) 1988 (Japan) */
	DRIVER( 88games )	/* GX861 (c) 1988 */
	DRIVER( konami88 )	/* GX861 (c) 1988 */
	DRIVER( hypsptsp )	/* GX861 (c) 1988 (Japan) */
	DRIVER( gbusters )	/* GX878 (c) 1988 */
	DRIVER( crazycop )	/* GX878 (c) 1988 (Japan) */
	DRIVER( crimfght )	/* GX821 (c) 1989 (US) */
	DRIVER( crimfgt2 )	/* GX821 (c) 1989 (World) */
	DRIVER( crimfgtj )	/* GX821 (c) 1989 (Japan) */
	DRIVER( spy )		/* GX857 (c) 1989 (US) */
	DRIVER( bottom9 )	/* GX891 (c) 1989 */
	DRIVER( bottom9n )	/* GX891 (c) 1989 */
	DRIVER( blockhl )	/* GX973 (c) 1989 */
	DRIVER( quarth )	/* GX973 (c) 1989 (Japan) */
	DRIVER( aliens )	/* GX875 (c) 1990 (World) */
	DRIVER( aliens2 )	/* GX875 (c) 1990 (World) */
	DRIVER( aliensu )	/* GX875 (c) 1990 (US) */
	DRIVER( aliensj )	/* GX875 (c) 1990 (Japan) */
	DRIVER( surpratk )	/* GX911 (c) 1990 (Japan) */
	DRIVER( parodius )	/* GX955 (c) 1990 (Japan) */
	DRIVER( rollerg )	/* GX999 (c) 1991 (US) */
	DRIVER( rollergj )	/* GX999 (c) 1991 (Japan) */
TESTDRIVER( xexex )		/* GX067 (c) 1991 */
	DRIVER( simpsons )	/* GX072 (c) 1991 */
	DRIVER( simpsn2p )	/* GX072 (c) 1991 */
	DRIVER( simps2pj )	/* GX072 (c) 1991 (Japan) */
	DRIVER( vendetta )	/* GX081 (c) 1991 (Asia) */
	DRIVER( vendett2 )	/* GX081 (c) 1991 (Asia) */
	DRIVER( vendettj )	/* GX081 (c) 1991 (Japan) */
	DRIVER( wecleman )	/* GX602 (c) 1986 */
	DRIVER( hotchase )	/* GX763 (c) 1988 */
TESTDRIVER( chqflag )	/* GX717 (c) 1988 */
TESTDRIVER( chqflagj )	/* GX717 (c) 1988 (Japan) */
	DRIVER( ultraman )	/* GX910 (c) 1991 Banpresto/Bandai */

	/* Konami "Nemesis hardware" games */
	DRIVER( nemesis )	/* GX456 (c) 1985 */
	DRIVER( nemesuk )	/* GX456 (c) 1985 */
	DRIVER( konamigt )	/* GX561 (c) 1985 */
	DRIVER( salamand )	/* GX587 (c) 1986 */
	DRIVER( lifefrce )	/* GX587 (c) 1986 */
	DRIVER( lifefrcj )	/* GX587 (c) 1986 */
	/* GX400 BIOS based games */
	DRIVER( rf2 )		/* GX561 (c) 1985 */
	DRIVER( twinbee )	/* GX412 (c) 1985 */
	DRIVER( gradius )	/* GX456 (c) 1985 */
	DRIVER( gwarrior )	/* GX578 (c) 1985 */

	/* Konami "Twin 16" games */
	DRIVER( devilw )	/* GX687 (c) 1987 */
	DRIVER( darkadv )	/* GX687 (c) 1987 */
	DRIVER( majuu )		/* GX687 (c) 1987 (Japan) */
	DRIVER( vulcan )	/* GX785 (c) 1988 */
	DRIVER( gradius2 )	/* GX785 (c) 1988 (Japan) */
	DRIVER( grdius2a )	/* GX785 (c) 1988 (Japan) */
	DRIVER( grdius2b )	/* GX785 (c) 1988 (Japan) */
	DRIVER( cuebrick )	/* GX903 (c) 1989 */
	DRIVER( fround )	/* GX870 (c) 1988 */
	DRIVER( hpuncher )	/* GX870 (c) 1988 (Japan) */
	DRIVER( miaj )		/* GX808 (c) 1989 (Japan) */

	/* Konami Gradius III board */
	DRIVER( gradius3 )	/* GX945 (c) 1989 (Japan) */
	DRIVER( grdius3a )	/* GX945 (c) 1989 (Asia) */

	/* (some) Konami 68000 games */
	DRIVER( mia )		/* GX808 (c) 1989 */
	DRIVER( mia2 )		/* GX808 (c) 1989 */
	DRIVER( tmnt )		/* GX963 (c) 1989 (US) */
	DRIVER( tmht )		/* GX963 (c) 1989 (UK) */
	DRIVER( tmntj )		/* GX963 (c) 1989 (Japan) */
	DRIVER( tmht2p )	/* GX963 (c) 1989 (UK) */
	DRIVER( tmnt2pj )	/* GX963 (c) 1990 (Japan) */
	DRIVER( tmnt2po )	/* GX963 (c) 1989 (Oceania) */
	DRIVER( punkshot )	/* GX907 (c) 1990 (US) */
	DRIVER( punksht2 )	/* GX907 (c) 1990 (US) */
	DRIVER( lgtnfght )	/* GX939 (c) 1990 (US) */
	DRIVER( trigon )	/* GX939 (c) 1990 (Japan) */
	DRIVER( blswhstl )	/* GX060 (c) 1991 */
	DRIVER( detatwin )	/* GX060 (c) 1991 (Japan) */
TESTDRIVER( glfgreat )	/* GX061 (c) 1991 */
TESTDRIVER( glfgretj )	/* GX061 (c) 1991 (Japan) */
	DRIVER( tmnt2 )		/* GX063 (c) 1991 (US) */
	DRIVER( tmnt22p )	/* GX063 (c) 1991 (US) */
	DRIVER( tmnt2a )	/* GX063 (c) 1991 (Asia) */
	DRIVER( ssriders )	/* GX064 (c) 1991 (World) */
	DRIVER( ssrdrebd )	/* GX064 (c) 1991 (World) */
	DRIVER( ssrdrebc )	/* GX064 (c) 1991 (World) */
	DRIVER( ssrdruda )	/* GX064 (c) 1991 (US) */
	DRIVER( ssrdruac )	/* GX064 (c) 1991 (US) */
	DRIVER( ssrdrubc )	/* GX064 (c) 1991 (US) */
	DRIVER( ssrdrabd )	/* GX064 (c) 1991 (Asia) */
	DRIVER( ssrdrjbd )	/* GX064 (c) 1991 (Japan) */
	DRIVER( xmen )		/* GX065 (c) 1992 (US) */
	DRIVER( xmen6p )	/* GX065 (c) 1992 */
	DRIVER( xmen2pj )	/* GX065 (c) 1992 (Japan) */
	DRIVER( thndrx2 )	/* GX073 (c) 1991 (Japan) */

/*
Konami System GX game list
1994.03 Racing Force (GX250)
1994.03 Golfing Greats 2 (GX218)
1994.04 Gokujou Parodius (GX321)
1994.07 Taisen Puzzle-dama (GX315)
1994.12 Soccer Super Stars (GX427)
1995.04 TwinBee Yahhoo! (GX424)
1995.08 Dragoon Might (GX417)
1995.12 Tokimeki Memorial Taisen Puzzle-dama (GX515)
1996.01 Salamander 2 (GX521)
1996.02 Sexy Parodius (GX533)
1996.03 Daisu-Kiss (GX535)
1996.03 Slam Dunk 2 / Run & Gun 2 (GX505)
1996.10 Taisen Tokkae-dama (GX615)
1996.12 Versus Net Soccer (GX627)
1997.07 Winning Spike (GX705)
1997.11 Rushing Heroes (GX?. Not released in Japan)
*/

	/* Exidy games */
	DRIVER( sidetrac )	/* (c) 1979 */
	DRIVER( targ )		/* (c) 1980 */
	DRIVER( spectar )	/* (c) 1980 */
	DRIVER( spectar1 )	/* (c) 1980 */
	DRIVER( venture )	/* (c) 1981 */
	DRIVER( venture2 )	/* (c) 1981 */
	DRIVER( venture4 )	/* (c) 1981 */
	DRIVER( mtrap )		/* (c) 1981 */
	DRIVER( mtrap3 )	/* (c) 1981 */
	DRIVER( mtrap4 )	/* (c) 1981 */
	DRIVER( pepper2 )	/* (c) 1982 */
	DRIVER( hardhat )	/* (c) 1982 */
	DRIVER( fax )		/* (c) 1983 */
	DRIVER( circus )	/* no copyright notice [1977?] */
	DRIVER( robotbwl )	/* no copyright notice */
	DRIVER( crash )		/* Exidy [1979?] */
	DRIVER( ripcord )	/* Exidy [1977?] */
	DRIVER( starfire )	/* Exidy [1979?] */
	DRIVER( fireone )	/* (c) 1979 Exidy */
	DRIVER( victory )	/* (c) 1982 */
	DRIVER( victorba )	/* (c) 1982 */

	/* Exidy 440 games */
	DRIVER( crossbow )	/* (c) 1983 */
	DRIVER( cheyenne )	/* (c) 1984 */
	DRIVER( combat )	/* (c) 1985 */
	DRIVER( cracksht )	/* (c) 1985 */
	DRIVER( claypign )	/* (c) 1986 */
	DRIVER( chiller )	/* (c) 1986 */
	DRIVER( topsecex )	/* (c) 1986 */
	DRIVER( hitnmiss )	/* (c) 1987 */
	DRIVER( hitnmis2 )	/* (c) 1987 */
	DRIVER( whodunit )	/* (c) 1988 */
	DRIVER( showdown )	/* (c) 1988 */

	/* Atari vector games */
	DRIVER( asteroid )	/* (c) 1979 */
	DRIVER( asteroi1 )	/* no copyright notice */
	DRIVER( asteroib )	/* bootleg */
	DRIVER( astdelux )	/* (c) 1980 */
	DRIVER( astdelu1 )	/* (c) 1980 */
	DRIVER( bwidow )	/* (c) 1982 */
	DRIVER( bzone )		/* (c) 1980 */
	DRIVER( bzone2 )	/* (c) 1980 */
	DRIVER( gravitar )	/* (c) 1982 */
	DRIVER( gravitr2 )	/* (c) 1982 */
	DRIVER( llander )	/* no copyright notice */
	DRIVER( llander1 )	/* no copyright notice */
	DRIVER( redbaron )	/* (c) 1980 */
	DRIVER( spacduel )	/* (c) 1980 */
	DRIVER( tempest )	/* (c) 1980 */
	DRIVER( tempest1 )	/* (c) 1980 */
	DRIVER( tempest2 )	/* (c) 1980 */
	DRIVER( temptube )	/* hack */
	DRIVER( starwars )	/* (c) 1983 */
	DRIVER( starwar1 )	/* (c) 1983 */
	DRIVER( esb )		/* (c) 1985 */
	DRIVER( mhavoc )	/* (c) 1983 */
	DRIVER( mhavoc2 )	/* (c) 1983 */
	DRIVER( mhavocp )	/* (c) 1983 */
	DRIVER( mhavocrv )	/* hack */
	DRIVER( quantum )	/* (c) 1982 */	/* made by Gencomp */
	DRIVER( quantum1 )	/* (c) 1982 */	/* made by Gencomp */
	DRIVER( quantump )	/* (c) 1982 */	/* made by Gencomp */

	/* Atari b/w games */
	DRIVER( sprint1 )	/* no copyright notice */
	DRIVER( sprint2 )	/* no copyright notice */
	DRIVER( sbrkout )	/* no copyright notice */
	DRIVER( dominos )	/* no copyright notice */
	DRIVER( nitedrvr )	/* no copyright notice [1976] */
	DRIVER( bsktball )	/* no copyright notice */
	DRIVER( copsnrob )	/* [1976] */
	DRIVER( avalnche )	/* no copyright notice [1978] */
	DRIVER( subs )		/* no copyright notice [1976] */
	DRIVER( atarifb )	/* no copyright notice [1978] */
	DRIVER( atarifb1 )	/* no copyright notice [1978] */
	DRIVER( atarifb4 )	/* no copyright notice [1979] */
	DRIVER( abaseb )	/* no copyright notice [1979] */
	DRIVER( abaseb2 )	/* no copyright notice [1979] */
	DRIVER( soccer )	/* no copyright notice */
	DRIVER( canyon )	/* no copyright notice [1977] */
	DRIVER( canbprot )	/* no copyright notice [1977] */
	DRIVER( skydiver )	/* no copyright notice [1977] */

	/* Atari "Centipede hardware" games */
	DRIVER( warlord )	/* (c) 1980 */
	DRIVER( centiped )	/* (c) 1980 */
	DRIVER( centipd2 )	/* (c) 1980 */
	DRIVER( centipdb )	/* bootleg */
	DRIVER( centipb2 )	/* bootleg */
	DRIVER( milliped )	/* (c) 1982 */
	DRIVER( qwakprot )	/* (c) 1982 */

	/* "Kangaroo hardware" games */
	DRIVER( fnkyfish )	/* (c) 1981 Sun Electronics */
	DRIVER( kangaroo )	/* (c) 1982 Sun Electronics */
	DRIVER( kangaroa )	/* (c) 1982 Atari */
	DRIVER( kangarob )	/* bootleg */
	DRIVER( arabian )	/* (c) 1983 Sun Electronics */
	DRIVER( arabiana )	/* (c) 1983 Atari */

	/* Atari "Missile Command hardware" games */
	DRIVER( missile )	/* (c) 1980 */
	DRIVER( missile2 )	/* (c) 1980 */
	DRIVER( suprmatk )	/* (c) 1980 + (c) 1981 Gencomp */

	/* misc Atari games */
	DRIVER( foodf )		/* (c) 1982 */	/* made by Gencomp */
	DRIVER( liberatr )	/* (c) 1982 */
TESTDRIVER( liberat2 )
	DRIVER( ccastles )	/* (c) 1983 */
	DRIVER( ccastle2 )	/* (c) 1983 */
	DRIVER( cloak )		/* (c) 1983 */
	DRIVER( cloud9 )	/* (c) 1983 */
	DRIVER( jedi )		/* (c) 1984 */

	/* Atari System 1 games */
	DRIVER( marble )	/* (c) 1984 */
	DRIVER( marble2 )	/* (c) 1984 */
	DRIVER( marblea )	/* (c) 1984 */
	DRIVER( peterpak )	/* (c) 1984 */
	DRIVER( indytemp )	/* (c) 1985 */
	DRIVER( indytem2 )	/* (c) 1985 */
	DRIVER( indytem3 )	/* (c) 1985 */
	DRIVER( indytem4 )	/* (c) 1985 */
	DRIVER( roadrunn )	/* (c) 1985 */
	DRIVER( roadblst )	/* (c) 1986, 1987 */

	/* Atari System 2 games */
	DRIVER( paperboy )	/* (c) 1984 */
	DRIVER( ssprint )	/* (c) 1986 */
	DRIVER( csprint )	/* (c) 1986 */
	DRIVER( 720 )		/* (c) 1986 */
	DRIVER( 720b )		/* (c) 1986 */
	DRIVER( apb )		/* (c) 1987 */
	DRIVER( apb2 )		/* (c) 1987 */

	/* later Atari games */
	DRIVER( gauntlet )	/* (c) 1985 */
	DRIVER( gauntir1 )	/* (c) 1985 */
	DRIVER( gauntir2 )	/* (c) 1985 */
	DRIVER( gaunt2p )	/* (c) 1985 */
	DRIVER( gaunt2 )	/* (c) 1986 */
	DRIVER( vindctr2 )	/* (c) 1988 */
	DRIVER( atetris )	/* (c) 1988 */
	DRIVER( atetrisa )	/* (c) 1988 */
	DRIVER( atetrisb )	/* bootleg */
	DRIVER( atetcktl )	/* (c) 1989 */
	DRIVER( atetckt2 )	/* (c) 1989 */
	DRIVER( toobin )	/* (c) 1988 */
	DRIVER( toobin2 )	/* (c) 1988 */
	DRIVER( toobinp )	/* (c) 1988 */
	DRIVER( vindictr )	/* (c) 1988 */
	DRIVER( klax )		/* (c) 1989 */
	DRIVER( klax2 )		/* (c) 1989 */
	DRIVER( klax3 )		/* (c) 1989 */
	DRIVER( klaxj )		/* (c) 1989 (Japan) */
	DRIVER( blstroid )	/* (c) 1987 */
	DRIVER( blstroi2 )	/* (c) 1987 */
	DRIVER( blsthead )	/* (c) 1987 */
	DRIVER( xybots )	/* (c) 1987 */
	DRIVER( eprom )		/* (c) 1989 */
	DRIVER( eprom2 )	/* (c) 1989 */
	DRIVER( skullxbo )	/* (c) 1989 */
	DRIVER( skullxb2 )	/* (c) 1989 */
	DRIVER( badlands )	/* (c) 1989 */
	DRIVER( cyberbal )	/* (c) 1989 */
	DRIVER( cyberba2 )	/* (c) 1989 */
	DRIVER( cyberbt )	/* (c) 1989 */
	DRIVER( cyberb2p )	/* (c) 1989 */
	DRIVER( rampart )	/* (c) 1990 */
	DRIVER( ramprt2p )	/* (c) 1990 */
	DRIVER( rampartj )	/* (c) 1990 (Japan) */
	DRIVER( shuuz )		/* (c) 1990 */
	DRIVER( shuuz2 )	/* (c) 1990 */
	DRIVER( hydra )		/* (c) 1990 */
	DRIVER( hydrap )	/* (c) 1990 */
	DRIVER( pitfight )	/* (c) 1990 */
	DRIVER( pitfigh3 )	/* (c) 1990 */
	DRIVER( thunderj )	/* (c) 1990 */
	DRIVER( batman )	/* (c) 1991 */
	DRIVER( relief )	/* (c) 1992 */
	DRIVER( relief2 )	/* (c) 1992 */
	DRIVER( offtwall )	/* (c) 1991 */
	DRIVER( offtwalc )	/* (c) 1991 */
	DRIVER( arcadecl )	/* (c) 1992 */
	DRIVER( sparkz )	/* (c) 1992 */
	DRIVER( firetrk )	/* (c) 1978 */

	/* SNK / Rock-ola games */
	DRIVER( sasuke )	/* [1980] Shin Nihon Kikaku (SNK) */
	DRIVER( satansat )	/* (c) 1981 SNK */
	DRIVER( zarzon )	/* (c) 1981 Taito, gameplay says SNK */
	DRIVER( vanguard )	/* (c) 1981 SNK */
	DRIVER( vangrdce )	/* (c) 1981 SNK + Centuri */
	DRIVER( fantasy )	/* (c) 1981 Rock-ola */
	DRIVER( fantasyj )	/* (c) 1981 SNK */
	DRIVER( pballoon )	/* (c) 1982 SNK */
	DRIVER( nibbler )	/* (c) 1982 Rock-ola */
	DRIVER( nibblera )	/* (c) 1982 Rock-ola */

	/* later SNK games, each game can be identified by PCB code and ROM
	code, the ROM code is the same between versions, and usually based
	upon the Japanese title. */
	DRIVER( lasso )		/*       'WM' (c) 1982 */
	DRIVER( joyfulr )	/* A2001      (c) 1983 */
	DRIVER( mnchmobl )	/* A2001      (c) 1983 + Centuri license */
	DRIVER( marvins )	/* A2003      (c) 1983 */
	DRIVER( madcrash )	/* A2005      (c) 1984 */
	DRIVER( vangrd2 )	/*            (c) 1984 */
	DRIVER( hal21 )		/*            (c) 1985 */
	DRIVER( hal21j )	/*            (c) 1985 (Japan) */
	DRIVER( aso )		/*            (c) 1985 */
	DRIVER( tnk3 )		/* A5001      (c) 1985 */
	DRIVER( tnk3j )		/* A5001      (c) 1985 */
	DRIVER( athena )	/*       'UP' (c) 1986 */
	DRIVER( fitegolf )	/*       'GU' (c) 1988 */
	DRIVER( ikari )		/* A5004 'IW' (c) 1986 */
	DRIVER( ikarijp )	/* A5004 'IW' (c) 1986 (Japan) */
	DRIVER( ikarijpb )	/* bootleg */
	DRIVER( victroad )	/*            (c) 1986 */
	DRIVER( dogosoke )	/*            (c) 1986 */
	DRIVER( gwar )		/* A7003 'GV' (c) 1987 */
	DRIVER( gwarj )		/* A7003 'GV' (c) 1987 (Japan) */
	DRIVER( gwara )		/* A7003 'GV' (c) 1987 */
	DRIVER( gwarb )		/* bootleg */
	DRIVER( bermudat )	/* A6003 'WW' (c) 1987 */
	DRIVER( bermudaj )	/* A6003 'WW' (c) 1987 */
	DRIVER( bermudaa )	/* A6003 'WW' (c) 1987 */
	DRIVER( worldwar )	/* A6003 'WW' (c) 1987 */
	DRIVER( psychos )	/*       'PS' (c) 1987 */
	DRIVER( psychosj )	/*       'PS' (c) 1987 (Japan) */
	DRIVER( chopper )	/* A7003 'KK' (c) 1988 */
	DRIVER( legofair )	/* A7003 'KK' (c) 1988 */
	DRIVER( ftsoccer )	/*            (c) 1988 */
	DRIVER( tdfever )	/* A6006 'TD' (c) 1987 */
	DRIVER( tdfeverj )	/* A6006 'TD' (c) 1987 */
	DRIVER( ikari3 )	/* A7007 'IK3'(c) 1989 */
	DRIVER( pow )		/* A7008 'DG' (c) 1988 */
	DRIVER( powj )		/* A7008 'DG' (c) 1988 */
	DRIVER( searchar )	/* A8007 'BH' (c) 1989 */
	DRIVER( sercharu )	/* A8007 'BH' (c) 1989 */
	DRIVER( streetsm )	/* A8007 'S2' (c) 1989 */
	DRIVER( streets1 )	/* A7008 'S2' (c) 1989 */
	DRIVER( streetsj )	/* A8007 'S2' (c) 1989 */
	/* Mechanized Attack   A8002 'MA' (c) 1989 */
	DRIVER( prehisle )	/* A8003 'GT' (c) 1989 */
	DRIVER( prehislu )	/* A8003 'GT' (c) 1989 */
	DRIVER( gensitou )	/* A8003 'GT' (c) 1989 */
	/* Beast Busters       A9003 'BB' (c) 1989 */

	/* SNK / Alpha 68K games */
TESTDRIVER( kouyakyu )
	DRIVER( sstingry )	/* (c) 1986 Alpha Denshi Co. */
	DRIVER( kyros )		/* (c) 1987 World Games */
TESTDRIVER( paddlema )	/* Alpha-68K96I  'PM' (c) 1988 SNK */
	DRIVER( timesold )	/* Alpha-68K96II 'BT' (c) 1987 SNK / Romstar */
	DRIVER( timesol1 )  /* Alpha-68K96II 'BT' (c) 1987 */
	DRIVER( btlfield )  /* Alpha-68K96II 'BT' (c) 1987 */
	DRIVER( skysoldr )	/* Alpha-68K96II 'SS' (c) 1988 SNK (Romstar with dip switch) */
	DRIVER( goldmedl )	/* Alpha-68K96II 'GM' (c) 1988 SNK */
TESTDRIVER( goldmedb )	/* Alpha-68K96II bootleg */
	DRIVER( skyadvnt )	/* Alpha-68K96V  'SA' (c) 1989 SNK of America licensed from Alpha */
	DRIVER( gangwars )	/* Alpha-68K96V       (c) 1989 Alpha Denshi Co. */
	DRIVER( gangwarb )	/* Alpha-68K96V bootleg */
	DRIVER( sbasebal )	/* Alpha-68K96V       (c) 1989 SNK of America licensed from Alpha */

	/* Alpha Denshi games */
	DRIVER( champbas )	/* (c) 1983 Sega */
	DRIVER( champbbj )	/* (c) 1983 Alpha Denshi Co. */
	DRIVER( champbb2 )	/* (c) 1983 Sega */
	DRIVER( exctsccr )	/* (c) 1983 Alpha Denshi Co. */
	DRIVER( exctscca )	/* (c) 1983 Alpha Denshi Co. */
	DRIVER( exctsccb )	/* bootleg */
	DRIVER( exctscc2 )	/* (c) 1984 Alpha Denshi Co. */

	/* Technos games */
	DRIVER( scregg )	/* TA-0001 (c) 1983 */
	DRIVER( eggs )		/* TA-0002 (c) 1983 Universal USA */
	DRIVER( bigprowr )	/* TA-0007 (c) 1983 */
	DRIVER( tagteam )	/* TA-0007 (c) 1983 + Data East license */
	DRIVER( ssozumo )	/* TA-0008 (c) 1984 */
	DRIVER( mystston )	/* TA-0010 (c) 1984 */
	/* TA-0011 Dog Fight (Data East) / Batten O'hara no Sucha-Raka Kuuchuu Sen 1985 */
	DRIVER( bogeyman )	/* X-0204-0 (Data East part number) (c) [1985?] */
	DRIVER( matmania )	/* TA-0015 (c) 1985 + Taito America license */
	DRIVER( excthour )	/* TA-0015 (c) 1985 + Taito license */
	DRIVER( maniach )	/* TA-0017 (c) 1986 + Taito America license */
	DRIVER( maniach2 )	/* TA-0017 (c) 1986 + Taito America license */
	DRIVER( renegade )	/* TA-0018 (c) 1986 + Taito America license */
	DRIVER( kuniokun )	/* TA-0018 (c) 1986 */
	DRIVER( kuniokub )	/* bootleg */
	DRIVER( xsleena )	/* TA-0019 (c) 1986 */
	DRIVER( xsleenab )	/* bootleg */
	DRIVER( solarwar )	/* TA-0019 (c) 1986 Taito + Memetron license */
	DRIVER( battlane )	/* TA-???? (c) 1986 + Taito license */
	DRIVER( battlan2 )	/* TA-???? (c) 1986 + Taito license */
	DRIVER( battlan3 )	/* TA-???? (c) 1986 + Taito license */
	DRIVER( ddragon )	/* TA-0021 (c) 1987 */
	DRIVER( ddragonu )	/* TA-0021 (c) 1987 Taito America */
	DRIVER( ddragonb )	/* bootleg */
	/* TA-0022 Super Dodge Ball */
	/* TA-0023 China Gate */
	/* TA-0024 WWF Superstars */
	/* TA-0025 Champ V'Ball */
	DRIVER( ddragon2 )	/* TA-0026 (c) 1988 */
	DRIVER( ctribe )	/* TA-0028 (c) 1990 (US) */
	DRIVER( ctribeb )	/* bootleg */
	DRIVER( blockout )	/* TA-0029 (c) 1989 + California Dreams */
	DRIVER( blckout2 )	/* TA-0029 (c) 1989 + California Dreams */
	DRIVER( ddragon3 )	/* TA-0030 (c) 1990 */
	DRIVER( ddrago3b )	/* bootleg */
	/* TA-0031 WWF Wrestlefest */

	/* Stern "Berzerk hardware" games */
	DRIVER( berzerk )	/* (c) 1980 */
	DRIVER( berzerk1 )	/* (c) 1980 */
	DRIVER( frenzy )	/* (c) 1982 */

	/* GamePlan games */
	DRIVER( megatack )	/* (c) 1980 Centuri */
	DRIVER( killcom )	/* (c) 1980 Centuri */
	DRIVER( challeng )	/* (c) 1981 Centuri */
	DRIVER( kaos )		/* (c) 1981 */

	/* "stratovox hardware" games */
	DRIVER( route16 )	/* (c) 1981 Tehkan/Sun + Centuri license */
	DRIVER( route16b )	/* bootleg */
	DRIVER( stratvox )	/* Taito */
	DRIVER( stratvxb )	/* bootleg */
	DRIVER( speakres )	/* no copyright notice */
	DRIVER( ttmahjng )	/* Taito */

	/* Zaccaria games */
	DRIVER( monymony )	/* (c) 1983 */
	DRIVER( jackrabt )	/* (c) 1984 */
	DRIVER( jackrab2 )	/* (c) 1984 */
	DRIVER( jackrabs )	/* (c) 1984 */

	/* UPL games */
	DRIVER( nova2001 )	/* UPL-83005 (c) 1983 */
	DRIVER( nov2001u )	/* UPL-83005 (c) [1983] + Universal license */
	DRIVER( pkunwar )	/* [1985?] */
	DRIVER( pkunwarj )	/* [1985?] */
	DRIVER( ninjakd2 )	/* (c) 1987 */
	DRIVER( ninjak2a )	/* (c) 1987 */
	DRIVER( ninjak2b )	/* (c) 1987 */
	DRIVER( rdaction )	/* (c) 1987 + World Games license */
	DRIVER( mnight )	/* (c) 1987 distributed by Kawakus */
	DRIVER( arkarea )	/* UPL-87007 (c) [1988?] */
/*
Urashima Mahjong    UPL-89052

UPL Game List
V1.2   May 27,1999

   83 Mouser                              Kit 2P              Action   83001
 3/84 Nova 2001                 Universal Kit 2P  8W+2B   HC  Shooter  85005
   84 Penguin Wars (Kun)                      2P              Action
   84 Ninja Kun                 Taito                                  85003
   85 Raiders 5                 Taito                                  85004
 8/87 Mission XX                          Kit 2P  8W+2B   VC  Shooter  86001
   87 Mutant Night                        Kit 2P  8W+2B   HC  Action
 7/87 Rad Action/Ninja Taro   World Games Kit 2P  8W+2B   HC  Action   87003
 7/87 Ninja Taro/Rad Action   World Games Kit 2P  8W+2B   HC  Action
   87 Ninja Taro II                       Kit 2P  8W+2B   HC  Action
   88 Aquaria                             Kit 2P  8W+2B
   89 Ochichi Mahjong                     Kit 2P  8W+2B   HC  Mahjong
 9/89 Omega Fighter        American Sammy Kit 2P  8W+2B   HC  Shooter  89016
12/89 Task Force Harrier   American Sammy Kit 2P  8W+2B   VC  Shooter  89053
   90 Atomic Robo-Kid      American Sammy Kit 2P  8W+2B   HC  Shooter  88013
   90 Mustang - U.S.A.A.F./Fire Mustang   Kit 2P  8W+2B   HC  Shooter  90058
   91 Acrobat Mission               Taito Kit 2P  8W+2B   VC  Shooter
   91 Bio Ship Paladin/Spaceship Gomera   Kit 2P  8W+2B   HC  Shooter  90062
   91 Black Heart                         Kit 2P  8W+2B   HC  Shooter
   91 Van Dyke Fantasy                    Kit 2P  8W+2B
 2/92 Strahl                              Kit 2P  8W+3B                91074
      Thunder Dragon 2                                                 93091

*/

	/* Williams/Midway TMS34010 games */
	DRIVER( narc )		/* (c) 1988 Williams */
	DRIVER( narc3 )		/* (c) 1988 Williams */
	DRIVER( trog )		/* (c) 1990 Midway */
	DRIVER( trog3 )		/* (c) 1990 Midway */
	DRIVER( trogp )		/* (c) 1990 Midway */
	DRIVER( smashtv )	/* (c) 1990 Williams */
	DRIVER( smashtv6 )	/* (c) 1990 Williams */
	DRIVER( smashtv5 )	/* (c) 1990 Williams */
	DRIVER( smashtv4 )	/* (c) 1990 Williams */
	DRIVER( hiimpact )	/* (c) 1990 Williams */
	DRIVER( shimpact )	/* (c) 1991 Midway */
	DRIVER( strkforc )	/* (c) 1991 Midway */
	DRIVER( mk )		/* (c) 1992 Midway */
	DRIVER( mkla1 )		/* (c) 1992 Midway */
	DRIVER( mkla2 )		/* (c) 1992 Midway */
	DRIVER( mkla3 )		/* (c) 1992 Midway */
	DRIVER( mkla4 )		/* (c) 1992 Midway */
	DRIVER( term2 )		/* (c) 1992 Midway */
	DRIVER( totcarn )	/* (c) 1992 Midway */
	DRIVER( totcarnp )	/* (c) 1992 Midway */
	DRIVER( mk2 )		/* (c) 1993 Midway */
	DRIVER( mk2r32 )	/* (c) 1993 Midway */
	DRIVER( mk2r14 )	/* (c) 1993 Midway */
	DRIVER( nbajam )	/* (c) 1993 Midway */
	DRIVER( nbajamr2 )	/* (c) 1993 Midway */
	DRIVER( nbajamte )	/* (c) 1994 Midway */
	DRIVER( nbajamt1 )	/* (c) 1994 Midway */
	DRIVER( nbajamt2 )	/* (c) 1994 Midway */
	DRIVER( nbajamt3 )	/* (c) 1994 Midway */
	DRIVER( mk3 )		/* (c) 1994 Midway */
	DRIVER( mk3r20 )	/* (c) 1994 Midway */
	DRIVER( mk3r10 )	/* (c) 1994 Midway */
	DRIVER( umk3 )		/* (c) 1994 Midway */
	DRIVER( umk3r11 )	/* (c) 1994 Midway */
	DRIVER( wwfmania )	/* (c) 1995 Midway */
	DRIVER( openice )	/* (c) 1995 Midway */
	DRIVER( nbamaxht )	/* (c) 1996 Midway */
	DRIVER( rmpgwt )	/* (c) 1997 Midway */
	DRIVER( rmpgwt11 )	/* (c) 1997 Midway */

	/* Cinematronics raster games */
	DRIVER( jack )		/* (c) 1982 Cinematronics */
	DRIVER( jack2 )		/* (c) 1982 Cinematronics */
	DRIVER( jack3 )		/* (c) 1982 Cinematronics */
	DRIVER( treahunt )	/* (c) 1982 Hara Ind. */
	DRIVER( zzyzzyxx )	/* (c) 1982 Cinematronics + Advanced Microcomputer Systems */
	DRIVER( zzyzzyx2 )	/* (c) 1982 Cinematronics + Advanced Microcomputer Systems */
	DRIVER( brix )		/* (c) 1982 Cinematronics + Advanced Microcomputer Systems */
	DRIVER( freeze )	/* Cinematronics */
	DRIVER( sucasino )	/* (c) 1982 Data Amusement */

	/* Cinematronics vector games */
	DRIVER( spacewar )
	DRIVER( barrier )
	DRIVER( starcas )	/* (c) 1980 */
	DRIVER( starcas1 )	/* (c) 1980 */
	DRIVER( tailg )
	DRIVER( ripoff )
	DRIVER( armora )
	DRIVER( wotw )
	DRIVER( warrior )
	DRIVER( starhawk )
	DRIVER( solarq )	/* (c) 1981 */
	DRIVER( boxingb )	/* (c) 1981 */
	DRIVER( speedfrk )
	DRIVER( sundance )
	DRIVER( demon )		/* (c) 1982 Rock-ola */
	/* this one uses 68000+Z80 instead of the Cinematronics CPU */
	DRIVER( cchasm )
	DRIVER( cchasm1 )	/* (c) 1983 Cinematronics / GCE */

	/* "The Pit hardware" games */
	DRIVER( roundup )	/* (c) 1981 Amenip/Centuri */
	DRIVER( fitter )	/* (c) 1981 Taito */
	DRIVER( thepit )	/* (c) 1982 Centuri */
	DRIVER( intrepid )	/* (c) 1983 Nova Games Ltd. */
	DRIVER( intrepi2 )	/* (c) 1983 Nova Games Ltd. */
	DRIVER( portman )	/* (c) 1982 Nova Games Ltd. */
	DRIVER( suprmous )	/* (c) 1982 Taito */
	DRIVER( suprmou2 )	/* (c) 1982 Chu Co. Ltd. */
	DRIVER( machomou )	/* (c) 1982 Techstar */

	/* Valadon Automation games */
	DRIVER( bagman )	/* (c) 1982 */
	DRIVER( bagnard )	/* (c) 1982 */
	DRIVER( bagmans )	/* (c) 1982 + Stern license */
	DRIVER( bagmans2 )	/* (c) 1982 + Stern license */
	DRIVER( sbagman )	/* (c) 1984 */
	DRIVER( sbagmans )	/* (c) 1984 + Stern license */
	DRIVER( pickin )	/* (c) 1983 */

	/* Seibu Denshi / Seibu Kaihatsu games */
	DRIVER( stinger )	/* (c) 1983 Seibu Denshi */
	DRIVER( scion )		/* (c) 1984 Seibu Denshi */
	DRIVER( scionc )	/* (c) 1984 Seibu Denshi + Cinematronics license */
	DRIVER( wiz )		/* (c) 1985 Seibu Kaihatsu */
	DRIVER( wizt )		/* (c) 1985 Taito Corporation */
	DRIVER( empcity )	/* (c) 1986 Seibu Kaihatsu (bootleg?) */
	DRIVER( empcityj )	/* (c) 1986 Taito Corporation (Japan) */
	DRIVER( stfight )	/* (c) 1986 Seibu Kaihatsu (Germany) (bootleg?) */
	DRIVER( dynduke )	/* (c) 1989 Seibu Kaihatsu + Fabtek license */
	DRIVER( dbldyn )	/* (c) 1989 Seibu Kaihatsu + Fabtek license */
	DRIVER( raiden )	/* (c) 1990 Seibu Kaihatsu */
	DRIVER( raidena )	/* (c) 1990 Seibu Kaihatsu */
	DRIVER( raidenk )	/* (c) 1990 Seibu Kaihatsu + IBL Corporation license */
	DRIVER( dcon )		/* (c) 1992 Success */
	DRIVER( kncljoe )	/* (c) 1985 Seibu Kaihatsu + Taito License */
	DRIVER( kncljoea )	/* (c) 1985 Seibu Kaihatsu + Taito License */

/* Seibu STI System games:

	Viper: Phase 1 					(c) 1995
	Viper: Phase 1 (New version)	(c) 1996
	Battle Balls					(c) 1996
	Raiden Fighters					(c) 1996
	Raiden Fighters 2 				(c) 1997
	Senku							(c) 1997

*/

	/* Tad games (Tad games run on Seibu hardware) */
	DRIVER( cabal )		/* (c) 1988 Tad + Fabtek license */
	DRIVER( cabal2 )	/* (c) 1988 Tad + Fabtek license */
	DRIVER( cabalbl )	/* bootleg */
	DRIVER( toki )		/* (c) 1989 Tad */
	DRIVER( toki2 )		/* (c) 1989 Tad */
	DRIVER( toki3 )		/* (c) 1989 Tad */
	DRIVER( tokiu )		/* (c) 1989 Tad + Fabtek license */
	DRIVER( tokib )		/* bootleg */
	DRIVER( bloodbro )	/* (c) 1990 Tad */
	DRIVER( weststry )	/* bootleg */

	/* Jaleco games */
	DRIVER( exerion )	/* (c) 1983 Jaleco */
	DRIVER( exeriont )	/* (c) 1983 Jaleco + Taito America license */
	DRIVER( exerionb )	/* bootleg */
	DRIVER( formatz )	/* (c) 1984 Jaleco */
	DRIVER( aeroboto )	/* (c) 1984 Williams */
	DRIVER( citycon )	/* (c) 1985 Jaleco */
	DRIVER( citycona )	/* (c) 1985 Jaleco */
	DRIVER( cruisin )	/* (c) 1985 Jaleco/Kitkorp */
	DRIVER( pinbo )		/* (c) 1984 Jaleco */
	DRIVER( pinbos )	/* (c) 1985 Strike */
	DRIVER( psychic5 )	/* (c) 1987 Jaleco */
	DRIVER( ginganin )	/* (c) 1987 Jaleco */
	DRIVER( cischeat )	/* (c) 1990 Jaleco */
	DRIVER( f1gpstar )	/* (c) 1991 Jaleco */
	DRIVER( skyfox )	/* (c) 1987 Jaleco + Nichibutsu USA license */
	DRIVER( exerizrb )	/* bootleg */
	DRIVER( argus )         /* (c) 1986 Jaleco */
	DRIVER( valtric )       /* (c) 1986 Jaleco */
	DRIVER( butasan )       /* (c) 1987 Jaleco */
	DRIVER( momoko )        /* (c) 1986 Jaleco */

	/* Jaleco Mega System 1 games */
	DRIVER( lomakai )	/* (c) 1988 (World) */
	DRIVER( makaiden )	/* (c) 1988 (Japan) */
	DRIVER( p47 )		/* (c) 1988 */
	DRIVER( p47j )		/* (c) 1988 (Japan) */
	DRIVER( kickoff )	/* (c) 1988 (Japan) */
	DRIVER( tshingen )	/* (c) 1988 (Japan) */
	DRIVER( astyanax )	/* (c) 1989 */
	DRIVER( lordofk )	/* (c) 1989 (Japan) */
	DRIVER( hachoo )	/* (c) 1989 */
	DRIVER( plusalph )	/* (c) 1989 */
	DRIVER( stdragon )	/* (c) 1989 */
	DRIVER( iganinju )	/* (c) 1989 (Japan) */
	DRIVER( rodland )	/* (c) 1990 */
	DRIVER( rodlandj )	/* (c) 1990 (Japan) */
	DRIVER( 64street )	/* (c) 1991 */
	DRIVER( 64streej )	/* (c) 1991 (Japan) */
	DRIVER( edf )		/* (c) 1991 */
	DRIVER( avspirit )	/* (c) 1991 */
	DRIVER( phantasm )	/* (c) 1991 (Japan) */
	DRIVER( bigstrik )	/* (c) 1992 */
	DRIVER( chimerab )	/* (c) 1993 */
	DRIVER( cybattlr )	/* (c) 1993 */
	DRIVER( peekaboo )	/* (c) 1993 */
	DRIVER( soldamj )	/* (c) 1992 (Japan) */

	/* Video System Co. games */
	DRIVER( rabiolep )	/* (c) 1987 V-System Co. (Japan) */
	DRIVER( rpunch )	/* (c) 1987 V-System Co. + Bally/Midway/Sente license (US) */
	DRIVER( svolley )	/* (c) 1989 V-System Co. (Japan) */
	DRIVER( svolleyk )	/* (c) 1989 V-System Co. (Japan) */
	DRIVER( tail2nos )	/* [1989] V-System Co. */
	DRIVER( sformula )	/* [1989] V-System Co. (Japan) */
	DRIVER( pipedrm )	/* (c) 1990 Video System Co. (Japan) */
	DRIVER( hatris )	/* (c) 1990 Video System Co. (Japan) */
	DRIVER( pspikes )	/* (c) 1991 Video System Co. (Korea) */
	DRIVER( svolly91 )	/* (c) 1991 Video System Co. */
	DRIVER( karatblz )	/* (c) 1991 Video System Co. */
	DRIVER( karatblu )	/* (c) 1991 Video System Co. (US) */
	DRIVER( spinlbrk )	/* (c) 1990 V-System Co. (World) */
	DRIVER( spinlbru )	/* (c) 1990 V-System Co. (US) */
	DRIVER( spinlbrj )	/* (c) 1990 V-System Co. (Japan) */
	DRIVER( turbofrc )	/* (c) 1991 Video System Co. */
	DRIVER( aerofgt )	/* (c) 1992 Video System Co. */
	DRIVER( aerofgtb )	/* (c) 1992 Video System Co. */
	DRIVER( aerofgtc )	/* (c) 1992 Video System Co. */
	DRIVER( sonicwi )	/* (c) 1992 Video System Co. (Japan) */

	/* Psikyo games */
	DRIVER( sngkace )	/* (c) 1993 */
	DRIVER( gunbird )	/* (c) 1994 */
TESTDRIVER( s1945 )		/* (c) 1995 */
TESTDRIVER( sngkblad )	/* (c) 1996 */

	/* Orca games */
	DRIVER( marineb )	/* (c) 1982 Orca */
	DRIVER( changes )	/* (c) 1982 Orca */
	DRIVER( looper )	/* (c) 1982 Orca */
	DRIVER( springer )	/* (c) 1982 Orca */
	DRIVER( hoccer )	/* (c) 1983 Eastern Micro Electronics, Inc. */
	DRIVER( hoccer2 )	/* (c) 1983 Eastern Micro Electronics, Inc. */
	DRIVER( hopprobo )	/* (c) 1983 Sega */
	DRIVER( wanted )	/* (c) 1984 Sigma Ent. Inc. */
	DRIVER( funkybee )	/* (c) 1982 Orca */
	DRIVER( skylancr )	/* (c) 1983 Orca + Esco Trading Co license */
	DRIVER( zodiack )	/* (c) 1983 Orca + Esco Trading Co license */
	DRIVER( dogfight )	/* (c) 1983 Thunderbolt */
	DRIVER( moguchan )	/* (c) 1982 Orca + Eastern Commerce Inc. license (doesn't appear on screen) */
	DRIVER( percuss )	/* (c) 1981 Orca */
TESTDRIVER( bounty )
	DRIVER( espial )	/* (c) 1983 Thunderbolt, Orca logo is hidden in title screen */
	DRIVER( espiale )	/* (c) 1983 Thunderbolt, Orca logo is hidden in title screen */
	/* Vastar was made by Orca, but when it was finished, Orca had already bankrupted. */
	/* So they sold this game as "Made by Sesame Japan" because they couldn't use */
	/* the name "Orca" */
	DRIVER( vastar )	/* (c) 1983 Sesame Japan */
	DRIVER( vastar2 )	/* (c) 1983 Sesame Japan */
/*
   other Orca games:
   82 Battle Cross                         Kit 2P
   82 River Patrol Empire Mfg/Kerstens Ind Ded 2P        HC Action
   82 Slalom                               Kit 2P        HC Action
   83 Net Wars                                 2P
   83 Super Crush                          Kit 2P           Action
*/

	/* Gaelco games */
	DRIVER( bigkarnk )	/* (c) 1991 Gaelco */
	DRIVER( splash )	/* (c) 1992 Gaelco */
	DRIVER( biomtoy )	/* (c) 1995 Gaelco */
TESTDRIVER( maniacsq )	/* (c) 1996 Gaelco */
/*
Gaelco Game list:
=================

1987:	Master Boy
1991:	Big Karnak, Master Boy 2
1992:	Splash, Thunder Hoop, Squash
1993:	World Rally, Glass
1994:	Strike Back, Target Hits, Thunder Hoop 2
1995:	Alligator Hunt, Toy, World Rally 2, Salter, Touch & Go
1996:	Maniac Square, Snow Board, Speed Up
1997:	Surf Planet
1998:	Radikal Bikers
1999:	Rolling Extreme

All games newer than Splash are heavily protected.
*/

	/* Kaneko "AX System" games */
	DRIVER( berlwall )	/* (c) 1991 Kaneko */
	DRIVER( berlwalt )	/* (c) 1991 Kaneko */
	DRIVER( gtmr )		/* (c) 1994 Kaneko */
	DRIVER( gtmre )		/* (c) 1994 Kaneko */
TESTDRIVER( gtmr2 )
TESTDRIVER( shogwarr )

	/* other Kaneko games */
	DRIVER( galpanic )	/* (c) 1990 Kaneko */
	DRIVER( airbustr )	/* (c) 1990 Kaneko */

	/* Seta games */
	DRIVER( hanaawas )	/* (c) SetaKikaku */
	DRIVER( tndrcade )	/* UA-0 (c) 1987 Taito */
	DRIVER( tndrcadj )	/* UA-0 (c) 1987 Taito */
	DRIVER( twineagl )	/* UA-2 (c) 1988 + Taito license */
	DRIVER( downtown )	/* UD-2 (c) 1989 + Romstar or Taito license (DSW) */
	DRIVER( usclssic )	/* UE   (c) 1989 + Romstar or Taito license (DSW) */
	DRIVER( calibr50 )	/* UH   (c) 1989 + Romstar or Taito license (DSW) */
	DRIVER( drgnunit )	/* (c) 1989 Athena / Seta + Romstar or Taito license (DSW) */
	DRIVER( arbalest )	/* UK   (c) 1989 + Jordan, Romstar or Taito license (DSW) */
	DRIVER( metafox )	/* UP   (c) 1989 + Jordan, Romstar or Taito license (DSW) */
	DRIVER( blandia )	/* (c) 1992 Allumer */
	DRIVER( zingzip )	/* UY   (c) 1992 Allumer + Tecmo */
TESTDRIVER( msgundam )
	DRIVER( wrofaero )	/* (c) 1993 Yang Cheng */

	/* Atlus games */
	DRIVER( ohmygod )	/* (c) 1993 Atlus (Japan) */
	DRIVER( powerins )	/* (c) 1993 Atlus (Japan) */

	/* SunSoft games */
	DRIVER( shanghai )	/* (c) 1988 Sunsoft (Sun Electronics) */
	DRIVER( shangha3 )	/* (c) 1993 Sunsoft */
	DRIVER( heberpop )	/* (c) 1994 Sunsoft / Atlus */
	DRIVER( blocken )	/* (c) 1994 KID / Visco */

	/* Suna games */
	DRIVER( rranger )	/* (c) 1988 SunA + Sharp Image license */
TESTDRIVER( sranger )	/* (c) 1988 SunA */
TESTDRIVER( srangerb )	/* bootleg */
TESTDRIVER( srangerw )
	DRIVER( hardhead )	/* (c) 1988 SunA */
	DRIVER( hardhedb )	/* bootleg */
TESTDRIVER( starfigh )
TESTDRIVER( hardhea2 )
TESTDRIVER( brickzn )
TESTDRIVER( brickzn3 )

	/* Dooyong games */
	DRIVER( gundealr )	/* (c) 1990 Dooyong */
	DRIVER( gundeala )	/* (c) Dooyong */
	DRIVER( yamyam )	/* (c) 1990 Dooyong */
	DRIVER( wiseguy )	/* (c) 1990 Dooyong */

	/* NMK games */
TESTDRIVER( macross )	/* (c) 1992 NMK + Big West */
	DRIVER( bjtwin )	/* (c) 1993 NMK */

	DRIVER( spacefb )	/* (c) [1980?] Nintendo */
	DRIVER( spacefbg )	/* 834-0031 (c) 1980 Gremlin */
	DRIVER( spacefbb )	/* bootleg */
	DRIVER( spacebrd )	/* bootleg */
	DRIVER( spacedem )	/* (c) 1980 Nintendo / Fortrek */
	DRIVER( blueprnt )	/* (c) 1982 Bally Midway (Zilec in ROM 3U, and the programmer names) */
	DRIVER( blueprnj )	/* (c) 1982 Jaleco (Zilec in ROM 3U, and the programmer names) */
	DRIVER( saturn )	/* (c) 1983 Jaleco (Zilec in ROM R6, and the programmer names) */
	DRIVER( omegrace )	/* (c) 1981 Midway */
	DRIVER( dday )		/* (c) 1982 Olympia */
	DRIVER( ddayc )		/* (c) 1982 Olympia + Centuri license */
	DRIVER( leprechn )	/* (c) 1982 Tong Electronic */
	DRIVER( potogold )	/* (c) 1982 Tong Electronic */
	DRIVER( hexa )		/* D. R. Korea */
	DRIVER( irobot )	/* (c) 1983 Atari */
	DRIVER( spiders )	/* (c) 1981 Sigma Ent. Inc. */
	DRIVER( spiders2 )	/* (c) 1981 Sigma Ent. Inc. */
	DRIVER( stactics )	/* [1981 Sega] */
	DRIVER( exterm )	/* (c) 1989 Premier Technology - a Gottlieb game */
	DRIVER( sharkatt )	/* (c) Pacific Novelty */
	DRIVER( kingofb )	/* (c) 1985 Woodplace Inc. */
	DRIVER( ringking )	/* (c) 1985 Data East USA */
	DRIVER( ringkin2 )
	DRIVER( ringkin3 )	/* (c) 1985 Data East USA */
	DRIVER( zerozone )	/* (c) 1993 Comad */
	DRIVER( speedbal )	/* (c) 1987 Tecfri */
	DRIVER( sauro )		/* (c) 1987 Tecfri */
	DRIVER( ambush )	/* (c) 1983 Nippon Amuse Co-Ltd */
	DRIVER( starcrus )	/* [1977 Ramtek] */
	DRIVER( goindol )	/* (c) 1987 Sun a Electronics */
	DRIVER( homo )		/* bootleg */
TESTDRIVER( dlair )
	DRIVER( meteor )	/* (c) 1981 Venture Line */
	DRIVER( aztarac )	/* (c) 1983 Centuri (vector game) */
	DRIVER( mole )		/* (c) 1982 Yachiyo Electronics, Ltd. */
	DRIVER( gotya )		/* (c) 1981 Game-A-Tron */
	DRIVER( mrjong )	/* (c) 1983 Kiwako */
	DRIVER( crazyblk )	/* (c) 1983 Kiwako + ECI license */
	DRIVER( polyplay )
	DRIVER( mermaid )	/* (c) 1982 Rock-ola */
	DRIVER( magix )		/* (c) 1995 Yun Sung */
	DRIVER( royalmah )	/* (c) 1982 Falcon */

	/* Neo Geo games */
	/* the four digits number is the game ID stored at address 0x0108 of the program ROM */
	DRIVER( nam1975 )	/* 0001 (c) 1990 SNK */
	DRIVER( bstars )	/* 0002 (c) 1990 SNK */
	DRIVER( tpgolf )	/* 0003 (c) 1990 SNK */
	DRIVER( mahretsu )	/* 0004 (c) 1990 SNK */
	DRIVER( maglord )	/* 0005 (c) 1990 Alpha Denshi Co. */
	DRIVER( maglordh )	/* 0005 (c) 1990 Alpha Denshi Co. */
	DRIVER( ridhero )	/* 0006 (c) 1990 SNK */
	DRIVER( alpham2 )	/* 0007 (c) 1991 SNK */
	/* 0008 */
	DRIVER( ncombat )	/* 0009 (c) 1990 Alpha Denshi Co. */
	DRIVER( cyberlip )	/* 0010 (c) 1990 SNK */
	DRIVER( superspy )	/* 0011 (c) 1990 SNK */
	/* 0012 */
	/* 0013 */
	DRIVER( mutnat )	/* 0014 (c) 1992 SNK */
	/* 0015 */
	DRIVER( kotm )		/* 0016 (c) 1991 SNK */
	DRIVER( sengoku )	/* 0017 (c) 1991 SNK */
	DRIVER( sengokh )	/* 0017 (c) 1991 SNK */
	DRIVER( burningf )	/* 0018 (c) 1991 SNK */
	DRIVER( burningh )	/* 0018 (c) 1991 SNK */
	DRIVER( lbowling )	/* 0019 (c) 1990 SNK */
	DRIVER( gpilots )	/* 0020 (c) 1991 SNK */
	DRIVER( joyjoy )	/* 0021 (c) 1990 SNK */
	DRIVER( bjourney )	/* 0022 (c) 1990 Alpha Denshi Co. */
	DRIVER( quizdais )	/* 0023 (c) 1991 SNK */
	DRIVER( lresort )	/* 0024 (c) 1992 SNK */
	DRIVER( eightman )	/* 0025 (c) 1991 SNK / Pallas */
	/* 0026 Fun Fun Brothers - prototype? */
	DRIVER( minasan )	/* 0027 (c) 1990 Monolith Corp. */
	/* 0028 */
	DRIVER( legendos )	/* 0029 (c) 1991 SNK */
	DRIVER( 2020bb )	/* 0030 (c) 1991 SNK / Pallas */
	DRIVER( 2020bbh )	/* 0030 (c) 1991 SNK / Pallas */
	DRIVER( socbrawl )	/* 0031 (c) 1991 SNK */
	DRIVER( roboarmy )	/* 0032 (c) 1991 SNK */
	DRIVER( fatfury1 )	/* 0033 (c) 1991 SNK */
	DRIVER( fbfrenzy )	/* 0034 (c) 1992 SNK */
	/* 0035 */
	DRIVER( bakatono )	/* 0036 (c) 1991 Monolith Corp. */
	DRIVER( crsword )	/* 0037 (c) 1991 Alpha Denshi Co. */
	DRIVER( trally )	/* 0038 (c) 1991 Alpha Denshi Co. */
	DRIVER( kotm2 )		/* 0039 (c) 1992 SNK */
	DRIVER( sengoku2 )	/* 0040 (c) 1993 SNK */
	DRIVER( bstars2 )	/* 0041 (c) 1992 SNK */
	DRIVER( quizdai2 )	/* 0042 (c) 1992 SNK */
	DRIVER( 3countb )	/* 0043 (c) 1993 SNK */
	DRIVER( aof )		/* 0044 (c) 1992 SNK */
	DRIVER( samsho )	/* 0045 (c) 1993 SNK */
	DRIVER( tophuntr )	/* 0046 (c) 1994 SNK */
	DRIVER( fatfury2 )	/* 0047 (c) 1992 SNK */
	DRIVER( janshin )	/* 0048 (c) 1994 Aicom */
	DRIVER( androdun )	/* 0049 (c) 1992 Visco */
	DRIVER( ncommand )	/* 0050 (c) 1992 Alpha Denshi Co. */
	DRIVER( viewpoin )	/* 0051 (c) 1992 Sammy */
	DRIVER( ssideki )	/* 0052 (c) 1992 SNK */
	DRIVER( wh1 )		/* 0053 (c) 1992 Alpha Denshi Co. */
	/* 0054 Crossed Swords 2 (CD only) */
	DRIVER( kof94 )		/* 0055 (c) 1994 SNK */
	DRIVER( aof2 )		/* 0056 (c) 1994 SNK */
	DRIVER( wh2 )		/* 0057 (c) 1993 ADK */
	DRIVER( fatfursp )	/* 0058 (c) 1993 SNK */
	DRIVER( savagere )	/* 0059 (c) 1995 SNK */
	DRIVER( fightfev )	/* 0060 (c) 1994 Viccom */
	DRIVER( ssideki2 )	/* 0061 (c) 1994 SNK */
	DRIVER( spinmast )	/* 0062 (c) 1993 Data East Corporation */
	DRIVER( samsho2 )	/* 0063 (c) 1994 SNK */
	DRIVER( wh2j )		/* 0064 (c) 1994 ADK / SNK */
	DRIVER( wjammers )	/* 0065 (c) 1994 Data East Corporation */
	DRIVER( karnovr )	/* 0066 (c) 1994 Data East Corporation */
	DRIVER( gururin )	/* 0067 (c) 1994 Face */
	DRIVER( pspikes2 )	/* 0068 (c) 1994 Video System Co. */
	DRIVER( fatfury3 )	/* 0069 (c) 1995 SNK */
	/* 0070 */
	/* 0071 */
	/* 0072 */
	DRIVER( panicbom )	/* 0073 (c) 1994 Eighting / Hudson */
	DRIVER( aodk )		/* 0074 (c) 1994 ADK / SNK */
	DRIVER( sonicwi2 )	/* 0075 (c) 1994 Video System Co. */
	DRIVER( zedblade )	/* 0076 (c) 1994 NMK */
	/* 0077 */
	DRIVER( galaxyfg )	/* 0078 (c) 1995 Sunsoft */
	DRIVER( strhoop )	/* 0079 (c) 1994 Data East Corporation */
	DRIVER( quizkof )	/* 0080 (c) 1995 Saurus */
	DRIVER( ssideki3 )	/* 0081 (c) 1995 SNK */
	DRIVER( doubledr )	/* 0082 (c) 1995 Technos */
	DRIVER( pbobble )	/* 0083 (c) 1994 Taito */
	DRIVER( kof95 )		/* 0084 (c) 1995 SNK */
	/* 0085 Shinsetsu Samurai Spirits Bushidoretsuden / Samurai Shodown RPG (CD only) */
	DRIVER( tws96 )		/* 0086 (c) 1996 Tecmo */
	DRIVER( samsho3 )	/* 0087 (c) 1995 SNK */
	DRIVER( stakwin )	/* 0088 (c) 1995 Saurus */
	DRIVER( pulstar )	/* 0089 (c) 1995 Aicom */
	DRIVER( whp )		/* 0090 (c) 1995 ADK / SNK */
	/* 0091 */
	DRIVER( kabukikl )	/* 0092 (c) 1995 Hudson */
	DRIVER( neobombe )	/* 0093 (c) 1997 Hudson */
	DRIVER( gowcaizr )	/* 0094 (c) 1995 Technos */
	DRIVER( rbff1 )		/* 0095 (c) 1995 SNK */
	DRIVER( aof3 )		/* 0096 (c) 1996 SNK */
	DRIVER( sonicwi3 )	/* 0097 (c) 1995 Video System Co. */
	/* 0098 Idol Mahjong - final romance 2 (CD only? not confirmed, MVS might exist) */
	/* 0099 */
	DRIVER( turfmast )	/* 0200 (c) 1996 Nazca */
	DRIVER( mslug )		/* 0201 (c) 1996 Nazca */
	DRIVER( puzzledp )	/* 0202 (c) 1995 Taito (Visco license) */
	DRIVER( mosyougi )	/* 0203 (c) 1995 ADK / SNK */
	/* 0204 ADK World (CD only) */
	/* 0205 Neo-Geo CD Special (CD only) */
	DRIVER( marukodq )	/* 0206 (c) 1995 Takara */
	DRIVER( neomrdo )	/* 0207 (c) 1996 Visco */
	DRIVER( sdodgeb )	/* 0208 (c) 1996 Technos */
	DRIVER( goalx3 )	/* 0209 (c) 1995 Visco */
	/* 0210 */
	/* 0211 Oshidashi Zintrick (CD only? not confirmed, MVS might exist) */
	DRIVER( overtop )	/* 0212 (c) 1996 ADK */
	DRIVER( neodrift )	/* 0213 (c) 1996 Visco */
	DRIVER( kof96 )		/* 0214 (c) 1996 SNK */
	DRIVER( ssideki4 )	/* 0215 (c) 1996 SNK */
	DRIVER( kizuna )	/* 0216 (c) 1996 SNK */
	DRIVER( ninjamas )	/* 0217 (c) 1996 ADK / SNK */
	DRIVER( ragnagrd )	/* 0218 (c) 1996 Saurus */
	DRIVER( pgoal )		/* 0219 (c) 1996 Saurus */
	/* 0220 Choutetsu Brikin'ger - iron clad (MVS existance seems to have been confirmed) */
	DRIVER( magdrop2 )	/* 0221 (c) 1996 Data East Corporation */
	DRIVER( samsho4 )	/* 0222 (c) 1996 SNK */
	DRIVER( rbffspec )	/* 0223 (c) 1996 SNK */
	DRIVER( twinspri )	/* 0224 (c) 1996 ADK */
	DRIVER( wakuwak7 )	/* 0225 (c) 1996 Sunsoft */
	/* 0226 */
	DRIVER( stakwin2 )	/* 0227 (c) 1996 Saurus */
	/* 0228 */
	/* 0229 King of Fighters '96 CD Collection (CD only) */
	DRIVER( breakers )	/* 0230 (c) 1996 Visco */
	DRIVER( miexchng )	/* 0231 (c) 1997 Face */
	DRIVER( kof97 )		/* 0232 (c) 1997 SNK */
	DRIVER( magdrop3 )	/* 0233 (c) 1997 Data East Corporation */
	DRIVER( lastblad )	/* 0234 (c) 1997 SNK */
	DRIVER( puzzldpr )	/* 0235 (c) 1997 Taito (Visco license) */
	DRIVER( irrmaze )	/* 0236 (c) 1997 SNK / Saurus */
	DRIVER( popbounc )	/* 0237 (c) 1997 Video System Co. */
	DRIVER( shocktro )	/* 0238 (c) 1997 Saurus */
	DRIVER( blazstar )	/* 0239 (c) 1998 Yumekobo */
	DRIVER( rbff2 )		/* 0240 (c) 1998 SNK */
	DRIVER( mslug2 )	/* 0241 (c) 1998 SNK */
	DRIVER( kof98 )		/* 0242 (c) 1998 SNK */
	DRIVER( lastbld2 )	/* 0243 (c) 1998 SNK */
	DRIVER( neocup98 )	/* 0244 (c) 1998 SNK */
	DRIVER( breakrev )	/* 0245 (c) 1998 Visco */
	DRIVER( shocktr2 )	/* 0246 (c) 1998 Saurus */
	DRIVER( flipshot )	/* 0247 (c) 1998 Visco */
TESTDRIVER( pbobbl2n )	/* 0248 (c) 1999 Taito (SNK license) */
TESTDRIVER( ctomaday )	/* 0249 (c) 1999 Visco */
TESTDRIVER( mslugx )	/* 0250 (c) 1999 SNK */
TESTDRIVER( kof99 )		/* 0251 (c) 1999 SNK */
TESTDRIVER( garou )		/* 0253 (c) 1999 SNK */
	/* Prehistoric Isle 2 */
	/* Strikers 1945 Plus */
	/* Ganryu */

#endif	/* DRIVER_RECURSIVE */

#endif	/* TINY_COMPILE */
