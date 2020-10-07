
#include <stdio.h>
#include <stdlib.h>
#include "project2.h"

/* ***************************************************************************
ALTERNATING BIT AND GO-BACK-N NETWORK EMULATOR: VERSION 1.1  J.F.Kurose

This code should be used for unidirectional or bidirectional
data transfer protocols from A to B and B to A.
Network properties:
- one way network delay averages five time units (longer if there
are other messages in the channel for GBN), but can be larger
- packets can be corrupted (either the header or the data portion)
or lost, according to user-defined probabilities
- packets may be delivered out of order.

Compile as gcc -g project2.c student2.c -o p2
**********************************************************************/


/***********************************************
		PACKET QUEUE STUFF
************************************************/
struct PacketNode {
	struct pkt *packet;
	struct PacketNode *next;
};

void addToQueue(struct pkt *packet, struct PacketNode *packetQueue) {

	if (!packetQueue->packet) { //if the packet queue is null, make it the head
		packetQueue->packet = packet;
		packetQueue->next = 0;
		return;
	}

	struct PacketNode *current = packetQueue;

	while (current->next) current = current->next;

	struct PacketNode *adding = (struct PacketNode *) malloc(sizeof(struct PacketNode));
	adding->packet = packet;
	adding->next = 0;

	current->next = adding;

}

struct pkt* getNextUp(struct PacketNode *packetQueue) {

	if (!packetQueue->packet)
		return 0;

	return packetQueue->packet;
}

void removeHead(struct PacketNode *packetQueue) {
	if (!packetQueue->packet)
		return;

	//get the next packet to go
	struct PacketNode *next = packetQueue->next;

	if (next->packet) {
		packetQueue->packet = next->packet;
		packetQueue->next = next->next;
	}
	else {
		packetQueue->next = 0;
		packetQueue->packet = 0;
	}
}

int emptyQueue(struct PacketNode *packetQueue) {
	return packetQueue->packet == 0;
}

/************************************************
	GENERAL VARIABLES
*************************************************/

#define RTT_TIME 10000000000

struct Sender {
	int currentSequenceNumber;
	int queueSequenceNumber;

	enum State state;

	struct PacketNode *bufferQueue;
};

struct Receiver {
	int expected_seq;
	int received;
};

enum State { WAITING_FOR_ACK, WAITING_FOR_DATA };

struct Sender aSender;
struct Receiver aReceiver;

struct Sender bSender;
struct Receiver bReceiver;

/********* STUDENTS WRITE THE NEXT SEVEN ROUTINES *********/
/*
* The routines you will write are detailed below. As noted above,
* such procedures in real-life would be part of the operating system,
* and would be called by other procedures in the operating system.
* All these routines are in layer 4*/

int get_checksum(struct pkt *packet) {
	int checksum = 0;
	checksum += packet->seqnum;
	checksum += packet->acknum;

	for (int x = 0; x < MESSAGE_LENGTH; x++) {
		checksum += (int) packet->payload[x] * (x + 1);
	}


	return checksum;
}

int check_sequenceNumber(int given, int expected) {
	return given == expected;
}

int check_checksum(struct pkt *packet) {
	return get_checksum(packet) == packet->checksum;
}

void NACK(int entity, int sequenceNumber) {
	struct pkt *packet = (struct pkt *) malloc(sizeof(struct pkt));
	packet->seqnum = sequenceNumber;
	packet->acknum = 0; //change later

	tolayer3(entity, *packet);
	
	printf("send NACK%d\n", sequenceNumber);
}

void ACK(int entity, int sequenceNumber) {
	struct pkt *packet = (struct pkt *) malloc(sizeof(struct pkt));
	packet->seqnum = sequenceNumber;
	packet->acknum = 1; //change later

	tolayer3(entity, *packet);

	printf("send ACK%d\n", sequenceNumber);
}

int isACK(struct pkt *packet, int sequenceNumber) {
	return packet->seqnum == sequenceNumber && packet->acknum == 1; //will need to modify for alternative bit
}

void retransmit(int entity, struct PacketNode *packetQueue) {
	startTimer(entity, RTT_TIME);

	if(!emptyQueue(packetQueue))
		tolayer3(entity, *getNextUp(packetQueue));
}

void transmitNext(int entity, struct PacketNode *packetQueue) {
	startTimer(entity, RTT_TIME);

	if (!emptyQueue(packetQueue))
		tolayer3(entity, *getNextUp(packetQueue));
}

