#ifndef database_h
#define database_h

#include <sqlite3.h>
#include <type_traits>

//#include "../../ren-cxx-basics/type.h"
#include "../../ren-cxx-basics/extrastandard.h"
#include "../../ren-cxx-filesystem/path.h"
#include "databaseoperations.h"

template <typename Classification> struct Type {};
/*template <typename Classification> struct Binary {};
template <typename Classification> using BinaryType = Type<Binary<Classification>>;*/

struct SQLDatabaseT
{
	inline SQLDatabaseT(OptionalT<Filesystem::PathT> const &Path = {}) : Context(nullptr)
	{
		if ((!Path && (sqlite3_open(":memory:", &Context) != 0)) ||
			(Path && (sqlite3_open(Path->Render().c_str(), &Context) != 0)))
			throw SystemErrorT() << "Could not create database: " << sqlite3_errmsg(Context);
	}

	inline ~SQLDatabaseT(void)
	{
		sqlite3_close(Context);
	}

	template <typename SignatureT> struct StatementT;
	template <typename ...ResultT, typename ...ArgumentsT>
		struct StatementT<std::tuple<ResultT...>(ArgumentsT...)>
	{
		StatementT(SQLDatabaseT *Base, const char *Template) : 
			Template(Template), BaseContext(Base->Context), Context(nullptr)
		{
			if (sqlite3_prepare_v2(BaseContext, Template, -1, &Context, 0) != SQLITE_OK)
				throw SystemErrorT() << "Could not prepare query \"" << Template << "\": " << sqlite3_errmsg(BaseContext);
		}

		StatementT(StatementT<std::tuple<ResultT...>(ArgumentsT...)> &&Other) : 
			Template(Other.Template), BaseContext(Other.BaseContext), Context(Other.Context)
		{
			Other.Context = nullptr;
		}

		~StatementT(void)
		{
			if (Context)
				sqlite3_finalize(Context);
		}

		void Execute(
			ArgumentsT const & ...Arguments, 
			function<void(ResultT && ...)> const &Function = function<void(ResultT & ...)>())
		{
			Assert(Context);
			if (sizeof...(Arguments) > 0)
				Bind(BaseContext, Context, 1, Arguments...);
			while (true)
			{
				int Result = sqlite3_step(Context);
				if (Result == SQLITE_DONE) break;
				if (Result != SQLITE_ROW)
					throw SystemErrorT() << "Could not execute query \"" << Template << "\": " << sqlite3_errmsg(BaseContext);
				Unbind<ResultT...>(Context, 0, Function);
			}
			sqlite3_reset(Context);
		}

		OptionalT<std::tuple<ResultT ...>> GetTuple(ArgumentsT const & ...Arguments)
		{
			bool Done = false;
			std::tuple<ResultT ...> Out;
			Execute(Arguments..., [&Out, &Done](ResultT && ...Results)
			{
				if (Done) return;
				Done = true;
				Out = std::make_tuple(std::forward<ResultT>(Results)...);
			});
			if (!Done) return {};
			return Out;
		}

		private:
			template <typename Enable = void, int ResultCount = sizeof...(ResultT)> struct GetInternal
			{
				void Get(StatementT<std::tuple<ResultT...>(ArgumentsT...)> &This) {}
			};

			template <int ResultCount> struct GetInternal<typename std::enable_if<ResultCount == 1>::type, ResultCount>
			{
				static OptionalT<typename std::tuple_element<0, std::tuple<ResultT ...>>::type>
					Get(StatementT<std::tuple<ResultT...>(ArgumentsT...)> &This, ArgumentsT const & ...Arguments)
				{
					auto Out = This.GetTuple(Arguments...);
					if (!Out) return {};
					return {std::get<0>(*Out)};
				}
			};

			template <int ResultCount> struct GetInternal<typename std::enable_if<ResultCount != 1>::type, ResultCount>
			{
				static OptionalT<std::tuple<ResultT ...>>
					Get(StatementT<std::tuple<ResultT...>(ArgumentsT...)> &This, ArgumentsT const & ...Arguments)
					{ return This.GetTuple(Arguments...); }
			};
		public:

		auto Get(ArgumentsT const & ...Arguments) -> decltype(GetInternal<>::Get(*this, Arguments...))
			{ return GetInternal<>::Get(*this, Arguments...); }

		auto operator()(ArgumentsT const & ...Arguments) -> decltype(GetInternal<>::Get(*this, Arguments...))
			{ return GetInternal<>::Get(*this, Arguments...); }

		private:
			template <typename NextT, typename... RemainingT>
				void Bind(
					sqlite3 *BaseContext, 
					sqlite3_stmt *Context, 
					int Index, 
					NextT const &Value, 
					RemainingT const &...Remaining)
			{
				DBImplm<NextT>::Bind(
					BaseContext, 
					Context, 
					Template, 
					Index, 
					Value);
				Bind(
					BaseContext, 
					Context, 
					Index, 
					std::forward<RemainingT const &>(Remaining)...);
			}

			void Bind(sqlite3 *BaseContext, sqlite3_stmt *Context, int Index)
				{ AssertE(Index - 1, sqlite3_bind_parameter_count(Context)); }

			template <typename NextT, typename ...RemainingT, typename ...ReadT>
				void Unbind(
					sqlite3_stmt *Context, 
					int Index, 
					function<void(ResultT && ...)> const &Function, 
					ReadT && ...ReadData)
			{
				Unbind<RemainingT...>(
					Context, 
					Index, 
					Function, 
					std::forward<ReadT>(ReadData)..., 
					DBImplm<NextT>::Unbind(
						Context, 
						Index));
			}

			template <typename ...ReadT>
				void Unbind(
					sqlite3_stmt *Context, 
					int Index, 
					function<void(ResultT && ...)> const &Function, 
					ReadT && ...ReadData)
			{
				AssertE(Index, sqlite3_column_count(Context));
				Function(std::forward<ReadT>(ReadData)...);
			}

			const char *Template;
			sqlite3 *BaseContext;
			sqlite3_stmt *Context;
	};

	template <typename ...ArgumentsT>
		struct StatementT<void(ArgumentsT...)> : StatementT<std::tuple<>(ArgumentsT...)>
	{
		using StatementT<std::tuple<>(ArgumentsT...)>::StatementT;
	};

	template <typename ResultType, typename ...ArgumentsT>
		struct StatementT<ResultType(ArgumentsT...)> : StatementT<std::tuple<ResultType>(ArgumentsT...)>
	{
		using StatementT<std::tuple<ResultType>(ArgumentsT...)>::StatementT;
	};

	template <typename ...ArgumentsT> 
		void Execute(char const *Template, ArgumentsT const & ...Arguments)
	{
		StatementT<void(ArgumentsT...)> Statement(this, Template);
		Statement.Execute(std::forward<ArgumentsT const &>(Arguments)...);
	}

	template <typename SignatureT, typename ...ArgumentsT>
		auto Get(char const *Template, ArgumentsT const & ...Arguments) 
		-> decltype(StatementT<SignatureT>(this, Template).Get(std::forward<ArgumentsT const &>(Arguments)...))
		{ return StatementT<SignatureT>(this, Template).Get(std::forward<ArgumentsT const &>(Arguments)...); }

	template <typename SignatureT> friend struct StatementT;
	private:
		sqlite3 *Context;
};

#endif

