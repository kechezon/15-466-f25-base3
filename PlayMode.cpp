#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

GLuint game_meshes_for_lit_color_texture_program = 0;
// TODO: set world bounds

/******************************************
 * General Object Structs
 * (Reused and edited from my Game 2 code)
 ******************************************/
struct PlayMode::GameObject {
	// for reference, Tireler has a radius of 1 unit, and a width of 1 unit

	Scene::Transform *transform = nullptr; // TODO: uhhh owner?
	std::list<Scene::Drawable>::iterator drawable; // initialize as uhhhhh = nullptr;

	PlayMode::GameObject(Scene::Transform *transform_, std::list<Scene::Drawable>::iterator drawable_)
		: transform(transform_), drawable(drawable_);

	glm::vec3 transform_forward() {
		return transform->rotation * glm::vec3(0.0f, 1.0f, 0.0f);
	}
};

struct PlayMode::ColliderSphere {
	PlayMode::GameObject *gameObject = nullptr; // "borrower"
	glm::vec2 offset = new glm::vec2(0.0f, 0.0f); // from a gameObject
	std::string collider_tag = "";
	float radius = 0.f;

	PlayMode::ColliderSphere(PlayMode::GameObject *gameObject_, glm::vec3 offset_, std::string tag_, float radius_)
		: gameObject(gameObject_), offset(offset_), collider_tag(tag_), radius(radius_)

	bool collider_test(ColliderSphere *other) {
		GameObject *otherObj = other->obj;
		// calculate vector from me to other, determine if magnitude is < myRadius + otherRadius
		glm::vec2 myCentroid = glm::vec2(obj->transform->position.x, obj->transform->position.y) +
							   glm::vec2(offset.x * obj->transform->scale.x, offset.y * obj->transform->scale.y);
		glm::vec2 otherCentroid = glm::vec2(otherObj->transform->position.x, otherObj->transform->position.y) +
								  glm::vec2(other->offset.x * otherObj->transform->osition.x, other->offset.y * otherObj->transform->osition.y);
		glm::vec2 vecToOther = otherCentroid - myCentroid;

		float magnitude = std::sqrtf((vecToOther.x * vecToOther.x) +
									 (vecToOther.y * vecToOther.y));
		float scaledRadius = radius * ((gameObject->transform->scale.x + gameObject->transform->scale.y) / 2);
		float scaledOtherRadius = other->radius * ((otherObj->transform->scale.x + otherObj->scale->position.y) / 2);
		float collisionThreshold = scaledRadius + scaledOtherRadius;

		return magnitude < collisionThreshold;
	};
};

struct PlayMode::ChimeBomb {
	// TODO
	PlayMode::GameObject gameObject_outer;
	PlayMode::GameObject gameObject_inner;

	bool detonating = false;

	/****************************
	 * Self-Contained Audio Info
	 ****************************/
	Load< Sound::Sample > chime = nullptr;
	float CHIME_DURATION = 1; // in units of beats
	float soundFadeTimer = 0.5f; // in SECONDS, time spent fading out the sound
	float DETONATE_DELAY = 4; // in units of beats
	float detonateTimer = 0; // in units of beats

	float pan = 0;
	size_t pitch = 0;
	float volume = 0;

	/*****************************
	 * Self-Contained Visual Info
	 *****************************/
	float VISUAL_DURATION = 1; // in units of beats (the very end of the chime)
	float visualTimer = 0; // in beats

	/************************************
	 * Level Info (info about the chime)
	 ************************************/
	size_t measure = 1; // 1-indexed (assume time signature doesn't change)
	float beat = 1; // 1.5 is the and (eighth-note), 2.667 is the third triplet on beat 2
	float startBeat = (measure - 1) * std::get<0>(timeSignature) + (beat - 1);
	float levelTimer = 0; // in beats

