# CoroTaskHpp 只需一个hpp文件，使用co_await随意灵活的切换任意工作线程

只需一个hpp你就可以写出这样的协程代码
```c++
MyTask<bool> myTask()
{
	//在工作线程work_thread中请求数据库，返回给主线程main_thread
	auto cursor = co_await MongoFind<work_thread,main_thread>(...);
	//在主线程中完成其他工作吧
	while (auto* o = cursor.fetch()){
		//.....
	}
	//send(...);
}
```

#此hpp只需要asio和c++20,当然也可以看我的hpp代码修改出你的版本

#need GNU 11 + 推荐使用WinLibs测试 https://winlibs.com/

#测试安装方法

cd dep

python build.py

cd ..

mkdir build

cd build

#cmake -G"your build tools" ..

#example mingw

#cmake -G"MinGW Makefiles" ..

make install

cd ..

./coro_test

#coro_test.exe

#gcc编译需要添加参数 -std=c++20 -fcoroutines

#vs2017编译需要添加参数 /await

超简单的使用方法 example:
```c++
#include <asio/asio.hpp>
#include "Task.hpp"

asio::io_context main_thread(1);//你的主线程io_context
asio::io_context work_threads(WORK_THREAD_COUNT);//你的工作线程io_context
using namespace nicehero;

const int WORK_THREAD_COUNT = 1;

//开始创建一个可以co_await的协程吧
//execute 为你执行时希望的线程，return_context 为你返回后回到的线程
template<asio::io_context& execute, asio::io_context& return_context>
Task<int, execute, return_context> coro_add(int x, int y)//一个返回int的模板协程
{
	//在你希望的线程中执行任务吧
	std::cout << "do coro_add in thread_id=" << std::this_thread::get_id() << std::endl;
	int r = x + y;
	co_return r;
}

typedef Task<bool,main_thread> MyTask;
int main()
{
	//do 一些初始化逻辑
	
	//创建一个主线程任务 (不需post,协程开始后会自动往相应的线程post出去)
	auto f = []()->MyTask {
		//-------主线程开始------
		std::cout << "step1: now thread_id=" << std::this_thread::get_id() << std::endl;
		//切换到工作线程执行coro_add,返回后不切换线程
		int x = co_await coro_add<work_threads, work_threads>(1, 2);
		//返回后还处于此线程
		std::cout << "step2: now thread_id=" << std::this_thread::get_id() << std::endl;
		//切换到工作线程执行coro_add,完成后返回主线程 (使用coro_add的默认模板参数)
		int y = co_await coro_add(3, 4);
		//-------回到主线程------
		std::cout << "step3: now thread_id=" << std::this_thread::get_id() << std::endl;
		co_return true;
	};
	//执行协程任务
	f();
	
	//如果任务中没有co_await,co_return,Task也可以作为一个普通函数使用
	main_thread.post([]()->Task<bool> {
		return true;
	});

	//do main_thread.run();等内容
	
	return 0;
}

```