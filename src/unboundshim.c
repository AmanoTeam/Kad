#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "unboundshim.h"
#include "logging.h"

#if defined(_WIN32)
	#include <winsock2.h>
	#include <ws2tcpip.h>
	
	typedef int socklen_t;
#else
	#include <errno.h>
	
	#include <arpa/inet.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
#endif

#include <unbound.h>
#include <unbound-event.h>

#define USHIM_MAX_QUERY (4096)

#define USHIM_RCODE_SERVFAIL (2)

#define USHIM_INVALID_SOCK ((evutil_socket_t) -1)

struct shim_query {
	struct sockaddr_storage client;
	socklen_t client_len;
	
	uint16_t id;
	size_t query_len;
	size_t question_end;
	uint8_t query[USHIM_MAX_QUERY];
};

static struct ub_ctx* shim_ctx = NULL;
static evutil_socket_t shim_sock = USHIM_INVALID_SOCK;
static uint16_t shim_port = 0;

static struct event* shim_sock_event = NULL;

static const char* shim_sock_error(void) {
	
	#if defined(_WIN32)
		return evutil_socket_error_to_string(WSAGetLastError());
	#else
		return strerror(errno);
	#endif
	
}

static int shim_sock_would_block(void) {
	
	#if defined(_WIN32)
		return WSAGetLastError() == WSAEWOULDBLOCK;
	#else
		return errno == EAGAIN || errno == EWOULDBLOCK;
	#endif
	
}

static int parse_query(
	const uint8_t* const pkt,
	const size_t len,
	char* const name,
	const size_t namesz,
	uint16_t* const qtype,
	uint16_t* const qclass,
	size_t* const question_end
) {
	
	size_t pos = 12;
	size_t nlen = 0;
	
	if (len < 12 || (pkt[4] == 0 && pkt[5] == 0)) {
		return -1;
	}
	
	for (;;) {
		uint8_t llen = 0;
		
		if (pos >= len) {
			return -1;
		}
		
		llen = pkt[pos++];
		
		if (llen == 0) {
			break;
		}
		
		if ((llen & 0xC0) != 0 || (pos + llen) > len) {
			return -1;
		}
		
		if ((nlen + llen + 2) > namesz) {
			return -1;
		}
		
		memcpy(name + nlen, pkt + pos, llen);
		
		nlen += llen;
		name[nlen++] = '.';
		
		pos += llen;
	}
	
	if (nlen == 0 || (pos + 4) > len) {
		return -1;
	}
	
	name[nlen - 1] = '\0';
	
	*qtype = (uint16_t) ((pkt[pos] << 8) | pkt[pos + 1]);
	*qclass = (uint16_t) ((pkt[pos + 2] << 8) | pkt[pos + 3]);
	*question_end = pos + 4;
	
	return 0;
	
}

static void send_error(const struct shim_query* const query, const uint8_t rcode) {
	
	uint8_t pkt[USHIM_MAX_QUERY];
	size_t len = query->question_end;
	uint16_t flags = 0;
	
	if (len < 12 || len > query->query_len) {
		return;
	}
	
	flags = (uint16_t) (
		0x8000u
		| (((uint16_t) query->query[2] << 8) & 0x0100u)
		| 0x0080u
		| (rcode & 0x0Fu)
	);
	
	memcpy(pkt, query->query, len);
	
	pkt[2] = (uint8_t) (flags >> 8);
	pkt[3] = (uint8_t) (flags & 0xFF);
	
	
	memset(pkt + 6, 0, 6);
	
	if (sendto(shim_sock, (const char*) pkt, (int) len, 0, (const struct sockaddr*) &query->client, query->client_len) == -1) {
		loggln(LOG_WARN, "[unbound-shim] could not send error response: %s", shim_sock_error());
	}
	
}

