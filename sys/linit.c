#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <lock.h>

struct lentry lock_tab[NLOCKS]; /* NOTICE: Using lock_tab, not locktab */
// Nextlock helps in providing a unique Lid for each instance of the Locks created and used
int nextlock = 0;

int linit(void)
{
    struct lentry *ptr;
    int i, j;
    nextlock = 0;
    /*I am initializing all locks as FREE*/
    for (i = 0; i < NLOCKS; i++)
    {
        ptr = &lock_tab[i];  /* CHANGE HERE: Use lock_tab, not locktab */
        ptr->lstate = LFREE; // initializing all the locks with state as free
        ptr->ltype = NONE;   // Taking read as my default value for all locks, will change in lock.c
        ptr->lprio = 0;      // Setting all the prio to zero
        ptr->lqhead = newqueue();
        ptr->lqtail = ptr->lqhead + 1;
        ptr->readers_count = 0; // Setting the readers count to zero
        ptr->writers_count = 0; // Setting the writers count to zero
        ptr->wait_return = OK;  // giving the return value as OK
        ptr->ltime = 0;
        ptr->lid = -1;
        ptr->lmax_prio = 9999;
        for (j = 0; j < NPROC; j++)
        {
            ptr->lproc_array[j] = NONE;
        }
    }
    return OK;
}