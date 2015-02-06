// TODO handle errors permissively when reading
// TODO abort program if errors when writing
// TODO allow mount in readonly mode, don't touch transactions

#include "core.h"

#include "md5/hash.h"

// ---------------------------------------------
// ---------------------------------------------
// From http://stackoverflow.com/questions/16858782/how-to-obtain-almost-unique-system-identifier-in-a-cross-platform-way/16859693#16859693

#if defined(__WIN32)
uint16_t getCpuHash()
{
	int cpuinfo[4] = { 0, 0, 0, 0 };
	__cpuid( cpuinfo, 0 );
	uint16_t hash = 0;
	uint16_t* ptr = (uint16_t*)(&cpuinfo[0]);
	for ( uint32_t i = 0; i < 8; i++ )
		hash += ptr[i];

	return hash;
}

#elif defined(DARWIN)
#include <mach-o/arch.h>
unsigned short getCpuHash()
{
	 const NXArchInfo* info = NXGetLocalArchInfo();
	 unsigned short val = 0;
	 val += (unsigned short)info->cputype;
	 val += (unsigned short)info->cpusubtype;
	 return val;
}

#else
static void getCpuid( unsigned int* p, unsigned int ax )
{
	__asm __volatile
	(
		"movl %%ebx, %%esi\n\t"
		"cpuid\n\t"
		"xchgl %%ebx, %%esi"
		: "=a" (p[0]), "=S" (p[1]), "=c" (p[2]), "=d" (p[3])
		: "0" (ax)
	);
}

unsigned short getCpuHash()
{
	unsigned int cpuinfo[4] = { 0, 0, 0, 0 };
	getCpuid( cpuinfo, 0 );
	unsigned short hash = 0;
	unsigned int* ptr = (&cpuinfo[0]);
	for ( unsigned int i = 0; i < 4; i++ )
		hash += (ptr[i] & 0xFFFF) + ( ptr[i] >> 16 );

	return hash;
}

#endif

// ---------------------------------------------
// ---------------------------------------------

CoreT::CoreT(OptionalT<std::string> const &InstanceName, Filesystem::PathT const &Root) : 
	Root(Root), 
	StorageRoot(Root.Enter("storage")),
	Log("core")
{
	bool Create = !Root.Exists();
	if (Create)
	{
		if (!InstanceName) 
			throw UserErrorT() << "You must specify an instance name for a new synch instance.";

		Root.CreateDirectory();
		StorageRoot.CreateDirectory();
	}

	// Start DB
	Database = std::make_unique<CoreDatabaseT>(Root.Enter("coredb.sqlite3"));

	// Make sure we (probably) weren't copied
	auto EnvHash = FormatHash(HashString(StringT()
		<< Root << ":"
		<< getCpuHash() << ":"
		));
	auto OldEnvHash = *Database->GetEnvHash();
	if (EnvHash != OldEnvHash)
	{
		std::string DefinitelyInstanceName;
		if (InstanceName) DefinitelyInstanceName = *InstanceName;
		else DefinitelyInstanceName = *Database->GetPrimaryInstanceName();
		Database->InsertInstance(
			DefinitelyInstanceName, 
			std::mt19937(std::random_device()())());
		ThisInstance = *Database->GetLastInstance();
		Database->SetPrimaryInstance(EnvHash, ThisInstance);
		Log(LogT::Debug, StringT() << "Environment hash changed (" << OldEnvHash << " -> " << EnvHash << "); primary instance is now " << ThisInstance << ".");
	}
	else ThisInstance = *Database->GetPrimaryInstance();

	// Clean up stray holds
	// TODO

	// Set up transactions, replay failed transactions
	Transact = std::make_unique<CoreTransactorT>(
		Root.Enter("coretransactions"),
		*this);
}

/*void CoreT::AddInstance(std::string const &Name)
{
}

InstanceIDT CoreT::AddInstance(std::string const &Name)
{
}*/

NodeIndexT CoreT::ReserveNode(void)
{
	auto Out = *Database->GetNodeCounter();
	Database->IncrementNodeCounter();
	return Out;
}

ChangeIndexT CoreT::ReserveChange(void)
{
	auto Out = *Database->GetChangeCounter();
	Database->IncrementChangeCounter();
	return Out;
}

