#ifndef coredatabase_h
#define coredatabase_h

#include "protocol/database.h"

enum class CoreDatabaseVersionT : unsigned int
{
	V1 = 0,
	End,
	Latest = End - 1
};

struct CoreDatabaseBaseT : SQLDatabaseT
{
	inline CoreDatabaseBaseT(Filesystem::PathT const &DatabasePath) : 
		SQLDatabaseT(DatabasePath)
	{
		bool Exists = *Get<bool (std::string const &TableName)>(
			"SELECT count(1) FROM \"sqlite_master\" WHERE \"type\" = \"table\" AND \"name\" = ? LIMIT 1", "Stats");
		if (!Exists)
		{
			// Settings and simple state
			Execute("CREATE TABLE \"Stats\" "
			"("
				"\"Version\" INTEGER NOT NULL , "
				"\"InstanceEnvHash\" CHAR(16) NOT NULL , "
				"\"InstanceIndex\" INTEGER NOT NULL , "
				"\"NodeCounter\" INTEGER NOT NULL , "
				"\"ChangeCounter\" INTEGER NOT NULL , "
				"\"StorageCounter\" INTEGER NOT NULL "
			")");
			Execute("INSERT INTO \"Stats\" VALUES (?, ?, ?, ?, ?, ?)", 
				(unsigned int)CoreDatabaseVersionT::Latest,
				std::string(""),
				0,
				1,
				1,
				1);
			
			// Tree
			Execute("CREATE TABLE \"Changes\" "
			"("
				"\"NodeInstance\" INTEGER NOT NULL , "
				"\"NodeIndex\" INTEGER NOT NULL , "
				"\"ChangeInstance\" INTEGER NOT NULL , "
				"\"ChangeIndex\" INTEGER NOT NULL , "
				"\"ParentChangeInstance\" INTEGER , "
				"\"ParentChangeIndex\" INTEGER , "
				"PRIMARY KEY (\"NodeInstance\", \"NodeIndex\", \"ChangeInstance\", \"ChangeIndex\")"
			")");

			Execute("CREATE TABLE \"Instances\" "
			"("
				"\"Instance\" INTEGER PRIMARY KEY AUTOINCREMENT , "
				"\"Name\" VARCHAR NOT NULL , "
				"\"Unique\" INTEGER NOT NULL "
			")");

			Execute("CREATE TABLE \"Heads\" "
			"("
				"\"NodeInstance\" INTEGER NOT NULL , "
				"\"NodeIndex\" INTEGER NOT NULL , "
				"\"ChangeInstance\" INTEGER NOT NULL , "
				"\"ChangeIndex\" INTEGER NOT NULL , "
				"\"StorageIndex\" INTEGER , " 
				"\"Filename\" VARCHAR NOT NULL , "
				"\"DirInstance\" INTEGER , "
				"\"DirIndex\" INTEGER , "
				"\"Writable\" BOOLEAN NOT NULL , "
				"\"Executable\" BOOLEAN NOT NULL , "
				"\"CreateTimestamp\" DATETIME NOT NULL , "
				"\"ModifyTimestamp\" DATETIME NOT NULL , "
				"PRIMARY KEY (\"NodeInstance\", \"NodeIndex\", \"ChangeInstance\", \"ChangeIndex\")"
			")");
			// TODO make root?
			
			Execute("CREATE TABLE \"Missing\" "
			"("
				"\"NodeInstance\" INTEGER NOT NULL , "
				"\"NodeIndex\" INTEGER NOT NULL , "
				"\"ChangeInstance\" INTEGER NOT NULL , "
				"\"ChangeIndex\" INTEGER NOT NULL , "
				"\"HeadInstance\" INTEGER , "
				"\"HeadIndex\" INTEGER , "
				"\"StorageIndex\" INTEGER , " 
				"PRIMARY KEY (\"NodeInstance\", \"NodeIndex\", \"ChangeInstance\", \"ChangeIndex\")"
			")");
			
			Execute("CREATE TABLE \"Storage\" "
			"("
				"\"StorageIndex\" INTEGER PRIMARY KEY , "
				"\"ReferenceCount\" INTEGER NOT NULL "
			")");
		}
		else 
		{
			auto Version = *Get<unsigned int(void)>("SELECT \"Version\" FROM \"Stats\"");
			switch ((CoreDatabaseVersionT)Version)
			{
				case CoreDatabaseVersionT::Latest: break;
				default: throw SYSTEM_ERROR << "Unknown database version " << Version;;
			}
		}
	}
};

