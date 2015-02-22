#ifndef protocoloperations_h
#define protocoloperations_h

#include "../../ren-cxx-basics/error.h"
#include "protocoltypes.h"

template <typename ArgT> struct StripT { typedef ArgT T; };
template <typename ArgT> struct StripT<ArgT const> { typedef ArgT T; };
template <typename ArgT> struct StripT<ArgT &> { typedef ArgT T; };
template <typename ArgT> struct StripT<ArgT const &> { typedef ArgT T; };
template <typename ArgT> struct StripT<ArgT &&> { typedef ArgT T; };
template <typename ArgT> struct StripT<ArgT const &&> { typedef ArgT T; };

template <typename Type, typename Enable = void> struct ProtocolOperations;

// ----------------
// Integer types
template <typename IntT> 
	struct ProtocolOperations
	<
		IntT, 
		typename std::enable_if<std::is_integral<IntT>::value>::type
	>
{
	constexpr static size_t GetSize(IntT const &Argument) 
	{ 
		return sizeof(IntT); 
	}

	inline static void Write(
		uint8_t *&Out, 
		IntT const &Argument)
	{ 
		*reinterpret_cast<IntT *>(Out) = Argument; 
		Out += sizeof(Argument); 
	}

	static void Read(
		Protocol::VersionIDT const &VersionID,
		Protocol::MessageIDT const &MessageID,
		uint8_t const *Buffer,
		Protocol::SizeT const BufferSize,
		Protocol::SizeT &Offset,
		IntT &Data)
	{
		if (*BufferSize < StrictCast(Offset, size_t) + sizeof(IntT))
			throw ASSERTION_ERROR;

		Data = *reinterpret_cast<IntT const *>(&Buffer[*Offset]);
		Offset += static_cast<Protocol::SizeT::Type>(sizeof(IntT));
	}
};

// ----------------
// Unique types
template <size_t Uniqueness, typename Type> 
	struct ProtocolOperations<ExplicitCastableT<Uniqueness, Type>, 
	void
>
{
	typedef ExplicitCastableT<Uniqueness, Type> Explicit;
	constexpr static size_t GetSize(Explicit const &Argument)
	{ 
		return ProtocolOperations<Type>::GetSize(*Argument); 
	}

	inline static void Write(
		uint8_t *&Out, 
		Explicit const &Argument)
	{ 
		ProtocolOperations<Type>::Write(Out, *Argument); 
	}

	static void Read(
		Protocol::VersionIDT const &VersionID, 
		Protocol::MessageIDT const &MessageID, 
		uint8_t const *Buffer, 
		Protocol::SizeT const BufferSize,
		Protocol::SizeT &Offset, 
		Explicit &Data)
	{ 
		ProtocolOperations<Type>::Read(
			VersionID, 
			MessageID, 
			Buffer, 
			BufferSize,
			Offset, 
			*Data); 
	}
};

// ----------------
// Strings
template <> struct ProtocolOperations<std::string, void>
{
	static size_t GetSize(std::string const &Argument)
	{
		assert(Argument.size() <= std::numeric_limits<Protocol::ArraySizeT::Type>::max());
		return {Protocol::SizeT::Type(Protocol::ArraySizeT::Size + Argument.size())};
	}

	inline static void Write(
		uint8_t *&Out, 
		std::string const &Argument)
	{
		auto const StringSize = Protocol::ArraySizeT(Argument.size());
		ProtocolOperations<
			typename std::remove_const<
				typename std::remove_reference<
					decltype(*StringSize)>::type>::type>::Write(Out, *StringSize);
		memcpy(Out, Argument.c_str(), Argument.size());
		Out += Argument.size();
	}

	static void Read(
		Protocol::VersionIDT const &VersionID,
		Protocol::MessageIDT const &MessageID,
		uint8_t const *Buffer,
		Protocol::SizeT const BufferSize,
		Protocol::SizeT &Offset,
		std::string &Data)
	{
		if (*BufferSize < StrictCast(Offset, size_t) + Protocol::ArraySizeT::Size)
			throw ASSERTION_ERROR;
		Protocol::ArraySizeT::Type const &Size = *reinterpret_cast<Protocol::ArraySizeT::Type const *>(&Buffer[*Offset]);
		Offset += static_cast<Protocol::SizeT::Type>(sizeof(Size));
		if (*BufferSize < StrictCast(Offset, size_t) + (size_t)Size)
			throw ASSERTION_ERROR;
		Data = std::string(reinterpret_cast<char const *>(&Buffer[*Offset]), Size);
		Offset += Size;
	}
};

