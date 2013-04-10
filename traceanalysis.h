#ifndef TRACE_ANALYSIS_H
#define TRACE_ANALYSIS_H
#include "model.h"

class Trace_Analysis {
 public:
	virtual void analyze(action_list_t *);
};
#endif
