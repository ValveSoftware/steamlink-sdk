#include <gnutls/gnutls.h>

typedef struct {
	int fd;
	gnutls_session_t session;
	int secure;
	char *hostname;
	char *ip;
	char *service;
	struct addrinfo *ptr;
	struct addrinfo *addr_info;
	int verbose;
} socket_st;

ssize_t socket_recv(const socket_st * socket, void *buffer,
		    int buffer_size);
ssize_t socket_send(const socket_st * socket, const void *buffer,
		    int buffer_size);
ssize_t socket_send_range(const socket_st * socket, const void *buffer,
			  int buffer_size, gnutls_range_st * range);
void socket_open(socket_st * hd, const char *hostname, const char *service,
		 int udp, const char *msg);

void socket_starttls(socket_st * hd, const char *app_proto);
void socket_bye(socket_st * socket);

void sockets_init(void);

int service_to_port(const char *service, const char *proto);
const char *port_to_service(const char *sport, const char *proto);

#define CONNECT_MSG "Connecting to"
