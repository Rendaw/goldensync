#ifndef databaseoperations_h
#define databaseoperations_h

#include "../../ren-cxx-basics/error.h"
#include "../../ren-cxx-basics/stricttype.h"

template <typename T, typename Enable = void> struct DBImplm {};

// ----------------
// String
template <> struct DBImplm<std::string>
{
	static void Bind(
		sqlite3 *BaseContext, 
		sqlite3_stmt *Context, 
		const char *Template, 
		int &Index, 
		std::string const &Value)
	{
		std::cout << "           Bind string at " << Index << std::endl;
		if (sqlite3_bind_text(Context, Index, Value.c_str(), static_cast<int>(Value.size()), nullptr) != SQLITE_OK)
			throw SystemErrorT() << "Could not bind argument " << Index << " to \"" << Template << "\": " << sqlite3_errmsg(BaseContext);
		++Index;
	}
	
	static void BindNull(
		sqlite3_stmt *Context, 
		int &Index)
	{
		std::cout << "           Bind null string at " << Index << std::endl;
		sqlite3_bind_null(Context, Index++);
	}

	static std::string Unbind(
		sqlite3_stmt *Context, 
		int &Index)
	{ 
		std::cout << "           Unbind string at " << Index << std::endl;
		return (char const *)sqlite3_column_text(Context, Index++); 
	}
	
	static void UnbindNull(int &Index)
		{ ++Index; }
};

// ----------------
// Integer
template <typename IntegerT>
	struct DBImplm
	<
		IntegerT,
		typename std::enable_if<std::is_integral<IntegerT>::value>::type
	>
{
	static void Bind(
		sqlite3 *BaseContext, 
		sqlite3_stmt *Context, 
		char const *Template, 
		int &Index, 
		IntegerT const &Value)
	{
		std::cout << "           Bind int at " << Index << std::endl;
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
			throw SystemErrorT() << "Could not bind argument " << Index << " to \"" << Template << "\": " << sqlite3_errmsg(BaseContext);
		++Index;
	}
	
	static void BindNull(
		sqlite3_stmt *Context, 
		int &Index)
	{
		std::cout << "           Bind null int at " << Index << std::endl;
		sqlite3_bind_null(Context, Index++);
	}

	static IntegerT Unbind(
		sqlite3_stmt *Context, 
		int &Index)
	{
		std::cout << "           Unbind int at " << Index << std::endl;
		static_assert(sizeof(IntegerT) <= sizeof(int64_t), "Integer type is too big for sqlite");
		if (sizeof(IntegerT) < sizeof(int))
			return static_cast<IntegerT>(sqlite3_column_int(Context, Index++));
		else if (sizeof(IntegerT) == sizeof(int))
		{
			int const Got = sqlite3_column_int(Context, Index++);
			return *reinterpret_cast<IntegerT const *>(&Got);
		}
		else if (sizeof(IntegerT) < sizeof(int64_t))
			return static_cast<IntegerT>(sqlite3_column_int64(Context, Index++));
		else
		{
			int64_t const Got = sqlite3_column_int64(Context, Index++);
			return *reinterpret_cast<IntegerT const *>(&Got);
		}
	}
	
	static void UnbindNull(int &Index)
		{ ++Index; }
};

// ----------------
// Strict
template 
<
	size_t Uniqueness, 
	typename InnerT
> 
	struct DBImplm<ExplicitCastableT<Uniqueness, InnerT>>
{
	static void Bind(
		sqlite3 *BaseContext, 
		sqlite3_stmt *Context, 
		const char *Template, 
		int &Index, 
		ExplicitCastableT<Uniqueness, InnerT> const &Value)
	{ 
		DBImplm<InnerT>::Bind(BaseContext, Context, Template, Index, *Value); 
	}
	
	static void BindNull(
		sqlite3_stmt *Context, 
		int &Index)
	{
		DBImplm<InnerT>::BindNull(Context, Index);
	}

	static ExplicitCastableT<Uniqueness, InnerT> Unbind(
		sqlite3_stmt *Context, 
		int &Index)
	{ 
		return ExplicitCastableT<Uniqueness, InnerT>(DBImplm<InnerT>::Unbind(Context, Index));
	}
	
	static void UnbindNull(int &Index)
	{ 
		DBImplm<InnerT>::UnbindNull(Index);
	}
};

// ----------------
// Tuple and derived

template <typename ValueT, typename AllT, typename RemainingT> struct DBImplm_Tuple;

template
<
	typename ValueT,
	typename ...AllT,
	typename NextT,
	typename ...RemainingT
