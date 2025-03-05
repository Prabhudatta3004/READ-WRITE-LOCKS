#ifndef _PROC_H_
#define _PROC_H_

#ifndef NPROC
#define NPROC 30
#endif

#ifndef NLOCKS
#define NLOCKS 50
#endif

#ifndef _NFILE
#define _NFILE 20
#endif

#define FDFREE -1
#define PRFREE '\002'

#define PRCURR '\001'
#define PRFREE '\002'
#define PRREADY '\003'
#define PRRECV '\004'
#define PRSLEEP '\005'
#define PRSUSP '\006'
#define PRWAIT '\007'
#define PRTRECV '\010'

#define PNMLEN 16
#define NULLPROC 0
#define BADPID -1

#define isbadpid(x) (x <= 0 || x >= NPROC)

struct pentry
{
	char pstate;
	int pprio;	   // Current priority, modified directly
	int base_prio; // Original priority at creation (replaces ogprio)
	int pesp;
	STATWORD pirmask;
	int psem;
	WORD pmsg;
	char phasmsg;
	WORD pbase;
	int pstklen;
	WORD plimit;
	char pname[PNMLEN];
	int pargs;
	WORD paddr;
	WORD pnxtkin;
	Bool ptcpumode;
	short pdevs[2];
	int fildes[_NFILE];
	int ppagedev;
	int pwaitret;
	int lockid;			   // Lock being waited on
	int lock_held[NLOCKS]; // Locks held by process
	int plocktype;		   // Type of lock requested
	unsigned long pwaittime;
};

extern struct pentry proctab[];
extern int numproc;
extern int nextproc;
extern int currpid;

#endif