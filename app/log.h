#ifndef log_h
#define log_h

#include <string>

struct LogT
{
	enum LevelT
	{
		Spam,
		Debug,
		Info,
		Warning,
		Error
	};

	virtual ~LogT(void);
	virtual operator bool(void) const = 0;
	virtual void operator()(LevelT Level, std::string const &Message) const = 0;
};

struct BasicLogT : LogT
{
	// TODO
	// automatically load config
	// disable by name
	// redirect to file w/ periodic rotation + trunctation?

	BasicLogT(std::string const &Name);
	operator bool(void) const;
	void operator()(LevelT Level, std::string const &Message) const;

	private:
		std::string const Name;
};

#endif

