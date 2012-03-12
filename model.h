#ifndef __MODEL_H__
#define __MODEL_H__

struct model_checker {
	struct scheduler *scheduler;
};

extern struct model_checker *model;

void model_checker_init(void);

#endif /* __MODEL_H__ */
