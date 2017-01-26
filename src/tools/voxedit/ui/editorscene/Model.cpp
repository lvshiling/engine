#include "Model.h"
#include "voxel/polyvox/VolumeMerger.h"
#include "voxel/polyvox/VolumeCropper.h"
#include "voxel/polyvox/VolumeRotator.h"
#include "voxel/polyvox/VolumeMover.h"
#include "voxel/polyvox/VolumeRescaler.h"
#include "voxel/polyvox//RawVolumeWrapper.h"
#include "voxel/polyvox//RawVolumeMoveWrapper.h"
#include "voxel/generator/NoiseGenerator.h"
#include "voxel/generator/WorldGenerator.h"
#include "voxel/generator/CloudGenerator.h"
#include "voxel/generator/CactusGenerator.h"
#include "voxel/generator/BuildingGenerator.h"
#include "voxel/model/VoxFormat.h"
#include "voxel/model/QBTFormat.h"
#include "voxel/model/QBFormat.h"
#include "core/Random.h"
#include "voxedit-util/tool/Crop.h"
#include "voxedit-util/tool/Expand.h"
#include "voxedit-util/tool/Fill.h"
#include "voxedit-util/ImportHeightmap.h"

namespace voxedit {

Model::Model() :
		_rawVolumeRenderer(true, false, true), _rawVolumeSelectionRenderer(false, false, false) {
}

Model::~Model() {
	shutdown();
}

bool Model::importHeightmap(const std::string& file) {
	voxel::RawVolume* v = modelVolume();
	if (v == nullptr) {
		return false;
	}
	const image::ImagePtr& img = image::loadImage(file, false);
	if (!img->isLoaded()) {
		return false;
	}
	voxedit::importHeightmap(*v, img);
	modified();
	return true;
}

bool Model::save(const std::string& file) {
	if (modelVolume() == nullptr) {
		return false;
	}
	const io::FilePtr& filePtr = core::App::getInstance()->filesystem()->open(std::string(file), io::FileMode::Write);
	if (filePtr->extension() == "qbt") {
		voxel::QBTFormat f;
		if (f.save(modelVolume(), filePtr)) {
			_dirty = false;
			return true;
		}
	} else if (filePtr->extension() == "vox") {
		voxel::VoxFormat f;
		if (f.save(modelVolume(), filePtr)) {
			_dirty = false;
			return true;
		}
	} else if (filePtr->extension() == "qb") {
		voxel::QBFormat f;
		if (f.save(modelVolume(), filePtr)) {
			_dirty = false;
			return true;
		}
	}
	return false;
}

bool Model::load(const std::string& file) {
	const io::FilePtr& filePtr = core::App::getInstance()->filesystem()->open(file);
	if (!(bool)filePtr) {
		Log::error("Failed to open model file %s", file.data());
		return false;
	}
	voxel::RawVolume* newVolume;

	if (filePtr->extension() == "qbt") {
		voxel::QBTFormat f;
		newVolume = f.load(filePtr);
	} else if (filePtr->extension() == "vox") {
		voxel::VoxFormat f;
		newVolume = f.load(filePtr);
	} else if (filePtr->extension() == "qb") {
		voxel::QBFormat f;
		newVolume = f.load(filePtr);
	} else {
		newVolume = nullptr;
	}
	if (newVolume == nullptr) {
		Log::error("Failed to load model file %s", file.c_str());
		return false;
	}
	Log::info("Loaded model file %s", file.c_str());
	undoHandler().clearUndoStates();
	setNewVolume(newVolume);
	modified();
	_dirty = false;
	return true;
}

void Model::select(const glm::ivec3& pos) {
	voxel::RawVolume* selectionVolume = _rawVolumeSelectionRenderer.volume();
	_extractSelection |= _selectionHandler.select(modelVolume(), selectionVolume, pos);
}

void Model::unselectAll() {
	_selectionHandler.unselectAll();
	_rawVolumeSelectionRenderer.volume()->clear();
	_extractSelection = true;
}

void Model::setMousePos(int x, int y) {
	_mouseX = x;
	_mouseY = y;
}

void Model::modified(bool markUndo) {
	if (markUndo) {
		undoHandler().markUndo(modelVolume());
	}
	_dirty = true;
	markExtract();
}

void Model::crop() {
	if (_empty) {
		Log::info("Empty volumes can't be cropped");
		return;
	}
	voxel::RawVolume* newVolume = voxedit::tool::crop(modelVolume());
	if (newVolume == nullptr) {
		return;
	}
	setNewVolume(newVolume);
	modified();
}

void Model::extend(int size) {
	voxel::RawVolume* newVolume = voxedit::tool::expand(modelVolume(), size);
	if (newVolume == nullptr) {
		return;
	}
	setNewVolume(newVolume);
	modified();
}

void Model::scale() {
	// TODO: check that src region boundaries are even
	const voxel::Region& srcRegion = modelVolume()->getRegion();
	const int w = srcRegion.getWidthInVoxels();
	const int h = srcRegion.getHeightInVoxels();
	const int d = srcRegion.getDepthInVoxels();
	const glm::ivec3 maxs(w / 2, h / 2, d / 2);
	voxel::Region region(glm::zero<glm::ivec3>(), maxs);
	voxel::RawVolume* newVolume = new voxel::RawVolume(region);
	voxel::RawVolumeWrapper wrapper(newVolume);
	voxel::rescaleVolume(*modelVolume(), *newVolume);
	setNewVolume(newVolume);
	modified();
}

void Model::fill(int x, int y, int z) {
	const bool overwrite = evalAction() == Action::OverrideVoxel;
	voxedit::tool::fill(*modelVolume(), glm::ivec3(x, y, z), _lockedAxis, _shapeHandler.currentVoxel(), overwrite);
	modified();
}

void Model::executeAction(long now) {
	const Action execAction = evalAction();
	if (execAction == Action::None) {
		Log::warn("Nothing to execute");
		return;
	}

	core_trace_scoped(EditorSceneExecuteAction);
	if (_lastAction == execAction) {
		if (now - _lastActionExecution < _actionExecutionDelay) {
			return;
		}
	}
	_lastAction = execAction;
	_lastActionExecution = now;

	bool extract = false;
	const bool didHit = _result.didHit;
	if (didHit && execAction == Action::CopyVoxel) {
		shapeHandler().setVoxel(getVoxel(_cursorPos));
	} else if (didHit && execAction == Action::SelectVoxels) {
		select(_cursorPos);
	} else if (didHit && execAction == Action::OverrideVoxel) {
		extract = placeCursor();
	} else if (didHit && execAction == Action::DeleteVoxel) {
		extract = setVoxel(_cursorPos, voxel::Voxel());
	} else if (_result.validPreviousVoxel && execAction == Action::PlaceVoxel) {
		extract = placeCursor();
	} else if (didHit && execAction == Action::PlaceVoxel) {
		extract = placeCursor();
	}

	if (!extract) {
		return;
	}
	resetLastTrace();
	modified();
}

void Model::undo() {
	voxel::RawVolume* v = undoHandler().undo();
	if (v == nullptr) {
		return;
	}
	setNewVolume(v);
	modified(false);
}

void Model::redo() {
	voxel::RawVolume* v = undoHandler().redo();
	if (v == nullptr) {
		return;
	}
	setNewVolume(v);
	modified(false);
}

bool Model::placeCursor() {
	if (_shapeHandler.placeCursor(modelVolume(), cursorPositionVolume())) {
		return true;
	}
	return false;
}

void Model::resetLastTrace() {
	_lastRaytraceX = _lastRaytraceY = -1;
}

void Model::setNewVolume(voxel::RawVolume* volume) {
	const voxel::Region& region = volume->getRegion();

	delete _cursorVolume;
	_cursorVolume = new voxel::RawVolume(region);
	setCursorShape(_shapeHandler.cursorShape());

	delete _rawVolumeSelectionRenderer.setVolume(0, new voxel::RawVolume(region));
	delete _rawVolumeRenderer.setVolume(0, volume);
	delete _rawVolumeRenderer.setVolume(1, new voxel::RawVolume(region));

	_dirty = false;
	_lastPlacement = glm::ivec3(-1);
	_result = voxel::PickResult();
	const glm::ivec3& pos = _cursorPos;
	_cursorPos = pos * 10 + 10;
	setCursorPosition(pos);
	resetLastTrace();
}

bool Model::newVolume(bool force) {
	if (dirty() && !force) {
		return false;
	}
	const voxel::Region region(glm::ivec3(0), glm::ivec3(size() - 1));
	undoHandler().clearUndoStates();
	setNewVolume(new voxel::RawVolume(region));
	modified();
	_dirty = false;
	return true;
}

void Model::rotate(int angleX, int angleY, int angleZ) {
	const voxel::RawVolume* model = modelVolume();
	voxel::RawVolume* newVolume = voxel::rotateVolume(model, glm::vec3(angleX, angleY, angleZ), voxel::Voxel(), false);
	setNewVolume(newVolume);
	modified();
}

void Model::move(int x, int y, int z) {
	voxel::RawVolume* model = modelVolume();
	voxel::RawVolume* newVolume = new voxel::RawVolume(model->getRegion());
	voxel::RawVolumeMoveWrapper wrapper(newVolume);
	voxel::moveVolume(&wrapper, model, glm::ivec3(x, y, z), voxel::Voxel());
	setNewVolume(newVolume);
	modified();
}

const voxel::Voxel& Model::getVoxel(const glm::ivec3& pos) const {
	return modelVolume()->getVoxel(pos);
}

bool Model::setVoxel(const glm::ivec3& pos, const voxel::Voxel& voxel) {
	const bool placed = modelVolume()->setVoxel(pos, voxel);
	if (placed) {
		modified();
		_lastPlacement = pos;
	}
	return placed;
}

void Model::copy() {
	voxel::mergeRawVolumesSameDimension(_cursorVolume, _rawVolumeSelectionRenderer.volume());
}

void Model::paste() {
	const voxel::Region& srcRegion = _cursorVolume->getRegion();
	const voxel::Region destRegion = srcRegion + _cursorPos;
	voxel::RawVolumeWrapper wrapper(modelVolume());
	voxel::mergeRawVolumes(&wrapper, _cursorVolume, destRegion, srcRegion);
}

void Model::cut() {
	voxel::mergeRawVolumesSameDimension(_cursorVolume, _rawVolumeSelectionRenderer.volume());
	// TODO: delete selected volume from model volume
}

void Model::render(const video::Camera& camera) {
	const voxel::Mesh* mesh = _rawVolumeRenderer.mesh(0);
	_empty = mesh != nullptr ? mesh->getNoOfIndices() > 0 : true;
	_rawVolumeRenderer.render(camera);
}

void Model::renderSelection(const video::Camera& camera) {
	_rawVolumeSelectionRenderer.render(camera);
}

void Model::onResize(const glm::ivec2& size) {
	_rawVolumeRenderer.onResize(glm::ivec2(), size);
	_rawVolumeSelectionRenderer.onResize(glm::ivec2(), size);
}

void Model::init() {
	if (_initialized > 0) {
		return;
	}
	++_initialized;
	_rawVolumeRenderer.init();
	_rawVolumeSelectionRenderer.init();
}

void Model::update() {
	extractVolume();
	extractCursorVolume();
	extractSelectionVolume();
}

void Model::shutdown() {
	--_initialized;
	if (_initialized > 0) {
		return;
	} else if (_initialized < 0) {
		_initialized = 0;
		return;
	}
	_initialized = 0;
	delete _cursorVolume;
	_cursorVolume = nullptr;
	{
		const std::vector<voxel::RawVolume*>& old = _rawVolumeRenderer.shutdown();
		for (voxel::RawVolume* v : old) {
			delete v;
		}
	}
	{
		const std::vector<voxel::RawVolume*>& old = _rawVolumeSelectionRenderer.shutdown();
		for (voxel::RawVolume* v : old) {
			delete v;
		}
	}

	undoHandler().clearUndoStates();
}

bool Model::extractSelectionVolume() {
	if (_extractSelection) {
		_extractSelection = false;
		_rawVolumeSelectionRenderer.extractAll();
		return true;
	}
	return false;
}

bool Model::extractVolume() {
	if (_extract) {
		_extract = false;
		_rawVolumeRenderer.extract(0);
		return true;
	}
	return false;
}

bool Model::extractCursorVolume() {
	if (_extractCursor) {
		_extractCursor = false;
		_rawVolumeRenderer.extract(1);
		return true;
	}
	return false;
}

void Model::noise(int octaves, float frequency, float persistence) {
	core::Random random;
	voxel::RawVolumeWrapper wrapper(modelVolume());
	voxel::noise::generate(wrapper, octaves, frequency, persistence, random);
	modified();
}

void Model::lsystem(const voxel::lsystem::LSystemContext& lsystemCtx) {
	core::Random random;
	voxel::RawVolumeWrapper wrapper(modelVolume());
	voxel::lsystem::generate(wrapper, lsystemCtx, random);
	modified();
}

void Model::world(const voxel::WorldContext& ctx) {
	const voxel::Region region(glm::ivec3(0), glm::ivec3(127, 63, 127));
	setNewVolume(new voxel::RawVolume(region));
	voxel::BiomeManager mgr;
	const io::FilesystemPtr& filesystem = core::App::getInstance()->filesystem();
	mgr.init(filesystem->load("biomes.lua"));
	voxel::RawVolumeWrapper wrapper(modelVolume());
	voxel::world::createWorld(ctx, wrapper, mgr, 1L, voxel::world::WORLDGEN_CLIENT, 0, 0);
	modified();
}

void Model::createCactus() {
	core::Random random;
	voxel::RawVolumeWrapper wrapper(modelVolume());
	voxel::cactus::createCactus(wrapper, _cursorPos, 18, 2, random);
	modified();
}

void Model::createCloud() {
	core::Random random;
	voxel::RawVolumeWrapper wrapper(modelVolume());
	struct HasClouds {
		inline bool hasClouds(const glm::ivec3&) const {
			return true;
		}
	};
	HasClouds hasClouds;
	voxel::cloud::CloudContext cloudCtx;
	cloudCtx.amount = 1;
	cloudCtx.regionBorder = 2;
	cloudCtx.randomPos = false;
	cloudCtx.pos = _cursorPos;
	voxel::cloud::createClouds(wrapper, hasClouds, cloudCtx, random);
	modified();
}

void Model::createPlant(voxel::PlantType type) {
	voxel::PlantGenerator g;
	voxel::RawVolumeWrapper wrapper(modelVolume());
	if (type == voxel::PlantType::Flower) {
		g.createFlower(5, _cursorPos, wrapper);
	} else if (type == voxel::PlantType::Grass) {
		g.createGrass(10, _cursorPos, wrapper);
	} else if (type == voxel::PlantType::Mushroom) {
		g.createMushroom(7, _cursorPos, wrapper);
	}
	g.shutdown();
	modified();
}

void Model::createBuilding(voxel::BuildingType type, const voxel::BuildingContext& ctx) {
	core::Random random;
	voxel::RawVolumeWrapper wrapper(modelVolume());
	voxel::building::createBuilding(wrapper, _cursorPos, type, random);
	modified();
}

void Model::createTree(voxel::TreeContext ctx) {
	core::Random random;
	voxel::RawVolumeWrapper wrapper(modelVolume());
	ctx.pos = _cursorPos;
	voxel::tree::createTree(wrapper, ctx, random);
	modified();
}

void Model::setCursorPosition(glm::ivec3 pos, bool force) {
	if (!force) {
		if ((_lockedAxis & Axis::X) != Axis::None) {
			pos.x = _cursorPos.x;
		}
		if ((_lockedAxis & Axis::Y) != Axis::None) {
			pos.y = _cursorPos.y;
		}
		if ((_lockedAxis & Axis::Z) != Axis::None) {
			pos.z = _cursorPos.z;
		}
	}

	const voxel::Region& region = modelVolume()->getRegion();
	if (!region.containsPoint(pos)) {
		pos = region.moveInto(pos.x, pos.y, pos.z);
	}
	if (_cursorPos == pos) {
		return;
	}
	_cursorPos = pos;

	voxel::RawVolume* cursorPosVolume = cursorPositionVolume();
	cursorPosVolume->clear();
	static constexpr voxel::Voxel air;
	const std::unique_ptr<voxel::RawVolume> cropped(voxel::cropVolume(_cursorVolume));
	if (cropped) {
		const voxel::Region& srcRegion = cropped->getRegion();
		const voxel::Region& destRegion = cursorPosVolume->getRegion();
		const glm::ivec3& lower = destRegion.getLowerCorner() + _cursorPos - srcRegion.getCentre();
		if (destRegion.containsPoint(lower)) {
			const glm::ivec3& regionUpperCorner = destRegion.getUpperCorner();
			glm::ivec3 upper = lower + srcRegion.getDimensionsInVoxels();
			if (!destRegion.containsPoint(upper)) {
				upper = regionUpperCorner;
			}
			voxel::RawVolumeWrapper wrapper(cursorPosVolume);
			voxel::mergeRawVolumes(&wrapper, cropped.get(), voxel::Region(lower, upper), srcRegion);
		}
	} else {
		Log::error("Failed to crop cursor volume");
	}

	markCursorExtract();
}

void Model::markCursorExtract() {
	_extractCursor = true;
}

void Model::markExtract() {
	_extract = true;
}

bool Model::trace(const video::Camera& camera) {
	if (modelVolume() == nullptr) {
		return false;
	}

	if (_lastRaytraceX != _mouseX || _lastRaytraceY != _mouseY) {
		core_trace_scoped(EditorSceneOnProcessUpdateRay);
		_lastRaytraceX = _mouseX;
		_lastRaytraceY = _mouseY;

		const video::Ray& ray = camera.mouseRay(glm::ivec2(_mouseX, _mouseY));
		const glm::vec3& dirWithLength = ray.direction * camera.farPlane();
		static constexpr voxel::Voxel air;
		_result = voxel::pickVoxel(modelVolume(), ray.origin, dirWithLength, air);

		if (actionRequiresExistingVoxel(evalAction())) {
			if (_result.didHit) {
				setCursorPosition(_result.hitVoxel);
			}
		} else if (_result.validPreviousVoxel) {
			setCursorPosition(_result.previousVoxel);
		}
	}

	return true;
}

}
