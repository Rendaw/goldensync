// TODO actually confirm results

// TODO
// Framework
// For normal condition runs:
// 1. initialize one core
// 2. run unique test code
// 3. verify state
// 4. store values of all core data (storage ids, storage count, heads, changes, instances)
// 5. initialize a new core
// 6. verify state
// 7. re-get all values and compare
//
// For crash runs:
// 1. initialize one core
// 2. set vfs to run x io events
// 3. run unique test code
// 4. initial a new core
// 5. verify state

#include "../../ren-cxx-basics/stricttype.h"
#include "../../ren-cxx-basics/variant.h"
#include "../../ren-cxx-filesystem/path.h"
#include "client.h"

#include "../core.h"

struct NormalRunT {};
typedef StrictType(size_t) IOStepsT;

template <typename CallbackT>
bool FrameInner(
	CallbackT const &Callback, 
	VariantT<NormalRunT, IOStepsT> const &Control, 
	std::shared_ptr<ClunkerControlT> &Clunker)
{
	constexpr size_t ListSize = 10;

	// for normal runs, return value is meaningless
	// for iostep runs, return true == need more steps
	bool GoDeeper = false;

	std::vector<MissingT> OriginalMissings;
	std::vector<HeadT> OriginalHeads;
	std::vector<ChangeT> OriginalChanges;
	std::vector<StorageT> OriginalStorage;

	static auto const Root = Filesystem::PathT::Qualify("test_data");

	/*FinallyT Finally([&](void) 
	{
		Clunker->Clean().get();
	});*/

	std::cout << "  - primary" << std::endl;
	{
		CoreT Core({"test"}, Root);
		FinallyT Finally([&](void)
		{
			Core.DumpGraphviz("post.dot");
		});
		AssertE(Core.GetThisInstance(), 1u);
		if (Control.Is<IOStepsT>())
		{
			Assert(Clunker->SetOpCount(*Control.Get<IOStepsT>()).get());
		}
		try { Callback(Core); } catch (...) {}
		if (Control.Is<NormalRunT>())
		{
			Assert(Core.Validate());
			while (true)
			{
				auto Missings = Core.ListMissing(OriginalMissings.size(), ListSize);
				OriginalMissings.insert(OriginalMissings.end(), Missings.begin(), Missings.end());
				if (Missings.size() < ListSize) break;
			} 
			while (true)
			{
				auto Heads = Core.ListHeads(OriginalHeads.size(), ListSize);
				OriginalHeads.insert(OriginalHeads.end(), Heads.begin(), Heads.end());
				if (Heads.size() < ListSize) break;
			} 
			while (true)
			{
				auto Changes = Core.ListChanges(OriginalChanges.size(), ListSize);
				OriginalChanges.insert(OriginalChanges.end(), Changes.begin(), Changes.end());
				if (Changes.size() < ListSize) break;
			} 
			while (true)
			{
				auto Storage = Core.ListStorage(OriginalStorage.size(), ListSize);
				OriginalStorage.insert(OriginalStorage.end(), Storage.begin(), Storage.end());
				if (Storage.size() < ListSize) break;
			} 
		}
		else if (Control.Is<IOStepsT>())
		{
			auto const Count = Clunker->GetOpCount().get();
			std::cout << "End test count " << Count << std::endl;
			if (Count == 0u) GoDeeper = true;
			Assert(Clunker->SetOpCount(-1).get());
		}
	}
	
	std::cout << "  - secondary" << std::endl;
	{
		CoreT Core({"test"}, Root);
		AssertE(Core.GetThisInstance(), 1u);
		Assert(Core.Validate());
		if (Control.Is<NormalRunT>())
		{
			{
				size_t Offset = 0;
				while (true)
				{
					auto NewMissing = Core.ListMissing(Offset, 10);
					if (NewMissing.empty()) break;
					for (auto const &Missing : NewMissing)
					{
						AssertLT(Offset, OriginalMissings.size());
						AssertE(Missing, OriginalMissings[Offset++]);
					}
					if (NewMissing.size() < ListSize) break;
				} 
				AssertE(Offset, OriginalMissings.size());
			}
			{
				size_t Offset = 0;
				while (true)
				{
					auto NewHeads = Core.ListHeads(Offset, 10);
					if (NewHeads.empty()) break;
					for (auto const &Head : NewHeads)
					{
						AssertLT(Offset, OriginalHeads.size());
						AssertE(Head, OriginalHeads[Offset++]);
					}
					if (NewHeads.size() < ListSize) break;
				} 
				AssertE(Offset, OriginalHeads.size());
			}
			{
				size_t Offset = 0;
				while (true)
				{
					auto NewChanges = Core.ListChanges(Offset, 10);
					if (NewChanges.empty()) break;
					for (auto const &Change : NewChanges)
					{
						AssertLT(Offset, OriginalChanges.size());
						AssertE(Change, OriginalChanges[Offset++]);
					}
					if (NewChanges.size() < ListSize) break;
				} 
				AssertE(Offset, OriginalChanges.size());
			}
			{
				size_t Offset = 0;
				while (true)
				{
					auto NewStorage = Core.ListStorage(Offset, 10);
					if (NewStorage.empty()) break;
					for (auto const &Storage : NewStorage)
					{
						AssertLT(Offset, OriginalStorage.size());
						AssertE(Storage, OriginalStorage[Offset++]);
					}
					if (NewStorage.size() < ListSize) break;
				} 
				AssertE(Offset, OriginalStorage.size());
			}
		}
	}

	/*std::cout << "Cleanin" << std::endl;
	Assert(Clunker->Clean().get());*/
	Root.DeleteDirectory();
		
	return GoDeeper;
}

