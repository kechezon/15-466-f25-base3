#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	// game world
	float X_MIN, X_MAX, Y_MIN, Y_MAX, CENTER_Z, EXPLOSION_DEPTH;

	// Level info
	float bpm;
	std::tuple< size_t, size_t > timeSignature;
	float pitchRange = 20;
	std::tuple< float, float > EXPLOSION_RADIUS_RANGE;

	// General needs
	struct GameObject;
	struct ColliderSphere;

	// Game specific needs
	struct Player;
	struct ChimeBomb; // the audio and visual cue
	struct Explosion; // the actual explosion

	std::list< ChimeBomb > chimeBombs; // this is basically "the level" (all the chimeBombs)
	std::list< Explosion > explosions; // this is a more dynamic list

	std::list<Scene::Drawable>::iterator PlayMode::new_drawable(Mesh const &mesh, Scene::Transform *tf);

	//hexapod leg to wobble:
	Scene::Transform *hip = nullptr;
	Scene::Transform *upper_leg = nullptr;
	Scene::Transform *lower_leg = nullptr;
	glm::quat hip_base_rotation;
	glm::quat upper_leg_base_rotation;
	glm::quat lower_leg_base_rotation;
	float wobble = 0.0f;

	glm::vec3 get_leg_tip_position();

	//music coming from the tip of the leg (as a demonstration):
	std::shared_ptr< Sound::PlayingSample > leg_tip_loop;

	//car honk sound:
	std::shared_ptr< Sound::PlayingSample > honk_oneshot;

	// music playing in the background
	std::shared_ptr< Sound::PlayingSample > sound_track;
	
	//camera:
	Scene::Camera *camera = nullptr;

};
