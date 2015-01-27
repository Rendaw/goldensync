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

CoreT::CoreT(OptionalT<std::string> const &InstanceName, Filesystem::PathT const &Root)
{
	bool Create = !Root.Exists();
	if (Create && !InstanceName) 
		throw UserErrorT() << "You must specify an instance name for a new synch instance.";

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
		auto Instance = *Database->GetLastInstance();
		Database->SetPrimaryInstance(Instance, EnvHash);
	}

	// Clean up stray holds
	// TODO

	// Set up transactions, replay failed transactions
	Transact = std::make_unique<CoreTransactionsT>(
		[this](
			GlobalChangeIDT const &ChangeID,
			ChangeIDT const &ParentID,
			OptionalT<ChangeIDT> const &HeadID,
			OptionalT<StorageIDT> const &StorageID,
			OptionalT<size_t> const &StorageRefCount,
			bool const &DeleteMissing)
		{
			TransactAddChange(
				ChangeID,
				ParentID,
				HeadID,
				StorageID,
				StorageRefCount,
				DeleteMissing);
		},

		[this](
			OptionalT<StorageIDT> const &StorageID,
			OptionalT<size_t> const &StorageRefCount,
			GlobalChangeIDT const &ChangeID,
			OptionalT<ChangeIDT> const &DeleteParent,
			OptionalT<HeadT> const &NewHead,
			OptionalT<StorageIDT> const &NewStorageID,
			StorageChangesT const &StorageChanges)
		{
			TransactUpdateDeleteHead(
				StorageID,
				StorageRefCount,
				ChangeID,
				DeleteParent,
				NewHead,
				NewStorageID,
				StorageChanges);
		});
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

void CoreT::AddChange(GlobalChangeIDT const &ChangeID, ChangeIDT const &ParentID)
{
	bool DeleteMissing = false;
	OptionalT<StorageIDT> StorageID;
	OptionalT<ChangeIDT> HeadID;
	OptionalT<size_t> StorageRefCount;
	if (auto Missing = Database->GetMissing(GlobalChangeIDT(ChangeID.NodeID(), ParentID)))
	{
		StorageID = Missing->StorageID();
		HeadID = Missing->HeadID();
		DeleteMissing = true;
	}
	else
	{
		auto Head = Database->GetHead(GlobalChangeIDT(ChangeID.NodeID(), ParentID));
		if (Head)
		{
			HeadID = ParentID;
			StorageID = Head->StorageID();
			if (StorageID)
			{
				auto Storage = *Database->GetStorage(*StorageID);
				StorageRefCount = Storage.ReferenceCount() + 1;
			}
		}
	}
	(*Transact)(CTV1AddChange(),
		ChangeID,
		ParentID,
		HeadID,
		StorageID,
		StorageRefCount,
		DeleteMissing);
}

void CoreT::TransactAddChange(
	GlobalChangeIDT const &ChangeID,
	ChangeIDT const &ParentID,
	OptionalT<ChangeIDT> const &HeadID,
	OptionalT<StorageIDT> const &StorageID,
	OptionalT<size_t> const &StorageRefCount,
	bool const &DeleteMissing)
{
	Database->InsertChange({{ChangeID.NodeID(), ChangeID.ChangeID()}, ParentID});
	ChangeAddListeners.Notify(ChangeID, ParentID);

	if (DeleteMissing)
	{
		MissingRemoveListeners.Notify(ChangeID);
		Database->DeleteMissing(ChangeID.NodeID(), ChangeID.ChangeID());
	}

	if (StorageRefCount)
	{
		Database->SetStorageRefCount(*StorageID, StorageRefCount);
	}

	Database->InsertMissing(ChangeID.NodeID(), ChangeID.ChangeID(), HeadID, StorageID);
	MissingAddListeners.Notify(ChangeID);
}

