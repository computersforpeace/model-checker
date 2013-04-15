#ifndef SCANALYSIS_H
#define SCANALYSIS_H
#include "traceanalysis.h"
#include "hashtable.h"

class SCAnalysis : public TraceAnalysis {
 public:
	SCAnalysis();
	~SCAnalysis();
	virtual void analyze(action_list_t *);

	SNAPSHOTALLOC
 private:
	void print_list(action_list_t *list);
	void buildVectors(action_list_t *);
	bool updateConstraints(ModelAction *act);
	void computeCV(action_list_t *);
	action_list_t * generateSC(action_list_t *);
	bool processRead(ModelAction *read, ClockVector *cv);
	ModelAction * getNextAction();
	bool merge(ClockVector * cv, const ModelAction * act, ClockVector *cv2);
	int maxthreads;
	HashTable<const ModelAction *,ClockVector *, uintptr_t, 4 > * cvmap;
	HashTable<const ModelAction *,const ModelAction *, uintptr_t, 4 > * cycleset;
	SnapVector<action_list_t> * threadlists;
};
#endif
