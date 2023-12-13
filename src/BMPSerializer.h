#pragma once

#include <string>
#include "ImageRawData.h"

namespace ProTerGen
{
	namespace BMP
	{
		bool ReadFileBMP_A(const std::string& filename, std::string& error, FileRawData& frd);
		bool ReadFileBMP_W(const std::wstring& filename, std::string& error, FileRawData& frd);
		ImageRawData DeserializeBMP_A(const std::string& filename, std::string& error);
		ImageRawData DeserializeBMP_W(const std::wstring& filename, std::string& error);
		bool SerializeBMP_A(const std::string& filename, const ImageRawData& image, std::string& error);
		bool SerializeBMP_W(const std::wstring& filename, const ImageRawData& image, std::string& error);

		bool Invert3Channels(void* data, size_t bufferSize);
	}
}