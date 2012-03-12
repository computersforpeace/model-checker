#include <stdlib.h>

#include "libthreads.h"
#include "schedule.h"
#include "common.h"
#include "model.h"

struct thread_list_node {
	struct thread *this;
	struct thread_list_node *next;
	int live;
};

#define NUM_LIST_NODES 32

struct thread_list_node *head, *tail;
struct thread_list_node nodes[NUM_LIST_NODES];

static void enqueue_thread(struct thread *t)
{
	int i;
	struct thread_list_node *node;

	for (node = nodes, i = 0; node->live && i < NUM_LIST_NODES; i++, node++);
	if (i >= NUM_LIST_NODES)
		printf("ran out of nodes\n");
	node->this = t;
	node->next = NULL;
	node->live = 1;

	if (tail)
		tail->next = node;
	else
		head = node;
	tail = node;
}

struct thread *dequeue_thread(void)
{
	struct thread *pop;

	if (!head)
		return NULL;

	pop = head->this;
	head->live = 0;
	if (head == tail)
		tail = NULL;
	head = head->next;

	return pop;
}

void schedule_add_thread(struct thread *t)
{
	DEBUG("thread %d\n", t->index);
	enqueue_thread(t);
}

struct thread *schedule_choose_next(void)
{
	return dequeue_thread();
}

void scheduler_init(struct model_checker *mod)
{
	struct scheduler *sched;

	/* Initialize FCFS scheduler */
	sched = malloc(sizeof(*sched));
	sched->init = NULL;
	sched->exit = NULL;
	sched->add_thread = schedule_add_thread;
	sched->next_thread = schedule_choose_next;
	sched->get_current_thread = thread_current;
	mod->scheduler = sched;
}
