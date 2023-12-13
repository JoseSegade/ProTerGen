#pragma once

#include <unordered_map>
#include <DirectXMath.h>
#include <thread>
#include <list>
#include "CommonHeaders.h"
#include "Meshes.h"
#include "QuadTree.h"
#include "ShaderConstants.h"
#include "Mesh.h"
#include "RenderSystem.h"
#include "ECS.h"
#include "CameraSystem.h"
#include "VirtualTexture.h"
#include "TerrainLayer.h"

namespace ProTerGen
{
    struct Chunk
    {
        uint16_t x      = 0xFFFF;
        uint16_t y      = 0xFFFF;
        uint8_t  lod    = 255;
        uint8_t  border = 0;
        uint8_t  corner = 0;

        constexpr bool operator==(const Chunk& rhs) const 
        {
            return (x == rhs.x) && (y == rhs.y) && (lod == rhs.lod) &&
                (border == rhs.border) && (corner == rhs.corner);
        }
        
        constexpr size_t GetHash() const noexcept
        {
			return ((size_t)x << 40) ^ ((size_t)y << 24) ^ ((size_t)lod << 16) ^
				((size_t)border << 8) ^ ((size_t)corner);
        }
    };
}

template<>
struct std::hash<ProTerGen::Chunk>
{
    constexpr size_t operator()(const ProTerGen::Chunk& c) const noexcept
    {
        return ((size_t)c.x << 40) ^ ((size_t)c.y << 24) ^ ((size_t)c.lod << 16) ^
            ((size_t)c.border << 8) ^ ((size_t)c.corner);
    }
};

namespace ProTerGen
{
    struct TerrainChunksAsyncComponent;
    struct ChunkInfo
    {
        Chunk chunk = {};
        TerrainChunksAsyncComponent* terrainComponent = nullptr;

        ChunkInfo() : chunk(), terrainComponent(nullptr) {}
        ChunkInfo(Chunk& c, TerrainChunksAsyncComponent* tc) : chunk(c), terrainComponent(tc) {}
        ChunkInfo(ChunkInfo& other)
        {
            chunk = other.chunk;
            terrainComponent = other.terrainComponent;
            other.terrainComponent = nullptr;
        }

        ChunkInfo& operator=(ChunkInfo& other)
        {
            chunk = other.chunk;
            terrainComponent = other.terrainComponent;
            other.terrainComponent = nullptr;
            return *this;
        }

        ~ChunkInfo()
        {
            terrainComponent = nullptr;
        }
    };

    struct TerrainSettings
    {
        float    Height           = 1.0f;
        float    TerrainWidth     = 1024;
        uint32_t ChunksPerSideExp = 8;
        uint32_t QuadsPerChunk    = 1;
        uint32_t GpuSubdivisions  = 1;

        std::vector<Layer>         Layers{};
        std::vector<MaterialLayer> MaterialLayers{};
    };

    struct TerrainChunksAsyncComponent
    {
        using Index = uint32_t;
        using Indices = std::vector<Index>;
        using Vertices = std::vector<Vertex>;

        TerrainSettings TerrainSettings{};

        std::array<Mesh, RQuadTreeTerrain::BORDER_COUNT> Chunks{};

        using MeshIdx = std::list<Mesh>::iterator;
        std::list<Mesh> MemoryChunks{};
        std::queue<MeshIdx> Offsets{};
        std::unordered_set<size_t> Requested{};
        LRUCache<Chunk, MeshIdx> Loaded {};
        std::unique_ptr<VT::PageThread<ChunkInfo>> Thread = nullptr;
    };

    struct TerrainQTComponent
    {
        using Index = uint32_t;
        using Indices = std::vector<Index>;
        using Vertices = std::vector<Vertex>;
        
        TerrainSettings TerrainSettings{};
        Mesh Mesh{};
        std::array<ProTerGen::Mesh, RQuadTreeTerrain::BORDER_COUNT> Models{};
        std::vector<ECS::Entity> ParticleSystems{};
    };

    class TerrainChunksAsyncSystem : public ECS::ECSSystem<TerrainChunksAsyncComponent>
    {
    public:
        static const uint32_t MAX_CHUNKS = 256;
    public:
        TerrainChunksAsyncSystem(Meshes& meshes, CameraComponent& camera) : mMeshes(meshes), mCamera(camera) {};
        virtual ~TerrainChunksAsyncSystem();

        void Init() override;
        void Update(double dt) override;
        void UpdateOnGpu(Microsoft::WRL::ComPtr<ID3D12Device> device, uint32_t currentFrame);
       
