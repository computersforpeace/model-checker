#ifndef SCANALYSIS_H
#define SCANALYSIS_H
#include "traceanalysis.h"

class SCAnalysis : public Trace_Analysis {
 public:
	SCAnalysis();
	virtual void analyze(action_list_t *);

};
#endif
