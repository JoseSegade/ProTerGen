#include "BMPSerializer.h"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long int u32;
typedef unsigned long long int u64;
typedef void* data_ptr;
typedef u8* byte_ptr;

struct Color
{
	u8 R;
	u8 G;
	u8 B;
};

struct Header
{
	u16 Type;
	u32 Size;
	u16 Reserved1;
	u16 Reserved2;
	u32 Offset;
};

struct Info
{
	u32 Size;
	u32 Width;
	u32 Height;
	u16 Planes;
	u16 BitCount;
	u32 Compression;
	u32 SizeImage;
	u32 XPixelsPerMeter;
	u32 YPixelsPerMeter;
	u32 ColorsUsed;
	u32 ColorsImportant;
};

enum Compression
{
	RGB = 0,
	BIT8 = 1,
	BIT4 = 2,
	MASK = 3
};

u8 Read_u8(FILE* file) noexcept
{
	return getc(file);
}

u16 Read_u16(FILE* file) noexcept
{
	const u8 b0 = getc(file);
	const u8 b1 = getc(file);

	return ((b1 << 8) | b0);
}

u32 Read_u32(FILE* file) noexcept
{
	const u8 b0 = getc(file);
	const u8 b1 = getc(file);
	const u8 b2 = getc(file);
	const u8 b3 = getc(file);

	return (((((b3 << 8) | b2) << 8) | b1) << 8) | b0;
}

void Write_u8(u8 value, FILE* file) noexcept
{
	putc(value, file);
}

void Write_u16(u16 value, FILE* file) noexcept
{
	putc(value >> 0, file);
	putc(value >> 8, file);
}

void Write_u32(u32 value, FILE* file) noexcept
{
	putc(value >> 0, file);
	putc(value >> 8, file);
	putc(value >> 16, file);
	putc(value >> 24, file);
}

const u16 BITMAP_TYPE = 0x4d42; // 'B' + 'M' joined in 2 bytes.

struct FileIntro
{
	Header header = {};
	Info info = {};
};

bool OpenFileW(FILE*& file, const std::wstring& filename, std::string& error, const std::wstring& mode)
{
	file = _wfopen(filename.c_str(), mode.c_str());
	//_wfopen_s(&file, filename.c_str(), mode.c_str());
	if (!(file))
	{
		error = "Could not open file.";
		return false;
	}
	return true;
}

bool OpenFileA(FILE*& file, const std::string& filename, std::string& error, const std::string& mode)
{
	file = fopen(filename.c_str(), mode.c_str());
	//fopen_s(&file, filename.c_str(), mode.c_str());
	if (!file)
	{
		error = "Could not open file.";
		return false;
	}
	return true;
}

bool ReadFileIntro(FileIntro& fi, FILE* file, std::string& error)
{
	fi.header =
	{
		.Type = Read_u16(file),
		.Size = Read_u32(file),
		.Reserved1 = Read_u16(file),
		.Reserved2 = Read_u16(file),
		.Offset = Read_u32(file)
	};

	if (fi.header.Type != BITMAP_TYPE)
	{
		fclose(file);
		error = "Wrong file encoding";
		return false;
	}

	const u32 infoSize = (u64)fi.header.Offset - 14;
	fi.info =
	{
		.Size = Read_u32(file),
		.Width = Read_u32(file),
		.Height = Read_u32(file),
		.Planes = Read_u16(file),
		.BitCount = Read_u16(file),
		.Compression = Read_u32(file),
		.SizeImage = Read_u32(file),
		.XPixelsPerMeter = Read_u32(file),
		.YPixelsPerMeter = Read_u32(file),
		.ColorsUsed = Read_u32(file),
		.ColorsImportant = Read_u32(file)
	};

	if (infoSize > 40)
	{
		const u32 colorsSize = infoSize - 40;
		void* colors = malloc(colorsSize);
		if (colors)
		{
			if (fread_s(colors, colorsSize, colorsSize, 1, file) < 1)
			{
				fclose(file);
				error = "Couldn't read color definition.";
				return false;
			}
		}
		else
		{
			fclose(file);
			error = "Ran out of memory.";
			return false;
		}
	}

	return true;
}

bool __ReadFileBMP(ProTerGen::FileRawData& frd, std::string& error)
{
	FileIntro fi = {};
	if (!ReadFileIntro(fi, frd.File, error))
	{
		return false;
	}
	assert(fi.info.SizeImage == fi.info.Width * fi.info.Height * (fi.info.BitCount / 8));
		
	frd.Width = fi.info.Width;
	frd.Height = fi.info.Height; 
	frd.FormatSize = (fi.info.BitCount / 8u); 
	frd.Offset = (int)fi.header.Offset;

	return true;
}

bool ProTerGen::BMP::ReadFileBMP_W(const std::wstring& filename, std::string& error, ProTerGen::FileRawData& frd)
{
	
	if (!OpenFileW(frd.File, filename, error, L"rb+"))
	{
		return false;
	}
	
	return __ReadFileBMP(frd, error);
}

bool ProTerGen::BMP::ReadFileBMP_A(const std::string& filename, std::string& error, ProTerGen::FileRawData& frd)
{
	if (!OpenFileA(frd.File, filename, error, "rb+"))
	{
		return false;
	}

	return __ReadFileBMP(frd, error);
}

