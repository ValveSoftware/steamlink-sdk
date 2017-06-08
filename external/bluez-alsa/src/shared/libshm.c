#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "libshm.h"

#ifdef DEBUG
#define DPRINTF(format, ...) printf(format, ##__VA_ARGS__)
#else
#define DPRINTF(format, ...)
#endif


typedef struct
{
	uint8_t data[0]; // ring buffer
} shared_data_t;


struct libshm_data
{
	int listening_sock;
	int sock;

	// writer
	size_t write_offset;
	size_t bytes_available;

	// reader
	size_t read_offset;
	size_t bytes_used;

	uint32_t data_length; // per direction

	shared_data_t *shared;
	uint8_t *shared_in;
	uint8_t *shared_out;

	// master only
	bool peer_opened;
	char *listening_socket_path;
	char *shm_path;

	size_t seq;
};

struct ipc_block
{
#define	OP_NOP	 0
#define OP_READ  1
#define OP_WRITE 2
	uint32_t 	op;
	size_t		nr_bytes;
	size_t		seq;
};

static inline size_t max(size_t a, size_t b)
{
	return a > b ? a : b;
}

static inline size_t min(size_t a, size_t b)
{
	return a < b ? a : b;
}


struct libshm_data *libshm_create_master(const char *sock_path, const char *mem_path, size_t size)
{
	int mem_fd, sock;
	struct libshm_data *p = NULL;
	shared_data_t *shared = NULL;
	size_t data_size = sizeof(shared_data_t) + 2 * size;
	struct sockaddr_un addr = {
	       .sun_family = AF_UNIX,
	};

	printf("%s: sock_path=%s mem_path=%s size=%zu\n", __FUNCTION__, sock_path, mem_path, size);

	shm_unlink(mem_path);

	if ((mem_fd = shm_open(mem_path, O_CREAT|O_EXCL|O_RDWR, S_IRUSR|S_IWUSR)) < 0) {
		printf("Could not create shared memory at %s: %s\n", mem_path, strerror(errno));
		return NULL;
	}

	ftruncate(mem_fd, data_size);
	shared = (shared_data_t*)mmap(0, data_size, PROT_READ|PROT_WRITE, MAP_SHARED, mem_fd, 0);
	close(mem_fd);
	
	if (shared == MAP_FAILED) {
		printf("Could not mmap() file: %s\n", strerror(errno));
		return NULL;
	}

	if ((sock = socket(AF_UNIX, SOCK_STREAM, 0) ) < 0 ) {
		printf("Could not create domain socket: %s\n", strerror(errno));
		goto unmap_mem;
	}

	strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);

	unlink(sock_path);
	if (bind(sock, (const struct sockaddr *) &addr, sizeof(addr)) < 0) {
		printf("Couldn't bind local socket to %s: %s\n", sock_path, strerror(errno));
		goto close_sock;
	}

	if (listen(sock, 5) < 0) {
		printf("Couldn't listen on master socket: %s\n", strerror(errno));
	}

	p = calloc(1, sizeof(*p));
	if (!p)
		goto close_sock;

	p->listening_sock = sock;
	p->listening_socket_path = strdup(sock_path);
	p->shm_path = strdup(mem_path);
	p->sock = -1; // no open socket yet
	p->shared = shared;
	p->data_length = size;
	p->shared_in = &shared->data[0];
	p->shared_out = &shared->data[size];

	return p;

close_sock:
	close(sock);
unmap_mem:
	munmap(shared, data_size);

	return NULL;
}


int libshm_bytes_outgoing(struct libshm_data *shm)
{
	if (!shm)
		return 0;

	return shm->bytes_available;
}


int libshm_bytes_incoming(struct libshm_data *shm)
{
	if (!shm)
		return 0;

	return shm->bytes_used;
}


struct libshm_data *libshm_create_slave(const char *sock_path, const char *mem_path, size_t size)
{
	int mem_fd, sock;
	shared_data_t *shared = NULL;
	struct libshm_data *c = NULL;
	size_t data_size = sizeof(shared_data_t) + 2 * size;
	struct sockaddr_un addr = {
	       .sun_family = AF_UNIX,
	};
	struct ipc_block ipc = {
		.nr_bytes = 0,
	};

