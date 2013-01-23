#include "promise.h"
#include "model.h"
#include "schedule.h"

bool Promise::increment_threads(thread_id_t tid)
{
	unsigned int id = id_to_int(tid);
	if (id >= synced_thread.size())
		synced_thread.resize(id + 1, false);
	if (synced_thread[id])
		return false;

	synced_thread[id] = true;
	unsigned int sync_size = synced_thread.size();
	int promise_tid = id_to_int(read->get_tid());
	for (unsigned int i = 1; i < model->get_num_threads(); i++) {
		if ((i >= sync_size || !synced_thread[i]) && ((int)i != promise_tid) && model->is_enabled(int_to_id(i))) {
			return false;
		}
	}
	return true;
}

bool Promise::check_promise() const
{
	unsigned int sync_size = synced_thread.size();
	for (unsigned int i = 1; i < model->get_num_threads(); i++) {
		if ((i >= sync_size || !synced_thread[i]) && model->is_enabled(int_to_id(i))) {
			return false;
		}
	}
	return true;
}