// ----------------
// Collections
template <typename ElementType> 
	struct ProtocolOperations
	<
		std::vector<ElementType>, 
		typename std::enable_if<!std::is_class<ElementType>::value>::type
	>
{
	static size_t GetSize(std::vector<ElementType> const &Argument)
	{
		assert(Argument.size() <= std::numeric_limits<Protocol::ArraySizeT::Type>::max());
		return Protocol::ArraySizeT::Size + (Protocol::ArraySizeT::Type)Argument.size() * sizeof(ElementType);
	}

	inline static void Write(uint8_t *&Out, std::vector<ElementType> const &Argument)
	{
		auto const ArraySize = Protocol::ArraySizeT(Argument.size());
		ProtocolOperations<StripT<decltype(*ArraySize)>::T>::Write(Out, *ArraySize);
		memcpy(Out, &Argument[0], Argument.size() * sizeof(ElementType));
		Out += Argument.size() * sizeof(ElementType);
	}

	static void Read(
		Protocol::VersionIDT const &VersionID,
		Protocol::MessageIDT const &MessageID,
		uint8_t const *Buffer,
		Protocol::SizeT const BufferSize,
		Protocol::SizeT &Offset,
		std::vector<ElementType> &Data)
	{
		// Read the size
		if (*BufferSize < StrictCast(Offset, size_t) + Protocol::ArraySizeT::Size)
			throw ASSERTION_ERROR;
		Protocol::ArraySizeT::Type const &Size = 
			*reinterpret_cast<Protocol::ArraySizeT::Type const *>(&Buffer[*Offset]);
		Offset += static_cast<Protocol::SizeT::Type>(sizeof(Size));

		// Read the data
		if (*BufferSize < StrictCast(Offset, size_t) + Size * sizeof(ElementType))
			throw ASSERTION_ERROR;
		Data.resize(Size);
		memcpy(&Data[0], &Buffer[*Offset], Size * sizeof(ElementType));
		Offset += static_cast<Protocol::SizeT::Type>(Size * sizeof(ElementType));
	}
};

template <typename ElementType> 
	struct ProtocolOperations
	<
		std::vector<ElementType>, 
		typename std::enable_if<std::is_class<ElementType>::value>::type
	>
{
	static size_t GetSize(std::vector<ElementType> const &Argument)
	{
		assert(Argument.size() <= std::numeric_limits<Protocol::ArraySizeT::Type>::max());
		Protocol::ArraySizeT const ArraySize = Protocol::ArraySizeT(Argument.size());
		size_t Out = Protocol::ArraySizeT::Size;
		for (
			Protocol::ArraySizeT ElementIndex = Protocol::ArraySizeT(0); 
			ElementIndex < ArraySize; 
			++ElementIndex)
		{
			Out += ProtocolGetSize(Argument[*ElementIndex]);
		}
		return Out;
	}

	inline static void Write(uint8_t *&Out, std::vector<ElementType> const &Argument)
	{
		auto const ArraySize = Protocol::ArraySizeT(Argument.size());
		ProtocolOperations<
			typename std::remove_const<
				typename std::remove_reference<
					decltype(*ArraySize)>::type>::type>::Write(Out, *ArraySize);
		for (
			Protocol::ArraySizeT ElementIndex = Protocol::ArraySizeT(0); 
			ElementIndex < ArraySize; 
			++ElementIndex)
		{
			ProtocolWrite(Out, Argument[*ElementIndex]);
		}
	}

	static void Read(
		Protocol::VersionIDT const &VersionID,
		Protocol::MessageIDT const &MessageID,
		uint8_t const *Buffer,
		Protocol::SizeT const BufferSize,
		Protocol::SizeT &Offset,
		std::vector<ElementType> &Data)
	{
		if (*BufferSize < StrictCast(Offset, size_t) + Protocol::ArraySizeT::Size) 
			throw ASSERTION_ERROR;
		Protocol::ArraySizeT::Type const &Size = *reinterpret_cast<Protocol::ArraySizeT::Type const *>(&Buffer[*Offset]);
		Offset += static_cast<Protocol::SizeT::Type>(sizeof(Size));
		if (*BufferSize < StrictCast(Offset, size_t) + (size_t)Size)
			throw ASSERTION_ERROR 
				<< "Buf size " << *BufferSize << " vs expected " << (StrictCast(Offset, size_t) + (size_t)Size);
		Data.resize(Size);
		for (Protocol::ArraySizeT ElementIndex = Protocol::ArraySizeT(0); ElementIndex < Size; ++ElementIndex)
			ProtocolRead(
				VersionID, 
				MessageID, 
				Buffer, 
				BufferSize, 
				Offset, 
				Data[*ElementIndex]);
	}
};