void CoreT::DefineChange(GlobalChangeIDT const &ChangeID, VariantT<DefineHeadT, DeleteHeadT> const &Definition)
{
	OptionalT<ChangeIDT> DeleteParent;
	auto Now = time(nullptr);
	HeadT NewHead;
	NewHead.CreateTimestamp() = Now;

	OptionalT<size_t> StorageRefCount;

	auto Missing = Database->GetMissing(ChangeID.NodeID(), ChangeID.ChangeID());
	if (!Missing)
	{
		ERROR;
		return;
	}
	if (Missing->StorageID)
	{
		auto Storage = Database->GetStorage(StorageID);
		StorageRefCount = Storage.RefCount - 1;
	}
	if (auto Head = Database->GetHead(ChangeID.NodeID(), Missing->ParentID()))
	{
		DeleteParent = Missing->ParentID;
		NewHead = *Head;
		if (StorageRefCount) *StorageRefCount -= 1;
	}
	NewHead.ModifyTimestamp() = time(nullptr);
	assert(Definition);
	if (Definition.Is<DefineHeadT>())
	{
		auto const &DefineHead = Definition.As<DefineHeadT>();
		OptionalT<StorageIDT> NewStorageID;
		if (StorageRefCount)
		{
			if (DefineHead.StorageChanges.empty())
			{
				NewStorageID = Missing->StorageID();
				*StorageRefCount += 1;
			}
			else
			{
				if (*StorageRefCount == 0)
				{
					NewStorageID = Missing->StorageID();
					*StorageRefCount += 1;
				}
				else
				{
					NewStorageID = Database->GetStorageCounter();
					Database->IncrementStorageCounter();
				}
			}
		}
		else
		{
			if (DefineHead.StorageChanges())
			{
				NewStorageID = Database->GetStorageID();
				Database->IncrementStorageID();
			}
		}
		NewHead.Meta() = DefineHead.ChangeMeta();
		(*Transact)(
			CTV1UpdateDeleteHead(),
			StorageID,
			StorageRefCount,
			ChangeID,
			DeleteParent,
			NewHead,
			NewStorageID,
			DefineHead.StorageChanges);
	}
	else if (Definition.Is<DeleteHeadT>())
		(*Transact)(
			CTV1UpdateDeleteHead(),
			StorageID,
			StorageRefCount,
			ChangeID,
			DeleteParent,
			{},
			{},
			{});
}

void CoreT::TransactUpdateDeleteHead(
	OptionalT<StorageIDT> const &StorageID,
	OptionalT<size_t> const &StorageRefCount,
	GlobalChangeIDT const &ChangeID,
	OptionalT<ChangeIDT> const &DeleteParent,
	OptionalT<HeadT> const &NewHead,
	OptionalT<StorageIDT> const &NewStorageID,
	StorageChangesT const &StorageChanges)
{
	MissingRemoveListeners.Notify(ChangeID);
	Database->DeleteMissing(ChangeID.NodeID(), ChangeID.ChangeID);
	if (DeleteParent)
	{
		HeadRemoveListeners.Notify({ChangeID.NodeID(), *DeleteParent});
		Database->DeleteHead(ChangeID.NodeID(), *DeleteParent);
	}
	if (NewHead)
	{
		if (StorageChanges)
		{
			NewStoragePath = GetStoragePath(*NewStorageID);
			if (StorageChanges.As<DefineHeadT::TruncateT>())
			{
				auto File = Filesystem::fopen_write(NewStoragePath.Render());
				fclose(File);
			}
			else if (StorageChanges)
			{
				FILE *NewStorage = nullptr;
				if (StorageID && (StorageID != NewStorageID) && !StorageChanges->empty())
				{
					OldStoragePath = GetStoragePath(*StorageID);
					auto OldStorage = Filesystem::fopen_read(OldStoragePath.Render());
					NewStorage = Filesystem::fopen_write(NewStoragePath.Render());
					constexpr size_t BUFFER_SIZE = 8192;
					char Buffer[BUFFER_SIZE];
					size_t ReadSize;
					while (ReadSize = fread(Buffer, 1, BUFFER_SIZE, OldStorage))
						fwrite(Buffer, 1, ReadSize, NewStorage); // TODO handle errors
					fclose(OldStorage);
				}
				else
				{
					if (!StorageID) NewStorage = Filesystem::fopen_write(NewStoragePath.Render());
					else NewStorage = Filesystem::fopen_modify(NewStoragePath.Render());
				}
				for (auto const &Change : *StorageChanges)
				{
					fseek(NewStorage, Change.first, SEEK_SET); // TODO handle errors
					fwrite(&Change.second[0], 1, Change.second.size(), NewStorage); // TODO handle errors
				}
				fclose(NewStorage);
			}
		}
		Database->InsertHead(
			ChangeID.NodeID,
			ChangeID.ChangeID,
			NewStorageID,
			NewHead->Meta.Filename(),
			NewHead->Meta.ParentID(),
			NewHead->Meta.Writable(),
			NewHead->Meta.Executable(),
			NewHead->CreateTimestamp(),
			NewHead->ModifyTimestamp());
		if (NewStorageID)
			Database->InsertStorage(*NewStorageID);
		HeadAddListeners.Notify(ChangeID);
	}
	if (StorageRefCount)
	{
		if (*StorageRefCount == 0)
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

/*std::array<GlobalChangeIDT, PageSize> CoreT::GetMissings(size_t Page);
HeadT CoreT::GetHead(GlobalChangeIDT const &HeadID);
std::array<HeadT, PageSize> CoreT::GetHeads(NodeIDT ParentID, OptionalT<InstanceIDT> Split);*/
