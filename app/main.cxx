#include "core.h"

int main(int argc, char **argv)
{
	CoreT Core({"freg"}, Filesystem::PathT::Qualify("test-back"));
	return 0;
}

