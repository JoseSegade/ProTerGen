#include <filesystem>
#include "WICTextureLoader.h"
#include "DDSTextureLoader.h"
#include "TextureCreator.h"

void ProTerGen::TextureCreator::CreateFromFile
(

   Microsoft::WRL::ComPtr<ID3D12Device> device,
   Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList,
   const std::wstring& path,
   Microsoft::WRL::ComPtr<ID3D12Resource>& resource,
   Microsoft::WRL::ComPtr<ID3D12Resource>& uploadResource,
   bool& isCubeMap
) const
{
   std::wstring thisPath(path);
   const std::wstring extension = thisPath.substr(thisPath.find_last_of('.'));

   HRESULT hr = E_FAIL;
   std::unique_ptr<uint8_t[]> decodedData;

   std::vector<D3D12_SUBRESOURCE_DATA> subresources = { D3D12_SUBRESOURCE_DATA() };
   if (extension == L".png" || extension == L".bmp" || extension == L".PNG" || extension == L".BMP")
   {
      hr = DirectX::LoadWICTextureFromFile
      (
         device.Get(),
         path.c_str(),
         resource.ReleaseAndGetAddressOf(),
         decodedData,
         subresources[0]
      );
   }
   else if (extension == L".dds" || extension == L".DDS")
   {
      hr = DirectX::LoadDDSTextureFromFile
      (
         device.Get(),
         path.c_str(),
         resource.ReleaseAndGetAddressOf(),
         decodedData,
         subresources,
         SIZE_MAX,
         nullptr,
         &isCubeMap
      );
   }
   else
   {
      OutputDebugString(L"ERROR: Can be only loaded images of DDS, PNG, or BMP format.");
      printf("ERROR: Can be only loaded images of DDS, PNG, or BMP format.");
   }
   ThrowIfFailed(hr);

   const size_t uploadBufferSize = GetRequiredIntermediateSize(resource.Get(), 0, subresources.size());

   CD3DX12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
   CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
   ThrowIfFailed(device->CreateCommittedResource
   (
      &heapProps,
      D3D12_HEAP_FLAG_NONE,
      &bufferDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ,
      nullptr,
      IID_PPV_ARGS(uploadResource.ReleaseAndGetAddressOf())
   ));

   UpdateSubresources
   (
      commandList.Get(), 
      resource.Get(), 
      uploadResource.Get(),
      0,
      0, 
      subresources.size(),
      subresources.data()
   );
   
   CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition
   (
      resource.Get(),
      D3D12_RESOURCE_STATE_COPY_DEST,
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
   );
   commandList->ResourceBarrier(1, &barrier);
}
