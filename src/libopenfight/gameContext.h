#ifndef OPEN_FIGHT_GAME_CONTEXT_H
#define OPEN_FIGHT_GAME_CONTEXT_H

#include "fileReader.h"
#include "input.h"
#include "playerAgent.h"
#include "playerBridge.h"
#include "renderBackend.h"
#include "sprite.h"
#include <memory>
#include <string>
#include <vector>

struct GameSetup {
    std::string player_one_path;
    std::string player_two_path;
    std::string moves_path;
    std::string background_path;
};

struct FrameInput {
    bool players[2][KEY_MAX] = {{false}};
    bool pause = false;
    bool start = false; // Enter / Space: menu confirm
};

enum class MatchPhase { ROUND_INTRO, FIGHT_INTRO, FIGHTING, KO };
enum class GameScreen { TITLE, MATCH, RESULT };

class GameContext {
private:
    static const int ROUNDS_TO_WIN = 2;
    static const int ROUND_INTRO_TICKS = 120;
    static const int FIGHT_INTRO_TICKS = 80;
    static const int KO_TICKS = 300;
    static const int RESULT_LOCK_TICKS = 90;
    static const int ROUND_SECONDS = 99;
    static constexpr float MIN_FIGHTER_GAP = 10.0f;
    static const int TICKS_PER_SECOND = 100; // main loop fixed step is 10 ms

    PlayerAgent player_one;
    PlayerAgent player_two;
    Sprite background;
    PlayerBridge bridge;
    bool initialized;

    GameScreen screen;
    int screen_ticks;
    int mode_index;
    bool vs_cpu;
    int winner;
    bool perfect_round;
    bool time_over;
    int timer_ticks;
    bool start_latch;
    bool nav_latch;
    bool paused;
    int pause_index;
    bool pause_latch;
    unsigned int ai_seed;
    int ai_hold;
    bool ai_keys[KEY_MAX];
    MatchPhase phase;
    int phase_ticks;
    int round_number;
    int wins[2];
    int loser;
    float shown_health[2];
    Sprite sign_round[3];
    Sprite sign_fight;
    Sprite sign_title;
    Sprite sign_press_start;
    Sprite sign_wins[2];
    Sprite sign_perfect;
    Sprite sign_timeover;
    Sprite sign_draw;
    Sprite digits[10];
    Sprite sign_mode[2];
    Sprite sign_hint_title;
    Sprite sign_hint_result;
    Sprite sign_paused;
    Sprite sign_menu[3];
    Sprite sign_hint_pause;
    Sprite sign_ko;
    Sprite label_p1;
    Sprite label_p2;

    void updateObjects();
    void updateMatch();
    void separateFighters();
    void startRound();
    void startMatch();
    void updateCpu(bool keys[KEY_MAX]);
    int aiRandom(int n);
    void updateTitle(const FrameInput &input);
    void updateResult(const FrameInput &input);
    void updatePause(const FrameInput &input);
    void drawPause(RenderBackend &renderer);
    bool anyStartPressed(const FrameInput &input);
    void drawTitle(RenderBackend &renderer);
    void drawResult(RenderBackend &renderer);
    void clearTemporaryObjects();
    void drawHealthBar(RenderBackend &renderer, Player *player, int side);
    void drawSign(RenderBackend &renderer, Sprite &sign, int duration, float y = 55.0f);
    void drawHud(RenderBackend &renderer);
    void drawTimer(RenderBackend &renderer);
    void endRound(int round_loser);
    bool players_health_full(int index);

public:
    GameContext();
    ~GameContext();

    bool setup(const GameSetup &setup, RenderBackend &renderer,
               const ReadTextFileCallback &read_text_file = ReadTextFileCallback());
    void update(const FrameInput &input, float delta_ms);
    void render(RenderBackend &renderer);
    void shutdown();
};

#endif
