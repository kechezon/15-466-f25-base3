#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <numbers>
#include <random>

GLuint game_meshes_for_lit_color_texture_program = 0;
float X_MIN = -15.f;
float X_MAX = 15.f;
float Y_MIN = -10.f;
float Y_MAX = 10.f;
float CENTER_Z = 10.f;
float CUE_DEPTH = 0.5f;
auto timeSignature = std::make_tuple(4, 4);
auto EXPLOSION_RADIUS_RANGE = std::make_tuple(1.f, 6.f);
bool victory = false;

Load< MeshBuffer > game_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("egbdf.pnct"));
	game_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > game_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("egbdf.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name) {
	});
});

/******************************************
 * General Object Structs
 * (Reused and edited from my Game 2 code)
 ******************************************/
struct PlayMode::GameObject {
	Scene::Transform *transform = nullptr; // TODO: uhhh owner?
	std::list<Scene::Drawable>::iterator drawable; // initialize as uhhhhh = nullptr;

	PlayMode::GameObject() {
		
	}

	PlayMode::GameObject(Scene::Transform *transform_, std::list<Scene::Drawable>::iterator drawable_)
		: transform(transform_), drawable(drawable_) {

	}

	glm::vec3 transform_forward() {
		return transform->rotation * glm::vec3(0.0f, 1.0f, 0.0f);
	}
};

struct PlayMode::ColliderSphere {
	PlayMode::GameObject *gameObject = nullptr; // "borrower"
	glm::vec2 offset = glm::vec2(0.0f, 0.0f); // from a gameObject
	std::string collider_tag = "";
	float radius = 0.f;

	PlayMode::ColliderSphere() {

	}

	PlayMode::ColliderSphere(PlayMode::GameObject *gameObject_, glm::vec2 offset_, std::string tag_, float radius_)
		: gameObject(gameObject_), collider_tag(tag_), radius(radius_) {
			offset = offset_;
		};

	bool collider_test(ColliderSphere *other) {
		GameObject *otherObj = other->gameObject;
		// calculate vector from me to other, determine if magnitude is < myRadius + otherRadius
		glm::vec2 myCentroid = glm::vec2(gameObject->transform->position.x, gameObject->transform->position.y) +
							   glm::vec2(offset.x * gameObject->transform->scale.x, offset.y * gameObject->transform->scale.y);
		glm::vec2 otherCentroid = glm::vec2(otherObj->transform->position.x, otherObj->transform->position.y) +
								  glm::vec2(other->offset.x * otherObj->transform->position.x, other->offset.y * otherObj->transform->position.y);
		glm::vec2 vecToOther = otherCentroid - myCentroid;

		float magnitude = std::sqrtf((vecToOther.x * vecToOther.x) +
									 (vecToOther.y * vecToOther.y));
		float scaledRadius = radius * ((gameObject->transform->scale.x + gameObject->transform->scale.y) / 2);
		float scaledOtherRadius = other->radius * ((otherObj->transform->scale.x + otherObj->transform->scale.y) / 2);
		float collisionThreshold = scaledRadius + scaledOtherRadius;

		return magnitude < collisionThreshold;
	};
};

