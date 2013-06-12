#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include "timer_queue.h"


struct timer *TQ;

static struct timer hello_timer;

void timer_add_msec(struct timer *t, long msec);
void send_hb(int sock);

void timer_queue_init()
{
    TQ = NULL;

/*
    timer_add_msec(&hello_timer, 10000);
*/
}

void printTQ()
{
    struct timer *t;
    struct timeval now;
    int n = 0;
    

    gettimeofday(&now, NULL);

    printf("%-12s %-4s\n", "left", "n");
    for (t = TQ; t != NULL; t = t->next) {
	printf("%-12ld %-4d %-1s\n", timeval_diff(&t->timeout, &now), n, t->name);
	n++;
        if(n>20)
            break;
    }
    return;
}

/* Called when a timer should timeout */
void timer_timeout(struct timeval *now)
{
    struct timer *prev, *expiredTQ, *tmp;
    expiredTQ = TQ;
    for (prev = NULL; TQ; prev = TQ, TQ = TQ->next)
	if (timeval_diff(&TQ->timeout, now) > 0)
	    break;

    /* No expired timers? */
    if (prev == NULL)
	return;

    /* Decouple the expired timers from the rest of the queue. */
    prev->next = NULL;

/*
    fprintf(stderr,"PREV: %d \n", (int)TQ->timeout.tv_sec);
    fprintf(stderr,"PREV: %d \n", (int)TQ->timeout.tv_usec);
*/

    while (expiredTQ) {
	tmp = expiredTQ;
	tmp->used = 0;
	expiredTQ = expiredTQ->next;
	/* Execute handler function for expired timer... */
	if (tmp->handler) {
	    tmp->handler(tmp->data);
	}        
    }
}


void timer_add(struct timer *t)
{
    struct timer *curr, *prev;
    /* Sanity checks: */

    if (!t) {
	perror("NULL timer!!!\n");
	exit(-1);
    }
    if (!t->handler) {
	perror("NULL handler!!!\n");
	exit(-1);
    }

/*
    fprintf(stderr,"TQ: %d \n", (int)t->timeout.tv_sec);
    fprintf(stderr,"TQ: %d \n", (int)t->timeout.tv_usec);
*/

    /* Make sure we remove unexpired timers before adding a new timeout... */
    if (t->used){
        fprintf(stderr,"Timer used, so remove timer\n");
        timer_remove(t);
    }

    t->used = 1;

    /* Base case when queue is empty: */
    if (!TQ) {
        fprintf(stderr,"Timer queue is empty\n");
	TQ = t;
	t->next = NULL;
	/* Since we insert first in the queue we must reschedule the
	   first timeout */
	goto end;
    }
    
    

    for (prev = NULL, curr = TQ; curr != NULL; prev = curr, curr = curr->next){             
	if (timeval_diff(&t->timeout, &curr->timeout) < 0)
            // Is this checking for the right place to put the new timer?
	    break;
    }
    
    

    if (curr == TQ) {
	/* We insert first in queue */
	t->next = TQ;
	TQ = t;
    } else {
	t->next = curr;
	prev->next = t;
    }
    
    

  end:

//    printTQ();
    return;
}

int timer_timeout_now(struct timer *t)
{

    struct timer *curr, *prev;
    if (!t)
	return -1;
    t->used = 0;

    for (prev = NULL, curr = TQ; curr; prev = curr, curr = curr->next) {
	if (curr == t) {
	    /* Decouple timer from queue */
	    if (prev == NULL)
		TQ = t->next;
	    else
		prev->next = t->next;
	    t->next = NULL;
	    t->handler(t->data);

	    return 0;
	}
    }
    /* We didn't find the timer... */
    return -1;
}

int timer_remove(struct timer *t)
{
    struct timer *curr, *prev;

    if (!t){
        fprintf(stderr,"The timer to remove is NULL\n");
        return -1;
    }

    t->used = 0;

    for (prev = NULL, curr = TQ; curr != NULL; prev = curr, curr = curr->next) {
	if (curr == t) {
	    if (prev == NULL)
		TQ = t->next;
	    else
		prev->next = t->next;

	    t->next = NULL;
	    return 0;
	}
    }
    /* We didn't find the timer... */
    fprintf(stderr,"We didn't find the timer to remove\n");
    return -1;
}


void timer_add_msec(struct timer *t, long msec)
{
    fprintf(stderr, "Timer add msec\n");
    gettimeofday(&t->timeout, NULL);

    if (msec < 0)
	fprintf(stderr, "timer_add_msec: Negative timeout!!! \n");    

    t->timeout.tv_usec += msec * 1000;
    t->timeout.tv_sec += t->timeout.tv_usec / 1000000;
    t->timeout.tv_usec = t->timeout.tv_usec % 1000000; 
    
    fprintf(stderr,"We will enter timer_add\n");

    timer_add(t);
    return;
}

long timer_left(struct timer *t)
{
    struct timeval now;

    if (!t)
	return -1;

    gettimeofday(&now, NULL);

    return timeval_diff(&now, &t->timeout);
}

struct timeval *timer_age_queue()
{
    struct timeval now;
    static struct timeval remaining;
    gettimeofday(&now, NULL);
    timer_timeout(&now);  

    if (!TQ){

        fprintf(stderr,"TQ ES NULL EN TIME_AGE_QUEUE \n");
        return NULL;
    }
    remaining.tv_usec = (TQ->timeout.tv_usec - now.tv_usec);
    remaining.tv_sec = (TQ->timeout.tv_sec - now.tv_sec);

    if (remaining.tv_usec < 0) {
	remaining.tv_usec += 1000000;
	remaining.tv_sec -= 1;
    }
    return (&remaining);
}





