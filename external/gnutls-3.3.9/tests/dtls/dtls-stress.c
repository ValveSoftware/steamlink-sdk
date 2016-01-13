/*
 * Copyright (C) 2012 Sean Buckheister
 *
 * This file is part of GnuTLS.
 *
 * GnuTLS is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * GnuTLS is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GnuTLS; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

/*
 * DTLS stress test utility
 *
 * **** Available parameters ****
 *
 *	-nb                 enable nonblocking operations on sessions
 *	-batch              read test identifiers from stdin and run them
 *	-d                  increase debug level by one
 *	-r                  replay messages (very crude replay mechanism)
 *	-d <n>              set debug level to <n>
 *	-die                don't start new tests after the first detected failure
 *	-timeout <n>        set handshake timeout to <n> seconds. Tests that don't make progress
 *	                    within twice this time will be forcibly killed. (default: 120)
 *	-retransmit <n>     set retransmit timeout to <n> milliseconds (default: 100)
 *	-j <n>              run up to <n> tests in parallel
 *	-full               use full handshake with mutual certificate authentication
 *	-shello <perm>      run only one test, with the server hello flight permuted as <perm>
 *	-sfinished <perm>   run only one test, with the server finished flight permuted as <perm>
 *	-cfinished <perm>   run only one test, with the client finished flight permuted as <perm>
 *	<packet name>       run only one test, drop <packet name> three times
 *	                    valid values for <packet name> are:
 *	                        SHello, SCertificate, SKeyExchange, SCertificateRequest, SHelloDone,
 *	                        CCertificate, CKeyExchange, CCertificateVerify, CChangeCipherSpec,
 *	                        CFinished, SChangeCipherSpec, SFinished
 *	                    using *Certificate* without -full will yield unexpected results
 *
 * 
 * **** Permutation handling ****
 *
 * Flight length for -sfinished is 2, for -shello and -cfinished they are 5 with -full, 3 otherwise.
 * Permutations are given with base 0 and specify the order in which reordered packets are transmitted.
 * For example, -full -shello 42130 will transmit server hello flight packets in the order
 * SHelloDone, SKeyExchange, SCertificate, SCertificateRequest, SHello
 *
 *
 * **** Output format ****
 *
 * Every line printed for any given test is prefixed by a unique id for that test. See run_test_by_id for
 * exact composition. Errors encountered during execution are printed, with one status line after test
 * completen. The format for this line is as follows:
 *
 * <id> <status> SHello(<shperm>), SFinished(<sfinperm>), CFinished(<cfinperm>) :- <drops>
 *
 * The format for error lines is <id> <role>| <text>, with <role> being the role of the child process
 * that encountered the error, and <text> being obvious.
 *
 * <id> is the unique id for the test, it can be used as input to -batch.
 * <status> can be ++ for a successful test, -- for a failure, TT for a deadlock timeout killed test,
 *     or !! for a test has died due to some unforeseen circumstances like syscall failures.
 * <shperm>, <sfinperm>, <cfinperm> show the permutation for the respective flights used.
 *    They can be used as input to -shello, -sfinished, and -cfinished, respectively.
 * <drops> is a comma separated list of <packet name>, one for every packet dropped thrice
 *
 *
 * **** Exit status ****
 *
 * 0    all tests have passed
 * 1    some tests have failed
 * 4    the master processed has encountered unexpected errors
 * 8    error parsing command line
 */

#include <config.h>
#include <gnutls/gnutls.h>
#include <gnutls/openpgp.h>
#include <gnutls/dtls.h>
#include <unistd.h>
#include "../utils.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <time.h>
#include <sys/wait.h>

#if _POSIX_TIMERS && (_POSIX_TIMERS - 200112L) >= 0

// {{{ types

typedef struct {
	int count;
} filter_packet_state_t;

typedef struct {
	gnutls_datum_t packets[5];
	int *order;
	int count;
} filter_permute_state_t;

typedef void (*filter_fn) (gnutls_transport_ptr_t, const unsigned char *,
			   size_t);

typedef int (*match_fn) (const unsigned char *, size_t);

enum role { SERVER, CLIENT };

// }}}

// {{{ static data

static int permutations2[2][2]
= { {0, 1}, {1, 0} };

static const char *permutation_names2[]
= { "01", "10", 0 };

static int permutations3[6][3]
= { {0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0} };

static const char *permutation_names3[]
= { "012", "021", "102", "120", "201", "210", 0 };

