#include "PageLoaderFromDisk.h"

ProTerGen::VT::TileDataFile::~TileDataFile()
{
	Close();
}

void ProTerGen::VT::TileDataFile::Open(const std::wstring& filename, const VTDesc* info, uint32_t formatSize, AccessMode accessMode)
{
	mMode = accessMode;
	mInfo = info;
	mFormatSize = formatSize;

	std::wstring mode = L"rb";
	if ((mMode & WRITE) == WRITE)
	{
		if ((mMode & READ) == READ)
		{
			mode = L"wb+";
		}
		else
		{
			mode = L"wb";
		}
	}

	mFile = _wfopen(filename.c_str(), mode.c_str());
	if (!mFile) perror("fopen");

	assert(mFile);
}

void ProTerGen::VT::TileDataFile::Open(const std::string& filename, const VTDesc* info, uint32_t formatSize, AccessMode accessMode)
{
	mMode = accessMode;
	mInfo = info;
	mFormatSize = formatSize;

	std::string mode = "rb";
	if ((mMode & WRITE) == WRITE)
	{
		if ((mMode & READ) == READ)
		{
			mode = "wb+";
		}
		else
		{
			mode = "wb";
		}
	}

	mFile = fopen(filename.c_str(), mode.c_str());
	if (!mFile) perror("fopen");

	assert(mFile);
}

void ProTerGen::VT::TileDataFile::Close()
{
	if (mFile)
	{
		fclose(mFile);
		mFile = nullptr;
	}
}

void ProTerGen::VT::TileDataFile::WritePage(PageIndex index, const data_ptr data)
{
	if (mFile && ((mMode & WRITE) == WRITE))
	{
		const size_t pageTotalSize = (size_t)mInfo->BorderedTileSize() * mInfo->BorderedTileSize() * mFormatSize;
		if (_fseeki64(mFile, index * pageTotalSize + 1, SEEK_SET) != 0) perror("fseek");
		assert(fwrite(data, 1, pageTotalSize, mFile) == pageTotalSize);
		fflush(mFile);
	}
}

void ProTerGen::VT::TileDataFile::WriteCharOnBeginning(char c)
{
	if (_fseeki64(mFile, 0, SEEK_SET) != 0) perror("fseek");
	putc(c, mFile);
}

bool ProTerGen::VT::TileDataFile::ReadPage(PageIndex index, data_ptr data, uint32_t formatSize) const
{
	if (mFile && ((mMode & READ) == READ))
	{
		const size_t pageTotalSize = (size_t)mInfo->BorderedTileSize() * mInfo->BorderedTileSize() * mFormatSize;
		if (formatSize == 0) formatSize = mFormatSize;
		if (formatSize != mFormatSize)
		{
			void* __data = malloc(pageTotalSize);
			if (!__data) return false;
			if (_fseeki64(mFile, index * pageTotalSize + 1, SEEK_SET) != 0)
			{
				perror("fseek");
				free(__data);
				return false;
			}
			const size_t elementsCopied = fread_s(__data, pageTotalSize, 1, pageTotalSize, mFile);
			if (elementsCopied != pageTotalSize)
			{
				free(__data);
				return false;
			}
			for (size_t i = 0, j = 0; i < pageTotalSize; i += mFormatSize, j += formatSize)
			{
				for (size_t ii = 0; ii < formatSize; ++ii)
				{
					((uint8_t*)data)[j + ii] = ii <= mFormatSize ? ((uint8_t*)__data)[i + ii] : 255;
				}
			}

			free(__data);

			return true;
		}
		else
		{
			if (_fseeki64(mFile, index * pageTotalSize + 1, SEEK_SET) != 0)
			{
				perror("fseek");
				return false;
			}
			if (fread_s(data, pageTotalSize, 1, pageTotalSize, mFile) != pageTotalSize)
			{
				perror("fread");
				return false;
			}

			return true;
		}
	}


	return false;
}