>
	struct DBImplm_Tuple
	<
		ValueT,
		std::tuple<AllT...>,
		std::tuple<NextT, RemainingT...>
	>
{
	static void Bind(
		sqlite3 *BaseContext,
		sqlite3_stmt *Context,
		const char *Template,
		int &Index,
		ValueT const &Value)
	{
		DBImplm<NextT>::Bind(
			BaseContext, 
			Context, 
			Template, 
			Index, 
			std::get<sizeof...(AllT) - (sizeof...(RemainingT) + 1)>(Value)); 
		DBImplm_Tuple<
			ValueT, 
			std::tuple<AllT...>,
			std::tuple<RemainingT...>>::Bind(
				BaseContext,
				Context,
				Template,
				Index,
				Value);
	}

	static void BindNull(
		sqlite3_stmt *Context,
		int &Index)
	{
		sqlite3_bind_null(Context, Index++);
		DBImplm_Tuple<
			ValueT, 
			std::tuple<AllT...>,
			std::tuple<RemainingT...>>::BindNull(
				Context,
				Index);
	}
	
	template 
	<
		typename ...SeenT
	>
		static ValueT Unbind(
			sqlite3_stmt *Context, 
			int &Index,
			SeenT && ...Seen)
	{ 
		return DBImplm_Tuple<
			ValueT, 
			std::tuple<AllT...>,
			std::tuple<RemainingT...>>::Unbind(
				Context, 
				Index, 
				std::forward<SeenT>(Seen)...,
				DBImplm<NextT>::Unbind(
					Context,
					Index));
	}
	
	static void UnbindNull(int &Index)
	{ 
		DBImplm<NextT>::UnbindNull(Index);
		DBImplm_Tuple<
			ValueT,
			std::tuple<AllT...>,
			std::tuple<RemainingT...>>::UnbindNull(Index);
	}
};

template
<
	typename ValueT,
	typename ...AllT
>
	struct DBImplm_Tuple
	<
		ValueT,
		std::tuple<AllT...>,
		std::tuple<>
	>
{
	static void Bind(
		sqlite3 *BaseContext,
		sqlite3_stmt *Context,
		const char *Template,
		int &Index,
		ValueT const &Value)
	{
	}
	
	static void BindNull(
		sqlite3_stmt *Context, 
		int &Index)
	{
	}
	
	template <typename ...SeenT>
		static ValueT Unbind(
			sqlite3_stmt *Context, 
			int &Index, 
			SeenT && ...Seen)
	{
	       return ValueT(std::forward<SeenT>(Seen)...);
	}
	
	static void UnbindNull(int &Index)
	{ 
	}
};

template 
<
	typename ValueT
> 
	struct DBImplm
	<
		ValueT,
		typename std::enable_if<IsTuply<ValueT>::Result>::type
	>
{
	static void Bind(
		sqlite3 *BaseContext,
		sqlite3_stmt *Context,
		const char *Template,
		int &Index,
		ValueT const &Value)
	{
		DBImplm_Tuple<
			ValueT, 
			typename ValueT::TupleT, 
			typename ValueT::TupleT>::Bind(
				BaseContext,
				Context, 
				Template,
				Index,
				Value);
	}
	
	static void BindNull(
		sqlite3_stmt *Context, 
		int &Index)
	{
		DBImplm_Tuple<
			ValueT, 
			typename ValueT::TupleT, 
			typename ValueT::TupleT>::BindNull(Context, Index);
	}

		
	static ValueT Unbind(
		sqlite3_stmt *Context, 
		int &Index)
	{ 
		return DBImplm_Tuple<
			ValueT, 
			typename ValueT::TupleT, 
			typename ValueT::TupleT>::template Unbind<>(
				Context, 
				Index);
	}
	
	static void UnbindNull(int &Index)
	{ 
		return DBImplm_Tuple<
			ValueT, 
			typename ValueT::TupleT, 
			typename ValueT::TupleT>::UnbindNull(Index);
	}
};

// ----------------
// Optional
template 
<
	typename InnerT
> 
	struct DBImplm<OptionalT<InnerT>>
{
	typedef OptionalT<InnerT> ValueT;

	static void Bind(
		sqlite3 *BaseContext, 
		sqlite3_stmt *Context, 
		const char *Template, 
		int &Index, 
		ValueT const &Value)
	{ 
		if (!Value) DBImplm<InnerT>::BindNull(Context, Index);
		else DBImplm<InnerT>::Bind(BaseContext, Context, Template, Index, *Value); 
	}

	static void BindNull(
		sqlite3_stmt *Context, 
		int &Index)
	{
		DBImplm<InnerT>::BindNull(Context, Index);
	}

	static ValueT Unbind(
		sqlite3_stmt *Context, 
		int &Index)
	{ 
		if (sqlite3_column_type(Context, Index) == SQLITE_NULL) 
		{
			DBImplm<InnerT>::UnbindNull(Index);
			return {};
		}
		else return { DBImplm<InnerT>::Unbind(Context, Index) };
	}
	
	static void UnbindNull(int &Index)
	{ 
		return DBImplm_Tuple<
			ValueT, 
			typename ValueT::TupleT, 
			typename ValueT::TupleT>::UnbindNull(Index);
	}
};

#endif