	printf("%s: sock_path=%s mem_path=%s size=%zu\n", __FUNCTION__, sock_path, mem_path, size);

	c = calloc(1, sizeof(*c));
	if (!c)
		return NULL;

	if ((mem_fd = shm_open(mem_path, O_RDWR, S_IRUSR|S_IWUSR)) < 0) {
		printf("Could not open shared memory at %s: %s\n", mem_path, strerror(errno));
		goto out_free;
	}

	shared = (shared_data_t*)mmap(0, data_size, PROT_READ|PROT_WRITE, MAP_SHARED, mem_fd, 0);
	close(mem_fd);
	
	if (shared == MAP_FAILED) {
		printf("Could not mmap() file: %s\n", strerror(errno));
		goto out_free;
	}

	if ((sock = socket(AF_UNIX, SOCK_STREAM, 0) ) < 0 ) {
		printf("Could not create domain socket: %s\n", strerror(errno));
		goto unmap_mem;
	}

#if 0
	// No need to bind the slave socket
	snprintf(addr.sun_path, sizeof(addr.sun_path), "/tmp/shm-consumer%ld-%p.socket", (long) getpid(), c);
	if (bind(sock, (const struct sockaddr *) &addr, sizeof(addr)) < 0) {
		printf("Could not find consumer socket to: %s: %s\n", addr.sun_path, strerror(errno));
	}
#endif

	strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);

	if (connect(sock, (const struct sockaddr *) &addr, sizeof(addr)) < 0) {
		printf("Couldn't connect local socket to %s: %s\n", sock_path, strerror(errno));
		goto close_sock;
	}

	if (write(sock, &ipc, sizeof(ipc)) != sizeof(ipc)) {
		printf("Could not write handshake to socket: %s\n", strerror(errno));
		goto close_sock;
	}


	c->sock = sock;
	c->listening_sock = -1;
	c->shared = shared;
	c->data_length = size;
	c->shared_out = &shared->data[0];
	c->shared_in = &shared->data[size];
	c->peer_opened = true;

	return c;

close_sock:
	close(sock);
unmap_mem:
	munmap(shared, data_size);
out_free:
	free(c);

	return NULL;
}

int libshm_write(struct libshm_data *p, const void *data, int length)
{
	size_t bytes_to_write = min(length, p->data_length - p->bytes_used);
	size_t bytes_remaining = bytes_to_write;
	size_t w;
	const uint8_t *in_ptr = data;

	if (!p)
		return 0;

	DPRINTF("libshm_write length=%d p->data_length=%d p->bytes_used=%d\n", length, (int) p->data_length, (int) p->bytes_used);

	if (p->peer_opened && p->sock < 0) // peer has been opened and closed
		return -1;
	else if (p->sock < 0) // peer has not been opened yet, should only happen on master side
		return 0;

	if (bytes_to_write == 0)
		return 0;

	while (bytes_remaining)
	{
		w = min(bytes_remaining, p->data_length - p->write_offset);
		memcpy(p->shared->data + p->write_offset, in_ptr, w);
		in_ptr += w;
		p->write_offset += w;
		bytes_remaining -= w;

		if (p->write_offset == p->data_length)
			p->write_offset = 0;
	}

	p->bytes_used += bytes_to_write;

	struct ipc_block ipc = {
		.op = OP_WRITE,
		.nr_bytes = bytes_to_write,
		.seq = p->seq++,
	};

	int len = send(p->sock, &ipc, sizeof(ipc), 0);

	if (len != sizeof(ipc)) {
		printf("Error sending write-report (%d): %s\n", len, strerror(errno));
	}
	DPRINTF("Sending WRITE OP for %d bytes seq=%zu\n", bytes_to_write, ipc.seq);

	return bytes_to_write;
}


