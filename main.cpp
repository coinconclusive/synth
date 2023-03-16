#include <cstdio>
#include <cmath>
#include <chrono>
#include <functional>
#include <limits>
#include <iostream>

#include <qalsa.h>

using V = double;

struct Mod {
	std::function<V(V)> f;
	Mod operator>>(Mod b) {
		return Mod {[=](V v) -> V { return b.f(f(v)); }};
	}
};

V operator>>=(V v, Mod b) {
	return b.f(v);
}

constexpr V rate = 44100.0;

namespace mods {
	auto osc_sine(double freq) {
		return Mod {[=](V time) -> V {
			return std::sin(2 * M_PI * time * freq * rate);
		}};
	}

	auto volume(double vol) {
		return Mod {[=](V v) -> V { return v * vol; }};
	}

	template<typename T>
	T sigmoid(T x) {
		return 2.0 / (1.0 + std::exp(-x)) - 1.0;
	}

	auto distort(double vol) {
		return Mod {[=](V v) -> V {
			return sigmoid(M_E * v);
		}};
	}
}

struct State {
	V phase = 0;
};

V compute(size_t i, V time, void *ptr) {
	State &state = *(State*)ptr;
	state.phase += 440.0 / rate;
	if(state.phase >= 1) state.phase -= 1;
	return std::sin(2 * M_PI * state.phase) * 0.25;
}

	// using namespace mods;
	// time >>= osc_sine(440);// >> distort(2.0) >> volume(0.5);


struct qalsa_player {
	using clock_type = std::chrono::high_resolution_clock;
	double time;
	clock_type::time_point last_time;
	clock_type clock;
	void *ptr;
	V (*compute)(size_t sample, V time, void *ptr);

	qalsa_player(decltype(compute) compute, void *ptr)
		: compute(compute), ptr(ptr) {
	}

	void play() {
		last_time = clock.now();
		qalsa_play(
			"default",
			1, rate,
			QALSA_FORMAT_FLOAT_LE,
			&callback_raw,
			this
		);
	}

	double time_debug = 0;
	size_t frame = 0;

	void callback(void *buf, const size_t buf_size) {
		auto start_time = clock.now();
		using E = float;
		constexpr V Efac = 1; // std::numeric_limits<E>::max();
		E *v_buf = (E *)buf;

		auto this_time = clock.now();
		auto delta_time = this_time - last_time;
		auto dt = std::chrono::duration<double>(delta_time).count();

		for(size_t i = 0; i < buf_size / sizeof(E); ++i) {
			auto res = compute(frame + i, V(time), ptr);
			v_buf[i] = E(res * Efac);
		}

		frame += buf_size / sizeof(E);

		time += dt;
		time_debug += dt;

		auto end_time = clock.now();
		if(time_debug > 1) {
			auto run_time = end_time - start_time;
			auto rt = run_time.count();
			auto n = buf_size / sizeof(E);
			printf("time: %fs\n", time);
			printf("  dt: %fs\n", dt);
			printf("  rt: %ldns\n", rt);
			printf("      %ldns/sample\n", rt / n);
			printf("  sample count: %zu\n", n);
			time_debug = 0;
		}

		last_time = this_time;
	}

	static bool callback_raw(void *buf, size_t buf_size, void *ptr) {
		((qalsa_player*)ptr)->callback(buf, buf_size);
		return false;
	}
};

int main() {
	State state;
	qalsa_player p(compute, &state);
	p.play();
}
