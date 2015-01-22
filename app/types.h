#ifndef types_h
#define types_h

#include <stdint.h>

typedef uint64_t IDBaseT;
typedef StrictType(IDBaseT) NodeIDT;
typedef StrictType(IDBaseT) ChangeIDT;
typedef StrictType(IDBaseT) StorageIDT;
typedef uint64_t TimeT; // UTC seconds since Jan 1 1970

struct GlobalChangeIDT { NodeIDT NodeID; ChangeIDT ChangeID; };

struct NodeMetaT
{
	std::string Filename;
	OptionalT<ChangeIDT> ParentID;
	bool Writable;
	bool Executable;
};

struct HeadT
{
	StorageIDT StorageID;
	NodeMetaT Meta;
	TimeT CreateTimestamp;
	TimeT ModifyTimestamp;
};

#endif

