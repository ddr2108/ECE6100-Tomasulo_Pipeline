#include <stdlib.h>
#include <stdio.h>
#include "procsim.hpp"

//Boolean
#define FALSE 	-1
#define TRUE  	1

//Array Status
#define FULL  		2
#define EMPTY  		3
#define HAS_ROOM  	4

//Field Status
#define UNINITIALIZED -2
#define READY         -1

//Register structure
typedef struct _reg{
	int tag;
} reg;

//Generic node
typedef struct _CDBbus{
	int tag; 
	int reg; 
} CDBbus;

//Pointers for Linked List 
typedef struct _llPointers{
	node* head;
	node* tail;
	int size;
	int availExec;
} llPointers; 
//Linked list node
typedef struct _node{
	schedNode *next;
	schedNode *prev;
	proc_inst_t p_inst;
	int line_number;
	int destTag;
	int src1Tag;
	int src2Tag;
	int age;
} node;

//Pointers for circular FIFO array
typedef struct _FIFOPointers{
	int head;
	int tail;
} FIFOPointers; 
//Structure for ROB
typedef struct _ROB{
	proc_inst_t p_inst;
	int line_number;
	int destTag;
	int done;
} ROB;

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
llPointers dispatchPointers; 

//Scheudler
llPointers k0QueuePointers; 
llPointers k1QueuePointers; 
llPointers k2QueuePointers; 

//Execute
node** inK0;
node** inK1;
node** inK2;

//ROB Table for execution
ROB *ROBTable;
FIFOPointers ROBPointers; 

//array to represent CDB
CDBbus* CDB;
int CDBsize = 0;

/////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////ROB MANIPULATION//////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
/*
* statusROB
* Returns status of the ROB
*
* parameters: 
* none
*
* returns:
* int - status
*/
int statusROB(){
	if((ROBPointers.tail-ROBPointers.head)==1){
		return EMPTY;
	}else if (ROBPointers.tail==0 && ROBPointers.head==(r-1)){
		return EMPTY; 
	}else if(ROBPointers.head == ROBPointers.tail){
		return FULL;
	}else{
		return HAS_ROOM;
	}
}

/*
* addROB
* Adds element to ROB
*
* parameters: 
* node* dispatchNode - the node being added
*
* returns:
* int - tag into ROB, -1 if no room
*/
int addROB(node* dispatchNode){
	int tag;		//tag added to 

	if (ROBStatus!=FULL){			//if there is room in the ROB
		//Put item into ROB table
		ROBTable[ROBPointers.tail].line_number = dispatchNode->line_number;
		ROBTable[ROBPointers.tail].destTag = ROBPointers.tail;
		ROBTable[ROBPointers.tail].p_inst = dispatchNode->p_inst;
		ROBTable[ROBPointers.tail].done = 0;
		//Fix register file
		regFile[dispatchNode->p_inst.dest_reg] = ROBPointers.tail;
		//Fix pointers
		ROBPointers.tail = (ROBPointers.tail+1)%r;
	}else{
		return FALSE;
	}

	return tag;
}

/*
* updateROB
* updates ROB entry to done
*
* parameters: 
* int index - index that has completed
*
* returns:
* none
*/
void updateROB(int index){
	//Mark as complete
	ROBTable[index].done = 1; 
}

/*
* removeROB
* Removes head element from ROB
*
* parameters: 
* none
*
* returns:
* none
*/
void removeROB(){
	//Fix ROB queue
	ROBPointers.head = (ROBPointers.head+1)%r;
}

/////////////////////////////////////////////////////////////////////////////////////
///////////////////////LINKED LIST MANIPULATION//////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

/*
* addLL
* Adds element to tail of linked list
*
* parameters: 
* llPointers* pointersIn - pointers to linked list
* node* nextNode         - node to add
*
* returns:
* int - success
*/
int addLL(llPointers* pointersIn, node* nextNode){
	//Add new element if there is enough room
	if (pointersIn->size>0){
		pointersIn->size--;	//Decrease room

		//Fix next and prev pointers
		nextNode->prev = pointersIn->tail;
		nextNode->next = NULL;

		//Fix existing tail and head
		if (pointersIn->tail != NULL){
			pointersIn->tail->next = nextNode;
		}else{		//means it was empty before
			//Fix array pointers
			pointersIn->head = nextNode;
		}

		//Fix tail pointer
		pointersIn->tail = nextNode;
	}else{
		return FALSE;
	}

	return TRUE;
}

