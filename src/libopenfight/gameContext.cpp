#include "gameContext.h"
#include "audioBackend.h"
#include <cmath>
#include "graphicsCore.h"
#include "utilities.h"
#include <list>

using namespace std;

GameContext::GameContext()
    : initialized(false), screen(GameScreen::TITLE), screen_ticks(0), mode_index(0), vs_cpu(false), winner(0),
      perfect_round(false), time_over(false), timer_ticks(0), start_latch(true), nav_latch(true), paused(false), pause_index(0),
      pause_latch(false), ai_seed(12345u), ai_hold(0), ai_keys{false},
      phase(MatchPhase::ROUND_INTRO), phase_ticks(0), round_number(1), wins{0, 0}, loser(-1),
      shown_health{100.0f, 100.0f} {
}

GameContext::~GameContext() {
    shutdown();
}

bool GameContext::setup(const GameSetup &setup, RenderBackend &renderer, const ReadTextFileCallback &read_text_file) {
    if (initialized)
        shutdown();

    texture_manager->setRenderBackend(&renderer);

    player_one.initialize(setup.player_one_path, setup.moves_path, true, read_text_file);
    player_two.initialize(setup.player_two_path, setup.moves_path, false, read_text_file);

    auto p2 = player_two.getPlayer().get();
    auto p1 = player_one.getPlayer().get();
    player_one.getPlayer()->setOpponent(p2);
    player_two.getPlayer()->setOpponent(p1);

    unsigned int texture = texture_manager->addTexture(setup.background_path, false);
    background.setTexture(texture, 200, 100);

    bridge.initialize(p1, p2);

    object_manager->add("0", p1);
    object_manager->add("1", p2);

    for (int i = 0; i < 3; i++)
        sign_round[i].setTexture(texture_manager->addTexture("data/ui/round" + to_string(i + 1) + ".png", false), 80, 20);
    sign_fight.setTexture(texture_manager->addTexture("data/ui/fight.png", false), 80, 20);
    sign_title.setTexture(texture_manager->addTexture("data/ui/title.png", false), 80, 20);
    sign_ko.setTexture(texture_manager->addTexture("data/ui/ko.png", false), 80, 20);
    label_p1.setTexture(texture_manager->addTexture("data/ui/p1.png", false), 20, 5);
    label_p2.setTexture(texture_manager->addTexture("data/ui/p2.png", false), 20, 5);
    sign_press_start.setTexture(texture_manager->addTexture("data/ui/press_start.png", false), 80, 20);
    sign_wins[0].setTexture(texture_manager->addTexture("data/ui/p1wins.png", false), 80, 20);
    sign_wins[1].setTexture(texture_manager->addTexture("data/ui/p2wins.png", false), 80, 20);
    sign_perfect.setTexture(texture_manager->addTexture("data/ui/perfect.png", false), 80, 20);
    sign_timeover.setTexture(texture_manager->addTexture("data/ui/timeover.png", false), 80, 20);
    sign_draw.setTexture(texture_manager->addTexture("data/ui/draw.png", false), 80, 20);
    for (int i = 0; i < 10; i++)
        digits[i].setTexture(texture_manager->addTexture("data/ui/digit" + to_string(i) + ".png", false), 6, 8);
    sign_mode[0].setTexture(texture_manager->addTexture("data/ui/mode_2p.png", false), 80, 20);
    sign_mode[1].setTexture(texture_manager->addTexture("data/ui/mode_cpu.png", false), 80, 20);
    sign_hint_title.setTexture(texture_manager->addTexture("data/ui/hint_title.png", false), 200, 10);
    sign_hint_result.setTexture(texture_manager->addTexture("data/ui/hint_result.png", false), 80, 20);
    sign_paused.setTexture(texture_manager->addTexture("data/ui/paused.png", false), 80, 20);
    sign_menu[0].setTexture(texture_manager->addTexture("data/ui/menu_resume.png", false), 80, 20);
    sign_menu[1].setTexture(texture_manager->addTexture("data/ui/menu_rematch.png", false), 80, 20);
    sign_menu[2].setTexture(texture_manager->addTexture("data/ui/menu_quit.png", false), 80, 20);
    sign_hint_pause.setTexture(texture_manager->addTexture("data/ui/hint_pause.png", false), 80, 20);
    audio->playMusic("music_loop");

    round_number = 1;
    wins[0] = wins[1] = 0;
    startRound();

    initialized = true;
    return true;
}

