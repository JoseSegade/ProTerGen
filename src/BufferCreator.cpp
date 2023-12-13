#include "BufferCreator.h"

Microsoft::WRL::ComPtr<ID3D12Resource> ProTerGen::BufferCreator::CreateDefaultBuffer
(
   Microsoft::WRL::ComPtr<ID3D12Device> device, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList, 
   const void* initData, 
   size_t byteSize, 
   Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer
)
{
   Microsoft::WRL::ComPtr<ID3D12Resource> defaultBuffer = nullptr;

   CD3DX12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
   CD3DX12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
   ThrowIfFailed(device->CreateCommittedResource
   (
      &heapProps,
      D3D12_HEAP_FLAG_NONE,
      &resDesc,
      D3D12_RESOURCE_STATE_COMMON,
      nullptr,
      IID_PPV_ARGS(defaultBuffer.ReleaseAndGetAddressOf())
   ));

   heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
   ThrowIfFailed(device->CreateCommittedResource
   (
      &heapProps,
      D3D12_HEAP_FLAG_NONE,
      &resDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ,
      nullptr,
      IID_PPV_ARGS(uploadBuffer.ReleaseAndGetAddressOf())
   ));

   D3D12_SUBRESOURCE_DATA subResourceData = 
   {
      .pData = initData,
      .RowPitch = static_cast<LONG_PTR>(byteSize),
      .SlicePitch = static_cast<LONG_PTR>(byteSize)
   };
   CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition
   (
      defaultBuffer.Get(),
      D3D12_RESOURCE_STATE_COMMON, 
      D3D12_RESOURCE_STATE_COPY_DEST
   );
   cmdList->ResourceBarrier(1, &barrier);
   UpdateSubresources<1>(cmdList.Get(), defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);

   barrier = CD3DX12_RESOURCE_BARRIER::Transition
   (
      defaultBuffer.Get(),
      D3D12_RESOURCE_STATE_COPY_DEST, 
      D3D12_RESOURCE_STATE_GENERIC_READ
   );
   cmdList->ResourceBarrier(1, &barrier);


   return defaultBuffer;
}
