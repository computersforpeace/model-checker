#include <stdlib.h>

#include "libthreads.h"
#include "schedule.h"
#include "common.h"
#include "model.h"

struct thread_list_node {
	struct thread *t;
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
	node->t = t;
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

	pop = head->t;
	head->live = 0;
	if (head == tail)
		tail = NULL;
	head = head->next;

	/* Set new current thread */
	current = pop;

	return pop;
}

void DefaultScheduler::add_thread(struct thread *t)
{
	DEBUG("thread %d\n", t->id);
	enqueue_thread(t);
}

struct thread *DefaultScheduler::next_thread(void)
{
	return dequeue_thread();
}

struct thread *DefaultScheduler::get_current_thread(void)
{
	return current;
}
