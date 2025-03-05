#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <lock.h>
#include <stdio.h>

/*My lcreate function can be used by any process to create
a lock of NONE type with FREE state untill and unless some
process acquires it*/

extern int nextlock; // I am taking the next lock value for getting a unique lock
extern int ctr1000;  // Using ctr1000 for calculating the creation time

int lcreate(void)
{
    STATWORD ps;
    int i;
    int lock; // Used as an iterator for locks
    struct lentry *lock_ptr;

    disable(ps); // Following the same logic as screate by disabling interrupts

    for (i = 0; i < NLOCKS; i++)
    {
        lock = (nextlock + i) % NLOCKS; // Unique way of getting a lock(nextlock is helping me to get an unique lock always)
        lock_ptr = &lock_tab[lock];     // accessing the lock_tab for that lock
        if (lock_ptr->lstate == LFREE)  // If the lock is free then we can create it
        {
            lock_ptr->lstate = LUSED;    // Setting the state to LUSED
            lock_ptr->ltype = NONE;      // Setting the lock type to be NONE
            lock_ptr->lprio = 0;         // Currently no process holds it so lprio is 0
            lock_ptr->readers_count = 0; // Currently the reader_count is 0
            lock_ptr->writers_count = 0; // No writer is holding it
            lock_ptr->wait_return = OK;  // Setting the wait_return to OK (DEFAULT)
            lock_ptr->ltime = ctr1000;   // Calculating the time of creation using ctr1000
            lock_ptr->lid = nextlock++;  // Second use of my nextlock that helps me to get an unique LID
            restore(ps);
            return lock;
        }
    }
    restore(ps);
    return SYSERR;
}