/*
struct PlayMode::ChimeBombData {
	// Self Contained Info
	float pan = 0;
	size_t pitch = 0; // 0 to (pitchRange - 1)
	float volume = 0;
	std::string sample_name;

	// Level Info
	size_t measure = 1; // 1-indexed (assume time signature doesn't change)
	float beat = 1; // 1.5 is the and (eighth-note), 2.667 is the third triplet on beat 2
}; */

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

	PlayMode::Explosion(Scene::Transform *transform, std::list<Scene::Drawable>::iterator drawable, float power_, float timer_)
		: gameObject(transform, drawable), power(power_), timer(timer_) {
		collider = ColliderSphere(&gameObject, glm::vec2(0, 0), "explosion", 0.5f);
	}

	// returns true on dissipating (finishing)
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
	float FACE_PTS_RATE = 100; // base points per second for flying near explosions without getting hit
	float MAX_SAFETY_FOR_FACE = 5; // any farther away from an explosion, you do not get FACE points
	glm::vec2 flightVector = glm::vec2(0, 0);
	float health = 100;
	float faceScore = 0; // "*F*lying *A*-lotta *C*lose to *E*xplosions" score

	/*********************
	 * Collision Tracking
	 *********************/
	std::list< Explosion* > hitMe = {};

	/************
	 * Animation
	 ************/
	float FLIGHT_TILT_SPEED = 120; // degrees per second along x axis
	float MAX_FLIGHT_TILT = 30; // degrees along x axis
	float currentFlightTilt = 0;

	/*********
	 * Sounds
	 *********/
	Sound::Sample *ouch;
	Sound::Sample *crash;

	PlayMode::Player() {
		
	};

	PlayMode::Player(Scene::Transform *transform, std::list<Scene::Drawable>::iterator drawable)
		: gameObject(transform, drawable) {
		collider = ColliderSphere(&gameObject, glm::vec2(0, 0), "player", 1);
		ouch = new Sound::Sample(data_path("player_ouch.wav"));
		crash = new Sound::Sample(data_path("player_crashing.wav"));
	}

	void move(float x, float y) {
		if (x != 0 || y != 0) {
			flightVector = glm::normalize(glm::vec2(x, y)) * FLIGHT_SPEED;
		}
		else {
			flightVector = glm::vec2(0);
		}
	}

	void update(float t, PlayMode *pm) {
		if (health > 0) {
			// movement
			gameObject.transform->position.x += flightVector.x * t;
			gameObject.transform->position.y += flightVector.y * t;
			if (flightVector.y > 0) currentFlightTilt -= FLIGHT_TILT_SPEED * t;
			else if (flightVector.y < 0) currentFlightTilt += FLIGHT_TILT_SPEED * t;
			else { // bring back to neutral
				if (currentFlightTilt > 0) currentFlightTilt = std::clamp(currentFlightTilt - FLIGHT_TILT_SPEED * t, 0.f, MAX_FLIGHT_TILT);
				else if (currentFlightTilt < 0) currentFlightTilt = std::clamp(currentFlightTilt + FLIGHT_TILT_SPEED * t, -MAX_FLIGHT_TILT, 0.f);
			}
			currentFlightTilt = std::clamp(currentFlightTilt, -MAX_FLIGHT_TILT, MAX_FLIGHT_TILT);
			gameObject.transform->rotation = glm::quat(1.0f, currentFlightTilt * std::numbers::pi_v<float> / 180.f, 0, 0);

			// bounds
			if (gameObject.transform->position.x < X_MIN) gameObject.transform->position.x = X_MIN;
			if (gameObject.transform->position.x > X_MAX) gameObject.transform->position.x = X_MAX;
			if (gameObject.transform->position.y < Y_MIN) gameObject.transform->position.y = Y_MIN;
			if (gameObject.transform->position.y > Y_MAX) gameObject.transform->position.y = Y_MAX;

			// collision with Explosions
			for (auto explosion = pm->explosions.begin(); explosion != pm->explosions.end(); explosion++) {
				if (std::find(hitMe.begin(), hitMe.end(), *explosion) == hitMe.end()) {
					ColliderSphere otherCollider = (*explosion)->collider;
					if (collider.collider_test(&otherCollider)) {
						health -= (*explosion)->power;
						hitMe.emplace_back((*explosion));
						Sound::play(*ouch);
					}
					else {
						glm::vec2 vectorFromExplosion = glm::vec2((*explosion)->gameObject.transform->position.x - gameObject.transform->position.x,
																(*explosion)->gameObject.transform->position.y - gameObject.transform->position.y);
						float distFromExplosion = std::sqrtf((vectorFromExplosion.x * vectorFromExplosion.x) + (vectorFromExplosion.y * vectorFromExplosion.y));
						float safety = distFromExplosion - (*explosion)->gameObject.transform->scale.x;
						if (safety > MAX_SAFETY_FOR_FACE) {
							faceScore += (MAX_SAFETY_FOR_FACE - safety) * FACE_PTS_RATE * t;
						}
					}
				}
			}

			if (health < 0) {
				gameObject.transform->rotation = glm::quat(1.0f, 0.0f, gameObject.transform->rotation.y, 30);

				// at the suggestion of Jia:
				Sound::stop_all_samples();
				Sound::loop(*crash);
			}
		}
		else {
			// CRASH!!!
			gameObject.transform->rotation = glm::quat(1.0f, gameObject.transform->rotation.x + (FLIGHT_TILT_SPEED * t) * std::numbers::pi_v<float> / 180.f,
													   0.0f, gameObject.transform->rotation.z);

			gameObject.transform->position.x += FLIGHT_SPEED * t * 0.25f * sqrtf(2.f);
			gameObject.transform->position.y -= FLIGHT_SPEED * t * sqrtf(2.f);

			if (gameObject.transform->position.y < Y_MIN - (Y_MAX - Y_MIN)) {
				Sound::stop_all_samples();
				Mode::set_current(std::make_shared< PlayMode >());
			}
		}
	}
};
struct PlayMode::ChimeBomb {
	PlayMode::GameObject gameObject_outer;
	PlayMode::GameObject gameObject_inner;

