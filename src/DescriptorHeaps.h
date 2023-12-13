#pragma once

#include <unordered_map>
#include <array>
#include "CommonHeaders.h"

namespace ProTerGen
{   
   class DescriptorHeaps
   {
   public:
       static const uint32_t InvalidIndex = 0xffffff;

      struct DescriptorHeap
      {
         DescriptorHeap() = default;
         DescriptorHeap(Microsoft::WRL::ComPtr<ID3D12Device> device, uint32_t count, D3D12_DESCRIPTOR_HEAP_TYPE type);
         CD3DX12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(uint32_t index = 0) const;
         CD3DX12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(uint32_t index = 0) const;
         inline bool IsFull() const { return Occupied >= Count; }

         Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> Heap = nullptr;
         CD3DX12_CPU_DESCRIPTOR_HANDLE CpuHandle = {};
         CD3DX12_GPU_DESCRIPTOR_HANDLE GpuHandle = {};
         size_t IncrementSize = 0;
         uint32_t Count = 0;
         uint32_t Occupied = 0;
      };

      DescriptorHeaps() = default;
      DescriptorHeaps
      (
         Microsoft::WRL::ComPtr<ID3D12Device> device,
         uint32_t rtvCount = 1,
         uint32_t dsvCount = 1,
         uint32_t srvCount = 0,
         uint32_t samplersCount = 0
      );
      virtual ~DescriptorHeaps();

      void BuildSRVTable(const std::vector<std::pair<std::string, uint32_t>>& tableConfig);
            
      inline DescriptorHeap& GetRtvHeap() { return mRtvHeap; };
      inline DescriptorHeap& GetSrvHeap() { return mSrvHeap; };
      inline DescriptorHeap& GetDsvHeap() { return mDsvHeap; };
      inline DescriptorHeap& GetSamplersHeap() { return mSamplersHeap; };

      inline const DescriptorHeap& GetRtvHeap() const { return mRtvHeap; };
      inline const DescriptorHeap& GetSrvHeap() const { return mSrvHeap; };
      inline const DescriptorHeap& GetDsvHeap() const { return mDsvHeap; };
      inline const DescriptorHeap& GetSamplersHeap() const { return mSamplersHeap; };

      inline uint32_t SetRtvIndex(const std::string& name) 
      { 
          assert(!mRtvHeap.IsFull() && "RTVHeap Full"); mIndexTable[name] = mRtvHeap.Occupied++; return mIndexTable.at(name); 
      }
      /*
      inline uint32_t SetSrvIndex(const std::string& name) 
      { 
          assert(!mSrvHeap.IsFull() && "SRVHeap Full"); mIndexTable[name] = mSrvHeap.Occupied++; return mIndexTable.at(name); 
      }
      */
      inline uint32_t SetDsvIndex(const std::string& name) 
      { 
          assert(!mDsvHeap.IsFull() && "DSVHeap Full"); mIndexTable[name] = mDsvHeap.Occupied++; return mIndexTable.at(name); 
      }
      inline uint32_t SetSamplersIndex(const std::string& name) 
      { 
          assert(!mSamplersHeap.IsFull() && "SamplerHeap Full"); mIndexTable[name] = ++mSamplersHeap.Occupied; return mIndexTable.at(name); 
      }

      inline uint32_t RtvReserveIndices(const std::string& name, uint32_t count)
      {
          const uint32_t res = mRtvHeap.Occupied;
          for (uint32_t i = 0; i < count; ++i)
          {
              const std::string key = name + std::to_string(i);
              SetRtvIndex(key);
          }
          return res;
      }

      /*
      inline uint32_t SrvReserveIndices(const std::string& name, uint32_t count)
      {
          const uint32_t res = mSrvHeap.Occupied;
          for (uint32_t i = 0; i < count; ++i)
          {
              const std::string key = name + std::to_string(i);
              SetSrvIndex(key);
          }
          return res;
      }
      */

      inline uint32_t DsvReserveIndices(const std::string& name, uint32_t count)
      {
          const uint32_t res = mDsvHeap.Occupied;
          for (uint32_t i = 0; i < count; ++i)
          {
              const std::string key = name + std::to_string(i);
              SetDsvIndex(key);
          }
          return res;
      }

      inline uint32_t SamplersReserveIndices(const std::string& name, uint32_t count)
      {
          const uint32_t res = mSamplersHeap.Occupied;
          for (uint32_t i = 0; i < count; ++i)
          {
              const std::string key = name + std::to_string(i);
              SetSamplersIndex(key);
          }
          return res;
      }

      inline uint32_t GetIndex(const std::string& name) const 
      { 
          if (mIndexTable.count(name) < 1)
          {
              return InvalidIndex;
          }
          return mIndexTable.at(name);
      }

      uint32_t SetSrvIndex(const std::string& tableEntryName, const std::string& uniqueId, int32_t tablePosition = -1);
      uint32_t SetContinuousSrvIndices(const std::string& tableEntryName, const std::string uniqueId, uint32_t count);
      inline uint32_t GetSrvTableEntryLocation(const std::string& tableEntryName) const
      {
          if (mSrvTable.count(tableEntryName) < 1)
          {
              printf("Table entry %s does not exist. Please provide a valid table name.", tableEntryName.c_str());
              throw - 1;
          }
          return mSrvTable.at(tableEntryName).IndexOffset;
      }
      inline bool IsSrvTableFull(const std::string& tableEntryName) const
      {
          if (mSrvTable.count(tableEntryName) < 1) return true;
          return mSrvTable.at(tableEntryName).MaxSize == mSrvTable.at(tableEntryName).OccupiedElements;
      }
      
   private: 

      DescriptorHeap mRtvHeap;
      DescriptorHeap mSrvHeap;
      DescriptorHeap mDsvHeap;
      DescriptorHeap mSamplersHeap;

      struct srv_table_entry
      {
          uint32_t OccupiedMask     = 0;
          uint8_t  OccupiedElements = 0;
          uint8_t  MaxSize          = 0;
          uint32_t IndexOffset      = 0;
      };
      std::unordered_map<std::string, srv_table_entry> mSrvTable;
      std::unordered_map<std::string, uint32_t> mIndexTable;
   };
   
}
