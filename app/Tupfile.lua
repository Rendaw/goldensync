DoOnce 'ren-cxx-filesystem/Tupfile.lua'

local GeneratedHeaders = Item()
GeneratedHeaders = GeneratedHeaders + Define.Lua
{
	Script = 'protocol/generate_wrappedtuples.lua',
	Arguments = 'structtypes.src.lua',
	Inputs = Item() + 'structtypes.src.lua',
	Outputs = Item() + 'structtypes.h',
}

Define.Executable
{
	Name = 'goldensync',
	BuildExtras = GeneratedHeaders,
	LinkFlags = '-lsqlite3',
	Sources = Item()
		+ 'main.cxx'
		+ 'core.cxx'
		+ 'log.cxx'
		+ 'md5/hash.cxx'
		+ 'md5/md5.c'
		,
	Objects = Item()
		+ FilesystemObjects
		,
}