void CoreT::AddChange(ChangeT const &Change)
{
	bool DeleteMissing = false;
	OptionalT<StorageIDT> StorageID;
	OptionalT<ChangeIDT> HeadID;
	OptionalT<StorageReferenceCountT> StorageRefCount;
	if (Change.ParentID())
	{
		if (auto Missing = Database->GetMissing(GlobalChangeIDT(Change.ChangeID().NodeID(), *Change.ParentID())))
		{
			StorageID = Missing->StorageID();
			HeadID = Missing->HeadID();
			DeleteMissing = true;
		}
		else
		{
			auto Head = Database->GetHead(GlobalChangeIDT(Change.ChangeID().NodeID(), *Change.ParentID()));
			if (Head)
			{
				HeadID = Head->ChangeID().ChangeID();
				StorageID = Head->StorageID();
				if (StorageID)
				{
					auto Storage = *Database->GetStorage(*StorageID);
					StorageRefCount = Storage.ReferenceCount() + StorageReferenceCountT(1);
				}
			}
		}
	}
	(*Transact)(CTV1AddChange(),
		Change,
		HeadID,
		StorageID,
		StorageRefCount,
		DeleteMissing);
}

void CoreT::Handle(
	CTV1AddChange,
	ChangeT const &Change,
	OptionalT<ChangeIDT> const &HeadID,
	OptionalT<StorageIDT> const &StorageID,
	OptionalT<StorageReferenceCountT> const &StorageRefCount,
	bool const &DeleteMissing)
{
	LOG(Log, Spam, (StringT() << "Adding change " << Change.ChangeID() << "---"));
	Database->InsertChange(Change);
	ChangeAddListeners.Notify(Change);

	if (DeleteMissing)
	{
		MissingRemoveListeners.Notify(Change.ChangeID());
		Database->DeleteMissing(Change.ChangeID());
	}

	if (StorageRefCount)
	{
		Database->SetStorageRefCount(*StorageID, *StorageRefCount);
		LOG(Log, Spam, StringT() << "New missing: setting storage " << *StorageID << " to " << (int)**StorageRefCount);
	}

	Database->InsertMissing(
		MissingT(
			Change.ChangeID(),
			HeadID, 
			StorageID));
	MissingAddListeners.Notify(Change.ChangeID());
}

void CoreT::DefineChange(GlobalChangeIDT const &ChangeID, VariantT<DefineHeadT, DeleteHeadT> const &Definition)
{
	LOG(Log, Spam, (StringT() << "Defining change " << ChangeID << "---"));
	OptionalT<ChangeIDT> DeleteParent;
	//auto Now = time(nullptr);
	HeadT NewHead;
	NewHead.ChangeID() = ChangeID;

	OptionalT<StorageIDT> StorageID;
	OptionalT<StorageReferenceCountT> StorageRefCount;
	Assert(!StorageRefCount);

	auto Missing = Database->GetMissing(ChangeID);
	if (!Missing)
	{
		LOG(Log, Warning, (StringT() << "Attempting to define change with no Missing: " << ChangeID));
		return;
	}
	StorageID = Missing->StorageID();
	if (StorageID)
	{
		LOG(Log, Spam, (StringT() << "Missing has storage " << *StorageID));
		auto Storage = *Database->GetStorage(*StorageID);
		AssertGT(Storage.ReferenceCount(), StorageReferenceCountT(0));
		StorageRefCount = Storage.ReferenceCount() - StorageReferenceCountT(1);
	}
	if (Missing->HeadID())
		if (auto Head = Database->GetHead(GlobalChangeIDT(ChangeID.NodeID(), *Missing->HeadID())))
		{
			LOG(Log, Spam, (StringT() << "Missing has valid head " << *Missing->HeadID()));
			DeleteParent = Missing->HeadID();
			NewHead = *Head;
			NewHead.ChangeID() = ChangeID;
			if (StorageRefCount) 
			{
				AssertGT(*StorageRefCount, StorageReferenceCountT(0));
				*StorageRefCount -= StorageReferenceCountT(1);
			}
		}

	Assert(Definition);
	if (Definition.Is<DefineHeadT>())
	{
		auto const &DefineHead = Definition.Get<DefineHeadT>();
		if (StorageID)
		{
			if (!DefineHead.StorageChanges)
			{
				NewHead.StorageID() = StorageID;
				*StorageRefCount += StorageReferenceCountT(1);
				LOG(Log, Spam, (StringT() << "No storage changes, reusing " << *StorageID << "(" << (int)**StorageRefCount << ")"));
			}
			else
			{
				if (*StorageRefCount == StorageReferenceCountT(0))
				{
					NewHead.StorageID() = StorageID;
					*StorageRefCount += StorageReferenceCountT(1);
					LOG(Log, Spam, (StringT() << "Storage changes but no other storage references, reusing " << *StorageID << "(" << (int)**StorageRefCount << ")"));
				}
				else
				{
					NewHead.StorageID() = Database->GetStorageCounter();
					Database->IncrementStorageCounter();
					LOG(Log, Spam, (StringT() << "Storage changes but other storage references, creating new " << *NewHead.StorageID() << "(" << (int)**StorageRefCount << ")"));
				}
			}
		}
		else
		{
			if (DefineHead.StorageChanges)
			{
				NewHead.StorageID() = Database->GetStorageCounter();
				Database->IncrementStorageCounter();
				LOG(Log, Spam, (StringT() << "No existing storage, creating new " << *NewHead.StorageID()));
			}
		}
		NewHead.Meta() = DefineHead.MetaChanges;
		(*Transact)(
			CTV1UpdateDeleteHead(),
			StorageID,
			StorageRefCount,
			ChangeID,
			DeleteParent,
			NewHead,
			DefineHead.StorageChanges);
	}
	else if (Definition.Is<DeleteHeadT>())
		(*Transact)(
			CTV1UpdateDeleteHead(),
			StorageID,
			StorageRefCount,
			ChangeID,
			DeleteParent,
			OptionalT<HeadT>(),
			StorageChangesT());
}

