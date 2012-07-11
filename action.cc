#include <stdio.h>

#include "model.h"
#include "action.h"
#include "clockvector.h"
#include "common.h"

ModelAction::ModelAction(action_type_t type, memory_order order, void *loc, int value) :
	type(type),
	order(order),
	location(loc),
	value(value),
	reads_from(NULL),
	cv(NULL)
{
	Thread *t = thread_current();
	this->tid = t->get_id();
	this->seq_number = model->get_next_seq_num();
}

ModelAction::~ModelAction()
{
	if (cv)
		delete cv;
}

bool ModelAction::is_read() const
{
	return type == ATOMIC_READ;
}

bool ModelAction::is_write() const
{
	return type == ATOMIC_WRITE || type == ATOMIC_INIT;
}

bool ModelAction::is_rmw() const
{
	return type == ATOMIC_RMW;
}

bool ModelAction::is_initialization() const
{
	return type == ATOMIC_INIT;
}

bool ModelAction::is_acquire() const
{
	switch (order) {
	case memory_order_acquire:
	case memory_order_acq_rel:
	case memory_order_seq_cst:
		return true;
	default:
		return false;
	}
}

bool ModelAction::is_release() const
{
	switch (order) {
	case memory_order_release:
	case memory_order_acq_rel:
	case memory_order_seq_cst:
		return true;
	default:
		return false;
	}
}

bool ModelAction::is_seqcst() const
{
	return order==memory_order_seq_cst;
}

bool ModelAction::same_var(const ModelAction *act) const
{
	return location == act->location;
}

bool ModelAction::same_thread(const ModelAction *act) const
{
	return tid == act->tid;
}

/** The is_synchronizing method should only explore interleavings if:
 *  (1) the operations are seq_cst and don't commute or
 *  (2) the reordering may establish or break a synchronization relation.
 *  Other memory operations will be dealt with by using the reads_from
 *  relation.
 *
 *  @param act is the action to consider exploring a reordering.
 *  @return tells whether we have to explore a reordering.
 */

bool ModelAction::is_synchronizing(const ModelAction *act) const
{
	//Same thread can't be reordered
	if (same_thread(act))
		return false;

	// Different locations commute
	if (!same_var(act))
		return false;
	
	// Explore interleavings of seqcst writes to guarantee total order
	// of seq_cst operations that don't commute
	if (is_write() && is_seqcst() && act->is_write() && act->is_seqcst())
		return true;

	// Explore synchronizing read/write pairs
	if (is_read() && is_acquire() && act->is_write() && act->is_release())
		return true;
	if (is_write() && is_release() && act->is_read() && act->is_acquire())
		return true;

	// Otherwise handle by reads_from relation
	return false;
}

void ModelAction::create_cv(const ModelAction *parent)
{
	ASSERT(cv == NULL);

	if (parent)
		cv = new ClockVector(parent->cv, this);
	else
		cv = new ClockVector(NULL, this);
}

void ModelAction::read_from(const ModelAction *act)
{
	ASSERT(cv);
	if (act->is_release() && this->is_acquire())
		cv->merge(act->cv);
	reads_from = act;
	value = act->value;
}

/**
 * Check whether 'this' happens before act, according to the memory-model's
 * happens before relation. This is checked via the ClockVector constructs.
 * @return true if this action's thread has synchronized with act's thread
 * since the execution of act, false otherwise.
 */
bool ModelAction::happens_before(const ModelAction *act) const
{
	return act->cv->synchronized_since(this);
}

void ModelAction::print(void) const
{
	const char *type_str;
	switch (this->type) {
	case THREAD_CREATE:
		type_str = "thread create";
		break;
	case THREAD_YIELD:
		type_str = "thread yield";
		break;
	case THREAD_JOIN:
		type_str = "thread join";
		break;
	case ATOMIC_READ:
		type_str = "atomic read";
		break;
	case ATOMIC_WRITE:
		type_str = "atomic write";
		break;
	case ATOMIC_RMW:
		type_str = "atomic rmw";
		break;
	case ATOMIC_INIT:
		type_str = "init atomic";
		break;
	default:
		type_str = "unknown type";
	}

	printf("(%3d) Thread: %-2d    Action: %-13s    MO: %d    Loc: %14p    Value: %-4d",
			seq_number, id_to_int(tid), type_str, order, location, value);
	if (reads_from)
		printf(" Rf: %d", reads_from->get_seq_number());
	if (cv) {
		printf("\t");
		cv->print();
	} else
		printf("\n");
}