	PlayMode::ChimeBomb(Scene::Transform *transform_outer, std::list<Scene::Drawable>::iterator drawable_outer,
						Scene::Transform *transform_inner, std::list<Scene::Drawable>::iterator drawable_inner,
						std::string sample_name, float pan_, size_t pitch_, float volume_)
		: gameObject_outer(transform_outer, drawable_outer), gameObject_inner(transform_inner, drawable_inner),
		  pan(pan_), pitch(pitch_), volume(volume_) {
		
		// convert pan to volume->size and power, position.x -> pan, pitch to position.y -> pitch
		// float s = std::lerp(std::get<0>(EXPLOSION_SIZE_RANGE), std::get<1>(EXPLOSION_SIZE_RANGE), volume);

		transform_outer->position.x = std::lerp(X_MIN, X_MAX, (pan + 1.f) / 2.f);
		transform_outer->position.y = std::lerp(Y_MIN, Y_MAX, pitch_ / pitchRange);
		transform_outer->position.z = CENTER_Z - EXPLOSION_DEPTH;
		transform_outer->scale = glm::vec2(0, 0, 0); // use this to then determine power

		transform_inner->position = glm::vec3(transform_outer->position) + glm::vec3(0, 0, 0.001f);
		transform_inner->scale = glm::vec3(0, 0, 0);

		chime(LoadTagDefault, []() -> Sound::Sample const * {
			return new Sound::Sample(data_path(sample_name));
		});
	}

	// Detonate (starts the chime)
	void detonate(float t) {
		Scene::Transform *tf = new Scene::Transform();
		tf->position = glm::vec3(transform->position.x, transform->position.y, CENTER_Z);
		tf->scale = glm::vec3(transform->scale.x, transform->scale.y, 0);

		assert(gameObject_outer);
		assert(gameObject_outer->drawable);
		scene.drawables.erase(gameObject_outer->drawable);

		assert(gameObject_inner);
		assert(gameObject_inner->drawable);
		scene.drawables.erase(gameObject_inner->drawable);

		PlayMode::Explosion explosion = PlayMode::Explosion(tf, new_drawable(game_meshes->lookup("Ship"), tf), t);
		explosions.emplace_back(explosion);
	}

	// return true on actually detonating
	bool update(float t) {
		if (!detonating) { // before the beat arrives
			levelTimer += (t / 60) * bpm;
			if (levelTimer >= startBeat) {
				detonateTimer = levelTimer - startBeat;
				detonating = true;
			}
			return false;
		}
		else { // now that the beat has arrived
			detonateTimer += (t / 60) * bpm;

			if (DETONATE_DELAY - detonateTimer <= 1.0f) {
				float aoeRadius = std::lerp(std::get<0>(EXPLOSION_RADIUS_RANGE), std::get<1>(EXPLOSION_RADIUS_RANGE), volume) * 2;
				float timerRadius = std::lerp(0, aoeRadius, 1.0f - (DETONATE_DELAY - detonateTimer));
				gameObject_outer->transform->scale = glm::vec3(aoeRadius, aoeRadius, EXPLOSION_DEPTH);
				gameObject_inner->transform->scale = glm::vec3(timerRadius, timerRadius, EXPLOSION_DEPTH);
			}

			if (detonateTimer >= DETONATE_DELAY) {
				detonate(detonateTimer - DETONATE_DELAY);
				return true;
			}
		}
	}
};

struct PlayMode::Explosion {
	PlayMode::GameObject gameObject;
	PlayMode::ColliderSphere collider;

	/******************
	 * Game Properties
	 ******************/
	float SIZE = 0;
	float DURATION = 4; // in units of beats
	float timer = 0; // in units of beats
	float power = 0;

	PlayMode::Explosion(Scene::Transform *transform, std::list<Scene::Drawable>::iterator drawable, float timer_)
		: gameObject(transform, drawable), timer(timer_) {
		collider = ColliderSphere(&gameObject, glm::vec2(0, 0), "explosion", 0.5f);
	}

	// returns true on dispersing
	bool update(float t) {
		timer += (t / 60) * bpm; // time in seconds -> time in minutes -> time in beats
		return t >= DURATION;
	}
};


struct PlayMode::Player {
	PlayMode::GameObject gameObject;
	PlayMode::ColliderSphere collider;

