#include <cstdint>
#include <iomanip>
#include <limits>
#include "../../ren-cxx-basics/extrastandard.h"
#include "../../ren-cxx-basics/variant.h"
#include "../protocol/protocol.h"

struct BufferStream
{
	std::vector<uint8_t> &Buffer;
	size_t Offset;
	bool Dead;
	BufferStream(std::vector<uint8_t> &Buffer) : Buffer(Buffer), Offset(0), Dead(false) {}

	size_t Filled(void) const
	{
		return Buffer.size() - Offset;
	}

	uint8_t const *FilledStart(size_t Size, size_t PlusOffset = 0) const
	{
		if (Offset + PlusOffset + Size > Buffer.size()) return nullptr;
		return &Buffer[Offset + PlusOffset];
	}
	void Consume(size_t Size)
	{
		AssertLTE(Offset + Size, Buffer.size());
		Offset += Size;
	}

	//bool operator!(void) const { return Dead; }
};

struct TestTupleT : std::tuple<int, bool, int>
{
	typedef std::tuple<int, bool, int> TupleT;
	using TupleT::TupleT;
};
template <> struct IsTuply<TestTupleT> 
	{ static constexpr bool Result = true; };

DefineProtocol(Proto1)
DefineProtocolVersion(Proto1_1, Proto1)
DefineProtocolMessage(Proto1_1_1, Proto1_1, void(int Val))
DefineProtocolMessage(Proto1_1_1b, Proto1_1, void(int Val))
DefineProtocolMessage(Proto1_1_2, Proto1_1, void(unsigned int Val))
DefineProtocolMessage(Proto1_1_3, Proto1_1, void(uint64_t Val))
DefineProtocolMessage(Proto1_1_4, Proto1_1, void(bool Val))
DefineProtocolMessage(Proto1_1_5, Proto1_1, void(std::string Val))
DefineProtocolMessage(Proto1_1_6, Proto1_1, void(std::vector<uint8_t> Val))
DefineProtocolMessage(Proto1_1_6b, Proto1_1, void(std::vector<uint8_t> Val))
DefineProtocolMessage(Proto1_1_7, Proto1_1, void(OptionalT<int> Val))
DefineProtocolMessage(Proto1_1_7b, Proto1_1, void(OptionalT<int> Val))
DefineProtocolMessage(Proto1_1_8, Proto1_1, void(TestTupleT Val))
DefineProtocolVersion(Proto1_2, Proto1)
DefineProtocolMessage(Proto1_2_1, Proto1_2, void(bool Space, int Val))
DefineProtocolMessage(Proto1_2_1b, Proto1_2, void(bool Space, int Val))
DefineProtocolMessage(Proto1_2_2, Proto1_2, void(bool Space, unsigned int Val))
DefineProtocolMessage(Proto1_2_3, Proto1_2, void(bool Space, uint64_t Val))
DefineProtocolMessage(Proto1_2_4, Proto1_2, void(bool Space, bool Val))
DefineProtocolMessage(Proto1_2_5, Proto1_2, void(bool Space, std::string Val))
DefineProtocolMessage(Proto1_2_6, Proto1_2, void(bool Space, std::vector<uint8_t> Val))
DefineProtocolMessage(Proto1_2_6b, Proto1_2, void(bool Space, std::vector<uint8_t> Val))
DefineProtocolMessage(Proto1_2_7, Proto1_2, void(bool Space, OptionalT<int> Val))
DefineProtocolMessage(Proto1_2_7b, Proto1_2, void(bool Space, OptionalT<int> Val))
DefineProtocolMessage(Proto1_2_8, Proto1_2, void(bool Space, TestTupleT Val))

DefineProtocol(Proto2)
DefineProtocolVersion(Proto2_1, Proto2)
DefineProtocolMessage(Proto2_1_1, Proto2_1, void(int Val))

template<size_t Value> 
	struct Overflow : 
		std::integral_constant<size_t, Value + std::numeric_limits<unsigned char>::max() + 1> {};
