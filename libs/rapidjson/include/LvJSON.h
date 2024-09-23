#ifndef LVJSON_H
#define LVJSON_H

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/writer.h"


class LvJSON : public rapidjson::Document
{
public:
	std::string stringify()
	{
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		this->Accept(writer);
		return buffer.GetString();
	}
	std::string stringify_pretty()
	{
		rapidjson::StringBuffer pbuffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> pwriter(pbuffer);
		this->Accept(pwriter);
		return pbuffer.GetString();
	}

	enum ParseType {
		Int,
		Double,
		Bool,
		String,
		Array,
		Object
	};

	typedef rapidjson::SizeType SizeType;
	typedef rapidjson::Value Value;

	static int checkType(const rapidjson::Value& val, const char* key, ParseType pt, const int range = 0) {
		rapidjson::Document tempDoc;
		tempDoc.CopyFrom(val, tempDoc.GetAllocator());
		return checkType(tempDoc, key, pt, range);
	}

	static int checkType(const rapidjson::Document& doc, const char* key, ParseType pt, const int range = 0) {
		if (!doc.HasMember(key))
		{
			std::string err = "Property \"" + std::string(key) + "\" Not Found";
			throw err;
		}
		switch (pt) {
		case Int:
			if (!doc[key].IsInt()) {
				std::string err = "Property \"" + std::string(key) + "\" must be of type Int";
				throw err;
			}
			else if (range && (doc[key].GetInt() < 0 || doc[key].GetInt() >= range)) {
				std::string err = "Property \"" + std::string(key) + "\" must be from 0 to " + std::to_string(range - 1);
				throw err;
			}
			break;
		case Double:
			if (!doc[key].IsDouble() && !doc[key].IsInt()) {
				std::string err = "Property \"" + std::string(key) + "\" must be of type Double";
				throw err;
			}
			break;
		case Bool:
			if (!doc[key].IsBool()) {
				std::string err = "Property \"" + std::string(key) + "\" must be of type Bool";
				throw err;
			}
			break;
		case String:
			if (!doc[key].IsString()) {
				std::string err = "Property \"" + std::string(key) + "\" must be of type String";
				throw err;
			}
			break;
		case Array:
			if (!doc[key].IsArray()) {
				std::string err = "Property \"" + std::string(key) + "\" must be of type Array";
				throw err;
			}
			break;
		case Object:
			if (!doc[key].IsObject()) {
				std::string err = "Property \"" + std::string(key) + "\" must be of type Object";
				throw err;
			}
			break;
		}
		return 0;
	}
private:
};

#endif // LVJSON_H