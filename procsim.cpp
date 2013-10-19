#include <stdlib.h>
#include <stdio.h>
#include "procsim.hpp"

#define FALSE 0
#define TRUE  1

//Structure for nodes of scheduler, dispatcher
typedef struct _node{
	proc_inst_t p_inst;
} node;

//Structure for ROB
typedef struct _ROB{
	int dest;
	proc_inst_t p_inst;
} ROB;

//Pointers for circular FIFO array
typedef struct _arrayPointers{
	int head;
	int tail;
} arrayPointers; 

//Initialization Parameters
uint64_t r = 0; 
uint64_t k0 = 0;
uint64_t k1 = 0;
uint64_t k2 = 0;
uint64_t f = 0;
uint64_t m = 0;

//Register file
int regFile[32];		

//Dispatcher
node* dispatchQueue;
arrayPointers dispatchPointers; 

//Scheudler
node* k0Queue = NULL;
arrayPointers k0Pointers; 
node* k1Queue = NULL;
arrayPointers k1Pointers; 
node* k2Queue = NULL;
arrayPointers k2Pointers; 

//ROB Table for execution
ROB *ROBTable;
arrayPointers ROBPointers; 

int hasRoom(arrayPointers queuePointers){
	if(queuePointers.head == queuePointers.tail){
		return FALSE;
	}else{
		return TRUE;
	}
}

int addROB(proc_inst_t p_inst){
	if (ROBPointers.tail!=ROBPointers.head){
		//Put item into ROB table
		ROBTable[ROBPointers.tail].dest = p_inst.dest_reg;
		ROBTable[ROBPointers.tail].p_inst = p_inst;
		//Fix register file
		regFile[p_inst.dest_reg] = ROBPointers.tail;
		//Fix pointers
		ROBPointers.tail = (ROBPointers.tail+1)%r;
	}else{
		return FALSE;
	}

	return TRUE;
}

int removeROB(){
	//Fix register file
	if (regFile[ROBTable[ROBPointers.head].dest] == ROBPointers.head){
		regFile[ROBTable[ROBPointers.head].dest] = -1;
	}

	//Fix ROB queue
	ROBPointers.head = (ROBPointers.head+1)%r;

	return TRUE;
}

int addQueue(node** queue, arrayPointers* queuePointers, proc_inst_t p_inst){
	if (queuePointers->tail!=queuePointers->head){
		(*queue)[queuePointers->tail].p_inst = p_inst;
		//Fix pointers
		queuePointers->tail = (queuePointers->tail+1)%r;
	}else{
		return FALSE;
	}

	return TRUE;
}

node* removeQueue(node** queue, arrayPointers* queuePointers){
	node* returnNode = (node *) malloc(sizeof(node));
	*returnNode = (*queue)[queuePointers->head];

	queuePointers->head = (queuePointers->head+1)%r;

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

	 //Initialize reg array
	 for (int i = 0; i<32; i++){
	 	regFile[i] = -1;
	 }

	 //Allocate array
	 ROBTable = (ROB*) malloc(r*sizeof(ROB));
	 dispatchQueue = (node*) malloc(r*sizeof(node));
	 k0Queue = (node*) malloc(k0*m*sizeof(node));
	 k1Queue = (node*) malloc(k1*m*sizeof(node));
	 k2Queue = (node*) malloc(k2*m*sizeof(node));

	 //Initialize pointers
	 dispatchPointers = {0,1}; 
	 k0Pointers = {0,1};
	 k1Pointers = {0,1};
	 k2Pointers = {0,1};
	 ROBPointers = {0,1};
}

/**
 * Subroutine that simulates the processor.
 *   The processor should fetch instructions as appropriate, until all instructions have executed
 * XXX: You're responsible for completing this routine
 *
 * @p_stats Pointer to the statistics structure
 */
void run_proc(proc_stats_t* p_stats) {
	int flag = 1;
	int success = 0;

	//line number
	int instruction = 1;
	int cycle = 0;

	//Instruction
	proc_inst_t* p_inst;
	proc_inst_t temp;

	//Allocate instruction
	p_inst = (proc_inst_t*) malloc(sizeof(proc_inst_t));

	//Read the instructions
	while(flag){
		//Fetch F instructions at a time
		for (int i = 0; i<f; i++){
			if (dispatchPointers.head != dispatchPointers.tail){
				instruction++;													//Line number increment
				success = read_instruction(p_inst);									//fetch instruction
				//Check if end of file reached
				if (success){
					addQueue(&dispatchQueue, &dispatchPointers, *p_inst);	//add to dispatch queue
				}else {
					flag = 0;
					break;
				}

			}else{
				break;
			}
		}

		//Dispatcher
		while(flag && dispatchPointers.head != dispatchPointers.tail){
			//Get the instruction
			temp = dispatchQueue[dispatchPointers.head].p_inst;

			//add to correct scheduling queue and ROB adn remove from dispatcher
			if (temp.op_code == 0 && hasRoom(k0Pointers)){
				success = addQueue(&k0Queue, &k0Pointers, temp);	
			}else if(temp.op_code == 1 && hasRoom(k1Pointers)){
				success = addQueue(&k1Queue, &k1Pointers, temp);	
			}else if(temp.op_code == 2 && hasRoom(k2Pointers)){
				success = addQueue(&k1Queue, &k1Pointers, temp);	
			}else{
				success = 0;
			}

			//If there was room in schedule add to ROB and remove from dispatcher
			if (success){
				addROB(temp);
				removeQueue(&dispatchQueue, &dispatchPointers);
			}
		}

		//Change clock cycle
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
