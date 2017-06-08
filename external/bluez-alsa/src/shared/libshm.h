#pragma once
#include <stdbool.h>
#include <stddef.h>

struct libshm_data;
struct pollfd;

/* Create a master node for the shm transport.
 * Master is responsible for creating the connection points for the slave and it has to be
 * done first before libsum_create_slave() is called.
 *
 * SHM transport is bi-directional, with size being the same for both directions
 *
 * @param sock_path path for the AF_UNIX socket used for communication between nodes
 * @param mem_path  path for the shared memory file that is mapped
 * @param size      size of the shared region in bytes, per direction
 */
struct libshm_data* libshm_create_master(const char *sock_path, const char *mem_path, size_t size);
struct libshm_data* libshm_create_slave(const char *sock_path, const char *mem_path, size_t size);

/* Close the transport and deallocate the structure */
int libshm_close(struct libshm_data*);

/* Write data in to shm transport, returns number of bytes written */
int libshm_write(struct libshm_data *d, const void *data, int length);
/* Read data from the shm transport, returns number of bytes read */
int libshm_read(struct libshm_data *d, void *data, int length);

/* Blocks. Write write exactly <length> bytes in to SHM transport.
 * Will call poll() etc. internally */
int libshm_write_all(struct libshm_data *d, const void *data, int length);

/* Blocks. Read exactly <length> bytes from SHM transport.
 * Will call poll() etc. internally */
int libshm_read_all(struct libshm_data *d, void *data, int length);

/* How many bytes are in the outgoing buffer */
int libshm_bytes_outgoing(struct libshm_data *);
/* How many bytes are in the incoming buffer */
int libshm_bytes_incoming(struct libshm_data *);

/* Returns the number of file descriptors required by the shm node */
int libshm_nr_pollfd(struct libshm_data *);
/* Populate the pollfd structure(s) */
int libshm_populate_pollfd(struct libshm_data *, struct pollfd *);
/* Function to handle poll() for sockets */
int libshm_poll(struct libshm_data *, struct pollfd *, int nr_pollfd);

void hexdump(uint8_t *data, size_t len);
