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
		TransactionPath(TransactionPath),
		Handler(Handler)
	{
		TransactionPath.CreateDirectory();
		TransactionPath.List([&](Filesystem::PathT &&Path, bool IsFile, bool IsDir)
		{
			auto In = Filesystem::FileT::OpenRead(Path);
			ReadBufferT Buffer;
			while (In.Read(Buffer))
			{
				if (!Reader.Read(Buffer, Handler))
					throw SystemErrorT() << "Could not read transaction file " << Path << ", file may be corrupt.";
			}
			Path.Delete();
			return true;
		});
	}

	template <typename MessageT, typename ...ArgumentTypes> 
		void Act(ArgumentTypes const &... Arguments)
	{
		static_assert(
			TypeIn<MessageT, MessagesT...>::Value, 
			"MessageT is unregistered.  Type must be registered with callback in constructor.");
		Filesystem::PathT ThreadPath = TransactionPath.Enter(StringT() << std::this_thread::get_id());
		Filesystem::FileT::OpenWrite(ThreadPath).Write(MessageT::Write(Arguments...));
		Handler.Handle(MessageT(), std::forward<ArgumentTypes const &>(Arguments)...);
		ThreadPath.Delete();
	}

	template <typename MessageT, typename ...ArgumentTypes> 
		void operator()(MessageT, ArgumentTypes const &... Arguments)
	{ 
		Act<MessageT>(std::forward<ArgumentTypes const &>(Arguments)...); 
	}

	private:
		Filesystem::PathT const TransactionPath;
		ProtoHandlerT &Handler;
		Protocol::ReaderT<MessagesT...> Reader;
};

#endif

