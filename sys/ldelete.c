#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <lock.h>
#include <stdio.h>

int ldelete(int lock_descriptor)
{
    STATWORD ps;
    struct lentry *lock_ptr;
    int pid;
    int i;

    disable(ps);

    /*Step: 01 It is very important to check if the
    lock descriptor is valid or not*/
    if (lock_descriptor < 0 || lock_descriptor >= NLOCKS)
    {
        restore(ps);
        return SYSERR;
    }

    lock_ptr = &lock_tab[lock_descriptor];

    /*STEP: 02 Checking if the lock has already been deleted or not
    created at all*/
    if (lock_ptr->lstate == LFREE)
    {
        restore(ps);
        return SYSERR;
    }
    /*STEP: 03*/
    // Now I have to set the flag to LDELETED for wait_return
    // To signal the waiting processes

    lock_ptr->wait_return = LDELETED;
    lock_ptr->lstate = LFREE;

    /*STEP: 04 Now I am going to clear out the
    waiting queue of the lock for all the processes that were waiting earlier*/
    for (i = 0; i < NPROC; i++)
    {
        if (lock_ptr->lproc_array[i] != 0) // If the lock is held either READ or WRITE
        {
            proctab[i].lock_held[lock_descriptor] = 0; // Clearing the process
            lock_ptr->lproc_array[i] = 0;              // Clearing the lock
        }
    }

    /*Step: 05 Also have to reset both the counters to 0*/

    lock_ptr->readers_count = 0;
    lock_ptr->writers_count = 0;

    /*Step: 06 is important where I have to wake the waiting processes*/

    while (nonempty(lock_ptr->lqhead))
    {
        pid = getfirst(lock_ptr->lqhead);
        struct pentry *proc_ptr = &proctab[pid];
        proc_ptr->pstate = PRREADY;
        insert(pid, rdyhead, proc_ptr->pprio);
        proc_ptr->lockid = -1;
    }

    /*Step: 07 has to reset the lock fields*/
    lock_ptr->lprio = 0;
    lock_ptr->ltype = NONE;

    resched();

    restore(ps);
    return OK;
}