template <typename ElementType, size_t Count> 
	struct ProtocolOperations
	<
		std::array<ElementType, Count>, 
		typename std::enable_if<!std::is_class<ElementType>::value>::type
	>
{
	constexpr static size_t GetSize(std::array<ElementType, Count> const &Argument)
		{ return Count * sizeof(ElementType); }

	inline static void Write(uint8_t *&Out, std::array<ElementType, Count> const &Argument)
	{
		memcpy(Out, &Argument[0], Count * sizeof(ElementType));
		Out += Count * sizeof(ElementType);
	}

	static void Read(
		Protocol::VersionIDT const &VersionID,
		Protocol::MessageIDT const &MessageID,
		uint8_t const *Buffer,
		Protocol::SizeT const BufferSize,
		Protocol::SizeT &Offset,
		std::array<ElementType, Count> &Data)
	{
		if (*BufferSize < Count * sizeof(ElementType))
			throw ASSERTION_ERROR;
		memcpy(&Data[0], &Buffer[*Offset], Count * sizeof(ElementType));
		Offset += static_cast<Protocol::SizeT::Type>(Count * sizeof(ElementType));
	}
};

// ----------------
// Tuple and derived

template <typename ValueT, typename AllT, typename RemainingT> struct ProtocolOperations_TupleT;

template <typename ValueT, typename AllT> struct ProtocolOperations_TupleT<ValueT, AllT, std::tuple<>>
{
	static size_t GetSize(ValueT const &Argument)
	{ 
		return 0; 
	}
	
	static void Write(uint8_t *&Out, ValueT const &Argument)
	{
	}
	
	static void Read(
		Protocol::VersionIDT const &VersionID,
		Protocol::MessageIDT const &MessageID,
		uint8_t const *Buffer,
		Protocol::SizeT const BufferSize,
		Protocol::SizeT &Offset,
		ValueT &Data)
	{
	}
};

template <typename ValueT, typename ...AllT, typename NextT, typename ...RemainingT>
	struct ProtocolOperations_TupleT<ValueT, std::tuple<AllT...>, std::tuple<NextT, RemainingT...>>
{
	static size_t GetSize(ValueT const &Argument)
	{
		return 
			ProtocolOperations<NextT>::GetSize(
				std::get<sizeof...(AllT) - sizeof...(RemainingT) - 1>(Argument)) +
			ProtocolOperations_TupleT<ValueT, std::tuple<AllT...>, std::tuple<RemainingT...>>::GetSize(Argument);
	}

	static void Write(uint8_t *&Out, ValueT const &Argument)
	{
		ProtocolOperations<NextT>::Write(
			Out, 
			std::get<sizeof...(AllT) - sizeof...(RemainingT) - 1>(Argument));
		ProtocolOperations_TupleT<ValueT, std::tuple<AllT...>, std::tuple<RemainingT...>>::Write(Out, Argument);
	}

