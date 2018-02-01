/* ------------------------------------------------------------------------
 phase1.c

 University of Arizona
 Computer Science 452
 Spring 2018
 Students: Sean Pearson
 Dong Liang

 ------------------------------------------------------------------------ */

#include "phase1.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "kernel.h"

/* ------------------------- Prototypes ----------------------------------- */
int sentinel(char *);
void dispatcher(void);
void launch();
static void checkDeadlock();

/* -------------------------- Globals ------------------------------------- */

// Patrick's debugging global variable...
int debugflag = 1;
int other_debug_flag = 1;
// the process table
procStruct ProcTable[MAXPROC];

// Process lists
static procPtr ReadyList;

// current process ID
procPtr Current;
procPtr Previous;
// the next pid to be assigned
unsigned int nextPid = SENTINELPID;

/* -------------------------- Functions ----------------------------------- */

/*_____________OUR_FUNCTIONS________________*/
/* ------------------------------------------------------------------------
 Name - currentMode
 Purpose - returns which mode you are in.
 Parameters -
 Returns - 1 is kernal, 0 is user, and -1 is error
 Side Effects -
 ----------------------------------------------------------------------- */

int currentMode() {
	union psrValues psr;
	psr.integerPart = USLOSS_PsrGet();
	return psr.bits.curMode;
}

/*
 * CPUtime---return the time of CPU used by the current process
 */

// int CPUtime() {
// if(USLOSS_DeviceInput(USLOSS_CLOCK_DEV, 0, -1) == USLOSS_DEV_OK)
// return -1;
// else
// return USLOSS_DEV_INVALID;
// }

void DisableInterrupts() {
	if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
		USLOSS_Console("Error:Not in the kernel mode.");
		USLOSS_Halt(1);
	}      //if it is not in the kernel mode
	else {
		// USLOSS_PsrSet( USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_INT );
	}      //in the kernel mode
}

void EnableInterrupts() {
	if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
		USLOSS_Console("Error:Not in the kernel mode.");
		USLOSS_Halt(1);
	}      //if it is not in the kernel mode
	else {
		//USLOSS_PsrSet( USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
	}      //in the kernel mode
}

int isZapped() {
	return Current->zapped;
}

/*
 *Free everything for the input procPtr.
 */
void freePtr(procPtr p) {
	// empty out everything
	p->nextProcPtr = NULL;
	p->childProcPtr = NULL;
	p->nextSiblingPtr = NULL;
	p->quitChild = NULL;
	strcpy(p->name, "");
	p->startArg[0] = '\0';
	p->pid = -1;
	p->parentpid = -1;
	p->priority = -1;
	p->startFunc = NULL;
	p->stack = NULL;
	p->stackSize = -1;
	p->status = 0;
	p->cpuTime = -1;
	p->children = 0;
	p->quitVal = -1;
  p->time = -1;
	for (int i = 0; i < MAXPROC; i++)
		p->zapList[i] = NULL;

}

void readyListEntry(procPtr entry) {
	//sort by priority
	//insert sort
	//ReadyList will have initial blank head ptr.
	if (ReadyList == NULL) {
		ReadyList = entry;
	} else {
		procPtr next = ReadyList;
		while (next->nextInReadyList != NULL && next->nextInReadyList->priority > entry->priority);
        if (next->nextInReadyList != NULL) {
				next->nextInReadyList = entry;
		} else {
			procPtr tmp = next->nextInReadyList;
			next->nextInReadyList = entry;
			next->nextInReadyList->nextInReadyList = tmp;
        }
	}

}
void readyListRemove(procPtr entry) {
	//ReadyList will have initial blank head ptr->
	procPtr next = ReadyList;
	while (next->nextInReadyList != NULL && next->nextInReadyList != entry) {
		if (next->nextInReadyList != NULL) {
			exit(-1);
		} else {
			if (next->nextInReadyList->nextInReadyList != NULL) {
				next->nextInReadyList = next->nextInReadyList->nextInReadyList;
			} else {
				next->nextInReadyList = NULL;
			}
		}
	}
}

/*
 *Return values for zap(pid):
 *-1:   the calling process itself was zapped while in zap.
 *0:    the zapped process has called quit.
 */