void GameContext::startRound() {
    phase = MatchPhase::ROUND_INTRO;
    time_over = false;
    perfect_round = false;
    timer_ticks = ROUND_SECONDS * TICKS_PER_SECOND;
    audio->playSound("round");
    phase_ticks = 0;
    loser = -1;
    shown_health[0] = shown_health[1] = static_cast<float>(player_one.getPlayer()->getMaxHealth());
}

void GameContext::clearTemporaryObjects() {
    while (true) {
        string victim;
        string key = object_manager->first();
        while (key != "") {
            if (key != "0" && key != "1") {
                victim = key;
                break;
            }
            key = object_manager->next();
        }
        if (victim.empty())
            break;
        object_manager->remove(victim);
    }
}

void GameContext::update(const FrameInput &input, float delta_ms) {
    (void)delta_ms;
    if (!initialized)
        return;

    bool player_one_keys[KEY_MAX];
    bool player_two_keys[KEY_MAX];
    screen_ticks++;
    if (screen == GameScreen::TITLE) {
        updateTitle(input);
        return;
    }
    if (screen == GameScreen::RESULT) {
        updateResult(input);
        return;
    }

    bool pause_edge = input.pause && !pause_latch;
    pause_latch = input.pause;
    if (pause_edge && !paused) {
        paused = true;
        pause_index = 0;
        nav_latch = true;
        start_latch = true;
        audio->playSound("block");
        return;
    }
    if (paused) {
        if (pause_edge)
            paused = false;
        else
            updatePause(input);
        return;
    }

    bool accept_input = phase == MatchPhase::FIGHTING;

    for (int i = 0; i < KEY_MAX; i++) {
        player_one_keys[i] = accept_input && input.players[0][i];
        player_two_keys[i] = accept_input && input.players[1][i];
    }
    if (vs_cpu && accept_input)
        updateCpu(player_two_keys);

    player_one.update(player_one_keys);
    player_two.update(player_two_keys);
    separateFighters();
    updateObjects();
    updateMatch();
}

// Push box: grounded fighters cannot stand inside each other.
void GameContext::separateFighters() {
    Player *a = player_one.getPlayer().get();
    Player *b = player_two.getPlayer().get();
    if (!a->isGrounded() || !b->isGrounded())
        return;
    float dx = b->getXpos() - a->getXpos();
    float gap = dx < 0 ? -dx : dx;
    if (gap >= MIN_FIGHTER_GAP)
        return;
    float push = (MIN_FIGHTER_GAP - gap) / 2.0f;
    float dir = dx >= 0 ? 1.0f : -1.0f;
    a->nudgeX(-dir * push);
    b->nudgeX(dir * push);
}

