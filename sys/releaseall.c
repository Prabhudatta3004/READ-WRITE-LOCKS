#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <lock.h>
#include <stdio.h>

extern int ctr1000;

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
        int ldes = *argptr++;

        if (ldes < 0 || ldes >= NLOCKS || lock_tab[ldes].lstate == LFREE)
        {
            error = SYSERR;
            continue;
        }

        if (pptr->lock_held[ldes] != 1)
        {
            error = SYSERR;
            continue;
        }

        struct lentry *lptr = &lock_tab[ldes];

        // CRITICAL FIX: Check the current lock type for this process
        int curr_type = lptr->lproc_array[pid];

        if (curr_type == READ)
        {
            lptr->readers_count--;
        }
        else if (curr_type == WRITE)
        {
            lptr->writers_count--;
        }

        // Clear the lock status for this process
        lptr->lproc_array[pid] = NONE;
        pptr->lock_held[ldes] = 0;

        // CRITICAL FIX: Count actual writers to fix phantom writer issue
        int real_writers = 0;
        int j;
        for (j = 0; j < NPROC; j++)
        {
            if (lptr->lproc_array[j] == WRITE)
            {
                real_writers++;
            }
        }

        // If writers_count doesn't match real count, fix it
        if (lptr->writers_count != real_writers)
        {
            lptr->writers_count = real_writers;
        }

        // Check if lock is now completely free
        if (lptr->readers_count == 0 && lptr->writers_count == 0)
        {
            if (nonempty(lptr->lqhead))
            {
                // Find highest priority
                int highest_prio = -1;
                int qp = q[lptr->lqhead].qnext;
                while (qp < NPROC)
                {
                    if (q[qp].qkey > highest_prio)
                        highest_prio = q[qp].qkey;
                    qp = q[qp].qnext;
                }

                // Find highest priority writer
                int highest_writer_prio = -1;
                qp = q[lptr->lqhead].qnext;
                while (qp < NPROC)
                {
                    if (proctab[qp].plocktype == WRITE && q[qp].qkey > highest_writer_prio)
                        highest_writer_prio = q[qp].qkey;
                    qp = q[qp].qnext;
                }

                // Find process with longest wait at highest priority
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

                // Check for writer preference with grace period
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

                        // Now admit all other eligible readers
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
                    else // WRITE
                    {
                        lptr->writers_count = 1; // CRITICAL FIX: Set to 1, not increment
                        lptr->lproc_array[next_pid] = WRITE;
                        nextproc->lock_held[ldes] = 1;
                    }
                }
            }
        }
    }

    resched();
    restore(ps);
    return error;
}