	/*********************
	 * Gameplay Variables
	 *********************/
	float FLIGHT_SPEED = 10; // units per second
	glm::vec2 flightVector = glm::vec2(0, 0);
	float health = 100;

	/*********************
	 * Collision Tracking
	 *********************/
	std::vector< Explosion > hitMe = {};

	/************
	 * Animation
	 ************/
	float FLIGHT_TILT_SPEED = 60; // degrees per second along x axis
	float MAX_FLIGHT_TILT = 60; // degrees along x axis
	float currentFlightTilt = 0;

	PlayMode::Player(Scene::Transform *transform, std::list<Scene::Drawable>::iterator drawable)
		: gameObject(transform, drawable);

	void move(float x, float y) {
		if (x != 0 || y != 0) {
			flightVector = glm::normalize(glm::vec2(x, y)) * FLIGHT_SPEED;
		}
		else {
			flightVector = glm::vec2(0);
		}
	}

	void update(float t) {
		// movement
		gameObject.transform->position += flightVector * t;
		if (flightVector.y > 0) currentFlightTilt += FLIGHT_TILT_SPEED * t;
		if (flightVector.y < 0) currentFlightTilt -= FLIGHT_TILT_SPEED * t;
		currentFlightTilt = std::clamp(currentFlightTilt, -MAX_FLIGHT_TILT, MAX_FLIGHT_TILT);
		gameObject.transform->rotation = glm::quat(1.0f, currentFlightTilt, 0, 0);

		// bounds
		if (gameObject.transform->position.x < X_MIN) gameObject.transform->position.x = X_MIN;
		if (gameObject.transform->position.x > X_MAX) gameObject.transform->position.x = X_MAX;
		if (gameObject.transform->position.y < Y_MIN) gameObject.transform->position.y = Y_MIN;
		if (gameObject.transform->position.y > Y_MAX) gameObject.transform->position.y = Y_MAX;

		// collision with Explosions
		for (Explosion explosion : explosions) {
			ColliderSphere otherCollider = explosion.collider;
			if (collider.collider_test(otherCollider)) {
				health -= explosion.power;
				hitMe.emplace_back(explosion);
			}
		}

		if (health < 0) {
			Mode::set_current(std::make_shared< PlayMode >());
		}
	}
};

std::list<Scene::Drawable>::iterator PlayMode::new_drawable(Mesh const &mesh, Scene::Transform *tf) {
	scene.drawables.emplace_back(tf);
	Scene::Drawable &drawable = scene.drawables.back();
	drawable.pipeline = lit_color_texture_program_pipeline;

	drawable.pipeline.vao = game_meshes_for_lit_color_texture_program;
	drawable.pipeline.type = mesh.type;
	drawable.pipeline.start = mesh.start;
	drawable.pipeline.count = mesh.count;
	drawable.transform = tf;

	std::list<Scene::Drawable>::iterator ret = scene.drawables.begin();
	for (int i = 0; i < scene.drawables.size() - 1; i++) {
		ret++;
	}

	return ret;
}

// GLuint hexapod_meshes_for_lit_color_texture_program = 0;
// Load< MeshBuffer > hexapod_meshes(LoadTagDefault, []() -> MeshBuffer const * {
// 	MeshBuffer const *ret = new MeshBuffer(data_path("hexapod.pnct"));
// 	hexapod_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
// 	return ret;
// });

Load< MeshBuffer > game_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("egbdf.pnct"));
	game_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > hexapod_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("hexapod.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = hexapod_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = hexapod_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

Load< Sound::Sample > dusty_floor_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("dusty-floor.opus"));
});


Load< Sound::Sample > honk_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("honk.wav"));
});