int zap(int pid){

   /*Cases that will print out errors*/
   if(Current->pid == pid)
   {
    USLOSS_Console("Error:In zap(), the process %d tried to zap itself.\n", Current->pid);
    USLOSS_Halt(1);
   }
   if(ProcTable[pid % 50].status == -1) {
    USLOSS_Console("Error:In zap(), the process %d attempts to zap a nonexistent process.\n", Current->pid);
    USLOSS_Halt(1);
   }


   procPtr zapped = &ProcTable[pid % 50];
   int i =0;
   while(zapped->zapList[i]!=NULL)
	i++;

   zapped->zapList[i]=Current;
   zapped->zapped=1;

   Current->status=3;//Or blocked by zap.
   //Since blocked,we call dispatcher
   dispatcher();


/*
 *Return values:
 *0:    the calling process has not been zapped.
 *1:    the calling process has been zapped.
 *	notes from phase1-v1.0
 */
   if(Current->zapped)
   return -1;
   else
   return 0;
}/*int zap(pid)*/





/*--------------------------OUR-FUNCTIONS-END-------------------------------------*/

/* ------------------------------------------------------------------------
 Name - startup
 Purpose - Initializes process lists and clock interrupt vector.
 Start up sentinel process and the test process.
 Parameters - argc and argv passed in by USLOSS
 Returns - nothing
 Side Effects - lots, starts the whole thing
 Jan 17 (Not yet done)
 ----------------------------------------------------------------------- */

void startup(int argc, char *argv[]) {
	if (currentMode() != 1) {
		if (other_debug_flag && debugflag)
			printf("You are not in kernal mode");
		USLOSS_Halt(1);
	} else {
		EnableInterrupts();
	}
	int result; /* value returned by call to fork1() */

	/* initialize the process table */
	if (other_debug_flag && debugflag)
		USLOSS_Console("startup(): initializing process table, ProcTable[]\n");
	for (int i = 0; i < MAXPROC; i++) {
		ProcTable[i].stack = NULL;
		ProcTable[i].nextProcPtr = NULL;
		ProcTable[i].childProcPtr = NULL;
		ProcTable[i].nextInReadyList = NULL;
		ProcTable[i].name[0] = '\0';
		ProcTable[i].startArg[0] = '\0';
		//ProcTable[i].state = NULL;
		ProcTable[i].pid = -1;
		ProcTable[i].priority = -1;
		ProcTable[i].stackSize = 0;
		ProcTable[i].status = 0; // 0 is UNUSED
		ProcTable[i].quitChild = NULL;
		ProcTable[i].quitVal = -1;
		ProcTable[i].parentpid = -1;
		ProcTable[i].cpuTime = 0;
		ProcTable[i].children = 0;
		ProcTable[i].zapped = -1;
		ProcTable[i].zapList[0] = '\0';

	}
	/////////////////////

	// Initialize the Ready list, etc.
	if (other_debug_flag && debugflag)
		USLOSS_Console("startup(): initializing the Ready list\n");
	ReadyList = NULL;

	// Initialize the clock interrupt handler
	// USLOSS_IntVec[USLOSS_ILLEGAL_INT] = interruptCase;
	// USLOSS_IntVec[USLOSS_ALARM_INT] = interruptCase;
	EnableInterrupts();

	// startup a sentinel process
	if (other_debug_flag && debugflag)
		USLOSS_Console("startup(): calling fork1() for sentinel\n");
	result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK,
			SENTINELPRIORITY);
	if (result < 0) {
		if (other_debug_flag && debugflag) {
			USLOSS_Console("startup(): fork1 of sentinel returned error, ");
			USLOSS_Console("halting...\n");
		}
		USLOSS_Halt(1);
	}

	// start the test process
	if (other_debug_flag && debugflag)
		USLOSS_Console("startup(): calling fork1() for start1\n");
	result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1); //office hours
	if (result < 0) {
		USLOSS_Console("startup(): fork1 for start1 returned an error, ");
		USLOSS_Console("halting...\n");
		USLOSS_Halt(1);
	}

	USLOSS_Console("startup(): Should not see this message! \n");
	USLOSS_Console("Returned from fork1 call that created start1\n");

	return;
} /* startup */

/* ------------------------------------------------------------------------
 Name - finish
 Purpose - Required by USLOSS
 Parameters - none
 Returns - nothing
 Side Effects - none
 ----------------------------------------------------------------------- */