/*
* removeLL
* Removes element from linked list
*
* parameters: 
* llPointers* pointersIn - pointers to linked list
* node* deleteNode       - node to delete
* int freeMem			 - free memory
*
* returns:
* none
*/
void removeLL(llPointers* pointersIn, node* deleteNode, int freeMem){
	if (deleteNode->prev==NULL){			//Head element
		if (deleteNode->next==NULL){		//Special case where there is only 1 element
			pointersIn->head = NULL;
			pointersIn->tail = NULL;
		}else{
			pointersIn->head = deleteNode->next;
			pointersIn->head->prev = NULL;
		}
	}else if (deleteNode->next==NULL){		//Tail element
		pointersIn->tail = deleteNode->prev;
		pointersIn->tail->next = NULL;
	} else{									//Element in middle
		pointersIn->prev->next = deleteNode->next;
		pointersIn->next->prev = deleteNode->prev;		
	}
	//Add room to linked list
	pointersIn->size++;

	//Free the node if desired
	if (freeMem==TRUE){
		free(deleteNode);		//Free allocated memory
	}
}

/*
* createNode
* Creates node for dispatcher
*
* parameters: 
* int line_number    - line number of instruction
* proc_inst_t p_inst - instuction of the node
*
* returns:
* node* - node that has been created
*/
node* createNode(proc_inst_t p_inst, int line_number){
	node* newNode = (node*) malloc(sizeof(node));	//Create a new node

	//Copy over data
	newNode->p_inst = p_inst;
	newNode->line_number = line_number;

	//Add valididty data
	newNode->destTag = UNINITIALIZED;
	newNode->src1Tag = UNINITIALIZED;
	newNode->src2Tag = UNINITIALIZED;
	newNode->age = UNINITIALIZED;

	//Return Data
	return newNode; 
}

/*
* createNodeforSched
* Modifies node for use in scheduler
*
* parameters: 
* node* dispatchNode - node to be modified
* int tag 			 - tag of the node 
*
* returns:
* none
*/
void createNodeforSched(node* dispatchNode, int tag){

	//Copy over data
	dispatchNode->destTag = tag;

	//Add valididty data
	dispatchNode->src1Tag = regFile[dispatchNode->p_inst.src_reg[0]];
	dispatchNode->src2Tag = regFile[dispatchNode->p_inst.src_reg[1]];

	//Set so not in FU yet
	newNode->age = READY;

	//Return Data
	return newNode; 
}

/////////////////////////////////////////////////////////////////////////////////////
///////////////////////////PIPELINE INSTUCTIONS//////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