struct CoreDatabaseT : CoreDatabaseBaseT
{
	template <typename SignatureT> using StatementT = typename SQLDatabaseT::StatementT<SignatureT>;

	StatementT<NodeIndexT (void)> GetNodeCounter;
	StatementT<void (void)> IncrementNodeCounter;
	StatementT<ChangeIndexT (void)> GetChangeCounter;
	StatementT<void (void)> IncrementChangeCounter;
	StatementT<StorageIndexT (void)> GetStorageCounter;
	StatementT<void (void)> IncrementStorageCounter;
		
	StatementT<std::string (void)> GetEnvHash;
	StatementT<void (std::string EnvHash, InstanceIndexT InstanceIndex)> SetPrimaryInstance;
	StatementT<InstanceIndexT (void)> GetPrimaryInstance;
	StatementT<std::string (void)> GetPrimaryInstanceName;
	
	StatementT<void (std::string Name, InstanceUniqueT Unique)> InsertInstance;
	StatementT<InstanceIndexT (void)> GetLastInstance;

	StatementT<ChangeT (size_t Start, size_t Count)> ListChanges;
	StatementT<ChangeT (GlobalChangeIDT const &ID)> GetChange;
	StatementT<void (ChangeT const &Change)> InsertChange;

	StatementT<MissingT (size_t Start, size_t Count)> ListMissing;
	StatementT<MissingT (GlobalChangeIDT const &ID)> GetMissing;
	StatementT<void (MissingT const &Missing)> InsertMissing;
	StatementT<void (GlobalChangeIDT const &ID)> DeleteMissing;

	StatementT<HeadT (size_t Start, size_t Count)> ListHeads;
	StatementT<HeadT (OptionalT<NodeIDT> const &Dir, size_t Start, size_t Count)> ListDirHeads;
	StatementT<HeadT (GlobalChangeIDT const &ID)> GetHead;
	StatementT<void (HeadT const &Head)> InsertHead;
	StatementT<void (GlobalChangeIDT const &ID)> DeleteHead;

	StatementT<StorageT (size_t Start, size_t Count)> ListStorage;
	StatementT<StorageT (StorageIndexT const &ID)> GetStorage;
	StatementT<void (StorageIDT const &StorageID)> InsertStorage;
	StatementT<void (StorageIndexT const &ID)> DeleteStorage;
	StatementT<void (StorageIndexT const &ID, StorageReferenceCountT RefCount)> SetStorageRefCount;

	inline CoreDatabaseT(Filesystem::PathT const &DatabasePath) : 
		CoreDatabaseBaseT(DatabasePath),
		GetNodeCounter(this, 
			"SELECT \"NodeCounter\" FROM \"Stats\" LIMIT 1"),
		IncrementNodeCounter(this, 
			"UPDATE \"Stats\" SET \"NodeCounter\" = \"NodeCounter\" + 1"),
		GetChangeCounter(this, 
			"SELECT \"ChangeCounter\" FROM \"Stats\" LIMIT 1"),
		IncrementChangeCounter(this, 
			"UPDATE \"Stats\" SET \"ChangeCounter\" = \"ChangeCounter\" + 1"),
		GetStorageCounter(this, 
			"SELECT \"StorageCounter\" FROM \"Stats\" LIMIT 1"),
		IncrementStorageCounter(this, 
			"UPDATE \"Stats\" SET \"StorageCounter\" = \"StorageCounter\" + 1"),

