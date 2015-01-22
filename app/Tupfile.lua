DoOnce 'ren-cxx-filesystem/Tupfile.lua'

local GeneratedHeaders = Item()
GeneratedHeaders = GeneratedHeaders + Define.Lua
{
	Script = 'shared/generate_wrappedtuples.lua',
	Arguments = 'structtypes.src.lua',
	Inputs = Item() + 'structtypes.src.lua',
	Outputs = Item() + 'structtypes.h',
}

Define.Executable
{
	Name = 'goldensync',
	BuildExtras = GeneratedHeaders,
	Sources = Item()
		+ 'core.cxx'
		+ 'main.cxx',
}

