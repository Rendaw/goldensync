#ifndef transaction_h
#define transaction_h

#include "protocol.h"
#include "../../ren-cxx-filesystem/path.h"
#include "../../ren-cxx-filesystem/file.h"

#include <mutex>
#include <thread>

template <typename Type, typename ...TypeSet> 
	struct TypeIn;

template <typename Type, typename ...RemainingTypes> 
	struct TypeIn<Type, Type, RemainingTypes...>
{ 
	static constexpr bool Value = true; 
};

template <typename Type> 
	struct TypeIn<Type>
{ 
	static constexpr bool Value = false; 
};

template <typename Type, typename OtherType, typename ...RemainingTypes> 
	struct TypeIn<Type, OtherType, RemainingTypes...> : TypeIn<Type, RemainingTypes...> 
{
};

template <typename ProtoHandlerT, typename ...MessagesT> struct TransactorT
{
	TransactorT(Filesystem::PathT const &TransactionPath, ProtoHandlerT &Handler) : 
		Log("transactor"),
		TransactionPath(TransactionPath),
		Handler(Handler)
	{
		LOG(Log, Debug, (StringT() << "Replaying transactions."));
		TransactionPath.CreateDirectory();
		TransactionPath.List([&](Filesystem::PathT &&Path, bool IsFile, bool IsDir)
		{
			LOG(Log, Info, (StringT() << "Recovering " << Path));
			auto In = Filesystem::FileT::OpenRead(Path);
			ReadBufferT Buffer;
			while (In.Read(Buffer))
			{
				try
				{
					Reader.Read(Buffer, Handler);
				}
				catch (SystemErrorT const &Error)
				{ 
					LOG(
						Log, 
						Info, 
						StringT() << "Error reading transaction file " << Path << ": " << Error); 
				}
			}
			Path.Delete();
			return true;
		});
		LOG(Log, Debug, (StringT() << "Done replaying transactions."));
	}

	template <typename MessageT, typename ...ArgumentTypes> 
		void Act(ArgumentTypes const &... Arguments)
	{
		static_assert(
			TypeIn<MessageT, MessagesT...>::Value, 
			"MessageT is unregistered.  Type must be registered with callback in constructor.");
		Filesystem::PathT ThreadPath = TransactionPath.Enter(StringT() << std::this_thread::get_id());
		LOG(Log, Info, (StringT() << "Starting transaction " << ThreadPath));
		Filesystem::FileT::OpenWrite(ThreadPath).Write(
			MessageT::Write(std::forward<ArgumentTypes const &>(Arguments)...));
		LOG(Log, Info, (StringT() << "Wrote transaction " << ThreadPath));
		Handler.Handle(
			MessageT(), 
			std::forward<ArgumentTypes const &>(Arguments)...);
		LOG(Log, Info, (StringT() << "Ending transaction " << ThreadPath));
		ThreadPath.Delete();
	}

	template <typename MessageT, typename ...ArgumentTypes> 
		void operator()(MessageT, ArgumentTypes const &... Arguments)
	{ 
		Act<MessageT>(std::forward<ArgumentTypes const &>(Arguments)...); 
	}

	private:
		BasicLogT Log;
		Filesystem::PathT const TransactionPath;
		ProtoHandlerT &Handler;
		Protocol::ReaderT<MessagesT...> Reader;
};

#endif