void CoreT::Handle(
	CTV1UpdateDeleteHead,
	OptionalT<StorageIDT> const &StorageID,
	OptionalT<StorageReferenceCountT> const &StorageRefCount,
	GlobalChangeIDT const &ChangeID,
	OptionalT<ChangeIDT> const &DeleteParent,
	OptionalT<HeadT> const &NewHead,
	StorageChangesT const &StorageChanges)
{
	MissingRemoveListeners.Notify(ChangeID);
	Database->DeleteMissing(GlobalChangeIDT(ChangeID.NodeID(), ChangeID.ChangeID()));
	if (DeleteParent)
	{
		auto const HeadID = GlobalChangeIDT(ChangeID.NodeID(), *DeleteParent);
		HeadRemoveListeners.Notify(HeadID);
		Database->DeleteHead(HeadID);
		LOG(Log, Spam, (StringT() << "Deleting head " << HeadID));
	}
	if (NewHead)
	{
		if (StorageChanges)
		{
			auto NewStoragePath = GetStoragePath(*NewHead->StorageID());
			if (StorageChanges.Is<TruncateT>())
			{
				LOG(Log, Spam, StringT() << "Truncating " << NewStoragePath.Render());
				Filesystem::FileT::OpenWrite(NewStoragePath.Render());
			}
			else
			{
				auto &Changes = StorageChanges.Get<std::vector<BytesChangeT>>();

				Filesystem::FileT NewStorage;

				if (!StorageID) 
				{
					LOG(Log, Spam, StringT() << "Creating file " << NewStoragePath.Render());
					NewStorage = Filesystem::FileT::OpenWrite(NewStoragePath);
				}
				else if (StorageID != NewHead->StorageID())
				{
					// If modifying old storage
					auto OldStoragePath = GetStoragePath(*StorageID);
					auto OldStorage = Filesystem::FileT::OpenRead(OldStoragePath);
					LOG(Log, Spam, StringT() << "Copying to " << NewStoragePath.Render());
					NewStorage = Filesystem::FileT::OpenWrite(NewStoragePath);
					std::vector<uint8_t> Buffer;
					while (OldStorage.Read(Buffer)) NewStorage.Write(Buffer);
				}
				else 
				{
					LOG(Log, Spam, StringT() << "Old storage is " << *StorageID);
					LOG(Log, Spam, StringT() << "Opening to modify " << NewStoragePath.Render());
					NewStorage = Filesystem::FileT::OpenModify(NewStoragePath);
				}

				for (auto const &Change : Changes)
				{
					NewStorage.Seek(Change.Offset());
					NewStorage.Write(Change.Bytes());
				}
			}
		}
		Database->InsertHead(*NewHead);
		if (NewHead->StorageID() != StorageID) Database->InsertStorage(*NewHead->StorageID());
		HeadAddListeners.Notify(ChangeID);
	}
	if (StorageRefCount)
	{
		if (*StorageRefCount == StorageReferenceCountT(0))
		{
			GetStoragePath(*StorageID).Delete();
			Database->DeleteStorage(*StorageID);
		}
		else
		{
			Database->SetStorageRefCount(*StorageID, *StorageRefCount);
		}
	}
}

InstanceIndexT CoreT::GetThisInstance(void) const
	{ return ThisInstance; }

std::vector<ChangeT> CoreT::ListChanges(size_t Start, size_t Count)
{
	std::vector<ChangeT> Out;
	Database->ListChanges.Execute(
		Start, 
		Count, 
		[&Out](ChangeT &&Change) { Out.push_back(std::move(Change)); });
	return Out;
}

std::vector<MissingT> CoreT::ListMissing(size_t Start, size_t Count)
{
	std::vector<MissingT> Out;
	Database->ListMissing.Execute(
		Start, 
		Count, 
		[&Out](MissingT &&Missing) { Out.push_back(std::move(Missing)); });
	return Out;
}