static int permutations5[120][5]
= { {0, 1, 2, 3, 4}, {0, 2, 1, 3, 4}, {1, 0, 2, 3, 4}, {1, 2, 0, 3, 4}, {2,
									 0,
									 1,
									 3,
									 4},
    {2, 1, 0, 3, 4},
{0, 1, 3, 2, 4}, {0, 2, 3, 1, 4}, {1, 0, 3, 2, 4}, {1, 2, 3, 0, 4}, {2, 0,
								     3, 1,
								     4},
    {2, 1, 3, 0, 4},
{0, 3, 1, 2, 4}, {0, 3, 2, 1, 4}, {1, 3, 0, 2, 4}, {1, 3, 2, 0, 4}, {2, 3,
								     0, 1,
								     4},
    {2, 3, 1, 0, 4},
{3, 0, 1, 2, 4}, {3, 0, 2, 1, 4}, {3, 1, 0, 2, 4}, {3, 1, 2, 0, 4}, {3, 2,
								     0, 1,
								     4},
    {3, 2, 1, 0, 4},
{0, 1, 2, 4, 3}, {0, 2, 1, 4, 3}, {1, 0, 2, 4, 3}, {1, 2, 0, 4, 3}, {2, 0,
								     1, 4,
								     3},
    {2, 1, 0, 4, 3},
{0, 1, 3, 4, 2}, {0, 2, 3, 4, 1}, {1, 0, 3, 4, 2}, {1, 2, 3, 4, 0}, {2, 0,
								     3, 4,
								     1},
    {2, 1, 3, 4, 0},
{0, 3, 1, 4, 2}, {0, 3, 2, 4, 1}, {1, 3, 0, 4, 2}, {1, 3, 2, 4, 0}, {2, 3,
								     0, 4,
								     1},
    {2, 3, 1, 4, 0},
{3, 0, 1, 4, 2}, {3, 0, 2, 4, 1}, {3, 1, 0, 4, 2}, {3, 1, 2, 4, 0}, {3, 2,
								     0, 4,
								     1},
    {3, 2, 1, 4, 0},
{0, 1, 4, 2, 3}, {0, 2, 4, 1, 3}, {1, 0, 4, 2, 3}, {1, 2, 4, 0, 3}, {2, 0,
								     4, 1,
								     3},
    {2, 1, 4, 0, 3},
{0, 1, 4, 3, 2}, {0, 2, 4, 3, 1}, {1, 0, 4, 3, 2}, {1, 2, 4, 3, 0}, {2, 0,
								     4, 3,
								     1},
    {2, 1, 4, 3, 0},
{0, 3, 4, 1, 2}, {0, 3, 4, 2, 1}, {1, 3, 4, 0, 2}, {1, 3, 4, 2, 0}, {2, 3,
								     4, 0,
								     1},
    {2, 3, 4, 1, 0},
{3, 0, 4, 1, 2}, {3, 0, 4, 2, 1}, {3, 1, 4, 0, 2}, {3, 1, 4, 2, 0}, {3, 2,
								     4, 0,
								     1},
    {3, 2, 4, 1, 0},
{0, 4, 1, 2, 3}, {0, 4, 2, 1, 3}, {1, 4, 0, 2, 3}, {1, 4, 2, 0, 3}, {2, 4,
								     0, 1,
								     3},
    {2, 4, 1, 0, 3},
{0, 4, 1, 3, 2}, {0, 4, 2, 3, 1}, {1, 4, 0, 3, 2}, {1, 4, 2, 3, 0}, {2, 4,
								     0, 3,
								     1},
    {2, 4, 1, 3, 0},
{0, 4, 3, 1, 2}, {0, 4, 3, 2, 1}, {1, 4, 3, 0, 2}, {1, 4, 3, 2, 0}, {2, 4,
								     3, 0,
								     1},
    {2, 4, 3, 1, 0},
{3, 4, 0, 1, 2}, {3, 4, 0, 2, 1}, {3, 4, 1, 0, 2}, {3, 4, 1, 2, 0}, {3, 4,
								     2, 0,
								     1},
    {3, 4, 2, 1, 0},
{4, 0, 1, 2, 3}, {4, 0, 2, 1, 3}, {4, 1, 0, 2, 3}, {4, 1, 2, 0, 3}, {4, 2,
								     0, 1,
								     3},
    {4, 2, 1, 0, 3},
{4, 0, 1, 3, 2}, {4, 0, 2, 3, 1}, {4, 1, 0, 3, 2}, {4, 1, 2, 3, 0}, {4, 2,
								     0, 3,
								     1},
    {4, 2, 1, 3, 0},
{4, 0, 3, 1, 2}, {4, 0, 3, 2, 1}, {4, 1, 3, 0, 2}, {4, 1, 3, 2, 0}, {4, 2,
								     3, 0,
								     1},
    {4, 2, 3, 1, 0},
{4, 3, 0, 1, 2}, {4, 3, 0, 2, 1}, {4, 3, 1, 0, 2}, {4, 3, 1, 2, 0}, {4, 3,
								     2, 0,
								     1},
    {4, 3, 2, 1, 0}
};

static const char *permutation_names5[]
    = { "01234", "02134", "10234", "12034", "20134", "21034", "01324",
	    "02314", "10324", "12304", "20314", "21304",
	"03124", "03214", "13024", "13204", "23014", "23104", "30124",
	    "30214", "31024", "31204", "32014", "32104",
	"01243", "02143", "10243", "12043", "20143", "21043", "01342",
	    "02341", "10342", "12340", "20341", "21340",
	"03142", "03241", "13042", "13240", "23041", "23140", "30142",
	    "30241", "31042", "31240", "32041", "32140",
	"01423", "02413", "10423", "12403", "20413", "21403", "01432",
	    "02431", "10432", "12430", "20431", "21430",
	"03412", "03421", "13402", "13420", "23401", "23410", "30412",
	    "30421", "31402", "31420", "32401", "32410",
	"04123", "04213", "14023", "14203", "24013", "24103", "04132",
	    "04231", "14032", "14230", "24031", "24130",
	"04312", "04321", "14302", "14320", "24301", "24310", "34012",
	    "34021", "34102", "34120", "34201", "34210",
	"40123", "40213", "41023", "41203", "42013", "42103", "40132",
	    "40231", "41032", "41230", "42031", "42130",
	"40312", "40321", "41302", "41320", "42301", "42310", "43012",
	    "43021", "43102", "43120", "43201", "43210", 0
};

static const char *filter_names[8]
    = { "SHello",
	"SKeyExchange",
	"SHelloDone",
	"CKeyExchange",
	"CChangeCipherSpec",
	"CFinished",
	"SChangeCipherSpec",
	"SFinished"
};

static const char *filter_names_full[12]
    = { "SHello",
	"SCertificate",
	"SKeyExchange",
	"SCertificateRequest",
	"SHelloDone",
	"CCertificate",
	"CKeyExchange",
	"CCertificateVerify",
	"CChangeCipherSpec",
	"CFinished",
	"SChangeCipherSpec",
	"SFinished"
};