void GameContext::updateMatch() {
    Player *players[2] = {player_one.getPlayer().get(), player_two.getPlayer().get()};
    phase_ticks++;

    for (int i = 0; i < 2; i++) {
        float health = static_cast<float>(players[i]->getHealth());
        if (health >= shown_health[i])
            shown_health[i] = health;
        else
            shown_health[i] -= max(0.15f, (shown_health[i] - health) * 0.05f);
    }

    switch (phase) {
    case MatchPhase::ROUND_INTRO:
        if (phase_ticks >= ROUND_INTRO_TICKS) {
            phase = MatchPhase::FIGHT_INTRO;
            phase_ticks = 0;
            audio->playSound("fight");
        }
        break;
    case MatchPhase::FIGHT_INTRO:
        if (phase_ticks >= FIGHT_INTRO_TICKS) {
            phase = MatchPhase::FIGHTING;
            phase_ticks = 0;
        }
        break;
    case MatchPhase::FIGHTING:
        for (int i = 0; i < 2; i++) {
            if (players[i]->isDefeated()) {
                endRound(i);
                break;
            }
        }
        if (phase == MatchPhase::FIGHTING && --timer_ticks <= 0) {
            time_over = true;
            int h0 = players[0]->getHealth();
            int h1 = players[1]->getHealth();
            endRound(h0 == h1 ? -1 : (h0 < h1 ? 0 : 1));
        }
        break;
    case MatchPhase::KO: {
        if (loser >= 0 && !time_over && players[loser]->getCurrentState() == "idle")
            players[loser]->updateState("KNOCK_DOWN", true);

        if (phase_ticks >= KO_TICKS) {
            clearTemporaryObjects();
            if (loser >= 0 && wins[1 - loser] >= ROUNDS_TO_WIN) {
                winner = 1 - loser;
                screen = GameScreen::RESULT;
                screen_ticks = 0;
                start_latch = true;
                audio->playSound("round");
            } else {
                players[0]->reset();
                players[1]->reset();
                round_number++;
                startRound();
            }
        }
        break;
    }
    }
}

void GameContext::updateObjects() {
    string key = object_manager->first();
    while (key != "") {
        bool done = false;

        if (key != "0" && key != "1")
            done = object_manager->get(key)->update();

        if (done) {
            object_manager->remove(key);
            break;
        }

        key = object_manager->next();
    }
}

void GameContext::render(RenderBackend &renderer) {
    if (!initialized)
        return;

    bridge.update();
    background.draw(renderer, 50, 50, 1, false);

    if (screen != GameScreen::TITLE) {
        list<Player *> objects = object_manager->getSortedList();
        for (auto i = objects.begin(); i != objects.end(); ++i)
            (*i)->draw(renderer);
    }

    if (screen == GameScreen::TITLE)
        drawTitle(renderer);
    else if (screen == GameScreen::RESULT)
        drawResult(renderer);
    else
        drawHud(renderer);
    if (paused && screen == GameScreen::MATCH)
        drawPause(renderer);
}

bool GameContext::anyStartPressed(const FrameInput &input) {
    if (input.start)
        return true;
    for (int p = 0; p < 2; p++)
        for (int k = KEY_A; k < KEY_MAX; k++)
            if (input.players[p][k])
                return true;
    return false;
}

int GameContext::aiRandom(int n) {
    ai_seed = ai_seed * 1103515245u + 12345u;
    return static_cast<int>((ai_seed >> 16) % static_cast<unsigned int>(n));
}