	static void Read(
		Protocol::VersionIDT const &VersionID,
		Protocol::MessageIDT const &MessageID,
		uint8_t const *Buffer,
		Protocol::SizeT const BufferSize,
		Protocol::SizeT &Offset,
		ValueT &Data)
	{
		ProtocolOperations<NextT>::Read(
			VersionID,
			MessageID,
			Buffer,
			BufferSize,
			Offset,
			std::get<sizeof...(AllT) - sizeof...(RemainingT) - 1>(Data));
		ProtocolOperations_TupleT<ValueT, std::tuple<AllT...>, std::tuple<RemainingT...>>::Read(
			VersionID,
			MessageID,
			Buffer,
			BufferSize,
			Offset,
			Data);
	}
};

template <typename ValueT>
	struct ProtocolOperations
	<
		ValueT,
		typename std::enable_if<IsTuply<ValueT>::Result>::type
	>
{
	// TODO SFINAE constexpr GetSize or something
	
	static size_t GetSize(ValueT const &Argument)
	{ 
		return ProtocolOperations_TupleT<ValueT, typename ValueT::TupleT, typename ValueT::TupleT>::GetSize(Argument); 
	}

	inline static void Write(uint8_t *&Out, ValueT const &Argument)
	{
		ProtocolOperations_TupleT<ValueT, typename ValueT::TupleT, typename ValueT::TupleT>::Write(Out, Argument);
	}

	static void Read(
		Protocol::VersionIDT const &VersionID,
		Protocol::MessageIDT const &MessageID,
		uint8_t const *Buffer,
		Protocol::SizeT const BufferSize,
		Protocol::SizeT &Offset,
		ValueT &Data)
	{
		ProtocolOperations_TupleT<ValueT, typename ValueT::TupleT, typename ValueT::TupleT>::Read(
			VersionID,
			MessageID,
			Buffer,
			BufferSize,
			Offset,
			Data);
	}
};

// ----------------
// Variant

template <typename ValueT> 
	struct ProtocolOperations
	<
		ValueT, 
		typename std::enable_if<IsVariant<ValueT>::Result>::type
	>
{
	static size_t GetSize(ValueT const &Argument)
	{ 
		return 
			sizeof(VariantTagT) + 
			Argument.template Examine<size_t>([](auto const &Value) -> size_t
			{ 
				return ProtocolOperations<
					typename std::remove_const<
						typename std::remove_reference<
							decltype(Value)>::type>::type>::GetSize(Value); 
			});
	}

	inline static void Write(uint8_t *&Out, ValueT const &Argument)
	{
		ProtocolOperations<VariantTagT>::Write(Out, Argument.GetTag());
		Argument.template Examine<int>([&Out](auto const &Value) 
		{ 
			ProtocolOperations<
				typename std::remove_const<
					typename std::remove_reference<
						decltype(Value)>::type>::type>::Write(Out, Value); 
			return 0;
		});
	}

	static void Read(
		Protocol::VersionIDT const &VersionID,
		Protocol::MessageIDT const &MessageID,
		uint8_t const *Buffer,
		Protocol::SizeT const BufferSize,
		Protocol::SizeT &Offset,
		ValueT &Data)
	{
		VariantTagT Type;
		ProtocolOperations<VariantTagT>::Read(
			VersionID, 
			MessageID, 
			Buffer, 
			BufferSize,
			Offset, 
			Type);
		if (Type != VariantTagT(0u))
			if (!Data.template SetByTag<bool>(Type, [&](auto &Value) 
				{ 
					ProtocolOperations<typename std::remove_reference<decltype(Value)>::type>::Read(
						VersionID, 
						MessageID, 
						Buffer, 
						BufferSize,
						Offset, 
						Value); 
					return true;
				})) 
				throw SYSTEM_ERROR << "Unknown variant index " << (int)Type << " for variant " << typeid(ValueT).name();
	}
};

#endif
