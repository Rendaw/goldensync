#ifndef coretransactions_h
#define coretransactions_h

#include "protocol/transaction.h"
#include "types2.h"

DefineProtocol(CoreTransactorProtocol)
DefineProtocolVersion(CoreTransactorVersion1, CoreTransactorProtocol)

DefineProtocolMessage(CTV1AddChange, CoreTransactorVersion1,
	void(
		ChangeT Change,
		OptionalT<ChangeIDT> HeadID,
		OptionalT<StorageIDT> StorageID,
		OptionalT<StorageReferenceCountT> StorageRefCount,
		bool DeleteMissing))

DefineProtocolMessage(CTV1UpdateDeleteHead, CoreTransactorVersion1,
	void(
		OptionalT<StorageIDT> StorageID,
		OptionalT<StorageReferenceCountT> StorageRefCount,
		GlobalChangeIDT ChangeID,
		OptionalT<ChangeIDT> DeleteParent,
		OptionalT<HeadT> NewHead,
		StorageChangesT StorageChanges))

#endif