int TestLabelCount = 1;
template <typename CallbackT> 
	void Frame(CallbackT const &Callback, std::shared_ptr<ClunkerControlT> &Clunker)
{
	std::cout << "--- TEST " << TestLabelCount++ << " ---" << std::endl;
	std::cout << " == plain == " << std::endl;
	FrameInner(Callback, NormalRunT(), Clunker);
	if (Clunker)
	{
		auto Steps = IOStepsT(0u);
		while (true) 
		{
			std::cout << " == steps " << Steps << " == " << std::endl;
			if (!FrameInner(Callback, Steps++, Clunker)) break;
		}
	}
}

void CompareStorage(CoreT &Core, StorageIDT const &Storage, std::string const &Comparison)
{
	auto Handle = Core.Open(Storage);
	ReadBufferT Buffer;
	while (Handle.Read(Buffer)) {}
	AssertE(std::string((char const *)Buffer.FilledStart(Comparison.size()), Comparison.size()), Comparison);
}
	
auto Now = time(nullptr);

auto const Comparison1 = std::string("hellog");
auto const AddData1 = StorageChangesT(std::vector<BytesChangeT>{
	BytesChangeT{0, std::vector<uint8_t>(Comparison1.begin(), Comparison1.end())}
});
auto const AddData2 = StorageChangesT(std::vector<BytesChangeT>{
	BytesChangeT{0, std::vector<uint8_t>({'w', 'h', 'i', 't', 'e'})},
	BytesChangeT{5, std::vector<uint8_t>({'w', 'i', 'z', 'a', 'r', 'd'})},
	BytesChangeT{3, std::vector<uint8_t>({'p', 'e', 'a', 'n', 'u', 't', ' ', 'd', 'i', 'v', 'a'})}
});
auto const AddData3 = StorageChangesT(std::vector<BytesChangeT>{
	BytesChangeT{0, std::vector<uint8_t>({'w', 'o', 'g'})},
});
auto const Truncate1 = StorageChangesT(TruncateT());
auto const Meta1 = NodeMetaT(
	"I am a file fo rreal",
	{},
	true,
	false,
	Now,
	Now);
auto const Meta2 = NodeMetaT(
	"windows.exe",
	{},
	true,
	true,
	Now,
	Now);

