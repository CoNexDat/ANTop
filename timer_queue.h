#include <sys/time.h>
#include <stdio.h>

typedef void (*timeout_func_t) (void *);

struct timer {
    struct timeval timeout;
    timeout_func_t handler;
    void *data;
    int used;
    char name[20];
    struct timer *next;
};

static inline long timeval_diff(struct timeval *t1, struct timeval *t2)
{
    if (!t1 || !t2)
	return -1;
    else
	return (1000000 * (t1->tv_sec - t2->tv_sec) + t1->tv_usec -
		t2->tv_usec);
}

static inline int timeval_add_msec(struct timeval *t, long msec)
{
    if (!t)
	return -1;

    t->tv_usec += msec * 1000;
    t->tv_sec += t->tv_usec / 1000000;
    t->tv_usec = t->tv_usec % 1000000;
    return 0;
}



void timer_queue_init();
int timer_remove(struct timer *t);
void timer_add_msec(struct timer *t, long msec);
int timer_timeout_now(struct timer *t);
struct timeval *timer_age_queue();






