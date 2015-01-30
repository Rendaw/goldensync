#include "log.h"

#include <chrono>

#include "../ren-cxx-basics/extrastandard.h"

LogT::~LogT(void) {}

BasicLogT::BasicLogT(std::string const &Name) : Name(Name) {}

BasicLogT::operator bool(void) const { return true; }

void BasicLogT::operator()(LevelT Level, std::string const &Message) const
{
	auto const Time = std::time(nullptr);
	auto const LocalTime = std::localtime(&Time);
	std::vector<char> Buffer(256);
	std::stringstream FormattedMessage;
	if (auto Wrote = strftime(&Buffer[0], Buffer.size(), "%F %T", LocalTime))
		FormattedMessage.write(&Buffer[0], Wrote);
	FormattedMessage << " (" << Name << "):" << Message;
	if (Level >= Error) std::cerr << FormattedMessage << std::endl;
	else std::cout << FormattedMessage << "\n";
}

