#include "FlameWork.h"
#include "Game.h"
#include <memory>

/// <summary>
/// 繧｢繝励Μ繧ｱ繝ｼ繧ｷ繝ｧ繝ｳ縺ｮ繧ｨ繝ｳ繝医Μ繝ｼ繝昴う繝ｳ繝医→縺励※繧ｲ繝ｼ繝繧定ｵｷ蜍輔＠縲∫ｵゆｺ・∪縺ｧ螳溯｡後＠縺ｾ縺吶・
/// </summary>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

	std::unique_ptr<FlameWork> game = std::make_unique<Game>();
	game->Run();

	return 0;
}