void finish(int argc, char *argv[]) {
	if (currentMode() != 1) {
		if (other_debug_flag && debugflag)
			printf("You are not in kernal mode");
		USLOSS_Halt(1);
	} else {
		EnableInterrupts();
	}

	if (other_debug_flag && debugflag)
		USLOSS_Console("in finish...\n");
} /* finish */

/* ------------------------------------------------------------------------
 Name - fork1
 Purpose - Gets a new process from the process table and initializes
 information of the process.  Updates information in the
 parent process to reflect this child process creation.
 Parameters - the process procedure address, the size of the stack and
 the priority to be assigned to the child process.
 Returns - the process id of the created child or -1 if no child could
 be created or if priority is not between max and min priority.
 Side Effects - ReadyList is changed, ProcTable is changed, Current
 process information changed
 ------------------------------------------------------------------------ */
int fork1(char *name, int (*startFunc)(char *), char *arg, int stacksize,
		int priority) {

	if (other_debug_flag && debugflag)
		USLOSS_Console("fork1(): creating process %s\n", name);

	// test if in kernel mode; halt if in user mode
	if (currentMode() != 1) {
		printf("You are not in kernal mode");
		USLOSS_Halt(1);
	} else {
		EnableInterrupts();
	}

	// Return if stack size is too small
	if (stacksize < USLOSS_MIN_STACK) {
		return -2;
	}

	// Is there room in the process table? What is the next PID?
	int ProcSpace = -1;
	for (int i = 0; i < MAXPROC; i++) {
		if (ProcTable[(nextPid + i) % MAXPROC].status == 0) { //checking if it is unused.
			ProcSpace = (nextPid + i);
			break;
		}
		if (i == MAXPROC - 1) {
			return -1;
		}
	}
	nextPid = ProcSpace + 1; //This ensures that you are increasing the pid and jumping to the next possibe.

	/* fill-in entry in process table */
	procStruct *entry = &(ProcTable[ProcSpace]);
	entry->childProcPtr = NULL;
	entry->nextSiblingPtr = NULL;
	entry->stack = malloc(stacksize);
	entry->pid = ProcSpace;
	entry->priority = priority;
	entry->stackSize = stacksize;
	entry->status = 0;
	entry->quitChild = NULL;
	entry->quitVal = -1;
	entry->parentpid = -1;
	entry->cpuTime = 0;
	entry->children = 0;
	entry->zapped = -1;
	entry->zapList[0] = '\0';
  entry->time = 0;

	if (strlen(name) >= (MAXNAME - 1)) {
		USLOSS_Console("fork1(): Process name is too long.  Halting...\n");
		USLOSS_Halt(1);
	}
	strcpy(ProcTable[ProcSpace].name, name);
	ProcTable[ProcSpace].startFunc = startFunc;
	if (arg == NULL)
		ProcTable[ProcSpace].startArg[0] = '\0';
	else if (strlen(arg) >= (MAXARG - 1)) {
		USLOSS_Console("fork1(): argument too long.  Halting...\n");
		USLOSS_Halt(1);
	} else
		strcpy(ProcTable[ProcSpace].startArg, arg);

	// Initialize context for this process, but use launch function pointer for
	// the initial value of the process's program counter (PC)
	USLOSS_Console("ProcSpace = %d, stack = %p, stackSize = %d\n", ProcSpace,
			&(ProcTable[ProcSpace].state), ProcTable[ProcSpace].stack,
			ProcTable[ProcSpace].stackSize);
	USLOSS_ContextInit(&(ProcTable[ProcSpace].state),
			ProcTable[ProcSpace].stack, ProcTable[ProcSpace].stackSize, NULL,
			launch);

	// for future phase(s)
	p1_fork(ProcTable[ProcSpace].pid);

	readyListEntry(&ProcTable[ProcSpace]);

	// More stuff to do here...
	if (ProcSpace % 50 != 1) {
		dispatcher();
	}

	return entry->pid;  // -1 is not correct! Here to prevent warning.
} /* fork1 */

/* ------------------------------------------------------------------------
 Name - launch
 Purpose - Dummy function to enable interrupts and launch a given process
 upon startup.
 Parameters - none
 Returns - nothing
 Side Effects - enable interrupts
 ------------------------------------------------------------------------ */
