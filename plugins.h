#ifndef PLUGINS_H
#define PLUGINS_H
#include "traceanalysis.h"
#include "stl-model.h"

void register_plugins();
ModelVector<TraceAnalysis *> * getRegisteredTraceAnalysis();
ModelVector<TraceAnalysis *> * getInstalledTraceAnalysis();

#endif
