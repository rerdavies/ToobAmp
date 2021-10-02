#pragma once

#include <string>
class Lv2Exception {

private:
	std::string _message;
public :
	static void Throw(const char* message) {
		throw Lv2Exception(message);
	}

	Lv2Exception(const char* message)
		: _message(message)
	{
	}
	const char* GetMessage()
	{
		return _message.c_str();
	}
};