#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <sem.h>
#include <mem.h>
#include <io.h>
#include <q.h>
#include <stdio.h>
#include <lock.h>

extern void adjust_lock_holders_priority(int lock_desc);

SYSCALL kill(int pid)
{
	STATWORD ps;
	struct pentry *pptr;
	int dev;

	disable(ps);
	if (isbadpid(pid) || (pptr = &proctab[pid])->pstate == PRFREE)
	{
		restore(ps);
		return SYSERR;
	}

	////kprintf("DEBUG: Killing PID %d, state %d, pprio %d\n", pid, pptr->pstate, pptr->pprio);

	if (pptr->pstate == PRWAIT && pptr->lockid != -1)
	{
		int ldes = pptr->lockid;
		struct lentry *lptr = &lock_tab[ldes];

		// kprintf("DEBUG: PID %d waiting on lock %d\n", pid, ldes);

		dequeue(pid);
		pptr->lockid = -1;

		// kprintf("DEBUG: After dequeue, wait queue for lock %d: ", ldes);
		int qp = q[lptr->lqhead].qnext;
		while (qp < NPROC)
		{
			// kprintf("%d (prio %d) ", qp, proctab[qp].pprio);
			qp = q[qp].qnext;
		}
		////kprintf("\n");

		adjust_lock_holders_priority(ldes);
	}

	int i;
	for (i = 0; i < NLOCKS; i++)
	{
		if (pptr->lock_held[i] == 1)
		{
			struct lentry *lptr = &lock_tab[i];
			////kprintf("DEBUG: PID %d releasing lock %d\n", pid, i);
			if (lptr->lproc_array[pid] == READ)
			{
				lptr->readers_count--;
			}
			else if (lptr->lproc_array[pid] == WRITE)
			{
				lptr->writers_count--;
			}
			lptr->lproc_array[pid] = NONE;
			pptr->lock_held[i] = 0;

			if (lptr->readers_count == 0 && lptr->writers_count == 0 && nonempty(lptr->lqhead))
			{
				int next_pid = getfirst(lptr->lqhead);
				struct pentry *nextproc = &proctab[next_pid];
				nextproc->pstate = PRREADY;
				nextproc->lockid = -1;
				insert(next_pid, rdyhead, nextproc->pprio);
				if (nextproc->plocktype == READ)
				{
					lptr->readers_count++;
					lptr->lproc_array[next_pid] = READ;
				}
				else
				{
					lptr->writers_count++;
					lptr->lproc_array[next_pid] = WRITE;
				}
				nextproc->lock_held[i] = 1;
				////kprintf("DEBUG: Woke up PID %d for lock %d\n", next_pid, i);
			}

			adjust_lock_holders_priority(i);
		}
	}

	if (--numproc == 0)
	{
		xdone();
	}

	dev = pptr->pdevs[0];
	if (!isbaddev(dev))
	{
		close(dev);
	}
	dev = pptr->pdevs[1];
	if (!isbaddev(dev))
	{
		close(dev);
	}
	dev = pptr->ppagedev;
	if (!isbaddev(dev))
	{
		close(dev);
	}

	send(pptr->pnxtkin, pid);

	freestk(pptr->pbase, pptr->pstklen);

	switch (pptr->pstate)
	{
	case PRCURR:
		pptr->pstate = PRFREE;
		resched();
		break;

	case PRWAIT:
		semaph[pptr->psem].semcnt++;
		/* Fall through */

	case PRREADY:
		dequeue(pid);
		pptr->pstate = PRFREE;
		break;

	case PRSLEEP:
	case PRTRECV:
		unsleep(pid);
		/* Fall through */

	default:
		pptr->pstate = PRFREE;
	}

	resched();
	restore(ps);
	return OK;
}