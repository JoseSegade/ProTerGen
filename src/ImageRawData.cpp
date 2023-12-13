#include "ImageRawData.h"

ProTerGen::FileRawData::~FileRawData()
{
	if (File)
	{
		fclose(File);
		File = nullptr;
	}
}

void ProTerGen::FileRawData::RewindPtr() const 
{
	const size_t offset = max(0, Offset);
	if (_fseeki64(File, offset, SEEK_SET) != 0) perror("fseek");
}

void ProTerGen::FileRawData::CopyTo(void* dst, const Point& dstOffset, const Rectangle& srcRegion) const
{
	const uint32_t width = min(Width - srcRegion.X, srcRegion.Width);
	const uint32_t height = min(Height - srcRegion.Y, srcRegion.Height);

	RewindPtr();
	const size_t initOffset = ((size_t)Width * srcRegion.Y) * FormatSize;
	if (_fseeki64(File, initOffset, SEEK_CUR) != 0) perror("fseek");

	for (uint32_t i = 0; i < height; ++i)
	{
		const size_t srcOffsetX = (size_t)srcRegion.X * FormatSize;
		if (_fseeki64(File, srcOffsetX, SEEK_CUR) != 0) perror("fseek");
		const size_t size = (size_t)width * FormatSize;

		const size_t dstIndex = (srcRegion.Width * ((size_t)dstOffset.Y + i) + dstOffset.X) * FormatSize;
		char* pDst = (char*)dst + dstIndex;

		const size_t elementsCopied = fread_s(pDst, size, 1, size, File);
		if (elementsCopied != size)
		{
			perror("fread");
			printf("Can't copy elements into buffer.\t(%llu/%llu) elements copied.\n", elementsCopied, size);
			printf("File image width = %lu and height = %lu\n", Width, Height);
			printf("Computed width = %lu and height = %lu\n", width, height);
			printf("On region { x = %lu, y = %lu, w = %lu, h = %lu }.\n", srcRegion.X, srcRegion.Y, srcRegion.Width, srcRegion.Height);
			printf("Error on line = %lu\n", i);
		}
		assert(elementsCopied == size && "Can't copy elements to buffer.");

		const size_t srcOffsetEndLine = ((size_t)Width - width - srcRegion.X) * FormatSize;
		if (_fseeki64(File, srcOffsetEndLine, SEEK_CUR) != 0) perror("fseek");
	}

	RewindPtr();
	fflush(File);
}

void ProTerGen::FileRawData::WriteIn(const void* src, const Point& srcOffset, const Rectangle& dstRegion) const
{
	const uint32_t width = min(Width - dstRegion.X, dstRegion.Width - srcOffset.X);
	const uint32_t height = min(Height - dstRegion.Y, dstRegion.Height - srcOffset.Y);

	RewindPtr();
	const size_t initOffset = ((size_t)Width * dstRegion.Y) * FormatSize;
	if(_fseeki64(File, initOffset, SEEK_CUR) != 0) perror("fseek");

	for (uint32_t i = 0; i < height; ++i)
	{
		const size_t dstOffsetX = (size_t)dstRegion.X * FormatSize;
		if(_fseeki64(File, dstOffsetX, SEEK_CUR) != 0) perror("fseek");
		const size_t size = (size_t)width * FormatSize;

		const size_t srcIndex = (dstRegion.Width * ((size_t)srcOffset.Y + i) + srcOffset.X) * FormatSize;
		const char* pSrc = (char*)src + srcIndex;

		const size_t elementsWritten = fwrite(pSrc, 1, size, File);
		assert(elementsWritten == size && "Can't write elements in file.");

		const size_t dstOffsetEndLine = ((size_t)Width - width - dstRegion.X) * FormatSize;
		if(_fseeki64(File, dstOffsetEndLine, SEEK_CUR) != 0) perror("fseek");
	}

	RewindPtr();
}

void ProTerGen::ImageRawData::Init(uint32_t width, uint32_t height, uint32_t formatSize)
{
	Clear();

	Width = width;
	Height = height;
	FormatSize = formatSize;

	const size_t size = (size_t)Width * Height * FormatSize;
	Data = calloc(1, size);

	assert(Data && "Not enough memory to allocate a new image.");
}

ProTerGen::ImageRawData::~ImageRawData()
{
	Clear();
}

void ProTerGen::ImageRawData::CopyTo(ImageRawData& dstImg, const Point& dstOffset, const Rectangle& srcRegion) const
{
	assert(FormatSize == dstImg.FormatSize);
	assert(srcRegion.Width + srcRegion.X <= Width);
	assert(srcRegion.Height + srcRegion.Y <= Height);

	const uint32_t width = min(dstImg.Width - dstOffset.X, srcRegion.Width);
	const uint32_t height = min(dstImg.Height - dstOffset.Y, srcRegion.Height);

	for (uint32_t j = 0; j < height; ++j)
	{
		const size_t dstIndex = (dstImg.Width * ((size_t)dstOffset.Y + j) + dstOffset.X) * FormatSize;
		void* dst = (unsigned char*)dstImg.Data + dstIndex;
		const size_t srcIndex = (Width * ((size_t)srcRegion.Y + j) + srcRegion.X) * FormatSize;
		const void* src = (unsigned char*)Data + srcIndex;
		const uint32_t size = width * FormatSize;
		memcpy(dst, src, size);
	}
}

