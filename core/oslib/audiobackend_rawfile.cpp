#include "audiostream.h"
#include "cfg/cfg.h"
#include <sndfile.h>

#include <chrono>
#include <thread>

using the_clock = std::chrono::high_resolution_clock;

static the_clock::time_point last_time;
static SNDFILE *audio_file;
static bool audio_time_start = false;
static unsigned long last_frame_time;

static void rawfile_init() // Open file
{
	last_time = the_clock::time_point();

	SF_INFO sfinfo;
	memset (&sfinfo, 0, sizeof (sfinfo));
	sfinfo.format = (SF_FORMAT_WAV | SF_FORMAT_PCM_24);
	sfinfo.samplerate = 44100;
	sfinfo.channels = 2;

	audio_file = sf_open(cfgLoadStr("record", "rawaudio","").c_str(), SFM_WRITE, &sfinfo);

	if (audio_file == NULL) {
		printf(sf_strerror(audio_file));
	}
}

static void rawfile_term()
{
	sf_write_sync(audio_file);
	sf_close(audio_file);
}

static u32 rawfile_push(const void* frame, u32 samples, bool wait)
{
	if (audio_time_start == false) {
		const auto p1 = std::chrono::system_clock::now();
		NOTICE_LOG(AUDIO, "audio_time_start: %lu\n", std::chrono::duration_cast<std::chrono::milliseconds>(p1.time_since_epoch()).count());

		//printf("audio_time_start: %lu\n", std::chrono::duration_cast<std::chrono::milliseconds>(p1.time_since_epoch()).count());
        last_frame_time = std::chrono::duration_cast<std::chrono::milliseconds>(p1.time_since_epoch()).count();
		audio_time_start = true;

	}

	if (!wait)
	{
		// NOTICE_LOG(AUDIO, "Writing audio frame\n");

		// Write audio
		if (vidstarted) {
			sf_writef_short(audio_file, (short *)frame, samples);
		}
		// Debug to figure out how long it takes an audio/video write to occur
		const auto p2 = std::chrono::system_clock::now();
		unsigned long frame_timer = std::chrono::duration_cast<std::chrono::milliseconds>(p2.time_since_epoch()).count() - last_frame_time;
		last_frame_time = std::chrono::duration_cast<std::chrono::milliseconds>(p2.time_since_epoch()).count();
		// printf("Audio Frame render time: %lu\n", frame_timer);

		// Wait for a certain amount of time
		if (last_time.time_since_epoch() != the_clock::duration::zero())
		{
			auto fduration = std::chrono::nanoseconds(1000000000L * samples / 44100);
			auto duration = fduration - (the_clock::now() - last_time);
			std::this_thread::sleep_for(duration);
			last_time += fduration;
		}
		else
			last_time = the_clock::now();
	}
	return 1;
}

static bool rawfile_init_record(u32 sampling_freq) // Don't care about recording
{
	return true;
}

static u32 rawfile_record(void *buffer, u32 samples) // Don't care about recording
{
	memset(buffer, 0, samples * 2);
	return samples;
}

static audiobackend_t audiobackend_rawfile = {
    "rawaudio", // Slug
    "Raw file output", // Name
    &rawfile_init,
    &rawfile_push,
    &rawfile_term,
	nullptr,
	&rawfile_init_record,
	&rawfile_record,
	&rawfile_term
};

static bool rawaudio = RegisterAudioBackend(&audiobackend_rawfile);