static const unsigned char PUBKEY[] =
    "-----BEGIN PGP PUBLIC KEY BLOCK-----\n"
    "\n"
    "mI0ETz0XRAEEAKXSU/tg2yGvoKf/r1pdzj7dnfPHeS+BRiT34763uUhibAbTgMkp\n"
    "v44OlBPiAaZ54uuXVkz8e4pgvrBgQwIRtNp3xPaWF1CfC4F+V4LdZV8l8IG+AfES\n"
    "K0GbfUS4q8vjnPJ0TyxnXE2KtbcRdzZzWBshJ8KChKwbH2vvrMrlmEeZABEBAAG0\n"
    "CHRlc3Qga2V5iLgEEwECACIFAk89F0QCGwMGCwkIBwMCBhUIAgkKCwQWAgMBAh4B\n"
    "AheAAAoJEMNjhmkfkLY9J/YD+wYZ2BD/0/c5gkkDP2NlVvrLGyFmEwQcR7DcaQYB\n"
    "P3/Teq2gnscZ5Xm/z1qgGEpwmaVfVHY8mfEj8bYI8jAu0v1C1jCtJPUTmxf9tmkZ\n"
    "QYFNR8T+F5Xae2XseOH70lSN/AEiW02BEBFlGBx0a3T30muFfqi/KawaE7KKn2e4\n"
    "uNWvuI0ETz0XRAEEAKgZExsb7Lf9P3DmwJSvNVdkGVny7wr4/M1s0CDX20NkO7Y1\n"
    "Ao9g+qFo5MlCOEuzjVaEYmM+rro7qyxmDKsaNIzZF1VN5UeYgPFyLcBK7C+QwUqw\n"
    "1PUl/w4dFq8neQyqIPUVGRwQPlwpkkabRPNT3t/7KgDJvYzV9uu+cXCyfqErABEB\n"
    "AAGInwQYAQIACQUCTz0XRAIbDAAKCRDDY4ZpH5C2PTBtBACVsR6l4HtuzQb5WFQt\n"
    "sD/lQEk6BEY9aVfK957Oj+A4alGEGObToqVJFo/nq+P7aWExIXucJQRL8lYnC7u+\n"
    "GjPVCun5TYzKMiryxHPkQr9NBx4hh8JjkDCc8nAgI3il49uPYkmsv70CgqJFFtT8\n"
    "NfM+8fS537I+XA+hfjt20NUFIA==\n"
    "=oD3a\n" "-----END PGP PUBLIC KEY BLOCK-----\n";

static const unsigned char PRIVKEY[] =
    "-----BEGIN PGP PRIVATE KEY BLOCK-----\n"
    "\n"
    "lQHYBE89F0QBBACl0lP7YNshr6Cn/69aXc4+3Z3zx3kvgUYk9+O+t7lIYmwG04DJ\n"
    "Kb+ODpQT4gGmeeLrl1ZM/HuKYL6wYEMCEbTad8T2lhdQnwuBfleC3WVfJfCBvgHx\n"
    "EitBm31EuKvL45zydE8sZ1xNirW3EXc2c1gbISfCgoSsGx9r76zK5ZhHmQARAQAB\n"
    "AAP6A6VhRVi22MHE1YzQrTr8yvMSgwayynGcOjndHxdpEodferLx1Pp/BL+bT+ib\n"
    "Qq7RZ363Xg/7I2rHJpenQYdkI5SI4KrXIV57p8G+isyTtsxU38SY84WoB5os8sfT\n"
    "YhxG+edoTfDzXkRSWFB8EUjRaLa2b//nvLpxNRyqDSzzUxECAMtEnL5H/8gHbpZf\n"
    "D98TSJVxdAl9rBAQaVMgrFgcU/IlmxCyVEh9eh/P261tefgOnyVcGFYHxdZvJ3td\n"
    "miM+DNUCANDW1S9t7IiqflDpQIS2wGTZ/rLKPoE1F3285EaYAd0FQUq0O4/Nu31D\n"
    "5pz/S7D+PfXn9oEZH3Dvl3EVIDyq4bUB+QEzFc3BsH2uueD3g42RoBfMGl6m3LI9\n"
    "yWOnrUmIW+h9Fu8W9mcU6y82Q1G7OPIxA1me/Qtzo20lGQa8jAyzLhuit7QIdGVz\n"
    "dCBrZXmIuAQTAQIAIgUCTz0XRAIbAwYLCQgHAwIGFQgCCQoLBBYCAwECHgECF4AA\n"
    "CgkQw2OGaR+Qtj0n9gP7BhnYEP/T9zmCSQM/Y2VW+ssbIWYTBBxHsNxpBgE/f9N6\n"
    "raCexxnleb/PWqAYSnCZpV9UdjyZ8SPxtgjyMC7S/ULWMK0k9RObF/22aRlBgU1H\n"
    "xP4Xldp7Zex44fvSVI38ASJbTYEQEWUYHHRrdPfSa4V+qL8prBoTsoqfZ7i41a+d\n"
    "AdgETz0XRAEEAKgZExsb7Lf9P3DmwJSvNVdkGVny7wr4/M1s0CDX20NkO7Y1Ao9g\n"
    "+qFo5MlCOEuzjVaEYmM+rro7qyxmDKsaNIzZF1VN5UeYgPFyLcBK7C+QwUqw1PUl\n"
    "/w4dFq8neQyqIPUVGRwQPlwpkkabRPNT3t/7KgDJvYzV9uu+cXCyfqErABEBAAEA\n"
    "A/4wX+brqkGZQTv8lateHn3PRHM3O34nPjgiNeo/SV9EKZg1e1PdRx9ZTAJrGK9y\n"
    "uZ03BKn7vZIy7fD4ufVzV/s/BaypVmvwjZud8fdMgsMQAJYtoMhozbOtUelCFpja\n"
    "I1xAbDBx1PAAbS8Sh022/0jvOGnZhvkgZMG90z7AEANUYQIAwzywU087TcJk8Bzd\n"
    "37JGWyE4f3iYFGA+r8BoIOrxvvgfUHKxdhG0gaT8SDeRAwNY6D43dCBZkG7Uel1F\n"
    "x9MlLQIA3Goaz58hEN0fdm4TM7A8crtMB+f8/h87EneBgMl+Yj/3sklhyahR6Itm\n"
    "lGuAAGTAOmD7i8OmS/a1ac5MtHAGtwH6A0B5GjaL8VnLQo4vFnuR7JuCQaLqGadV\n"
    "mBmKxVHElduLf/VauBQPD5KZA+egpg+laJ4JLVXMmKIZGqRzopcIWZnKiJ8EGAEC\n"
    "AAkFAk89F0QCGwwACgkQw2OGaR+Qtj0wbQQAlbEepeB7bs0G+VhULbA/5UBJOgRG\n"
    "PWlXyveezo/gOGpRhBjm06KlSRaP56vj+2lhMSF7nCUES/JWJwu7vhoz1Qrp+U2M\n"
    "yjIq8sRz5EK/TQceIYfCY5AwnPJwICN4pePbj2JJrL+9AoKiRRbU/DXzPvH0ud+y\n"
    "PlwPoX47dtDVBSA=\n" "=EVlv\n" "-----END PGP PRIVATE KEY BLOCK-----\n";

// }}}

// {{{ other global state

enum role role;

#define role_name (role == SERVER ? "server" : "client")

int debug;
int nonblock;
int replay;
int full;
int timeout_seconds;
int retransmit_milliseconds;
int run_to_end;

int run_id;

