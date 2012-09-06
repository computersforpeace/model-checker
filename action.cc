#include <stdio.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <vector>

#include "model.h"
#include "action.h"
#include "clockvector.h"
#include "common.h"

ModelAction::ModelAction(action_type_t type, memory_order order, void *loc, uint64_t value) :
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
	return type == ATOMIC_READ || type == ATOMIC_RMWR || type == ATOMIC_RMW;
}

bool ModelAction::is_write() const
{
	return type == ATOMIC_WRITE || type == ATOMIC_RMW || type == ATOMIC_INIT;
}

bool ModelAction::is_rmwr() const
{
	return type == ATOMIC_RMWR;
}

bool ModelAction::is_rmw() const
{
	return type == ATOMIC_RMW;
}

bool ModelAction::is_rmwc() const
{
	return type == ATOMIC_RMWC;
}

bool ModelAction::is_initialization() const
{
	return type == ATOMIC_INIT;
}

bool ModelAction::is_acquire() const
{
	switch (order) {
	case std::memory_order_acquire:
	case std::memory_order_acq_rel:
	case std::memory_order_seq_cst:
		return true;
	default:
		return false;
	}
}

bool ModelAction::is_release() const
{
	switch (order) {
	case std::memory_order_release:
	case std::memory_order_acq_rel:
	case std::memory_order_seq_cst:
		return true;
	default:
		return false;
	}
}

bool ModelAction::is_seqcst() const
{
	return order==std::memory_order_seq_cst;
}

bool ModelAction::same_var(const ModelAction *act) const
{
	return location == act->location;
}

bool ModelAction::same_thread(const ModelAction *act) const
{
	return tid == act->tid;
}

void ModelAction::copy_typeandorder(ModelAction * act) {
	this->type=act->type;
	this->order=act->order;
}

/** This method changes an existing read part of an RMW action into either:
 *  (1) a full RMW action in case of the completed write or
 *  (2) a READ action in case a failed action.
 * @todo  If the memory_order changes, we may potentially need to update our
 * clock vector.
 */
void ModelAction::process_rmw(ModelAction * act) {
	this->order=act->order;
	if (act->is_rmwc())
		this->type=ATOMIC_READ;
	else if (act->is_rmw()) {
		this->type=ATOMIC_RMW;
		this->value=act->value;
	}
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
	if (cv)
		delete cv;

	if (parent)
		cv = new ClockVector(parent->cv, this);
	else
		cv = new ClockVector(NULL, this);
}

/** Update the model action's read_from action */
void ModelAction::read_from(const ModelAction *act)
{
	ASSERT(cv);
	reads_from = act;
	if (act != NULL && this->is_acquire()) {
		std::vector<const ModelAction *> release_heads;
		model->get_release_seq_heads(this, &release_heads);
		for (unsigned int i = 0; i < release_heads.size(); i++)
			synchronize_with(release_heads[i]);
	}
}

/**
 * Synchronize the current thread with the thread corresponding to the
 * ModelAction parameter.
 * @param act The ModelAction to synchronize with
 */
void ModelAction::synchronize_with(const ModelAction *act) {
	ASSERT(*act < *this);
	model->check_promises(cv, act->cv);
	cv->merge(act->cv);
}

bool ModelAction::has_synchronized_with(const ModelAction *act) const
{
	return cv->has_synchronized_with(act->cv);
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
	const char *type_str, *mo_str;
	switch (this->type) {
	case THREAD_CREATE:
		type_str = "thread create";
		break;
	case THREAD_START:
		type_str = "thread start";
		break;
	case THREAD_YIELD:
		type_str = "thread yield";
		break;
	case THREAD_JOIN:
		type_str = "thread join";
		break;
	case THREAD_FINISH:
		type_str = "thread finish";
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
	case ATOMIC_RMWR:
		type_str = "atomic rmwr";
		break;
	case ATOMIC_RMWC:
		type_str = "atomic rmwc";
		break;
	case ATOMIC_INIT:
		type_str = "init atomic";
		break;
	default:
		type_str = "unknown type";
	}

	uint64_t valuetoprint=type==ATOMIC_READ?(reads_from!=NULL?reads_from->value:VALUE_NONE):value;

	switch (this->order) {
	case std::memory_order_relaxed:
		mo_str = "relaxed";
		break;
	case std::memory_order_acquire:
		mo_str = "acquire";
		break;
	case std::memory_order_release:
		mo_str = "release";
		break;
	case std::memory_order_acq_rel:
		mo_str = "acq_rel";
		break;
	case std::memory_order_seq_cst:
		mo_str = "seq_cst";
		break;
	default:
		mo_str = "unknown";
		break;
	}

	printf("(%3d) Thread: %-2d   Action: %-13s   MO: %7s  Loc: %14p  Value: %-12" PRIu64,
			seq_number, id_to_int(tid), type_str, mo_str, location, valuetoprint);
	if (is_read()) {
		if (reads_from)
			printf(" Rf: %d", reads_from->get_seq_number());
		else
			printf(" Rf: ?");
	}
	if (cv) {
		printf("\t");
		cv->print();
	} else
		printf("\n");
}
