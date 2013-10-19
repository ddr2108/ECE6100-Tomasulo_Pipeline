#include <stdlib.h>
#include <stdio.h>
#include "procsim.hpp"

#define FALSE 0
#define TRUE  1

typedef struct _node{
	int counter;
	proc_inst_t p_inst;
	_node *next;
} node;

typedef struct _ROB{
	int dest;
	proc_inst_t p_inst;
} ROB;

typedef struct _arrayPointers{
	int head;
	int tail;
} arrayPointers; 

uint64_t r = 0; 
uint64_t k0 = 0;
uint64_t k1 = 0;
uint64_t k2 = 0;
uint64_t f = 0;
uint64_t m = 0;

int regFile[32];		

node* dispatchQueue;
int dispatchRoom; 

node* k0Queue = NULL;
int k0Room; 
node* k1Queue = NULL;
int k1Room; 
node* k2Queue = NULL;
int k2Room; 

ROB *ROBTable;
int ROBhead = 0;
int ROBtail = 1;

int ROBadd(proc_inst_t p_inst){
	if (ROBtail!=ROBhead){
		//Put item into ROB table
		ROBTable[ROBtail].dest = p_inst.dest_reg;
		ROBTable[ROBtail].p_inst = p_inst;
		//Fix register file
		regFile[p_inst.dest_reg] = ROBtail;
		//Fix pointers
		ROBtail = (ROBtail+1)%r;
	}else{
		return FALSE;
	}

	return TRUE;
}

int ROBremove(){
	//Fix register file
	if (regFile[ROBTable[ROBhead].dest] == ROBhead){
		regFile[ROBTable[ROBhead].dest] = -1;
	}

	//Fix ROB queue
	ROBhead = (ROBhead+1)%r;

	return TRUE;
}

void addQueue(node** head, int counter, proc_inst_t p_inst,int* room){
	node* createNode = (node*) malloc(sizeof(node));	//Malloc new item
	createNode->counter = counter;						//Adjust value
	createNode->p_inst = p_inst;						//Adjust value

	//Fix ordering of queue
	createNode->next = *(head);		
	*head = createNode;

	(*room)--;			//Change size
}

node* removeQueue(node** head, int* room){
	node* returnNode = *head;		//Save original head
	*head = (*head)->next;			//Reassign head

	(*room)++;						//Change size

	return returnNode;				//Return node
}

/**
 * Subroutine for initializing the processor. You many add and initialize any global or heap
 * variables as needed.
 * XXX: You're responsible for completing this routine
 *
 * @r ROB size
 * @k0 Number of k0 FUs
 * @k1 Number of k1 FUs
 * @k2 Number of k2 FUs
 * @f Number of instructions to fetch
 * @m Schedule queue multiplier
 */
void setup_proc(uint64_t rIn, uint64_t k0In, uint64_t k1In, uint64_t k2In, uint64_t fIn, uint64_t mIn) {
	 //Set globally accessible
	 r = rIn; 
	 k0 = k0In;
	 k1 = k1In;
	 k2 = k2In;
	 f = fIn;
	 m = mIn;

	 //Size of queue
	 dispatchRoom = r;
	 k0Room = m*k0;
	 k1Room = m*k1;
	 k2Room = m*k2;

	 //Initialize reg array
	 for (int i = 0; i<32; i++){
	 	regFile[i] = -1;
	 }

	 //Allocate array
	 ROBTable = (ROB*) malloc(r*sizeof(ROB));
	 dispatchQueue = 
	 k0Queue = 
	 k1Queue = 
	 k2Queue = 
}

/**
 * Subroutine that simulates the processor.
 *   The processor should fetch instructions as appropriate, until all instructions have executed
 * XXX: You're responsible for completing this routine
 *
 * @p_stats Pointer to the statistics structure
 */
void run_proc(proc_stats_t* p_stats) {
	//line number
	int instruction = 1;
	int cycle = 0;
	//Instruction
	proc_inst_t* p_inst;

	//Allocate instruction
	p_inst = (proc_inst_t*) malloc(sizeof(proc_inst_t));

	//Read the instructions
	while(dispatchRoom != 0){
		//Fetch F instructions at a time
		for (int i = 0; i<f; i++){
			if (dispatchRoom != 0){
				instruction++;													//Line number increment
				read_instruction(p_inst);									//fetch instruction
				addQueue(&dispatchQueue, instruction, *p_inst, &dispatchRoom);	//add to dispatch queue
			}else{
				break;
			}
		}

		cycle++;

	}

	//Free allocated memory
	free(p_inst);
}

/**
 * Subroutine for cleaning up any outstanding instructions and calculating overall statistics
 * such as average IPC or branch prediction percentage
 * XXX: You're responsible for completing this routine
 *
 * @p_stats Pointer to the statistics structure
 */
void complete_proc(proc_stats_t *p_stats) {
}