void ProTerGen::ImageRawData::CopyTo(void* dest, const Point& destOffset, const Rectangle& srcRegion) const
{
	const uint32_t width = min(Width - srcRegion.X, srcRegion.Width);
	const uint32_t height = min(Height - srcRegion.Y, srcRegion.Height);

	for (uint32_t j = 0; j < height; ++j)
	{
		const size_t dstOffset = (srcRegion.Width * ((size_t)destOffset.Y + j) + destOffset.X) * FormatSize;
		void* dst = (unsigned char*)dest + dstOffset;
		const size_t srcOffset = (Width * ((size_t)srcRegion.Y + j) + srcRegion.X) * FormatSize;
		const void* src = (unsigned char*)Data + srcOffset;
		const uint32_t size = width * FormatSize;
		memcpy(dst, src, size);
	}
}

void ProTerGen::ImageRawData::FillRegion(const Rectangle& region, const void* color)
{
	const uint32_t width = min(Width, region.Width);
	const uint32_t height = min(Height, region.Height);

	for (uint32_t y = region.Y; y < region.Y + height; ++y)
	{
		for (uint32_t x = region.X; x < region.X + width; ++x)
		{
			unsigned char* data = (unsigned char*)Data + ((y * Width + x) * FormatSize);
			memcpy(data, color, FormatSize);
		}
	}
}

void ProTerGen::ImageRawData::Mipmap(void* dest, const Point& dstOffset, uint32_t mipSize, uint32_t channels) const
{
	const size_t size = mipSize >> 1;

	for (size_t y = 0; y < size; ++y)
	{
		for (size_t x = 0; x < size; ++x)
		{
			const size_t srcIdx = ((2 * y * mipSize) + (2 * x)) * channels;
			const size_t dstIdx = (((y + dstOffset.Y) * size) + (x + dstOffset.X)) * channels;
			for (uint32_t c = 0; c < channels; ++c)
			{
				const size_t dstOffset = dstIdx + c;

				const double nw = (double)((unsigned char*)Data)[srcIdx + c];
				const double ne = (double)((unsigned char*)Data)[srcIdx + c + channels];
				const double sw = (double)((unsigned char*)Data)[srcIdx + c + ((size_t)mipSize * channels)];
				const double se = (double)((unsigned char*)Data)[srcIdx + c + ((size_t)mipSize * channels) + channels];

				const double sum = (nw + ne + sw + se) * 0.25;

				((unsigned char*)dest)[dstOffset] = (uint8_t)sum;
			}
		}
	}
}

void ProTerGen::ImageRawData::Clear()
{
	if (Data)
	{
		free(Data);
		Data = nullptr;
	}
}

void ProTerGen::ImageRawDataFootprint::Init(void* data, const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& footprint, uint32_t formatSize)
{
	Data = data;
	Footprint = footprint;
	FormatSize = formatSize;
}

void ProTerGen::ImageRawDataFootprint::CopyTo(ImageRawData& dstImg, const Point& dstOffset, const Rectangle& srcRegion) const
{
	const uint32_t fp_width = Footprint.Footprint.Width;
	const uint32_t fp_height = Footprint.Footprint.Height;
	const uint32_t fp_alignedWidth = Footprint.Footprint.RowPitch / FormatSize;

	assert(FormatSize == dstImg.FormatSize);
	assert(srcRegion.Width + srcRegion.X <= fp_width);
	assert(srcRegion.Height + srcRegion.Y <= fp_height);

	const uint32_t width = min(dstImg.Width - dstOffset.X, srcRegion.Width);
	const uint32_t height = min(dstImg.Height - dstOffset.Y, srcRegion.Height);

	for (uint32_t j = 0; j < height; ++j)
	{
		const size_t dstIndex = (dstImg.Width * ((size_t)dstOffset.Y + j) + dstOffset.X) * FormatSize;
		void* dst = (unsigned char*)dstImg.Data + dstIndex;

		const size_t srcIndex = (fp_alignedWidth * ((size_t)srcRegion.Y + j) + srcRegion.X) * FormatSize;
		const void* src = (unsigned char*)Data + srcIndex;

		const uint32_t size = width * FormatSize;
		memcpy(dst, src, size);
	}
}

void ProTerGen::ImageRawDataFootprint::CopyTo(void* dest, const Point& destOffset, const Rectangle& srcRegion) const
{
	const uint32_t fp_width = Footprint.Footprint.Width;
	const uint32_t fp_height = Footprint.Footprint.Height;
	const uint32_t fp_alignedWidth = Footprint.Footprint.RowPitch / FormatSize;

	const uint32_t width = min(fp_width - srcRegion.X, srcRegion.Width);
	const uint32_t height = min(fp_height - srcRegion.Y, srcRegion.Height);

	for (uint32_t j = 0; j < height; ++j)
	{
		const size_t dstOffset = (srcRegion.Width * ((size_t)destOffset.Y + j) + destOffset.X) * FormatSize;
		void* dst = (unsigned char*)dest + dstOffset;

		const size_t srcOffset = (fp_alignedWidth * ((size_t)srcRegion.Y + j) + srcRegion.X) * FormatSize;
		const void* src = (unsigned char*)Data + srcOffset;

		const uint32_t size = width * FormatSize;
		memcpy(dst, src, size);
	}
}

void ProTerGen::ImageRawDataFootprint::FillRegion(const Rectangle& region, const void* color)
{
	const uint32_t fp_width = Footprint.Footprint.Width;
	const uint32_t fp_height = Footprint.Footprint.Height;
	const uint32_t fp_alignedWidth = Footprint.Footprint.RowPitch / FormatSize;

	const uint32_t width = min(fp_width, region.Width);
	const uint32_t height = min(fp_height, region.Height);

	for (uint32_t y = region.Y; y < region.Y + height; ++y)
	{
		for (uint32_t x = region.X; x < region.X + width; ++x)
		{
			unsigned char* data = (unsigned char*)Data + ((y * fp_alignedWidth + x) * FormatSize);
			memcpy(data, color, FormatSize);
		}
	}
}


