/**
 * @file workqueue.h
 * @brief Provides structures for queueing ModelChecker actions to be taken
 */

#ifndef __WORKQUEUE_H__
#define __WORKQUEUE_H__

#include "mymemory.h"
#include "stl-model.h"

class ModelAction;

typedef enum {
	WORK_NONE = 0,           /**< No work to be done */
	WORK_CHECK_CURR_ACTION,  /**< Check the current action; used for the
	                              first action of the work loop */
	WORK_CHECK_RELEASE_SEQ,  /**< Check if any pending release sequences
	                              are resolved */
	WORK_CHECK_MO_EDGES,     /**< Check if new mo_graph edges can be added */
} model_work_t;

/**
 */
class WorkQueueEntry {
 public:
	/** @brief Type of work queue entry */
	model_work_t type;

	/**
	 * @brief Object affected
	 * @see CheckRelSeqWorkEntry
	 */
	void *location;

	/**
	 * @brief The ModelAction to work on
	 * @see MOEdgeWorkEntry
	 */
	ModelAction *action;
};

/**
 * @brief Work: perform initial promise, mo_graph checks on the current action
 *
 * This WorkQueueEntry performs the normal, first-pass checks for a ModelAction
 * that is currently being explored. The current ModelAction (@a action) is the
 * only relevant parameter to this entry.
 */
class CheckCurrWorkEntry : public WorkQueueEntry {
 public:
	/**
	 * @brief Constructor for a "check current action" work entry
	 * @param curr The current action
	 */
	CheckCurrWorkEntry(ModelAction *curr) {
		type = WORK_CHECK_CURR_ACTION;
		location = NULL;
		action = curr;
	}
};

/**
 * @brief Work: check an object location for resolved release sequences
 *
 * This WorkQueueEntry checks synchronization and the mo_graph for resolution
 * of any release sequences. The object @a location is the only relevant
 * parameter to this entry.
 */
class CheckRelSeqWorkEntry : public WorkQueueEntry {
 public:
	/**
	 * @brief Constructor for a "check release sequences" work entry
	 * @param l The location which must be checked for release sequences
	 */
	CheckRelSeqWorkEntry(void *l) {
		type = WORK_CHECK_RELEASE_SEQ;
		location = l;
		action = NULL;
	}
};

/**
 * @brief Work: check a ModelAction for new mo_graph edges
 *
 * This WorkQueueEntry checks for new mo_graph edges for a particular
 * ModelAction (e.g., that was just generated or that updated its
 * synchronization). The ModelAction @a action is the only relevant parameter
 * to this entry.
 */
class MOEdgeWorkEntry : public WorkQueueEntry {
 public:
	/**
	 * @brief Constructor for a mo_edge work entry
	 * @param updated The ModelAction which was updated, triggering this work
	 */
	MOEdgeWorkEntry(ModelAction *updated) {
		type = WORK_CHECK_MO_EDGES;
		location = NULL;
		action = updated;
	}
};

/** @brief typedef for the work queue type */
typedef ModelList<WorkQueueEntry> work_queue_t;

#endif /* __WORKQUEUE_H__ */
