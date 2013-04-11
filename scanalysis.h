#ifndef SCANALYSIS_H
#define SCANALYSIS_H
#include "traceanalysis.h"
#include "hashtable.h"

class SCAnalysis : public Trace_Analysis {
 public:
	SCAnalysis();
	~SCAnalysis();
	virtual void analyze(action_list_t *);

	SNAPSHOTALLOC
 private:
	void buildVectors(action_list_t *);
	void computeCV(action_list_t *);
	bool processRead(ModelAction *read, ClockVector *cv);
	int maxthreads;
	HashTable<const ModelAction *,ClockVector *, uintptr_t, 4 > * cvmap;
};
#endif
