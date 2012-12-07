#include <stdio.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <vector>

#include "model.h"
#include "action.h"
#include "clockvector.h"
#include "common.h"
#include "threads-model.h"
#include "nodestack.h"

#define ACTION_INITIAL_CLOCK 0

/**
 * @brief Construct a new ModelAction
 *
 * @param type The type of action
 * @param order The memory order of this action. A "don't care" for non-ATOMIC
 * actions (e.g., THREAD_* or MODEL_* actions).
 * @param loc The location that this action acts upon
 * @param value (optional) A value associated with the action (e.g., the value
 * read or written). Defaults to a given macro constant, for debugging purposes.
 * @param thread (optional) The Thread in which this action occurred. If NULL
 * (default), then a Thread is assigned according to the scheduler.
 */
ModelAction::ModelAction(action_type_t type, memory_order order, void *loc,
		uint64_t value, Thread *thread) :
	type(type),
	order(order),
	location(loc),
	value(value),
	reads_from(NULL),
	last_fence_release(NULL),
	node(NULL),
	seq_number(ACTION_INITIAL_CLOCK),
	cv(NULL),
	sleep_flag(false)
{
	/* References to NULL atomic variables can end up here */
	ASSERT(loc || type == ATOMIC_FENCE || type == MODEL_FIXUP_RELSEQ);

	Thread *t = thread ? thread : thread_current();
	this->tid = t->get_id();
}

/** @brief ModelAction destructor */
ModelAction::~ModelAction()
{
	/**
	 * We can't free the clock vector:
	 * Clock vectors are snapshotting state. When we delete model actions,
	 * they are at the end of the node list and have invalid old clock
	 * vectors which have already been rolled back to an unallocated state.
	 */

	/*
	 if (cv)
		delete cv; */
}

void ModelAction::copy_from_new(ModelAction *newaction)
{
	seq_number = newaction->seq_number;
}

void ModelAction::set_seq_number(modelclock_t num)
{
	/* ATOMIC_UNINIT actions should never have non-zero clock */
	ASSERT(!is_uninitialized());
	ASSERT(seq_number == ACTION_INITIAL_CLOCK);
	seq_number = num;
}

bool ModelAction::is_thread_start() const
{
	return type == THREAD_START;
}

bool ModelAction::is_relseq_fixup() const
{
	return type == MODEL_FIXUP_RELSEQ;
}

bool ModelAction::is_mutex_op() const
{
	return type == ATOMIC_LOCK || type == ATOMIC_TRYLOCK || type == ATOMIC_UNLOCK || type == ATOMIC_WAIT || type == ATOMIC_NOTIFY_ONE || type == ATOMIC_NOTIFY_ALL;
}

bool ModelAction::is_lock() const
{
	return type == ATOMIC_LOCK;
}

bool ModelAction::is_wait() const {
	return type == ATOMIC_WAIT;
}

bool ModelAction::is_notify() const {
	return type==ATOMIC_NOTIFY_ONE || type==ATOMIC_NOTIFY_ALL;
}

bool ModelAction::is_notify_one() const {
	return type==ATOMIC_NOTIFY_ONE;
}

bool ModelAction::is_unlock() const
{
	return type == ATOMIC_UNLOCK;
}

bool ModelAction::is_trylock() const
{
	return type == ATOMIC_TRYLOCK;
}

bool ModelAction::is_success_lock() const
{
	return type == ATOMIC_LOCK || (type == ATOMIC_TRYLOCK && value == VALUE_TRYSUCCESS);
}

bool ModelAction::is_failed_trylock() const
{
	return (type == ATOMIC_TRYLOCK && value == VALUE_TRYFAILED);
}

bool ModelAction::is_uninitialized() const
{
	return type == ATOMIC_UNINIT;
}

bool ModelAction::is_read() const
{
	return type == ATOMIC_READ || type == ATOMIC_RMWR || type == ATOMIC_RMW;
}

bool ModelAction::is_write() const
{
	return type == ATOMIC_WRITE || type == ATOMIC_RMW || type == ATOMIC_INIT || type == ATOMIC_UNINIT;
}

