#ifndef core_h
#define core_h

#include <list>

#include "../ren-cxx-basics/function.h"
#include "../ren-cxx-filesystem/path.h"

#include "types.h"
#include "structtypes.h"
#include "coredatabase.h"
#include "coretransactions.h"

template <typename SignatureT> struct NotifyT {};

template <typename ...ArgumentsT> struct NotifyT<void(ArgumentsT...)>
{
	struct TokenT
	{
		public:
			TokenT(TokenT &&Other);
			TokenT(TokenT const &) = delete;
			int &operator =(TokenT &&) = delete;
			int &operator =(TokenT const &) = delete;
			~TokenT(void);
		friend struct NotifyT<ArgumentsT...>;
		private:
			TokenT(uint64_t ID, NotifyT<ArgumentsT...> *Base);
			uint64_t ID;
			NotifyT<ArgumentsT...> *Base;
	};

	~NotifyT(void);
	TokenT Add(function<void(ArgumentsT...)> &&Function);
	void Notify(ArgumentsT ...Arguments);

	private:
		struct ListenerT
		{
			function<void(ArgumentsT...)> Callback;
			uint64_t ID;
			ListenerT(function<void(ArgumentsT...)> &&Callback, uint64_t ID);
		};
		std::list<ListenerT> Listeners;
		uint64_t IDCounter;
};

struct CoreT
{
	NotifyT<void(GlobalChangeIDT const &ChangeID, ChangeIDT const &ParentID)> ChangeAddListeners;
	NotifyT<void(GlobalChangeIDT const &)> MissingAddListeners;
	NotifyT<void(GlobalChangeIDT const &)> MissingRemoveListeners;
	NotifyT<void(GlobalChangeIDT const &)> HeadAddListeners;
	NotifyT<void(GlobalChangeIDT const &)> HeadRemoveListeners;

	CoreT(OptionalT<std::string> const &InstanceName, Filesystem::PathT const &Root);
	/*void AddInstance(std::string const &Name);
	InstanceIDT AddInstance(std::string const &Name);*/
	NodeIndexT ReserveNode(void);
	ChangeIndexT ReserveChange(void);
	void AddChange(GlobalChangeIDT const &ChangeID, ChangeIDT const &ParentID);
	void DefineChange(GlobalChangeIDT const &ChangeID, VariantT<DefineHeadT, DeleteHeadT> const &Definition);
	/*std::array<GlobalChangeIDT, PageSize> GetMissings(size_t Page);
	HeadT GetHead(GlobalChangeIDT const &HeadID);
	std::array<HeadT, PageSize> GetHeads(NodeIDT ParentID, OptionalT<InstanceIDT> Split);*/

	private:
		std::unique_ptr<CoreDatabaseT> Database;
		std::unique_ptr<CoreTransactionsT> Transact;

		void TransactAddChange(
			GlobalChangeIDT const &ChangeID,
			ChangeIDT const &ParentID, 
			OptionalT<ChangeIDT> const &HeadID,
			OptionalT<StorageIDT> const &StorageID, 
			OptionalT<size_t> const &StorageRefCount,
			bool const &DeleteMissing);
		void TransactUpdateDeleteHead(
			OptionalT<StorageIDT> const &StorageID, 
			OptionalT<size_t> const &StorageRefCount,
			GlobalChangeIDT const &ChangeID,
			OptionalT<ChangeIDT> const &DeleteParent,
			OptionalT<HeadT> const &NewHead,
			OptionalT<StorageIDT> const &NewStorageID,
			VariantT<std::vector<ChunkT>, TruncateT> const &DataChanges);
};

#endif