// Simple reactive CPU for player two: close distance, poke in range, block/jump when the human attacks.
void GameContext::updateCpu(bool keys[KEY_MAX]) {
    Player *me = player_two.getPlayer().get();
    Player *foe = player_one.getPlayer().get();
    float dx = foe->getXpos() - me->getXpos();
    float dist = dx < 0 ? -dx : dx;
    int toward = dx > 0 ? KEY_RIGHT : KEY_LEFT;
    int away = dx > 0 ? KEY_LEFT : KEY_RIGHT;
    string foe_state = foe->getCurrentState();
    bool foe_attacking = foe_state == "1punch" || foe_state == "2punch" || foe_state == "kick" ||
                         foe_state == "back_kick" || foe_state == "jump_kick" || foe_state == "duck_punch" ||
                         foe_state == "fireball";

    if (ai_hold > 0) {
        ai_hold--;
    } else {
        for (int i = 0; i < KEY_MAX; i++)
            ai_keys[i] = false;
        int roll = aiRandom(100);

        if (foe_attacking && dist < 40 && roll < 55) {
            ai_keys[away] = true; // block (hold back)
            if (roll < 20)
                ai_keys[KEY_DOWN] = true;
            ai_hold = 14 + aiRandom(10);
        } else if (dist > 34) {
            if (roll < 12) {
                ai_keys[KEY_UP] = true;
                ai_keys[toward] = true;
                ai_hold = 20;
            } else if (roll < 20) {
                ai_keys[KEY_DOWN] = true;
                ai_hold = 10;
            } else {
                ai_keys[toward] = true;
                ai_hold = 6 + aiRandom(10);
            }
        } else if (dist > 18) {
            if (roll < 45) {
                ai_keys[toward] = true;
                ai_hold = 4 + aiRandom(6);
            } else if (roll < 75) {
                ai_keys[roll < 60 ? KEY_C : KEY_Z] = true; // heavy punch / heavy kick reach
                ai_hold = 12;
            } else {
                ai_hold = 6 + aiRandom(8); // wait
            }
        } else {
            if (roll < 60) {
                static const int attacks[] = {KEY_A, KEY_B, KEY_C, KEY_X, KEY_Y, KEY_Z};
                ai_keys[attacks[aiRandom(6)]] = true;
                if (roll < 15)
                    ai_keys[KEY_DOWN] = true;
                ai_hold = 8 + aiRandom(8);
            } else if (roll < 80) {
                ai_keys[away] = true;
                ai_hold = 10 + aiRandom(10);
            } else {
                ai_keys[KEY_UP] = true;
                ai_keys[away] = true;
                ai_hold = 20;
            }
        }
    }

    for (int i = 0; i < KEY_MAX; i++)
        keys[i] = ai_keys[i];
    // Attack buttons must be tapped, not held, or the engine sees a single press.
    if (ai_hold % 2 == 1)
        for (int k = KEY_A; k < KEY_MAX; k++)
            keys[k] = false;
}

void GameContext::startMatch() {
    vs_cpu = mode_index == 1;
    wins[0] = wins[1] = 0;
    round_number = 1;
    perfect_round = false;
    clearTemporaryObjects();
    player_one.getPlayer()->reset();
    player_two.getPlayer()->reset();
    screen = GameScreen::MATCH;
    screen_ticks = 0;
    paused = false;
    startRound();
}

void GameContext::updatePause(const FrameInput &input) {
    bool nav_up = input.players[0][KEY_UP] || input.players[1][KEY_UP];
    bool nav_down = input.players[0][KEY_DOWN] || input.players[1][KEY_DOWN];
    if ((nav_up || nav_down) && !nav_latch) {
        pause_index = (pause_index + (nav_down ? 1 : 2)) % 3;
        audio->playSound("block");
    }
    nav_latch = nav_up || nav_down;

    bool start = anyStartPressed(input);
    if (start && !start_latch) {
        audio->playSound("fight");
        if (pause_index == 0) {
            paused = false;
        } else if (pause_index == 1) {
            startMatch();
        } else {
            paused = false;
            screen = GameScreen::TITLE;
            screen_ticks = 0;
            start_latch = true;
        }
    }
    start_latch = start;
}

void GameContext::drawPause(RenderBackend &renderer) {
    renderer.drawRect(0.0f, 0.0f, 200.0f, 100.0f, RenderColor{0.0f, 0.0f, 0.0f, 0.6f});
    sign_paused.draw(renderer, 50.0f, 72.0f, 1.2f, false, 1.0f);
    for (int i = 0; i < 3; i++) {
        bool selected = pause_index == i;
        sign_menu[i].draw(renderer, 50.0f, 50.0f - i * 13.0f, selected ? 0.75f : 0.6f, false, selected ? 1.0f : 0.35f);
    }
}

