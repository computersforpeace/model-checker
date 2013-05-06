#ifndef TRACE_ANALYSIS_H
#define TRACE_ANALYSIS_H
#include "model.h"

class TraceAnalysis {
 public:
	virtual void setExecution(ModelExecution * execution) = 0;
	virtual void analyze(action_list_t *) = 0;
	virtual char * name() = 0;
	virtual bool option(char *) = 0;

	SNAPSHOTALLOC
};
#endif
