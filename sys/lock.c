#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <lock.h>
#include <stdio.h>

extern unsigned long ctr1000;

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

    if (ldes1 < 0 || ldes1 >= NLOCKS || lock_ptr->lstate == LFREE)
    {
        restore(ps);
        return SYSERR;
    }

    if (proc_ptr->lockid == ldes1 && proc_ptr->lock_lid != lock_ptr->lid)
    {
        restore(ps);
        return SYSERR;
    }

    // Case 1: Lock is free
    if (lock_ptr->readers_count == 0 && lock_ptr->writers_count == 0)
    {
        if (type == READ)
        {
            lock_ptr->readers_count++;
            lock_ptr->lproc_array[pid] = READ;
        }
        else // WRITE
        {
            lock_ptr->writers_count++;
            lock_ptr->lproc_array[pid] = WRITE;
        }
        lock_ptr->ltype = type;
        proc_ptr->lock_held[ldes1] = 1;
        restore(ps);
        return OK;
    }

    // Case 2: Lock held by readers only and new request is READ
    if (lock_ptr->writers_count == 0 && lock_ptr->readers_count > 0 && type == READ)
    {
        int highest_prio = -1;
        unsigned long longest_wait = 0;
        int next = q[lock_ptr->lqhead].qnext;
        while (next < NPROC)
        {
            if (proctab[next].plocktype == WRITE)
            {
                int wprio = q[next].qkey;
                unsigned long wait_time = ctr1000 - proctab[next].pwaittime;
                if (wprio > highest_prio)
                {
                    highest_prio = wprio;
                    longest_wait = wait_time;
                }
                else if (wprio == highest_prio && wait_time > longest_wait)
                {
                    longest_wait = wait_time;
                }
            }
            next = q[next].qnext;
        }
        if (highest_prio == -1 || highest_prio < priority ||
            (highest_prio == priority && longest_wait <= 500))
        {
            lock_ptr->readers_count++;
            lock_ptr->lproc_array[pid] = READ;
            proc_ptr->lock_held[ldes1] = 1;
            restore(ps);
            return OK;
        }
    }

    // Wait logic
    proc_ptr->pstate = PRWAIT;
    proc_ptr->lockid = ldes1;
    proc_ptr->plocktype = type;
    proc_ptr->pwaittime = ctr1000;
    proc_ptr->lock_lid = lock_ptr->lid;

    insert(pid, lock_ptr->lqhead, priority);
    if (priority > lock_ptr->lprio)
    {
        lock_ptr->lprio = priority;
    }

    resched();

    // Wakeup logic
    if (lock_ptr->wait_return == LDELETED)
    {
        proc_ptr->lockid = -1;
        restore(ps);
        return LDELETED;
    }

    if (proc_ptr->lockid == ldes1 && proc_ptr->lock_lid != lock_ptr->lid)
    {
        restore(ps);
        return SYSERR;
    }

    // Grant lock after waking
    if (type == READ)
    {
        lock_ptr->readers_count++;
        lock_ptr->lproc_array[pid] = READ;
        proc_ptr->lock_held[ldes1] = 1;

        // Find highest priority writer waiting
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

        // Collect all readers with higher priority than highest writer
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

        // Admit all collected readers
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
    else // WRITE
    {
        lock_ptr->writers_count++;
        lock_ptr->lproc_array[pid] = WRITE;
        proc_ptr->lock_held[ldes1] = 1;
    }
    lock_ptr->ltype = type;
    restore(ps);
    return OK;
}