static void resolve_callback(
	void* const arg,
	const int rcode,
	void* const packet,
	const int packet_len,
	const int sec,
	char* const why_bogus,
	const int was_ratelimited
) {
	
	struct shim_query* const query = (struct shim_query* const) arg;
	
	(void) was_ratelimited;
	
	if (rcode == 0 && sec != 1 && packet != NULL && packet_len >= 12) {
		uint8_t* const pkt = malloc((size_t) packet_len);
		
		if (pkt != NULL) {
			memcpy(pkt, packet, (size_t) packet_len);
			
			pkt[0] = (uint8_t) (query->id >> 8);
			pkt[1] = (uint8_t) (query->id & 0xFF);
			
			if (sendto(shim_sock, (const char*) pkt, (int) packet_len, 0, (const struct sockaddr*) &query->client, query->client_len) == -1) {
				loggln(LOG_WARN, "[unbound-shim] could not send answer: %s", shim_sock_error());
			}
			
			free(pkt);
		}
	}
	else {
		if (sec == 1 && why_bogus != NULL) {
			loggln(LOG_VERBOSE, "[unbound-shim] bogus answer rejected: %s", why_bogus);
		}
		
		send_error(query, USHIM_RCODE_SERVFAIL);
	}
	
	free(query);
	
}

static void shim_udp_callback(evutil_socket_t fd, const short events, void* const arg) {
	
	static uint8_t buf[65536];
	
	(void) events;
	(void) arg;
	
	for (;;) {
		struct sockaddr_storage client = {0};
		socklen_t client_len = sizeof(client);
		
		struct shim_query* query = NULL;
		
		char name[256] = {0};
		uint16_t qtype = 0;
		uint16_t qclass = 0;
		
		int rc = 0;
		
		const int size = recvfrom(fd, (char*) buf, (int) sizeof(buf), 0, (struct sockaddr*) &client, &client_len);
		
		if (size == -1) {
			if (!shim_sock_would_block()) {
				loggln(LOG_WARN, "[unbound-shim] could not read from socket: %s", shim_sock_error());
			}
			
			break;
		}
		
		if (size > USHIM_MAX_QUERY) {
			loggln(LOG_VERBOSE, "[unbound-shim] dropping oversized query of %d bytes", size);
			
			continue;
		}
		
		query = calloc(1, sizeof(*query));
		
		if (query == NULL) {
			continue;
		}
		
		if (parse_query(buf, (size_t) size, name, sizeof(name), &qtype, &qclass, &query->question_end) != 0) {
			loggln(LOG_VERBOSE, "[unbound-shim] dropping malformed query of %d bytes", size);
			
			free(query);
			
			continue;
		}
		
		query->client = client;
		query->client_len = client_len;
		query->id = (uint16_t) ((buf[0] << 8) | buf[1]);
		query->query_len = (size_t) size;
		
		memcpy(query->query, buf, (size_t) size);
		
		rc = ub_resolve_event(shim_ctx, name, qtype, qclass, query, resolve_callback, NULL);
		
		if (rc != 0) {
			loggln(LOG_WARN, "[unbound-shim] ub_resolve_event() failed for %s: %s", name, ub_strerror(rc));
			
			send_error(query, USHIM_RCODE_SERVFAIL);
			free(query);
			
			continue;
		}
		
		loggln(LOG_VERBOSE, "[unbound-shim] resolving %s (type %u, class %u)", name, qtype, qclass);
	}
	
}

