#include "Meshes.h"
#include "BufferCreator.h"
#include "ObjReader.h"

ProTerGen::Mesh* ProTerGen::Meshes::LoadMeshFromFile(std::wstring path)
{
	Mesh* result = new Mesh();

	ObjReader objReader{};
	bool isDefinedByQuads = false;
	bool readed = objReader.ReadObj(path, *result, isDefinedByQuads);
	if (!readed)
	{
		wprintf(L"Error while reading object.\nPath: %s\n", path.c_str());;
		delete result;
		return nullptr;
	}
	if (isDefinedByQuads)
	{
		wprintf(L"Can't read a quad defined mesh.\nPath: %s\n", path.c_str());
		delete result;
		return nullptr;
	}
	return result;
}

ProTerGen::MeshGpu& ProTerGen::Meshes::CreateNewMeshGpu(std::string name)
{
	mMeshes.emplace(name, MeshGpu{});

    return mMeshes[name];
}

void ProTerGen::Meshes::RemoveMeshGpu(std::string name)
{
    mMeshes.erase(name);
}

ProTerGen::MeshGpu& ProTerGen::Meshes::GetMeshGpu(std::string name)
{
    return mMeshes[name];
}

void ProTerGen::Meshes::LoadMeshesInGpu
(
	Microsoft::WRL::ComPtr<ID3D12Device>              device, 
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList, 
	const std::vector<Mesh*>&                         meshes, 
	const std::string                                 name
)
{
	if (meshes.empty()) return;

	if (mMeshes.count(name) < 1)
	{
		mMeshes.emplace(name, MeshGpu{});
	}
	auto& meshGpu = mMeshes.at(name);	
	
	std::vector<Vertex> vertices;
	std::vector<Index> indices;

	size_t vertexCount = 0;
	size_t indexCount = 0;

	for (Mesh* mesh : meshes)
	{
		SubmeshParameters submesh
		{
			.IndexCount         = mesh->Indices.size(),
			.StartIndexLocation = indexCount,
			.BaseVertexLocation = vertexCount
		};

		vertexCount += mesh->Vertices.size();
		indexCount  += mesh->Indices.size();

		if (mesh->Name != "")
		{
			meshGpu.SubMesh.emplace(mesh->Name, submesh);
		}

		vertices.insert(vertices.end(), mesh->Vertices.begin(), mesh->Vertices.end());
		indices.insert(indices.end(), mesh->Indices.begin(), mesh->Indices.end());

		mesh->Vertices.clear();
		mesh->Indices.clear();
	}
	meshGpu.SubMesh.emplace("", SubmeshParameters{ indexCount, 0, 0 });

	const uint32_t vbByteSize = static_cast<uint32_t>(vertices.size()) * sizeof(Vertex);
	const uint32_t ibByteSize = static_cast<uint32_t>(indices.size()) * sizeof(Index);

	//ThrowIfFailed(D3DCreateBlob(vbByteSize, &meshGpu->VertexBufferCPU));
	//CopyMemory(meshGpu->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	//ThrowIfFailed(D3DCreateBlob(ibByteSize, &meshGpu->IndexBufferCPU));
	//CopyMemory(meshGpu->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	BufferCreator bc = BufferCreator();
	meshGpu.VertexBufferGPU = bc.CreateDefaultBuffer
	(
		device,
		commandList, 
		vertices.data(),
		vbByteSize,
		meshGpu.VertexBufferUploader
	);

	meshGpu.IndexBufferGPU = bc.CreateDefaultBuffer
	(
		device,
		commandList, 
		indices.data(), 
		ibByteSize, 
		meshGpu.IndexBufferUploader
	);

	meshGpu.VertexByteStride     = sizeof(Vertex);
	meshGpu.VertexBufferByteSize = vbByteSize;
	meshGpu.IndexBufferByteSize  = ibByteSize;
}

ProTerGen::MeshGpuLocations ProTerGen::Meshes::GetMeshGpuLocation(std::string meshName, std::string submeshName)
{
	auto& meshGpu = mMeshes.at(meshName);
	return MeshGpuLocations
	{
		.Vbv = meshGpu.VertexBufferView(),
		.Ibv = meshGpu.IndexBufferView(),
		.Instances = meshGpu.InstanceBufferView(),
		.MeshRenderValues = meshGpu.SubMesh[submeshName]
	};
}

