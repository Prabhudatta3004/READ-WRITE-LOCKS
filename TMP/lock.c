#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <lock.h>
#include <stdio.h>

extern unsigned long ctr1000;

void adjust_lock_holders_priority(int lock_desc)
{
    /*This is my major function that handles the priority
    inheritance logic*/
    struct lentry *lptr = &lock_tab[lock_desc];
    int i;

    // kprintf("DEBUG: adjust_lock_holders_priority for lock %d\n", lock_desc);

    // Find the maximum priority among waiters
    int max_wait_prio = -1; // Assuming that there is no waiters
    int qp = q[lptr->lqhead].qnext;
    // kprintf("DEBUG: Wait queue for lock %d: ", lock_desc);
    while (qp < NPROC)
    {
        // kprintf("%d (prio %d) ", qp, proctab[qp].pprio);

        int wprio = proctab[qp].pprio; // storing the pprio of the process
        if (wprio > max_wait_prio)     // Swapping with max_wait_pro to store the maximum prio
        {
            max_wait_prio = wprio;
        }
        qp = q[qp].qnext;
    }
    // kprintf("\nDEBUG: Computed max_wait_prio = %d\n", max_wait_prio);

    // Adjust priorities of all processes holding the lock
    for (i = 0; i < NPROC; i++)
    {
        if (lptr->lproc_array[i] != NONE) // If a process holds the lock it will not be NONE
        {                                 // Process holds the lock
            struct pentry *pptr = &proctab[i];
            // kprintf("DEBUG: Holder PID %d, old pprio = %d, base_prio = %d\n", i, pptr->pprio, pptr->base_prio);
            if (max_wait_prio != -1) // This means that there are waiter processes
            {                        // Set pprio to max_wait_prio if there are waiters
                pptr->pprio = max_wait_prio;
            }
            else
            { // Resetting to base_prio if no waiters
                pptr->pprio = pptr->base_prio;
            }
            // kprintf("DEBUG: Holder PID %d, new pprio = %d\n", i, pptr->pprio);
            //  Transitivity
            // For transitivity I am checking for the process state being in PRWAIT and wating for some other lock
            if (pptr->pstate == PRWAIT && pptr->lockid != -1)
            {
                // Recursively calling the process
                adjust_lock_holders_priority(pptr->lockid);
            }
        }
    }
}

int lock(int ldes1, int type, int priority)
{
    STATWORD ps;
    struct lentry *lock_ptr;
    struct pentry *proc_ptr;
    int pid;

    disable(ps);
    lock_ptr = &lock_tab[ldes1];
    pid = getpid();
    proc_ptr = &proctab[pid];

    /*Step: 01 This is my basic descriptor check
    If the state of the lock is also in LFREE that means that
    the lock was never created or Deleted*/
    if (ldes1 < 0 || ldes1 >= NLOCKS || lock_ptr->lstate == LFREE)
    {
        restore(ps);
        return SYSERR;
    }

    // kprintf("DEBUG: PID %d requesting lock %d, type %d, pprio %d\n", pid, ldes1, type, proc_ptr->pprio);

    // Case 1: Lock is free (Ideal Case)
    if (lock_ptr->readers_count == 0 && lock_ptr->writers_count == 0)
    {
        if (type == READ)
        {
            lock_ptr->readers_count++;
            lock_ptr->lproc_array[pid] = READ; // Storing the list value of the process with READ
        }
        else
        { // WRITE
            lock_ptr->writers_count++;
            lock_ptr->lproc_array[pid] = WRITE; // Storing the list value of the process with WRITE
        }
        lock_ptr->ltype = type;         // I am updating the lock type with the process specified type
        proc_ptr->lock_held[ldes1] = 1; // I am also updating the lock_held array in proctab with 1 stating that it holds the lock
        restore(ps);
        return OK;
    }

    // Case 2: Lock held by readers only and new request is READ (Major case)
    if (lock_ptr->writers_count == 0 && lock_ptr->readers_count > 0 && type == READ)
    {
        /*My check is for writers_count, reader_count>0 and type is read*/
        int highest_prio = -1;          // Will be using this for my waiting writer prio
        unsigned long longest_wait = 0; // for waiting writers waittime
        int next = q[lock_ptr->lqhead].qnext;
        while (next < NPROC)
        {
            if (proctab[next].plocktype == WRITE) // If the process has a type WRITE
            {
                int wprio = q[next].qkey;                                    // priority of the waiting writer
                unsigned long wait_time = ctr1000 - proctab[next].pwaittime; // calculating the wait time
                if (wprio > highest_prio)                                    // if
                {
                    highest_prio = wprio;     // swapping the highest prio
                    longest_wait = wait_time; // storing its wait time as the longest wait
                }
                else if (wprio == highest_prio && wait_time > longest_wait) // check for the wait time logic
                {
                    longest_wait = wait_time;
                }
            }
            next = q[next].qnext;
        }
        if (highest_prio == -1 || highest_prio < priority ||
            (highest_prio == priority && longest_wait <= 500)) // the reader will only be allowed if these conditions satisfies
        {
            lock_ptr->readers_count++;
            lock_ptr->lproc_array[pid] = READ;
            proc_ptr->lock_held[ldes1] = 1;
            restore(ps);
            return OK;
        }
    }
    /*This is the fall through wait logic */
    // Wait logic with priority inheritance
    proc_ptr->pstate = PRWAIT;
    proc_ptr->lockid = ldes1;
    proc_ptr->plocktype = type;
    proc_ptr->pwaittime = ctr1000;

    insert(pid, lock_ptr->lqhead, priority);
    if (priority > lock_ptr->lprio)
    {
        lock_ptr->lprio = priority;
    }

    adjust_lock_holders_priority(ldes1); // this is called to update the priorities of the wait processes

    resched();

    // Wakeup logic
    if (lock_ptr->wait_return == LDELETED) // if the process finds out that the lock was deleted
    {
        proc_ptr->lockid = -1; // no lockid (that is it is not waiting for any process)
        restore(ps);
        return LDELETED; // returning LDELETED
    }

    if (type == READ)
    {
        lock_ptr->readers_count++;
        lock_ptr->lproc_array[pid] = READ;
        proc_ptr->lock_held[ldes1] = 1;

        int max_writer_prio = -1;
        int qn = q[lock_ptr->lqhead].qnext;
        while (qn < NPROC)
        {
            if (proctab[qn].plocktype == WRITE && q[qn].qkey > max_writer_prio)
            {
                max_writer_prio = q[qn].qkey;
            }
            qn = q[qn].qnext;
        }

        int readers_to_admit[NPROC];
        int reader_count = 0;

        qn = q[lock_ptr->lqhead].qnext;
        while (qn < NPROC)
        {
            if (proctab[qn].plocktype == READ && q[qn].qkey > max_writer_prio)
            {
                readers_to_admit[reader_count++] = qn;
            }
            qn = q[qn].qnext;
        }

        int j;
        for (j = 0; j < reader_count; j++)
        {
            int rpid = readers_to_admit[j];
            struct pentry *rptr = &proctab[rpid];

            dequeue(rpid);
            rptr->pstate = PRREADY;
            rptr->lockid = -1;
            insert(rpid, rdyhead, rptr->pprio);
            lock_ptr->readers_count++;
            lock_ptr->lproc_array[rpid] = READ;
            rptr->lock_held[ldes1] = 1;
        }
    }
    else
    { // WRITE
        lock_ptr->writers_count++;
        lock_ptr->lproc_array[pid] = WRITE;
        proc_ptr->lock_held[ldes1] = 1;
    }
    lock_ptr->ltype = type;
    restore(ps);
    return OK;
}