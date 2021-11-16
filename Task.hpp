#ifndef ___NICE_CORO_TASK_HPP__
#define ___NICE_CORO_TASK_HPP__
#include<thread>
namespace asio{
	class io_context;
}
namespace nicehero {

#if !__has_include(<experimental/coroutine>)
#define STDCORO std
	using std::coroutine_handle;
	using std::suspend_always;
	using std::suspend_never;
#else
#define STDCORO std::experimental
	using std::experimental::coroutine_handle;
	using std::experimental::suspend_always;
	using std::experimental::suspend_never;
#endif

	template <
		//								返回类型
		typename				R 
		//								执行线程
		, asio::io_context&	executer
		//								返回线程
		, asio::io_context&	return_context>
	struct Task {
		//////////-----------------------------
		struct promise_type {
			auto initial_suspend() {
				return suspend_always{};
			}
			auto final_suspend() noexcept {
				return suspend_never{};
			}
			auto get_return_object() {
				return Task<R, executer, return_context>(this);
			}
			void unhandled_exception() {
				std::terminate();
			}
			//void return_void() {}
			template<typename U>
			void return_value(U&& value) {
				if (m_task) {
					m_task->ret = std::move(value);
				}
			}
			promise_type() {
				m_task = nullptr;
			}
			~promise_type() {
				if (m_task) {
					m_task->m_promise = nullptr;
				}
			}
			friend struct Task<R, executer, return_context>;
		protected:
			Task* m_task = nullptr;
		};
		bool await_ready() const {
			return false;
		}
		R await_resume() {
			return std::move(ret);
		}
		void await_suspend(coroutine_handle<> handle) {
			executer.post([handle, this]() {
				if (&executer == &return_context) {
					handle.resume();
					return;
				}
				return_context.post([handle, this]() {
					handle.resume();
				});
			});
		}
		Task(promise_type* p) {
			m_promise = p;
			if (m_promise) {
				m_promise->m_task = this;
			}
			executer.post([p] {
				auto handle = coroutine_handle<promise_type>::from_promise(*p);
				handle.resume();
			});
		}

		~Task() {
			if (m_promise) {
				m_promise = nullptr;
			}
		}
		Task(R&& r) :ret(r) {
			hasRet = true;
		}
		friend struct promise_type;
	protected:
		promise_type* m_promise = nullptr;
		R ret;
		bool hasRet = false;
	};
}

#endif

