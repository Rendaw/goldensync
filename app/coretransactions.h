#ifndef coretransactions_h
#define coretransactions_h

#include "protocol/transaction.h"
#include "types2.h"

DefineProtocol(CoreTransactorProtocol)
DefineProtocolVersion(CoreTransactorVersion1, CoreTransactorProtocol)

DefineProtocolMessage(CTV1AddChange, CoreTransactorVersion1,
	void(
		GlobalChangeIDT ChangeID,
		ChangeIDT ParentID,
		OptionalT<ChangeIDT> HeadID,
		OptionalT<StorageIDT> StorageID,
		OptionalT<size_t> StorageRefCount,
		bool DeleteMissing))

DefineProtocolMessage(CTV1UpdateDeleteHead, CoreTransactorVersion1,
	void(
		OptionalT<StorageIDT> StorageID,
		OptionalT<size_t> StorageRefCount,
		GlobalChangeIDT ChangeID,
		OptionalT<ChangeIDT> DeleteParent,
		OptionalT<HeadT> NewHead,
		OptionalT<StorageIDT> NewStorageID,
		StorageChangesT StorageChanges))

typedef TransactorT<
	CTV1AddChange,
	CTV1UpdateDeleteHead> CoreTransactionsT;

#endif

