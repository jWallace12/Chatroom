/* prog3_observer.c */
/* Jonah Wallace */
/* WWU - CSCI 367 */
/* Used code from demo_client.c */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/*------------------------------------------------------------------------
* Program: demo_client
*
* Purpose: allocate a socket, connect to a server, and print all output
*
* Syntax: ./demo_client server_address server_port
*
* server_address - name of a computer on which server is executing
* server_port    - protocol port number server is using
*
*------------------------------------------------------------------------
*/
int main( int argc, char **argv) {
	struct hostent *ptrh; /* pointer to a host table entry */
	struct protoent *ptrp; /* pointer to a protocol table entry */
	struct sockaddr_in sad; /* structure to hold an IP address */
	int sd; /* socket descriptor */
	int port; /* protocol port number */
	char *host; /* pointer to host name */
	int n; /* number of characters read */
	char buf[1100]; /* buffer for data from the server */
	char connectionAccept[1];
	uint8_t userLen = -1;
	uint16_t networkLen;
	uint16_t msgLen;
	char partAccept[1]; // answer if username was accepted
	char partName[255];
	for (int i = 0; i < 255; i++){
		partName[i] = '+';
	}
	memset(buf,0,sizeof(buf));

	memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
	sad.sin_family = AF_INET; /* set family to Internet */

	if( argc != 3 ) {
		fprintf(stderr,"Error: Wrong number of arguments\n");
		fprintf(stderr,"usage:\n");
		fprintf(stderr,"./client server_address server_port\n");
		exit(EXIT_FAILURE);
	}

	port = atoi(argv[2]); /* convert to binary */
	if (port > 0) /* test for legal value */
		sad.sin_port = htons((u_short)port);
	else {
		fprintf(stderr,"Error: bad port number %s\n",argv[2]);
		exit(EXIT_FAILURE);
	}

	host = argv[1]; /* if host argument specified */

	/* Convert host name to equivalent IP address and copy to sad. */
	ptrh = gethostbyname(host);
	if ( ptrh == NULL ) {
		fprintf(stderr,"Error: Invalid host: %s\n", host);
		exit(EXIT_FAILURE);
	}

	memcpy(&sad.sin_addr, ptrh->h_addr, ptrh->h_length);

	/* Map TCP transport protocol name to protocol number. */
	if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
		fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
		exit(EXIT_FAILURE);
	}

	/* Create a socket. */
	sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if (sd < 0) {
		fprintf(stderr, "Error: Socket creation failed\n");
		exit(EXIT_FAILURE);
	}

	/* Connect the socket to the specified server. */
	if (connect(sd, (struct sockaddr *)&sad, sizeof(sad)) < 0) {
		fprintf(stderr,"connect failed\n");
		exit(EXIT_FAILURE);
	}

	recv(sd, &connectionAccept, 1, 0);
	// go on if connected
	if (connectionAccept[0] == 'Y') {
		while (true) {
			bool pass = true;
			// continue until name is right length
			while (pass) {
				printf("Enter participant to be affiliated with: ");
				scanf("%s", partName);
				for (int i = 0; i < 255; i++){
					if (partName[i] != '+') {
						userLen++;
					} else {
						break;
					}
				}
				// username is too long, re enter
				if (userLen > 10) {
					printf("Username is too long. Try again.\n");
					userLen = -1;
					for (int i = 0; i < 255; i++) {
						partName[i] = '+';
					}
				} else {
					pass = false;
				}
			}

			// send attempted affiliation
			send(sd, &userLen, 1, 0);
			send(sd, partName, strlen(partName), 0);
			memset(partName, 0, sizeof(partName));
			recv(sd, &partAccept, 1, 0);
			// try again if received T
			if (partAccept[0] == 'T') {
				printf("An observer has already been affiliated with this participant. Try again.\n");
				for (int i = 0; i < 255; i++) {
					partName[i] = '+';
				}
				userLen = -1;
				// closed if received N
			} else if (partAccept[0] == 'N') {
				close(sd);
				exit(EXIT_FAILURE);
				// else receive messages!
			} else {
				break;
			}
		}
		// receive messages
		while (n > 0) {
			n = recv(sd, &networkLen, sizeof(networkLen), 0);
			msgLen = ntohs(networkLen);
			recv(sd, buf, sizeof(buf), 0);
			// print newline in one is not provided
			if (buf[msgLen-1] != '\n') {
				printf("%s\n", buf);
			} else {
				printf("%s", buf);
			}
			memset(buf, 0, sizeof(buf));
		}
	}

	close(sd);

	exit(EXIT_SUCCESS);
}
