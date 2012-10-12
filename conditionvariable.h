#ifndef CONDITIONVARIABLE_H
#define CONDITIONVARIABLE_H

namespace std {
	class mutex;

	struct condition_variable_state {
		int reserved;
	};

	class condition_variable {
	public:
		condition_variable();
		~condition_variable();
		void notify_one();
		void notify_all();
		void wait(mutex& lock);
		
	private:
		struct condition_variable_state state;
	};
}
#endif
