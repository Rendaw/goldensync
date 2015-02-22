DoOnce 'app/Tupfile.lua'

for Index, Source in ipairs(tup.glob '*.cxx')
do
	Executable = Define.Executable
	{
		Name = tup.base(Source),
		Sources = Item(Source),
		BuildExtras = GeneratedHeaders,
		Objects = CoreObjects,
		LinkFlags = '-lsqlite3 -lluxem-cxx',
	}

	--[[Define.Test
	{
		Executable = Executable
	}]]
end

