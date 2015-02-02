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
		if (sqlite3_bind_text(Context, Index, Value.c_str(), static_cast<int>(Value.size()), nullptr) != SQLITE_OK)
			throw SystemErrorT() << "Could not bind argument " << Index << " to \"" << Template << "\": " << sqlite3_errmsg(BaseContext);
		++Index;
	}
	
	static void BindNull(
		sqlite3_stmt *Context, 
		int &Index)
	{
		sqlite3_bind_null(Context, Index++);
	}

	static std::string Unbind(
		sqlite3_stmt *Context, 
		int &Index)
	{ 
		return (char const *)sqlite3_column_text(Context, Index++); 
	}
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
		sqlite3_bind_null(Context, Index++);
	}

	static IntegerT Unbind(
		sqlite3_stmt *Context, 
		int &Index)
	{
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
};

// ----------------
// Tuple and derived

template <typename ValueT, typename TupleT> struct DBImplm_Tuple {};

template
<
	typename ValueT,
	typename ...InnerT
>
	struct DBImplm_Tuple
	<
		ValueT,
		std::tuple<InnerT...>
	>
{
	template <typename ...AllT>
		static void Bind2(
			sqlite3 *BaseContext,
			sqlite3_stmt *Context,
			const char *Template,
			int &Index,
			ValueT const &Value)
	{
	}
	
	template <typename NextT, typename ...RemainingT>
		static void Bind2(
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
			std::get<sizeof...(InnerT) - (sizeof...(RemainingT) + 1)>(Value)); 
		Bind2<RemainingT...>(
			BaseContext,
			Context,
			Template,
			Index,
			Value);
	}

	static void Bind(
		sqlite3 *BaseContext,
		sqlite3_stmt *Context,
		const char *Template,
		int &Index,
		ValueT const &Value)
	{
		Bind2<InnerT...>(
			BaseContext,
			Context,
			Template,
			Index,
			Value);
	}
	
	template <typename NextT, typename ...RemainingT>
		static void BindNull2(
			sqlite3_stmt *Context,
			int &Index)
	{
		sqlite3_bind_null(Context, Index++);
		BindNull2<RemainingT...>(
			Context,
			Index);
	}
	
	static void BindNull2(
		sqlite3_stmt *Context, 
		int &Index)
	{
	}
	
	static void BindNull(
		sqlite3_stmt *Context, 
		int &Index)
	{
		BindNull2<InnerT...>(
			Context, 
			Index);
	}

	template 
	<
		typename NextT, 
		typename ...RemainingT, 
		typename ...SeenT
	>
		static ValueT Unbind2(
			sqlite3_stmt *Context, 
			int &Index,
			SeenT ...Seen)
	{ 
		return Unbind2<RemainingT...>(
			Context, 
			Index, 
			std::forward<SeenT>(Seen)...,
			DBImplm<NextT>::Unbind(
				Context,
				Index));
	}

	template <typename ...SeenT>
		static ValueT Unbind2(
			sqlite3_stmt *Context, 
			int &Index, 
			SeenT ...Seen)
	{
	       return ValueT(std::forward<SeenT>(Seen)...);
	}
	
	static ValueT Unbind(
		sqlite3_stmt *Context, 
		int &Index)
	{
		return Unbind2<InnerT...>(
			Context,
			Index);
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
		DBImplm_Tuple<ValueT, typename ValueT::TupleT>::Bind(
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
		DBImplm_Tuple<ValueT, typename ValueT::TupleT>::BindNull(Context, Index);
	}

		
	static ValueT Unbind(
		sqlite3_stmt *Context, 
		int &Index)
	{ 
		return DBImplm_Tuple<ValueT, typename ValueT::TupleT>::Unbind(
			Context, 
			Index);
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
			++Index;
			return {};
		}
		else return { DBImplm<InnerT>::Unbind(Context, Index) };
	}
};

#endif

