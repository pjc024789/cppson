#pragma once
#include <string>
#include <map>
#include <sstream>
#include <fstream>
#include <functional>
#include <vector>
#ifndef OUT
#define OUT
#endif
#define JSON_CLASS(name) class name final : public cppson::Parsable<name>
#define FIELD(type, name) \
struct Init_ ## name \
{\
	Init_ ## name ## () \
	{ \
		meta[ #name ] = [](Type* t, cppson::JsonValue value) -> bool \
		{ \
			return value.parse(t->name); \
		}; \
	} \
};\
struct Meta_ ## name\
{\
	Meta_ ## name ## ()\
	{\
		static Init_ ## name init;\
	}\
};\
Meta_ ## name meta_ ## name;\
type name;


namespace cppson
{

class Single
{
public:
	template<typename R>
	R getValue()
	{
		static_assert(false, "invalid call.");
	}

	template<>
	int getValue<int>()
	{
		return atoi(str.c_str());
	}

	template<>
	bool getValue<bool>()
	{
		if (str == "true")
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	template<>
	double getValue<double>()
	{
		return atof(str.c_str());
	}

	template<>
	std::string getValue<std::string>()
	{
		return str;
	}

	std::string& getStr()
	{
		return str;
	}

private:
	std::string str;
};

class JsonValue
{
public:
	
	//if failed, return -1
	int init(std::vector<std::string>& tokens, int offset)
	{
		//object
		if (tokens[offset] == "{")
		{
			type = OBJECT;
			offset++;
			
			while (offset < tokens.size())
			{
				if (tokens[offset] == "\"")
				{
					std::string& key = tokens[offset + 1];

					if (tokens[offset + 2] != "\"" || tokens[offset + 3] != ":")
						return -1;

					offset = obj[key].init(tokens, offset + 4);

					if (offset == -1)
						return -1;
				}
				else if (tokens[offset] == ",")
				{
					offset++;
				}
				else if (tokens[offset] == "}")
				{
					return offset + 1;
				}
				else
				{
					return -1;
				}
			}
			return -1;
		}
		else if (tokens[offset] == "[")
		{
			type = ARRAY;
			offset++;
			while (offset < tokens.size())
			{
				if (tokens[offset] == ",")
				{
					offset++;
				}
				else if (tokens[offset] == "]")
				{
					return offset + 1;
				}
				else
				{
					JsonValue value;
					offset = value.init(tokens, offset);
					if (offset == -1)
						return -1;

					arr.push_back(value);
				}
			}
			return -1;
		}
		else
		{
			type = SINGLE;
			if (tokens[offset] == "\"")
			{
				single.getStr() = tokens[offset + 1];
				return offset + 3;
			}
			else
			{
				single.getStr() = tokens[offset];
				return offset + 1;
			}
		}
	}

	template<typename T>
	bool parse(T& val)
	{
		static_assert(std::is_base_of<Parsable<T>, T>::value, "T must be derived from Parsable<T>");

		if (type != OBJECT)
			return false;

		for (auto m : obj)
		{
			if (!T::getMeta().at(m.first)(&val, m.second))
			{
				return false;
			}
		}

		return true;
	}

	template<typename T>
	bool parse(std::vector<T>& val)
	{
		static_assert(std::is_base_of<Parsable<T>, T>::value ||
					std::is_same<int, T>::value ||
					std::is_same<double, T>::value ||
					std::is_same<std::string, T>::value ||
					std::is_same<bool, T>::value, "T must be derived from Parsable<T>, or T must be int or double or std::string or bool.");

		if (type != ARRAY)
			return false;

		for (auto v : arr)
		{
			T p;

			if (v.parse(p))
				val.push_back(p);
			else
				return false;
		}

		return true;
	}

	template<>
	bool parse<int>(int& val)
	{
		if (type != SINGLE)
			return false;

		val = single.getValue<int>();

		return true;
	}

	template<>
	bool parse<double>(double& val)
	{
		if (type != SINGLE)
			return false;

		val = single.getValue<double>();

		return true;
	}

	template<>
	bool parse<bool>(bool& val)
	{
		if (type != SINGLE)
			return false;

		val = single.getValue<bool>();

		return true;
	}

	template<>
	bool parse<std::string>(std::string& val)
	{
		if (type != SINGLE)
			return false;

		val = single.getValue<std::string>();

		return true;
	}

private:
	enum Type
	{
		NONE,
		SINGLE,
		OBJECT,
		ARRAY
	};

	Type type;
	Single single;
	std::map<std::string, JsonValue> obj;
	std::vector<JsonValue> arr;
};

bool tokenize(const std::string& str, OUT std::vector<std::string>& tokens)
{
	std::string token;
	std::string space = " \t\r\n";
	std::string delim = ":{}[],";
	std::string number = "0123456789.";
	int state = 0;

	for (size_t i = 0; i < str.size(); i++)
	{
		switch (state)
		{
		case 0:
			//ignore space
			if (space.find(str[i]) != std::string::npos)
			{
				if (token.size() != 0)
				{
					tokens.push_back(token);
					token.clear();
				}

				continue;
			}
			//go string
			else if (str[i] == '"')
			{
				if (token.size() != 0)
					return false;

				tokens.push_back({ str[i] });
				state = 1;
			}
			//one char token
			else if (delim.find(str[i]) != std::string::npos)
			{
				if (token.size() != 0)
				{
					tokens.push_back(token);
					token.clear();
				}
				tokens.push_back({ str[i] });
			}
			//number
			else if (number.find(str[i]) != std::string::npos)
			{
				token.push_back({ str[i] });
			}
			//true or false
			else if (str.substr(i, 4) == "true")
			{
				if (token.size() != 0)
					return false;

				tokens.push_back(str.substr(i, 4));
			}
			else if (str.substr(i, 5) == "false")
			{
				if (token.size() != 0)
					return false;

				tokens.push_back(str.substr(i, 5));
			}
			else
			{
				return false;
			}
			break;
		case 1:
			if (str[i] == '"')
			{
				tokens.push_back(token);
				tokens.push_back({ str[i] });
				token.clear();
				state = 0;
			}
			else
			{
				token.push_back({ str[i] });
			}
			break;
		}
	}

	return true;
}

template<typename T>
class Parsable
{
public:
	using Type = T;

	bool loadFile(const std::string& fileName)
	{
		Type& t = static_cast<Type&>(*this);
		return cppson::loadFile(t, fileName);
	}

	using Meta = std::map<std::string, std::function<bool(Type*, JsonValue)> >;

	static const Meta& getMeta()
	{
		return meta;
	}

protected:

	static Meta meta;
};


template<typename T>
bool loadFile(T& value, const std::string& fileName)
{
	std::ifstream file(fileName);

	if (!file.is_open())
		return false;

	std::string str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	
	return loadString(value, str);
}

template<typename T>
bool loadString(T& value, const std::string& str)
{
	std::vector<std::string> tokens;

	if (!tokenize(str, tokens))
	{
		return false;
	}

	JsonValue json;

	if (json.init(tokens, 0) == -1)
		return false;

	json.parse(value);
	return true;
}

template<typename T>
typename Parsable<T>::Meta Parsable<T>::meta;

}