std::vector<HeadT> CoreT::ListHeads(size_t Start, size_t Count)
{
	std::vector<HeadT> Out;
	Database->ListHeads.Execute(
		Start, 
		Count, 
		[&Out](HeadT &&Head) { Out.push_back(std::move(Head)); });
	return Out;
}
	
std::vector<HeadT> CoreT::ListDirHeads(OptionalT<NodeIDT> const &Dir, size_t Start, size_t Count)
{
	std::vector<HeadT> Out;
	Database->ListDirHeads.Execute(
		Dir,
		Start, 
		Count, 
		[&Out](HeadT &&Head) { Out.push_back(std::move(Head)); });
	return Out;
}

std::vector<StorageT> CoreT::ListStorage(size_t Start, size_t Count)
{
	std::vector<StorageT> Out;
	Database->ListStorage.Execute(
		Start, 
		Count, 
		[&Out](StorageT &&Storage) { Out.push_back(std::move(Storage)); });
	return Out;
}
	
OptionalT<HeadT> CoreT::GetHead(GlobalChangeIDT const &HeadID)
{
	return Database->GetHead(HeadID);
}

Filesystem::FileT CoreT::Open(StorageIDT const &Storage)
{
	return Filesystem::FileT::OpenRead(GetStoragePath(Storage));
}
	
void CoreT::DumpGraphviz(std::string const &RawPath)
{
	auto Out = Filesystem::FileT::OpenWrite(Filesystem::PathT::Qualify(RawPath));
	Out.Write(StringT() << "digraph \"" << RawPath << "\"\n{\n\n");
	constexpr size_t ListSize = 10;

	OptionalT<NodeIDT> LastNode;
	while (true)
	{
		auto Changes = ListChanges(0, ListSize);
		for (auto const &Change : Changes)
		{
			if (!LastNode || (*LastNode != Change.ChangeID().NodeID()))
			{
				if (LastNode)
					Out.Write(StringT() << "}\n");
				Out.Write(StringT() << 
					"subgraph \"cluster" << Change.ChangeID().NodeID() << "\" {\n" <<
					"\tlabel = \"" << Change.ChangeID().NodeID() << "\";\n");
				LastNode = Change.ChangeID().NodeID();
			}
			Out.Write(StringT() << 
				"\t { "
					"\"" << Change.ChangeID() << "\" " <<
					"[label = \"" << Change.ChangeID().ChangeID() << "\"] "
				"} ");
			if (Change.ParentID())
				Out.Write(StringT() << 
					" -> \"" << 
					GlobalChangeIDT(
						Change.ChangeID().NodeID(), 
						*Change.ParentID()) <<
					"\"");
			else Out.Write("");
			Out.Write(StringT() << ";\n");
		}
		if (Changes.size() < ListSize) break;
	} 
	if (LastNode)
		Out.Write(StringT() << "}\n");

	while (true)
	{
		auto Missings = ListMissing(0, ListSize);
		for (auto const &Missing : Missings)
		{
			Out.Write(StringT() << 
				"{ " <<
					"\"m" << Missing.ChangeID() << "\" " <<
					"[label = \"m\", shape = box, color = blue] " <<
				"} " <<
				"-> \"" << Missing.ChangeID() << "\";\n");
			if (Missing.HeadID())
				Out.Write(StringT() << 
					"\"m" << Missing.ChangeID() << "\" " <<
					"-> \"h" << GlobalChangeIDT(
						Missing.ChangeID().NodeID(), 
						*Missing.HeadID()) << "\""
					";\n");
		}
		if (Missings.size() < ListSize) break;
	}

	while (true)
	{
		auto Heads = ListHeads(0, ListSize);
		for (auto const &Head : Heads)
		{
			Out.Write(StringT() << 
				"{ " <<
					"\"h" << Head.ChangeID() << "\" " <<
					"[label = \"h\", shape = box, color = green] " <<
				"} " <<
				"-> \"" << Head.ChangeID() << "\""
				";\n");
			if (Head.StorageID())
				Out.Write(StringT() << 
					"\"h" << Head.ChangeID() << "\" " <<
					"-> \"s" << *Head.StorageID() << "\""
					";\n");
		}
		if (Heads.size() < ListSize) break;
	}

	while (true)
	{
		auto Storage = ListStorage(0, ListSize);
		for (auto const &AStorage : Storage)
		{
			Out.Write(StringT() << 
				"{ "
					"\"s" << AStorage.StorageID() << "\" " <<
					"[label = \"s " << AStorage.StorageID() << " (" << (int)*AStorage.ReferenceCount() << ")\", shape = box, color = brown] "
				"};\n");
		}
		if (Storage.size() < ListSize) break;
	}

	Out.Write(StringT() << "\n}\n");
}

Filesystem::PathT CoreT::GetStoragePath(StorageIDT const &StorageID)
{
	return StorageRoot.Enter(StringT() << StorageID);
}
