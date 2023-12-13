#pragma once

#include "CommonHeaders.h"
#include "DirectXMath.h"
#include "ECS.h"
#include "Meshes.h"

namespace ProTerGen
{
    struct GrassParticleComponent
    {
        using Index     = uint32_t;
        using Indices   = std::vector<Index>;
        using Vertices  = std::vector<Vertex>;
        using Position  = DirectX::XMFLOAT3;
        using Positions = std::vector<Position>;

        Positions ParticlePositions{};
        size_t MaxSize = 1;

        bool Dirty = true;
        bool CreatedOnGpu = false;
    };
    
    struct DynamicParticleComponent
    {
        using Index     = uint32_t;
        using Indices   = std::vector<Index>;
        using Vertices  = std::vector<Vertex>;
        using Position  = DirectX::XMFLOAT3;
        using Positions = std::vector<Position>;

        Positions PreloadedPositions{};
        Positions ParticlePositions{};
        size_t MaxSize = 1;
        size_t ParticlesPerChunk = 0;

        bool Dirty = true;
        bool CreatedOnGpu = false;
    };

    class StaticGrassParticleSystem : public ECS::ECSSystem<GrassParticleComponent>
    {
    public:
        StaticGrassParticleSystem(Meshes& meshes) : mMeshes(meshes) {};
        virtual ~StaticGrassParticleSystem() {};

        void Init() override;
        void Update(double dt) override;

        void CreateOnGpu(Microsoft::WRL::ComPtr<ID3D12Device> device, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList);
        void UpdateOnGpu(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList);

        inline void SetMeshes(Meshes& meshes) { mMeshes = meshes; }
    protected:
        Meshes& mMeshes;
    };

    class DynamicGrassParticleSystem : public ECS::ECSSystem<DynamicParticleComponent>
    {
    public:
        DynamicGrassParticleSystem(Meshes& meshes) : mMeshes(meshes) {};
        virtual ~DynamicGrassParticleSystem() {};

        void Init() override;
        void Update(double dt) override;

        void UpdateOnGpu(Microsoft::WRL::ComPtr<ID3D12Device> device, uint32_t currentFrame);

        inline void SetMeshes(Meshes& meshes) { mMeshes = meshes; }
    protected:
        Meshes& mMeshes;
    };
}
