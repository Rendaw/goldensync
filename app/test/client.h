#include <future>

#include <luxem-cxx/luxem.h>

#include "../../ren-cxx-basics/error.h"
#include "../asio_utils.h"

struct lock_guard
{
	lock_guard(std::mutex &mutex) : locked(true), mutex(mutex)
	{
		mutex.lock();
	}

	void unlock(void)
	{
		if (locked)
		{
			locked = false;
			mutex.unlock();
		}
	}

	~lock_guard(void)
	{
		unlock();
	}

	private:
		bool locked;
		std::mutex &mutex;
};

struct ClunkerControlT
{
	typedef function<void(bool Success)> CleanCallbackT;
	void Clean(CleanCallbackT &&Callback)
	{
		Write(Connection, 
			luxem::writer()
				.type("clean")
				.value("")
				.dump());
		{
			std::lock_guard<std::mutex> Guard(Mutex);
			CleanCallbacks.emplace_back(std::move(Callback));
		}
	}

	std::future<bool> Clean(void)
	{
		std::promise<bool> Promise;
		auto Future = Promise.get_future();
		Clean([Promise = std::move(Promise)](bool Success) mutable { Promise.set_value(Success); });
		return std::move(Future);
	}
		
	typedef function<void(int64_t)> GetOpCountCallbackT;
	void GetOpCount(GetOpCountCallbackT &&Callback)
	{
		Write(Connection, 
			luxem::writer()
				.type("get_count")
				.value("")
				.dump());
		{
			std::lock_guard<std::mutex> Guard(Mutex);
			GetOpCountCallbacks.push_back(std::move(Callback));
		}
	}

	std::future<int64_t> GetOpCount(void)
	{
		std::promise<int64_t> Promise;
		auto Future = Promise.get_future();
		GetOpCount([Promise = std::move(Promise)](int64_t Count) mutable { Promise.set_value(Count); });
		return std::move(Future);
	}

	typedef function<void(bool Success)> SetOpCountCallbackT;
	void SetOpCount(int64_t Count, SetOpCountCallbackT &&Callback)
	{
		Write(Connection, 
			luxem::writer()
				.type("set_count")
				.value(Count)
				.dump());
		{
			std::lock_guard<std::mutex> Guard(Mutex);
			SetOpCountCallbacks.push_back(std::move(Callback));
		}
	}
	
	std::future<bool> SetOpCount(int64_t Count)
	{
		std::promise<bool> Promise;
		auto Future = Promise.get_future();
		SetOpCount(
			Count, 
			[Promise = std::move(Promise)](bool Success) mutable { Promise.set_value(Success); });
		return std::move(Future);
	}

	friend void ConnectClunker(
		asio::io_service &Service, 
		asio::ip::tcp::endpoint const &Endpoint, 
		function<void(std::shared_ptr<ClunkerControlT> Control)> &&Callback);
	private:
		std::mutex Mutex;
		std::shared_ptr<asio::ip::tcp::socket> Connection;
				
		std::list<CleanCallbackT> CleanCallbacks;
		std::list<GetOpCountCallbackT> GetOpCountCallbacks;
		std::list<SetOpCountCallbackT> SetOpCountCallbacks;
};

void ConnectClunker(
	asio::io_service &Service, 
	asio::ip::tcp::endpoint const &Endpoint, 
	function<void(std::shared_ptr<ClunkerControlT> Control)> &&Callback)
{
	TCPConnect(
		Service, 
		Endpoint, 
		[Callback = std::move(Callback)](std::shared_ptr<asio::ip::tcp::socket> Connection)
		{
			auto Control = std::make_shared<ClunkerControlT>();
			Control->Connection = Connection;

			auto Reader = std::make_shared<luxem::reader>();
			Reader->element([Control, Connection](std::shared_ptr<luxem::value> &&Data)
			{
				if (!Data->has_type()) 
				{
					std::cerr << "Message has no type: [" << luxem::writer().value(Data).dump() << "]";
					return;
				}

				auto Type = Data->get_type();
				if (Type == "clean_result")
				{
					
					lock_guard Guard(Control->Mutex);
					AssertGT(Control->CleanCallbacks.size(), 0u);
					auto Callback = std::move(Control->CleanCallbacks.front());
					Control->CleanCallbacks.pop_front();
					Guard.unlock();

					Callback(Data->as<luxem::primitive>().get_bool());
				}
				else if (Type == "set_result")
				{
					lock_guard Guard(Control->Mutex);
					AssertGT(Control->SetOpCountCallbacks.size(), 0u);
					auto Callback = std::move(Control->SetOpCountCallbacks.front());
					Control->SetOpCountCallbacks.pop_front();
					Guard.unlock();

					Callback(Data->as<luxem::primitive>().get_bool());
				}
				else if (Type == "count")
				{
					lock_guard Guard(Control->Mutex);
					AssertGT(Control->GetOpCountCallbacks.size(), 0u);
					auto Callback = std::move(Control->GetOpCountCallbacks.front());
					Control->GetOpCountCallbacks.pop_front();
					Guard.unlock();

					Callback(Data->as<luxem::primitive>().get_int());
				}
				else
				{
					throw SystemErrorT() << "Unknown message type [" << Type << "]";
				}
			});

			LoopRead(std::move(Connection), [Reader](ReadBufferT &Buffer)
			{
				auto Consumed = Reader->feed((char const *)Buffer.FilledStart(), Buffer.Filled(), false);
				Buffer.Consume(Consumed);
				return true;
			});

			Callback(std::move(Control));
		});
}

