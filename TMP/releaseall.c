#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <lock.h>
#include <stdio.h>

extern int ctr1000;

extern void adjust_lock_holders_priority(int lock_desc);

int releaseall(int numlocks, int ldes1, ...)
{
    STATWORD ps;
    struct pentry *pptr;
    int pid;
    int *argptr;
    int i;
    int error = OK;

    disable(ps);
    pid = getpid();
    pptr = &proctab[pid];
    argptr = (int *)(&ldes1);

    for (i = 0; i < numlocks; i++)
    {
        int ldes = *argptr++; // Checking for each descriptor

        /*Basic check for the descriptor*/
        if (ldes < 0 || ldes >= NLOCKS || lock_tab[ldes].lstate == LFREE)
        {
            error = SYSERR;
            continue;
        }

        if (pptr->lock_held[ldes] != 1) // If the lock is not held by the process I am throwing error
        {
            error = SYSERR;
            continue;
        }

        struct lentry *lptr = &lock_tab[ldes]; // Going through lock_tab

        int curr_type = lptr->lproc_array[pid]; // Using this to get the type of LOCK for the process

        if (curr_type == READ)
        {
            lptr->readers_count--; // Decrementing reader
        }
        else if (curr_type == WRITE)
        {
            lptr->writers_count--; // Decrementing Writer
        }

        lptr->lproc_array[pid] = NONE; // Setting the ltype to NONE
        pptr->lock_held[ldes] = 0;     // setting 0 in the proctab

        int real_writers = 0;
        int j;
        for (j = 0; j < NPROC; j++)
        {
            if (lptr->lproc_array[j] == WRITE)
            {
                real_writers++;
            }
        }

        if (lptr->writers_count != real_writers) // Checking if the writers count is not same as real_writers to assgin the writer count
        {
            lptr->writers_count = real_writers;
        }

        if (lptr->readers_count == 0 && lptr->writers_count == 0)
        {
            if (nonempty(lptr->lqhead))
            {
                int highest_prio = -1;
                int qp = q[lptr->lqhead].qnext;
                while (qp < NPROC)
                {
                    if (q[qp].qkey > highest_prio)
                        highest_prio = q[qp].qkey;
                    qp = q[qp].qnext;
                }

                int highest_writer_prio = -1;
                qp = q[lptr->lqhead].qnext;
                while (qp < NPROC)
                {
                    if (proctab[qp].plocktype == WRITE && q[qp].qkey > highest_writer_prio)
                        highest_writer_prio = q[qp].qkey;
                    qp = q[qp].qnext;
                }

                int next_pid = -1;
                unsigned long longest_wait = 0;

                qp = q[lptr->lqhead].qnext;
                while (qp < NPROC)
                {
                    if (q[qp].qkey == highest_prio)
                    {
                        unsigned long wait_time = ctr1000 - proctab[qp].pwaittime;
                        if (next_pid == -1 || wait_time > longest_wait)
                        {
                            next_pid = qp;
                            longest_wait = wait_time;
                        }
                    }
                    qp = q[qp].qnext;
                }

                if (next_pid != -1 && proctab[next_pid].plocktype == READ)
                {
                    qp = q[lptr->lqhead].qnext;
                    while (qp < NPROC)
                    {
                        if (q[qp].qkey == highest_prio && proctab[qp].plocktype == WRITE)
                        {
                            unsigned long writer_wait = ctr1000 - proctab[qp].pwaittime;
                            if (longest_wait - writer_wait <= 500)
                            {
                                next_pid = qp;
                                break;
                            }
                        }
                        qp = q[qp].qnext;
                    }
                }

                if (next_pid != -1)
                {
                    struct pentry *nextproc = &proctab[next_pid];
                    int next_type = nextproc->plocktype;

                    dequeue(next_pid);
                    nextproc->pstate = PRREADY;
                    nextproc->lockid = -1;
                    insert(next_pid, rdyhead, nextproc->pprio);

                    if (next_type == READ)
                    {
                        lptr->readers_count++;
                        lptr->lproc_array[next_pid] = READ;
                        nextproc->lock_held[ldes] = 1;

                        int readers[NPROC];
                        int reader_count = 0;

                        qp = q[lptr->lqhead].qnext;
                        while (qp < NPROC)
                        {
                            if (proctab[qp].plocktype == READ && q[qp].qkey > highest_writer_prio)
                            {
                                readers[reader_count++] = qp;
                            }
                            qp = q[qp].qnext;
                        }
                        int j;
                        for (j = 0; j < reader_count; j++)
                        {
                            int rpid = readers[j];
                            struct pentry *rproc = &proctab[rpid];

                            dequeue(rpid);
                            rproc->pstate = PRREADY;
                            rproc->lockid = -1;
                            insert(rpid, rdyhead, rproc->pprio);

                            lptr->readers_count++;
                            lptr->lproc_array[rpid] = READ;
                            rproc->lock_held[ldes] = 1;
                        }
                    }
                    else
                    { // WRITE
                        lptr->writers_count = 1;
                        lptr->lproc_array[next_pid] = WRITE;
                        nextproc->lock_held[ldes] = 1;
                    }
                }
            }
        }
    }

    // Updating the  releasing process's priority
    int max_wait_prio = -1;
    for (i = 0; i < NLOCKS; i++)
    {
        if (pptr->lock_held[i] == 1 && lock_tab[i].lstate != LFREE)
        {
            struct lentry *lptr = &lock_tab[i];
            int qp = q[lptr->lqhead].qnext;
            while (qp < NPROC)
            {
                int wprio = proctab[qp].pprio;
                if (wprio > max_wait_prio)
                {
                    max_wait_prio = wprio;
                }
                qp = q[qp].qnext;
            }
        }
    }
    if (max_wait_prio > pptr->pprio)
    {
        pptr->pprio = max_wait_prio;
    }
    else if (max_wait_prio == -1)
    {
        pptr->pprio = pptr->base_prio; // Resetting to original if no waiters
    }

    // Update priority for each released lock
    for (i = 0; i < numlocks; i++)
    {
        int ldes = *(argptr - numlocks + i);
        adjust_lock_holders_priority(ldes);
    }

    resched();
    restore(ps);
    return error;
}
