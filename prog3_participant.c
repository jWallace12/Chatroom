/* prog3_participant.c - client that sends message to server */

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
* Program: prog3_participant
*
* Purpose: allocate a socket, connect to a server, send user input to be read by observers
*
* Syntax: ./prog3_participant server_address server_port
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
	char username[255]; /* participant username */
	uint8_t userLen = 0; /* length of username */
	char connectionAccept[1]; /* answer if connected to server */
	char usernameAccept[1]; /* answer if username was valid and accepted */

	memset(usernameAccept, 1, 0);
	memset(buf, sizeof(buf), 0);
	memset(username, 0, sizeof(username));

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

	memcpy(&sad.sin_addr, ptrh->h_addr, ptrh->h_length);

	/* Map TCP transport protocol name to protocol number. */
	if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
		fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
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
	// continue if accepted by server
	if (connectionAccept[0] == 'Y') {
		// loop infinitely while username isn't valid
		while (usernameAccept[0] != 'Y') {
			bool pass = true;
			// loop infinitely while username is too long
			while (pass) {
				printf("Enter username: ");
				scanf("%s", username);
				userLen = strlen(username);
				// try another name if too long
				if (userLen > 10) {
					printf("Username is too long. Try again.\n");
					userLen = 0;
					memset(username, 0, sizeof(username));
				} else {
					pass = false;
				}
			}
			send(sd, &userLen, 1, 0);
			send(sd, username, strlen(username), 0);
			memset(username, 0, sizeof(username));
			recv(sd, &usernameAccept, 1, 0);
			// try another name if it was invalid
			if (usernameAccept[0] == 'I') {
				printf("Invalid name. Try again.\n");
				memset(username, 0, sizeof(username));
				userLen = 0;
				// try another name if it was already taken by another user
			} else if (usernameAccept[0] == 'T') {
				printf("Username already taken. Try again.\n");
				memset(username, 0, sizeof(username));
				userLen = 0;
			}
		}
		fgets(buf, sizeof(buf), stdin);
		// infinitely send messages
		while (1) {
			uint16_t msgLen;
			// recieve user input
			printf("Enter message: ");
			fgets(buf, sizeof(buf), stdin);
			msgLen = strlen(buf);
			// try another message if it was too long
			if (msgLen > 1000) {
				printf("Message was too long. Try again.\n");
				memset(buf, 0, sizeof(buf));
				// else send the message to the server
			} else {
				uint16_t networkLen = htons(msgLen);
				send(sd, &networkLen, sizeof(networkLen), 0);
				send(sd, buf, sizeof(buf), 0);
				msgLen = 0;
				memset(buf, 0, sizeof(buf));
			}
		}
		// close out if the server is too full
	} else {
		close(sd);
		exit(EXIT_SUCCESS);
	}
}