static_assert(Proto1_1::ID == (Protocol::VersionIDT::Type)0, "ID calculation failed");
static_assert(Proto1_1_1::ID == (Protocol::MessageIDT::Type)0, "ID calculation failed");
static_assert(Proto1_1_1b::ID == (Protocol::MessageIDT::Type)1, "ID calculation failed");
static_assert(Proto1_1_2::ID == (Protocol::MessageIDT::Type)2, "ID calculation failed");
static_assert(Proto1_1_3::ID == (Protocol::MessageIDT::Type)3, "ID calculation failed");
static_assert(Proto1_1_4::ID == (Protocol::MessageIDT::Type)4, "ID calculation failed");
static_assert(Proto1_1_5::ID == (Protocol::MessageIDT::Type)5, "ID calculation failed");
static_assert(Proto1_1_6::ID == (Protocol::MessageIDT::Type)6, "ID calculation failed");
static_assert(Proto1_1_6b::ID == (Protocol::MessageIDT::Type)7, "ID calculation failed");
static_assert(Proto1_1_7::ID == (Protocol::MessageIDT::Type)8, "ID calculation failed");
static_assert(Proto1_1_7b::ID == (Protocol::MessageIDT::Type)9, "ID calculation failed");
static_assert(Proto1_1_8::ID == (Protocol::MessageIDT::Type)10, "ID calculation failed");
static_assert(Proto1_2::ID == (Protocol::VersionIDT::Type)1, "ID calculation failed");
static_assert(Proto1_2_1::ID == (Protocol::MessageIDT::Type)0, "ID calculation failed");
static_assert(Proto1_2_1b::ID == (Protocol::MessageIDT::Type)1, "ID calculation failed");
static_assert(Proto1_2_2::ID == (Protocol::MessageIDT::Type)2, "ID calculation failed");
static_assert(Proto1_2_3::ID == (Protocol::MessageIDT::Type)3, "ID calculation failed");
static_assert(Proto1_2_4::ID == (Protocol::MessageIDT::Type)4, "ID calculation failed");
static_assert(Proto1_2_5::ID == (Protocol::MessageIDT::Type)5, "ID calculation failed");
static_assert(Proto1_2_6::ID == (Protocol::MessageIDT::Type)6, "ID calculation failed");
static_assert(Proto1_2_6b::ID == (Protocol::MessageIDT::Type)7, "ID calculation failed");
static_assert(Proto1_2_7::ID == (Protocol::MessageIDT::Type)8, "ID calculation failed");
static_assert(Proto1_2_7b::ID == (Protocol::MessageIDT::Type)9, "ID calculation failed");
static_assert(Proto1_2_8::ID == (Protocol::MessageIDT::Type)10, "ID calculation failed");