	bool detonating = false;

	/****************************
	 * Self-Contained Audio Info
	 ****************************/
	Sound::Sample *chime;
	Sound::Sample *boom;
	float CHIME_DURATION = 1; // in units of beats
	float soundFadeTimer = 0.5f; // in SECONDS, time spent fading out the sound
	float DETONATE_DELAY = 4; // in units of beats
	float detonateTimer = 0; // in units of beats

	float pan = 0;
	size_t pitch = 0; // 0 to (pitchRange - 1)
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
	float START_BEAT = (measure - 1) * std::get<0>(timeSignature) + (beat - 1);
	float levelTimer = 0; // in beats

	PlayMode::ChimeBomb(Scene::Transform *transform_outer, std::list<Scene::Drawable>::iterator drawable_outer,
						Scene::Transform *transform_inner, std::list<Scene::Drawable>::iterator drawable_inner,
						std::string sample_name, float pan_, size_t pitch_, float volume_,
						size_t measure_, float beat_)
		: gameObject_outer(transform_outer, drawable_outer), gameObject_inner(transform_inner, drawable_inner),
		  pan(pan_), pitch(pitch_), volume(volume_), measure(measure_), beat(beat_),
		  START_BEAT((measure - 1) * std::get<0>(timeSignature) + (beat - 1)) {
		
		// gameObject_inner

		// convert pan to volume->size and power, position.x -> pan, pitch to position.y -> pitch
		// float s = std::lerp(std::get<0>(EXPLOSION_SIZE_RANGE), std::get<1>(EXPLOSION_SIZE_RANGE), volume);

		transform_outer->position.x = std::lerp(X_MIN, X_MAX, (pan + 1.f) / 2.f);
		transform_outer->position.y = std::lerp(Y_MIN, Y_MAX, pitch_ / pitchRange);
		transform_outer->position.z = CENTER_Z - CUE_DEPTH;
		transform_outer->scale = glm::vec3(0, 0, 0); // use this to then determine power

		transform_inner->position = glm::vec3(transform_outer->position) + glm::vec3(0, 0, 0.001f);
		transform_inner->scale = glm::vec3(0, 0, 0);

		chime = new Sound::Sample(data_path(sample_name));
		boom = new Sound::Sample(data_path("chime_boom.wav"));
	}

