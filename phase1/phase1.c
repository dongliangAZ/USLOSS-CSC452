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
int sentinel (char *);
void dispatcher(void);
void launch();
static void checkDeadlock();


/* -------------------------- Globals ------------------------------------- */

// Patrick's debugging global variable...
int debugflag = 1;
int mydebugflag = 1;
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

/*_____________OUR_METHODS________________*/
/* ------------------------------------------------------------------------
   Name - currentMode
   Purpose - returns which mode you are in.
   Parameters -
   Returns - 1 is kernal, 0 is user, and -1 is error
   Side Effects -
   ----------------------------------------------------------------------- */

int currentMode()
{
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


void DisableInterrupts(){
     if(DEBUG && debugflag){
         USLOSS_Console("Disable the interrupts\n");
     }
     union psrValues psr = {.integerPart =  USLOSS_PsrGet()};
     if(psr.bits.curMode != 1){
         USLOSS_Halt(1);
     }
     psr.bits.curIntEnable = 0;
     if(USLOSS_PsrSet(psr.integerPart) == USLOSS_ERR_INVALID_PSR){
         USLOSS_Console("Invalid psr");
     }
}

void EnableInterrupts(){
  if(DEBUG && debugflag){
          USLOSS_Console("Enable the interrupts\n");
      }
      union psrValues psr = {.integerPart =  USLOSS_PsrGet()};
      if(psr.bits.curMode != 1){
          USLOSS_Halt(1);
      }
      psr.bits.curIntEnable = 1;
      if(USLOSS_PsrSet(psr.integerPart) == USLOSS_ERR_INVALID_PSR){
          USLOSS_Console("Invalid psr");
      }
      if ((DEBUG && debugflag))
        USLOSS_Console("psr = %d\n",psr.integerPart);
}

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

void startup(int argc, char *argv[])
{
    if(currentMode() != 1){
        if (DEBUG && debugflag)
            printf("You are not in kernal mode");
        USLOSS_Halt(1);
    } else {
        EnableInterrupts();
    }
    int result; /* value returned by call to fork1() */

    /* initialize the process table */
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing process table, ProcTable[]\n");
    for(int i=0;i<MAXPROC;i++){
        ProcTable[i].stack = NULL;
        ProcTable[i].nextProcPtr = NULL;
        ProcTable[i].childProcPtr = NULL;
        ProcTable[i].name[0]= '\0';
        ProcTable[i].startArg[0] = '\0';
        //ProcTable[i].state = NULL;
        ProcTable[i].pid = -1;
        ProcTable[i].priority = -1;
        ProcTable[i].stackSize = 0;
        ProcTable[i].status = 0; // 0 is UNUSED
    }
      /////////////////////

    // Initialize the Ready list, etc.
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing the Ready list\n");
    ReadyList = NULL;

    // Initialize the clock interrupt handler
    // USLOSS_IntVec[USLOSS_ILLEGAL_INT] = interruptCase;
    // USLOSS_IntVec[USLOSS_ALARM_INT] = interruptCase;
    EnableInterrupts();



    // startup a sentinel process
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): calling fork1() for sentinel\n");
    result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK,
                    SENTINELPRIORITY);
    if (result < 0) {
        if (DEBUG && debugflag) {
            USLOSS_Console("startup(): fork1 of sentinel returned error, ");
            USLOSS_Console("halting...\n");
        }
        USLOSS_Halt(1);
    }

    // start the test process
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): calling fork1() for start1\n");
    result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1); //office hours
    if (result < 0) {
        USLOSS_Console("startup(): fork1 for start1 returned an error, ");
        USLOSS_Console("halting...\n");
        USLOSS_Halt(1);
    }

    USLOSS_Console("startup(): Should not see this message! ");
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
void finish(int argc, char *argv[])
{
    if(currentMode() != 1){
        if (DEBUG && debugflag)
            printf("You are not in kernal mode");
        USLOSS_Halt(1);
    } else {
        EnableInterrupts();
    }
    
    if (DEBUG && debugflag)
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
int fork1(char *name, int (*startFunc)(char *), char *arg,
          int stacksize, int priority)
{

    if (DEBUG && debugflag)
        USLOSS_Console("fork1(): creating process %s\n", name);

    // test if in kernel mode; halt if in user mode
    if(currentMode() != 1){
        printf("You are not in kernal mode");
        USLOSS_Halt(1);
    } else {
        EnableInterrupts();
    }

    // Return if stack size is too small
    if(stacksize < USLOSS_MIN_STACK){
        return -2;
    }

    // Is there room in the process table? What is the next PID?
    int ProcSpace = -1;
    for(int i = 0; i < MAXPROC; i++){
        if(ProcTable[(nextPid + i)% MAXPROC].status == 0){ //checking if it is unused.
            ProcSpace = (nextPid + i);
            break;
        }
        if(i == MAXPROC-1){
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
    entry->status = 1;
    
    
    if ( strlen(name) >= (MAXNAME - 1) ) {
        USLOSS_Console("fork1(): Process name is too long.  Halting...\n");
        USLOSS_Halt(1);
    }
    strcpy(ProcTable[ProcSpace].name, name);
    ProcTable[ProcSpace].startFunc = startFunc;
    if ( arg == NULL )
        ProcTable[ProcSpace].startArg[0] = '\0';
    else if ( strlen(arg) >= (MAXARG - 1) ) {
        USLOSS_Console("fork1(): argument too long.  Halting...\n");
        USLOSS_Halt(1);
    }
    else
        strcpy(ProcTable[ProcSpace].startArg, arg);

    // Initialize context for this process, but use launch function pointer for
    // the initial value of the process's program counter (PC)
USLOSS_Console("ProcSpace = %d, stack = %p, stackSize = %d\n", ProcSpace, &(ProcTable[ProcSpace].state), ProcTable[ProcSpace].stack, ProcTable[ProcSpace].stackSize);
    USLOSS_ContextInit(&(ProcTable[ProcSpace].state),
                       ProcTable[ProcSpace].stack,
                       ProcTable[ProcSpace].stackSize,
                       NULL,
                       launch);

    // for future phase(s)
    p1_fork(ProcTable[ProcSpace].pid);

    // More stuff to do here...
    if(ProcSpace%50 != 1){
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
void launch()
{
    if(currentMode() != 1){
        if (DEBUG && debugflag)
            printf("You are not in kernal mode");
        USLOSS_Halt(1);
    } else {
        EnableInterrupts();
    }
    
    int result;

    if (DEBUG && debugflag)
        USLOSS_Console("launch(): started\n");

    // Enable interrupts

    // Call the function passed to fork1, and capture its return value
    result = Current->startFunc(Current->startArg);

    if (DEBUG && debugflag)
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
int join(int *status)
{
    if(currentMode() != 1){
        if (DEBUG && debugflag)
            printf("You are not in kernal mode");
        USLOSS_Halt(1);
    } else {
        EnableInterrupts();
    }
    return -1;  // -1 is not correct! Here to prevent warning.
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
void quit(int status)
{
    if(currentMode() != 1){
        if (DEBUG && debugflag)
            printf("You are not in kernal mode");
        USLOSS_Halt(1);
    } else {
        EnableInterrupts();
    }
    p1_quit(Current->pid);
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
void dispatcher(void)
{
    if(currentMode() != 1){
        if (DEBUG && debugflag)
            printf("You are not in kernal mode");
        USLOSS_Halt(1);
    } else {
        EnableInterrupts();
    }
    procPtr nextProcess = NULL;

    p1_switch(Current->pid, nextProcess->pid);
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
int sentinel (char *dummy)
{
    if(currentMode() != 1){
        if (DEBUG && debugflag){
            printf("You are not in kernal mode");
        }
        USLOSS_Halt(1);
    } else {
        EnableInterrupts();
    }
    
    if (DEBUG && debugflag)
        USLOSS_Console("sentinel(): called\n");
    while (1)
    {
        checkDeadlock();
        USLOSS_WaitInt();
    }
} /* sentinel */


/* check to determine if deadlock has occurred... */
static void checkDeadlock()
{
    if(currentMode() != 1){
        if (DEBUG && debugflag)
            printf("You are not in kernal mode");
        USLOSS_Halt(1);
    } else {
        EnableInterrupts();
    }
} /* checkDeadlock */





