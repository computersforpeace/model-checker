#include "scanalysis.h"
#include "action.h"
#include "threads-model.h"
#include "clockvector.h"

SCAnalysis::SCAnalysis() {
	cvmap=new HashTable<const ModelAction *, ClockVector *, uintptr_t, 4>();
}

SCAnalysis::~SCAnalysis() {
	delete(cvmap);
}

void SCAnalysis::analyze(action_list_t * actions) {
 	buildVectors(actions);
	computeCV(actions);
}

void SCAnalysis::buildVectors(action_list_t *list) {
	maxthreads=0;
	for (action_list_t::iterator it = list->begin(); it != list->end(); it++) {
		ModelAction *act = *it;
		int threadid=id_to_int(act->get_tid());
		if (threadid > maxthreads)
			maxthreads=threadid;
	}
}

bool SCAnalysis::processRead(ModelAction *read, ClockVector *cv) {
	bool changed=false;

	/* Merge in the clock vector from the write */
	const ModelAction *write=read->get_reads_from();
	ClockVector *writecv=cvmap->get(write);
	changed|= ( writecv == NULL || cv->merge(writecv) && (*read < *write));

	for(int i=0;i<=maxthreads;i++) {
		thread_id_t tid=int_to_id(i);
		if (tid==read->get_tid())
			continue;
		action_list_t * list=model->get_actions_on_obj(read->get_location(), tid);
		if (list==NULL)
			continue;
		for (action_list_t::reverse_iterator rit = list->rbegin(); rit != list->rend(); rit++) {
			ModelAction *write2 = *rit;
			ClockVector *write2cv = cvmap->get(write2);
			if (write2cv == NULL)
				continue;
			
			/* write -sc-> write2 &&
				 write -rf-> R =>
				 R -sc-> write2 */
			if (write2cv->synchronized_since(write)) {
				changed |= write2cv->merge(cv);
			}
		
			//looking for earliest write2 in iteration to satisfy this
			/* write2 -sc-> R &&
				 write -rf-> R =>
				 write2 -sc-> write */
			if (cv->synchronized_since(write2)) {
				changed |= writecv == NULL || writecv->merge(write2cv);
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
				lastact=model->get_thread(act)->get_creation();
			ClockVector *lastcv=(lastact != NULL) ? cvmap->get(lastact) : NULL;
			last_act[id_to_int(act->get_tid())]=act;
			ClockVector *cv=cvmap->get(act);
			if ( cv == NULL ) {
				cv = new ClockVector(lastcv, act);
				cvmap->put(act, cv);
			} else if ( lastcv != NULL ) {
					cv->merge(lastcv);
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