int main(void)
{
	try
	{
		asio::io_service MainService;
		std::thread ClunkerThread;
		std::shared_ptr<ClunkerControlT> Clunker;
		FinallyT ClunkerCleanup;
		//Assert(getenv("CLUNKER_PORT"));
		if (getenv("CLUNKER_PORT"))
		{
			int16_t Port = 0;
			Assert(!!(StringT(getenv("CLUNKER_PORT")) >> Port));
			std::promise<std::shared_ptr<ClunkerControlT>> ClunkerPromise;
			auto ClunkerFuture = ClunkerPromise.get_future();
			ClunkerThread = std::thread(
				[&MainService, Port, ClunkerPromise = std::move(ClunkerPromise)]
					(void) mutable
				{
					ConnectClunker(
						MainService,
						asio::ip::tcp::endpoint(
							asio::ip::address_v4::loopback(),
							Port),
						[ClunkerPromise = std::move(ClunkerPromise)]
							(std::shared_ptr<ClunkerControlT> NewClunker) mutable
						{
							ClunkerPromise.set_value(std::move(NewClunker));
						});
					MainService.run();
				});
			ClunkerCleanup = [&MainService, &ClunkerThread](void) mutable
			{
				MainService.stop();
				ClunkerThread.join();
			};
			Clunker = std::move(ClunkerFuture.get());
		}

		auto Frame = [&Clunker](auto &&Callback)
		{
			::Frame(std::move(Callback), Clunker);
		};

		// Creation
		Frame([](CoreT &Core) 
		{
			AssertE(Core.ListChanges(0, 10).size(), 0u);
		});

		// Create new file, reopen
		Frame([](CoreT &Core) 
		{
			GlobalChangeIDT ChangeID;
			{
				auto InstanceIndex = Core.GetThisInstance();
				ChangeID = GlobalChangeIDT(
					NodeIDT(InstanceIndex, Core.ReserveNode()),
					ChangeIDT(InstanceIndex, Core.ReserveChange()));
				Core.AddChange(ChangeT{ChangeID, {}});
			}

			auto Changes = Core.ListChanges(0, 10);
			AssertE(Changes.size(), 1u);
			AssertE(Changes[0].ChangeID(), ChangeID);
			Assert(!Core.GetHead(ChangeID));
		});

		// Create file, define
		Frame([](CoreT &Core) 
		{
			GlobalChangeIDT ChangeID;
			{
				auto InstanceIndex = Core.GetThisInstance();
				ChangeID = GlobalChangeIDT(
					NodeIDT(InstanceIndex, Core.ReserveNode()),
					ChangeIDT(InstanceIndex, Core.ReserveChange()));
				Core.AddChange(ChangeT(ChangeID, {}));
				Core.DefineChange(ChangeID, DefineHeadT(AddData1, Meta1));
			}

			auto Changes = Core.ListChanges(0, 10);
			AssertE(Changes.size(), 1u);
			AssertE(Changes[0], ChangeT(ChangeID, {}));
			auto Head = Core.GetHead(ChangeID);
			Assert(Head);
			AssertE(Head->ChangeID(), ChangeID);
			Assert(Head->StorageID());
			CompareStorage(Core, *Head->StorageID(), "hellog");
			AssertE(Head->Meta().Filename(), "I am a file fo rreal");
			Assert(!Head->Meta().DirID());
			AssertE(Head->Meta().Writable(), true);
			AssertE(Head->Meta().Executable(), false);

			auto Heads = Core.ListDirHeads({}, 0, 10);
			AssertE(Heads.size(), 1u);
			AssertE(Heads[0].ChangeID().NodeID(), Head->ChangeID().NodeID());
		});

		// Write to file, truncate file, write again, delete
		Frame([](CoreT &Core) 
		{
			GlobalChangeIDT ChangeID1, ChangeID2, ChangeID3, ChangeID4;
			auto InstanceIndex = Core.GetThisInstance();
			auto NodeID = NodeIDT(InstanceIndex, Core.ReserveNode());
			StorageIDT StorageID1, StorageID2, StorageID3;
			{
				ChangeID1 = GlobalChangeIDT(
					NodeID,
					ChangeIDT(InstanceIndex, Core.ReserveChange()));
				Core.AddChange(ChangeT(ChangeID1, {}));
				Core.DefineChange(ChangeID1, DefineHeadT(AddData1, Meta1));
				
				StorageID1 = *Core.GetHead(ChangeID1)->StorageID();
				CompareStorage(Core, StorageID1, "hellog");
				AssertE(Core.ListStorage(0, 10).size(), 1u);
			}
			{
				ChangeID2 = GlobalChangeIDT(
					NodeID,
					ChangeIDT(InstanceIndex, Core.ReserveChange()));
				Core.AddChange(ChangeT(ChangeID2, ChangeID1.ChangeID()));
				Core.DefineChange(ChangeID2, DefineHeadT(TruncateT(), Meta1));
				
				Assert(!Core.GetHead(ChangeID1));

				auto Head = Core.GetHead(ChangeID2);
				Assert(Head);
				Assert(Head->StorageID());
				StorageID2 = *Head->StorageID();
				AssertE(StorageID2, StorageID1);
				CompareStorage(Core, StorageID2, "");
				auto AllStorage = Core.ListStorage(0, 10);
				AssertE(AllStorage.size(), 1u);
			}
			{
				ChangeID3 = GlobalChangeIDT(
					NodeID,
					ChangeIDT(InstanceIndex, Core.ReserveChange()));
				Core.AddChange(ChangeT(ChangeID3, ChangeID2.ChangeID()));
				Core.DefineChange(ChangeID3, DefineHeadT(AddData2, Meta1));

				Assert(!Core.GetHead(ChangeID2));

				auto Head = Core.GetHead(ChangeID3);
				Assert(Head);
				Assert(Head->StorageID());
				StorageID3 = *Head->StorageID();
				AssertE(StorageID3, StorageID2);
				CompareStorage(Core, StorageID3, "whipeanut diva");
				auto AllStorage = Core.ListStorage(0, 10);
				AssertE(AllStorage.size(), 1u);
			}
			{
				ChangeID4 = GlobalChangeIDT(
					NodeID,
					ChangeIDT(InstanceIndex, Core.ReserveChange()));
				Core.AddChange(ChangeT(ChangeID4, ChangeID3.ChangeID()));
				Core.DefineChange(ChangeID4, DeleteHeadT());

				Assert(!Core.GetHead(ChangeID3));
				Assert(!Core.GetHead(ChangeID4));
				try 
				{
					Core.Open(StorageID3);
					Assert(false);
				}
				catch (...) {}
				AssertE(Core.ListStorage(0, 10).size(), 0u);
			}
		});
		
		// Create directory, add file to directory
		Frame([](CoreT &Core) 
		{
			auto InstanceIndex = Core.GetThisInstance();
			NodeIDT DirID, FileID;
			{
				DirID = NodeIDT(InstanceIndex, Core.ReserveNode());
				GlobalChangeIDT ChangeID(
					DirID,
					ChangeIDT(InstanceIndex, Core.ReserveChange()));
				Core.AddChange(ChangeT(ChangeID, {}));
				Core.DefineChange(ChangeID, DefineHeadT({}, Meta1));

				auto RootHeads = Core.ListDirHeads({}, 0, 10);
				AssertE(RootHeads.size(), 1u);
				AssertE(RootHeads[0].ChangeID().NodeID(), DirID);
				auto SubHeads = Core.ListDirHeads(DirID, 0, 10);
				AssertE(SubHeads.size(), 0u);
				auto AllStorage = Core.ListStorage(0, 10);
				AssertE(AllStorage.size(), 0u);
			}
			{
				auto InstanceIndex = Core.GetThisInstance();
				FileID = NodeIDT(InstanceIndex, Core.ReserveNode());
				GlobalChangeIDT ChangeID(
					FileID,
					ChangeIDT(InstanceIndex, Core.ReserveChange()));
				Core.AddChange(ChangeT(ChangeID, {}));
				auto FileMeta = Meta2;
				FileMeta.DirID() = DirID;
				Core.DefineChange(ChangeID, DefineHeadT(AddData1, FileMeta));
				
				auto RootHeads = Core.ListDirHeads({}, 0, 10);
				AssertE(RootHeads.size(), 1u);
				AssertE(RootHeads[0].ChangeID().NodeID(), DirID);
				auto SubHeads = Core.ListDirHeads(DirID, 0, 10);
				AssertE(SubHeads.size(), 1u);
				AssertE(SubHeads[0].ChangeID(), ChangeID);
				auto AllStorage = Core.ListStorage(0, 10);
				AssertE(AllStorage.size(), 1u);
			}
		});

		// Split a file, check old storage persistence until both updates
		Frame([](CoreT &Core) 
		{
			GlobalChangeIDT Change1, Change2, Change3;
			auto InstanceIndex = Core.GetThisInstance();
			auto NodeID = NodeIDT(InstanceIndex, Core.ReserveNode());
			StorageIDT StorageID1, StorageID2, StorageID3;
			{
				Change1 = GlobalChangeIDT(
					NodeID,
					ChangeIDT(InstanceIndex, Core.ReserveChange()));
				Core.AddChange(ChangeT(Change1, {}));
				Core.DefineChange(Change1, DefineHeadT(AddData1, Meta1));
				
				CompareStorage(Core, *Core.GetHead(Change1)->StorageID(), "hellog");
				AssertE(Core.ListStorage(0, 10).size(), 1u);
			}
			
			{
				Change2 = GlobalChangeIDT(
					NodeID,
					ChangeIDT(InstanceIndex, Core.ReserveChange()));
				Core.AddChange(ChangeT(Change2, Change1.ChangeID()));
			}
			{
				Change3 = GlobalChangeIDT(
					NodeID,
					ChangeIDT(InstanceIndex, Core.ReserveChange()));
				Core.AddChange(ChangeT(Change3, Change1.ChangeID()));
			}

			{
				Core.DefineChange(Change2, DefineHeadT(AddData2, Meta1));

				AssertE(Core.ListStorage(0, 10).size(), 2u);
				CompareStorage(Core, *Core.GetHead(Change2)->StorageID(), "whipeanut diva");
			}

			{
				Core.DefineChange(Change3, DefineHeadT(AddData3, Meta1));

				AssertE(Core.ListStorage(0, 10).size(), 2u);
				CompareStorage(Core, *Core.GetHead(Change3)->StorageID(), "woglog");
			}
		});
	}
	catch (SystemErrorT const &Error)
	{
		std::cerr << "---" << std::endl;
		std::cerr << "Got system error: \n" << Error << std::endl;
		return 1;
	}
	catch (AssertionErrorT const &Error)
	{
		std::cerr << "---" << std::endl;
		std::cerr << "Got assertion error: \n" << Error << std::endl;
		return 1;
	}
	catch (...) 
	{
		std::cerr << "---" << std::endl;
		return 1;
	}

	return 0;
}
