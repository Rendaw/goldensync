#ifndef protocol_h
#define protocol_h

#include "protocoltypes.h"

// TODO Add conditional big endian -> little endian conversions

/*
Major versions are incompatible, and could be essentially different protocols.
 versions are compatible.

You may add minor versions to a protocol at any time, but you must never remove them.
Messages are completely redefined for each new version.
*/

#include "constcount.h"
//#include "../ren-cxx-basics/type.h"
#include "protocoloperations.h"

#include <vector>
#include <functional>
#include <limits>
#include <cassert>
#include <cstring>
#include <memory>
#include <type_traits>
#include <typeinfo>

#define DefineProtocol(Name) \
	typedef Protocol::Protocol<__COUNTER__> Name;
#define DefineProtocolVersion(Name, InProtocol) \
	typedef Protocol::Version<static_cast<Protocol::VersionIDT::Type>(GetConstCount(InProtocol)), InProtocol> Name; \
	IncrementConstCount(InProtocol)
#define DefineProtocolMessage(Name, InVersion, Signature) \
	typedef Protocol::Message<static_cast<Protocol::MessageIDT::Type>(GetConstCount(InVersion)), InVersion, Signature> Name; \
	IncrementConstCount(InVersion)

template <typename Type> 
	constexpr size_t ProtocolGetSize(
		Type const &Argument)
{ 
	return ProtocolOperations<Type>::GetSize(Argument); 
}

template <typename Type> 
	inline void ProtocolWrite(
		uint8_t *&Out, 
		Type const &Argument)
{ 
	return ProtocolOperations<Type>::Write(Out, Argument); 
}

template <typename Type> 
	bool ProtocolRead(
		Protocol::VersionIDT const &VersionID,
		Protocol::MessageIDT const &MessageID,
		uint8_t const *Buffer,
		Protocol::SizeT const BufferSize,
		Protocol::SizeT &Offset,
		Type &Data)
{ 
	return ProtocolOperations<Type>::Read(VersionID, MessageID, Buffer, BufferSize, Offset, Data); 
}

namespace Protocol
{

// Infrastructure
constexpr SizeT HeaderSize{SizeT::Type(
	VersionIDT::Size + 
	MessageIDT::Size + 
	SizeT::Size)};

template <size_t Individuality> 
	struct Protocol 
{
};

template <VersionIDT::Type IDValue, typename InProtocol> 
	struct Version
{ 
	static constexpr VersionIDT ID{IDValue}; 
};
template <VersionIDT::Type IDValue, typename InProtocol> 
	constexpr VersionIDT Version<IDValue, InProtocol>::ID;

template <MessageIDT::Type, typename, typename> 
	struct Message;

template <MessageIDT::Type IDValue, typename InVersion, typename ...Definition> 
	struct Message<IDValue, InVersion, void(Definition...)>
{
	typedef InVersion Version;
	typedef void Signature(Definition...);
	typedef std::function<void(Definition const &...)> FunctionT;
	static constexpr MessageIDT ID{IDValue};

	static std::vector<uint8_t> Write(Definition const &...Arguments)
	{
		std::vector<uint8_t> Out;
		auto RequiredSize = Size(Arguments...);
		if (RequiredSize > std::numeric_limits<SizeT::Type>::max())
			{ assert(false); return Out; }
		Out.resize(StrictCast(HeaderSize, size_t) + RequiredSize);
		uint8_t *WritePointer = &Out[0];
		ProtocolWrite(WritePointer, InVersion::ID);
		ProtocolWrite(WritePointer, ID);
		ProtocolWrite(WritePointer, (SizeT::Type)RequiredSize);
		Write(WritePointer, Arguments...);
		return Out;
	}

	private:
		template <typename NextType, typename... RemainingTypes>
			static inline size_t Size(
				NextType NextArgument, 
				RemainingTypes... RemainingArguments)
		{ 
			return 
				ProtocolGetSize(NextArgument) + 
				Size(RemainingArguments...); 
		}

		static constexpr size_t Size(void) { return {0}; }

		template <typename NextT, typename... RemainingT>
			static inline void Write(
				uint8_t *&Out, 
				NextT Next, 
				RemainingT... Remaining)
			{
				ProtocolWrite(Out, Next);
				Write(Out, std::forward<RemainingT>(Remaining)...);
			}

