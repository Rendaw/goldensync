#ifndef database_operations_h
#define database_operations_h

// ----------------
// String
void BindImplementation(
	sqlite3 *BaseContext, 
	sqlite3_stmt *Context, 
	const char *Template, 
	int &Index, 
	std::string const &Value)
{
	if (sqlite3_bind_text(Context, Index, Value.c_str(), static_cast<int>(Value.size()), nullptr) != SQLITE_OK)
		throw SystemError() << "Could not bind argument " << Index << " to \"" << Template << "\": " << sqlite3_errmsg(BaseContext);
	++Index;
}

std::string UnbindImplementation(
	sqlite3_stmt *Context, 
	int &Index, 
	Type<std::string>)
{ 
	return (char const *)sqlite3_column_text(Context, Index++); 
}

// ----------------
// Integer
template <typename IntegerType, typename std::enable_if<std::is_integral<IntegerType>::value>::type* = nullptr>
	void BindImplementation(
		sqlite3 *BaseContext, 
		sqlite3_stmt *Context, 
		char const *Template, 
		int &Index, 
		IntegerType const &Value)
{
	static_assert(sizeof(Value) <= sizeof(int64_t), "Value is too big for sqlite");
	int Result = 0;
	if (sizeof(Value) < sizeof(int))
		sqlite3_bind_int(Context, Index, static_cast<int>(Value));
	else if (sizeof(Value) == sizeof(int))
		sqlite3_bind_int(Context, Index, *reinterpret_cast<int const *>(&Value));
	else if (sizeof(Value) < sizeof(int64_t))
		sqlite3_bind_int64(Context, Index, static_cast<int64_t>(Value));
	else sqlite3_bind_int64(Context, Index, *reinterpret_cast<int64_t const *>(&Value));

	if (Result != SQLITE_OK)
		throw SystemError() << "Could not bind argument " << Index << " to \"" << Template << "\": " << sqlite3_errmsg(BaseContext);
	++Index;
}

template <typename IntegerType, typename std::enable_if<std::is_integral<IntegerType>::value>::type* = nullptr>
	IntegerType UnbindImplementation(
		sqlite3_stmt *Context, 
		int &Index, 
		Type<IntegerType>)
{
	static_assert(sizeof(IntegerType) <= sizeof(int64_t), "Integer type is too big for sqlite");
	if (sizeof(IntegerType) < sizeof(int))
		return static_cast<IntegerType>(sqlite3_column_int(Context, Index++));
	else if (sizeof(IntegerType) == sizeof(int))
	{
		int const Got = sqlite3_column_int(Context, Index++);
		return *reinterpret_cast<IntegerType const *>(&Got);
	}
	else if (sizeof(IntegerType) < sizeof(int64_t))
		return static_cast<IntegerType>(sqlite3_column_int64(Context, Index++));
	else
	{
		int64_t const Got = sqlite3_column_int64(Context, Index++);
		return *reinterpret_cast<IntegerType const *>(&Got);
	}
}

// ----------------
// Strict
template <size_t ExplicitCastableUniqueness, typename ExplicitCastableType>
	void BindImplementation(
		sqlite3 *BaseContext, 
		sqlite3_stmt *Context, 
		const char *Template, 
		int &Index, 
		ExplicitCastable<ExplicitCastableUniqueness, ExplicitCastableType> const &Value)
{ 
	BindImplementation(BaseContext, Context, Template, Index, *Value); 
}

template <size_t ExplicitCastableUniqueness, typename ExplicitCastableType>
	ExplicitCastable<ExplicitCastableUniqueness, ExplicitCastableType> UnbindImplementation(
		sqlite3_stmt *Context, 
		int &Index, 
		Type<ExplicitCastable<ExplicitCastableUniqueness, ExplicitCastableType>>)
{ 
	return { UnbindImplementation(Context, Index, Type<ExplicitCastableType>()) };
}

// ----------------
// Tuple
template <typename NextT, typename ...RemainingT, typename ...TypesT>
	void BindImplementation(
		sqlite3 *BaseContext,
		sqlite3_stmt *Context,
		const char *Template,
		int &Index,
		std::tuple<TypesT ...> const &Value)
{
	BindImplementation(
		BaseContext, 
		Context, 
		Template, 
		Index, 
		std::get<sizeof...(TypesT) - (sizeof...(RemainingT) + 1)>(Value)); 
	BindImplementation<RemainingT...>(
		BaseContext,
		Context,
		Template,
		Index,
		Value);
}

template <typename ...TypesT>
	void BindImplementation(
		sqlite3 *BaseContext,
		sqlite3_stmt *Context,
		const char *Template,
		int &Index,
		std::tuple<TypesT ...> const &Value)
{
}

template <typename NextT, typename ...RemainingT, typename ...TypesT, typename ...SeenT>
	std::tuple<TypesT ...> UnbindImplementation(
		sqlite3_stmt *Context, 
		int &Index, 
		std::tuple<TypesT ...>,
		SeenT ...Seen)
{ 
	return UnbindImplementation<RemainingT...>(
		Context, 
		Index, 
		std::tuple<TypesT ...>,
		std::forward<SeenT>(Seen)...,
		UnbindImplementation(
			Context,
			Index,
			std::tuple<TypesT ...>,
			NextT));
}

template <typename ...TypesT, typename ...SeenT>
	std::tuple<TypesT ...> UnbindImplementation(
		sqlite3_stmt *Context, 
		int &Index, 
		std::tuple<TypesT ...>,
		SeenT ...Seen)
{
       return std::tuple<TypesT ...>(std::forward<SeenT>(Seen)...);
}

#endif