ProTerGen::ImageRawData __DeserializeBMP(FILE* file, std::string& error)
{
	ProTerGen::ImageRawData img = {};

	FileIntro fi = {};
	if (!ReadFileIntro(fi, file, error))
	{
		return img;
	}
	
	Header& header = fi.header;
	Info& info = fi.info;

	const u64 byteSize = info.SizeImage != 0 ? info.SizeImage :
		(((u64)info.Width * info.BitCount + 7) / 8) * info.Height;

	img.Data = malloc(byteSize);
	if (!img.Data)
	{
		fclose(file);
		error = "Ran out of memory to image allocation. TOTAL SIZE REQUIRED: " + std::to_string(byteSize);
		return img;
	}

	const u64 result = fread_s(img.Data, byteSize, 1, byteSize, file);
	if (result < byteSize)
	{
		fclose(file);
		free(img.Data);
		img.Data = nullptr;
		error = "Ran out of memory while reading image.";
		return img;
	}

	ProTerGen::BMP::Invert3Channels(img.Data, byteSize);
	fclose(file);

	img.FormatSize = info.BitCount / 8;
	img.Width = info.Width;
	img.Height = info.Height;

	return img;
}

ProTerGen::ImageRawData ProTerGen::BMP::DeserializeBMP_A(const std::string& filename, std::string& error)
{
	FILE* file = nullptr;
	if (!OpenFileA(file, filename, error, "rb+"))
	{
		return ImageRawData{};
	}
	return __DeserializeBMP(file, error);
}

ProTerGen::ImageRawData ProTerGen::BMP::DeserializeBMP_W(const std::wstring& filename, std::string& error)
{
	FILE* file = nullptr;
	if (!OpenFileW(file, filename, error, L"rb+"))
	{
		return ImageRawData{};
	}
	return __DeserializeBMP(file, error);
}

bool __SerializeBMP(FILE* file, const ProTerGen::ImageRawData& image, std::string& error)
{

	const u32 channels = image.FormatSize;	
	const u32 headerSize = 14;
	const u32 infoSize = 40;
	const u64 size = (u64)image.Width * image.Height * channels;

	const Header header =
	{
		.Type = BITMAP_TYPE, // chars 'B' and 'M'
		.Size = (image.Width * image.Height * channels) + headerSize + infoSize,
		.Reserved1 = 0,
		.Reserved2 = 0,
		.Offset = headerSize + infoSize
	};

	const Info info =
	{
		.Size = infoSize,
		.Width = image.Width,
		.Height = image.Height,
		.Planes = 1,
		.BitCount = (u16)(channels * 8u),
		.Compression = 0,
		.SizeImage = (u32)size,
		.XPixelsPerMeter = 2834, // 72ppi
		.YPixelsPerMeter = 2834, // 72ppi
		.ColorsUsed = 0,
		.ColorsImportant = 0
	};
	u8 bmppad[3] = { 0,0,0 };	

	Write_u16(header.Type, file);
	Write_u32(header.Size, file);
	Write_u16(header.Reserved1, file);
	Write_u16(header.Reserved2, file);
	Write_u32(header.Offset, file);

	Write_u32(info.Size, file);
	Write_u32(info.Width, file);
	Write_u32(info.Height, file);
	Write_u16(info.Planes, file);
	Write_u16(info.BitCount, file);
	Write_u32(info.Compression, file);
	Write_u32(info.SizeImage, file);
	Write_u32(info.XPixelsPerMeter, file);
	Write_u32(info.YPixelsPerMeter, file);
	Write_u32(info.ColorsUsed, file);
	Write_u32(info.ColorsImportant, file);

	u64 elems = 0;
	void* copy = malloc(size);
	if(!copy)
	{
		fclose(file);
		error = "Can't create an intermediate buffer for color reversing.";
		return false;
	}
	memcpy(copy, image.Data, size);
	ProTerGen::BMP::Invert3Channels(copy, size);

	try
	{
		elems = fwrite(copy, 1, size, file);
	}
	catch (void*)
	{
		fclose(file);
		error = "Can't obtain access to image data.";
		return false;
	}

	if (elems != size)
	{
		fclose(file);
		error = "IO error while writing image. ELEMENTS COPIED: " + std::to_string(elems) + ".";
		return false;
	}
	fclose(file);
	return true;
}

bool ProTerGen::BMP::SerializeBMP_A(const std::string& filename, const ProTerGen::ImageRawData& image, std::string& error)
{
	FILE* file = nullptr;
	if(!OpenFileA(file, filename, error, "wb"))
	{
		return false;
	}
	return __SerializeBMP(file, image, error);
}

bool ProTerGen::BMP::SerializeBMP_W(const std::wstring& filename, const ProTerGen::ImageRawData& image, std::string& error)
{
	FILE* file = nullptr;
	if (!OpenFileW(file, filename, error, L"wb"))
	{
		return false;
	}
	return __SerializeBMP(file, image, error);

}

bool ProTerGen::BMP::Invert3Channels(void* data, size_t size)
{
	for (size_t i = 0; i < size; i += 3)
	{
		byte temp = ((byte_ptr)data)[i];
		((byte_ptr)data)[i] = ((byte_ptr)data)[i + 2];
		((byte_ptr)data)[i + 2] = temp;
	}
	return true;
}