		static inline void Write(uint8_t *&) {}

};
template 
<
	MessageIDT::Type IDValue, 
	typename InVersion, 
	typename ...Definition
> 
	constexpr MessageIDT Message<IDValue, InVersion, void(Definition...)>::ID;

// Deserialization
template 
<
	VersionIDT::Type CurrentVersionID,
	MessageIDT::Type CurrentMessageID,
	typename Enabled,
	typename ...MessageTypes
> 
	struct ReaderTupleElement;

template
<
	VersionIDT::Type CurrentVersionID,
	MessageIDT::Type CurrentMessageID,
	typename MessageType,
	typename ...RemainingMessageTypes
>
	struct ReaderTupleElement
	<
		CurrentVersionID,
		CurrentMessageID,
		typename std::enable_if
		<
			(CurrentVersionID == *MessageType::Message::Version::ID) && 
			(CurrentMessageID == *MessageType::Message::ID)
		>::type,
		MessageType, 
		RemainingMessageTypes...
	>
		: ReaderTupleElement
		<
			CurrentVersionID,
			CurrentMessageID + 1,
			void,
			RemainingMessageTypes...
		>
{
	private:
		template <typename ParentType = MessageType> struct MessageDerivedTypes;
		template <MessageIDT::Type ID, typename InVersion, typename ...Definition>
			struct MessageDerivedTypes<Message<ID, InVersion, void(Definition...)>>
		{
			typedef std::tuple<Definition...> Tuple;
		};

		typedef ReaderTupleElement
		<
			CurrentVersionID,
			CurrentMessageID + 1,
			void,
			RemainingMessageTypes...
		> NextElement;

	protected:
		template <typename HandlerT, typename... ExtraT>
			bool Read(
				HandlerT &Handler,
				VersionIDT const &VersionID,
				MessageIDT const &MessageID,
				uint8_t const *Buffer,
				SizeT BufferSize,
				ExtraT const &... Extra)
		{
			if ((VersionID == MessageType::Version::ID) && (MessageID == MessageType::ID))
			{
				SizeT Offset{(SizeT::Type)0};
				return ReadImplementation
				<
					HandlerT, 
					typename MessageDerivedTypes<>::Tuple, 
					std::tuple<ExtraT...>
				>::Read(
					Handler, 
					VersionID, 
					MessageID, 
					Buffer, 
					BufferSize,
					Offset, 
					std::forward<ExtraT const &>(Extra)...);
			}
			return NextElement::Read(
				Handler, 
				VersionID, 
				MessageID, 
				Buffer, 
				BufferSize,
				std::forward<ExtraT const &>(Extra)...);
		}

	private:
		template 
		<
			typename HandlerT, 
			typename UnreadTypes, 
			typename ReadTypes
		> 
			struct ReadImplementation 
		{
		};

		template 
		<
			typename HandlerT, 
			typename NextType, 
			typename... RemainingTypes, 
			typename... ReadTypes
		>
			struct ReadImplementation
			<
				HandlerT, 
				std::tuple<NextType, RemainingTypes...>, 
				std::tuple<ReadTypes...>
			>
		{
			static bool Read(
				HandlerT &Handler,
				VersionIDT const &VersionID,
				MessageIDT const &MessageID,
				uint8_t const *Buffer,
				SizeT const BufferSize,
				SizeT &Offset,
				ReadTypes const &...ReadData)
			{
				NextType Data;
				if (!ProtocolRead(
					VersionID, 
					MessageID,
					Buffer,
					BufferSize,
					Offset,
					Data)) return false;
				return ReadImplementation
				<
					HandlerT, 
					std::tuple<RemainingTypes...>, 
					std::tuple<ReadTypes..., NextType>
				>::Read(
					Handler,
					VersionID,
					MessageID,
					Buffer,
					BufferSize,
					Offset,
					std::forward<ReadTypes const &>(ReadData)...,
					std::move(Data));
			}
		};

		template 
		<
			typename HandlerT, 
			typename... ReadTypes
		>
			struct ReadImplementation
			<
				HandlerT, 
				std::tuple<>, 
				std::tuple<ReadTypes...>
			>
		{
			static bool Read(
				HandlerT &Handler,
				VersionIDT const &VersionID,
				MessageIDT const &MessageID,
				uint8_t const *Buffer,
				SizeT const BufferSize,
				SizeT &Offset,
				ReadTypes const &...ReadData)
			{
				Handler.Handle(
					MessageType{}, 
					std::forward<ReadTypes const &>(ReadData)...);
				return true;
			}
		};
};

template
<
	VersionIDT::Type CurrentVersionID,
	MessageIDT::Type CurrentMessageID,
	typename MessageType,
	typename ...RemainingMessageTypes
>
	struct ReaderTupleElement
	<
		CurrentVersionID,
		CurrentMessageID,
		typename std::enable_if<(CurrentVersionID + 1 == *MessageType::Message::Version::ID)>::type,
		MessageType, 
		RemainingMessageTypes...
	>
		: ReaderTupleElement
		<
			CurrentVersionID + 1,
			0,
			void,
			MessageType, 
			RemainingMessageTypes...
		>
{
	typedef ReaderTupleElement
	<
		CurrentVersionID + 1,
		0,
		void,
		MessageType, 
		RemainingMessageTypes...
	> NextElement;
};

template
<
	VersionIDT::Type CurrentVersionID,
	MessageIDT::Type CurrentMessageID
>
struct ReaderTupleElement
<
	CurrentVersionID,
	CurrentMessageID,
	void
>
{
	template <typename HandlerT> 
		bool Read(
			HandlerT &Handler,
			VersionIDT const &VersionID,
			MessageIDT const &MessageID,
			uint8_t const *Buffer,
			SizeT const BufferSize)
	{
		assert(false);
		return false;
	}
};

template <typename ...MessageTypes> 
	struct ReaderT : ReaderTupleElement<0, 0, void, MessageTypes...>
{
	template <typename StreamT, typename HandlerT, typename... ExtraT> 
		bool Read(
			StreamT &&Stream,
			HandlerT &Handler,
			ExtraT const ...Extra)
	{
		while (true)
		{
			auto Header = Stream.FilledStart(StrictCast(HeaderSize, size_t));
			if (!Header) return true;
			VersionIDT const VersionID = *reinterpret_cast<VersionIDT *>(&Header[0]);
			MessageIDT const MessageID = *reinterpret_cast<MessageIDT *>(&Header[VersionIDT::Size]);
			SizeT const DataSize = *reinterpret_cast<SizeT *>(&Header[VersionIDT::Size + MessageIDT::Size]);

			auto Body = Stream.FilledStart(StrictCast(DataSize, size_t), StrictCast(HeaderSize, size_t));
			if ((DataSize > SizeT(0)) && !Body) return true;

			auto Success = HeadElement::Read(
				Handler, 
				VersionID, 
				MessageID, 
				Body, 
				DataSize, 
				Extra...);
			if (!Success) return false;

			Stream.Consume(StrictCast(HeaderSize + DataSize, size_t));
		}
	}

	private:
		typedef ReaderTupleElement<0, 0, void, MessageTypes...> HeadElement;
};

}

#endif
