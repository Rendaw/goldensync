#ifndef database_h
#define database_h

#include <sqlite3.h>
#include <type_traits>

#include "../../ren-cxx-basics/type.h"
#include "../../ren-cxx-filesystem/filesystem.h"

template <typename Classification> struct Type {};
/*template <typename Classification> struct Binary {};
template <typename Classification> using BinaryType = Type<Binary<Classification>>;*/

template <typename Operations> struct BareSQLDatabase
{
	inline BareSQLDatabase(OptionalT<Filesystem::PathT> const &Path = {}) : Context(nullptr)
	{
		if ((!Path && (sqlite3_open(":memory:", &Context) != 0)) ||
			(Path && (sqlite3_open(Path->Render(), &Context) != 0)))
			throw SystemError() << "Could not create database: " << sqlite3_errmsg(Context);
	}

	inline ~BareSQLDatabase(void)
	{
		sqlite3_close(Context);
	}

	template <typename Signature> struct Statement;
	template <typename ...ResultTypes, typename ...ArgumentTypes>
		struct Statement<std::tuple<ResultTypes...>(ArgumentTypes...)> : private Operations
	{
		Statement(sqlite3 *BaseContext, const char *Template) : 
			Template(Template), BaseContext(BaseContext), Context(nullptr)
		{
			if (sqlite3_prepare_v2(BaseContext, Template, -1, &Context, 0) != SQLITE_OK)
				throw SystemError() << "Could not prepare query \"" << Template << "\": " << sqlite3_errmsg(BaseContext);
		}

		Statement(Statement<std::tuple<ResultTypes...>(ArgumentTypes...)> &&Other) : 
			Template(Other.Template), BaseContext(Other.BaseContext), Context(Other.Context)
		{
			Other.Context = nullptr;
		}

		~Statement(void)
		{
			if (Context)
				sqlite3_finalize(Context);
		}

		void Execute(
			ArgumentTypes const & ...Arguments, 
			function<void(ResultTypes && ...)> const &Function = function<void(ResultTypes & ...)>())
		{
			Assert(Context);
			if (sizeof...(Arguments) > 0)
				Bind(BaseContext, Context, 1, Arguments...);
			while (true)
			{
				int Result = sqlite3_step(Context);
				if (Result == SQLITE_DONE) break;
				if (Result != SQLITE_ROW)
					throw SystemError() << "Could not execute query \"" << Template << "\": " << sqlite3_errmsg(BaseContext);
				Unbind<ResultTypes...>(Context, 0, Function);
			}
			sqlite3_reset(Context);
		}

		Optional<std::tuple<ResultTypes ...>> GetTuple(ArgumentTypes const & ...Arguments)
		{
			bool Done = false;
			std::tuple<ResultTypes ...> Out;
			Execute(Arguments..., [&Out, &Done](ResultTypes && ...Results)
			{
				if (Done) return;
				Done = true;
				Out = std::make_tuple(std::forward<ResultTypes>(Results)...);
			});
			if (!Done) return {};
			return Out;
		}

		private:
			template <typename Enable = void, int ResultCount = sizeof...(ResultTypes)> struct GetInternal
			{
				void Get(Statement<std::tuple<ResultTypes...>(ArgumentTypes...)> &This) {}
			};

			template <int ResultCount> struct GetInternal<typename std::enable_if<ResultCount == 1>::type, ResultCount>
			{
				static Optional<typename std::tuple_element<0, std::tuple<ResultTypes ...>>::type>
					Get(Statement<std::tuple<ResultTypes...>(ArgumentTypes...)> &This, ArgumentTypes const & ...Arguments)
				{
					auto Out = This.GetTuple(Arguments...);
					if (!Out) return {};
					return {std::get<0>(Out.Data)};
				}
			};

			template <int ResultCount> struct GetInternal<typename std::enable_if<ResultCount != 1>::type, ResultCount>
			{
				static Optional<std::tuple<ResultTypes ...>>
					Get(Statement<std::tuple<ResultTypes...>(ArgumentTypes...)> &This, ArgumentTypes const & ...Arguments)
					{ return This.GetTuple(Arguments...); }
			};
		public:

		auto Get(ArgumentTypes const & ...Arguments) -> decltype(GetInternal<>::Get(*this, Arguments...))
			{ return GetInternal<>::Get(*this, Arguments...); }

		auto operator()(ArgumentTypes const & ...Arguments) -> decltype(GetInternal<>::Get(*this, Arguments...))
			{ return GetInternal<>::Get(*this, Arguments...); }

		private:
			using Operations::Bind;
			using Operations::Unbind;

			template <typename NextType, typename... RemainingTypes>
				void Bind(
					sqlite3 *BaseContext, 
					sqlite3_stmt *Context, 
					int Index, 
					NextType const &Value, 
					RemainingTypes const &...RemainingArguments)
			{
				Bind_Implementation(BaseContext, Context, Template, Index, Value);
				Bind(BaseContext, Context, Index, std::forward<RemainingTypes const &>(RemainingArguments)...);
			}

			void Bind(sqlite3 *BaseContext, sqlite3_stmt *Context, int Index)
				{ Assert(Index - 1, sqlite3_bind_parameter_count(Context)); }

			template <typename NextType, typename ...RemainingTypes, typename ...ReadTypes>
				void Unbind(
					sqlite3_stmt *Context, 
					int Index, 
					function<void(ResultTypes && ...)> const &Function, 
					ReadTypes && ...ReadData)
			{
				auto NewValue = Unbind_Implementation(Context, Index, Type<NextType>());
				Unbind<RemainingTypes...>(
					Context, Index, Function, std::forward<ReadTypes>(ReadData)..., std::move(NewValue));
			}

			template <typename ...ReadTypes>
				void Unbind(
					sqlite3_stmt *Context, 
					int Index, 
					function<void(ResultTypes && ...)> const &Function, 
					ReadTypes && ...ReadData)
			{
				Assert(Index, sqlite3_column_count(Context));
				Function(std::forward<ReadTypes>(ReadData)...);
			}

			const char *Template;
			sqlite3 *BaseContext;
			sqlite3_stmt *Context;
	};

	template <typename ...ArgumentTypes>
		struct Statement<void(ArgumentTypes...)> : Statement<std::tuple<>(ArgumentTypes...)>
	{
		using Statement<std::tuple<>(ArgumentTypes...)>::Statement;
	};

	template <typename ResultType, typename ...ArgumentTypes>
		struct Statement<ResultType(ArgumentTypes...)> : Statement<std::tuple<ResultType>(ArgumentTypes...)>
	{
		using Statement<std::tuple<ResultType>(ArgumentTypes...)>::Statement;
	};

	template <typename Signature> Statement<Signature> Prepare(char const *Template)
		{ return Statement<Signature>(Context, Template); }

	template <typename ...ArgumentTypes> void Execute(char const *Template, ArgumentTypes const & ...Arguments)
	{
		Statement<void(ArgumentTypes...)> Statement(Context, Template);
		Statement.Execute(Arguments...);
	}

	template <typename Signature, typename ...ArgumentTypes>
		auto Get(char const *Template, ArgumentTypes const & ...Arguments) -> decltype(Statement<Signature>(nullptr, Template).Get(Arguments...))
		{ return Statement<Signature>(Context, Template).Get(Arguments...); }

	private:
		sqlite3 *Context;
};

#endif