// }}}

// {{{ logging and error handling

static void logfn(int level, const char *s)
{
	if (debug) {
		fprintf(stdout, "%i %s|<%i> %s", run_id, role_name, level,
			s);
	}
}

static void auditfn(gnutls_session_t session, const char *s)
{
	if (debug) {
		fprintf(stdout, "%i %s| %s", run_id, role_name, s);
	}
}

static void drop(const char *packet)
{
	if (debug) {
		fprintf(stdout, "%i %s| dropping %s\n", run_id, role_name,
			packet);
	}
}

static int _process_error(int loc, int code, int die)
{
	if (code < 0 && (die || code != GNUTLS_E_AGAIN)) {
		fprintf(stdout, "%i <%s tls> line %i: %s", run_id,
			role_name, loc, gnutls_strerror(code));
		if (gnutls_error_is_fatal(code) || die) {
			fprintf(stdout, " (fatal)\n");
			exit(1);
		} else {
			fprintf(stdout, "\n");
		}
	}
	return code;
}

#define die_on_error(code) _process_error(__LINE__, code, 1)
#define process_error(code) _process_error(__LINE__, code, 0)

static void _process_error_or_timeout(int loc, int err, time_t tdiff)
{
	if (err < 0) {
		if (err != GNUTLS_E_TIMEDOUT || tdiff >= 60) {
			_process_error(loc, err, 0);
		} else {
			fprintf(stdout,
				"%i %s| line %i: {spurious timeout} (fatal)",
				run_id, role_name, loc);
			exit(1);
		}
	}
}

#define process_error_or_timeout(code, tdiff) _process_error_or_timeout(__LINE__, code, tdiff)

static void rperror(const char *name)
{
	fprintf(stdout, "%i %s| %s: %s\n", run_id, role_name, name,
		strerror(errno));
}

// }}}

// {{{ init, shared, and teardown code and data for packet stream filters

filter_packet_state_t state_packet_ServerHello = { 0 };
filter_packet_state_t state_packet_ServerCertificate = { 0 };
filter_packet_state_t state_packet_ServerKeyExchange = { 0 };
filter_packet_state_t state_packet_ServerCertificateRequest = { 0 };
filter_packet_state_t state_packet_ServerHelloDone = { 0 };
filter_packet_state_t state_packet_ClientCertificate = { 0 };
filter_packet_state_t state_packet_ClientKeyExchange = { 0 };
filter_packet_state_t state_packet_ClientCertificateVerify = { 0 };
filter_packet_state_t state_packet_ClientChangeCipherSpec = { 0 };
filter_packet_state_t state_packet_ClientFinished = { 0 };
filter_packet_state_t state_packet_ServerChangeCipherSpec = { 0 };
filter_packet_state_t state_packet_ServerFinished = { 0 };

filter_permute_state_t state_permute_ServerHello =
    { {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}}, 0, 0 };
filter_permute_state_t state_permute_ServerHelloFull =
    { {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}}, 0, 0 };
filter_permute_state_t state_permute_ServerFinished =
    { {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}}, 0, 0 };
filter_permute_state_t state_permute_ClientFinished =
    { {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}}, 0, 0 };
filter_permute_state_t state_permute_ClientFinishedFull =
    { {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}}, 0, 0 };

filter_fn filter_chain[32];
int filter_current_idx;

static void filter_permute_state_free_buffer(filter_permute_state_t *
					     state)
{
	unsigned int i;

	for (i = 0; i < sizeof(state->packets) / sizeof(state->packets[0]);
	     i++) {
		free(state->packets[i].data);
		state->packets[i].data = NULL;
	}
}

static void filter_clear_state(void)
{
	filter_current_idx = 0;

	filter_permute_state_free_buffer(&state_permute_ServerHello);
	filter_permute_state_free_buffer(&state_permute_ServerHelloFull);
	filter_permute_state_free_buffer(&state_permute_ServerFinished);
	filter_permute_state_free_buffer(&state_permute_ClientFinished);
	filter_permute_state_free_buffer
	    (&state_permute_ClientFinishedFull);

	memset(&state_packet_ServerHello, 0,
	       sizeof(state_packet_ServerHello));
	memset(&state_packet_ServerCertificate, 0,
	       sizeof(state_packet_ServerCertificate));
	memset(&state_packet_ServerKeyExchange, 0,
	       sizeof(state_packet_ServerKeyExchange));
	memset(&state_packet_ServerCertificateRequest, 0,
	       sizeof(state_packet_ServerCertificateRequest));
	memset(&state_packet_ServerHelloDone, 0,
	       sizeof(state_packet_ServerHelloDone));
	memset(&state_packet_ClientCertificate, 0,
	       sizeof(state_packet_ClientCertificate));
	memset(&state_packet_ClientKeyExchange, 0,
	       sizeof(state_packet_ClientKeyExchange));
	memset(&state_packet_ClientCertificateVerify, 0,
	       sizeof(state_packet_ClientCertificateVerify));
	memset(&state_packet_ClientChangeCipherSpec, 0,
	       sizeof(state_packet_ClientChangeCipherSpec));
	memset(&state_packet_ClientFinished, 0,
	       sizeof(state_packet_ClientFinished));
	memset(&state_packet_ServerChangeCipherSpec, 0,
	       sizeof(state_packet_ServerChangeCipherSpec));
	memset(&state_packet_ServerFinished, 0,
	       sizeof(state_packet_ServerFinished));
	memset(&state_permute_ServerHello, 0,
	       sizeof(state_permute_ServerHello));
	memset(&state_permute_ServerHelloFull, 0,
	       sizeof(state_permute_ServerHelloFull));
	memset(&state_permute_ServerFinished, 0,
	       sizeof(state_permute_ServerFinished));
	memset(&state_permute_ClientFinished, 0,
	       sizeof(state_permute_ClientFinished));
	memset(&state_permute_ClientFinishedFull, 0,
	       sizeof(state_permute_ClientFinishedFull));
}

static int rbuffer[5*1024];
unsigned rbuffer_size = 0;