void GameContext::updateTitle(const FrameInput &input) {
    bool nav_up = input.players[0][KEY_UP] || input.players[1][KEY_UP];
    bool nav_down = input.players[0][KEY_DOWN] || input.players[1][KEY_DOWN];
    if ((nav_up || nav_down) && !nav_latch) {
        int next = nav_down ? min(mode_index + 1, 1) : max(mode_index - 1, 0);
        if (next != mode_index)
            audio->playSound("block");
        mode_index = next;
    }
    nav_latch = nav_up || nav_down;

    bool start = anyStartPressed(input);
    if (start && !start_latch && screen_ticks > 30) {
        audio->playSound("fight");
        startMatch();
    }
    start_latch = start;
}

void GameContext::updateResult(const FrameInput &input) {
    bool start = anyStartPressed(input);
    if (start && !start_latch && screen_ticks > RESULT_LOCK_TICKS) {
        screen = GameScreen::TITLE;
        screen_ticks = 0;
        start_latch = true;
    } else {
        start_latch = start;
    }
}

void GameContext::drawTitle(RenderBackend &renderer) {
    renderer.drawRect(0.0f, 0.0f, 200.0f, 100.0f, RenderColor{0.0f, 0.0f, 0.0f, 0.6f});
    float pulse = 1.0f + 0.03f * static_cast<float>(sin(screen_ticks * 0.08));
    sign_title.draw(renderer, 50.0f, 72.0f, 1.9f * pulse, false, 1.0f);
    for (int i = 0; i < 2; i++) {
        bool selected = mode_index == i;
        sign_mode[i].draw(renderer, 50.0f, 46.0f - i * 13.0f, selected ? 0.75f : 0.6f, false, selected ? 1.0f : 0.35f);
    }
    if ((screen_ticks / 30) % 2 == 0)
        sign_press_start.draw(renderer, 50.0f, 18.0f, 0.6f, false, 1.0f);
    sign_hint_title.draw(renderer, 50.0f, 5.0f, 1.0f, false, 0.9f);
    sign_hint_pause.draw(renderer, 50.0f, 11.0f, 0.45f, false, 0.7f);
}

void GameContext::drawResult(RenderBackend &renderer) {
    drawHealthBar(renderer, player_one.getPlayer().get(), 0);
    drawHealthBar(renderer, player_two.getPlayer().get(), 1);
    renderer.drawRect(0.0f, 0.0f, 200.0f, 100.0f, RenderColor{0.0f, 0.0f, 0.0f, 0.45f});
    float pop = screen_ticks < 15 ? 1.0f + (15 - screen_ticks) * 0.05f : 1.0f;
    sign_wins[winner].draw(renderer, 50.0f, 58.0f, 1.3f * pop, false, 1.0f);
    if (screen_ticks > RESULT_LOCK_TICKS && (screen_ticks / 30) % 2 == 0)
        sign_hint_result.draw(renderer, 50.0f, 30.0f, 0.7f, false, 1.0f);
}

void GameContext::drawHealthBar(RenderBackend &renderer, Player *player, int side) {
    const float width = 80.0f;
    const float height = 5.0f;
    const float y = 6.0f;
    const float x = side == 0 ? 8.0f : 200.0f - 8.0f - width;
    float max_health = static_cast<float>(player->getMaxHealth());
    float fill = width * player->getHealth() / max_health;
    float lag = width * shown_health[side] / max_health;
    // Remaining health hugs the outer edge; damage eats the bar from the centre of the screen.
    float fill_x = side == 0 ? x : x + width - fill;
    float lag_x = side == 0 ? x : x + width - lag;

    renderer.drawRect(x - 1.0f, y - 1.0f, width + 2.0f, height + 2.0f, {0.0f, 0.0f, 0.0f, 0.85f});
    renderer.drawRect(x, y, width, height, {0.45f, 0.05f, 0.05f, 1.0f});
    renderer.drawRect(lag_x, y, lag, height, {0.95f, 0.35f, 0.15f, 1.0f});
    renderer.drawRect(fill_x, y, fill, height, {1.0f, 0.85f, 0.2f, 1.0f});
    renderer.drawRect(fill_x, y, fill, 1.0f, {1.0f, 1.0f, 0.75f, 0.9f});

    float marker_y = y + height + 2.0f;
    for (int i = 0; i < ROUNDS_TO_WIN; i++) {
        float marker_x = side == 0 ? x + i * 5.0f : x + width - 3.0f - i * 5.0f;
        bool won = i < wins[side];
        renderer.drawRect(marker_x, marker_y, 3.0f, 3.0f, won ? RenderColor{1.0f, 0.85f, 0.2f, 1.0f}
                                                              : RenderColor{0.0f, 0.0f, 0.0f, 0.6f});
    }

    Sprite &label = side == 0 ? label_p1 : label_p2;
    float label_x = side == 0 ? x + width - 10.0f : x + 10.0f;
    label.draw(renderer, label_x - 50.0f, 100.0f - (marker_y + 1.5f), 1.0f, false);
}

