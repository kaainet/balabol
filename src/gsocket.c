/**
 * gsocket.c
 * 
 * Created by Kainet
 *
 * Non-blocking connection to google speech api
 */

#include <kaitalk/gsocket.h>

// response buffer size
#define SOCKET_READ_BUFFER 400
#define SOCKET_HOST "www.google.com"
#define SOCKET_PORT "80"

/**
 * Recieve sockaddr IPv4 or IPv6
 */
void *kaitalk_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa)->sin_addr);

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/**
 * Establish non-blocking socket connection to google
 */
int kaitalk_socket_connect() {
	int sockfd, resolv;
	struct addrinfo hints, *servinfo, *p;

	// Prepearing hints struct
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	// filling servinfo structure
	if ((resolv = getaddrinfo(SOCKET_HOST, SOCKET_PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(resolv));
		return -1;
	}

	// loop through all the servinfo results
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}
		
		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		// set socket to non blocking
		fcntl(sockfd, F_SETFL, O_NONBLOCK);

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return -2;
	}

	freeaddrinfo(servinfo); // no need anymore

	return sockfd;
}

/**
 * Send buffer to socket
 */
int kaitalk_socket_send(int sockfd, char** buffer, int buffer_len) {
	int sock_numbytes;

	if ((sock_numbytes=send(sockfd, *buffer, buffer_len, 0)) == -1) {
		printf("socket send error\n");
		return -1;
	}

	return sock_numbytes;
}

/**
 * Read data from socket in non-blocking mode
 */
int kaitalk_socket_read(int sockfd, char** buffer) {
	fd_set master,read_flags;
	struct timeval waitd;
	int sock_numbytes, offset;
	char internal_buf[SOCKET_READ_BUFFER];

	waitd.tv_sec = 1;  // Make select wait up to 1 second for data
	waitd.tv_usec = 0; // and 0 milliseconds.

	// non blocking sets
	FD_ZERO(&master);
	FD_SET(sockfd, &master);

	//input buffer offest
	offset = 0;

	while (1) {
		read_flags = master;

		if (select(sockfd+1, &read_flags, NULL, NULL, &waitd) < 0) {
			printf("socket select error\n");
			return -1;
		}

		// Check if data is available to read
		if (FD_ISSET(sockfd, &read_flags)) {

			FD_CLR(sockfd, &read_flags);

			// flush internal buffer
			memset(&internal_buf, 0, sizeof(char) * SOCKET_READ_BUFFER);

			if ((sock_numbytes = recv(sockfd, internal_buf, SOCKET_READ_BUFFER, 0)) < 0) {
				printf("socket recv error\n");
				return -1;
			}

			// fill input buffer
			memcpy((*buffer) + offset, internal_buf, sock_numbytes);

			offset += sock_numbytes;

		} else
			break;
	}

	(*buffer)[offset] = '\0';

	return offset+1;
}

/**
 * Send hdl command via udp socket
 */
int kaitalk_create_hdl_send_cmd(char *addr, int addr_port, unsigned char **data, int data_len) {
	int sockfd;
	struct sockaddr_in their_addr; // connector's address information
	struct hostent *he;
	int numbytes;
	int broadcast = 1;

	if ((he=gethostbyname(addr)) == NULL) {  // get the host info
		perror("gethostbyname");
		return -1;
	}

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("socket");
		return -1;
	}

	// this call is what allows broadcast packets to be sent:
	if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof broadcast) == -1) {
		perror("setsockopt (SO_BROADCAST)");
		return -1;
	}

	their_addr.sin_family = AF_INET; // host byte order
	their_addr.sin_port = htons(addr_port); // short, network byte order
	their_addr.sin_addr = *((struct in_addr *)he->h_addr);
	memset(their_addr.sin_zero, '\0', sizeof their_addr.sin_zero);

	if ((numbytes=sendto(sockfd, *data, data_len, 0, (struct sockaddr *)&their_addr, sizeof their_addr)) == -1) {
		perror("sendto");
		exit(1);
	}

	close(sockfd);

	return numbytes;
}