	PlayMode::ChimeBomb(Scene::Transform *transform_outer, std::list<Scene::Drawable>::iterator drawable_outer,
						Scene::Transform *transform_inner, std::list<Scene::Drawable>::iterator drawable_inner,
						ChimeBombData data) {
		PlayMode::ChimeBomb::ChimeBomb(transform_outer, drawable_outer, transform_inner, drawable_inner,
							data.sample_name, data.pan, data.pitch, data.volume, data.measure, data.beat);
	}

	// Explodes
	void explode(float t, PlayMode *pm) {
		Scene::Transform *tf = new Scene::Transform();
		tf->position = glm::vec3(gameObject_outer.transform->position.x, gameObject_outer.transform->position.y, CENTER_Z);
		tf->scale = glm::vec3(gameObject_outer.transform->scale.x, gameObject_outer.transform->scale.y, 0);
		tf->name = gameObject_outer.transform->name + "-explode";

		// assert(gameObject_outer); TODO figure out how to text
		// assert(gameObject_outer.drawable != nullptr);
		pm->scene.drawables.erase(gameObject_outer.drawable);

		// assert(gameObject_inner); TODO figure out how to text
		// assert(gameObject_inner.drawable);
		pm->scene.drawables.erase(gameObject_inner.drawable);

		float explosionPower = std::lerp(std::get<0>(EXPLOSION_RADIUS_RANGE), std::get<1>(EXPLOSION_RADIUS_RANGE), volume);
		Explosion *explosion = new Explosion(tf, pm->new_drawable(game_meshes->lookup("Ship"), tf), explosionPower, detonateTimer - DETONATE_DELAY);
		pm->explosions.emplace_back(explosion);
	}

	// return true on actually detonating
	bool update(float t, PlayMode *pm) {
		if (!detonating) { // before the beat arrives
			std::cout << levelTimer << std::endl;
			levelTimer += (t / 60) * bpm;
			if (levelTimer >= START_BEAT) {
				detonateTimer = levelTimer - START_BEAT;
				detonating = true;
				Sound::play(*chime, volume, pan);
			}
			return false;
		}
		else { // now that the beat has arrived
			assert(levelTimer >= START_BEAT);
			detonateTimer += (t / 60) * bpm;

			if (DETONATE_DELAY - detonateTimer <= 1.0f) {
				float aoeRadius = std::lerp(std::get<0>(EXPLOSION_RADIUS_RANGE), std::get<1>(EXPLOSION_RADIUS_RANGE), volume) * 2;
				float timerRadius = (float)std::lerp(0, aoeRadius, 1.0f - (DETONATE_DELAY - detonateTimer));
				gameObject_outer.transform->scale = glm::vec3(aoeRadius, aoeRadius, CUE_DEPTH);
				gameObject_inner.transform->scale = glm::vec3(timerRadius, timerRadius, CUE_DEPTH);
			}

			if (detonateTimer >= DETONATE_DELAY) {
				explode(detonateTimer - DETONATE_DELAY, pm);
				return true;
			}
			return false;
		}
	}
};

PlayMode::Player player;

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

// Load< Scene > hexapod_scene(LoadTagDefault, []() -> Scene const * {
// 	return new Scene(data_path("hexapod.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
// 		Mesh const &mesh = hexapod_meshes->lookup(mesh_name);

// 		scene.drawables.emplace_back(transform);
// 		Scene::Drawable &drawable = scene.drawables.back();

// 		drawable.pipeline = lit_color_texture_program_pipeline;

// 		drawable.pipeline.vao = hexapod_meshes_for_lit_color_texture_program;
// 		drawable.pipeline.type = mesh.type;
// 		drawable.pipeline.start = mesh.start;
// 		drawable.pipeline.count = mesh.count;

// 	});
// });

// Load< Sound::Sample > dusty_floor_sample(LoadTagDefault, []() -> Sound::Sample const * {
// 	return new Sound::Sample(data_path("dusty-floor.opus"));
// });


// Load< Sound::Sample > honk_sample(LoadTagDefault, []() -> Sound::Sample const * {
// 	return new Sound::Sample(data_path("honk.wav"));
// });