int libshm_nr_pollfd(struct libshm_data *d)
{
	int nr = 0;

	if (!d)
		return 0;

	if (d->listening_sock > 0)
		nr++;
	if (d->sock > 0)
		nr++;

	return nr;
}


int libshm_populate_pollfd(struct libshm_data *d, struct pollfd *pfds)
{
	int i = 0;

	if (!d)
		return 0;

	if (d->listening_sock > 0) {
		pfds[i].fd = d->listening_sock;
		pfds[i].events = POLLIN | POLLERR;
		i++;
	}

	if (d->sock > 0) {
		pfds[i].fd = d->sock;
		pfds[i].events = POLLIN | POLLERR | POLLHUP | POLLNVAL;
		i++;
	}

	return i;
}

int libshm_poll(struct libshm_data *d, struct pollfd *pfds, int nr_sock)
{
#define NR_IPC 16
	struct ipc_block ipc[NR_IPC];
	struct sockaddr_un addr;
	socklen_t address_len = sizeof(addr);
	int i, p;
	int ret = 0;

//	DPRINTF("libshm_poll: pfds=%p, nr_sock=%d\n", pfds, nr_sock);

	if (!d)
		return 0;


	for (i = 0; i < nr_sock; i++) {
#ifdef DEBUG
		if (pfds[i].fd >= 0 && pfds[i].revents) {
			printf("libshm_poll: pfds[%d].fd=%d pfds[%d].revents = %x\n", i, pfds[i].fd, i, pfds[i].revents);
		}
#endif

		if (d->listening_sock > 0 && pfds[i].fd == d->listening_sock) {
			if (pfds[i].revents & POLLIN) {
				DPRINTF("Listening socket %d\n", pfds[i].fd);
				address_len = sizeof(addr);
				int s = accept(d->listening_sock, (struct sockaddr*) &addr, &address_len);
				if (s < 0) {
					printf("Accept failed: %s\n", strerror(errno));
				}
				DPRINTF("Accept returned %d\n", s);
				d->sock = s;
			}
		}
		else if (pfds[i].fd == d->sock) {
			if (pfds[i].revents & POLLIN)
			{
				address_len = sizeof(addr);
				int len = recvfrom(d->sock, ipc, sizeof(ipc), 0, (struct sockaddr *)&addr, &address_len);
				if (len <= 0) {
					// EOF
					printf("Socket %d EOF!\n", d->sock);
					close(d->sock);
					d->sock = -1;
					continue;
				}

				for (p = 0; p < len / sizeof(struct ipc_block); p++) {
					DPRINTF("Got IPC op=%d, nr_bytes = %zu seq=%zu\n", ipc[p].op, ipc[p].nr_bytes, ipc[p].seq);
					switch (ipc[p].op)
					{
						case OP_NOP:
							d->peer_opened = true;
							DPRINTF("Peer opened! Addr len=%d family = %d path %s\n",
								(int) address_len, addr.sun_family, addr.sun_path);
							// Send IPC write for all that is in buffer currently
							break;

						case OP_READ: /* peer read from shm */
							if (ipc[p].nr_bytes > d->bytes_used) {
								printf("Too many bytes read!\n");
								d->bytes_used = 0;
							}
							else {
								d->bytes_used -= ipc[p].nr_bytes;
							}
							break;

						case OP_WRITE: /* peer wrote in to shm */
							d->bytes_available += ipc[p].nr_bytes;
							break;

						default:
							printf("Unknown IPC OP %d\n", ipc[p].op);
							break;
					}
				}
			}
			if (pfds[i].revents & (POLLHUP|POLLERR|POLLNVAL))
			{
				DPRINTF("Hang up on socket\n");
				close(d->sock);
				d->sock = -1;
				continue;
			}
		}
	}

	if (d->bytes_available > 0)
		ret |= POLLIN;

	if (d->bytes_used < d->data_length)
		ret |= POLLOUT;

	return ret;
}

int libshm_can_read(struct libshm_data *c)
{
	if (!c)
		return 0;
	return c->bytes_available;
}

