return 
{
	include = {'types.h'},
	define = {
		------------------------
		-- Data model ids
		{
			name = 'NodeIDT',
			elements = 
			{
				{ 'Instance', 'InstanceIndexT' },
				{ 'Node', 'NodeIndexT' },
			},
		},
		
		{
			name = 'ChangeIDT',
			elements = 
			{
				{ 'Instance', 'InstanceIndexT' },
				{ 'Change', 'ChangeIndexT' },
			},
		},

		{ 
			name = 'GlobalChangeIDT', 
			elements = 
			{
				{ 'NodeID', 'NodeIDT', },
				{ 'ChangeID', 'ChangeIDT', },
			},
		},

		------------------------
		-- Data model
		{
			name = 'ChangeT',
			elements = 
			{
				{ 'ChangeID', 'GlobalChangeIDT' },
				{ 'ParentID', 'ChangeIDT' },
			},
		},
		
		{ 
			name = 'NodeMetaT', 
			elements = 
			{
				{ 'Filename', 'std::string', },
				{ 'DirID', 'OptionalT<NodeIDT>', },
				{ 'Writable', 'bool', },
				{ 'Executable', 'bool', },
			},
		},

		{ 
			name = 'HeadT', 
			elements = 
			{
				{ 'ChangeID', 'GlobalChangeIDT', },
				{ 'StorageID', 'OptionalT<StorageIDT>', },
				{ 'Meta', 'NodeMetaT', },
				{ 'CreateTimestamp', 'TimeT', },
				{ 'ModifyTimestamp', 'TimeT', },
			},
		},

		{
			name = 'MissingT',
			elements = 
			{
				{ 'ChangeID', 'GlobalChangeIDT', },
				{ 'HeadID', 'OptionalT<ChangeIDT>', },
				{ 'StorageID', 'OptionalT<StorageIDT>', },
			},
		},

		{
			name = 'StorageT',
			elements =
			{
				{ 'StorgeID', 'StorageIDT', },
				{ 'ReferenceCount', 'uint8_t', },
			},
		},
		
		------------------------
		-- Misc
		{
			name = 'BytesChangeT',
			elements = 
			{
				{ 'Offset', 'size_t', },
				{ 'Bytes', 'std::vector<uint8_t>', },
			},
		},

		{
			name = 'TruncateT',
			elements = {},
		},
	},
}

