#ifndef QALSA_H_
#define QALSA_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>

enum qalsa_format {
	/** Signed 8 bit */
	QALSA_FORMAT_S8 = 0,
	/** Unsigned 8 bit */
	QALSA_FORMAT_U8,
	/** Signed 16 bit Little Endian */
	QALSA_FORMAT_S16_LE,
	/** Signed 16 bit Big Endian */
	QALSA_FORMAT_S16_BE,
	/** Unsigned 16 bit Little Endian */
	QALSA_FORMAT_U16_LE,
	/** Unsigned 16 bit Big Endian */
	QALSA_FORMAT_U16_BE,
	/** Signed 24 bit Little Endian using low three bytes in 32-bit word */
	QALSA_FORMAT_S24_LE,
	/** Signed 24 bit Big Endian using low three bytes in 32-bit word */
	QALSA_FORMAT_S24_BE,
	/** Unsigned 24 bit Little Endian using low three bytes in 32-bit word */
	QALSA_FORMAT_U24_LE,
	/** Unsigned 24 bit Big Endian using low three bytes in 32-bit word */
	QALSA_FORMAT_U24_BE,
	/** Signed 32 bit Little Endian */
	QALSA_FORMAT_S32_LE,
	/** Signed 32 bit Big Endian */
	QALSA_FORMAT_S32_BE,
	/** Unsigned 32 bit Little Endian */
	QALSA_FORMAT_U32_LE,
	/** Unsigned 32 bit Big Endian */
	QALSA_FORMAT_U32_BE,
	/** Float 32 bit Little Endian, Range -1.0 to 1.0 */
	QALSA_FORMAT_FLOAT_LE,
	/** Float 32 bit Big Endian, Range -1.0 to 1.0 */
	QALSA_FORMAT_FLOAT_BE,
	/** Float 64 bit Little Endian, Range -1.0 to 1.0 */
	QALSA_FORMAT_FLOAT64_LE,
	/** Float 64 bit Big Endian, Range -1.0 to 1.0 */
	QALSA_FORMAT_FLOAT64_BE,
};

/** Callback function. shall write values of specified format
    to output. At most `bufsize` bytes can be written. */
typedef bool (*qalsa_callback)(void *output, size_t bufsize, void *ptr);

/** Return the size of a single value of the specified format ino bytes. */
int qalsa_format_sizeof(enum qalsa_format format);

/** Play generated audio on specified device. The callback
    function is called to retreive the samples for the 
		specified number of channels at a specified rate in Hertz
		in the specified format. The pointer is passed to the callback. */
bool qalsa_play(
	const char *device,
	unsigned int channels,
	unsigned int rate,
	enum qalsa_format format,
	qalsa_callback callback,
	void *ptr
);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // QALSA_H_
