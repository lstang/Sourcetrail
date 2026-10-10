#include "TextCodec.h"

TextCodec::TextCodec(const std::string& name)
	: m_name(name)
	, m_decoder(name.c_str())
	, m_encoder(name.c_str())
{
}

std::string TextCodec::getName() const
{
	return m_name;
}

bool TextCodec::isValid() const
{
	return m_decoder.isValid() && m_encoder.isValid();
}

std::wstring TextCodec::decode(const std::string& unicodeString) const
{
	if (m_decoder.isValid())
	{
		const QString decoded = m_decoder(QByteArrayView(unicodeString.data(), unicodeString.size()));
		return decoded.toStdWString();
	}
	return QString::fromStdString(unicodeString).toStdWString();
}

std::string TextCodec::encode(const std::wstring& string) const
{
	if (m_encoder.isValid())
	{
		QByteArray encoded = m_encoder(QStringView(QString::fromStdWString(string)));
		return std::string(encoded.constData(), encoded.size());
	}
	return QString::fromStdWString(string).toStdString();
}