static void filter_run_next(gnutls_transport_ptr_t fd,
			    const unsigned char *buffer, size_t len)
{
	filter_fn fn = filter_chain[filter_current_idx];
	filter_current_idx++;
	if (fn) {
		fn(fd, buffer, len);
	} else {
		send((int) (intptr_t) fd, buffer, len, 0);
	}
	filter_current_idx--;

	if (replay != 0) {
		if (rbuffer_size == 0 && len < sizeof(rbuffer)) {
			memcpy(rbuffer, buffer, len);
			rbuffer_size = len;
		} else if (rbuffer_size != 0) {
			send((int) (intptr_t) fd, rbuffer, rbuffer_size, 0);
			if (len < sizeof(rbuffer) && len > rbuffer_size) {
				memcpy(rbuffer, buffer, len);
				rbuffer_size = len;
			}
		}
	}
}

// }}}

// {{{ packet match functions

static int match_ServerHello(const unsigned char *buffer, size_t len)
{
	return role == SERVER && len >= 13 + 1 && buffer[0] == 22
	    && buffer[13] == 2;
}

static int match_ServerCertificate(const unsigned char *buffer, size_t len)
{
	return role == SERVER && len >= 13 + 1 && buffer[0] == 22
	    && buffer[13] == 11;
}

static int match_ServerKeyExchange(const unsigned char *buffer, size_t len)
{
	return role == SERVER && len >= 13 + 1 && buffer[0] == 22
	    && buffer[13] == 12;
}

static int match_ServerCertificateRequest(const unsigned char *buffer,
					  size_t len)
{
	return role == SERVER && len >= 13 + 1 && buffer[0] == 22
	    && buffer[13] == 13;
}

static int match_ServerHelloDone(const unsigned char *buffer, size_t len)
{
	return role == SERVER && len >= 13 + 1 && buffer[0] == 22
	    && buffer[13] == 14;
}

static int match_ClientCertificate(const unsigned char *buffer, size_t len)
{
	return role == CLIENT && len >= 13 + 1 && buffer[0] == 22
	    && buffer[13] == 11;
}

static int match_ClientKeyExchange(const unsigned char *buffer, size_t len)
{
	return role == CLIENT && len >= 13 + 1 && buffer[0] == 22
	    && buffer[13] == 16;
}

static int match_ClientCertificateVerify(const unsigned char *buffer,
					 size_t len)
{
	return role == CLIENT && len >= 13 + 1 && buffer[0] == 22
	    && buffer[13] == 15;
}

static int match_ClientChangeCipherSpec(const unsigned char *buffer,
					size_t len)
{
	return role == CLIENT && len >= 13 && buffer[0] == 20;
}

static int match_ClientFinished(const unsigned char *buffer, size_t len)
{
	return role == CLIENT && len >= 13 && buffer[0] == 22
	    && buffer[4] == 1;
}

static int match_ServerChangeCipherSpec(const unsigned char *buffer,
					size_t len)
{
	return role == SERVER && len >= 13 && buffer[0] == 20;
}

static int match_ServerFinished(const unsigned char *buffer, size_t len)
{
	return role == SERVER && len >= 13 && buffer[0] == 22
	    && buffer[4] == 1;
}

// }}}

// {{{ packet drop filters

