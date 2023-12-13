#pragma once

#include <unordered_map>
#include <string>
#include <memory>
#include <functional>
#include "Material.h"
#include "Texture.h"
#include "UploadBuffer.h"
#include "ShaderConstants.h"
#include "TextureCreator.h"
#include "DescriptorHeaps.h"

namespace ProTerGen
{
	class Materials
	{
	public:
		enum class TextureType : uint32_t
		{
			TEXTURE_SRV = 1,
			SKYMAP_SRV = 2,
			TEXTURE_UAV = 3,
		};		
		struct TextureInfo
		{
			std::unique_ptr<Texture> TexturePtr   = nullptr;
			TextureType              Type         = TextureType::TEXTURE_SRV;
			bool                     CanUserWrite = false;
			std::string				 HeapTable    = "";
		};
		using filter_func_t = std::function<bool(const std::string& name, const TextureInfo& texInfo)>;

		Materials() = default;
		virtual ~Materials() = default;

		Material*                CreateMaterial(const std::string& name);
		size_t                   GetMaterialCount() const;
		Material*                GetMaterial(const std::string& name);
		std::vector<Material*>   GetMaterials() const;
		std::vector<std::string> GetMaterialNames() const;
		Material*                GetDefaultMaterial() const;
		void                     ReloadMaterialsCBIndices();
		constexpr void           MarkMaterialDirtyGlobalFlag() { mGlobalDirty = true; }

		void                     FillMaterialsBuffer(UploadBuffer<MaterialConstants>* buffer);

		Texture* const           CreateTexture
		(
			Microsoft::WRL::ComPtr<ID3D12Device>              device,
			Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList,
			const std::string                                 name,
			const std::wstring                                path,
			bool                                              writeByUserPermission = false
		);
		Texture* const           CreateEmptyTexture(const std::string& name, TextureType type = TextureType::TEXTURE_SRV, bool writeByUserPermission = false);
		void                     CreateDefaultTextures(Microsoft::WRL::ComPtr<ID3D12Device> device, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList);
		Texture* const           GetTexture(const std::string& name);
		void                     ChangeTextureName(const std::string oldName, const std::string newName);
		std::vector<std::string> GetTextureNames(filter_func_t filter) const;
		std::string              GetTextureHeapTable(const std::string& texName) const;
		Texture* const           ChangeTextureFromPath
		(
			Microsoft::WRL::ComPtr<ID3D12Device> device,
			Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList,
			DescriptorHeaps& descriptorHeaps, 
			Texture* oldTexture,
			const std::wstring& newPath
		);
		inline bool              TextureNameExists(const std::string& texname) const { return mTextures.count(texname) > 0; }

		void                     InsertTexturesInSRVHeap
		(
			Microsoft::WRL::ComPtr<ID3D12Device> device,
			DescriptorHeaps&                     descriptorHeaps,
			const std::string&                   tableEntryName,
			const std::vector<std::string>&      textureNames
		);
		void                     OverwriteTextureInSrvHeap
		(
			Microsoft::WRL::ComPtr<ID3D12Device> device,
			DescriptorHeaps&                     descriptorHeaps,
			const std::string&                   oldTextureName,
			const std::string&                   newTextureName
		);
		size_t                   GetTextureCount() const;

		constexpr const std::string& GetDefaultMaterialName()      { return DEFAULT_MATERIAL_NAME; }
		constexpr const std::string& GetDefaultTextureColorName()  { return DEFAULT_TEX_COLOR_MAP_NAME; }
		constexpr const std::string& GetDefaultTextureNormalName() { return DEFAULT_TEX_NORMAL_MAP_NAME; }
		constexpr uint32_t           NextMatNameIndex()            { return ++mMaterialsCreated; }

		void CleanTextureGarbage();

		Material*    NewMaterialDummy();
		Material*    GetMaterialDummy();
		TextureInfo* NewTextureDummy();
		TextureInfo* GetTextureDummy();
	protected:
		// WARNING: This method moves texture pointer to temporary memory but 
		// does not remove the original entry in mTextures. Using the 
		// original entry could cause memory corruption and stop the application.
		void AddTextureToGarbage(const std::string& texName);

	protected:
		struct TextureGarbage
		{
			std::unique_ptr<Texture> Texture    = nullptr;
			uint32_t                 FramesLeft = 0;
		};

	protected:
		const std::string                                          DEFAULT_MATERIAL_NAME       = "DEFAULT_MAT_NAME";
		const std::string                                          DEFAULT_TEX_COLOR_MAP_NAME  = "DEFAULT_TEX_COLOR_MAP";
		const std::string                                          DEFAULT_TEX_NORMAL_MAP_NAME = "DEFAULT_TEX_NORMAL_MAP";
		std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
		std::unordered_map<std::string, TextureInfo>               mTextures;
		std::unique_ptr<Material>                                  mMaterialDummy              = nullptr;
		std::unique_ptr<TextureInfo>                               mTextureDummy               = nullptr;
		uint32_t                                                   mMaterialsCreated           = 0;
		bool                                                       mGlobalDirty                = false;
		std::vector<TextureGarbage>                                mTextureGarbage;
	};
}