bool ProTerGen::VT::TileDataFile::ReadTile(PageIndex index, data_ptr data) const
{
	if (mFile && ((mMode & READ) == READ))
	{
		const size_t pageTotalSize = (size_t)mInfo->BorderedTileSize() * mInfo->BorderedTileSize() * mFormatSize;
		const size_t pageBorderXOffset = (size_t)mInfo->BorderSize * mFormatSize;
		const size_t pageBorderYOffset = (size_t)mInfo->BorderedTileSize() * pageBorderXOffset;
		const size_t tileByteSize = (size_t)mInfo->TileSize() * mFormatSize;

		if (_fseeki64(mFile, index * pageTotalSize + pageBorderYOffset + 1, SEEK_SET) != 0)
		{
			perror("fseek");
			return false;
		}

		for (uint32_t i = 0; i < mInfo->TileSize(); ++i)
		{
			if (_fseeki64(mFile, pageBorderXOffset, SEEK_CUR) != 0)
			{
				perror("fseek");
				return false;
			}

			char* dst = (char*)data + i * tileByteSize;
			if (fread_s(dst, tileByteSize, 1, tileByteSize, mFile) != tileByteSize)
			{
				return false;
			}

			if (_fseeki64(mFile, pageBorderXOffset, SEEK_CUR) != 0)
			{
				perror("fseek");
				return false;
			}
		}
		return true;
	}
	return false;
}

ProTerGen::VT::PageLoaderFromDisk::~PageLoaderFromDisk()
{
	Dispose();
}

void ProTerGen::VT::PageLoaderFromDisk::Init(const std::wstring& fileName, PageIndexer* indexer, const VTDesc* info)
{
	mInfo = info;
	mIndexer = indexer;

	mFile.Open(fileName, info, 3, TileDataFile::AccessMode::READ);

	mReadThread.OnComplete([&](Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList, const ReadState& state) { PageLoadComplete(commandList, state); });
	mReadThread.OnRun([&](ReadState& state) { return LoadPage(state); });
	mReadThread.Init();
}

void ProTerGen::VT::PageLoaderFromDisk::Dispose()
{
	mReadThread.Dispose();
	mFile.Close();
}

void ProTerGen::VT::PageLoaderFromDisk::Update(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList, uint32_t uploads)
{
	mReadThread.Update(commandList, uploads);
}

void ProTerGen::VT::PageLoaderFromDisk::Submit(const Page& request)
{
	ReadState state = {};
	state.page = request;

	mReadThread.Enqueue(state);
}

void ProTerGen::VT::PageLoaderFromDisk::Clear()
{
	mReadThread.Dispose();
}

void ProTerGen::VT::PageLoaderFromDisk::Restart()
{
	mReadThread.Init();
}

bool ProTerGen::VT::PageLoaderFromDisk::LoadPage(ReadState& state)
{
	bool result = true;

	uint32_t size = mInfo->BorderedTileSize() * mInfo->BorderedTileSize() * 4;

	state.data = malloc(size);
	if (!state.data) return false;

	const size_t pageIndex = mIndexer->PageIndex(state.page);
	result &= mFile.ReadPage(pageIndex, state.data, 4);

	if (mShowBorders)
	{
		result &= CopyBorder(state.data);
	}

	return result;
}

void ProTerGen::VT::PageLoaderFromDisk::PageLoadComplete(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList, const ReadState& state)
{
	mOnLoadComplete(commandList, state.page, state.data);
}

bool ProTerGen::VT::PageLoaderFromDisk::CopyBorder(data_ptr& imgData)
{
	const uint32_t pageSize = mInfo->BorderedTileSize();
	const uint32_t borderSize = mInfo->BorderSize;

	const byte color[4] = { (byte)(mBorderColor.r * 255.0), (byte)(mBorderColor.g * 255.0), (byte)(mBorderColor.b * 255.0), (byte)(mBorderColor.a * 255.0) };
	for (uint32_t i = 0; i < pageSize; ++i)
	{
		uint32_t ix = borderSize * pageSize + i;
		data_ptr dst = (byte_ptr)imgData + (ix * 4);
		memcpy(dst, color, 4);

		uint32_t iy = i * pageSize + borderSize;
		dst = (byte_ptr)imgData + (iy * 4);
		memcpy(dst, color, 4);
	}
	return true;
}