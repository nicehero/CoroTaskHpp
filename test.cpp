#include <iostream>
#include<thread>
#include<chrono>
#include <asio/asio.hpp>
#if !__has_include(<experimental/coroutine>)
#include <coroutine>
#else
#include <experimental/coroutine>
#endif


const int WORK_THREAD_COUNT = 1;
asio::io_context main_thread(1);
asio::io_context work_threads(WORK_THREAD_COUNT);
asio::signal_set signals(main_thread, SIGINT, SIGTERM);

void start()
{
	for (int i = 0; i < WORK_THREAD_COUNT; ++i)
	{
		std::thread t([] {
			std::cout << "work_thread=" << std::this_thread::get_id() << std::endl;
			asio::io_context::work work(work_threads);
			work_threads.run();
		});
		t.detach();
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	signals.async_wait([&](asio::error_code p1, auto p2) {
		auto s = p1.message();
		std::cout << s << std::endl;
		main_thread.stop();
		work_threads.stop();
	});
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
		, asio::io_context&	executer = main_thread
		//								返回线程
		, asio::io_context&	return_context = main_thread>
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

using namespace nicehero;
template<asio::io_context& execute = work_threads, asio::io_context& return_context = main_thread>
Task<int, execute, return_context> coro_add(int x, int y)
{
	std::cout << "do coro_add in thread_id=" << std::this_thread::get_id() << std::endl;
	int r = x + y;
	co_return r;
}
int main(int argc, char* argv[])
{
	try
	{
		std::cout << "main_thread=" << std::this_thread::get_id() << std::endl;
		start();
		main_thread.post([]()->Task<bool> {
			//-------主线程------
			std::cout << "step1: now thread_id=" << std::this_thread::get_id() << std::endl;
			//切换到工作线程执行coro_add
			int x = co_await coro_add<work_threads,work_threads>(1, 2);
			//返回后还处于此线程
			std::cout << "step2: now thread_id=" << std::this_thread::get_id() << std::endl;
			//切换到工作线程执行coro_add,完成后返回主线程
			int y = co_await coro_add(3, 4);
			//-------主线程------
			std::cout << "step3: now thread_id=" << std::this_thread::get_id() << std::endl;
			co_return true;
		});

		main_thread.post([]()->Task<bool> {
			return true;
		});

		asio::io_context::work work(main_thread);
		main_thread.run();
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}

	return 0;
}

/*
co_spawn(main_thread, listener(), asio::detached);

asio::awaitable<int> asio_add(int x, int y)
{
	int r = x + y + 100;
	co_return r;
}


asio::awaitable<int> listener()
{
	auto x = asio_add(3, 3);
	return co_await std::move(x);
}
*/