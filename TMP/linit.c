#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <lock.h>

struct lentry lock_tab[NLOCKS]; /*Using the lock_tab that I created before*/

// Nextlock helps in providing a unique Lid for each instance of the Locks created and used
int nextlock = 0;
/*My nextlock serves two purposes helps me to
assign a new lock instance to the processes that ask
for a lock as well as helps me to generate a unique lockid
for every new instance of the lock*/

int linit(void)
{
    struct lentry *lock_ptr;
    int i, j;
    nextlock = 0;
    /*This is my core initiation process for all the locks
    I am setting default values for all the fields*/
    for (i = 0; i < NLOCKS; i++)
    {
        lock_ptr = &lock_tab[i];                 // Accessing each lock through the pointer
        lock_ptr->lstate = LFREE;                // Initiating the state to be LFREE as default
        lock_ptr->ltype = NONE;                  // Initiating the type to NONE as default
        lock_ptr->lprio = 0;                     // Initiating the max prio of the waiting queue to be 0
        lock_ptr->lqhead = newqueue();           // Creating a new priority queue
        lock_ptr->lqtail = lock_ptr->lqhead + 1; // Setting the tail as head+1
        lock_ptr->readers_count = 0;             // Initializing the readers_count as 0
        lock_ptr->writers_count = 0;             // Initializing the writers_count as 0
        lock_ptr->wait_return = OK;              // Initializing the wait_return as OK
        lock_ptr->ltime = 0;                     // Initializing the ltime to be 0
        lock_ptr->lid = -1;                      // Intializing the unique lid from -1
        // lock_ptr->lmax_prio = 9999;              // Initializing the max scheduling prio to high no.
        for (j = 0; j < NPROC; j++)
        {
            lock_ptr->lproc_array[j] = NONE; // Initializing the entire array with NONE for all the processess
        }
    }
    return OK;
}