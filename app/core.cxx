// TODO handle errors permissively when reading
// TODO abort program if errors when writing
// TODO allow mount in readonly mode, don't touch transactions


CoreT::CoreT(Filesystem::PathT const &Root)
{
	// Get ID, confirm validity + regenerate if necessary
	// Clean up stray holds
	// Replay transactions
}

/*void CoreT::AddInstance(std::string const &Name)
{
}

InstanceIDT CoreT::AddInstance(std::string const &Name)
{
}*/

NodeIDT CoreT::ReserveNode(void)
{
	auto Out = Database.GetNodeID();
	Database.IncrementNodeID();
	return Out;
}

ChangeIDT CoreT::ReserveChange(void)
{
	auto Out = Database.GetChangeID();
	Database.IncrementChangeID();
	return Out;
}

void CoreT::AddChange(GlobalChangeIDT const &ChangeID, ChangeIDT const &ParentID)
{
	bool DeleteMissing = false;
	OptionalT<StorageIDT> StorageID;
	OptionalT<ChangeIDT> HeadID;
	OptionalT<size_t> StorageRefCount;
	if (auto Missing = Database.GetMissing(ChangeID.NodeID, ParentID))
	{
		StorageID = Missing.StorageID;
		HeadID = Missing.HeadID;
		DeleteMissing = true;
	}
	else
	{
		auto Head = Database.GetHead(ChangeID.NodeID, ParentID); 
		if (Head)
		{
			HeadID = ParentID;
			StorageID = Head.StorageID;
			if (StorageID)
			{
				auto Storage = Database.GetStorage(StorageID);
				StorageRefCount = Storage.RefCount + 1;
			}
		}
	}
	(*Transact)(CTV1AddChange(), 
		ChangeID.NodeID, 
		ChangeID.ChangeID, 
		ParentID, 
		HeadID, 
		StorageID, 
		StorageRefCount, 
		DeleteMissing);
}

void CoreT::TransactAddChange(
	NodeIDT const &NodeID, 
	ChangeIDT const &ChangeID, 
	ChangeIDT const &ParentID, 
	StorageIDT const &StorageID, 
	bool const &DeleteMissing,
	OptionalT<size_t> const &StorageRefCount)
{
	Database.InsertChange(ChangeID.NodeID, ChangeID.ChangeID, ParentID);
	ChangeAddListeners.Notify(ChangeID, ParentID);
	
	if (DeleteMissing)
	{
		MissingRemoveListeners.Notify(ChangeID);
		Database.DeleteMissing(ChangeID.NodeID, ChangeID.ChangeID);
	}

	if (StorageRefCount)
	{
		Database.SetStorageRefCount(*StorageID, StorageRefCount);
	}

	Database.InsertMissing(ChangeID.NodeID, ChangeID.ChangeID, HeadID, StorageID);
	MissingAddListeners.Notify(ChangeID);
}

void CoreT::DefineChange(GlobalChangeIDT const &ChangeID, VariantT<DefineHeadT, DeleteHeadT> const &Definition)
{
	OptionalT<ChangeIDT> DeleteParent;
	auto Now = time(nullptr);
	HeadT NewHead;
	NewHead.CreateTimestamp = Now;

	OptionalT<size_t> StorageRefCount;

	auto Missing = Database.GetMissing(ChangeID.NodeID, ChangeID.ChangeID);
	if (!Missing)
	{
		ERROR;
		return;
	}
	if (Missing->StorageID)
	{
		auto Storage = Database.GetStorage(StorageID);
		StorageRefCount = Storage.RefCount - 1;
	}
	if (auto Head = Database.GetHead(ChangeID.NodeID, Missing->ParentID))
	{
		DeleteParent = Missing->ParentID;
		NewHead = *Head;
		if (StorageRefCount) *StorageRefCount -= 1;
	}
	NewHead.ModifyTimestamp = time(nullptr);
	assert(Definition);
	if (auto const &DefineHead = Definition.As<DefineHeadT>())
	{
		OptionalT<StorageIDT> NewStorageID;
		if (StorageRefCount)
		{
			if (DefineHead.DataChanges.empty()) 
			{
				NewStorageID = Missing->StorageID;
				*StorageRefCount += 1;
			}
			else
			{
				if (*StorageRefCount == 0) 
				{
					NewStorageID = Missing->StorageID;
					*StorageRefCount += 1;
				}
				else 
				{
					NewStorageID = Database.GetStorageID();
					Database.IncrementStorageID();
				}
			}
		}
		else
		{
			if (DefineHead.DataChanges)
			{
				NewStorageID = Database.GetStorageID();
				Database.IncrementStorageID();
			}
		}
		NewHead.Meta = DefineHead.ChangeMeta;
		(*Transact)(
			CTV1UpdateDeleteHead(), 
			StorageID, 
			StorageRefCount,
			ChangeID, 
			DeleteParent, 
			NewHead,
			NewStorageID,
			DefineHead.DataChanges);
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
	VariantT<std::vector<ChunkT>, DefineHeadT::TruncateT> const &DataChanges)
{
	MissingRemoveListeners.Notify(ChangeID);
	Database.DeleteMissing(ChangeID.NodeID, ChangeID.ChangeID);
	if (DeleteParent) 
	{
		HeadRemoveListeners.Notify({ChangeID.NodeID, *DeleteParent});
		Database.DeleteHead(ChangeID.NodeID, *DeleteParent);
	}
	if (NewHead)
	{
		if (DataChanges)
		{
			NewStoragePath = GetStoragePath(*NewStorageID);
			if (DataChanges.As<DefineHeadT::TruncateT>())
			{
				auto File = Filesystem::fopen_write(NewStoragePath.Render());
				fclose(File);
			}
			else if (DataChanges)
			{
				FILE *NewStorage = nullptr;
				if (StorageID && (StorageID != NewStorageID) && !DataChanges->empty())
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
				for (auto const &Change : *DataChanges)
				{
					fseek(NewStorage, Change.first, SEEK_SET); // TODO handle errors
					fwrite(&Change.second[0], 1, Change.second.size(), NewStorage); // TODO handle errors
				}
				fclose(NewStorage);
			}
		}
		Database.InsertHead(
			ChangeID.NodeID,
			ChangeID.ChangeID,
			NewStorageID,
			NewHead->Meta.Filename,
			NewHead->Meta.ParentID,
			NewHead->Meta.Writable,
			NewHead->Meta.Executable,
			NewHead->CreateTimestamp,
			NewHead->ModifyTimestamp);
		if (NewStorageID)
			Database.InsertStorage(*NewStorageID);
		HeadAddListeners.Notify(ChangeID);
	}
	if (StorageRefCount)
	{
		if (*StorageRefCount == 0)
		{
			GetStoragePath(*StorageID).Delete();
			Database.DeleteStorage(*StorageID);
		}
		else
		{
			Database.SetStorageRefCount(*StorageID, *StorageRefCount);
		}
	}
}

/*std::array<GlobalChangeIDT, PageSize> CoreT::GetMissings(size_t Page);
HeadT CoreT::GetHead(GlobalChangeIDT const &HeadID);
std::array<HeadT, PageSize> CoreT::GetHeads(NodeIDT ParentID, OptionalT<InstanceIDT> Split);*/
