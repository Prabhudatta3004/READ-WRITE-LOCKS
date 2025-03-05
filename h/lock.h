#ifndef _LOCK_H_
#define _LOCK_H_

#define NLOCKS 50 // Maximum number of locks

// LOCK STATES
#define LFREE 0
#define LUSED 1

// SPECIAL RETURN FOR DELETED
#define LDELETED -1

// LOCK TYPES
#define NONE 0
#define READ 1
#define WRITE 2

struct lentry
{
    int lstate;                // Storing the state of each lock
    int ltype;                 // Important to define the type of the lock-READ or write
    int lprio;                 // To store the highest priority among the waiting processes
    int lqhead;                // Storing the head of the queue of the waiting process
    int lqtail;                // Storing the tail of the queue of the waiting process
    int readers_count;         // Number of readers holding this lock
    int writers_count;         // Number of writers holding this lock
    int lid;                   // this is an id I am giving to detect duplicacy
    int lproc_array[NPROC];    // list of processes holding this lock
    unsigned long wait_return; // returning value for the waiting processes - OK or delete
    unsigned long ltime;       // Time when the lock was created for debugging
    int lmax_prio;
};

extern struct lentry lock_tab[NLOCKS];

extern int linit(void);
extern int lcreate(void);
extern int ldelete(int lock_descriptor);
extern int lock(int ldes1, int type, int priority);
extern int releaseall(int numlocks, int ldes1, ...);
extern void update_prio_inheritance(int ldes1, int requester_prio);

#endif