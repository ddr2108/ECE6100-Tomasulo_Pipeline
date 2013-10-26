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
#define READY         -3
#define DONE          -4

//Register structure
typedef struct _reg{
	int tag;
} reg;

//CDB node
typedef struct _CDBbus{
	int line_number;
	int tag; 
	int reg;
	int FU; 
} CDBbus;

//Linked list node
typedef struct _node{
	_node *next;
	_node *prev;
	proc_inst_t p_inst;
	int line_number;
	int destTag;
	int src1Tag;
	int src2Tag;
	int age;
	int fetch;
	int disp;
	int sched;
	int exec;
	int state;
	int retire;
} node;
//Pointers for Linked List 
typedef struct _llPointers{
	node* head;
	node* tail;
	int size;
	int availExec;
} llPointers; 


//Structure for ROB
typedef struct _ROB{
	proc_inst_t p_inst;
	int line_number;
	int destTag;
	int done;
	int fetch;
	int disp;
	int sched;
	int exec;
	int state;
	int retire;
} ROB;
//Pointers for circular FIFO array
typedef struct _FIFOPointers{
	int head;
	int tail;
	int size;
} FIFOPointers; 


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

//Holds line number
int instruction;
//File done flag
int readDoneFlag  = 1;
int flag = 1; 
//Clock
int cycle = 1;
/////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////ROB MANIPULATION//////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
/*
* printROB
* Prints information in node
*
* parameters: 
* int index - index to print
*
* returns:
* none
*/
void printROB(int index){
	printf("%d %d %d %d %d %d %d\n", ROBTable[index].line_number, ROBTable[index].fetch, ROBTable[index].disp, ROBTable[index].sched, ROBTable[index].exec, ROBTable[index].state, ROBTable[index].retire);
}

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
	if(ROBPointers.tail==ROBPointers.head && ROBPointers.size == 0){
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
	int tag = ROBPointers.tail;		//tag added to 

	if (statusROB()!=FULL){			//if there is room in the ROB
		//Put item into ROB table
		ROBTable[ROBPointers.tail].line_number = dispatchNode->line_number;
		ROBTable[ROBPointers.tail].destTag = ROBPointers.tail;
		ROBTable[ROBPointers.tail].p_inst = dispatchNode->p_inst;
		ROBTable[ROBPointers.tail].done = 0;
		//Fix register file
		if (dispatchNode->p_inst.dest_reg!=-1){
			regFile[dispatchNode->p_inst.dest_reg].tag = ROBPointers.tail;
		}
		//Fix pointers
		ROBPointers.tail = (ROBPointers.tail+1)%r;
		ROBPointers.size++;
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
	//Update stats
	ROBTable[ROBPointers.head].retire = cycle;
	//Print stats
	printROB(ROBPointers.head);

	ROBTable[ROBPointers.head].done = 0; 

	//Fix ROB queue
	ROBPointers.head = (ROBPointers.head+1)%r;
	ROBPointers.size--;
}

/////////////////////////////////////////////////////////////////////////////////////
///////////////////////LINKED LIST MANIPULATION//////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
/*
* updateROBfromNode
* Puts infor in node to ROB
*
* parameters: 
* node* print
*
* returns:
* none
*/
void updateROBfromNode(node* update){
	ROBTable[update->destTag].line_number = update->line_number;
	ROBTable[update->destTag].fetch = update->fetch;
	ROBTable[update->destTag].disp = update->disp;
	ROBTable[update->destTag].sched = update->sched;
	ROBTable[update->destTag].exec = update->exec;
	ROBTable[update->destTag].state = update->state;
	ROBTable[update->destTag].retire = update->retire;
}

/*
* statusLL
* Returns status of the linked list
*
* parameters: 
* llPointers* pointersIn - pointer to lined list
*
* returns:
* int - status
*/
int statusLL(llPointers pointersIn){
	if (pointersIn.size==0){
		return FULL; 
	}else if(pointersIn.head == NULL){
		return EMPTY;
	}else{
		return HAS_ROOM;
	}
}

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
		deleteNode->prev->next = deleteNode->next;
		deleteNode->next->prev = deleteNode->prev;		
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
* proc_inst_t p_inst - instruction of the node
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
*
* returns:
* none
*/
void createNodeforSched(node* dispatchNode){

	//Add valididty data
	if (dispatchNode->p_inst.src_reg[0]!=-1){
		dispatchNode->src1Tag = regFile[dispatchNode->p_inst.src_reg[0]].tag;
	}else{
		dispatchNode->src1Tag = READY;
	}
	if (dispatchNode->p_inst.src_reg[1]!=-1){
		dispatchNode->src2Tag = regFile[dispatchNode->p_inst.src_reg[1]].tag;
	}else{
		dispatchNode->src2Tag = READY;
	}

	dispatchNode->sched = cycle+1;

	//Set so not in FU yet
	dispatchNode->age = READY;
}

/////////////////////////////////////////////////////////////////////////////////////
///////////////////////////PIPELINE INSTUCTIONS//////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

///////////////////////////INSTUCTION FETCH/DECODE///////////////////////////////////

/*
* fetchInstructions
* Fetch Instcutions
*
* parameters: 
* none 
*
* returns:
* none
*/
void fetchInstructions(){
	//Node of new instruction
	node* readNode;
	//Instruction
	proc_inst_t* p_inst;
	//Flag
	int readFlag = TRUE;

	//Allocate memory for isntructions
	p_inst = (proc_inst_t*) malloc(sizeof(proc_inst_t));

	//Fetch F instructions at a time
	for (int i = 0; i<f && readFlag==TRUE; i++){

		if (dispatchPointers.size > 0){		//if there is room in dispatcher queue

			//Read in  instruction
			readFlag = read_instruction(p_inst);									//fetch instruction
			
			//Check if end of file reached
			if (readFlag==TRUE){		//If thre is an instruction
				instruction++;									
				//Create new node
				readNode = createNode(*p_inst, instruction);
				readNode->fetch = cycle;
				readNode->disp = cycle + 1;
				//Add node to list of instructions
				addLL(&dispatchPointers, readNode);	//add to dispatch queue
				//Increment instruction number
			}else{
				readDoneFlag = 0;
			}
	
		}else{
			break;
		}
	}

	//Free memory
	free(p_inst);
}

///////////////////////////DISPATCH///////////////////////////////////
/*
* dispatchInstructions
* Dispatch Instcutions
*
* parameters: 
* none 
*
* returns:
* none
*/
void dispatchInstructions(){
	//Node
	node* dispatchNode;
	node* dispatchNodeTemp; 
	//Flag
	int dispatcherFlag = TRUE;
	//Instruction being looked at
	proc_inst_t instructionDispatch;
	//Tag;
	int tag; 

	//Initialize initial node to be dispatched
	dispatchNode = dispatchPointers.head;		//Node for instruction in dispatch queue

	//Read from dispatch queue
	while(dispatcherFlag!=FALSE && statusLL(dispatchPointers)!=EMPTY && dispatchNode!=NULL){

		instructionDispatch = dispatchNode->p_inst; 	//Get instruction

		//add to correct scheduling queue and ROB and remove from dispatcher
		if ((instructionDispatch.op_code == 0 || instructionDispatch.op_code == -1 ) && statusLL(k0QueuePointers)!=FULL){
			if (statusROB()!=FULL){
				//Add new data	
				createNodeforSched(dispatchNode);
				//Add to ROB
				tag = addROB(dispatchNode);	
				//Copy over data
				dispatchNode->destTag = tag;

				//Go to next item in scheduler
				dispatchNodeTemp = dispatchNode;
				dispatchNode = dispatchNode->next;
				//Remove item from dispatcher queue
				removeLL(&dispatchPointers, dispatchNodeTemp, FALSE);

				//Add to queue linked list
				addLL(&k0QueuePointers, dispatchNodeTemp);	
			}else{
				//if ROB full, stop dispatch
				dispatcherFlag = FALSE;
			}
		}else if(instructionDispatch.op_code == 1 && statusLL(k1QueuePointers)!=FULL){
			if (statusROB()!=FULL){
				//Add new data	
				createNodeforSched(dispatchNode);
				//Add to ROB
				tag = addROB(dispatchNode);	
				//Copy over data
				dispatchNode->destTag = tag;

				//Go to next item in scheduler
				dispatchNodeTemp = dispatchNode;
				dispatchNode = dispatchNode->next;
				//Remove item from dispatcher queue
				removeLL(&dispatchPointers, dispatchNodeTemp, FALSE);

				//Add to queue linked list
				addLL(&k1QueuePointers, dispatchNodeTemp);	
			}else{
				//if ROB full, stop dispatch
				dispatcherFlag = FALSE;
			}
		}else if(instructionDispatch.op_code == 2 && statusLL(k2QueuePointers)!=FULL){
			if (statusROB()!=FULL){
				//Add new data	
				createNodeforSched(dispatchNode);
				//Add to ROB
				tag = addROB(dispatchNode);	
				//Copy over data
				dispatchNode->destTag = tag;

				//Go to next item in scheduler
				dispatchNodeTemp = dispatchNode;
				dispatchNode = dispatchNode->next;
				//Remove item from dispatcher queue
				removeLL(&dispatchPointers, dispatchNodeTemp, FALSE);

				//Add to queue linked list
				addLL(&k2QueuePointers, dispatchNodeTemp);	
			}else{
				//if ROB full, stop dispatch
				dispatcherFlag = FALSE;
			}
		}else{
			dispatcherFlag = FALSE;
		}


	}
}

///////////////////////////SCHEDULE///////////////////////////////////

/*
* scheduleUpdate
* Updates scheduler with new values
*
* parameters: 
* none 
*
* returns:
* none
*/
void scheduleUpdate(){
 	//temporary node
 	node* updateNode; 

 	//update k0 queue
 	updateNode = k0QueuePointers.head;
 	while (updateNode!=NULL){
 		//go through CDB
 		for (int j = 0;j<CDBsize; j++){
 			if(CDB[j].tag==updateNode->src1Tag){
 				updateNode->src1Tag = READY;
 			} 
 			if (CDB[j].tag==updateNode->src2Tag){
 				updateNode->src2Tag = READY;
 			}
 		}
 		//go to next element
 		updateNode = updateNode->next;
 	}

 	//update k1 queue
 	updateNode = k1QueuePointers.head;
 	while (updateNode!=NULL){
 		//go through CDB
 		for (int j = 0;j<CDBsize; j++){
 			if(CDB[j].tag==updateNode->src1Tag){
 				updateNode->src1Tag = READY;
 			}
 			if (CDB[j].tag==updateNode->src2Tag){
 				updateNode->src2Tag = READY;
 			}
 		}
  		//go to next element
 		updateNode = updateNode->next;
 	}

 	//update k2 queue
 	updateNode = k2QueuePointers.head;
 	while (updateNode!=NULL){
 		//go through CDB
 		for (int j = 0;j<CDBsize; j++){
 			if(CDB[j].tag==updateNode->src1Tag){
 				updateNode->src1Tag = READY;
 			}
 			if (CDB[j].tag==updateNode->src2Tag){
 				updateNode->src2Tag = READY;
 			}
 		}
  		//go to next element
 		updateNode = updateNode->next;
 	}
}


/*
* scheduleInstructionstoFU
* Schedule Instrutions to FU
*
* parameters: 
* none 
*
* returns:
* none
*/
void scheduleInstructionstoFU(){
	//Scheduler
	node* temp0 = k0QueuePointers.head;
	node* temp1 = k1QueuePointers.head;
	node* temp2 = k2QueuePointers.head;

	//Do while there is room in all schedulers
	while(temp0 != NULL || temp1 != NULL || temp2 != NULL ){

		//Check if item can be put in k0 execute
		if (temp0!=NULL && k0QueuePointers.availExec>0 && temp0->src1Tag == READY && temp0->src2Tag == READY && temp0->age == READY){
			//Add new node to list
			k0QueuePointers.availExec--;
			temp0->age  = 1;
	
			//Add cycle info
			temp0->exec = cycle+1;

			//Store pointers for things currently in FU	
			for (int j = 0; j<k0; j++){
				if (inK0[j]==NULL){
					inK0[j] = temp0;
					break;
				}
			}
	
			//Go to next element
			temp0 = temp0->next;
		}else if (temp0 != NULL){
			//Go to next element
			temp0 = temp0->next;
		}

		//Check if item can be put in k1 execute
		if (temp1!=NULL && k1QueuePointers.availExec>0 && temp1->src1Tag == READY && temp1->src2Tag== READY && temp1->age == READY){
			//Add new node to list
			k1QueuePointers.availExec--;
			temp1->age  = 2;
			
			//Add cycle info
			temp1->exec = cycle+1;

			//Store pointers for things currently in FU	
			for (int j = 0; j<k1; j++){
				if (inK1[j]==NULL){
					inK1[j] = temp1;
					break;
				}
			}

			//Go to next element
			temp1 = temp1->next;
		}else if (temp1 != NULL){
			//Go to next element
			temp1 = temp1->next;
		}
		

		//Check if item can be put in k2 execute
		if (temp2!=NULL && k2QueuePointers.availExec>0 && temp2->src1Tag == READY && temp2->src2Tag== READY && temp2->age == READY){
			//Add new node to list
			k2QueuePointers.availExec--;
			temp2->age  = 3;

			//Add cycle info
			temp2->exec = cycle+1;

			//Store pointers for things currently in FU	
			for (int j = 0; j<k2; j++){
				if (inK2[j]==NULL){
					inK2[j] = temp2;
					break;
				}
			}	

			//Go to next element
			temp2 = temp2->next;
		}else if (temp2 != NULL){
			//Go to next element
			temp2 = temp2->next;
		}
	}

}

/*
* scheduleInstructions2
* Schedule Instrutions
*
* parameters: 
* none 
*
* returns:
* none
*/
void scheduleInstructions1(){
	//Put on FU
	scheduleInstructionstoFU();
}
/*
* scheduleInstructions1
* Schedule Instrutions
*
* parameters: 
* none 
*
* returns:
* none
*/
void scheduleInstructions2(){
	scheduleUpdate();
}

///////////////////////////EXECUTE///////////////////////////////////
/*
* updateReg
* Update registers
*
* parameters: 
* none 
*
* returns:
* none
*/
void updateReg(){
	//Update register file
	for (int i = 0; i < CDBsize; i++){
		if (regFile[CDB[i].reg].tag == CDB[i].tag){
			regFile[CDB[i].reg].tag = READY;
		}
	}
}

/*
* removeFU
* Removes Items from functional units
*
* parameters: 
* none 
*
* returns:
* none
*/
void removeFU(){
	//Execute k0 instructions
	for (int j= 0; j<k0; j++){
		if (inK0[j] != NULL){
			//Check if instructions is done
			if (inK0[j]->age == DONE){
				k0QueuePointers.availExec++;
				inK0[j] = NULL;
			}
		}
	}

	//Execute k1 instructions
	for (int j= 0; j<k1; j++){
		if (inK1[j] != NULL){
			//Check if instructions is done
			if (inK1[j]->age == DONE){
				k1QueuePointers.availExec++;
				inK1[j] = NULL;
			}
		}
	}	

	//Execute k2 instructions
	for (int j= 0; j<k2; j++){
		if (inK2[j] != NULL){
			//Check if instructions is done
			if (inK2[j]->age == DONE){
				k2QueuePointers.availExec++;
				inK2[j] = NULL;
			}
		}
	}
}

/*
* incrementTimer
* Increment Age
*
* parameters: 
* none 
*
* returns:
* none
*/
void incrementTimer(){
	//Reset CDB bus
	CDBsize = 0;

	//Execute k0 instructions
	for (int j= 0; j<k0; j++){
		if (inK0[j] != NULL){
			//Decrease time left
			inK0[j]->age--;
			//Check if instructions is done
			if (inK0[j]->age == 0){
				CDB[CDBsize].tag = inK0[j]->destTag;
				CDB[CDBsize].FU = 0;
				CDB[CDBsize].line_number = inK0[j]->line_number;
				CDB[CDBsize++].reg = inK0[j]->p_inst.dest_reg;
			//	printf("done - tag:%d, line:%d, cycle:%d\n", CDB[CDBsize-1].tag, CDB[CDBsize-1].line_number, cycle);
				//Add cycle info
				inK0[j]->state = cycle+1;
				//Fix up FU array
				inK0[j]->age = DONE;
			}
		}
	}

	//Execute k1 instructions
	for (int j= 0; j<k1; j++){
		if (inK1[j] != NULL){
			//Decrease time left
			inK1[j]->age--;
			//Check if instructions is done
			if (inK1[j]->age == 0){
				CDB[CDBsize].tag = inK1[j]->destTag;
				CDB[CDBsize].FU = 1;
				CDB[CDBsize].line_number = inK1[j]->line_number;
				CDB[CDBsize++].reg = inK1[j]->p_inst.dest_reg;	
			//	printf("done - tag:%d, line:%d, cycle:%d\n", CDB[CDBsize-1].tag, CDB[CDBsize-1].line_number, cycle);

				//Add cycle info
				inK1[j]->state = cycle+1;				
				//Fix up FU array					
				inK1[j]->age = DONE;
			}
		}
	}	

	//Execute k2 instructions
	for (int j= 0; j<k2; j++){
		if (inK2[j] != NULL){
			//Decrease time left
			inK2[j]->age--;
			//Check if instructions is done
			if (inK2[j]->age == 0){
				CDB[CDBsize].tag = inK2[j]->destTag;
				CDB[CDBsize].FU = 2;
				CDB[CDBsize].line_number = inK2[j]->line_number;
				CDB[CDBsize++].reg = inK2[j]->p_inst.dest_reg;
								printf("%d, %d, %d\n", CDB[CDBsize-1].tag, CDB[CDBsize-1].FU, CDB[CDBsize-1].line_number);

				//Add cycle info
				inK2[j]->state = cycle+1;					
				//Fix up FU array					
				inK2[j]->age = DONE;
			}
		}
	}

}

/*
* orderCDB
* Order CDB
*
* parameters: 
* none 
*
* returns:
* none
*/
void orderCDB(){
	//Temporary CDB holder
	CDBbus tempCDB;

	//Go though all elements and switch as necessary
	for(int i=0; i<CDBsize; i++){
        for(int j=i; j<CDBsize; j++){
           	if(CDB[i].line_number > CDB[j].line_number){
           		tempCDB=CDB[i];
 	      		CDB[i]=CDB[j];
               	CDB[j]=tempCDB;
           	}
        }
    }

}

/*
* executeInstructions1
* Execute Instcutions
*
* parameters: 
* none 
*
* returns:
* none
*/
void executeInstructions1(){
	//Fix aging of instructions in execute
	incrementTimer();
	//Order the CDB
	orderCDB();
	//Update register
	updateReg();
}

/*
* executeInstructions2
* Execute Instcutions
*
* parameters: 
* none 
*
* returns:
* none
*/
void executeInstructions2(){
	removeFU();
}
///////////////////////////STATE UPDATE////////////////////////////////
/*
* markROBDone
* Mark instructions done in ROB
*
* parameters: 
* none 
*
* returns:
* none
*/
void markROBDone(){
	for (int i= 0; i < CDBsize; i++){
		//printf("update - tag:%d cycle:%d\n", CDB[i].tag, cycle);
		updateROB(CDB[i].tag);
	}
}

/*
* removeScheduler
* Remove completed instructions from scheduler
*
* parameters: 
* none 
*
* returns:
* none
*/
void removeScheduler(){
	//Node for access to scheduler
	node* updateNode;

	for(int j = 0;j<CDBsize; j++){
		if (CDB[j].FU == 0){
			//Navigate though k0 scheduler
			updateNode = k0QueuePointers.head;
 			while (updateNode!=NULL){
 				if (CDB[j].tag==updateNode->destTag){
 					updateNode->retire = cycle;
 					updateROBfromNode(updateNode);
 					removeLL(&k0QueuePointers, updateNode,TRUE);
 					break;
 				}
 				//go to next node
 				updateNode = updateNode->next;
			}
		}else if(CDB[j].FU == 1){
			//Navigate though k0 scheduler
			updateNode = k1QueuePointers.head;
 			while (updateNode!=NULL){
 				if (CDB[j].tag==updateNode->destTag){
 					updateNode->retire = cycle;
 					updateROBfromNode(updateNode);
 					removeLL(&k1QueuePointers, updateNode,TRUE);
 					break;
 				}
 				//go to next node
 				updateNode = updateNode->next;
			}
		}else if (CDB[j].FU == 2){
			//Navigate though k0 scheduler
			updateNode = k2QueuePointers.head;
 			while (updateNode!=NULL){
 				if (CDB[j].tag==updateNode->destTag){
 					updateNode->retire = cycle;
 					updateROBfromNode(updateNode);
 					removeLL(&k2QueuePointers, updateNode,TRUE);
 					break;
 				}
 				//go to next node
 				updateNode = updateNode->next;
			}

		}
	}
}

/*
* retireInstructions
* Retire completed instruction in ROB
*
* parameters: 
* none 
*
* returns:
* none
*/
void retireInstructions(){
	int indexROB;
	int initHead = ROBPointers.head;
	int initSize = ROBPointers.size;
	//Retire as many instructions as possible
	for (int i = 0; i<initSize; i++){
		indexROB = (initHead + i)%r;
		//check if it is valid and remove if it is
		if (ROBTable[indexROB].done ==1){
		//			printf("retire - tag:%d cycle:%d\n", indexROB, cycle);

			removeROB();
		}else{		//if not done, stop removing
			break;
		}
	}

	//all instructions done
	if (readDoneFlag == 0 && statusROB()==EMPTY){
		flag = 0;
	}
}


/*
* updateState1
* Update States
*
* parameters: 
* none 
*
* returns:
* none
*/
void updateState1(){
	retireInstructions();
	markROBDone();
}

/*
* updateState2
* Update States
*
* parameters: 
* none 
*
* returns:
* none
*/
void updateState2(){
	removeScheduler();
}
/////////////////////////////////////////////////////////////////////////////////////
///////////////////////////PIPELINE DRIVERS//////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////


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
	 CDB = (CDBbus *) malloc((k0+k1+k2+10)*sizeof(CDBbus));		//CDB
	 //Arrays to hold pointers to currently in FU
	 inK0 = (node**) malloc(k0*sizeof(node*));
	 inK1 = (node**) malloc(k1*sizeof(node*));
	 inK2 = (node**) malloc(k2*sizeof(node*));

	 //Initialize pointers
	 //ROB FIFO
	 ROBPointers = {0,0,0};
	 //LL Pointers
	 dispatchPointers = {NULL, NULL, (int)r      , (int)0}; 
	 k0QueuePointers =  {NULL, NULL, (int)(m*k0) , (int)k0};
	 k1QueuePointers =  {NULL, NULL, (int)(m*k1) , (int)k1};
	 k2QueuePointers =  {NULL, NULL, (int)(m*k2) , (int)k2};

}

