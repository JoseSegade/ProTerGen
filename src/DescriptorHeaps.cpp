#include "DescriptorHeaps.h"
#include "MathHelpers.h"

ProTerGen::DescriptorHeaps::DescriptorHeaps

(
	Microsoft::WRL::ComPtr<ID3D12Device> device, 
   uint32_t rtvCount,
   uint32_t dsvCount,
   uint32_t srvCount, 
   uint32_t samplersCount
)
	: mRtvHeap(device, rtvCount, D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
	, mDsvHeap(device, dsvCount, D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
	, mSrvHeap(device, srvCount, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
	, mSamplersHeap(device, samplersCount, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
{
	if(mRtvHeap.Heap) mRtvHeap.Heap->SetName(L"DescriptorHeap_RTV");
	if(mDsvHeap.Heap) mDsvHeap.Heap->SetName(L"DescriptorHeap_DSV");
	if(mSrvHeap.Heap) mSrvHeap.Heap->SetName(L"DescriptorHeap_SRV");
	if(mSamplersHeap.Heap) mSamplersHeap.Heap->SetName(L"DescriptorHeap_Samplers");
}

ProTerGen::DescriptorHeaps::~DescriptorHeaps()
{
}

void ProTerGen::DescriptorHeaps::BuildSRVTable(const std::vector<std::pair<std::string, uint32_t>>& tableConfig)
{
	uint32_t offset = 0;
	for (const auto& [name, count] : tableConfig)
	{
		if (name == "")
		{
			printf("SRV table entry must have a valid name.\n");
			throw - 1;
		}
		if (count < 1)
		{
			printf("SRV table must have at least one element, at entry %s.\n", name.c_str());
			throw - 1;
		}
		if (count > 32)
		{
			printf("Currently maximum element size is 32, %lu provided for entry %s.\n", count, name.c_str());
			throw - 1;
		}
		mSrvTable[name] =
		{
			.MaxSize = (uint8_t)count,
			.IndexOffset = offset
		};
		offset += count;
	}
	if (offset > mSrvHeap.Count)
	{
		printf("Element number in SRV table (%lu) is superior than the assigned to the SRV Heap (%lu).\n", offset, mSrvHeap.Count);
		throw - 1;
	}
	if (offset < mSrvHeap.Count)
	{
		printf("WARNING: Element number in table (%lu) is inferior than the assigned to the SRV Heap (%lu).\n", offset, mSrvHeap.Count);
	}
}

uint32_t ProTerGen::DescriptorHeaps::SetSrvIndex(const std::string& tableName, const std::string& uniqueId, int32_t tablePosition)
{
	if (mRtvHeap.IsFull() && tablePosition == -1)
	{
		printf("SRV heap is full.\n");
		throw - 1;
	}
	if ((tableName == "" && mSrvTable.size() > 0) || (mSrvTable.count(tableName) < 1 && tableName != ""))
	{
		printf("Table entry not specified. Please provide a valid table name.\n");
		throw - 1;
	}
	if (tablePosition > 31)
	{
		printf("Currently table entries only support 32 elements. Please provide a table position inferior to 32. %lu provided.\n", tablePosition);
		throw - 1;
	}
	uint32_t index = 0xffffffff;
	if (tableName == "" && mSrvTable.size() == 0)
	{
		index = mSrvHeap.Occupied;
	}
	else
	{
		srv_table_entry& entry = mSrvTable[tableName];
		if (tablePosition < 0 && (entry.OccupiedElements == entry.MaxSize))
		{
			printf("SRV internal table %s is full.\n", tableName.c_str());
			throw - 1;
		}

		if (tablePosition < 0)
		{
			const uint32_t i = BitScanOne(~entry.OccupiedMask);
			index = entry.IndexOffset + i;
			entry.OccupiedMask |= (1 << i);
			entry.OccupiedElements++;
		}
		else
		{
			uint32_t maskPos = 1 << tablePosition;
			if ((maskPos & entry.OccupiedMask) != maskPos)
			{
				entry.OccupiedElements++;
			}
			entry.OccupiedMask |= maskPos;
			index = entry.IndexOffset + tablePosition;
		}
	}
	mIndexTable[uniqueId] = index; 
	mSrvHeap.Occupied++;
	return index;
}

uint32_t ProTerGen::DescriptorHeaps::SetContinuousSrvIndices(const std::string& tableName, const std::string uniqueId, uint32_t count)
{
	if (mSrvHeap.Occupied + count > mSrvHeap.Count)
	{
		printf("SRV heap is full.\n");
		throw - 1;
	}
	if ((tableName == "" && mSrvTable.size() > 0) || (mSrvTable.count(tableName) < 1 && tableName != ""))
	{
		printf("Table entry not specified. Please provide a valid table name.\n");
		throw - 1;
	}

	uint32_t index = 0xffffffff;
	if (tableName == "")
	{
		index = mSrvHeap.Occupied;
		for (uint32_t i = 0; i < count; ++i)
		{
			mIndexTable[uniqueId + "_" + std::to_string(i)] = index + i;
		}
	}
	else
	{
		srv_table_entry& entry = mSrvTable[tableName];
		if (entry.OccupiedElements + count > entry.MaxSize)
		{
			printf("Table entry %s is full.", tableName.c_str());
			throw - 1;
		}

		const uint32_t ei = BitScanOne(~entry.OccupiedMask);
		index = entry.IndexOffset + ei;
		for (uint32_t i = 0; i < count; ++i)
		{
			entry.OccupiedMask |= (1 << (ei + i));
			mIndexTable[uniqueId + "_" + std::to_string(i)] = index + i;
		}
		entry.OccupiedElements += count;
	}
	mSrvHeap.Occupied += count;
	return index;
}

ProTerGen::DescriptorHeaps::DescriptorHeap::DescriptorHeap(Microsoft::WRL::ComPtr<ID3D12Device> device, uint32_t count, D3D12_DESCRIPTOR_HEAP_TYPE type)
{
	Count = count;
	if (type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV || type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
	{
		assert(Count > 0 && "Descriptor heap count must be at least 1");
	}

	if (Count > 0)
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {
			.Type = type,
			.NumDescriptors = Count,
			.Flags = type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV 
				? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE 
				: D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
			.NodeMask = 0
		};
		ThrowIfFailed(device->CreateDescriptorHeap
		(
			&heapDesc,
			IID_PPV_ARGS(Heap.ReleaseAndGetAddressOf())
		));
		CpuHandle = Heap->GetCPUDescriptorHandleForHeapStart();
		GpuHandle = Heap->GetGPUDescriptorHandleForHeapStart();
		IncrementSize = device->GetDescriptorHandleIncrementSize(type);
	}
}

CD3DX12_CPU_DESCRIPTOR_HANDLE ProTerGen::DescriptorHeaps::DescriptorHeap::GetCPUHandle(uint32_t index) const
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE ret = CpuHandle;
	ret.Offset(index, IncrementSize);
	return ret;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE ProTerGen::DescriptorHeaps::DescriptorHeap::GetGPUHandle(uint32_t index) const
{
	CD3DX12_GPU_DESCRIPTOR_HANDLE ret = GpuHandle;
	ret.Offset(index, IncrementSize);
	return ret;
}