///////////////////////////INSTUCTION FETCH/DECODE///////////////////////////////////
void readInstructions(){
	int readFlag = 1;
	node* readNode;			//Node for new instructions

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
	 	regFile[i].tag = READY;
	 }

	 //Allocate array
	 ROBTable = (ROB*) malloc(r*sizeof(ROB));			//ROB
	 CDB = (CDBbus *) maclloc(r*sizeof(CDBbus));		//CDB
	 //Arrays to hold pointers to currently in FU
	 ink0 = (node**) malloc(k0*sizeof(node*))
	 ink1 = (node**) malloc(k1*sizeof(node*))
	 ink2 = (node**) malloc(k2*sizeof(node*))

	 //Initialize pointers
	 //ROB FIFO
	 ROBPointers = {0,1};
	 //LL Pointers
	 dispatchPointers = {0,0,r}; 
	 k0QueuePointers = {0,0,m*k0,k0};
	 k1QueuePointers = {0,0,m*k1 ,k1};
	 k2QueuePointers = {0,0,m*k2, k2};

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

	//Dispatcher flags
	int dispatcherFlag = 1;
	//SchedulerFlag
	int schedFlag = 1; 
	//State update flag
	int stateUpdate = 1;
	//line number
	int instruction = 1;
	int cycle = 0;

	//Instruction
	proc_inst_t* p_inst;
	proc_inst_t temp;

	//Pipeline
	while(flag){
		//////////////FIRST HALF OF CYCLE///////////////////////

		//////////////SECOND HALF OF CYCLE//////////////////////
		readInstructions();
		////////////////////////////////////////////////////////
		

		//Dispatcher
		//Reset dispatcherFlag
		node* dispatchNode = dispatchPointers.head;		//Node for instruction in dispatch queue
		node* dispatchNodeTemp; 
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
					dispatcherFlag = addArray(&k0QueuePointers, schedNodeAdd);	
				}else{
					dispatcherFlag = FALSE;
				}
			}else if(dispatchInstruction.op_code == 1 && arrayStatus(k1QueuePointers)==HAS_ROOM){
				tag = addROB(dispatchNode);		//Add to ROB
				//if added to ROB, add to scheduler
				if (tag!=FALSE){
					schedNodeAdd = createSchedNode(dispatchNode, tag);
					dispatcherFlag = addArray(&k1QueuePointers, schedNodeAdd);	
				}else{
					dispatcherFlag = FALSE;
				}
			}else if(dispatchInstruction.op_code == 2 && arrayStatus(k2QueuePointers)==HAS_ROOM){
				tag = addROB(dispatchNode);		//Add to ROB
				//if added to ROB, add to scheduler
				if (tag!=FALSE){
					schedNodeAdd = createSchedNode(dispatchNode, tag);
					dispatcherFlag = addArray(&k2QueuePointers, schedNodeAdd);	
				}else{
					dispatcherFlag = FALSE;
				}
			}else{
				dispatcherFlag = FALSE;
			}

			//If there was room in schedule add to ROB and remove from dispatcher
			if (dispatcherFlag!=FALSE){
				//Go to next item in scheduler
				dispatchNodeTemp = dispatchNode;
				dispatchNode = dispatchNode->next;
				//Remove item from dispatcher queue
				removeArray(&dispatchQueue, dispatchNodeTemp);
			}

		}

		//Scheduler
		schedNode* temp;
		schedNode* temp0 = k0QueuePointers.head;
		schedNode* temp1 = k1QueuePointers.head;
		schedNode* temp2 = k2QueuePointers.head;
		FUnode* functionalNode;
		do{

			//Check if registers are avaialable and functional unit is avaialable
			if (temp0 && k0QueuePointers.availExec>0 && temp0.src1Tag == -1 && temp0.src2Tag[1]==-1){
				//Add new node to list
				k0QueuePointers.availExec--;
				temp0.age  = 1;
				
				for (int j = 0; j<k0; j++){
					if (ink0[j]==NULL){
						ink0[j] = temp0;
					}
				}

				//Go to next element
				temp0 = temp0->next;
			}else{
				if (temp0 != NULL){
					//Go to next element
					temp0 = temp0->next;
				}
			}
			//Check if registers are avaialable and functional unit is avaialable
			if (temp1 && k1QueuePointers.availExec>0 && temp1.src1Tag == -1 && temp1.src2Tag[1]==-1){
				k1QueuePointers.availExec--;
				temp1.age  = 1;
				
				for (int j = 0; j<k0; j++){
					if (ink1[j]==NULL){
						ink1[j] = temp1;
					}
				}
				temp1 = temp1->next;
				//Remove previous item from array
				removeArray(&k1QueuePointers, temp);
			}else{
				if (temp1 != NULL){
					//Go to next element
					temp1 = temp1->next;
				}
			}
			//Check if registers are avaialable and functional unit is avaialable
			if (temp2 && k2QueuePointers.availExec>0 && temp2.src1Tag == -1 && temp2.src2Tag[1]==-1){
				k2QueuePointers.availExec--;
				temp2.age  = 1;
				
				for (int j = 0; j<k0; j++){
					if (ink2[j]==NULL){
						ink2[j] = temp2;
					}
				}				
				temp2 = temp2->next;
				//Remove previous item from array
				removeArray(&k2QueuePointers, temp);
			}else{
				if (temp2 != NULL){
					//Go to next element
					temp2 = temp2->next;
				}
			}
		} while(temp0 != NULL || temp1 != NULL || temp2 != NULL );

		//Execute
		FUnode* FUnodeCheck;
		FUnode* tempFUnode;
		//Execute k0 instructions
		FUnodeCheck = k0FUPointers.head;
		for (int i= 0; i<k0; i++){
			if (ink0[j] != NULL){
				//Decrease time left
				(*(ink0[j]))->age--;
				//Check if instructions is done
				if ((*(ink0[j]))->age == 0){
					CDB[CDBsize].tag = (*(ink0[j]))->destTag;
					CDB[CDBsize++].reg = (*(ink0[j]))->p_inst.dest;
					//Fix up FU array					
					(*(ink0[j]))->age = -1;
					ink0[j] = NULL;
				}
			}
		}
		//Execute k1 instructions
		FUnodeCheck = k1FUPointers.head;
		for (int i= 0; i<k1; i++){
			if (ink1[j] != NULL){
				//Decrease time left
				(*(ink1[j]))->age--;
				//Check if instructions is done
				if ((*(ink0[j]))->age == 0){
					CDB[CDBsize].tag = (*(ink0[j]))->destTag;
					CDB[CDBsize++].reg = (*(ink0[j]))->p_inst.dest;					
					//Fix up FU array					
					(*(ink1[j]))->age = -1;
					ink1[j] = NULL;
				}
			}
		}				
		FUnodeCheck = k2FUPointers.head;
		for (int i= 0; i<k2; i++){
			if (ink2[j] != NULL){
				//Decrease time left
				(*(ink2[j]))->age--;
				//Check if instructions is done
				if ((*(ink0[j]))->age == 0){
					CDB[CDBsize].tag = (*(ink0[j]))->destTag;
					CDB[CDBsize++].reg = (*(ink0[j]))->p_inst.dest;					
					//Fix up FU array					
					(*(ink2[j]))->age = -1;
					ink2[j] = NULL;
				}
			}
		}


		//Ordering of CDB
		CDB tempCDB;
		for(int i=0; i<CDBsize; i++){
          	for(int j=i; j<CDBsize; j++){
            	if(CDB[i] > CDB[j]){
               		tempCDB=CDB[i];
               		CDB[i]=CDB[j];
               		CDB[j]=tempCDB;
               	}
           	}
     	}

     	//Update the scheduler
     	schedNode* updateNode = k0FUPointers.head;
     	while (updateNode!=NULL){
     		for (int j = 0;j<CDBsize; j++){
     			if(CDB[j].tag==updateNode->src1Tag){
     				updateNode->src1Tag = -1;
     			}else if (CDB[j].tag==updateNode->src2Tag){
     				updateNode->src2Tag = -1;
     			}
     		}
     		updateNode = updateNode->next;
     	}
		schedNode* updateNode = k1FUPointers.head;
     	while (updateNode!=NULL){
     		for (int j = 0;j<CDBsize; j++){
     			if(CDB[j].tag==updateNode->src1Tag){
     				updateNode->src1Tag = -1;
     			}else if (CDB[j].tag==updateNode->src2Tag){
     				updateNode->src2Tag = -1;
     			}
     		}
     		updateNode = updateNode->next;
     	}schedNode* updateNode = k2FUPointers.head;
     	while (updateNode!=NULL){
     		for (int j = 0;j<CDBsize; j++){
     			if(CDB[j].tag==updateNode->src1Tag){
     				updateNode->src1Tag = -1;
     			}else if (CDB[j].tag==updateNode->src2Tag){
     				updateNode->src2Tag = -1;
     			}
     		}
     		updateNode = updateNode->next;
     	}
		//Commit
		//update the ROB			
		for (int i= 0; i < CDBsize; i++){
			updateROB(CDB[i].tag);
		}

		//Update register file
		for (int i= 0; i < CDBsize; i++){
			if (regFile[CDB[i].reg] == CDB[i].tag){
				regFile[CDB[i].reg] = -1;
			}
		}

		int indexROB;
		for (i = 0; i<r && stateUpdate; i++){
			indexROB = (ROBPointers.head + i)%r;
			//check if it is valid and remove if it is
			if (ROBTable[indexROB].done ==1){
				remove from scheduler
				removeROB();
			}else{		//if not valid, stop removing
				break;
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
	//Free allocated memory
	free(CDB);
	free(ROB);
	free(ink0);
	free(ink1);
	free(ink2);
}
