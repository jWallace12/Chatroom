/* prog3_server.c */
/* Jonah Wallace */
/* WWU - CSCI 367 */
/* Used code from demo_server.c */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>

#define QLEN 6 /* size of request queue */
#define maxClients 255 /* maximum number of each type of client */

/*------------------------------------------------------------------------
* Program: prog3_server
* Purpose: Act as an active chat room between users
*
* Purpose: recieve input for participants, send output to observers
*
* Syntax: ./demo_server participantPort observerPort
*
* port - protocol port number to use
*
*------------------------------------------------------------------------
*/


int main(int argc, char **argv) {
	struct protoent *ptrp; /* pointer to a protocol table entry */
	struct sockaddr_in sad1; /* structure to hold server's address */
	struct sockaddr_in sad2; /* structure to hold other address */
	struct sockaddr_in cad1; /* structure to hold participant address */
	struct sockaddr_in cad2; /* structure to hold observer address */
	int participantSocket, observerSocket; /* socket descriptors */
	int participantPort, observerPort; /* protocol port numbers */
	int alen; /* length of address */
	int optval = 1; /* boolean value when we set socket option */
	char buf[1100]; /* buffer for string the server receives */
	int numParts = 0;
	int numObs = 0;
	int numPartTimers = 0;
	int numObsTimers = 0;
	fd_set chief;
	fd_set reader;
	uint8_t affLen = 1;
	uint16_t netLen;
	uint16_t msgLen;
	char connectionAcceptPart[1];
	char connectionAcceptObs[1];
	char validUser[1];
	char validAff[1];
	FD_ZERO(&chief);
	memset(buf, 0, sizeof(buf));

	// struct for timer
	struct timeval timer;
	timer.tv_sec = 60;
	timer.tv_usec = 0;

	// participant struct
	typedef struct partSet {
		int socket;
		int nameLength;
		char partName[255];
		int aff;
		struct timeval userTimer;
	}partSet;

	// observer struct
	typedef struct obsSet {
		int socket;
		char affPart[255];
		struct timeval userTimer;
	}obsSet;


	partSet participantList[maxClients]; // set of participants
	obsSet observerList[maxClients]; // set of observers


	// initialize clients
	for (int i = 0; i < maxClients; i++) {
		participantList[i].socket = 0;
		participantList[i].nameLength = 0;
		participantList[i].aff = 0;
		participantList[i].userTimer.tv_sec = 60;
		participantList[i].userTimer.tv_usec = 0;
		observerList[i].socket = 0;
		observerList[i].userTimer.tv_sec = 60;
		observerList[i].userTimer.tv_usec = 0;
	}

	// check if participant username is valid
	char validateUsername(int userLen, char thisUsername[], partSet participantList[]) {
		for (int i = 0; i < userLen; i++) {
			// check if username contains only letters, digits, and/or underscores
			if (!((isalpha(thisUsername[i])) || (isdigit(thisUsername[i])) || (thisUsername[i] == '_'))) {
				return 'I';
			}
		}
		// check if username already exists
		for (int j = 0; j < maxClients; j++) {
			if (userLen == participantList[j].nameLength) {
				if (strcmp(thisUsername, participantList[j].partName) == 0) {
					return 'T';
				}
			}
		}
		return 'Y';
	}

	// check if observer can affiliate with username
	char validateAffiliation(char affUsername[], int affLen, int obsSocket) {
		bool sameName;
		// check if there is a username that matches
		for (int i = 0; i < maxClients; i++) {
			if (participantList[i].nameLength == affLen) {
				sameName = true;
				for (int j = 0; j < affLen; j++) {
					if (participantList[i].partName[j] == affUsername[j]) {
						continue;
					} else {
						sameName = false;
						break;
					}
				}
				// if names matches...
				if (sameName) {
					// check if participant already has an affiliation
					if (participantList[i].aff > 0) {
						return 'T';
					} else {
						participantList[i].aff = obsSocket;
						return 'Y';
					}
				}
			}
		}
		return 'N';
	}

	// broad to observers a new participant has joined
	void newParticipant(int socket, char thisUser[]) {
		int length = strlen(thisUser);
		uint16_t msgLen = 16+length;
		// create message
		char newUser[msgLen];
		newUser[0] = 'U';
		newUser[1] = 's';
		newUser[2] = 'e';
		newUser[3] = 'r';
		newUser[4] = ' ';
		int iter = 0;
		for (int i = 5; i < length+5; i++) {
			newUser[i] = thisUser[iter];
			iter++;
		}
		newUser[5+length] = ' ';
		newUser[6+length] = 'h';
		newUser[7+length] = 'a';
		newUser[8+length] = 's';
		newUser[9+length] = ' ';
		newUser[10+length] = 'j';
		newUser[11+length] = 'o';
		newUser[12+length] = 'i';
		newUser[13+length] = 'n';
		newUser[14+length] = 'e';
		newUser[15+length] = 'd';
		newUser[16+length] = ' ';
		uint16_t networkLen = htons(msgLen);
		// send to active users
		for (int j = 0; j < maxClients; j++) {
			if (observerList[j].socket > 0) {
				send(observerList[j].socket, &networkLen, sizeof(networkLen), MSG_DONTWAIT | MSG_NOSIGNAL);
				send(observerList[j].socket, newUser, msgLen, MSG_DONTWAIT | MSG_NOSIGNAL);
			}
		}
	}

	// disconnect affiliation from participant
	void closeAff(int socket) {
		for (int i = 0; i < maxClients; i++) {
			if (observerList[i].socket == socket) {
				numObs--;
				observerList[i].socket = 0;
				FD_CLR(socket, &chief);
				close(socket);
			}
		}
	}

	// check if non-active participant closed itself
	bool checkClosedPartNonActive(int rec, int socket, partSet participantList[]) {
		// if closed...
		if (rec < 1) {
			// remove
			for (int i = 0; i < maxClients; i++) {
				if (participantList[i].socket == socket) {
					numParts--;
					participantList[i].socket = 0;
					close(socket);
					FD_CLR(socket, &chief);
					return false;
				}
			}
			// else, continue
		} else {
			return true;
		}
	}

	// check if active participant closed itself
	bool checkClosedPart(int rec, int socket, char thisUsername[]) {
		// if closed
		if (rec < 1) {
			// remove
			for (int i = 0; i < maxClients; i++) {
				if (participantList[i].socket == socket) {
					uint8_t thisLen = participantList[i].nameLength;
					uint16_t msgLen = 14+thisLen;
					// create message
					char closedUser[msgLen];
					closedUser[0] = 'U';
					closedUser[1] = 's';
					closedUser[2] = 'e';
					closedUser[3] = 'r';
					closedUser[4] = ' ';
					int iter = 0;
					for (int j = 5; j < thisLen+5; j++) {
						closedUser[j] = thisUsername[iter];
						iter++;
					}
					closedUser[5+thisLen] = ' ';
					closedUser[6+thisLen] = 'h';
					closedUser[7+thisLen] = 'a';
					closedUser[8+thisLen] = 's';
					closedUser[9+thisLen] = ' ';
					closedUser[10+thisLen] = 'l';
					closedUser[11+thisLen] = 'e';
					closedUser[12+thisLen] = 'f';
					closedUser[13+thisLen] = 't';
					closedUser[14+thisLen] = ' ';
					participantList[i].socket = 0;
					participantList[i].nameLength = 0;
					memset(participantList[i].partName, 0, sizeof(participantList[i].partName));
					// clear data
					numParts--;
					close(socket);
					participantList[i].socket = 0;
					if (participantList[i].aff > 0) {
						closeAff(participantList[i].aff);
					}
					participantList[i].aff = 0;
					uint16_t networkLen = htons(msgLen);
					for (int l = 0; l < maxClients; l++) {
						if (observerList[l].socket > 0) {
							send(observerList[l].socket, &networkLen, sizeof(networkLen), MSG_DONTWAIT | MSG_NOSIGNAL);
							send(observerList[l].socket, closedUser, msgLen, MSG_DONTWAIT | MSG_NOSIGNAL);
						}
					}
					FD_CLR(socket, &chief);
					return false;
				}
			}
			// else, continue
		} else {
			return true;
		}
	}

	// check if observer closed itself
	bool checkClosedObs(int rec, int socket) {
		// if closed...
		if (rec < 1) {
			// remove
			for (int i = 0; i < maxClients; i++) {
				if (observerList[i].socket == socket) {
					for (int j = 0; j < maxClients; j++) {
						if (participantList[i].aff == socket) {
							participantList[i].aff = 0;
						}
					}
					// remove data
					memset(observerList[i].affPart, 0, sizeof(observerList[i].affPart));
					numObs--;
					close(socket);
					observerList[i].socket = 0;
					FD_CLR(socket, &chief);
					return false;
				}
			}
			// else, continue
		} else {
			return true;
		}
	}

	// broadcast public message
	void sendMessage(char username[]) {
		char msgSend[1100]; // buffer for string the server sends */
		int userLength = strlen(username);
		int varLen = 11-userLength;
		// create message
		msgSend[0] = '>';
		for (int i = 1; i < varLen+1; i++) {
			msgSend[i] = ' ';
		}
		for (int j = varLen+1; j < 12; j++) {
			msgSend[j] = username[j-varLen-1];
		}
		msgSend[12] = ':';
		msgSend[13] = ' ';
		for (int k = 14; k < msgLen+14; k++) {
			msgSend[k] = buf[k-14];
		}
		int length = msgLen + 14;
		uint16_t networkLen = htons(length);
		for (int l = 0; l < maxClients; l++) {
			// send to active observers
			if (observerList[l].socket > 0) {
				send(observerList[l].socket,  &networkLen, sizeof(networkLen), MSG_DONTWAIT | MSG_NOSIGNAL);
				send(observerList[l].socket, msgSend, sizeof(msgSend), MSG_DONTWAIT | MSG_NOSIGNAL);
				memset(buf, 0, sizeof(buf));
			}
		}
		memset(msgSend, 0, sizeof(msgSend));
	}

	// send private message
	void sendPrivateMessage(char senderUsername[], char deliveryUsername[], int msgLen) {
		char msgSend[1100]; // buffer for string the server sends */
		int senderSocket = 0;
		int deliverySocket = 0;
		bool foundName = false;
		// find sender and recipient socket
		for (int i = 0; i < maxClients; i++) {
			if (strcmp(participantList[i].partName, senderUsername) == 0) {
				senderSocket = participantList[i].aff;
			} else if (strcmp(participantList[i].partName, deliveryUsername) == 0) {
				foundName = true;
				deliverySocket = participantList[i].aff;
			}
		}
		// check if recipient is sender
		if (strcmp(senderUsername, deliveryUsername) == 0) {
			foundName = true;
		}
		// if recipient doesn't exist, send warning to user
		if (!(foundName)) {
			// create message
			int deliveryLen = strlen(deliveryUsername);
			char notFound[32+deliveryLen];
			notFound[0] = 'W';
			notFound[1] = 'a';
			notFound[2] = 'r';
			notFound[3] = 'n';
			notFound[4] = 'i';
			notFound[5] = 'n';
			notFound[6] = 'g';
			notFound[7] = ':';
			notFound[8] = ' ';
			notFound[9] = 'u';
			notFound[10] = 's';
			notFound[11] = 'e';
			notFound[12] = 'r';
			notFound[13] = ' ';
			int iter = 0;
			for (int j = 14; j < deliveryLen+14; j++) {
				notFound[j] = deliveryUsername[iter];
				iter++;
			}
			notFound[deliveryLen+14] = ' ';
			notFound[deliveryLen+15] = 'd';
			notFound[deliveryLen+16] = 'o';
			notFound[deliveryLen+17] = 'e';
			notFound[deliveryLen+18] = 's';
			notFound[deliveryLen+19] = 'n';
			notFound[deliveryLen+20] = '\'';
			notFound[deliveryLen+21] = 't';
			notFound[deliveryLen+22] = ' ';
			notFound[deliveryLen+23] = 'e';
			notFound[deliveryLen+24] = 'x';
			notFound[deliveryLen+25] = 'i';
			notFound[deliveryLen+26] = 's';
			notFound[deliveryLen+27] = 't';
			notFound[deliveryLen+28] = '.';
			notFound[deliveryLen+29] = '.';
			notFound[deliveryLen+30] = '.';
			notFound[deliveryLen+31] = ' ';
			uint16_t failedLength = strlen(notFound);
			int failedNetworkLen = htons(failedLength);
			// tell sender
			send(senderSocket, &failedNetworkLen, sizeof(failedNetworkLen), MSG_DONTWAIT | MSG_NOSIGNAL);
			send(senderSocket, notFound, sizeof(notFound), MSG_DONTWAIT | MSG_NOSIGNAL);
			// else send message to recipient and sender
		} else {
			int nameLen = strlen(senderUsername);
			int varLen = 11-nameLen;
			msgSend[0] = '*';
			for (int k = 1; k < varLen+1; k++) {
				msgSend[k] = ' ';
			}
			for (int l = varLen+1; l < 12; l++) {
				msgSend[l] = senderUsername[l-varLen-1];
			}
			msgSend[12] = ':';
			msgSend[13] = ' ';
			for (int m = 14; m < msgLen+14; m++) {
				msgSend[m] = buf[m-14];
			}
			int length = msgLen + 14;
			uint16_t networkLen = htons(length);
			send(senderSocket, &networkLen, sizeof(networkLen), MSG_DONTWAIT | MSG_NOSIGNAL);
			send(senderSocket, msgSend, sizeof(msgSend), MSG_DONTWAIT | MSG_NOSIGNAL);
			if (deliverySocket > 0) {
				send(deliverySocket, &networkLen, sizeof(networkLen), MSG_DONTWAIT | MSG_NOSIGNAL);
				send(deliverySocket, msgSend, sizeof(msgSend), MSG_DONTWAIT | MSG_NOSIGNAL);
			}
		}
		memset(buf, 0, sizeof(buf));
		memset(msgSend, 0, sizeof(msgSend));
	}

	// broadcast that new observer has joined
	void newObserver() {
		uint16_t len = 26;
		char msg[26] = "A new observer has joined ";
		uint16_t networkLen = htons(len);
		// send to active observers
		for (int i = 0; i < maxClients; i++) {
			if (observerList[i].socket > 0) {
				send(observerList[i].socket, &networkLen, sizeof(networkLen), MSG_DONTWAIT | MSG_NOSIGNAL);
				send(observerList[i].socket, msg, sizeof(msg), MSG_DONTWAIT | MSG_NOSIGNAL);
			}
		}
	}

	if (argc != 3 ) {
		fprintf(stderr,"Error: Wrong number of arguments\n");
		fprintf(stderr,"usage:\n");
		fprintf(stderr,"./server participant_port observer_port\n");
		exit(EXIT_FAILURE);
	}

	memset((char *)&sad1,0,sizeof(sad1)); /* clear sockaddr structure */
	sad1.sin_family = AF_INET; /* set family to Internet */
	sad1.sin_addr.s_addr = INADDR_ANY; /* set the local IP address */

	memset((char *)&sad2,0,sizeof(sad2)); /* clear sockaddr structure */
	sad2.sin_family = AF_INET; /* set family to Internet */
	sad2.sin_addr.s_addr = INADDR_ANY; /* set the local IP address */

	participantPort = atoi(argv[1]); /* convert argument to binary */
	observerPort = atoi(argv[2]);

	/* Map TCP transport protocol name to protocol number */
	if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
		fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
		exit(EXIT_FAILURE);
	}

	if ((participantPort > 1024) && (participantPort < 65535)) { /* test for illegal value */
		sad1.sin_port = htons((u_short)participantPort);
	} else { /* print error message and exit */
		fprintf(stderr,"Error: Bad port number %s\n",argv[1]);
		exit(EXIT_FAILURE);
	}

	/* Create a socket */
	participantSocket = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if (participantSocket < 0) {
		fprintf(stderr, "Error: Socket creation failed\n");
		exit(EXIT_FAILURE);
	}

	/* Allow reuse of port - avoid "Bind failed" issues */
	if( setsockopt(participantSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0 ) {
		fprintf(stderr, "Error Setting socket option failed\n");
		exit(EXIT_FAILURE);
	}

	/* Bind a local address to the socket */
	if (bind(participantSocket, (struct sockaddr *)&sad1, sizeof(sad1)) < 0) {
		fprintf(stderr,"Error: Bind failed\n");
		exit(EXIT_FAILURE);
	}

	/* Specify size of request queue */
	if (listen(participantSocket, QLEN) < 0) {
		fprintf(stderr,"Error: Listen failed\n");
		exit(EXIT_FAILURE);
	}

	if ((observerPort > 1024) && (observerPort < 65535)) { /* test for illegal value */
		sad2.sin_port = htons((u_short)observerPort);
	} else { /* print error message and exit */
		fprintf(stderr,"Error: Bad port 2 number %s\n",argv[2]);
		exit(EXIT_FAILURE);
	}

	/* Create a socket */
	observerSocket = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if (observerSocket < 0) {
		fprintf(stderr, "Error: Socket 2 creation failed\n");
		exit(EXIT_FAILURE);
	}

	/* Allow reuse of port - avoid "Bind failed" issues */
	if( setsockopt(observerSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0 ) {
		fprintf(stderr, "Error Setting socket option failed\n");
		exit(EXIT_FAILURE);
	}

	/* Bind a local address to the socket */
	if (bind(observerSocket, (struct sockaddr *)&sad2, sizeof(sad2)) < 0) {
		fprintf(stderr,"Error: Bind 2 failed\n");
		exit(EXIT_FAILURE);
	}

	/* Specify size of request queue */
	if (listen(observerSocket, QLEN) < 0) {
		fprintf(stderr,"Error: Listen 2 failed\n");
		exit(EXIT_FAILURE);
	}

	int selection;
	int newsd1 = 0;
	int newsd2 = 0;
	int fdmax = 0;

	// reset fdmax if needed
	if (participantSocket > observerSocket) {
		fdmax = participantSocket;
	} else {
		fdmax = observerSocket;
	}

	// create set
	FD_SET(participantSocket, &chief);
	FD_SET(observerSocket, &chief);

	/* Main server loop - accept and handle requests */
	while (1) {

		// copy over data from last iteration
		reader = chief;
		// select
		selection = select(fdmax + 1, &reader, NULL, NULL, &timer);
		// select failure
		if (selection < 0) {
			printf("Select failure. Exiting...\n");
			exit(EXIT_FAILURE);
		}

		// if the timer runs out, disconnect the corresponding clients
		if (selection == 0) {

			// reset timer
			timer.tv_sec = 60;
			timer.tv_usec = 0;

			// select finds a socket
		} else if (selection > 0) {

			// check all sockets for flags
			for (int i = 0; i < fdmax+1; i++) {
				if (FD_ISSET(i, &reader)) {
					// new participant
					if (i == participantSocket) {
						alen = sizeof(cad1);
						if ((newsd1 = accept(participantSocket, (struct sockaddr *)&cad1, &alen)) < 0) {
							fprintf(stderr, "Error participant: Accept failed\n");
							exit(EXIT_FAILURE);
						}
						// disconnect if server has reached its limit
						if (numParts == maxClients) {
							connectionAcceptPart[0] = 'N';
							send(newsd1, connectionAcceptPart, 1, 0);
							close(newsd1);
							// else add to fd_set
						} else {
							connectionAcceptPart[0] = 'Y';
							send(newsd1, connectionAcceptPart, 1, 0);
							FD_SET(newsd1, &chief);
							if (newsd1 > fdmax)
							fdmax = newsd1;
							participantList[numParts].socket = newsd1;
							participantList[numParts].nameLength = 0;
							numParts++;
						}
						// new observer
					} else if (i == observerSocket) {
						alen = sizeof(cad2);
						if ((newsd2 = accept(observerSocket, (struct sockaddr *)&cad2, &alen)) < 0) {
							fprintf(stderr, "Error participant: Accept failed\n");
							exit(EXIT_FAILURE);
						}
						// disconnect if server has reached its limit
						if (numObs == maxClients) {
							connectionAcceptObs[0] = 'N';
							send(newsd2, connectionAcceptObs, 1, 0);
							close(newsd2);
							// else add to fdset
						} else {
							connectionAcceptObs[0] = 'Y';
							send(newsd2, connectionAcceptObs, 1, 0);
							FD_SET(newsd2, &chief);

							if (newsd2 > fdmax) {
								fdmax = newsd2;
							}
							observerList[numObs].socket = newsd2;
							numObs++;
						}
						// else find existing socket that awoke
					} else {
						for (int j = 0; j < maxClients; j++) {
							// found existing participant socket
							if (i == participantList[j].socket) {
								// participant is setting username
								if (!(participantList[j].nameLength > 0)) {
									char newUsername[255];
									uint8_t userLen;
									recv(i, &userLen, 1, 0);
									int check = recv(i, &newUsername, sizeof(newUsername), 0);
									// check if participant closed itself
									if (checkClosedPartNonActive(check, i, participantList)) {
										validUser[0] = validateUsername(userLen, newUsername, participantList);
										// send T if username is taken, reset participant's timer
										if (validUser[0] == 'T') {
											participantList[i].userTimer.tv_sec = 60;
											send(i, validUser, 1, 0);
											memset(newUsername, 0, sizeof(newUsername));
											// send I if username is invalid, don't reset timer
										} else if (validUser[0] == 'I') {
											send(i, validUser, 1, 0);
											memset(newUsername, 0, sizeof(newUsername));
											// send Y if username if valid, accept and give attributes
										} else {
											send(i, validUser, 1, 0);
											for (int k = 0; k < userLen; k++) {
												participantList[j].partName[k] = newUsername[k];
											}
											participantList[j].nameLength = userLen;
											newParticipant(participantList[i].socket, newUsername);
											memset(newUsername, 0, sizeof(newUsername));
											break;
										}
										// participant closed itself
									} else {
										memset(newUsername, 0, sizeof(newUsername));
									}
									// else participant is sending message
								} else {
									if (numParts != 0) {
										int closed = recv(i, &netLen, sizeof(netLen), 0);
										msgLen = ntohs(netLen);
										// check if participant closed itself
										if (checkClosedPart(closed, i, participantList[j].partName)) {
											bool hasSpace = false;
											recv(i, buf, sizeof(buf), MSG_WAITALL);
											for (int j = 2; j < 13; j++) {
												if (buf[j] == ' ') {
													hasSpace = true;
													break;
												}
											}
											// check if message is private
											if ((buf[0] == '@') && (buf[1] != ' ') && (hasSpace)) {
												char username[255];
												memset(username, 0, sizeof(username));
												for (int j = 0; j < 10; j++) {
													if (buf[j+1] != ' ') {
														username[j] = buf[j+1];
													} else {
														break;
													}
												}
												// send private message
												sendPrivateMessage(participantList[j].partName, username, msgLen);
												break;
											} else {
												// send public message
												sendMessage(participantList[j].partName);
												break;
											}
											// participant closed itself
										} else {
											break;
										}
									}
								}
								// found exisiting observer socket
							} else if (observerList[j].socket == i) {
								int answer = recv(i, &affLen, 1, 0);
								// check if observer closed itself
								if (checkClosedObs(answer, i)) {
									char affUsername[255];
									int answer = recv(i, &affUsername, sizeof(affUsername), 0);
									// check for valid affiliation
									validAff[0] = validateAffiliation(affUsername, affLen, i);
									// send Y is username could be affiliated with
									if (validAff[0] == 'Y') {
										send(i, validAff, 1, 0);
										newObserver();
										// send T if username was taken
									} else if (validAff[0] == 'T') {
										send(i, validAff, 1, 0);
										// close observer if username didn't exist
									} else {
										send(i, validAff, 1, 0);
										close(i);
										FD_CLR(i, &chief);
										observerList[j].socket = 0;
									}
									break;
									// observer closed itself
								} else {
									break;
								}
							}
						}
					}
				}
			}
		}
	}
}