/*
* A_output(message), where message is a structure of type msg, containing
* data to be sent to the B-side. This routine will be called whenever the
* upper layer at the sending side (A) has a message to send. It is the job
* of your protocol to insure that the data in such a message is delivered
* in-order, and correctly, to the receiving side upper layer.
*/
void A_output(struct msg message) {

	/*
		Format into a packet and add to the queue
	*/
	struct pkt *packet = (struct pkt *) malloc(sizeof(struct pkt));

	packet->seqnum = aSender.queueSequenceNumber;
	packet->acknum = 0;

	aSender.queueSequenceNumber++;

	for (int x = 0; x < MESSAGE_LENGTH; x++) {
		packet->payload[x] = message.data[x];
	}

	packet->checksum = get_checksum(packet);

	addToQueue(packet, aSender.bufferQueue);

	if (aSender.state == WAITING_FOR_DATA) { //send the packet
		transmitNext(AEntity, aSender.bufferQueue);
		aSender.state = WAITING_FOR_ACK;

		printf("send pkt%d\n", getNextUp(aSender.bufferQueue)->seqnum);
	}
}

/*
* Just like A_output, but residing on the B side.  USED only when the
* implementation is bi-directional.
*/
void B_output(struct msg message) {

}

/*
* A_input(packet), where packet is a structure of type pkt. This routine
* will be called whenever a packet sent from the B-side (i.e., as a result
* of a tolayer3() being done by a B-side procedure) arrives at the A-side.
* packet is the (possibly corrupted) packet sent from the B-side.
*/
void A_input(struct pkt packet) {
	stopTimer(AEntity);

	if (isACK(&packet, aSender.currentSequenceNumber)) {

		removeHead(aSender.bufferQueue);
		aSender.currentSequenceNumber++;

		if (!emptyQueue(aSender.bufferQueue)) {
			transmitNext(AEntity, aSender.bufferQueue);
		}
		else {
			aSender.state = WAITING_FOR_DATA;
		}


	}
	else { //it is an NACK
		retransmit(AEntity, aSender.bufferQueue);
	}
}

/*
* A_timerinterrupt()  This routine will be called when A's timer expires
* (thus generating a timer interrupt). You'll probably want to use this
* routine to control the retransmission of packets. See starttimer()
* and stoptimer() in the writeup for how the timer is started and stopped.
*/
void A_timerinterrupt() {
	//printf("Luke: A_timerInterrupt() called\n");
	retransmit(AEntity, aSender.bufferQueue);

}

/* The following routine will be called once (only) before any other    */
/* entity A routines are called. You can use it to do any initialization */
void A_init() {
	aSender.bufferQueue = (struct PacketNode *) malloc(sizeof(struct PacketNode));
	aSender.bufferQueue->packet = 0;
	aSender.bufferQueue->next = 0;

	aSender.state = WAITING_FOR_DATA;

	aSender.currentSequenceNumber = 1;
	aSender.queueSequenceNumber = 1;
}


/*
* Note that with simplex transfer from A-to-B, there is no routine  B_output()
*/

/*
* B_input(packet),where packet is a structure of type pkt. This routine
* will be called whenever a packet sent from the A-side (i.e., as a result
* of a tolayer3() being done by a A-side procedure) arrives at the B-side.
* packet is the (possibly corrupted) packet sent from the A-side.
*/
void B_input(struct pkt packet) {

	if (!check_sequenceNumber(packet.seqnum, bReceiver.expected_seq) || !check_checksum(&packet)) {
		printf("recv pkt%d, discard\n", packet.seqnum);
		ACK(BEntity, bReceiver.expected_seq - 1); //need to change later
	}
	else {
		printf("recv pkt%d, deliver\n", packet.seqnum);

		if (bReceiver.expected_seq == packet.seqnum && bReceiver.received) {
			bReceiver.received = 0;
		}

		struct msg *message = (struct msg *) malloc(sizeof(struct msg));

		for (int x = 0; x < MESSAGE_LENGTH; x++) {
			message->data[x] = packet.payload[x];
		}

		tolayer5(BEntity, *message);
		ACK(BEntity, packet.seqnum); //need a way to send ACKS again and again

		bReceiver.expected_seq++;
		bReceiver.received = 1;
	

	}
}

/*
* B_timerinterrupt()  This routine will be called when B's timer expires
* (thus generating a timer interrupt). You'll probably want to use this
* routine to control the retransmission of packets. See starttimer()
* and stoptimer() in the writeup for how the timer is started and stopped.
*/
void  B_timerinterrupt() {
}

/*
* The following routine will be called once (only) before any other
* entity B routines are called. You can use it to do any initialization
*/
void B_init() {
	bReceiver.expected_seq = 1;
}

