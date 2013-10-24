#include <stdlib.h>
#include <stdio.h>
#include "procsim.hpp"

#define FALSE -1
#define TRUE  1

#define FULL  2
#define EMPTY  3
#define HAS_ROOM  4

//Register structure
typedef struct _reg{
	int ready;
	int tag;
} reg;

//Generic node
typedef struct _node{
	proc_inst_t p_inst;
	node *next;
	node *prev;
	int line_number;
	char filler[20]
} node;

//Structure for scheduling node
typedef struct _schedNode{
	proc_inst_t p_inst;
	schedNode *next;
	schedNode *prev;
	int line_number;
	int destTag;
	int src1Tag;
	int src2Tag;
} schedNode;

//Structure for functional unit
typedef struct _FUnode{
	proc_inst_t p_inst;
	FUnode *next;
	FUnode *prev;
	int line_number;
	int destTag;
	int age;
	int valid;
	char filler[8];
} FUnode;

//Structure for ROB
typedef struct _ROB{
	proc_inst_t p_inst;
	int line_number;
	int destTag;
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
reg regFile[32];		

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

int addROB(node* dispatchNode){
	int tag;		//tag added to 

	if (ROBPointers.tail!=ROBPointers.head){
		//Put item into ROB table
		ROBTable[ROBPointers.tail].line_number = dispatchNode->line_number;
		ROBTable[ROBPointers.tail].destTag = ROBPointers.tail;
		ROBTable[ROBPointers.tail].p_inst = dispatchNode->p_inst;
		//Fix register file
		regFile[dispatchNode->p_inst.dest_reg] = ROBPointers.tail;
		//Fix pointers
		ROBPointers.tail = (ROBPointers.tail+1)%r;
	}else{
		return FALSE;
	}

	return tag;
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

schedNode* createSchedNode(node* dispatchNode, int tag){
	schedNode* newNode = (schedNode*) malloc(sizeof(schedNode));	//Create a new node

	//Copy over data
	newNode->p_inst = dispatchNode->p_inst;
	newNode->line_number = dispatchNode
	newNode->destTag = tag;

	//Add valididty data
	newNode->src1Tag = regFile[dispatchNode->p_inst.src_reg[0]];
	newNode->src2Tag = regFile[dispatchNode->p_inst.src_reg[1]];

	//Return Data
	return newNode; 
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
	 	regFile[i].ready = 0;
	 	regFile[i].tag = 0;
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
	 dispatchPointers = {0,0,0}; 
	 k0QueuePointers = {0,0,0};
	 k1QueuePointers = {0,0,0};
	 k2QueuePointers = {0,0,0};
	 k0FUPointers = {0,0,0};
	 k1FUPointers = {0,0,0};
	 k2FUPointers = {0,0,0};
	 ROBPointers = {0,0,0};
}

/**
 * Subroutine that simulates the processor.
 *   The processor should fetch instructions as appropriate, until all instructions have executed
 * XXX: You're responsible for completing this routine
 *
 * @p_stats Pointer to the statistics structure
 */
void run_proc(proc_stats_t* p_stats) {
	//Flag to keep program running
	int flag = 1;

	//Reading flags
	int readFlag = 1;
	//Dispatcher flags
	int dispatcherFlag = 1;

	//line number
	int instruction = 1;
	int cycle = 0;

	//Instruction
	proc_inst_t* p_inst;
	proc_inst_t temp;

	//Pipeline
	while(flag){

		//Read Instructions
		node* readNode;		//Node for new instructions
		//Fetch F instructions at a time
		for (int i = 0; i<f && readFlag; i++){
			if (arrayStatus(dispatchPointers)!=FULL){
				//Allocate instruction
				p_inst = (proc_inst_t*) malloc(sizeof(proc_inst_t));
				readFlag = read_instruction(p_inst);									//fetch instruction
				//Check if end of file reached
				if (readFlag){		//If thre is an instruction
					//Create new node
					readNode = (node*) malloc(sizeof(node));
					readNode->line_number = instruction;
					readNode->p_inst = *p_inst;
					//Add node to list of instructions
					addArray(&dispatchPointers, readNode);	//add to dispatch queue
					instruction++;													//Line number increment
				}
			}else{
				break;
			}
		}

		//Dispatcher
		//Reset dispatcherFlag
		node* dispatchNode = dispatchPointers.head;		//Node for instruction in dispatch queue
		schedNode* schedNodeAdd;
		int tag; 
		while(dispatcherFlag!=FALSE && arrayStatus(dispatchPointers)!=EMPTY && dispatchNode){
			p_inst dispatchInstruction = dispatchNode->p_inst; 	//Get instruction

			//add to correct scheduling queue and ROB and remove from dispatcher
			if (dispatchInstruction.op_code == 0 && arrayStatus(k0QueuePointers)==HAS_ROOM){
				tag = addROB(dispatchNode);		//Add to ROB
				//if added to ROB, add to scheduler
				if (tag!=FALSE){
					schedNodeAdd = createSchedNode(dispatchNode, tag);
					dispatcherFlag = addArray(&k0Queue, schedNodeAdd);	
				}else{
					dispatcherFlag = FALSE;
				}
			}else if(dispatchInstruction.op_code == 1 && arrayStatus(k1QueuePointers)==HAS_ROOM){
				tag = addROB(dispatchNode);		//Add to ROB
				//if added to ROB, add to scheduler
				if (tag!=FALSE){
					schedNodeAdd = createSchedNode(dispatchNode, tag);
					dispatcherFlag = addArray(&k1Queue, schedNodeAdd);	
				}else{
					dispatcherFlag = FALSE;
				}
			}else if(dispatchInstruction.op_code == 2 && arrayStatus(k2QueuePointers)==HAS_ROOM){
				tag = addROB(dispatchNode);		//Add to ROB
				//if added to ROB, add to scheduler
				if (tag!=FALSE){
					schedNodeAdd = createSchedNode(dispatchNode, tag);
					dispatcherFlag = addArray(&k2Queue, schedNodeAdd);	
				}else{
					dispatcherFlag = FALSE;
				}
			}else{
				dispatcherFlag = FALSE;
			}

			//If there was room in schedule add to ROB and remove from dispatcher
			if (dispatcherFlag!=FALSE){
				//Go to next item in scheduler
				dispatchNode = dispatchNode->next;
				//Remove item from dispatcher queue
				removeArray(&dispatchQueue, dispatchNode->prev);
			}

		}

		//Scheduler
		schedNode* temp0 = k0QueuePointers.head;
		schedNode* temp1 = k1QueuePointers.head;
		schedNode* temp2 = k2QueuePointers.head;
		do{

			success = 0;
			//Scheduling
			//Check if registers are avaialable and functional unit is avaialable
			if (temp0 && i<m*k0 && temp0.src1Ready == 1 && temp0.src2Ready[1]==1  && k0FUPointers.size>0){
				//Add new node to list
				FUnode* newNode = (FUnode*) malloc(sizeof(newNode));
				newNode->age = 1;
				newNode->valid = 1;
				newNode->tagDest = temp0->tagDest;
				
				addArray(k0FUPointers, newNode);
				//Update scoreboard
				k0FUPointers.size--;
				success = 1;
			
				//Go to next element
				temp0 = temp0->next;
				removeArray(k0QueuePointers, temp0);
			}else{
				//Go to next element
				temp0 = temp0->next;
			}

			//Check if registers are avaialable and functional unit is avaialable
			if (temp1 && i<m*k1 && temp1.src1Ready == 1 && temp1.src2Ready[1]==1  && k1FUPointers.size>0){
				//Add new node to list
				FUnode* newNode = (FUnode*) malloc(sizeof(newNode));
				newNode->age = 2;
				newNode->valid = 1;
				newNode->tagDest = temp1->tagDest;
				
				addArray(k1FUPointers, newNode);
				//Update scoreboard
				k1FUPointers.size--;
				success = 1;
				
				//Go to next element
				temp1 = temp1->next;
				removeArray(k1QueuePointers, temp1);
			}else{
				//Go to next element
				temp1 = temp1->next;
			}

			//Check if registers are avaialable and functional unit is avaialable
			if (temp2 && i<m*k2 && temp2.src1Ready == 1 && temp2.src2Ready[1]==1 && k2FUPointers.size>0){
				//Add new node to list
				FUnode* newNode = (FUnode*) malloc(sizeof(newNode));
				newNode->age = 3;
				newNode->valid = 1;
				newNode->tagDest = temp2->tagDest;
				
				addArray(k2FUPointers, newNode);
				//Update scoreboard
				k2FUPointers.size--;
				success = 1;
			
				//Go to next element
				temp2 = temp2->next;
				removeArray(k2QueuePointers, temp2);
			}else{
				//Go to next element
				temp2 = temp2->next;
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
			removeROB()
			update reg file
			update scheduler
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