void launch() {
	if (currentMode() != 1) {
		if (other_debug_flag && debugflag)
			printf("You are not in kernal mode");
		USLOSS_Halt(1);
	} else {
		EnableInterrupts();
	}

	int result;

	if (other_debug_flag && debugflag)
		USLOSS_Console("launch(): started\n");

	// Enable interrupts

	// Call the function passed to fork1, and capture its return value
	result = Current->startFunc(Current->startArg);

	if (other_debug_flag && debugflag)
		USLOSS_Console("Process %d returned to launch\n", Current->pid);

	quit(result);

} /* launch */

/* ------------------------------------------------------------------------
 Name - join
 Purpose - Wait for a child process (if one has been forked) to quit.  If
 one has already quit, don't wait.
 Parameters - a pointer to an int where the termination code of the
 quitting process is to be stored.
 Returns - the process id of the quitting child joined on.
 -1 if the process was zapped in the join
 -2 if the process has no children
 Side Effects - If no child process has quit before join is called, the
 parent is removed from the ready list and blocked.
 ------------------------------------------------------------------------ */
int join(int *status) {
	DisableInterrupts();
	// -1 if the process was zapped in the join
	if (isZapped())
		return -1;
	//-2 if the process has no children
	if (Current->childProcPtr == NULL && Current->quitChild == NULL)
		return -2;

	//Child(ren) quit() before the join() occurred.
	if (Current->quitChild != NULL) {
		procPtr temp = Current->quitChild;
		Current->quitChild = temp->quitChild;

		*status = temp->quitVal;
		int Pid = temp->pid;
		freePtr(temp);
		return Pid;
	}

	//No(unjoined) child has quit() ...  must wait.
	Current->status = JOIN;  //Become blocked
	dispatcher();  //Call the dispatcher
	DisableInterrupts();
	//Then do the process again
	if (isZapped()){
		return -1;
	}
	else if (Current->childProcPtr == NULL && Current->quitChild == NULL){
		return -2;
	}
	else{
	procPtr temp = Current->quitChild;
	Current->quitChild = temp->quitChild;

	*status = temp->quitVal;
	int Pid = temp->pid;
	freePtr(temp);

	return Pid;
 }
} /* join */

/* ------------------------------------------------------------------------
 Name - quit
 Purpose - Stops the child process and notifies the parent of the death by
 putting child quit info on the parents child completion code
 list.
 Parameters - the code to return to the grieving parent
 Returns - nothing
 Side Effects - changes the parent of pid child completion status list.
 ------------------------------------------------------------------------ */
void quit(int status) { /*
 if(currentMode() != 1){
 if (other_debug_flag && debugflag)
 printf("You are not in kernal mode");
 USLOSS_Halt(1);
 } else {
 EnableInterrupts();
 }
 p1_quit(Current->pid);
 */

	procPtr temp;
	int deadChildren = 0;
	int hasParent = 1;

	if (other_debug_flag && debugflag){
		USLOSS_Console("Error:In quit(): process %d called quit()\n",Current->pid);
	}
	if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
		USLOSS_Console("Error:quit() is called when not in the kernel mode.");
		USLOSS_Halt(1);
	}  //if it is not in the kernel mode

	DisableInterrupts();
	if (Current->childProcPtr != NULL) {
		USLOSS_Console("Error: if a process with active children calls quit()");
		USLOSS_Halt(1);
	}
	/*Set the current's status*/
	Current->quitVal = status;
	Current->status = 1;

	p1_quit(Current->pid);
	/*if there is any dead children*/
	if (Current->quitChild != NULL)
		deadChildren = 1;
	/*if current has parrent or not*/

	if (Current->parentpid == 0)
		hasParent = 0;

	if (deadChildren) {
		while (Current->quitChild != NULL) {
			temp = Current->quitChild;
			freePtr(temp);
			Current->quitChild = temp->quitChild;
			temp->quitChild = NULL;
		}

		if (hasParent)
		temp = &ProcTable[Current->parentpid % 50]; //The pointer point to the parent.
		procPtr next;
		for (next = temp; next->quitChild != NULL; next = next->quitChild)
			;
		next->quitChild = Current; //moce current to the end of the parent's list.

		if (temp->childProcPtr->pid == Current->pid) {
			//swap
			procPtr t = Current->nextSiblingPtr;
			temp->childProcPtr = t;
		} else {
			procPtr before;
			for (before = temp->childProcPtr; before->nextSiblingPtr != NULL;
					before = before->nextSiblingPtr)
				if (before->nextSiblingPtr->pid == Current->pid) {
					before->nextSiblingPtr = Current->nextSiblingPtr;
					Current->nextSiblingPtr = NULL;
					break;
				}
		}
		temp->children--;
	}

	dispatcher();
} /* quit */

