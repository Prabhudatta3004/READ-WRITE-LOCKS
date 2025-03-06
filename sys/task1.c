/* This code tests the priority inversion with semaphores vs read/write lock I implemented */
#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <lock.h>
#include <stdio.h>

/* I am defining values for various processes */
#define Low 15  /* I am going to set the low for the producer to show priority inheritance */
#define Med 25  /* I will set this for first consumer */
#define High 35 /* I will set this for the second consumer */

int mylock;                 // This is the descriptor for my read/write lock
int mysem;                  // This is to test the Semaphore in XINU
char buffer[50] = "ABCABC"; // This is the resource the shared buffer

/* Going to write the logic for semaphores in here */

// For the consumers that use semaphore
void scon(char *name, int sem)
{
    kprintf("Consumer %s: is trying for a semaphore\n", name);
    wait(sem); // Waiting for the sem
    kprintf("Consumer %s: has a semaphore\n", name);
    sleep(3); // Doing nothing, just holding the sem
    kprintf("Consumer %s: gave away the semaphore\n", name);
    signal(sem); // Signaling the sem
}

// For the producer using semaphores
void sprod(char *name, int sem)
{
    kprintf("Producer %s: wants a semaphore\n", name);
    wait(sem);
    kprintf("Producer %s: has a semaphore\n", name);
    sleep(12);
    strcpy(buffer, "AAAAA");
    kprintf("Producer %s: returned a semaphore\n", name);
    signal(sem);
}

// The major test function for semaphore
void test_sem()
{
    int sem, consumer1, consumer2, producer;

    kprintf("\n=== Semaphore Priority Test ===\n");
    sem = screate(1); // Creating a semaphore with count of 1
    verify(sem != SYSERR, "Semaphore creation error\n");

    // Creating my producer and consumers with different priorities
    producer = create(sprod, 2000, Low, "producer", 2, "Sprod", sem);
    consumer1 = create(scon, 2000, Med, "Consumer1", 2, "Scon1", sem);
    consumer2 = create(scon, 2000, High, "Consumer2", 2, "Scon2", sem);

    // Starting the producer
    kprintf("Producer goes first with prio %d\n", Low);
    resume(producer);
    sleep(3);

    // Starting the consumer1
    kprintf("Consumer1 goes next with prio %d\n", Med);
    resume(consumer1);
    sleep(3);
    kprintf("Producer priority should still be 15 and the prio is %d\n", getprio(producer));

    // Starting the consumer2
    kprintf("Consumer2 goes next with prio %d\n", High);
    resume(consumer2);
    sleep(3);
    kprintf("Producer priority should still be 15 and the prio is %d\n", getprio(producer)); // Since semaphore has no inheritance

    // Now killing the 2nd consumer
    kprintf("Stopping consumer2 with prio %d\n", High); // Using High since consumer2 isn’t running yet
    kill(consumer2);
    sleep(1);
    kprintf("Producer priority should still be 15 and the prio is %d\n", getprio(producer));

    // Now killing the 1st consumer
    kprintf("Stopping consumer1 with prio %d\n", Med); // Using Med since consumer1 isn’t running
    kill(consumer1);
    sleep(1);
    kprintf("Producer priority should still be 15 and the prio is %d\n", getprio(producer));

    sleep(10); // Waiting for producer to finish
    kprintf("Semaphore Test Finished\n");
}

/* Function for consumers using my locks */
void lcon(char *name, int lck)
{
    kprintf("Consumer %s: want a lock\n", name);        /* Attempting to lock */
    lock(lck, READ, 20);                                /* Use my lock function, READ mode */
    kprintf("Consumer %s: got a lock\n", name);         /* Got it */
    sleep(3);                                           /* Work for 3s */
    kprintf("Consumer %s: Letting go of lock\n", name); /* Release it */
    releaseall(1, lck);
}

/* Function for producer using my locks */
void lprod(char *name, int lck)
{
    kprintf("Producer %s: want a lock\n", name);              /* Trying to lock */
    lock(lck, WRITE, 20);                                     /* Use WRITE mode for exclusive access */
    kprintf("Producer %s: got the lock working 20s\n", name); /* Locked, work for 20s */
    sleep(20);                                                /* Long task to test inheritance */
    strcpy(buffer, "AAAAA");                                  /* Updating buffer */
    kprintf("Producer %s: Letting go of lock\n", name);       /* Releasing it */
    releaseall(1, lck);
}

/* Test function for my locks */
void test_lock()
{
    /* Variables for processes and lock */
    int lock, cons1, cons2, prod;

    kprintf("\n=== Lock Priority Inheritance Test ===\n"); /* Start lock test */
    lock = lcreate();                                      /* Create my lock */
    verify(lock != SYSERR, "Lock creation error\n");       /* Check if it worked */

    /* Create producer and consumers */
    prod = create(lprod, 2000, Low, "producer", 2, "Lprod", lock);
    cons1 = create(lcon, 2000, Med, "consumer1", 2, "Lcon1", lock);
    cons2 = create(lcon, 2000, High, "consumer2", 2, "Lcon2", lock);

    /* Starting producer first */
    kprintf("Producer starts first with priority %d\n", Low);
    resume(prod);
    sleep(3); /* Let it grab the lock */

    /* Start first consumer, should raise Lprod’s priority */
    kprintf("Consumer1 starts next with prio %d\n", Med);
    resume(cons1);
    sleep(3);
    kprintf("Producer prio after consumer1: %d\n", getprio(prod)); /* Expected 25 */
    verify(getprio(prod) == Med, "Inheritance to 25 failed\n");

    /* Start second consumer, raises priority more */
    kprintf("Starting the second consumer with prio %d\n", High);
    resume(cons2);
    sleep(3);
    kprintf("Producer prio after 2nd consumer: %d\n", getprio(prod)); /* Expecting 35 */
    verify(getprio(prod) == High, "Inheritance to 35 failed\n");

    /* Kill second consumer, priority should drop */
    kprintf("Stop Lcon2, pause 3s\n");
    kill(cons2);
    sleep(3);
    kprintf("Producer prio after killing Lcon2: %d\n", getprio(prod)); /* Expect 25 */
    verify(getprio(prod) == Med, "Revert to 25 failed\n");

    /* Kill first consumer, back to original */
    kprintf("Stop Lcon1, pause 3s\n");
    kill(cons1);
    sleep(3);
    kprintf("Producer prio after killing Lcon1: %d\n", getprio(prod)); /* Expecting 15 */
    verify(getprio(prod) == Low, "Revert to 15 failed\n");

    sleep(10);
    kprintf("Lock Test Finished\n");
}

/* My main function to run both tests */
int main(void)
{
    kprintf("\n=== Priority Inversion Comparison ===\n");
    mysem = screate(1); /* Set up global semaphore */
    mylock = lcreate(); /* Set up global lock */
    test_sem();         /* Run semaphore test first */
    test_lock();        /* Then my lock test */
    return OK;
}