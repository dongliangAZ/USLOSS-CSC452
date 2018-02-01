/* Patrick's DEBUG printing constant... test */
#define DEBUG 0

typedef struct procStruct procStruct;

typedef struct procStruct * procPtr;

struct procStruct {
   procPtr         nextProcPtr;
   procPtr         childProcPtr;
   procPtr         nextSiblingPtr;
   procPtr         nextInReadyList;
   char            name[MAXNAME];     /* process's name */
   char            startArg[MAXARG];  /* args passed to process */
   USLOSS_Context  state;             /* current context for process */
   short           pid;               /* process id */
   int             priority;
   int (* startFunc) (char *);   /* function where process begins -- launch */
   char           *stack;
   unsigned int    stackSize;
   int             status;        /* QUIT = -2, BLOCKED = -1,  UNUSED = 0, READY = 1, JOIN = 2 etc. */
   /* added */
   procPtr         quitChild;
   int             quitVal;
   short           parentpid;
   int             cpuTime;
   int             children;
   int             zapped;
   procPtr         zapList[MAXPROC];
   /* other fields as needed... */
};

struct psrBits {
    unsigned int curMode:1;
    unsigned int curIntEnable:1;
    unsigned int prevMode:1;
    unsigned int prevIntEnable:1;
    unsigned int unused:28;
};

union psrValues {
   struct psrBits bits;
   unsigned int integerPart;
};

/* Some useful constants.  Add more as needed... */
#define NO_CURRENT_PROCESS NULL
#define MINPRIORITY 5
#define MAXPRIORITY 1
#define SENTINELPID 1
#define SENTINELPRIORITY (MINPRIORITY + 1)
#define UNUSED -1
//
#define QUIT -2
#define JOIN 2
#define BLOCKED -1
#define READY 0
