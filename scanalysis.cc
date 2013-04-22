#include "scanalysis.h"
#include "action.h"
#include "threads-model.h"
#include "clockvector.h"
#include "execution.h"

SCAnalysis::SCAnalysis(const ModelExecution *execution) :
	cvmap(),
	cyclic(false),
	badrfset(),
	lastwrmap(),
	threadlists(1),
	execution(execution)
{
}

SCAnalysis::~SCAnalysis() {
}

void SCAnalysis::print_list(action_list_t *list) {
	model_print("---------------------------------------------------------------------\n");
	if (cyclic)
		model_print("Not SC\n");
	unsigned int hash = 0;

	for (action_list_t::iterator it = list->begin(); it != list->end(); it++) {
		const ModelAction *act = *it;
		if (act->get_seq_number() > 0) {
			if (badrfset.contains(act))
				model_print("BRF ");
			act->print();
			if (badrfset.contains(act)) {
				model_print("Desired Rf: %u \n",badrfset.get(act)->get_seq_number());
			}
		}
		hash = hash ^ (hash << 3) ^ ((*it)->hash());
	}
	model_print("HASH %u\n", hash);
	model_print("---------------------------------------------------------------------\n");
}

void SCAnalysis::analyze(action_list_t *actions) {
 	buildVectors(actions);
	computeCV(actions);
	action_list_t *list = generateSC(actions);
	check_rf(list);
	print_list(list);
}

void SCAnalysis::check_rf(action_list_t *list) {
	for (action_list_t::iterator it = list->begin(); it != list->end(); it++) {
		const ModelAction *act = *it;
		if (act->is_read()) {
			if (act->get_reads_from()!=lastwrmap.get(act->get_location()))
				badrfset.put(act,lastwrmap.get(act->get_location()));
		}
		if (act->is_write())
			lastwrmap.put(act->get_location(), act);
	}
}

bool SCAnalysis::merge(ClockVector *cv, const ModelAction *act, const ModelAction *act2) {
	ClockVector * cv2=cvmap.get(act2);
	if (cv2==NULL)
		return true;
	if (cv2->getClock(act->get_tid()) >= act->get_seq_number() && act->get_seq_number() != 0) {
		cyclic=true;
		//refuse to introduce cycles into clock vectors
		return false;
	}

	return cv->merge(cv2);
}

ModelAction * SCAnalysis::getNextAction() {
	ModelAction *act = NULL;
	/* Find the earliest in SC ordering */
	for (int i = 0; i <= maxthreads; i++) {
		action_list_t *threadlist = &threadlists[i];
		if (threadlist->empty())
			continue;
		ModelAction *first = threadlist->front();
		if (act==NULL) {
			act=first;
			continue;
		}
		ClockVector *cv = cvmap.get(act);
		if (cv->synchronized_since(first)) {
			act = first;
		}
	}
	if (act == NULL)
		return act;


	/* Find the model action with the earliest sequence number in case of a cycle.
	 */

	for (int i = 0; i <= maxthreads; i++) {
		action_list_t *threadlist = &threadlists[i];
		if (threadlist->empty())
			continue;
		ModelAction *first = threadlist->front();
		ClockVector *cv = cvmap.get(act);
		ClockVector *cvfirst = cvmap.get(first);
		if (first->get_seq_number()<act->get_seq_number()&&
				(cv->synchronized_since(first)||!cvfirst->synchronized_since(act))) {
			act=first;
		}
	}

	/* See if hb demands an earlier action. */
	for (int i = 0; i <= maxthreads; i++) {
		action_list_t *threadlist = &threadlists[i];
		if (threadlist->empty())
			continue;
		ModelAction *first = threadlist->front();
		ClockVector *cv = act->get_cv();
		if (cv->synchronized_since(first)) {
			act = first;
		}
	}
	return act;
}

action_list_t * SCAnalysis::generateSC(action_list_t *list) {
	action_list_t *sclist = new action_list_t();
	while (true) {
		ModelAction *act = getNextAction();
		if (act == NULL)
			break;
		thread_id_t tid = act->get_tid();
		//remove action
		threadlists[id_to_int(tid)].pop_front();
		//add ordering constraints from this choice
		if (updateConstraints(act)) {
			//propagate changes if we have them
			computeCV(list);
		}
		//add action to end
		sclist->push_back(act);
	}
	return sclist;
}