PlayMode::PlayMode() : scene(*hexapod_scene) {
	//get pointers to leg for convenience:
	for (auto &transform : scene.transforms) {
		if (transform.name == "Hip.FL") hip = &transform;
		else if (transform.name == "UpperLeg.FL") upper_leg = &transform;
		else if (transform.name == "LowerLeg.FL") lower_leg = &transform;
	}
	if (hip == nullptr) throw std::runtime_error("Hip not found.");
	if (upper_leg == nullptr) throw std::runtime_error("Upper leg not found.");
	if (lower_leg == nullptr) throw std::runtime_error("Lower leg not found.");

	hip_base_rotation = hip->rotation;
	upper_leg_base_rotation = upper_leg->rotation;
	lower_leg_base_rotation = lower_leg->rotation;

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	//start music loop playing:
	// (note: position will be over-ridden in update())
	leg_tip_loop = Sound::loop_3D(*dusty_floor_sample, 1.0f, get_leg_tip_position(), 10.0f);
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_EVENT_KEY_DOWN) {
		if (evt.key.key == SDLK_ESCAPE) {
			SDL_SetWindowRelativeMouseMode(Mode::window, false);
			return true;
		} else if (evt.key.key == SDLK_A) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_D) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_W) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_S) {
			down.downs += 1;
			down.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_SPACE) {
			if (honk_oneshot) honk_oneshot->stop();
			honk_oneshot = Sound::play_3D(*honk_sample, 0.3f, glm::vec3(4.6f, -7.8f, 6.9f)); //hardcoded position of front of car, from blender
		}
	} else if (evt.type == SDL_EVENT_KEY_UP) {
		if (evt.key.key == SDLK_A) {
			left.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_D) {
			right.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_W) {
			up.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_S) {
			down.pressed = false;
			return true;
		}
	} else if (evt.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
		if (SDL_GetWindowRelativeMouseMode(Mode::window) == false) {
			SDL_SetWindowRelativeMouseMode(Mode::window, true);
			return true;
		}
	} else if (evt.type == SDL_EVENT_MOUSE_MOTION) {
		if (SDL_GetWindowRelativeMouseMode(Mode::window) == true) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);
			camera->transform->rotation = glm::normalize(
				camera->transform->rotation
				* glm::angleAxis(-motion.x * camera->fovy, glm::vec3(0.0f, 1.0f, 0.0f))
				* glm::angleAxis(motion.y * camera->fovy, glm::vec3(1.0f, 0.0f, 0.0f))
			);
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	//slowly rotates through [0,1):
	wobble += elapsed / 10.0f;
	wobble -= std::floor(wobble);

	hip->rotation = hip_base_rotation * glm::angleAxis(
		glm::radians(5.0f * std::sin(wobble * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 1.0f, 0.0f)
	);
	upper_leg->rotation = upper_leg_base_rotation * glm::angleAxis(
		glm::radians(7.0f * std::sin(wobble * 2.0f * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 0.0f, 1.0f)
	);
	lower_leg->rotation = lower_leg_base_rotation * glm::angleAxis(
		glm::radians(10.0f * std::sin(wobble * 3.0f * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 0.0f, 1.0f)
	);

	//move sound to follow leg tip position:
	leg_tip_loop->set_position(get_leg_tip_position(), 1.0f / 60.0f);

	//move camera:
	{

		//combine inputs into a move:
		constexpr float PlayerSpeed = 30.0f;
		glm::vec2 move = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) move.x =-1.0f;
		if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) move.y =-1.0f;
		if (!down.pressed && up.pressed) move.y = 1.0f;

		//make it so that moving diagonally doesn't go faster:
		if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

		glm::mat4x3 frame = camera->transform->make_parent_from_local();
		glm::vec3 frame_right = frame[0];
		//glm::vec3 up = frame[1];
		glm::vec3 frame_forward = -frame[2];

		camera->transform->position += move.x * frame_right + move.y * frame_forward;
	}

	{ //update listener to camera position:
		glm::mat4x3 frame = camera->transform->make_parent_from_local();
		glm::vec3 frame_right = frame[0];
		glm::vec3 frame_at = frame[3];
		Sound::listener.set_position_right(frame_at, frame_right, 1.0f / 60.0f);
	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		lines.draw_text("Mouse motion rotates camera; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("Mouse motion rotates camera; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
	GL_ERRORS();
}

glm::vec3 PlayMode::get_leg_tip_position() {
	//the vertex position here was read from the model in blender:
	return lower_leg->make_world_from_local() * glm::vec4(-1.26137f, -11.861f, 0.0f, 1.0f);
}