bool ModelAction::could_be_write() const
{
	return is_write() || is_rmwr();
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

bool ModelAction::is_fence() const 
{
	return type == ATOMIC_FENCE;
}

bool ModelAction::is_initialization() const
{
	return type == ATOMIC_INIT;
}

bool ModelAction::is_relaxed() const
{
	return order == std::memory_order_relaxed;
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
	if ( act->is_wait() || is_wait() ) {
		if ( act->is_wait() && is_wait() ) {
			if ( ((void *)value) == ((void *)act->value) )
				return true;
		} else if ( is_wait() ) {
			if ( ((void *)value) == act->location )
				return true;
		} else if ( act->is_wait() ) {
			if ( location == ((void *)act->value) )
				return true;
		}
	}

	return location == act->location;
}

bool ModelAction::same_thread(const ModelAction *act) const
{
	return tid == act->tid;
}

void ModelAction::copy_typeandorder(ModelAction * act) {
	this->type = act->type;
	this->order = act->order;
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
bool ModelAction::could_synchronize_with(const ModelAction *act) const
{
	//Same thread can't be reordered
	if (same_thread(act))
		return false;

	// Different locations commute
	if (!same_var(act))
		return false;

	// Explore interleavings of seqcst writes/fences to guarantee total
	// order of seq_cst operations that don't commute
	if ((could_be_write() || act->could_be_write() || is_fence() || act->is_fence())
			&& is_seqcst() && act->is_seqcst())
		return true;

	// Explore synchronizing read/write/fence pairs
	if (is_acquire() && act->is_release() && (is_read() || is_fence()) &&
			(act->could_be_write() || act->is_fence()))
		return true;

	//lock just released...we can grab lock
	if ((is_lock() ||is_trylock()) && (act->is_unlock()||act->is_wait()))
		return true;

	//lock just acquired...we can fail to grab lock
	if (is_trylock() && act->is_success_lock())
		return true;

	//other thread stalling on lock...we can release lock
	if (is_unlock() && (act->is_trylock()||act->is_lock()))
		return true;

	if (is_trylock() && (act->is_unlock()||act->is_wait()))
		return true;

	if ( is_notify() && act->is_wait() )
		return true;

	if ( is_wait() && act->is_notify() )
		return true;

	// Otherwise handle by reads_from relation
	return false;
}

bool ModelAction::is_conflicting_lock(const ModelAction *act) const
{
	//Must be different threads to reorder
	if (same_thread(act))
		return false;
	
	//Try to reorder a lock past a successful lock
	if (act->is_success_lock())
		return true;
	
	//Try to push a successful trylock past an unlock
	if (act->is_unlock() && is_trylock() && value == VALUE_TRYSUCCESS)
		return true;

	//Try to push a successful trylock past a wait
	if (act->is_wait() && is_trylock() && value == VALUE_TRYSUCCESS)
		return true;

	return false;
}

/**
 * Create a new clock vector for this action. Note that this function allows a
 * user to clobber (and leak) a ModelAction's existing clock vector. A user
 * should ensure that the vector has already either been rolled back
 * (effectively "freed") or freed.
 *
 * @param parent A ModelAction from which to inherit a ClockVector
 */
void ModelAction::create_cv(const ModelAction *parent)
{
	if (parent)
		cv = new ClockVector(parent->cv, this);
	else
		cv = new ClockVector(NULL, this);
}

void ModelAction::set_try_lock(bool obtainedlock) {
	if (obtainedlock)
		value=VALUE_TRYSUCCESS;
	else
		value=VALUE_TRYFAILED;
}

/** @return The Node associated with this ModelAction */
Node * ModelAction::get_node() const
{
	return node;
}

/**
 * Update the model action's read_from action
 * @param act The action to read from; should be a write
 */
void ModelAction::set_read_from(const ModelAction *act)
{
	reads_from = act;
	if (act && act->is_uninitialized())
		model->assert_bug("May read from uninitialized atomic\n");
}

/**
 * Synchronize the current thread with the thread corresponding to the
 * ModelAction parameter.
 * @param act The ModelAction to synchronize with
 * @return True if this is a valid synchronization; false otherwise
 */
bool ModelAction::synchronize_with(const ModelAction *act) {
	if (*this < *act && type != THREAD_JOIN && type != ATOMIC_LOCK)
		return false;
	model->check_promises(act->get_tid(), cv, act->cv);
	cv->merge(act->cv);
	return true;
}

bool ModelAction::has_synchronized_with(const ModelAction *act) const
{
	return cv->synchronized_since(act);
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

/** @brief Print nicely-formatted info about this ModelAction */
void ModelAction::print() const
{
	const char *type_str, *mo_str;
	switch (this->type) {
	case MODEL_FIXUP_RELSEQ:
		type_str = "relseq fixup";
		break;
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
	case ATOMIC_UNINIT:
		type_str = "uninitialized";
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
	case ATOMIC_FENCE:
		type_str = "fence";
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
	case ATOMIC_LOCK:
		type_str = "lock";
		break;
	case ATOMIC_UNLOCK:
		type_str = "unlock";
		break;
	case ATOMIC_TRYLOCK:
		type_str = "trylock";
		break;
	case ATOMIC_WAIT:
		type_str = "wait";
		break;
	case ATOMIC_NOTIFY_ONE:
		type_str = "notify one";
		break;
	case ATOMIC_NOTIFY_ALL:
		type_str = "notify all";
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

	model_print("(%4d) Thread: %-2d   Action: %-13s   MO: %7s  Loc: %14p   Value: %-#18" PRIx64,
			seq_number, id_to_int(tid), type_str, mo_str, location, valuetoprint);
	if (is_read()) {
		if (reads_from)
			model_print("  Rf: %-3d", reads_from->get_seq_number());
		else
			model_print("  Rf: ?  ");
	}
	if (cv) {
		if (is_read())
			model_print(" ");
		else
			model_print("          ");
		cv->print();
	} else
		model_print("\n");
}

/** @brief Print nicely-formatted info about this ModelAction */
unsigned int ModelAction::hash() const
{
	unsigned int hash=(unsigned int) this->type;
	hash^=((unsigned int)this->order)<<3;
	hash^=seq_number<<5;
	hash ^= id_to_int(tid) << 6;

	if (is_read()) {
		if (reads_from)
			hash^=reads_from->get_seq_number();
	}
	return hash;
}
