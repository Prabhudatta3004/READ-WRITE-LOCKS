#ifndef _LOCK_H_
#define _LOCK_H_

/*This header will serve as my
global header file for the entire
reader/writer implementation.*/

#define NLOCKS 50 // Maximum number of locks in my created system

/*My LOCK states*/
#define LFREE 0 // LFREE is a free state where the lock is not held by any process
#define LUSED 1 // LUSED indicates that the lock is held by some process or processes(READ)

/*USE of LDELETE is to inform
processes waiting for the LOCK
which has been deleted*/

#define LDELETED -1 // giving a special return value for LDELETED

/*My lock Types*/

#define NONE 0  // This is the initial type of the lock when the lock was just created
#define READ 1  // This is the type of the lock when the lock was acquired by a READER
#define WRITE 2 // This is the type of the lock when the lock was acquired by a WRITE

/*Creating the structure for my lock_table*/

struct lentry
{
    int lstate;                // Storing the state of each lock
    int ltype;                 // specifies the current type of the lock
    int lprio;                 // Stores the highest priority among the waiting processes in the queue
    int lqhead;                // This is the head of the waiting queue
    int lqtail;                // This stores the tail of the waiting queue
    int readers_count;         // Stores the total number of readers holding the lock
    int writers_count;         // I am using this as a flag for having only one writer
    int lid;                   // This is a unique id for my lock to maintain the instances of a single lock
    int lproc_array[NPROC];    // List of all processes holding the lock
    unsigned long wait_return; // stores a return value for the processes waiting for the lock(OK or LDELETED)
    unsigned long ltime;       // Time when the lock was created, I am using it for debugging
    int lmax_prio;             // Tracks the maximum scheduling priority among the waiting processes
};

extern struct lentry lock_tab[NLOCKS];

extern int linit(void);
extern int lcreate(void);
extern int ldelete(int lock_descriptor);
extern int lock(int ldes1, int type, int priority);
extern int releaseall(int numlocks, int ldes1, ...);
extern int update_prio_inheritance(int ldes1, int requester_prio);

#endif