/**
 * @file
 */

#pragma once

#include "ui/turbobadger/Window.h"
#include "layer/LayerWindow.h"
#include "settings/SceneSettingsWindow.h"
#include "core/Common.h"
#include "core/String.h"
#include "core/collection/Array.h"
#include "math/Axis.h"

class Viewport;
class VoxEdit;
class PaletteWidget;
class LayerWidget;

namespace voxedit {

/**
 * @brief Voxel editing tools panel
 */
class VoxEditWindow: public ui::turbobadger::Window {
	friend class ::VoxEdit;
private:
	using Super = ui::turbobadger::Window;
	Viewport* _scene = nullptr;
	Viewport* _sceneTop = nullptr;
	Viewport* _sceneLeft = nullptr;
	Viewport* _sceneFront = nullptr;
	Viewport* _sceneAnimation = nullptr;
	tb::TBLayout* _animationWidget = nullptr;
	PaletteWidget* _paletteWidget = nullptr;
	LayerWidget* _layerWidget = nullptr;
	tb::TBWidget* _saveButton = nullptr;
	tb::TBWidget* _saveAnimationButton = nullptr;
	tb::TBWidget* _undoButton = nullptr;
	tb::TBWidget* _redoButton = nullptr;

	tb::TBEditField* _cursorX = nullptr;
	tb::TBEditField* _cursorY = nullptr;
	tb::TBEditField* _cursorZ = nullptr;

	tb::TBEditField* _paletteIndex = nullptr;

	tb::TBCheckBox* _lockedX = nullptr;
	tb::TBCheckBox* _lockedY = nullptr;
	tb::TBCheckBox* _lockedZ = nullptr;

	tb::TBInlineSelect* _translateX = nullptr;
	tb::TBInlineSelect* _translateY = nullptr;
	tb::TBInlineSelect* _translateZ = nullptr;

	tb::TBRadioButton* _mirrorAxisNone = nullptr;
	tb::TBRadioButton* _mirrorAxisX = nullptr;
	tb::TBRadioButton* _mirrorAxisY = nullptr;
	tb::TBRadioButton* _mirrorAxisZ = nullptr;

	tb::TBRadioButton* _selectModifier = nullptr;
	tb::TBRadioButton* _placeModifier = nullptr;
	tb::TBRadioButton* _deleteModifier = nullptr;
	tb::TBRadioButton* _overrideModifier = nullptr;
	tb::TBRadioButton* _colorizeModifier = nullptr;

	std::string _voxelizeFile;
	std::string _loadFile;

	std::string _lastExecutedCommand;

	tb::TBGenericStringItemSource _treeItems;
	tb::TBGenericStringItemSource _fileItems;
	tb::TBGenericStringItemSource _structureItems;
	tb::TBGenericStringItemSource _animationItems;

	tb::TBInlineSelect *_voxelSize = nullptr;
	tb::TBCheckBox *_showGrid = nullptr;
	tb::TBCheckBox *_showAABB = nullptr;
	tb::TBCheckBox *_showAxis = nullptr;
	tb::TBCheckBox *_showLockAxis = nullptr;
	tb::TBCheckBox *_renderShadow = nullptr;

	bool _fourViewAvailable = false;
	bool _animationViewAvailable = false;
	core::VarPtr _lastOpenedFile;

	glm::ivec3 _lastCursorPos;

	LayerSettings _layerSettings;
	SceneSettings _settings;

	core::Array<video::TexturePtr, 4> _backgrounds;

	bool handleEvent(const tb::TBWidgetEvent &ev);

	bool handleClickEvent(const tb::TBWidgetEvent &ev);
	bool handleChangeEvent(const tb::TBWidgetEvent &ev);
	void resetCamera();
	void quit();

	void updateStatusBar();

	void afterLoad(const std::string& file);

	// commands
	void toggleViewport();
	void toggleAnimation();
	bool importHeightmap(const std::string& file);
	bool importAsPlane(const std::string& file);
	bool importPalette(const std::string& file);
	bool save(const std::string& file);
	bool load(const std::string& file);
	bool loadAnimationEntity(const std::string& file);
	bool saveScreenshot(const std::string& file);
	bool prefab(const std::string& file);
	bool createNew(bool force);
public:
	VoxEditWindow(VoxEdit* tool);
	~VoxEditWindow();
	bool init();

	bool isLayerWidgetDropTarget() const;
	bool isPaletteWidgetDropTarget() const;

	void update();
	bool isSceneHovered() const;

	bool onEvent(const tb::TBWidgetEvent &ev) override;
	void onProcess() override;
	void onDie() override;
};

}
