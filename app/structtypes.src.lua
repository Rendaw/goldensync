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
				{ 'ParentID', 'OptionalT<ChangeIDT>' },
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
				{ 'CreateTimestamp', 'TimeT', },
				{ 'ModifyTimestamp', 'TimeT', },
			},
		},

		{ 
			name = 'HeadT', 
			elements = 
			{
				{ 'ChangeID', 'GlobalChangeIDT', },
				{ 'StorageID', 'OptionalT<StorageIDT>', },
				{ 'Meta', 'NodeMetaT', },
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
				{ 'StorageID', 'StorageIDT', },
				{ 'ReferenceCount', 'StorageReferenceCountT', },
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

