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
struct thread *current;

static void enqueue_thread(struct thread *t)
{
	int i;
	struct thread_list_node *node;

	for (node = nodes, i = 0; node->live && i < NUM_LIST_NODES; i++, node++);
	if (i >= NUM_LIST_NODES) {
		printf("ran out of nodes\n");
		exit(1);
	}
	node->this = t;
	node->next = NULL;
	node->live = 1;

	if (tail)
		tail->next = node;
	else
		head = node;
	tail = node;
}

static struct thread *dequeue_thread(void)
{
	struct thread *pop;

	if (!head)
		return NULL;

	pop = head->this;
	head->live = 0;
	if (head == tail)
		tail = NULL;
	head = head->next;

	/* Set new current thread */
	current = pop;

	return pop;
}

static void default_add_thread(struct thread *t)
{
	DEBUG("thread %d\n", t->index);
	enqueue_thread(t);
}

static struct thread *default_choose_next(void)
{
	return dequeue_thread();
}

static struct thread *default_thread_current(void)
{
	return current;
}

void scheduler_init(struct model_checker *mod)
{
	struct scheduler *sched;

	/* Initialize default scheduler */
	sched = malloc(sizeof(*sched));
	sched->init = NULL;
	sched->exit = NULL;
	sched->add_thread = default_add_thread;
	sched->next_thread = default_choose_next;
	sched->get_current_thread = default_thread_current;
	mod->scheduler = sched;
}
