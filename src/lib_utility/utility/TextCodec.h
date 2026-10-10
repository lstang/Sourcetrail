#ifndef TEXT_CODEC_H
#define TEXT_CODEC_H

#include <memory>
#include <string>

#include <QStringConverter>
#include <QStringDecoder>
#include <QStringEncoder>

class TextCodec
{
public:
	TextCodec(const std::string& name);

	std::string getName() const;
	bool isValid() const;

	std::wstring decode(const std::string& unicodeString) const;

	std::string encode(const std::wstring& string) const;

private:
	const std::string m_name;
	mutable QStringDecoder m_decoder;
	mutable QStringEncoder m_encoder;
};

#endif	  // TEXT_CODEC_H
