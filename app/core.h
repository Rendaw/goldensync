#ifndef core_h
#define core_h

#include "types.h"

typedef std::tuple<size_t, std::string> ChunkT;

struct DefineHeadT
{
	struct TruncateT {};
	VariantT<std::vector<ChunkT>, TruncateT> const &DataChanges;
	NodeMetaT MetaChanges;
};
struct DeleteHeadT {};

struct Core
{
	ListenersT<void(GlobalChangeIDT const &ChangeID, ChangeIDT const &ParentID)> ChangeAddListeners;
	ListenersT<GlobalChangeIDT> MissingAddListeners;
	ListenersT<GlobalChangeIDT> MissingRemoveListeners;
	ListenersT<GlobalChangeIDT> HeadAddListeners;
	ListenersT<GlobalChangeIDT> HeadRemoveListeners;

	CoreT(void);
	/*void AddInstance(std::string const &Name);
	InstanceIDT AddInstance(std::string const &Name);*/
	NodeIDT ReserveNode(void);
	ChangeIDT ReserveChange(void);
	void AddChange(GlobalChangeIDT const &ChangeID);
	void DefineChange(GlobalChangeIDT const &ChangeID, VariantT<DefineHeadT, DeleteHeadT> const &Definition);
	/*std::array<GlobalChangeIDT, PageSize> GetMissings(size_t Page);
	HeadT GetHead(GlobalChangeIDT const &HeadID);
	std::array<HeadT, PageSize> GetHeads(NodeIDT ParentID, OptionalT<InstanceIDT> Split);*/

	private:
		CoreDatabaseT Database;
		CoreTransactionsT Transact;
};

#endif

