#ifndef TRACE_ANALYSIS_H
#define TRACE_ANALYSIS_H
#include "model.h"

class TraceAnalysis {
 public:
	virtual void analyze(action_list_t *) = 0;
	SNAPSHOTALLOC
};
#endif
