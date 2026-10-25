#include "player.h"
#include "audioBackend.h"

using namespace std;

Player::Player() {
    current_state = "start";
    opponent = NULL;
    is_hurt = false;
    index = 0;
    health = 100;
    max_health = 100;
    x_start = 0;
    y_start = 0;

    animation_manager = new ObjectManager<Animation>();
    objects = new ObjectManager<Player>();
}

Player::Player(const Player &p) {
    x_pos = p.x_pos;
    y_pos = p.y_pos;
    alpha = p.alpha;
    scale = p.scale;

    x_initial = p.x_initial;
    y_initial = p.y_initial;
    alpha_initial = p.alpha_initial;
    scale_initial = p.scale_initial;

    type = p.type;
    inverted = p.inverted;
    is_hurt = p.is_hurt;
    index = p.index;
    health = p.health;
    max_health = p.max_health;
    x_start = p.x_start;
    y_start = p.y_start;

    current_state = p.current_state;
    previous_state = p.previous_state;

    states = p.states;
    next_state = p.next_state;

    animation_manager = new ObjectManager<Animation>(*p.animation_manager);
    opponent = p.opponent;

    objects = new ObjectManager<Player>(*p.objects);

    state = p.state;
}

Player::~Player() {
    delete animation_manager;
    delete objects;
}

bool Player::operator<(const Player &p) const {
    return this->index < p.index;
}

float Player::getXpos() {
    return x_pos;
}

// Screen-space shift (ignores facing), used to keep the two fighters from overlapping.
void Player::nudgeX(float dx) {
    const float stage_min = -38.0f;
    const float stage_max = 138.0f;
    x_pos = max(stage_min, min(stage_max, x_pos + dx));
}

bool Player::isGrounded() {
    return y_pos <= y_initial + 0.01f;
}

bool Player::isInverted() {
    return inverted;
}

bool Player::isHurt() {
    return is_hurt;
}

int Player::getHealth() {
    return health;
}

int Player::getMaxHealth() {
    return max_health;
}

bool Player::isDefeated() {
    return health <= 0;
}

string Player::getCurrentState() {
    return current_state;
}

void Player::takeDamage(int amount) {
    health = max(0, health - amount);
}

void Player::reset() {
    health = max_health;
    is_hurt = false;
    index = 0;
    x_pos = x_start;
    y_pos = y_start;
    alpha = alpha_initial;
    scale = scale_initial;

    if (animation_manager->get("idle") != NULL) {
        animation_manager->get("idle")->restart();
        previous_state = "idle";
        setCurrentState("idle");
    }
}

// Damage dealt by a hit command once the defender has entered `state`; blocks absorb everything.
static string soundForHit(const string &command, const string &state) {
    if (state.rfind("block", 0) == 0)
        return "block";
    if (command == "HIT_L")
        return "hit_light";
    if (command == "HIT_M")
        return "hit_medium";
    if (command == "HIT_H")
        return "hit_heavy";
    return "knockdown";
}

// Sound played when a fighter enters an attack/jump state on its own (not as a reaction).
static string soundForState(const string &state) {
    if (state == "fireball")
        return "fireball";
    if (state.rfind("jump", 0) == 0 && state != "jump_kick")
        return "jump";
    if (state == "1punch" || state == "2punch" || state == "kick" || state == "back_kick" || state == "duck_punch" ||
        state == "jump_kick" || state == "throw")
        return "whiff";
    return "";
}

static int damageForHit(const string &command, const string &state) {
    if (state.rfind("block", 0) == 0)
        return 0;
    if (command == "HIT_L")
        return 7;
    if (command == "HIT_M")
        return 9;
    if (command == "HIT_H")
        return 11;
    if (command == "KNOCK_DOWN" || command == "THROWN")
        return 15;
    return 0;
}

Collision *Player::getOffense() {
    if (animation_manager->get(current_state) != NULL) {
        return animation_manager->get(current_state)->getOffense();
    }

    return NULL;
}

Collision *Player::getDefense() {
    if (animation_manager->get(current_state) != NULL) {
        return animation_manager->get(current_state)->getDefense();
    }

    return NULL;
}

void Player::setOpponent(Player *opponent) {
    this->opponent = opponent;
}

bool Player::updateState(string command) {
    return updateState(command, false);
}

bool Player::updateState(string command, bool from_opponent) {
    bool result = false;
    bool has_state = false;
    bool is_combo = false;
    bool is_continual = false;

    if (animation_manager->get(current_state) != NULL)
        is_combo = animation_manager->get(current_state)->isCombo();

    has_state = states[current_state][command] != "";

    if (from_opponent && has_state || (is_combo ? opponent->isHurt() && has_state : has_state)) {
        string state = states[current_state][command];

        if (animation_manager->get(state) != NULL) {
            result = true;

            if (from_opponent) {
                is_hurt = true;
                if (type == "player") {
                    takeDamage(damageForHit(command, state));
                    audio->playSound(soundForHit(command, state));
                }
            }

            is_continual = animation_manager->get(state)->isContinual();

            if (!is_continual)
                animation_manager->get(state)->restart();

            setCurrentState(state);
        }

        if (!from_opponent) {
            index = 1;
            opponent->index = 0;
        }
    }

    return result;
}

