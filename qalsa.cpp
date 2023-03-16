#include <qalsa.h>
#include <alsa/asoundlib.h>
#include <cassert>
#include <thread>
#include <mutex>
#include <semaphore>
#include <vector>
#include <queue>
#include <array>

int qalsa_format_sizeof(enum qalsa_format format) {
	static int sizes[] = {
		/* [QALSA_FORMAT_S8] = */ 1,
		/* [QALSA_FORMAT_U8] = */ 1,
		/* [QALSA_FORMAT_S16_LE] = */ 2,
		/* [QALSA_FORMAT_S16_BE] = */ 2,
		/* [QALSA_FORMAT_U16_LE] = */ 2,
		/* [QALSA_FORMAT_U16_BE] = */ 2,
		/* [QALSA_FORMAT_S24_LE] = */ 3,
		/* [QALSA_FORMAT_S24_BE] = */ 3,
		/* [QALSA_FORMAT_U24_LE] = */ 3,
		/* [QALSA_FORMAT_U24_BE] = */ 3,
		/* [QALSA_FORMAT_S32_LE] = */ 4,
		/* [QALSA_FORMAT_S32_BE] = */ 4,
		/* [QALSA_FORMAT_U32_LE] = */ 4,
		/* [QALSA_FORMAT_U32_BE] = */ 4,
		/* [QALSA_FORMAT_FLOAT_LE] = */ 4,
		/* [QALSA_FORMAT_FLOAT_BE] = */ 4,
		/* [QALSA_FORMAT_FLOAT64_LE] = */ 8,
		/* [QALSA_FORMAT_FLOAT64_BE] = */ 8,
	};

	assert(format >= 0);
	assert(format < sizeof(sizes) / sizeof(*sizes));
	return sizes[format];
}

namespace {
	struct qalsa_info_ {
		snd_pcm_t *handle;
		unsigned int mul;
		snd_pcm_uframes_t buf_size;
		qalsa_callback callback; void *ptr;
		bool is_done;

		std::mutex queue_mutex;
		std::array<std::pair<bool, uint8_t*>, 4> buffers;
		std::deque<int> queue_indices;
		std::counting_semaphore<4> empty_count, full_count;

		int get_next_buffer_index() const {
			for(int i = 0; const auto &[available, vector] : buffers)
				if(available) return i;
				else i += 1;
			assert(false);
		}

		const char *device;
		unsigned int channels, rate;
		enum qalsa_format format;
	};

	void qalsa__play_thread_(
		struct qalsa_info_ *info
	) {
		while(!info->is_done) {
			info->empty_count.acquire();
			info->queue_mutex.lock();
			info->queue_indices.push_back(info->get_next_buffer_index());
			info->buffers[info->queue_indices.back()].first = false;
			info->is_done = info->callback(
				info->buffers[info->queue_indices.back()].second,
				info->buf_size,
				info->ptr
			);
			info->queue_mutex.unlock();
			info->full_count.release();
		}
	}
}

bool qalsa_play(
	const char *device,
	unsigned int channels,
	unsigned int rate,
	enum qalsa_format format,
	qalsa_callback callback,
	void *ptr
) {
	struct qalsa_info_ info_ = {
		.callback = callback,
		.ptr = ptr,
		.is_done = false,
		.empty_count = std::counting_semaphore<4>{4},
		.full_count = std::counting_semaphore<4>{0},
		.device = device,
		.channels = channels,
		.rate = rate,
		.format = format
	};

	struct qalsa_info_ *info = &info_;

	int res;

	res = snd_pcm_open(
		/* pcm = */ &info->handle,
		/* name = */ device,
		/* stream = */ SND_PCM_STREAM_PLAYBACK,
		/* mode = */ 0
	);

	if(res < 0) {
		printf("ERROR: Can't open \"%s\" PCM device. %s\n", device, snd_strerror(res));
		return false;
	}

	snd_pcm_hw_params_t *params;
	snd_pcm_hw_params_alloca(&params);
	snd_pcm_hw_params_any(info->handle, params);

  if(!snd_pcm_hw_params_test_access(info->handle, params, SND_PCM_ACCESS_RW_INTERLEAVED))
    snd_pcm_hw_params_set_access(info->handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
  else return false;

  if(!snd_pcm_hw_params_test_format(info->handle, params, (snd_pcm_format_t)format))
    snd_pcm_hw_params_set_format(info->handle, params, (snd_pcm_format_t)format);
  else return false;

  if(!snd_pcm_hw_params_test_rate(info->handle, params, rate, 0))
    snd_pcm_hw_params_set_rate_near(info->handle, params, &rate, 0);
  else return false;

  if(!snd_pcm_hw_params_test_channels(info->handle, params, channels))
    snd_pcm_hw_params_set_channels(info->handle, params, channels);
  else return false;

	// unsigned int bufnum = 2;
 //  if(snd_pcm_hw_params_set_periods_near(pcm_handle, params, &bufnum, 0))
 //    return false;

  // if(snd_pcm_hw_params_set_period_size_near(pcm_handle, params, &buf_size, 0))
  //   return false;

  if(snd_pcm_hw_params(info->handle, params) < 0)
    return false;

	{ /* output info */
		printf("PCM name: '%s'\n", snd_pcm_name(info->handle));

		printf("PCM state: %s\n", snd_pcm_state_name(snd_pcm_state(info->handle)));

		{
			unsigned int tmp;
			snd_pcm_hw_params_get_channels(params, &tmp);
			printf("channels: %i ", tmp);
			if(tmp == 1)
				printf("(mono)\n");
			else if(tmp == 2)
				printf("(stereo)\n");
		}

		{
			unsigned int tmp;
			snd_pcm_hw_params_get_rate(params, &tmp, 0);
			printf("rate: %d bps\n", tmp);
		}
	}

	snd_pcm_hw_params_get_buffer_size(params, &info->buf_size);
	info->buf_size = 1024 * info->channels * qalsa_format_sizeof(info->format);

	for(auto &vec : info->buffers) {
		vec.first = true;
		vec.second = new uint8_t[info->buf_size];
	}

	std::thread gen_thread(&qalsa__play_thread_, info);
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	while(!info->is_done) {
		info->full_count.acquire();
		info->queue_mutex.lock();
		auto &front = info->queue_indices.front();
		info->buffers[front].first = true;
		res = snd_pcm_writei(info->handle, info->buffers[front].second, info->buf_size);
		if(res < 0) {
			snd_pcm_prepare(info->handle);
		}
		info->queue_indices.pop_front();
		info->queue_mutex.unlock();
		info->empty_count.release();
	}

done:
	for(auto &vec : info->buffers) {
		delete vec.second;
	}

	gen_thread.join();
	snd_pcm_drain(info->handle);
	snd_pcm_close(info->handle);

	return true;
}