/* ------------------------------------------------------------------------
 Name - dispatcher
 Purpose - dispatches ready processes.  The process with the highest
 priority (the first on the ready list) is scheduled to
 run.  The old process is swapped out and the new process
 swapped in.
 Parameters - none
 Returns - nothing
 Side Effects - the context of the machine is changed
 ----------------------------------------------------------------------- */
void dispatcher(void) {
	if (currentMode() != 1) {
		if (other_debug_flag && debugflag)
			printf("You are not in kernal mode");
		USLOSS_Halt(1);
	} else {
		EnableInterrupts();
	}
	procPtr nextProcess = ReadyList->nextInReadyList;  //the readylist'
    if(Current == NULL){
        p1_switch(-1, nextProcess->pid);
    } else {
        p1_switch(Current->pid, nextProcess->pid);
    }
} /* dispatcher */

/* ------------------------------------------------------------------------
 Name - sentinel
 Purpose - The purpose of the sentinel routine is two-fold.  One
 responsibility is to keep the system going when all other
 processes are blocked.  The other is to detect and report
 simple deadlock states.
 Parameters - none
 Returns - nothing
 Side Effects -  if system is in deadlock, print appropriate error
 and halt.
 ----------------------------------------------------------------------- */
int sentinel(char *dummy) {
	if (currentMode() != 1) {
		if (other_debug_flag && debugflag) {
			printf("You are not in kernal mode");
		}
		USLOSS_Halt(1);
	} else {
		EnableInterrupts();
	}

	if (other_debug_flag && debugflag)
		USLOSS_Console("sentinel(): called\n");
	while (1) {
		checkDeadlock();
		USLOSS_WaitInt();
	}
} /* sentinel */

/* check to determine if deadlock has occurred... */
static void checkDeadlock() {
	if (currentMode() != 1) {
		if (other_debug_flag && debugflag)
			printf("You are not in kernal mode");
		USLOSS_Halt(1);
	} else {
		EnableInterrupts();
	}
  int i=0;
	int running=0;int ready=0;

	for(;i<MAXPROC;i++){
		if(ProcTable[i].status==0){
			running++;
			ready++;
		}
		else if(ProcTable[i].status==3||ProcTable[i].status == 2){
			running++;
		}
		else{

		}
	}
/*1.this is normal termination of USLOSS and ends with USLOSS_Halt(0).*/
/*2. If checkDeadlock determines that there are process(es) other than the sentinel process
 this is abnormal termination of USLOSS and ends with USLOSS_Halt(1).*/
/*phase1-v1.0*/
   if(ready==1){
		 if(running==1){
		 		USLOSS_Halt(0);//normal
			}
		 else{
				USLOSS_Console("In checkDeadlock(): abnormal termination of USLOSS and ends with USLOSS_Halt(1).");
        USLOSS_Halt(1);
			}
	 }

	 else{

	 }

} /* checkDeadlock */

/*
 *In "phase1-notes-one" says:
 *Initialize the appropriate slot to point to your clock handler function.
 *      USLOSS_IntVec[USLOSS_CLOCK_DEV] = clockHandle;
 *Do this at function Startup() maybe?
 */
static void clockHandler(int dev, void *arg) {
    if (DEBUG && debugflag)
        USLOSS_Console("clockHandler!\n");
    int timeDiff = USLOSS_DeviceInput(USLOSS_CLOCK_DEV, 0, &(Current->status)) - Current->time;//Not sure this is correct.
		if(timeDiff >= 80000 )
				dispatcher();
		// If the current process has exceeded its time slice then call the dispatcher().
		//timeSlice is 80ms = 80000us.
}/*clockHandle*/
