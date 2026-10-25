#include "graphicsCore.h"
#include "audioBackend.h"

// global

ObjectManager<Player> *object_manager = new ObjectManager<Player>();
TextureManager *texture_manager = new TextureManager();
AudioBackend *audio = new AudioBackend();
Camera *camera = new Camera();
Graphics *graphics = new Graphics();