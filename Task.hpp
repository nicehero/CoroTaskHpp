#ifndef ___NICE_CORO_TASK_HPP__
#define ___NICE_CORO_TASK_HPP__
#include<thread>
#include <mutex>
#include <condition_variable>

#if !defined(NICE_HAS_CO_AWAIT)
# if !defined(NICE_DISABLE_CO_AWAIT)
#  if defined(_MSC_VER)
#   if (_MSC_VER >= 1928) && (_MSVC_LANG >= 201705) && !defined(__clang__)
#    define NICE_HAS_CO_AWAIT 1
#   elif (_MSC_FULL_VER >= 190023506)
#    if defined(_RESUMABLE_FUNCTIONS_SUPPORTED)
#     define NICE_HAS_CO_AWAIT 1
#    endif // defined(_RESUMABLE_FUNCTIONS_SUPPORTED)
#   endif // (_MSC_FULL_VER >= 190023506)
#  elif defined(__clang__)
#   if (__cplusplus >= 201703) && (__cpp_coroutines >= 201703)
#    if __has_include(<experimental/coroutine>)
#     define NICE_HAS_CO_AWAIT 1
#    endif // __has_include(<experimental/coroutine>)
#   endif // (__cplusplus >= 201703) && (__cpp_coroutines >= 201703)
#  elif defined(__GNUC__)
#   if (__cplusplus >= 201709) && (__cpp_impl_coroutine >= 201902)
#    if __has_include(<coroutine>)
#     define NICE_HAS_CO_AWAIT 1
#    endif // __has_include(<coroutine>)
#   endif // (__cplusplus >= 201709) && (__cpp_impl_coroutine >= 201902)
#  endif // defined(__GNUC__)
# endif // !defined(ASIO_DISABLE_CO_AWAIT)
#endif // !defined(ASIO_HAS_CO_AWAIT)

#ifdef NICE_HAS_CO_AWAIT
#if !__has_include(<experimental/coroutine>)
#include <coroutine>
#else
#include <experimental/coroutine>
#endif
#endif

namespace asio{
	class io_context;
}

extern thread_local asio::io_context* g_current_thread;
namespace nicehero {
	class semaphore {
	public:
		semaphore(long count = 0) :count(count) {}
		void wait() {
			std::unique_lock<std::mutex>lock(mx);
			cond.wait(lock, [&]() {return count > 0; });
			--count;
	}
		void signal() {
			std::unique_lock<std::mutex>lock(mx);
			++count;
			cond.notify_one();
		}

	private:
		std::mutex mx;
		std::condition_variable cond;
		long count;
	};

#ifdef NICE_HAS_CO_AWAIT
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
	struct suspend_if {
		bool _Ready;
		explicit suspend_if(bool _Condition) noexcept : _Ready(!_Condition) {
		}
		bool await_ready() noexcept {
			return _Ready;
		}
		void await_suspend(coroutine_handle<>) noexcept {}
		void await_resume() noexcept {}
	};
	template <
		//								????????????
		typename				R
		//								????????????
		, asio::io_context&	executer
		//								????????????(???????????????????????????)
		, asio::io_context&	return_context = executer>
	struct Task {
		//////////-----------------------------
		struct promise_type {
			auto initial_suspend() {
				return suspend_if{ &executer != g_current_thread };
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
					if (m_task->first_context != &executer) {
						m_semaphore.wait();
					}
					m_task->ret = std::move(value);
					auto h = m_task->m_handle;
					if (!h) {
						return;
					}
					if (&executer == &return_context) {
						h.resume();
						return;
					}
					return_context.post([h]() {
						if (!h) {
							return;
						}
						h.resume();
					});
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
			semaphore m_semaphore;
		};
		bool await_ready() const {
			return false;
		}
		R await_resume() {
			return std::move(ret);
		}
		void await_suspend(coroutine_handle<> handle) {
			if (first_context == &executer && (&return_context == &executer)) {
				handle.resume();
				return;
			}
			if (first_context == &executer && (&return_context != &executer)) {
				m_handle2 = handle;
				return_context.post([this] {
					m_handle2.resume();
				});
				return;
			}
			m_handle = handle;
			if (m_promise) {
				m_promise->m_semaphore.signal();
			}
		}
		Task(promise_type* p) {
			m_promise = p;
			if (m_promise) {
				m_promise->m_task = this;
			}
			first_context = g_current_thread;
			if (first_context != &executer) {
				executer.post([p] {
					auto handle = coroutine_handle<promise_type>::from_promise(*p);
					handle.resume();
				});
			}
		}

		~Task() {
			if (m_promise) {
				m_promise->m_task = nullptr;
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
			coroutine_handle<> m_handle = nullptr;
			coroutine_handle<> m_handle2 = nullptr;
			asio::io_context* first_context = nullptr;
	};
#else
	template <typename R, asio::io_context& executer, asio::io_context& return_context = executer>
	struct Task
	{
		Task(R&& r) :ret(std::move(r)) {	}
		R ret;
	};

#endif
}

#endif