Load < Sound::Sample > starter_soaring_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("starter_soaring.wav"));
});


PlayMode::PlayMode() : scene(*game_scene) {
	//get pointers to leg for convenience:
	// for (auto &transform : scene.transforms) {
	// 	if (transform.name == "Hip.FL") hip = &transform;
	// 	else if (transform.name == "UpperLeg.FL") upper_leg = &transform;
	// 	else if (transform.name == "LowerLeg.FL") lower_leg = &transform;
	// }
	// if (hip == nullptr) throw std::runtime_error("Hip not found.");
	// if (upper_leg == nullptr) throw std::runtime_error("Upper leg not found.");
	// if (lower_leg == nullptr) throw std::runtime_error("Lower leg not found.");

	// hip_base_rotation = hip->rotation;
	// upper_leg_base_rotation = upper_leg->rotation;
	// lower_leg_base_rotation = lower_leg->rotation;

	// make player
	{
		Scene::Transform *transform = new Scene::Transform();
		transform->position = glm::vec3(0, 0, 0);

		std::list<Scene::Drawable>::iterator drawable = new_drawable(game_meshes->lookup("Ship"), transform);
		player = PlayMode::Player(transform, drawable);
		// player.ouch(LoadTagDefault, []() -> Sound::Sample const * { return new Sound::Sample(data_path("player_ouch.wav"))});
		// player.crash(LoadTagDefault, []() -> Sound::Sample const * { return new Sound::Sample(data_path("player_crashing.wav"))});
	}

	// load chimebomb data from binary dump to create level
	{
		// load data
		// How to from this cpp thread: https://cplusplus.com/forum/general/122330/
		std::ifstream chartFile;
		chartFile.open(data_path("starter_soaring.chrtt"), std::ios::binary);
		ChimeBombData cbd;

		while (chartFile.read(reinterpret_cast<char*>(&cbd), sizeof(ChimeBombData)))
		{
			chimeBombDataList.emplace_back(cbd);
			std::cout << cbd.measure << "." << cbd.beat << ": " << cbd.sample_name << std::endl; // measure and beat are fine, sample_name is not
		}

		chartFile.close();

		// Construct
		for (auto data : chimeBombDataList) {
			Scene::Transform *transform_outer = new Scene::Transform();
			std::list<Scene::Drawable>::iterator drawable_outer = new_drawable(game_meshes->lookup("AoE_Radius"), transform_outer);
			transform_outer->name = "ChimeBomb " + std::to_string(chimeBombs.size());

			Scene::Transform *transform_inner = new Scene::Transform();
			std::list<Scene::Drawable>::iterator drawable_inner = new_drawable(game_meshes->lookup("AoE_Timer"), transform_inner);

			ChimeBomb chimeBomb(transform_outer, drawable_outer, transform_inner, drawable_inner, data); // which leads to an unhandled exception
			chimeBombs.emplace_back(chimeBomb);
		}
	}

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	//start music loop playing:
	// (note: position will be over-ridden in update())
	// leg_tip_loop = Sound::loop_3D(*dusty_floor_sample, 1.0f, get_leg_tip_position(), 10.0f);
	sound_track = Sound::play(*starter_soaring_sample);
	// const Sound::Sample song_sample = Sound::Sample(data_path("starter_soaring.wav"));
	// Sound::play(song_sample);
	// Sound::play(sound_track);
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
			// if (honk_oneshot) honk_oneshot->stop();
			// honk_oneshot = Sound::play_3D(*honk_sample, 0.3f, glm::vec3(4.6f, -7.8f, 6.9f)); //hardcoded position of front of car, from blender
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
			// camera->transform->rotation = glm::normalize(
			// 	camera->transform->rotation
			// 	* glm::angleAxis(-motion.x * camera->fovy, glm::vec3(0.0f, 1.0f, 0.0f))
			// 	* glm::angleAxis(motion.y * camera->fovy, glm::vec3(1.0f, 0.0f, 0.0f))
			// );
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	//slowly rotates through [0,1):
	// wobble += elapsed / 10.0f;
	// wobble -= std::floor(wobble);

	// hip->rotation = hip_base_rotation * glm::angleAxis(
	// 	glm::radians(5.0f * std::sin(wobble * 2.0f * float(M_PI))),
	// 	glm::vec3(0.0f, 1.0f, 0.0f)
	// );
	// upper_leg->rotation = upper_leg_base_rotation * glm::angleAxis(
	// 	glm::radians(7.0f * std::sin(wobble * 2.0f * 2.0f * float(M_PI))),
	// 	glm::vec3(0.0f, 0.0f, 1.0f)
	// );
	// lower_leg->rotation = lower_leg_base_rotation * glm::angleAxis(
	// 	glm::radians(10.0f * std::sin(wobble * 3.0f * 2.0f * float(M_PI))),
	// 	glm::vec3(0.0f, 0.0f, 1.0f)
	// );

	//move sound to follow leg tip position:
	// leg_tip_loop->set_position(get_leg_tip_position(), 1.0f / 60.0f);

	//move player
	{

		//combine inputs into a move:
		// constexpr float PlayerSpeed = 30.0f;
		glm::vec2 move = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) move.x =-1.0f;
		if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) move.y =-1.0f;
		if (!down.pressed && up.pressed) move.y = 1.0f;
		player.move(move.x, move.y); // handles normalization
	}

	//update listener to camera position
	{ 
		glm::mat4x3 frame = camera->transform->make_parent_from_local();
		glm::vec3 frame_right = frame[0];
		glm::vec3 frame_at = frame[3];
		Sound::listener.set_position_right(frame_at, frame_right, 1.0f / 60.0f);
	}

	// update game entitites
	{
		if (player.health > 0) {
			if (chimeBombs.size() > 0 && explosions.size() > 0) {
				for (auto iter = chimeBombs.begin(); iter != chimeBombs.end(); /* later */) {
					if (!(iter->update(elapsed, this))) {
						// chime bomb still exists
						iter++;
					}
					else {
						assert(iter->detonateTimer >= iter->START_BEAT); // detonated
						auto const detonated = iter;
						iter++;
						chimeBombs.erase(detonated);
					}
				}

				for (auto iter = explosions.begin(); iter != explosions.end(); /* later */) {
					if (!((*iter)->update(elapsed))) {
						// explosion is still going
						iter++;
					}
					else {
						assert((*iter)->timer >= (*iter)->DURATION); // dissapated
						auto const finished = iter;
						iter++;
						explosions.erase(finished);
					}
				}
			}
			else { // victory!
				victory = true;
			}
		}
		player.update(elapsed, this);
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

	glClearColor(0.f, 0.f, 0.39f, 1.0f);
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
		lines.draw_text("Health: " + std::to_string(player.health),
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		lines.draw_text("FACE: " + std::to_string(player.faceScore),
			glm::vec3(-aspect + 10.f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("Health: " + std::to_string(player.health),
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		lines.draw_text("FACE: " + std::to_string(player.faceScore),
			glm::vec3(-aspect + 10.f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));

		if (victory) {
			lines.draw_text("Victory!" + std::to_string(player.faceScore),
				glm::vec3(-aspect + 10.f * H, -1.0 + 5.f * H, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			lines.draw_text("Victory!" + std::to_string(player.faceScore),
				glm::vec3(-aspect + 10.f * H + ofs, -1.0 + + 5.f * H + ofs, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0x00, 0xff, 0xff, 0x00));
		}
	}
	GL_ERRORS();
}

glm::vec3 PlayMode::get_leg_tip_position() {
	//the vertex position here was read from the model in blender:
	return lower_leg->make_world_from_local() * glm::vec4(-1.26137f, -11.861f, 0.0f, 1.0f);
}
