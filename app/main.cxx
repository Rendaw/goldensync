#include "core.h"

int main(int argc, char **argv)
{
	try
	{
		CoreT Core({"freg"}, Filesystem::PathT::Qualify("test-back"));
	}
	catch (SystemErrorT const &Error)
	{
		std::cerr << "Unhandled error:\n" << Error << std::endl;
		return 1;
	}
	return 0;
}