/**
 * Subroutine that simulates the processor.
 *   The processor should fetch instructions as appropriate, until all instructions have executed
 * XXX: You're responsible for completing this routine
 *
 * @p_stats Pointer to the statistics structure
 */
void run_proc(proc_stats_t* p_stats) {
	//Cycle timer
	cycle = 0;

	//Line number
	instruction = 0;

	//Pipeline
	while(flag){
		//Change clock cycle
		cycle++;

		//////////////FIRST HALF OF CYCLE///////////////////////
		//SU1
		updateState1();
		//EXEC1
		executeInstructions1();
		//SCHED1
		scheduleInstructions1();
		//DISPATCH
		dispatchInstructions();
		//FETCH
		fetchInstructions();
		//////////////SECOND HALF OF CYCLE//////////////////////
		//SU2
		updateState2();
		//EXEC1
		executeInstructions2();
		//SCHED1
		scheduleInstructions2();
		////////////////////////////////////////////////////////
	}
}

/**
 * Subroutine for cleaning up any outstanding instructions and calculating overall statistics
 * such as average IPC or branch prediction percentage
 * XXX: You're responsible for completing this routine
 *
 * @p_stats Pointer to the statistics structure
 */
void complete_proc(proc_stats_t *p_stats) {
	//stats
	p_stats->retired_instruction = instruction;
	p_stats->cycle_count = cycle;
	p_stats->avg_ipc = instruction/cycle;

	//Free allocated memory
	free(CDB);
	free(ROBTable);
	free(inK0);
	free(inK1);
	free(inK2);
}
