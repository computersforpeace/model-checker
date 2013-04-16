#include "scanalysis.h"
#include "action.h"
#include "threads-model.h"
#include "clockvector.h"
#include "execution.h"

SCAnalysis::SCAnalysis(const ModelExecution *execution) :
	execution(execution)
{
	cvmap=new HashTable<const ModelAction *, ClockVector *, uintptr_t, 4>();
	cycleset=new HashTable<const ModelAction *, const ModelAction *, uintptr_t, 4>();
	threadlists=new SnapVector<action_list_t>(1);
}

SCAnalysis::~SCAnalysis() {
	delete cvmap;
	delete cycleset;
	delete threadlists;
}

void SCAnalysis::print_list(action_list_t *list) {
	action_list_t::iterator it;

	model_print("---------------------------------------------------------------------\n");

	unsigned int hash = 0;

	for (it = list->begin(); it != list->end(); it++) {
		const ModelAction *act = *it;
		if (act->get_seq_number() > 0) {
			if (cycleset->contains(act))
				model_print("CYC");
			act->print();
		}
		hash = hash^(hash<<3)^((*it)->hash());
	}
	model_print("HASH %u\n", hash);
	model_print("---------------------------------------------------------------------\n");
}

void SCAnalysis::analyze(action_list_t * actions) {
 	buildVectors(actions);
	computeCV(actions);
	action_list_t *list=generateSC(actions);
	print_list(list);
}

bool SCAnalysis::merge(ClockVector * cv, const ModelAction * act, ClockVector *cv2) {
	if (cv2->getClock(act->get_tid())>=act->get_seq_number() && act->get_seq_number() != 0) {
		cycleset->put(act, act);
	}
	return cv->merge(cv2);
}

ModelAction * SCAnalysis::getNextAction() {
	ModelAction *act=NULL;
	for(int i=0;i<=maxthreads;i++) {
		action_list_t * threadlist=&(*threadlists)[i];
		if (threadlist->empty())
			continue;
		ModelAction *first=threadlist->front();
		if (act==NULL) {
			act=first;
			continue;
		}
		ClockVector *cv=cvmap->get(act);
		if (cv->synchronized_since(first)) {
			act=first;
		}
	}
	if (act==NULL)
		return act;
	//print cycles in a nice way to avoid confusion
	//make sure thread starts appear after the create
	if (act->is_thread_start()) {
		ModelAction *createact=execution->get_thread(act)->get_creation();
		if (createact) {
			action_list_t *threadlist=&(*threadlists)[id_to_int(createact->get_tid())];
			if (!threadlist->empty()) {
				ModelAction *first=threadlist->front();
				if (first->get_seq_number() <= createact->get_seq_number())
					act=first;
			}
		}
	}

	//make sure that joins appear after the thread is finished
	if (act->is_thread_join()) {
		int jointhread=id_to_int(act->get_thread_operand()->get_id());
		action_list_t *threadlist=&(*threadlists)[jointhread];
		if (!threadlist->empty()) {
			act=threadlist->front();
		}
	}

	return act;
}

action_list_t * SCAnalysis::generateSC(action_list_t *list) {
	action_list_t *sclist=new action_list_t();
	while (true) {
		ModelAction * act=getNextAction();
		if (act==NULL)
			break;
		thread_id_t tid=act->get_tid();
		//remove action
		(*threadlists)[id_to_int(tid)].pop_front();
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
	maxthreads=0;
	for (action_list_t::iterator it = list->begin(); it != list->end(); it++) {
		ModelAction *act = *it;
		int threadid=id_to_int(act->get_tid());
		if (threadid > maxthreads) {
			threadlists->resize(threadid+1);
			maxthreads=threadid;
		}
		(*threadlists)[threadid].push_back(act);
	}
}

bool SCAnalysis::updateConstraints(ModelAction *act) {
	bool changed=false;
	ClockVector *actcv = cvmap->get(act);
	for(int i=0;i<=maxthreads;i++) {
		thread_id_t tid=int_to_id(i);
		if (tid==act->get_tid())
			continue;

		action_list_t * list=&(*threadlists)[id_to_int(tid)];
		for (action_list_t::iterator rit = list->begin(); rit != list->end(); rit++) {
			ModelAction *write = *rit;
			if (!write->is_write())
				continue;
			ClockVector *writecv = cvmap->get(write);
			if (writecv->synchronized_since(act))
				break;
			if (write->get_location() == act->get_location()) {
				//write is sc after act
				merge(writecv, write, actcv);
				changed=true;
				break;
			}
		}
	}
	return changed;
}

bool SCAnalysis::processRead(ModelAction *read, ClockVector *cv) {
	bool changed=false;

	/* Merge in the clock vector from the write */
	const ModelAction *write=read->get_reads_from();
	ClockVector *writecv=cvmap->get(write);
	changed |= writecv == NULL || (merge(cv, read, writecv) && (*read < *write));

	for(int i=0;i<=maxthreads;i++) {
		thread_id_t tid=int_to_id(i);
		if (tid==read->get_tid())
			continue;
		if (tid==write->get_tid())
			continue;
		action_list_t * list=execution->get_actions_on_obj(read->get_location(), tid);
		if (list==NULL)
			continue;
		for (action_list_t::reverse_iterator rit = list->rbegin(); rit != list->rend(); rit++) {
			ModelAction *write2 = *rit;
			if (!write2->is_write())
				continue;

			ClockVector *write2cv = cvmap->get(write2);
			if (write2cv == NULL)
				continue;

			/* write -sc-> write2 &&
				 write -rf-> R =>
				 R -sc-> write2 */
			if (write2cv->synchronized_since(write)) {
				changed |= merge(write2cv, write2, cv);
			}

			//looking for earliest write2 in iteration to satisfy this
			/* write2 -sc-> R &&
				 write -rf-> R =>
				 write2 -sc-> write */
			if (cv->synchronized_since(write2)) {
				changed |= writecv == NULL || merge(writecv, write, write2cv);
				break;
			}
		}
	}
	return changed;
}


void SCAnalysis::computeCV(action_list_t *list) {
	bool changed=true;
	bool firsttime=true;
	ModelAction **last_act=(ModelAction **)model_calloc(1,(maxthreads+1)*sizeof(ModelAction *));
	while(changed) {
		changed=changed&firsttime;
		firsttime=false;

		for (action_list_t::iterator it = list->begin(); it != list->end(); it++) {
			ModelAction *act = *it;
			ModelAction *lastact = last_act[id_to_int(act->get_tid())];
			if (act->is_thread_start())
				lastact=execution->get_thread(act)->get_creation();
			ClockVector *lastcv=(lastact != NULL) ? cvmap->get(lastact) : NULL;
			last_act[id_to_int(act->get_tid())]=act;
			ClockVector *cv=cvmap->get(act);
			if ( cv == NULL ) {
				cv = new ClockVector(lastcv, act);
				cvmap->put(act, cv);
			} else if ( lastcv != NULL ) {
				merge(cv, act, lastcv);
			}
			if (act->is_thread_join()) {
				Thread *joinedthr = act->get_thread_operand();
				ModelAction *finish = execution->get_last_action(joinedthr->get_id());
				ClockVector *finishcv = cvmap->get(finish);
				changed |= (finishcv == NULL) || merge(cv, act, finishcv);
			}
			if (act->is_read()) {
				changed|=processRead(act, cv);
			}
		}
		/* Reset the last action array */
		if (changed) {
			bzero(last_act, (maxthreads+1)*sizeof(ModelAction *));
		}
	}
	model_free(last_act);
}