void GameContext::drawSign(RenderBackend &renderer, Sprite &sign, int duration, float y) {
    const int pop_ticks = 12;
    const int fade_ticks = 20;
    float pop = phase_ticks < pop_ticks ? 1.0f - static_cast<float>(phase_ticks) / pop_ticks : 0.0f;
    float scale = 1.0f + pop * 0.8f;
    float alpha = 1.0f;
    if (duration > 0 && phase_ticks > duration - fade_ticks)
        alpha = max(0.0f, static_cast<float>(duration - phase_ticks) / fade_ticks);

    sign.draw(renderer, 50.0f, y, scale, false, alpha);
}

void GameContext::endRound(int round_loser) {
    loser = round_loser;
    if (loser >= 0) {
        wins[1 - loser]++;
        perfect_round = !time_over && players_health_full(1 - loser);
    }
    phase = MatchPhase::KO;
    phase_ticks = 0;
    audio->playSound("ko");
}

bool GameContext::players_health_full(int index) {
    Player *p = (index == 0 ? player_one : player_two).getPlayer().get();
    return p->getHealth() >= p->getMaxHealth();
}

void GameContext::drawTimer(RenderBackend &renderer) {
    int seconds = (timer_ticks + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND;
    if (seconds > 99)
        seconds = 99;
    if (seconds < 0)
        seconds = 0;
    float alpha = 1.0f;
    if (seconds <= 10 && phase == MatchPhase::FIGHTING && (timer_ticks / 15) % 2 == 0)
        alpha = 0.45f;
    digits[seconds / 10].draw(renderer, 46.5f, 91.5f, 1.0f, false, alpha);
    digits[seconds % 10].draw(renderer, 53.5f, 91.5f, 1.0f, false, alpha);
}

void GameContext::drawHud(RenderBackend &renderer) {
    drawHealthBar(renderer, player_one.getPlayer().get(), 0);
    drawHealthBar(renderer, player_two.getPlayer().get(), 1);
    drawTimer(renderer);

    switch (phase) {
    case MatchPhase::ROUND_INTRO:
        drawSign(renderer, sign_title, ROUND_INTRO_TICKS, 74.0f);
        drawSign(renderer, sign_round[min(round_number, 3) - 1], ROUND_INTRO_TICKS);
        break;
    case MatchPhase::FIGHT_INTRO:
        drawSign(renderer, sign_fight, FIGHT_INTRO_TICKS);
        break;
    case MatchPhase::KO:
        if (time_over) {
            drawSign(renderer, sign_timeover, KO_TICKS);
            if (loser < 0 && phase_ticks > 40)
                drawSign(renderer, sign_draw, KO_TICKS, 34.0f);
        } else {
            drawSign(renderer, sign_ko, KO_TICKS);
            if (perfect_round && phase_ticks > 40)
                drawSign(renderer, sign_perfect, KO_TICKS, 34.0f);
        }
        break;
    case MatchPhase::FIGHTING:
        break;
    }
}

void GameContext::shutdown() {
    if (!initialized)
        return;

    object_manager->detach("0");
    object_manager->detach("1");
    object_manager->clear();
    texture_manager->clear();
    initialized = false;
}
