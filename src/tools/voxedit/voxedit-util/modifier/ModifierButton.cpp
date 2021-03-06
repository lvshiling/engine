/**
 * @file
 */

#include "ModifierButton.h"
#include "core/BindingContext.h"
#include "../SceneManager.h"
#include "../CustomBindingContext.h"

namespace voxedit {

ModifierButton::ModifierButton(ModifierType newType) :
		_newType(newType) {
}

bool ModifierButton::handleDown(int32_t key, uint64_t pressedMillis) {
	const bool initialDown = Super::handleDown(key, pressedMillis);
	if (initialDown) {
		Modifier& mgr = sceneMgr().modifier();
		if (_newType != ModifierType::None) {
			_oldType = mgr.modifierType();
			mgr.setModifierType(_newType);
			sceneMgr().trace(true);
		}
		mgr.aabbStart();
	}
	return initialDown;
}

bool ModifierButton::handleUp(int32_t key, uint64_t releasedMillis) {
	const bool allUp = Super::handleUp(key, releasedMillis);
	if (allUp) {
		Modifier& mgr = sceneMgr().modifier();
		LayerManager& layerMgr = sceneMgr().layerMgr();
		layerMgr.foreachGroupLayer([&] (int layerId) {
			Log::debug("Execute modifier action on layer %i", layerId);
			voxel::RawVolume* volume = sceneMgr().volume(layerId);
			mgr.aabbAction(volume, [&] (const voxel::Region& region, ModifierType type) {
				if (type != ModifierType::Select) {
					sceneMgr().modified(layerId, region);
				}
			});
		});
		if (_oldType != ModifierType::None) {
			mgr.setModifierType(_oldType);
			sceneMgr().trace(true);
			_oldType = ModifierType::None;
		}
		mgr.aabbStop();
	} else {
		Log::debug("Not all modifier keys were released - skipped action execution");
	}
	return allUp;
}

}