bool Player::update() {
    bool result = false;
    bool done = false;
    bool changed = false;

    if (animation_manager->get(current_state)->isContinual() && inverted != state["INVERT"]) {
        updateState("INVERT");
        inverted = state["INVERT"];
    }

    doAction(typeid(MoveAction));

    if (next_state[current_state] == "*previous") {
        bool temp;
        if (animation_manager->get(previous_state) != NULL)
            animation_manager->get(previous_state)->nextFrame(temp);
    }

    if (animation_manager->get(current_state) != NULL) {
        animation_manager->get(current_state)->update(x_pos, y_pos, 1, inverted);
        done = animation_manager->get(current_state)->nextFrame(changed);
    }

    if (done) {
        setCurrentState(next_state[current_state]);
        is_hurt = false;

        if (type == "temporary")
            result = true;
    }

    if (opponent != NULL && this->getOffense() != NULL && this->getOffense()->overlaps(opponent->getDefense())) {
        if (type == "projectile") {
            result = true;
            doAction(typeid(CollisionAction));
        } else if (changed) {
            doAction(typeid(CollisionAction));
        }
    }

    if (changed)
        doAction(typeid(CreateObjectAction));

    return result;
}

void Player::createObject(string object) {
    if (!objects->contains(object)) {
        Player *new_object = new Player();

        new_object->initialize(object, !inverted, 0, 0, yaml_loader);
        new_object->setOpponent(opponent);

        objects->add(object, new_object);
    }
}

void Player::copyObject(string object, float x, float y, int index) {
    Player *obj_copy = new Player(*objects->get(object));
    obj_copy->setOpponent(opponent);
    obj_copy->state["INVERT"] = inverted;
    obj_copy->inverted = inverted;
    obj_copy->index = index;
    obj_copy->setLocation(x_pos + (inverted ? -x : x), y_pos + y);

    stringstream sstream;
    sstream << object << static_cast<int>(getGameTime());
    object_manager->add(sstream.str(), obj_copy);
}

void Player::draw(RenderBackend &renderer) {
    if (animation_manager->get(current_state) != NULL)
        animation_manager->get(current_state)->draw(renderer, alpha);
}

void Player::doAction(const type_info &type) {
    Actions *actions;
    bool completed = false;

    if (animation_manager->get(current_state) != NULL)
        actions = animation_manager->get(current_state)->getActions();

    if (actions != NULL)
        completed = actions->doActions(this, opponent, type);

    if (type == typeid(MoveAction) && !completed) {
        if (animation_manager->get(previous_state) != NULL)
            actions = animation_manager->get(previous_state)->getActions();

        if (actions != NULL)
            actions->doActions(this, opponent, type);
    }
}

void Player::move(float x_vel, float y_vel) {
    moveXpos(x_vel);
    y_pos = y_pos + y_vel;

    checkCollisions(x_vel);

    if (y_vel < 0 && y_pos <= y_initial) {
        y_pos = y_initial;
        updateState("GROUND");
    }
}

void Player::setLocation(float x, float y) {
    if (state["INVERT"])
        x_pos = x - x_initial;
    else
        x_pos = x + x_initial;
    y_pos = y + y_initial;
}

void Player::checkCollisions(float action_x_vel) {
    if (action_x_vel == 0)
        action_x_vel = 0.1;

    if (opponent != NULL && this->getDefense() != NULL && this->getDefense()->overlaps(opponent->getDefense())) {
        if (action_x_vel > 0) {
            moveXpos(-action_x_vel / 2);
            opponent->moveXpos(-action_x_vel * 2);
        } else {
            moveXpos(-action_x_vel / 2);
        }
    }
}

void Player::moveXpos(float value) {
    // Fighters stay inside the 200-unit wide stage (screen x = 50 + x_pos); projectiles may leave it.
    const float stage_min = -38.0f;
    const float stage_max = 138.0f;

    if (inverted)
        x_pos -= value;
    else
        x_pos += value;

    if (type == "player")
        x_pos = max(stage_min, min(stage_max, x_pos));
}

void Player::setCurrentState(string state) {
    if (state == "*previous") {
        current_state = previous_state;
    } else {
        previous_state = current_state;
        current_state = state;
        if (type == "player" && current_state != previous_state) {
            string sound = soundForState(current_state);
            if (!sound.empty())
                audio->playSound(sound);
        }
    }
    animation_manager->get(current_state)->update(x_pos, y_pos, 1, inverted);
}
