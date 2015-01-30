-- Augment std::tuples with methods for named value access.
-- Takes a source lua file and produces a header of types.

string.format2 = function(source, args)
	for key, val in pairs(args)
	do
		source = source:gsub('%%{' .. key .. '}', val)
	end
	return source
end

function values(a, callback)
	for index, value in ipairs(a) do callback(value) end
end

local source = arg[1]
local base = source:gsub('%.src%.%w+$', '')
local dest = base .. '.h'
local definition = loadfile(arg[1])()

local accessor_pattern = [[
	%{type} &%{name}(void) { return std::get<%{index}>(*this); }
	%{type} const &%{name}(void) const { return std::get<%{index}>(*this); }
]]

local struct_pattern = [[
struct %{name} : 
	std::tuple
	<
		%{types}
	>
{
	typedef std::tuple
	<
		%{types}
	> TupleT;

	using TupleT::TupleT;

%{methods}
};
template <> struct IsTuply<%{name}> 
	{ static constexpr bool Result = true; };
static_assert(IsTuply<%{name}>::Result, "%{name} tupliness not detected");
]]

local file_pattern = [[
// GENERATED FROM %{source}
// DO NOT MODIFY THIS FILE
#ifndef %{base}_h
#define %{base}_h

%{includes}

%{structs}

#endif
]]

local includes = {}
values(definition.include, function(include)
	includes[#includes + 1] = ('#include "%{include}"'):format2({include = include})
end)

local structs = {}
values(definition.define, function(struct)
	local element_types = {}
	local methods = {}
	for element_index, element in ipairs(struct.elements)
	do
		element_types[#element_types + 1] = element[2]
		methods[#methods + 1] = accessor_pattern:format2{
			type = element[2],
			name = element[1],
			index = element_index - 1,
		}
	end

	structs[#structs + 1] = struct_pattern:format2{
		name = struct.name,
		types = table.concat(element_types, ',\n\t\t'),
		methods = table.concat(methods, '\n'),
	}
end)

io.open(dest, 'w'):write(file_pattern:format2{
	source = source,
	base = base,
	includes = table.concat(includes, '\n'),
	structs = table.concat(structs, '\n'),
}):close()