uint16_t unbound_shim_start(struct event_base* const event_base) {
	
	
	static const struct {
		const char* key;
		const char* value;
	} options[] = {
		{"verbosity:", "0"},
		{"log-servfail:", "yes"},
		{"hide-version:", "yes"},
		{"hide-identity:", "yes"},
		{"hide-trustanchor:", "yes"},
		{"do-ip4:", "yes"},
		{"do-ip6:", "no"},
		{"prefer-ip6:", "no"},
		{"do-udp:", "yes"},
		{"do-tcp:", "yes"},
		{"tcp-upstream:", "no"},
		{"udp-upstream-without-downstream:", "no"},
		{"tcp-idle-timeout:", "15000"},
		{"qname-minimisation:", "yes"},
		{"qname-minimisation-strict:", "no"},
		{"deny-any:", "yes"},
		{"use-caps-for-id:", "yes"},
		{"minimal-responses:", "yes"},
		{"aggressive-nsec:", "yes"},
		{"harden-dnssec-stripped:", "yes"},
		{"edns-tcp-keepalive:", "no"},
		{"statistics-interval:", "0"},
		{"statistics-cumulative:", "no"},
		{"extended-statistics:", "no"},
		{"msg-cache-size:", "10m"},
		{"rrset-cache-size:", "50m"},
		{"cache-max-ttl:", "3600"},
		{"cache-min-ttl:", "1800"},
		{"infra-host-ttl:", "1000"},
		{"infra-cache-min-rtt:", "50"},
		{"prefetch:", "yes"},
		{"trust-anchor-signaling:", "no"}
	};
	
	
	static const char* const trust_anchors[] = {
		". IN DS 20326 8 2 E06D44B80B8F1D39A95C0B0D7C65D08458E880409BBC683457104237C7F8EC8D",
		". IN DS 38696 8 2 683D2D0ACB8C9B712A1948B27F741219298D0A450D612C483AF444A4C0FB2B16"
	};
	
	evutil_socket_t sock = USHIM_INVALID_SOCK;
	struct sockaddr_in addr = {0};
	socklen_t addr_len = sizeof(addr);
	
	size_t index = 0;
	
	if (shim_ctx != NULL) {
		loggln(LOG_WARN, "[unbound-shim] the embedded resolver is already running");
		
		return shim_port;
	}
	
	shim_ctx = ub_ctx_create_event(event_base);
	
	if (shim_ctx == NULL) {
		loggln(LOG_ERROR, "[unbound-shim] could not create resolver context");
		
		goto error;
	}
	
	for (index = 0; index < (sizeof(options) / sizeof(*options)); index++) {
		if (ub_ctx_set_option(shim_ctx, options[index].key, options[index].value) != 0) {
			loggln(LOG_ERROR, "[unbound-shim] could not set option %s", options[index].key);
			
			goto error;
		}
	}
	
	for (index = 0; index < (sizeof(trust_anchors) / sizeof(*trust_anchors)); index++) {
		if (ub_ctx_set_option(shim_ctx, "trust-anchor:", trust_anchors[index]) != 0) {
			loggln(LOG_ERROR, "[unbound-shim] could not set the root trust anchors");
			
			goto error;
		}
	}
	
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	
	if (sock == USHIM_INVALID_SOCK) {
		loggln(LOG_ERROR, "[unbound-shim] could not create socket: %s", shim_sock_error());
		
		goto error;
	}
	
	if (evutil_make_socket_nonblocking(sock) == -1) {
		loggln(LOG_ERROR, "[unbound-shim] could not set socket to non-blocking mode: %s", shim_sock_error());
		
		goto error;
	}
	
	{
		const int recv_buffer_size = 262144;
		
		if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const void*) &recv_buffer_size, (socklen_t) sizeof(recv_buffer_size)) == -1) {
			loggln(LOG_ERROR, "[unbound-shim] could not enlarge the receive socket buffer: %s", shim_sock_error());
			
			goto error;
		}
	}
	
	addr.sin_family = AF_INET;
	addr.sin_port = 0;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	
	if (bind(sock, (struct sockaddr*) &addr, (int) sizeof(addr)) == -1) {
		loggln(LOG_ERROR, "[unbound-shim] could not bind socket: %s", shim_sock_error());
		
		goto error;
	}
	
	if (getsockname(sock, (struct sockaddr*) &addr, &addr_len) == -1) {
		loggln(LOG_ERROR, "[unbound-shim] could not get socket address: %s", shim_sock_error());
		
		goto error;
	}
	
	shim_sock_event = event_new(event_base, sock, EV_READ | EV_PERSIST, shim_udp_callback, NULL);
	
	if (shim_sock_event == NULL) {
		loggln(LOG_ERROR, "[unbound-shim] could not create socket event");
		
		goto error;
	}
	
	event_add(shim_sock_event, NULL);
	
	shim_sock = sock;
	shim_port = ntohs(addr.sin_port);
	
	return shim_port;
	
error:
	
	if (shim_sock_event != NULL) {
		event_free(shim_sock_event);
		shim_sock_event = NULL;
	}
	
	if (sock != USHIM_INVALID_SOCK) {
		evutil_closesocket(sock);
	}
	
	if (shim_ctx != NULL) {
		ub_ctx_delete(shim_ctx);
		shim_ctx = NULL;
	}
	
	shim_sock = USHIM_INVALID_SOCK;
	shim_port = 0;
	
	return 0;
	
}

void unbound_shim_stop(void) {
	
	if (shim_ctx == NULL) {
		return;
	}
	
	event_free(shim_sock_event);
	shim_sock_event = NULL;
	
	evutil_closesocket(shim_sock);
	
	shim_sock = USHIM_INVALID_SOCK;
	shim_port = 0;
	
	ub_ctx_delete(shim_ctx);
	shim_ctx = NULL;
	
}