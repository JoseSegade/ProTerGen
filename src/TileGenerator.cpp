#include "TileGenerator.h"
#include "BMPSerializer.h"
#include "MathHelpers.h"

ProTerGen::VT::BMPTileGenerator::~BMPTileGenerator()
{
	Close();
}

void ProTerGen::VT::BMPTileGenerator::CloseSignal()
{
	mRun.store(false);
}

void ProTerGen::VT::BMPTileGenerator::Init(const std::wstring& filename, VTDesc* desc)
{
	mInfo = desc;
	const std::wstring tempFileName = filename + L".cache";

	{
		std::wstring thisPath(filename);
		const std::wstring extension = thisPath.substr(thisPath.find_last_of('.'));
		assert(extension == L".bmp" && "Currently only supporting BMP files.");
		std::string error = "";

		if (!BMP::ReadFileBMP_W(filename, error, mFile))
		{
			OutputDebugStringA(error.c_str());
			printf(error.c_str());
		}
		desc->VTSize = mFile.Width;

	}

	bool completed = false;

	if (FILE* f = _wfopen(tempFileName.c_str(), L"rb"))
	{
		char b = getc(f);
		completed = b == CompleteChar;
		fclose(f);
	}

	if (!completed)
	{
		mTileDataFile.Open(tempFileName, desc, mFile.FormatSize, TileDataFile::READ_WRITE);
		mPageIndexer.Init(*desc);

		mRawDataPage.Init(mInfo->BorderedTileSize(), mInfo->BorderedTileSize(), mFile.FormatSize);
		mRawData2xTile.Init(mInfo->TileSize() << 1, mInfo->TileSize() << 1, mFile.FormatSize);

		mRun.store(true);
	}
}

void ProTerGen::VT::BMPTileGenerator::Init(const std::string& filename, VTDesc* desc)
{
	mInfo = desc;
	const std::string tempFileName = filename + ".cache";

	{
		std::string thisPath(filename);
		const std::string extension = thisPath.substr(thisPath.find_last_of('.'));
		assert(extension == ".bmp" && "Currently only supporting BMP files.");
		std::string error = "";

		if (!BMP::ReadFileBMP_A(filename, error, mFile))
		{
			OutputDebugStringA(error.c_str());
			printf(error.c_str());
		}
		desc->VTSize = mFile.Width;

	}

	bool completed = false;

	if (FILE* f = fopen(tempFileName.c_str(), "rb"))
	{
		char b = getc(f);
		completed = b == CompleteChar;
		fclose(f);
	}

	if (!completed)
	{
		mTileDataFile.Open(tempFileName, desc, mFile.FormatSize, TileDataFile::READ_WRITE);
		mPageIndexer.Init(*desc);

		mRawDataPage.Init(mInfo->BorderedTileSize(), mInfo->BorderedTileSize(), mFile.FormatSize);
		mRawData2xTile.Init(mInfo->TileSize() << 1, mInfo->TileSize() << 1, mFile.FormatSize);

		mRun.store(true);
	}
	else
	{
		mOnFinish();
	}
}

void ProTerGen::VT::BMPTileGenerator::Generate()
{
	if (mRun.load() == true) mTileDataFile.WriteCharOnBeginning(IncompleteChar);
	const uint32_t mipCount = mInfo->VTTilesPerRowExp + 1;
	for (uint32_t i = 0; i < mipCount && mRun.load() == true; ++i)
	{
		printf("Generating Mip %lu\n", i);
		const uint32_t count = (mInfo->VTSize / mInfo->TileSize()) >> i;
		for (uint32_t y = 0; y < count && mRun.load() == true; ++y)
		{

			for (uint32_t x = 0; x < count && mRun.load() == true; ++x)
			{
				const Page page = { .X = x, .Y = (uint32_t)y, .Mip = i };
				const PageIndex index = mPageIndexer.PageIndex(page);

				CopyTile(mRawDataPage, page);

				const size_t size = (size_t)mRawDataPage.Width * mRawDataPage.Height * mRawDataPage.FormatSize;
				if (i == 0)
				{
					BMP::Invert3Channels(mRawDataPage.Data, size);
				}

				mTileDataFile.WritePage(index, mRawDataPage.Data);
			}
		}
	}
	if (mRun.load() == true)
	{
		mTileDataFile.WriteCharOnBeginning(CompleteChar);
		printf("Generation done.\n");
		mOnFinish();
	}
	Close();
}

void ProTerGen::VT::BMPTileGenerator::Close()
{
	if (mFile.File)
	{
		fclose(mFile.File);
		mFile.File = nullptr;
		mTileDataFile.Close();
	}
}

void ProTerGen::VT::BMPTileGenerator::CopyTile(ImageRawData& image, const Page& request)
{
	if (request.Mip == 0)
	{
		int32_t indexX = (int32_t)request.X * mInfo->TileSize() - mInfo->BorderSize;
		int32_t indexY = (int32_t)request.Y * mInfo->TileSize() - mInfo->BorderSize;

		const uint32_t px = max(0, -(indexX));
		const uint32_t py = max(0, -(indexY));

		const uint32_t rx = max(0, indexX);
		const uint32_t ry = max(0, indexY);

		const Rectangle r = { .X = rx, .Y = ry, .Width = mInfo->BorderedTileSize() - px, .Height = mInfo->BorderedTileSize() - py };
		const Point p = { .X = px, .Y = py };

		mFile.CopyTo(image.Data, p, r);

	}
	else
	{
		for (uint32_t y = 0; y < 2; ++y)
		{
			for (uint32_t x = 0; x < 2; ++x)
			{
				const Page page =
				{
					.X = (x + (request.X << 1)),
					.Y = (y + (request.Y << 1)),
					.Mip = request.Mip - 1
				};
				mTileDataFile.ReadPage(mPageIndexer.PageIndex(page), image.Data, 3);

				const Point p =
				{
					.X = x * mInfo->TileSize(),
					.Y = y * mInfo->TileSize()
				};
				const Rectangle r =
				{
					.X = mInfo->BorderSize,
					.Y = mInfo->BorderSize,
					.Width = mInfo->TileSize(),
					.Height = mInfo->TileSize()
				};
				image.CopyTo(mRawData2xTile, p, r);
			}
		}

		const Point p =
		{
			.X = mInfo->BorderSize,
			.Y = mInfo->BorderSize
		};
		mRawData2xTile.Mipmap(image.Data, p, mRawData2xTile.Width, image.FormatSize);
	}
}