int main(int argc, char **argv)
{
	try
	{
		std::vector<uint8_t> Buffer;

		struct Handler1T
		{
			void Handle(Proto1_1_1, int const &Val) { AssertE(Val, 11); }
			void Handle(Proto1_1_1b, int const &Val) { AssertE(Val, -4); }
			void Handle(Proto1_1_2, unsigned int const &Val) { AssertE(Val, 11u); }
			void Handle(Proto1_1_3, uint64_t const &Val) { AssertE(Val, 11u); }
			void Handle(Proto1_1_4, bool const &Val) { AssertE(Val, true); }
			void Handle(Proto1_1_5, std::string const &Val) { AssertE(Val, "dog"); }
			void Handle(Proto1_1_6, std::vector<uint8_t> const &Val)
			{
				AssertE(Val.size(), 3u);
				AssertE(Val[0], 0x00);
				AssertE(Val[1], 0x01);
				AssertE(Val[2], 0x02);
			}
			
			void Handle(Proto1_1_6b, std::vector<uint8_t> const &Val)
			{
				AssertE(Val.size(), 0u);
			}
			
			void Handle(Proto1_1_7, OptionalT<int> const &Val) { AssertE(*Val, 11); }
			void Handle(Proto1_1_7b, OptionalT<int> const &Val) { Assert(!Val); }
			void Handle(Proto1_1_8, TestTupleT const &Val) 
			{ 
				AssertE(std::get<0>(Val), 11);
				AssertE(std::get<1>(Val), false);
				AssertE(std::get<2>(Val), 3);
			}

			// Version 2
			void Handle(Proto1_2_1, bool const &Space, int const &Val) { AssertE(Space, true); AssertE(Val, 11); }
			void Handle(Proto1_2_1b, bool const &Space, int const &Val) { AssertE(Space, true); AssertE(Val, -4); }
			void Handle(Proto1_2_2, bool const &Space, unsigned int const &Val) { AssertE(Space, true); AssertE(Val, 11u); }
			void Handle(Proto1_2_3, bool const &Space, uint64_t const &Val) { AssertE(Space, true); AssertE(Val, 11u); }
			void Handle(Proto1_2_4, bool const &Space, bool const &Val) { AssertE(Space, true); AssertE(Val, true); }
			void Handle(Proto1_2_5, bool const &Space, std::string const &Val) { AssertE(Space, true); AssertE(Val, "dog"); }
			void Handle(Proto1_2_6, bool const &Space, std::vector<uint8_t> const &Val)
			{
				AssertE(Space, true);
				AssertE(Val.size(), 3u);
				AssertE(Val[0], 0x00);
				AssertE(Val[1], 0x01);
				AssertE(Val[2], 0x02);
			}
			
			void Handle(Proto1_2_6b, bool const &Space, std::vector<uint8_t> const &Val)
			{
				AssertE(Val.size(), 0u);
			}
			
			void Handle(Proto1_2_7, bool const &Space, OptionalT<int> const &Val) { AssertE(*Val, 11); }
			void Handle(Proto1_2_7b, bool const &Space, OptionalT<int> const &Val) { Assert(!Val); }
			
			void Handle(Proto1_2_8, bool const &Space, TestTupleT const &Val) 
			{ 
				AssertE(std::get<0>(Val), 11);
				AssertE(std::get<1>(Val), false);
				AssertE(std::get<2>(Val), 3);
			}
		} Handler1;
		Protocol::ReaderT
		<
			Proto1_1_1, 
			Proto1_1_1b, 
			Proto1_1_2, 
			Proto1_1_3, 
			Proto1_1_4,
			Proto1_1_5,
			Proto1_1_6,
			Proto1_1_6b,
			Proto1_1_7,
			Proto1_1_7b,
			Proto1_1_8,

			Proto1_2_1,
			Proto1_2_1b,
			Proto1_2_2,
			Proto1_2_3,
			Proto1_2_4,
			Proto1_2_5,
			Proto1_2_6,
			Proto1_2_6b,
			Proto1_2_7,
			Proto1_2_7b,
			Proto1_2_8
		>
		Reader1;

		// Proto 1
		Buffer = Proto1_1_1::Write(11);
		AssertE(Buffer, std::vector<uint8_t>({0x00, 0x00, 0x04, 0x00, 0x0b, 0x00, 0x00, 0x00}));
		Reader1.Read(BufferStream{Buffer}, Handler1);
		Buffer = Proto1_1_1b::Write(-4);
		AssertE(Buffer, std::vector<uint8_t>({0x00, 0x01, 0x04, 0x00, 0xFC, 0xFF, 0xFF, 0xFF}));
		Reader1.Read(BufferStream{Buffer}, Handler1);

		Buffer = Proto1_1_2::Write(11);
		AssertE(Buffer, std::vector<uint8_t>({0x00, 0x02, 0x04, 0x00, 0x0b, 0x00, 0x00, 0x00}));
		Reader1.Read(BufferStream{Buffer}, Handler1);

		Buffer = Proto1_1_3::Write(11);
		AssertE(Buffer, std::vector<uint8_t>({0x00, 0x03, 0x08, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));
		Reader1.Read(BufferStream{Buffer}, Handler1);

		Buffer = Proto1_1_4::Write(true);
		AssertE(Buffer, std::vector<uint8_t>({0x00, 0x04, 0x01, 0x00, 0x01}));
		Reader1.Read(BufferStream{Buffer}, Handler1);

		Buffer = Proto1_1_5::Write("dog");
		AssertE(Buffer, std::vector<uint8_t>({0x00, 0x05, 0x05, 0x00, 0x03, 0x00, 'd', 'o', 'g'}));
		Reader1.Read(BufferStream{Buffer}, Handler1);

		Buffer = Proto1_1_6::Write(std::vector<uint8_t>{0x00, 0x01, 0x02});
		AssertE(Buffer, std::vector<uint8_t>({0x00, 0x06, 0x05, 0x00, 0x03, 0x00, 0x00, 0x01, 0x02}));
		Reader1.Read(BufferStream{Buffer}, Handler1);

		Buffer = Proto1_1_6b::Write(std::vector<uint8_t>());
		AssertE(Buffer, std::vector<uint8_t>({0x00, 0x07, 0x02, 0x00, 0x00, 0x00}));
		Reader1.Read(BufferStream{Buffer}, Handler1);
		
		Buffer = Proto1_1_7::Write(11);
		AssertE(Buffer, std::vector<uint8_t>({0x00, 0x08, 0x05, 0x00, 0x01, 0x0b, 0x00, 0x00, 0x00}));
		Reader1.Read(BufferStream{Buffer}, Handler1);
		Buffer = Proto1_1_7b::Write({});
		AssertE(Buffer, std::vector<uint8_t>({0x00, 0x09, 0x01, 0x00, 0x00}));
		Reader1.Read(BufferStream{Buffer}, Handler1);
		
		AssertE(ProtocolOperations<TestTupleT>::GetSize(TestTupleT{11, false, 3}), 9u);
		Buffer = Proto1_1_8::Write(TestTupleT{11, false, 3});
		AssertE(Buffer, std::vector<uint8_t>({0x00, 0x0a, 0x09, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00}));
		Reader1.Read(BufferStream{Buffer}, Handler1);

		// Proto 1_2
		Buffer = Proto1_2_1::Write(true, 11);
		AssertE(Buffer, std::vector<uint8_t>({0x01, 0x00, 0x05, 0x00, 0x01, 0x0b, 0x00, 0x00, 0x00}));
		Reader1.Read(BufferStream{Buffer}, Handler1);
		Buffer = Proto1_2_1b::Write(true, -4);
		AssertE(Buffer, std::vector<uint8_t>({0x01, 0x01, 0x05, 0x00, 0x01, 0xFC, 0xFF, 0xFF, 0xFF}));
		Reader1.Read(BufferStream{Buffer}, Handler1);

		Buffer = Proto1_2_2::Write(true, 11);
		AssertE(Buffer, std::vector<uint8_t>({0x01, 0x02, 0x05, 0x00, 0x01, 0x0b, 0x00, 0x00, 0x00}));
		Reader1.Read(BufferStream{Buffer}, Handler1);

		Buffer = Proto1_2_3::Write(true, 11);
		AssertE(Buffer, std::vector<uint8_t>({0x01, 0x03, 0x09, 0x00, 0x01, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));
		Reader1.Read(BufferStream{Buffer}, Handler1);

		Buffer = Proto1_2_4::Write(true, true);
		AssertE(Buffer, std::vector<uint8_t>({0x01, 0x04, 0x02, 0x00, 0x01, 0x01}));
		Reader1.Read(BufferStream{Buffer}, Handler1);

		Buffer = Proto1_2_5::Write(true, "dog");
		AssertE(Buffer, std::vector<uint8_t>({0x01, 0x05, 0x06, 0x00, 0x01, 0x03, 0x00, 'd', 'o', 'g'}));
		Reader1.Read(BufferStream{Buffer}, Handler1);

		Buffer = Proto1_2_6::Write(true, std::vector<uint8_t>{0x00, 0x01, 0x02});
		AssertE(Buffer, std::vector<uint8_t>({0x01, 0x06, 0x06, 0x00, 0x01, 0x03, 0x00, 0x00, 0x01, 0x02}));
		Reader1.Read(BufferStream{Buffer}, Handler1);

		Buffer = Proto1_2_6b::Write(true, std::vector<uint8_t>());
		AssertE(Buffer, std::vector<uint8_t>({0x01, 0x07, 0x03, 0x00, 0x01, 0x00, 0x00}));
		Reader1.Read(BufferStream{Buffer}, Handler1);
		
		Buffer = Proto1_2_7::Write(true, 11);
		AssertE(Buffer, std::vector<uint8_t>({0x01, 0x08, 0x06, 0x00, 0x01, 0x01, 0x0b, 0x00, 0x00, 0x00}));
		Reader1.Read(BufferStream{Buffer}, Handler1);
		Buffer = Proto1_2_7b::Write(true, {});
		AssertE(Buffer, std::vector<uint8_t>({0x01, 0x09, 0x02, 0x00, 0x01, 0x00}));
		Reader1.Read(BufferStream{Buffer}, Handler1);
		
		Buffer = Proto1_2_8::Write(true, TestTupleT(11, false, 3));
		AssertE(Buffer, std::vector<uint8_t>({0x01, 0x0a, 0x0a, 0x00, 0x01, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00}));
		Reader1.Read(BufferStream{Buffer}, Handler1);

		// Proto 2_1
		int Mutate = 0;
		Buffer = Proto2_1_1::Write(45);
		struct Handler2T
		{
			int &Mutate;
			Handler2T(int &Mutate) : Mutate(Mutate) {}
			void Handle(Proto2_1_1, int const &Val) { Mutate = Val; }
		} Handler2(Mutate);
		Protocol::ReaderT<Proto2_1_1> Reader2;
		Reader2.Read(BufferStream{Buffer}, Handler2);
		AssertE(Mutate, 45);
	}
	catch (SystemErrorT &Error)
	{
		std::cout << Error << std::endl;
		return 1;
	}
	catch (AssertionErrorT &Error)
	{
		std::cout << Error << std::endl;
		return 1;
	}
	catch (ConstructionErrorT &Error)
	{
		std::cout << Error << std::endl;
		return 1;
	}
	catch (...)
	{
		std::cout << "Unknown error" << std::endl;
		return 1;
	}

	return 0;
}

