/**
 * @file
 * @defgroup Voxel
 * @{
 * @}
 */

#pragma once

#include "core/GLM.h"
#include "core/Common.h"
#include "voxel/Mesh.h"
#include "voxel/PagedVolume.h"
#include "voxel/Raycast.h"
#include "math/Frustum.h"
#include "voxel/Constants.h"
#include "voxel/Picking.h"
#include <memory>
#include <vector>
#include <atomic>
#include <list>

#include "WorldPager.h"
#include "core/io/Filesystem.h"
#include "BiomeManager.h"
#include "core/collection/ConcurrentQueue.h"
#include "core/ThreadPool.h"
#include "core/Var.h"
#include "math/Random.h"
#include "core/Log.h"
#include <unordered_set>

namespace voxelworld {

struct ChunkMeshes {
	static constexpr bool MAY_GET_RESIZED = true;
	ChunkMeshes(int opaqueVertices, int opaqueIndices, int waterVertices, int waterIndices) :
			opaqueMesh(opaqueVertices, opaqueIndices, MAY_GET_RESIZED), waterMesh(waterVertices, waterIndices, MAY_GET_RESIZED) {
	}

	inline const glm::ivec3& translation() const {
		return opaqueMesh.getOffset();
	}

	voxel::Mesh opaqueMesh;
	voxel::Mesh waterMesh;

	inline bool operator<(const ChunkMeshes& rhs) const {
		return glm::all(glm::lessThan(translation(), rhs.translation()));
	}
};

typedef std::unordered_set<glm::ivec3, std::hash<glm::ivec3> > PositionSet;

/**
 * @brief The WorldMgr class is responsible to maintaining the voxel volumes and handle the needed mesh extraction
 * @ingroup Voxel
 */
class WorldMgr {
public:
	enum Result {
		COMPLETED, ///< If the ray passed through the volume without being interrupted
		INTERUPTED, ///< If the ray was interrupted while traveling
		FAILED
	};

	WorldMgr();
	~WorldMgr();

	bool findPath(const glm::ivec3& start, const glm::ivec3& end, std::list<glm::ivec3>& listResult);

	template<typename VoxelTypeChecker>
	int findFloor(int x, int z, VoxelTypeChecker&& check) const {
		const glm::vec3 start(x, voxel::MAX_HEIGHT, z);
		const float distance = (float)voxel::MAX_HEIGHT;
		int y = voxel::NO_FLOOR_FOUND;
		raycast(start, glm::down, distance, [&] (const voxel::PagedVolume::Sampler& sampler) {
			if (check(sampler.voxel().getMaterial())) {
				y = sampler.position().y;
				return false;
			}
			return true;
		});
		return y;
	}

	/**
	 * @return The y component for the given x and z coordinates that is walkable - or @c NO_FLOOR_FOUND.
	 */
	int findWalkableFloor(const glm::vec3& position, float maxDistanceY = (float)voxel::MAX_HEIGHT) const;

	/**
	 * @return true if the ray hit something - false if not.
	 * @note The callback has a parameter of @c const PagedVolume::Sampler& and returns a boolean. If the callback returns false,
	 * the ray is interrupted. Only if the callback returned false at some point in time, this function will return @c true.
	 */
	template<typename Callback>
	inline bool raycast(const glm::vec3& start, const glm::vec3& direction, float maxDistance, Callback&& callback) const {
		const voxel::RaycastResults::RaycastResult result = voxel::raycastWithDirection(_volumeData, start, direction * maxDistance, std::forward<Callback>(callback));
		return result == voxel::RaycastResults::Interupted;
	}

	/**
	 * @return true if the ray hit something - false if not. If true is returned, the position is set to @c hit and
	 * the @c Voxel that was hit is stored in @c voxel
	 * @param[out] hit If the ray hits a voxel, this is the position of the hit
	 * @param[out] voxel The voxel that was hit
	 */
	bool raycast(const glm::vec3& start, const glm::vec3& direction, float maxDistance, glm::ivec3& hit, voxel::Voxel& voxel) const;

