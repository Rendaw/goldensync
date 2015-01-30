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
				"\"Instance\" INTEGER PRIMARY KEY AUTOINCREMENT, "
				"\"Name\" VARCHAR, "
				"\"Unique\" INTEGER "
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
				"\"IsFile\" BOOLEAN NOT NULL , "
				"\"Writable\" BOOLEAN NOT NULL , "
				"\"Executable\" BOOLEAN NOT NULL , "
				"\"CreateTimestamp\" DATETIME NOT NULL , "
				"\"ModifyTimestamp\" DATETIME NOT NULL , "
				"PRIMARY KEY (\"NodeInstance\", \"NodeIndex\")"
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
				"\"StorageIndex\" INTEGER " 
			")");
			
			Execute("CREATE TABLE \"Storage\" "
			"("
				"\"StorageIndex\" INTEGER NOT NULL , "
				"\"ReferenceCount\" INTEGER NOT NULL "
			")");
		}
		else 
		{
			auto Version = *Get<unsigned int(void)>("SELECT \"Version\" FROM \"Stats\"");
			switch ((CoreDatabaseVersionT)Version)
			{
				case CoreDatabaseVersionT::Latest: break;
				default: throw SystemErrorT() << "Unknown database version " << Version;;
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
	StatementT<void (InstanceIndexT InstanceIndex, std::string EnvHash)> SetPrimaryInstance;
	StatementT<std::string (void)> GetPrimaryInstanceName;
	
	StatementT<void (std::string Name, InstanceUniqueT Unique)> InsertInstance;
	StatementT<InstanceIndexT (void)> GetLastInstance;

	StatementT<void (ChangeT const &Change)> InsertChange;

	StatementT<MissingT (GlobalChangeIDT const &ID)> GetMissing;
	StatementT<void (MissingT const &Missing)> InsertMissing;
	StatementT<void (GlobalChangeIDT const &ID)> DeleteMissing;

	StatementT<HeadT (GlobalChangeIDT const &ID)> GetHead;
	StatementT<void (HeadT const &Head)> InsertHead;
	StatementT<void (GlobalChangeIDT const &ID)> DeleteHead;

	StatementT<StorageT (StorageIndexT const &ID)> GetStorage;
	StatementT<void (StorageIDT const &StorageID)> InsertStorage;
	StatementT<void (StorageIndexT const &ID)> DeleteStorage;
	StatementT<void (StorageIndexT const &ID, uint16_t RefCount)> SetStorageRefCount;

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
		GetPrimaryInstanceName(this,
			"SELECT \"Name\" FROM \"Instances\" WHERE \"Instance\" = (SELECT \"InstanceIndex\" FROM \"Stats\")"),
			
		InsertInstance(this,
			"INSERT INTO \"Instances\" (\"Instance\", \"Name\", \"Unique\") VALUES (NULL, ?, ?)"),
		GetLastInstance(this,
			"SELECT \"Instance\" FROM \"Instances\" ORDER BY \"Instance\" DESC LIMIT 1"),

		InsertChange(this,
			"INSERT INTO \"Changes\" VALUES (?, ?, ?, ?, ?, ?)"),

		GetMissing(this,
			"SELECT * FROM \"Missing\" WHERE \"NodeInstance\" = ? AND \"NodeIndex\" = ? AND \"ChangeInstance\" = ? AND \"ChangeIndex\" = ?"),
		InsertMissing(this,
			"INSERT INTO \"Missing\" VALUES (?, ?, ?, ?, ?, ?, ?)"),
		DeleteMissing(this,
			"DELETE FROM \"Missing\" WHERE \"NodeInstance\" = ? AND \"NodeIndex\" = ? AND \"ChangeInstance\" = ? AND \"ChangeIndex\" = ?"),

		GetHead(this,
			"SELECT * FROM \"Heads\" WHERE \"NodeInstance\" = ? AND \"NodeIndex\" = ? AND \"ChangeInstance\" = ? AND \"ChangeIndex\" = ?"),
		InsertHead(this,
			"INSERT INTO \"Heads\" VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"),
		DeleteHead(this,
			"DELETE FROM \"Heads\" WHERE \"NodeInstance\" = ? AND \"NodeIndex\" = ? AND \"ChangeInstance\" = ? AND \"ChangeIndex\" = ?"),

		GetStorage(this,
			"SELECT * FROM \"Storage\" WHERE \"StorageIndex\" = ?"),
		InsertStorage(this,
			"INSERT INTO \"Storage\" (\"StorageIndex\") VALUES (?)"),
		DeleteStorage(this,
			"DELETE FROM \"Storage\" WHERE \"StorageIndex\" = ?"),
		SetStorageRefCount(this,
			"UPDATE \"Storage\" SET \"ReferenceCount\" = ? WHERE \"StorageIndex\" = ?")
	{
	}
};
	
#endif

