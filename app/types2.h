#ifndef types2_h
#define types2_h

#include "structtypes.h"

typedef VariantT<std::vector<BytesChangeT>, TruncateT> StorageChangesT;

struct DefineHeadT
{
	inline DefineHeadT(StorageChangesT const &StorageChanges, NodeMetaT const &MetaChanges) :
		StorageChanges(StorageChanges), MetaChanges(MetaChanges) 
		{}
	StorageChangesT const &StorageChanges;
	NodeMetaT MetaChanges;
};
struct DeleteHeadT {};

#endif

