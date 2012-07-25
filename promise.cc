#include "promise.h"

Promise::Promise(ModelAction *act, uint64_t value) {
	this->value=value;
	this->read=act;
	this->numthreads=1;
}

