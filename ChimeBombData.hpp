#include <string>
struct ChimeBombData {
	// Self Contained Info
	float pan = 0;
	size_t pitch = 0; // 0 to (pitchRange - 1)
	float volume = 0;
	char sample_name[100];

	// Level Info
	size_t measure = 1; // 1-indexed (assume time signature doesn't change)
	float beat = 1; // 1.5 is the and (eighth-note), 2.667 is the third triplet on beat 2

	static_assert(sizeof(size_t) == 8, "size_t is not 8");
};