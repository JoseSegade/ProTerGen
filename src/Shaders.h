#pragma once

#include <unordered_map>
#include <map>
#include <string>
#include "CommonHeaders.h"
#include "ShaderCompiler.h"

namespace ProTerGen
{
   class Shaders
   {
   public:
      enum TYPE : uint32_t
      {
         VS,
         PS,
         GS,
         HS,
         DS,
         CS
      };

      Shaders() = default;

      Microsoft::WRL::ComPtr<ID3D12RootSignature> GetRootSignature(const std::string& name) const;
      Microsoft::WRL::ComPtr<ID3D12PipelineState> GetPSO(const std::string& name);

      void CreateRootSignature(const std::string& name);
      void AddTableToRootSignature
      (
         const std::string& tableName,
         const std::string& rootName,
         uint32_t elementNumber = 1,
         D3D12_DESCRIPTOR_RANGE_TYPE descRange = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
         D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_PIXEL,
         uint32_t slot = 0
      );
      void AddConstantBufferToRootSignature(const std::string& bufferName, const std::string& rootName, uint32_t slot = 0);
      void AddShaderResourceViewToRootSignature(const std::string& srvName, const std::string& rootName, uint32_t slot = 0, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_PIXEL);
      void SetSamplersToRootSignature(const std::string& rootName, const std::vector<CD3DX12_STATIC_SAMPLER_DESC>& samplers);

      void CompileRootSignatures(Microsoft::WRL::ComPtr<ID3D12Device> device);
      void CompileRootSignature(Microsoft::WRL::ComPtr<ID3D12Device> device, const std::string& name);

      void CompileShaders
      (
          const std::vector<std::string>& names, 
          const std::vector<std::wstring>& path, 
          const std::vector<TYPE>& types, 
          const std::vector<D3D_SHADER_MACRO>& macroDefines = {}
      );
      std::vector<D3D12_INPUT_ELEMENT_DESC>& InputLayout(const std::string& name);

      std::pair<uint32_t, uint32_t> GetLocation(const std::string name) const;

      void CreateGraphicPSO
      (
         const std::string& name, 
         Microsoft::WRL::ComPtr<ID3D12Device> device, 
         const std::string& rootName, 
         const std::string& inputLayoutName, 
         const std::vector<std::string>& shaderNames,
         D3D12_GRAPHICS_PIPELINE_STATE_DESC desc
      );
      void CreateComputePSO
      (
          const std::string& name,
          Microsoft::WRL::ComPtr<ID3D12Device> device,
          const std::string& rootName,
          const std::string& shaderName
      );
      void FreeShadersMemory();
   private:
      struct RootParameter
      {
         std::string RootSignature;
         uint32_t Location;
         uint32_t Slot;
      };

      struct RootSignature
      {
         Microsoft::WRL::ComPtr<ID3D12RootSignature> RootSignature;
         bool IsCompiled = false;
         std::vector<CD3DX12_STATIC_SAMPLER_DESC> Samplers;
         uint32_t ElementCount = 0;
         std::map<uint32_t, uint32_t> LastShaderSpace;
         std::map<uint32_t, uint32_t> LastTable;
      };

      struct Shader
      {
         Microsoft::WRL::ComPtr<ID3DBlob> Blob;
         TYPE Type;
      };

      std::unordered_map<std::string, RootSignature> mRootSignatures;
      std::unordered_map<std::string, RootParameter> mRootParameters;
      
      std::unordered_map<std::string, std::vector<std::unique_ptr<CD3DX12_DESCRIPTOR_RANGE1>>> mDirectXRootTables;
      std::unordered_map<std::string, std::vector<CD3DX12_ROOT_PARAMETER1>> mDirectXParamaters;

      std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;
      std::unordered_map<std::string, Shader> mShaders;
      std::unordered_map<std::string, std::vector<D3D12_INPUT_ELEMENT_DESC>> mInputLayouts;
      ShaderCompiler sc = {};

   };
}