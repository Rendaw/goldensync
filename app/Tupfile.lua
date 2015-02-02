DoOnce 'ren-cxx-filesystem/Tupfile.lua'

GeneratedHeaders = Item()
GeneratedHeaders = GeneratedHeaders + Define.Lua
{
	Script = 'protocol/generate_wrappedtuples.lua',
	Arguments = 'structtypes.src.lua',
	Inputs = Item() + 'structtypes.src.lua',
	Outputs = Item() + 'structtypes.h',
}

CoreObjects = Define.Objects
{
	Sources = Item()
		+ 'core.cxx'
		+ 'log.cxx'
		+ 'md5/hash.cxx'
		+ 'md5/md5.c'
		,
	BuildExtras = GeneratedHeaders,
} + FilesystemObjects

Define.Executable
{
	Name = 'goldensync',
	LinkFlags = '-lsqlite3',
	Sources = Item()
		+ 'main.cxx'
		,
	BuildExtras = GeneratedHeaders,
	Objects = Item()
		+ CoreObjects
		,
}

