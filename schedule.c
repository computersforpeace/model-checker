#include "schedule.h"
#include "common.h"

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

static int dequeue_thread(struct thread **t)
{
	if (!head) {
		*t = NULL;
		return -1;
	}
	*t = head->this;
	head->live = 0;
	if (head == tail)
		tail = NULL;
	head = head->next;
	return 0;
}

void schedule_add_thread(struct thread *t)
{
	DEBUG("thread %d\n", t->index);
	enqueue_thread(t);
}

int schedule_choose_next(struct thread **t)
{
	return dequeue_thread(t);
}
