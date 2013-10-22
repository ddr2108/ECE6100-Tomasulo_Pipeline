#include <stdlib.h>
#include <stdio.h>
#include "procsim.hpp"

#define FALSE 0
#define TRUE  1

#define FULL  2
#define EMPTY  3
#define HAS_ROOM  4


//Structure for nodes of scheduler, dispatcher
typedef struct _node{
	proc_inst_t p_inst;
	node *next;
	node *prev;
} node;

typedef struct _FUnode{
	proc_inst_t p_inst;
	node *next;
	node *prev;
	int age;
	int valid;
} FUnode;

//Structure for ROB
typedef struct _ROB{
	int dest;
	proc_inst_t p_inst;
} ROB;

//Pointers for circular FIFO array
typedef struct _arrayPointers{
	int head;
	int tail;
	int size;
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
arrayPointers k0QueuePointers; 
node* k1Queue = NULL;
arrayPointers k1QueuePointers; 
node* k2Queue = NULL;
arrayPointers k2QueuePointers; 

//Execute
node* k0FU = NULL;
arrayPointers k0FUPointers; 
node* k1FU = NULL;
arrayPointers k1FUPointers; 
node* k2FU = NULL;
arrayPointers k2FUPointers; 

//ROB Table for execution
ROB *ROBTable;
arrayPointers ROBPointers; 

int arrayStatus(arrayPointers queuePointers){
	if((queuePointers.tail-queuePointers.head)==1){
		return EMPTY;
	}else if (queuePointers.tail==0 && queuePointers.head==(r-1)){
		return EMPTY; 
	}else if(queuePointers.head == queuePointers.tail){
		return FULL;
	}else{
		return HAS_ROOM;
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

int addArray(arrayPointers* arrayPointersIn, node* nextNode){
	//Add new element if there is enough room
	if (arrayPointersIn->size>0){
		//Fix pointers
		nextNode->prev = arrayPointersIn->tail;
		arrayPointersIn->tail->next = nextNode;
		//Fix array pointers
		arrayPointersIn->tail = nextNode;
	}else{
		return FALSE;
	}

	return TRUE;
}

void removeArray(arrayPointers* queuePointers, node* deleteNode){
	if (deleteNode->prev==NULL){			//Head element
		if (deleteNode->next==NULL){		//Special case where there is only 1 element
			queuePointers->head = NULL;
			queuePointers->tail = NULL;
		}else{
			queuePointers->head = deleteNode->next;
			queuePointers->head->prev = NULL;
		}
		free(deleteNode);
	}else if (deleteNode->next==NULL){		//Tail element
		queuePointers->tail = deleteNode->prev;
		queuePointers->tail->next = NULL;
	} else{									//Element in middle
		deleteNode->prev->next = deleteNode->next;
		deleteNode->next->prev = deleteNode->prev;		
	}

	free(deleteNode);		//Free allocated memory
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
	 k0FU = (node*) malloc(k0*sizeof(node));
	 k1FU = (node*) malloc(k1*sizeof(node));
	 k2FU = (node*) malloc(k2*sizeof(node));

	 //Initialize pointers
	 dispatchPointers = {0,1}; 
	 k0QueuePointers = {0,1};
	 k1QueuePointers = {0,1};
	 k2QueuePointers = {0,1};
	 k0FUPointers = {0,1};
	 k1FUPointers = {0,1};
	 k2FUPointers = {0,1};
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
		for (int i = 0; i<f && flag==1; i++){
			if (arrayStatus(dispatchPointers)!=FULL){
				success = read_instruction(p_inst);									//fetch instruction
				//Check if end of file reached
				if (success){
					addArray(&dispatchQueue, &dispatchPointers, *p_inst, r);	//add to dispatch queue
					instruction++;													//Line number increment
				}else {
					flag = 0;
					break;
				}

			}else{
				break;
			}
		}

		//Dispatcher
		while(flag && arrayStatus(dispatchPointers)!=EMPTY){
			//Get the instruction
			temp = dispatchQueue[dispatchPointers.head].p_inst;

			//add to correct scheduling queue and ROB and remove from dispatcher
			if (temp.op_code == 0 && arrayStatus(k0QueuePointers)==HAS_ROOM){
				success = addArray(&k0Queue, &k0QueuePointers, temp, k0*m);	
			}else if(temp.op_code == 1 && arrayStatus(k1QueuePointers)==HAS_ROOM){
				success = addArray(&k1Queue, &k1QueuePointers, temp, k1*m);	
			}else if(temp.op_code == 2 && arrayStatus(k2QueuePointers)==HAS_ROOM){
				success = addArray(&k2Queue, &k2QueuePointers, temp, k2*m);	
			}else{
				success = 0;
			}

			//If there was room in schedule add to ROB and remove from dispatcher
			if (success){
				addROB(temp);
				removeArray(&dispatchQueue, &dispatchPointers,r);
			}else {
				flag = 0;
			}
		}

		//Scheduler
		do{
			success = 0;
			//Scheduling
			temp = k0Queue[k0QueuePointers.head].p_inst;
			//Check if registers are avaialable and functional unit is avaialable
			if (temp.src_reg[0]==-1 && temp.src_reg[1]==-1 && FU[0]>0){
				allocate new FU and intiialize age
				FU[0]--;
				success = 1;
			}
			temp = k1Queue[k0QueuePointers.head].p_inst;
			//Check if registers are avaialable and functional unit is avaialable
			if (temp.src_reg[0]==-1 && temp.src_reg[1]==-1 && FU[0]>0){
				FU[1]--;
				success = 1;
			}
			temp = k2Queue[k0QueuePointers.head].p_inst;
			//Check if registers are avaialable and functional unit is avaialable
			if (temp.src_reg[0]==-1 && temp.src_reg[1]==-1 && FU[0]>0){
				FU[2]--;
				success = 1;
			}
		} while(success);

		//Execute
		FUnode* tempFUnode;
		//Execute k0 instructions
		tempFUnode = k0FUPointers.head;
		for (int i = 0; i<k0 && tempFUnode!=NULL; i++){
			if (tempFUnode->valid == 1){
				//Decrease time left
				tempFUnode->age--;
				//Check if instructions is done
				if (tempFUnode->age == 0){
					addROB()
					removeArray()				}
			}
			tempFUnode = tempFUnode->next;
		}
		//Execute k1 instructions
		tempFUnode = k1FUPointers.head;
		for (int i = 0; i<k1 && tempFUnode!=NULL; i++){
			if (tempFUnode->valid == 1){
				//Decrease time left
				tempFUnode->age--;
				//Check if instructions is done
				if (tempFUnode->age == 0){
					addROB()
					removeArray()				}
			}
			tempFUnode = tempFUnode->next;
		}//Execute k0 instructions
		tempFUnode = k2FUPointers.head;
		for (int i = 0; i<k2 && tempFUnode!=NULL; i++){
			if (tempFUnode->valid == 1){
				//Decrease time left
				tempFUnode->age--;
				//Check if instructions is done
				if (tempFUnode->age == 0){
					addROB()
					removeArray()

				}
			}
			tempFUnode = tempFUnode->next;
		}


		//Commit
		for (){
			remove from ROB
			update reg
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
