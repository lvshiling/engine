#include "ShapeTool.h"
#include "sauce/ShapeToolInjector.h"
#include "video/Shader.h"
#include "video/Color.h"
#include "video/GLDebug.h"
#include "ui/WorldParametersWindow.h"
#include "ui/TreeParametersWindow.h"
#include "frontend/Movement.h"

// tool for testing the world createXXX functions without starting the application
ShapeTool::ShapeTool(io::FilesystemPtr filesystem, core::EventBusPtr eventBus, voxel::WorldPtr world) :
		ui::UIApp(filesystem, eventBus), _worldRenderer(world), _world(world) {
	init("engine", "shapetool");
}

ShapeTool::~ShapeTool() {
	core::Command::unregisterCommand("+move_right");
	core::Command::unregisterCommand("+move_left");
	core::Command::unregisterCommand("+move_upt");
	core::Command::unregisterCommand("+move_down");
}

core::AppState ShapeTool::onInit() {
	core::AppState state = ui::UIApp::onInit();
	GLDebug::enable(GLDebug::Medium);

	if (!_worldShader.init()) {
		return core::Cleanup;
	}

	registerMoveCmd("+move_right", MOVERIGHT);
	registerMoveCmd("+move_left", MOVELEFT);
	registerMoveCmd("+move_forward", MOVEFORWARD);
	registerMoveCmd("+move_backward", MOVEBACKWARD);

	_world->setSeed(1);
	_worldRenderer.onInit();
	_camera.init(_width, _height);
	_camera.setAngles(-M_PI_2, M_PI);
	_camera.setPosition(glm::vec3(0.0f, 100.0f, 0.0f));

	_clearColor = video::Color::LightBlue;

	// TODO: replace this with a scripting interface for the World::create* functions
	_worldRenderer.onSpawn(_camera.getPosition(), 2);

	new WorldParametersWindow(this);
	new TreeParametersWindow(this);

	return state;
}

void ShapeTool::beforeUI() {
	_world->onFrame(_deltaFrame);

	if (_resetTriggered && !_world->isReset()) {
		_world->setContext(_ctx);
		_worldRenderer.onSpawn(_camera.getPosition());
		_resetTriggered = false;
	}

	const bool left = _moveMask & MOVELEFT;
	const bool right = _moveMask & MOVERIGHT;
	const bool forward = _moveMask & MOVEFORWARD;
	const bool backward = _moveMask & MOVEBACKWARD;
	_camera.updatePosition(_deltaFrame, left, right, forward, backward);
	_camera.updateViewMatrix();

	_worldRenderer.onRunning(_now);

	const glm::mat4& view = _camera.getViewMatrix();
	_worldRenderer.renderWorld(_worldShader, view, _aspect);
}

core::AppState ShapeTool::onCleanup() {
	_worldRenderer.onCleanup();
	core::AppState state = UIApp::onCleanup();
	_world->destroy();
	return state;
}

void ShapeTool::onMouseMotion(int32_t x, int32_t y, int32_t relX, int32_t relY) {
	UIApp::onMouseMotion(x, y, relX, relY);
	_camera.onMotion(x, y, relX, relY);
}

void ShapeTool::placeTree(const voxel::World::TreeContext& ctx) {
	_world->placeTree(ctx);
	regenerate(ctx.pos);
	// TODO: might have affected more than one chunk
}

void ShapeTool::regenerate(const glm::ivec2& pos) {
	_worldRenderer.extractNewMeshes(glm::ivec3(pos.x, 0, pos.y), true);
}

void ShapeTool::reset(const voxel::World::WorldContext& ctx) {
	_ctx = ctx;
	_worldRenderer.reset();
	_world->reset();
	_resetTriggered = true;
}

int main(int argc, char *argv[]) {
	return getInjector()->get<ShapeTool>()->startMainLoop(argc, argv);
}
