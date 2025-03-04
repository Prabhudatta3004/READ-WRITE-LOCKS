#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <lock.h>
#include <stdio.h>

extern int nextlock;
extern int ctr1000;

int lcreate(void)
{
    STATWORD ps;
    int i;
    int lock;
    struct lentry *ptr;

    disable(ps);

    for (i = 0; i < NLOCKS; i++)
    {
        lock = (nextlock + i) % NLOCKS;
        ptr = &lock_tab[lock];
        if (ptr->lstate == LFREE)
        {
            ptr->lstate = LUSED;
            ptr->ltype = NONE;
            ptr->lprio = 0;
            ptr->readers_count = 0;
            ptr->writers_count = 0;
            ptr->wait_return = OK;
            ptr->ltime = ctr1000;
            ptr->lid = nextlock++;
            restore(ps);
            return lock;
        }
    }
    restore(ps);
    return SYSERR;
}