        inline void SetMeshes(Meshes& meshes) { mMeshes = meshes; };
        inline void SetCamera(CameraComponent& camera) { mCamera = camera; };
    protected:
        using Index = uint32_t;
        using Indices = std::vector<Index>;
        using Vertices = std::vector<Vertex>;

        std::vector<RQuadTreeTerrain*> ComputeQuadTree
        (
            const DirectX::XMFLOAT3& camPos,
            const DirectX::BoundingFrustum& frustum,
            std::unique_ptr<RQuadTreeTerrain>& rootNode,
            const TerrainChunksAsyncComponent& tc
        ) const;
        void RequestMesh(const std::vector<RQuadTreeTerrain*> requests, TerrainChunksAsyncComponent& tc);
        void OnEntityRemoved(ECS::Entity entity) override;
        bool ProcessGeometryFromHeightData(ChunkInfo& ci);
        void RemoveChunk(ECS::Entity entity, Chunk& chunk, TerrainChunksAsyncComponent::MeshIdx index);

        std::timed_mutex mMutex;
        Meshes& mMeshes;
        CameraComponent& mCamera;
    };

	class TerrainQuadTreeSystem : public ECS::ECSSystem<TerrainQTComponent>
    {
    public:
        TerrainQuadTreeSystem(Meshes& meshes, CameraComponent& camera) : mMeshes(meshes), mCamera(camera) {};
        virtual ~TerrainQuadTreeSystem() {};

        void Init() override;
        void Update(double dt) override;
        void UpdateOnGpu(Microsoft::WRL::ComPtr<ID3D12Device> device, uint32_t currentFrame);
       
        inline void SetMeshes(Meshes& meshes) { mMeshes = meshes; };
        inline void SetCamera(CameraComponent& camera) { mCamera = camera; };
    protected:
        using Index    = uint32_t;
        using Indices  = std::vector<Index>;
        using Vertices = std::vector<Vertex>;

        std::vector<RQuadTreeTerrain*> ComputeQuadTree
        (
            const DirectX::XMFLOAT3& camPos,
            const DirectX::BoundingFrustum& frustum,
            std::unique_ptr<RQuadTreeTerrain>& rootNode,
            const TerrainQTComponent& tc
        ) const;
        void RequestMesh(const std::vector<RQuadTreeTerrain*> requests, TerrainQTComponent& tc);
        void OnEntityRemoved(ECS::Entity entity) override;

        Meshes& mMeshes;
        CameraComponent& mCamera;
    };

	class TerrainQTMorphSystem : public ECS::ECSSystem<TerrainQTComponent>
    {
    public:
        static void   MetricResetCountMean();
        static double MetricGetVertexCountMean();

        TerrainQTMorphSystem(Meshes& meshes, CameraComponent& camera, VT::VTDesc& vtDesc) : mMeshes(meshes), mCamera(camera), mInfo(vtDesc) {};
        virtual ~TerrainQTMorphSystem() {};

        void Init() override;
        void Update(double dt) override;
        void UpdateOnGpu(Microsoft::WRL::ComPtr<ID3D12Device> device, uint32_t currentFrame);
        void FillTerrainMaterialLayersStructuredBuffer(UploadBuffer<TerrainMaterialLayerConstants>* buffer);

        const std::unordered_map<uint32_t, uint32_t>& GetLastRequests() const { return mRequests; }
       
        inline void SetMeshes(Meshes& meshes) { mMeshes = meshes; };
        inline void SetCamera(CameraComponent& camera) { mCamera = camera; };
        inline void SetVTDesc(const VT::VTDesc& vtDesc) { mInfo = vtDesc; };
    protected:
        static double sMetricVertexCountAcc;
        static size_t sMetricNumTimes;

        using Index    = uint32_t;
        using Indices  = std::vector<Index>;
        using Vertices = std::vector<Vertex>;

        std::vector<RQuadTreeTerrain*> ComputeQuadTree
        (
            const DirectX::XMFLOAT3& camPos,
            const DirectX::BoundingFrustum& frustum,
            std::unique_ptr<RQuadTreeTerrain>& rootNode,
            const TerrainQTComponent& tc
        ) const;
        void RequestMesh(const DirectX::XMFLOAT2& camPos, const std::vector<RQuadTreeTerrain*> requests, TerrainQTComponent& tc);
        void OnEntityRemoved(ECS::Entity entity) override;

        VT::LightPageIndexer mIndexer{};
        VT::VTDesc& mInfo;
        std::unordered_map<uint32_t, uint32_t> mRequests;
        Meshes& mMeshes;
        CameraComponent& mCamera;
    };
}