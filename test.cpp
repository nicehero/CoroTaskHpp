#include <iostream>
#include<thread>
#include<chrono>
#include <asio/asio.hpp>

const int WORK_THREAD_COUNT = 1;
asio::io_context main_thread(1);
asio::io_context work_threads(WORK_THREAD_COUNT);
asio::signal_set signals(main_thread, SIGINT, SIGTERM);

#ifdef WIN32
static BOOL WINAPI handleWin32Console(DWORD event)
{
	switch (event)
	{
	case CTRL_CLOSE_EVENT:
	case CTRL_C_EVENT:
	{
		printf("handle end\n");
		main_thread.stop();
		work_threads.stop();
	}

	return TRUE;
	}
	return FALSE;
}
#endif

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
#ifdef WIN32
	if (!SetConsoleCtrlHandler(handleWin32Console, TRUE))
	{
		fprintf(stderr, "error setting event handler.\n");
	}
#endif
}

#include "Task.hpp"
using namespace nicehero;

template<asio::io_context& execute = work_threads, asio::io_context& return_context = main_thread>
Task<int, execute, return_context> coro_add(int x, int y)
{
	std::cout << "do coro_add in thread_id=" << std::this_thread::get_id() << std::endl;
	int r = x + y;
	co_return r;
}

typedef Task<bool,main_thread> MyTask;
int main(int argc, char* argv[])
{
#if defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	try
	{
		std::cout << "main_thread=" << std::this_thread::get_id() << std::endl;
		start();
		//����һ�����߳�����f (����post,Э�̿�ʼ����Զ�����Ӧ���߳�post��ȥ)
		auto f = []()->MyTask {
			//-------���߳̿�ʼ------			std::cout << "step1: now thread_id=" << std::this_thread::get_id() << std::endl;
			//�л��������߳�ִ��coro_add
			int x = co_await coro_add<work_threads, work_threads>(1, 2);
			//���غ󻹴��ڴ��߳�
			std::cout << "step2: now thread_id=" << std::this_thread::get_id() << std::endl;
			//�л��������߳�ִ��coro_add,��ɺ󷵻����߳� (ʹ��coro_add��Ĭ��ģ�����)
			int y = co_await coro_add(3, 4);
			//-------�ص����߳�------			std::cout << "step3: now thread_id=" << std::this_thread::get_id() << std::endl;
			co_return true;
		};
		//ִ��Э������f
		f();
		//���������û��co_await,co_return,TaskҲ������Ϊһ����ͨ����ʹ��
		main_thread.post([]()->MyTask {
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