return 
{
	include = {'types.h'},
	define = {
		{ 
			name = 'GlobalChangeIDT', 
			elements = 
			{
				{ 'NodeID', 'NodeIDT', },
				{ 'ChangeID', 'ChangeIDT', },
			},
		},
		
		{ 
			name = 'NodeMetaT', 
			elements = 
			{
				{ 'Filename', 'std::string', },
				{ 'ParentID', 'OptionalT<ChangeIDT>', },
				{ 'Writable', 'bool', },
				{ 'Executable', 'bool', },
			},
		},

		{ 
			name = 'HeadT', 
			elements = 
			{
				{ 'StorageID', 'StorageIDT', },
				{ 'Meta', 'NodeMetaT', },
				{ 'CreateTimestamp', 'TimeT', },
				{ 'ModifyTimestamp', 'TimeT', },
			},
		},
	},
}