		GetEnvHash(this,
			"SELECT \"InstanceEnvHash\" FROM \"Stats\" LIMIT 1"),
		SetPrimaryInstance(this,
			"UPDATE \"Stats\" SET \"InstanceEnvHash\" = ?, \"InstanceIndex\" = ?"),
		GetPrimaryInstance(this,
			"SELECT \"InstanceIndex\" FROM \"Stats\" LIMIT 1"),
		GetPrimaryInstanceName(this,
			"SELECT \"Name\" FROM \"Instances\" WHERE \"Instance\" = (SELECT \"InstanceIndex\" FROM \"Stats\" LIMIT 1) LIMIT 1"),
			
		InsertInstance(this,
			"INSERT INTO \"Instances\" (\"Instance\", \"Name\", \"Unique\") VALUES (NULL, ?, ?)"),
		GetLastInstance(this,
			"SELECT \"Instance\" FROM \"Instances\" ORDER BY \"Instance\" DESC LIMIT 1"),

		ListChanges(this,
			"SELECT * FROM \"Changes\" LIMIT ?,?"),
		GetChange(this,
			"SELECT * FROM \"Changes\" WHERE \"NodeInstance\" = ? AND \"NodeIndex\" = ? AND \"ChangeInstance\" = ? AND \"ChangeIndex\" = ? LIMIT 1"),
		InsertChange(this,
			"INSERT INTO \"Changes\" VALUES (?, ?, ?, ?, ?, ?)"),

		ListMissing(this,
			"SELECT * FROM \"Missing\" LIMIT ?,?"),
		GetMissing(this,
			"SELECT * FROM \"Missing\" WHERE \"NodeInstance\" = ? AND \"NodeIndex\" = ? AND \"ChangeInstance\" = ? AND \"ChangeIndex\" = ? LIMIT 1"),
		InsertMissing(this,
			"INSERT INTO \"Missing\" VALUES (?, ?, ?, ?, ?, ?, ?)"),
		DeleteMissing(this,
			"DELETE FROM \"Missing\" WHERE \"NodeInstance\" = ? AND \"NodeIndex\" = ? AND \"ChangeInstance\" = ? AND \"ChangeIndex\" = ?"),

		ListHeads(this,
			"SELECT * FROM \"Heads\" LIMIT ?,?"),
		ListDirHeads(this,
			"SELECT * FROM \"Heads\" WHERE \"DirInstance\" IS ? AND \"DirIndex\" IS ? LIMIT ?,?"),
		GetHead(this,
			"SELECT * FROM \"Heads\" WHERE \"NodeInstance\" = ? AND \"NodeIndex\" = ? AND \"ChangeInstance\" = ? AND \"ChangeIndex\" = ? LIMIT 1"),
		InsertHead(this,
			"INSERT OR IGNORE INTO \"Heads\" VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"),
		DeleteHead(this,
			"DELETE FROM \"Heads\" WHERE \"NodeInstance\" = ? AND \"NodeIndex\" = ? AND \"ChangeInstance\" = ? AND \"ChangeIndex\" = ?"),

		ListStorage(this,
			"SELECT * FROM \"Storage\" LIMIT ?,?"),
		GetStorage(this,
			"SELECT * FROM \"Storage\" WHERE \"StorageIndex\" = ? LIMIT 1"),
		InsertStorage(this,
			"INSERT INTO \"Storage\" (\"StorageIndex\", \"ReferenceCount\") VALUES (?, 1)"),
		DeleteStorage(this,
			"DELETE FROM \"Storage\" WHERE \"StorageIndex\" = ?"),
		SetStorageRefCount(this,
			"UPDATE \"Storage\" SET \"ReferenceCount\" = ?2 WHERE \"StorageIndex\" = ?1")
	{
	}
};
	
#endif

