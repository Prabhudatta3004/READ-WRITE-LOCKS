/* chprio.c - chprio */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <stdio.h>
#include <lock.h>
/*------------------------------------------------------------------------
 * chprio  --  change the scheduling priority of a process
 *------------------------------------------------------------------------
 */
SYSCALL chprio(int pid, int newprio)
{
	STATWORD ps;
	struct pentry *pptr;
	int i;

	disable(ps);
	if (isbadpid(pid) || newprio <= 0 ||
		(pptr = &proctab[pid])->pstate == PRFREE)
	{
		restore(ps);
		return (SYSERR);
	}
	pptr->base_prio = newprio; // I have used base priority to store the original priority
	pptr->pprio = newprio;

	// Updating the priorities the process is waiting or holds

	for (i = 0; i < NLOCKS; i++)
	{
		struct lentry *lock_ptr = &lock_tab[i]; // getting access to the lock_tab
		if (lock_ptr->lstate == LUSED)			// This checks if the lock is in USE
		{
			// If the current PID holds the lock that means it cannot be NONE(either READ or WRITE)
			if (lock_ptr->lproc_array[pid] != NONE)
			{
				adjust_lock_holders_priority(i); // Calling my inheritance logic
			}
			// Assuming that the process is waiting for this lock
			if (pptr->pstate == PRWAIT && pptr->lockid == i)
			{
				adjust_lock_holders_priority(i); // I am updating the priority inheritance
			}
			// Assuming that the process is currently running
			if (pptr->pstate == PRCURR)
			{
				adjust_lock_holders_priority(i); // I am updating the priority inheritance
			}
		}
	}
	// If the process is in ready queue I need to reinsert
	if (pptr->pstate == PRREADY)
	{
		dequeue(pid);
		insert(pid, rdyhead, pptr->pprio);
	}

	restore(ps);
	return (newprio);
}