void SCAnalysis::buildVectors(action_list_t *list) {
	maxthreads = 0;
	for (action_list_t::iterator it = list->begin(); it != list->end(); it++) {
		ModelAction *act = *it;
		int threadid = id_to_int(act->get_tid());
		if (threadid > maxthreads) {
			threadlists.resize(threadid + 1);
			maxthreads = threadid;
		}
		threadlists[threadid].push_back(act);
	}
}

bool SCAnalysis::updateConstraints(ModelAction *act) {
	bool changed = false;
	for (int i = 0; i <= maxthreads; i++) {
		thread_id_t tid = int_to_id(i);
		if (tid == act->get_tid())
			continue;

		action_list_t *list = &threadlists[id_to_int(tid)];
		for (action_list_t::iterator rit = list->begin(); rit != list->end(); rit++) {
			ModelAction *write = *rit;
			if (!write->is_write())
				continue;
			ClockVector *writecv = cvmap.get(write);
			if (writecv->synchronized_since(act))
				break;
			if (write->get_location() == act->get_location()) {
				//write is sc after act
				merge(writecv, write, act);
				changed = true;
				break;
			}
		}
	}
	return changed;
}

bool SCAnalysis::processRead(ModelAction *read, ClockVector *cv) {
	bool changed = false;

	/* Merge in the clock vector from the write */
	const ModelAction *write = read->get_reads_from();
	ClockVector *writecv = cvmap.get(write);
	changed |= merge(cv, read, write) && (*read < *write);

	for (int i = 0; i <= maxthreads; i++) {
		thread_id_t tid = int_to_id(i);
		if (tid == read->get_tid())
			continue;
		if (tid == write->get_tid())
			continue;
		action_list_t *list = execution->get_actions_on_obj(read->get_location(), tid);
		if (list == NULL)
			continue;
		for (action_list_t::reverse_iterator rit = list->rbegin(); rit != list->rend(); rit++) {
			ModelAction *write2 = *rit;
			if (!write2->is_write())
				continue;

			ClockVector *write2cv = cvmap.get(write2);
			if (write2cv == NULL)
				continue;

			/* write -sc-> write2 &&
				 write -rf-> R =>
				 R -sc-> write2 */
			if (write2cv->synchronized_since(write)) {
				changed |= merge(write2cv, write2, read);
			}

			//looking for earliest write2 in iteration to satisfy this
			/* write2 -sc-> R &&
				 write -rf-> R =>
				 write2 -sc-> write */
			if (cv->synchronized_since(write2)) {
				changed |= writecv==NULL || merge(writecv, write, write2);
				break;
			}
		}
	}
	return changed;
}

void SCAnalysis::computeCV(action_list_t *list) {
	bool changed = true;
	bool firsttime = true;
	ModelAction **last_act = (ModelAction **)model_calloc(1, (maxthreads + 1) * sizeof(ModelAction *));
	while (changed) {
		changed = changed&firsttime;
		firsttime = false;

		for (action_list_t::iterator it = list->begin(); it != list->end(); it++) {
			ModelAction *act = *it;
			ModelAction *lastact = last_act[id_to_int(act->get_tid())];
			if (act->is_thread_start())
				lastact = execution->get_thread(act)->get_creation();
			last_act[id_to_int(act->get_tid())] = act;
			ClockVector *cv = cvmap.get(act);
			if (cv == NULL) {
				cv = new ClockVector(NULL, act);
				cvmap.put(act, cv);
			}
			if (lastact != NULL) {
				merge(cv, act, lastact);
			}
			if (act->is_thread_join()) {
				Thread *joinedthr = act->get_thread_operand();
				ModelAction *finish = execution->get_last_action(joinedthr->get_id());
				changed |= merge(cv, act, finish);
			}
			if (act->is_read()) {
				changed |= processRead(act, cv);
			}
		}
		/* Reset the last action array */
		if (changed) {
			bzero(last_act, (maxthreads + 1) * sizeof(ModelAction *));
		}
	}
	model_free(last_act);
}