int libshm_read(struct libshm_data *c, void *data, int length)
{
	uint8_t *out_ptr = data;
	size_t bytes_to_read = min(c->bytes_available, length);
	size_t bytes_remaining = bytes_to_read;
	size_t r;

	if (!c)
		return 0;

	DPRINTF("%s length=%d c->bytes_available=%d\n", __FUNCTION__, length, (int)c->bytes_available);
	if (c->bytes_available == 0 && c->sock < 0)
		return -1; // EOF

	if (bytes_to_read == 0)
		return 0;

	while (bytes_remaining)
	{
		r = min(bytes_remaining, c->data_length - c->read_offset); // read up to end of buffer
		memcpy(out_ptr, c->shared->data + c->read_offset, r);
		out_ptr += r;
		c->read_offset += r;
		bytes_remaining -= r;

		if (c->read_offset == c->data_length)
			c->read_offset = 0;
	}

	c->bytes_available -= bytes_to_read;

	struct ipc_block ipc = {
		.op = OP_READ,
		.nr_bytes = bytes_to_read,
		.seq = c->seq++,
	};

	int len = send(c->sock, &ipc, sizeof(ipc), 0); // send read-report to producer
	if (len != sizeof(ipc)) {
		printf("Error sending read-report, ret=%d: %s\n", len, strerror(errno));
	}
	DPRINTF("Sending READ OP for %d bytes seq=%zu\n", (int)bytes_to_read, ipc.seq);
		
	return bytes_to_read;
}

void hexdump(uint8_t *data, size_t len)
{
	while(len >= 16)
	{
		printf("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
		       data[0], data[1], data[2], data[3],
		       data[4], data[5], data[6], data[7],
		       data[8], data[9], data[10], data[11],
		       data[12], data[13], data[14], data[15]);
		len -= 16;
		data += 16;
	}
	while(len--)
		printf("%02x ", *data++);

	printf("\n");
}

int libshm_close(struct libshm_data *d)
{
	size_t data_size = sizeof(shared_data_t) + 2 * d->data_length;

	munmap(d->shared, data_size);

	if (d->sock >= 0)
		close(d->sock);
	if (d->listening_sock >= 0)
		close(d->listening_sock);

	if (d->listening_socket_path) {
		unlink(d->listening_socket_path);
		free(d->listening_socket_path);
	}

	if (d->shm_path) {
		shm_unlink(d->shm_path);
		free(d->shm_path);
	}

	free(d);

	return 0;
}

static int libshm_poll_internal(struct libshm_data *shm)
{
	int err = 0;

	do {
		int nr_pollfd = libshm_nr_pollfd(shm);
		struct pollfd pfds[nr_pollfd];

		libshm_populate_pollfd(shm, pfds);

		if ((err = poll(pfds, nr_pollfd, -1)) < 0) {
			if (errno == EINTR)
				continue;
			return -1; // Bad things happened, signal EOF
		}

		if ((err = libshm_poll(shm, pfds, nr_pollfd)) < 0) {
			return -1; // EOF
		}
	} while (err < 0);

	return err;
}

int libshm_write_all(struct libshm_data *shm, const void *data, int length)
{
	const uint8_t *head = (const uint8_t *)data;
	int len = length;

	if (!shm)
		return 0;

	while(len > 0) {
		int bytes_written = libshm_write(shm, head, len);
		if (bytes_written < 0) {
			return -1; //EOF
		}
		len -= bytes_written;
		head += bytes_written;

		if (len > 0) { // Wait for more space in SHM
			if (libshm_poll_internal(shm) < 0)
				return -1;
		}
	}

	return length;
}

int libshm_read_all(struct libshm_data *shm, void *data, int length)
{
	uint8_t *head = (uint8_t *)data;
	int len = length;

	if (!shm)
		return 0;

	while(len > 0) {
		int bytes_read = libshm_read(shm, head, len);
		if (bytes_read < 0) {
			return -1; //EOF
		}
		len -= bytes_read;
		head += bytes_read;

		if (len > 0) { // Wait for more data in to SHM
			if (libshm_poll_internal(shm) < 0)
				return -1;
		}
	}

	return length;
}