#define FILTER_DROP_COUNT 3
#define DECLARE_FILTER(packet) \
	static void filter_packet_##packet(gnutls_transport_ptr_t fd, \
			const unsigned char* buffer, size_t len) \
	{ \
		if (match_##packet(buffer, len) && (state_packet_##packet).count++ < FILTER_DROP_COUNT) { \
			drop(#packet); \
		} else { \
			filter_run_next(fd, buffer, len); \
		} \
	}

DECLARE_FILTER(ServerHello)
    DECLARE_FILTER(ServerCertificate)
    DECLARE_FILTER(ServerKeyExchange)
    DECLARE_FILTER(ServerCertificateRequest)
    DECLARE_FILTER(ServerHelloDone)
    DECLARE_FILTER(ClientCertificate)
    DECLARE_FILTER(ClientKeyExchange)
    DECLARE_FILTER(ClientCertificateVerify)
    DECLARE_FILTER(ClientChangeCipherSpec)
    DECLARE_FILTER(ClientFinished)
    DECLARE_FILTER(ServerChangeCipherSpec)
    DECLARE_FILTER(ServerFinished)
// }}}
// {{{ flight permutation filters
static void filter_permute_state_run(filter_permute_state_t * state,
				     int packetCount,
				     gnutls_transport_ptr_t fd,
				     const unsigned char *buffer,
				     size_t len)
{
	unsigned char *data = malloc(len);
	int packet = state->order[state->count];

	memcpy(data, buffer, len);
	state->packets[packet].data = data;
	state->packets[packet].size = len;
	state->count++;

	if (state->count == packetCount) {
		for (packet = 0; packet < packetCount; packet++) {
			filter_run_next(fd, state->packets[packet].data,
					state->packets[packet].size);
		}
		filter_permute_state_free_buffer(state);
		state->count = 0;
	}
}

#define DECLARE_PERMUTE(flight) \
	static void filter_permute_##flight(gnutls_transport_ptr_t fd, \
			const unsigned char* buffer, size_t len) \
	{ \
		int count = sizeof(permute_match_##flight) / sizeof(permute_match_##flight[0]); \
		int i; \
		for (i = 0; i < count; i++) { \
			if (permute_match_##flight[i](buffer, len)) { \
				filter_permute_state_run(&state_permute_##flight, count, fd, buffer, len); \
				return; \
			} \
		} \
		filter_run_next(fd, buffer, len); \
	}

static match_fn permute_match_ServerHello[] =
    { match_ServerHello, match_ServerKeyExchange, match_ServerHelloDone };
static match_fn permute_match_ServerHelloFull[] =
    { match_ServerHello, match_ServerCertificate, match_ServerKeyExchange,
	match_ServerCertificateRequest, match_ServerHelloDone
};
static match_fn permute_match_ServerFinished[] =
    { match_ServerChangeCipherSpec, match_ServerFinished };
static match_fn permute_match_ClientFinished[] =
    { match_ClientKeyExchange, match_ClientChangeCipherSpec,
match_ClientFinished };
static match_fn permute_match_ClientFinishedFull[] =
    { match_ClientCertificate, match_ClientKeyExchange,
	match_ClientCertificateVerify, match_ClientChangeCipherSpec,
	    match_ClientFinished
};

DECLARE_PERMUTE(ServerHello)
    DECLARE_PERMUTE(ServerHelloFull)
    DECLARE_PERMUTE(ServerFinished)
    DECLARE_PERMUTE(ClientFinished)
    DECLARE_PERMUTE(ClientFinishedFull)
// }}}
// {{{ emergency deadlock resolution time bomb
timer_t killtimer_tid = 0;

static void killtimer_set(void)
{
	struct sigevent sig;
	struct itimerspec tout = { {0, 0}, {2 * timeout_seconds, 0} };

	if (killtimer_tid != 0) {
		timer_delete(killtimer_tid);
	}

	memset(&sig, 0, sizeof(sig));
	sig.sigev_notify = SIGEV_SIGNAL;
	sig.sigev_signo = 15;
	if (timer_create(CLOCK_MONOTONIC, &sig, &killtimer_tid) < 0) {
		rperror("timer_create");
		exit(3);
	}

	timer_settime(killtimer_tid, 0, &tout, 0);
}

// }}}

// {{{ actual gnutls operations

gnutls_certificate_credentials_t cred;
gnutls_session_t session;

static ssize_t writefn(gnutls_transport_ptr_t fd, const void *buffer,
		       size_t len)
{
	filter_run_next(fd, (const unsigned char *) buffer, len);
	return len;
}

static void await(int fd, int timeout)
{
	if (nonblock) {
		struct pollfd p = { fd, POLLIN, 0 };
		if (poll(&p, 1, timeout) < 0 && errno != EAGAIN
		    && errno != EINTR) {
			rperror("poll");
			exit(3);
		}
	}
}

static void cred_init(void)
{
	gnutls_datum_t key = { (unsigned char *) PUBKEY, sizeof(PUBKEY) };
	gnutls_datum_t sec =
	    { (unsigned char *) PRIVKEY, sizeof(PRIVKEY) };

	gnutls_certificate_allocate_credentials(&cred);

	gnutls_certificate_set_openpgp_key_mem(cred, &key, &sec,
					       GNUTLS_OPENPGP_FMT_BASE64);
}

static void session_init(int sock, int server)
{
	gnutls_init(&session,
		    GNUTLS_DATAGRAM | (server ? GNUTLS_SERVER :
				       GNUTLS_CLIENT)
		    | GNUTLS_NONBLOCK * nonblock);
	gnutls_priority_set_direct(session,
				   "+CTYPE-OPENPGP:+CIPHER-ALL:+MAC-ALL:+ECDHE-RSA:+ANON-ECDH",
				   0);
	gnutls_transport_set_int(session, sock);

	if (full) {
		gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE,
				       cred);
		if (server) {
			gnutls_certificate_server_set_request(session,
							      GNUTLS_CERT_REQUIRE);
		}
	} else if (server) {
		gnutls_anon_server_credentials_t cred;
		gnutls_anon_allocate_server_credentials(&cred);
		gnutls_credentials_set(session, GNUTLS_CRD_ANON, cred);
	} else {
		gnutls_anon_client_credentials_t cred;
		gnutls_anon_allocate_client_credentials(&cred);
		gnutls_credentials_set(session, GNUTLS_CRD_ANON, cred);
	}

	gnutls_transport_set_push_function(session, writefn);

	gnutls_dtls_set_mtu(session, 1400);
	gnutls_dtls_set_timeouts(session, retransmit_milliseconds,
				 timeout_seconds * 1000);
}

static void client(int sock)
{
	int err = 0;
	time_t started = time(0);
	const char *line = "foobar!";
	char buffer[8192];
	int len;

	session_init(sock, 0);

	killtimer_set();
	do {
		err = process_error(gnutls_handshake(session));
		if (err != 0) {
			int t = gnutls_dtls_get_timeout(session);
			await(sock, t ? t : 100);
		}
	} while (err != 0);
	process_error_or_timeout(err, time(0) - started);

	killtimer_set();
	die_on_error(gnutls_record_send(session, line, strlen(line)));

	do {
		await(sock, -1);
		len =
		    process_error(gnutls_record_recv
				  (session, buffer, sizeof(buffer)));
	} while (len < 0);

	if (len > 0 && strncmp(line, buffer, len) == 0) {
		exit(0);
	} else {
		exit(1);
	}
}

static void server(int sock)
{
	int err;
	time_t started = time(0);
	char buffer[8192];
	int len;

	session_init(sock, 1);

	await(sock, -1);

	killtimer_set();
	do {
		err = process_error(gnutls_handshake(session));
		if (err != 0) {
			int t = gnutls_dtls_get_timeout(session);
			await(sock, t ? t : 100);
		}
	} while (err != 0);
	process_error_or_timeout(err, time(0) - started);

	killtimer_set();
	do {
		await(sock, -1);
		len =
		    process_error(gnutls_record_recv
				  (session, buffer, sizeof(buffer)));
	} while (len < 0);

	die_on_error(gnutls_record_send(session, buffer, len));
	exit(0);
}

// }}}

// {{{ test running/handling itself

#if 0
static void udp_sockpair(int *socks)
{
	struct sockaddr_in6 sa =
	    { AF_INET6, htons(30000), 0, in6addr_loopback, 0 };
	struct sockaddr_in6 sb =
	    { AF_INET6, htons(20000), 0, in6addr_loopback, 0 };

	socks[0] = socket(AF_INET6, SOCK_DGRAM, 0);
	socks[1] = socket(AF_INET6, SOCK_DGRAM, 0);

	bind(socks[0], (struct sockaddr *) &sa, sizeof(sa));
	bind(socks[1], (struct sockaddr *) &sb, sizeof(sb));

	connect(socks[1], (struct sockaddr *) &sa, sizeof(sa));
	connect(socks[0], (struct sockaddr *) &sb, sizeof(sb));
}
#endif

static int run_test(void)
{
	int fds[2];
	int pid1, pid2;
	int status2;

	if (socketpair(AF_LOCAL, SOCK_STREAM, 0, fds) < 0) {
		rperror("socketpair");
		exit(2);
	}

	if (nonblock) {
		fcntl(fds[0], F_SETFL, (long) O_NONBLOCK);
		fcntl(fds[1], F_SETFL, (long) O_NONBLOCK);
	}

	if (!(pid1 = fork())) {
		role = SERVER;
		server(fds[1]);	// noreturn
	} else if (pid1 < 0) {
		rperror("fork server");
		exit(2);
	}
	if (!(pid2 = fork())) {
		role = CLIENT;
		client(fds[0]);	// noreturn
	} else if (pid2 < 0) {
		rperror("fork client");
		exit(2);
	}
	while (waitpid(pid2, &status2, 0) < 0 && errno == EINTR);
	kill(pid1, 15);
	while (waitpid(pid1, 0, 0) < 0 && errno == EINTR);

	close(fds[0]);
	close(fds[1]);

	if (!WIFSIGNALED(status2) && WEXITSTATUS(status2) != 3) {
		return ! !WEXITSTATUS(status2);
	} else {
		return 3;
	}
}

static filter_fn filters[8]
    = { filter_packet_ServerHello,
	filter_packet_ServerKeyExchange,
	filter_packet_ServerHelloDone,
	filter_packet_ClientKeyExchange,
	filter_packet_ClientChangeCipherSpec,
	filter_packet_ClientFinished,
	filter_packet_ServerChangeCipherSpec,
	filter_packet_ServerFinished
};

static filter_fn filters_full[12]
    = { filter_packet_ServerHello,
	filter_packet_ServerCertificate,
	filter_packet_ServerKeyExchange,
	filter_packet_ServerCertificateRequest,
	filter_packet_ServerHelloDone,
	filter_packet_ClientCertificate,
	filter_packet_ClientKeyExchange,
	filter_packet_ClientCertificateVerify,
	filter_packet_ClientChangeCipherSpec,
	filter_packet_ClientFinished,
	filter_packet_ServerChangeCipherSpec,
	filter_packet_ServerFinished
};

static int run_one_test(int dropMode, int serverFinishedPermute,
			int serverHelloPermute, int clientFinishedPermute)
{
	int fnIdx = 0;
	int res, filterIdx;
	filter_fn *local_filters = full ? filters_full : filters;
	const char **local_filter_names =
	    full ? filter_names_full : filter_names;
	const char **permutation_namesX =
	    full ? permutation_names5 : permutation_names3;
	int filter_count = full ? 12 : 8;
	run_id =
	    ((dropMode * 2 + serverFinishedPermute) * (full ? 120 : 6) +
	     serverHelloPermute) * (full ? 120 : 6) +
	    clientFinishedPermute;

	filter_clear_state();

	if (full) {
		filter_chain[fnIdx++] = filter_permute_ServerHelloFull;
		state_permute_ServerHelloFull.order =
		    permutations5[serverHelloPermute];

		filter_chain[fnIdx++] = filter_permute_ClientFinishedFull;
		state_permute_ClientFinishedFull.order =
		    permutations5[clientFinishedPermute];
	} else {
		filter_chain[fnIdx++] = filter_permute_ServerHello;
		state_permute_ServerHello.order =
		    permutations3[serverHelloPermute];

		filter_chain[fnIdx++] = filter_permute_ClientFinished;
		state_permute_ClientFinished.order =
		    permutations3[clientFinishedPermute];
	}

	filter_chain[fnIdx++] = filter_permute_ServerFinished;
	state_permute_ServerFinished.order =
	    permutations2[serverFinishedPermute];

	if (dropMode) {
		for (filterIdx = 0; filterIdx < filter_count; filterIdx++) {
			if (dropMode & (1 << filterIdx)) {
				filter_chain[fnIdx++] =
				    local_filters[filterIdx];
			}
		}
	}
	filter_chain[fnIdx++] = NULL;

	res = run_test();

	switch (res) {
	case 0:
		fprintf(stdout, "%i ++ ", run_id);
		break;
	case 1:
		fprintf(stdout, "%i -- ", run_id);
		break;
	case 2:
		fprintf(stdout, "%i !! ", run_id);
		break;
	case 3:
		fprintf(stdout, "%i TT ", run_id);
		break;
	}

	fprintf(stdout, "SHello(%s), ",
		permutation_namesX[serverHelloPermute]);
	fprintf(stdout, "SFinished(%s), ",
		permutation_names2[serverFinishedPermute]);
	fprintf(stdout, "CFinished(%s) :- ",
		permutation_namesX[clientFinishedPermute]);
	if (dropMode) {
		for (filterIdx = 0; filterIdx < filter_count; filterIdx++) {
			if (dropMode & (1 << filterIdx)) {
				if (dropMode & ((1 << filterIdx) - 1)) {
					fprintf(stdout, ", ");
				}
				fprintf(stdout, "%s",
					local_filter_names[filterIdx]);
			}
		}
	}
	fprintf(stdout, "\n");

	return res;
}

static int run_test_by_id(int id)
{
	int pscale = full ? 120 : 6;
	int dropMode, serverFinishedPermute, serverHelloPermute,
	    clientFinishedPermute;

	clientFinishedPermute = id % pscale;
	id /= pscale;

	serverHelloPermute = id % pscale;
	id /= pscale;

	serverFinishedPermute = id % 2;
	id /= 2;

	dropMode = id;

	return run_one_test(dropMode, serverFinishedPermute,
			    serverHelloPermute, clientFinishedPermute);
}

int *job_pids;
int job_limit;
int children = 0;

static void register_child(int pid)
{
	int idx;

	children++;
	for (idx = 0; idx < job_limit; idx++) {
		if (job_pids[idx] == 0) {
			job_pids[idx] = pid;
			return;
		}
	}
}

static int wait_children(int child_limit)
{
	int fail = 0;
	int result = 1;

	while (children > child_limit) {
		int status;
		int idx;
		int pid = waitpid(0, &status, 0);
		if (pid < 0 && errno == ECHILD) {
			break;
		}
		for (idx = 0; idx < job_limit; idx++) {
			if (job_pids[idx] == pid) {
				children--;
				if (WEXITSTATUS(status)) {
					result = 1;
					if (!run_to_end && !fail) {
						fprintf(stderr,
							"One test failed, waiting for remaining tests\n");
						fail = 1;
						child_limit = 0;
					}
				}
				job_pids[idx] = 0;
				break;
			}
		}
	}

	if (fail) {
		exit(1);
	}

	return result;
}

static int run_tests_from_id_list(int childcount)
{
	int test_id;
	int ret;
	int result = 0;

	while ((ret = fscanf(stdin, "%i\n", &test_id)) > 0) {
		int pid;
		if (test_id < 0
		    || test_id >
		    2 * (full ? 120 * 120 * (1 << 12) : 6 * 6 * 256)) {
			fprintf(stderr, "Invalid test id %i\n", test_id);
			break;
		}
		if (!(pid = fork())) {
			exit(run_test_by_id(test_id));
		} else if (pid < 0) {
			rperror("fork");
			result = 4;
			break;
		} else {
			register_child(pid);
			result |= wait_children(childcount);
		}
	}

	if (ret < 0 && ret != EOF) {
		fprintf(stderr, "Error reading test id list\n");
	}

	result |= wait_children(0);

	return result;
}

static int run_all_tests(int childcount)
{
	int dropMode, serverFinishedPermute, serverHelloPermute,
	    clientFinishedPermute;
	int result = 0;

	for (dropMode = 0; dropMode != 1 << (full ? 12 : 8); dropMode++)
		for (serverFinishedPermute = 0; serverFinishedPermute < 2;
		     serverFinishedPermute++)
			for (serverHelloPermute = 0;
			     serverHelloPermute < (full ? 120 : 6);
			     serverHelloPermute++)
				for (clientFinishedPermute = 0;
				     clientFinishedPermute <
				     (full ? 120 : 6);
				     clientFinishedPermute++) {
					int pid;
					if (!(pid = fork())) {
						exit(run_one_test
						     (dropMode,
						      serverFinishedPermute,
						      serverHelloPermute,
						      clientFinishedPermute));
					} else if (pid < 0) {
						rperror("fork");
						result = 4;
						break;
					} else {
						register_child(pid);
						result |=
						    wait_children
						    (childcount);
					}
				}

	result |= wait_children(0);

	return result;
}

// }}}

static int parse_permutation(const char *arg, const char *permutations[],
			     int *val)
{
	*val = 0;
	while (permutations[*val]) {
		if (strcmp(permutations[*val], arg) == 0) {
			return 1;
		} else {
			*val += 1;
		}
	}
	return 0;
}

int main(int argc, const char *argv[])
{
	int dropMode = 0;
	int serverFinishedPermute = 0;
	int serverHelloPermute = 0;
	int clientFinishedPermute = 0;
	int batch = 0;
	int arg;

	nonblock = 0;
	replay = 0;
	debug = 0;
	timeout_seconds = 120;
	retransmit_milliseconds = 100;
	full = 0;
	run_to_end = 1;
	job_limit = 1;

#define NEXT_ARG(name) \
	do { \
		if (++arg >= argc) { \
			fprintf(stderr, "No argument for -" #name "\n"); \
			exit(8); \
		} \
	} while (0);
#define FAIL_ARG(name) \
	do { \
		fprintf(stderr, "Invalid argument for -" #name "\n"); \
		exit(8); \
	} while (0);

	for (arg = 1; arg < argc; arg++) {
		if (strcmp("-die", argv[arg]) == 0) {
			run_to_end = 0;
		} else if (strcmp("-batch", argv[arg]) == 0) {
			batch = 1;
		} else if (strcmp("-d", argv[arg]) == 0) {
			char *end;
			int level = strtol(argv[arg + 1], &end, 10);
			if (*end == '\0') {
				debug = level;
				arg++;
			} else {
				debug++;
			}
		} else if (strcmp("-nb", argv[arg]) == 0) {
			nonblock = 1;
		} else if (strcmp("-r", argv[arg]) == 0) {
			replay = 1;
		} else if (strcmp("-timeout", argv[arg]) == 0) {
			char *end;
			int val;

			NEXT_ARG(timeout);
			val = strtol(argv[arg], &end, 10);
			if (*end == '\0') {
				timeout_seconds = val;
			} else {
				FAIL_ARG(timeout);
			}
		} else if (strcmp("-retransmit", argv[arg]) == 0) {
			char *end;
			int val;

			NEXT_ARG(retransmit);
			val = strtol(argv[arg], &end, 10);
			if (*end == '\0') {
				retransmit_milliseconds = val;
			} else {
				FAIL_ARG(retransmit);
			}
		} else if (strcmp("-j", argv[arg]) == 0) {
			char *end;
			int val;

			NEXT_ARG(timeout);
			val = strtol(argv[arg], &end, 10);
			if (*end == '\0') {
				job_limit = val;
			} else {
				FAIL_ARG(j);
			}
		} else if (strcmp("-full", argv[arg]) == 0) {
			full = 1;
		} else if (strcmp("-shello", argv[arg]) == 0) {
			NEXT_ARG(shello);
			if (!parse_permutation
			    (argv[arg],
			     full ? permutation_names5 :
			     permutation_names3, &serverHelloPermute)) {
				FAIL_ARG(shell);
			}
		} else if (strcmp("-sfinished", argv[arg]) == 0) {
			NEXT_ARG(sfinished);
			if (!parse_permutation
			    (argv[arg], permutation_names2,
			     &serverFinishedPermute)) {
				FAIL_ARG(sfinished);
			}
		} else if (strcmp("-cfinished", argv[arg]) == 0) {
			NEXT_ARG(cfinished);
			if (!parse_permutation
			    (argv[arg],
			     full ? permutation_names5 :
			     permutation_names3, &clientFinishedPermute)) {
				FAIL_ARG(cfinished);
			}
		} else {
			int drop;
			int filter_count = full ? 12 : 8;
			const char **local_filter_names =
			    full ? filter_names_full : filter_names;
			for (drop = 0; drop < filter_count; drop++) {
				if (strcmp
				    (local_filter_names[drop],
				     argv[arg]) == 0) {
					dropMode |= (1 << drop);
					break;
				}
			}
			if (drop == filter_count) {
				fprintf(stderr, "Unknown packet %s\n",
					argv[arg]);
				exit(8);
			}
		}
	}

	setlinebuf(stdout);
	global_init();
	cred_init();
	gnutls_global_set_log_function(logfn);
	gnutls_global_set_audit_log_function(auditfn);
	gnutls_global_set_log_level(debug);

	if (dropMode || serverFinishedPermute || serverHelloPermute
	    || clientFinishedPermute) {
		return run_one_test(dropMode, serverFinishedPermute,
				    serverHelloPermute,
				    clientFinishedPermute);
	} else {
		job_pids = calloc(sizeof(int), job_limit);
		if (batch) {
			return run_tests_from_id_list(job_limit);
		} else {
			return run_all_tests(job_limit);
		}
	}
}

// vim: foldmethod=marker

#else				/* NO POSIX TIMERS */

int main(int argc, const char *argv[])
{
	exit(77);
}

#endif
