#ifndef core_h
#define core_h

#include <list>

#include "../ren-cxx-basics/function.h"
#include "../ren-cxx-filesystem/path.h"

#include "types.h"
#include "structtypes.h"
#include "coredatabase.h"
#include "coretransactions.h"
#include "log.h"

template <typename SignatureT> struct NotifyT {};

template <typename ...ArgumentsT> struct NotifyT<void(ArgumentsT...)>
{
	struct TokenT
	{
		public:
			TokenT(TokenT &&Other) :
				ID(Other.ID), Base(Other.Base) 
				{ Other.ID = 0; Other.Base = nullptr; }
			TokenT(TokenT const &) = delete;
			int &operator =(TokenT &&) = delete;
			int &operator =(TokenT const &) = delete;

			~TokenT(void)
			{
				if (!Base) return;
				auto Found = std::find_if(
					Base->Listeners.begin(), 
					Base->Listeners.end(),
					[this](ListenerT const &Listener) { return Listener.ID == ID; });
				Assert(Found != Base->Listeners.end());
				Base->Listeners.erase(Found);
			}

		friend struct NotifyT<ArgumentsT...>;
		private:
			TokenT(uint64_t ID, NotifyT<ArgumentsT...> *Base) :
				ID(ID), Base(Base) 
				{}

			uint64_t ID;
			NotifyT<ArgumentsT...> *Base;
	};

	~NotifyT(void)
	{
		AssertE(Listeners.size(), 0u);
	}

	TokenT Add(function<void(ArgumentsT...)> &&Function)
	{
		auto ID = IDCounter++;
		Listeners.emplace_back(std::move(Function), ID);
		return {ID, this};
	}

	void Notify(ArgumentsT ...Arguments)
	{
		for (auto &Listener : Listeners) 
			Listener.Callback(std::forward<ArgumentsT>(Arguments)...);
	}

	private:
		struct ListenerT
		{
			function<void(ArgumentsT...)> Callback;
			uint64_t ID;

			ListenerT(function<void(ArgumentsT...)> &&Callback, uint64_t ID) :
				Callback(std::move(Callback)), ID(ID)
				{}
		};
		std::list<ListenerT> Listeners;
		uint64_t IDCounter;
};

struct CoreT
{
	NotifyT<void(ChangeT const &Change)> ChangeAddListeners;
	NotifyT<void(GlobalChangeIDT const &)> MissingAddListeners;
	NotifyT<void(GlobalChangeIDT const &)> MissingRemoveListeners;
	NotifyT<void(GlobalChangeIDT const &)> HeadAddListeners;
	NotifyT<void(GlobalChangeIDT const &)> HeadRemoveListeners;

	CoreT(OptionalT<std::string> const &InstanceName, Filesystem::PathT const &Root);

	/*void AddInstance(std::string const &Name);
	InstanceIDT AddInstance(std::string const &Name);*/
	NodeIndexT ReserveNode(void);
	ChangeIndexT ReserveChange(void);
	void AddChange(ChangeT const &Change);
	void DefineChange(GlobalChangeIDT const &ChangeID, VariantT<DefineHeadT, DeleteHeadT> const &Definition);
	/*std::array<GlobalChangeIDT, PageSize> GetMissings(size_t Page);
	std::array<HeadT, PageSize> GetHeads(NodeIDT ParentID, OptionalT<InstanceIDT> Split);*/

	// Should be private, but I don't want to commit to a type name
	void Handle(
		CTV1AddChange,
		ChangeT const &Change,
		OptionalT<ChangeIDT> const &HeadID,
		OptionalT<StorageIDT> const &StorageID, 
		OptionalT<size_t> const &StorageRefCount,
		bool const &DeleteMissing);
	void Handle(
		CTV1UpdateDeleteHead,
		OptionalT<StorageIDT> const &StorageID, 
		OptionalT<size_t> const &StorageRefCount,
		GlobalChangeIDT const &ChangeID,
		OptionalT<ChangeIDT> const &DeleteParent,
		OptionalT<HeadT> const &NewHead,
		StorageChangesT const &DataChanges);

	InstanceIndexT GetThisInstance(void) const;
	std::vector<ChangeT> ListChanges(size_t Start, size_t Count);
	std::vector<MissingT> ListMissing(size_t Start, size_t Count);
	std::vector<HeadT> ListHeads(size_t Start, size_t Count);
	std::vector<HeadT> ListDirHeads(OptionalT<NodeIDT> const &Dir, size_t Start, size_t Count);
	std::vector<StorageT> ListStorage(size_t Start, size_t Count);
	
	OptionalT<HeadT> GetHead(GlobalChangeIDT const &HeadID);

	Filesystem::FileT Open(StorageIDT const &Storage);

	private:
		Filesystem::PathT const Root;
		Filesystem::PathT const StorageRoot;
		BasicLogT Log;
		std::unique_ptr<CoreDatabaseT> Database;
		typedef TransactorT<
				CoreT,
				CTV1AddChange,
				CTV1UpdateDeleteHead> CoreTransactorT;
		std::unique_ptr<CoreTransactorT> Transact;

		InstanceIndexT ThisInstance;

		Filesystem::PathT GetStoragePath(StorageIDT const &StorageID);
};

#endif