	bool init(const std::string& luaParameters, const std::string& luaBiomes, uint32_t volumeMemoryMegaBytes = 512, uint16_t chunkSideLength = 256);
	void shutdown();
	void reset();
	bool isReset() const;

	voxel::VoxelType material(int x, int y, int z) const;

	BiomeManager& biomeManager();
	const BiomeManager& biomeManager() const;

	voxel::PickResult pickVoxel(const glm::vec3& origin, const glm::vec3& directionWithLength);

	/**
	 * @brief Returns a random position inside the boundaries of the world (on the surface)
	 */
	glm::ivec3 randomPos() const;

	/**
	 * @brief Cuts the given world coordinate down to mesh tile vectors
	 */
	glm::ivec3 meshPos(const glm::ivec3& pos) const;

	/**
	 * @brief Cuts the given world coordinate down to chunk tile vectors
	 */
	glm::ivec3 chunkPos(const glm::ivec3& pos) const;

	/**
	 * @brief We need to pop the mesh extractor queue to find out if there are new and ready to use meshes for us
	 * @return @c false if this isn't the case, @c true if the given reference was filled with valid data.
	 */
	bool pop(ChunkMeshes& item);

	void stats(int& meshes, int& extracted, int& pending) const;

	/**
	 * @brief If you don't need an extracted mesh anymore, make sure to allow the reextraction at a later time.
	 * @param[in] pos A WorldMgr vector that is automatically converted into a mesh tile vector
	 * @return @c true if the given position was already extracted, @c false if not.
	 */
	bool allowReExtraction(const glm::ivec3& pos);

	/**
	 * @brief Reorder the scheduled extraction commands that the closest chunks to the given position are handled first
	 */
	void updateExtractionOrder(const glm::ivec3& sortPos);

	/**
	 * @brief Performs async mesh extraction. You need to call @c pop in order to see if some extraction is ready.
	 *
	 * @param[in] pos A WorldMgr vector that is automatically converted into a mesh tile vector
	 * @note This will not allow to reschedule an extraction for the same area until @c allowReExtraction was called.
	 */
	bool scheduleMeshExtraction(const glm::ivec3& pos);

	long seed() const;

	void setSeed(long seed);

	bool created() const;

	void setPersist(bool persist);

	int chunkSize() const;

	glm::ivec3 meshSize() const;

private:
	void extractScheduledMesh();

	WorldPager _pager;
	voxel::PagedVolume *_volumeData = nullptr;
	BiomeManager _biomeManager;
	mutable std::mt19937 _engine;
	long _seed = 0l;

	core::ThreadPool _threadPool;
	core::ConcurrentQueue<ChunkMeshes> _extracted;
	core::ConcurrentQueue<glm::ivec3, VecLessThan<3, int> > _pendingExtraction;
	// fast lookup for positions that are already extracted
	PositionSet _positionsExtracted;
	core::VarPtr _meshSize;
	math::Random _random;
	std::atomic_bool _cancelThreads { false };
};

inline glm::ivec3 WorldMgr::meshPos(const glm::ivec3& pos) const {
	const glm::vec3& size = meshSize();
	const int x = glm::floor(pos.x / size.x);
	const int y = glm::floor(pos.y / size.y);
	const int z = glm::floor(pos.z / size.z);
	return glm::ivec3(x * size.x, y * size.y, z * size.z);
}

inline glm::ivec3 WorldMgr::chunkPos(const glm::ivec3& pos) const {
	const float size = chunkSize();
	const int x = glm::floor(pos.x / size);
	const int y = glm::floor(pos.y / size);
	const int z = glm::floor(pos.z / size);
	return glm::ivec3(x, y, z);
}

inline bool WorldMgr::pop(ChunkMeshes& item) {
	return _extracted.pop(item);
}

inline bool WorldMgr::created() const {
	return _seed != 0;
}

inline void WorldMgr::setPersist(bool persist) {
	_pager.setPersist(persist);
}

inline long WorldMgr::seed() const {
	return _seed;
}

inline BiomeManager& WorldMgr::biomeManager() {
	return _biomeManager;
}

inline const BiomeManager& WorldMgr::biomeManager() const {
	return _biomeManager;
}

typedef std::shared_ptr<WorldMgr> WorldMgrPtr;

}