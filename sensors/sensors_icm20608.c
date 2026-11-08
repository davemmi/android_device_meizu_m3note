/*
 * Sensors HAL (legacy hw module "sensors", API 1.3) for the Meizu M5 Note
 * ICM-20608-D on the mainline inv_mpu6050 IIO driver.
 *
 * The M3 sensors.mt6755.so is the Invensense MPL, which needs the
 * out-of-tree Invensense DMP driver and Meizu sysfs nodes; the 4.9 kernel
 * has neither.  This HAL reads accelerometer and gyroscope from the IIO
 * buffer (/dev/iio:deviceN, all six axes plus timestamp enabled) and maps
 * the axes like the stock M5 DT (axis_map 1/0/2, all negated):
 *   android_x = -raw_y, android_y = -raw_x, android_z = -raw_z
 *
 * Built freestanding against bionic (no headers): only libc/liblog symbols
 * that exist in every bionic are used.
 */
#include <stddef.h>
#include <stdint.h>

/* ---- bionic ---- */
extern int open(const char *path, int flags, ...);
extern int close(int fd);
extern long read(int fd, void *buf, size_t n);
extern long write(int fd, const void *buf, size_t n);
extern int pipe2(int fds[2], int flags);
struct pollfd { int fd; short events; short revents; };
extern int poll(struct pollfd *fds, unsigned long n, int timeout);
struct timespec { long tv_sec; long tv_nsec; };
extern int clock_gettime(int clk, struct timespec *ts);
extern int pthread_mutex_lock(void *m);
extern int pthread_mutex_unlock(void *m);
extern int __android_log_print(int prio, const char *tag, const char *fmt, ...);

#define O_RDONLY	0
#define O_WRONLY	1
#define O_NONBLOCK	04000
#define O_CLOEXEC	02000000
#define POLLIN		1
#define CLOCK_BOOTTIME	7
#define EINTR_NEG	(-4)

#define TAG "sensors.icm20608"
#define LOGI(...) __android_log_print(4, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(6, TAG, __VA_ARGS__)

/* ---- hardware.h / sensors.h (LP64) ---- */
#define MAKE_TAG(a, b, c, d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))
#define HARDWARE_MODULE_TAG	MAKE_TAG('H', 'W', 'M', 'T')
#define HARDWARE_DEVICE_TAG	MAKE_TAG('H', 'W', 'D', 'T')
#define SENSORS_DEVICE_API_VERSION_1_3	((1 << 24) | (3 << 16) | 1)

#define SENSOR_TYPE_ACCELEROMETER	1
#define SENSOR_TYPE_GYROSCOPE		4
#define SENSOR_TYPE_META_DATA		0
#define META_DATA_FLUSH_COMPLETE	1
#define META_DATA_VERSION		0x103
#define SENSOR_STATUS_ACCURACY_HIGH	3
#define SENSOR_FLAG_CONTINUOUS_MODE	0
#define GRAVITY_EARTH			9.80665f

struct hw_module_t;
struct hw_device_t;
struct hw_module_methods_t {
	int (*open)(const struct hw_module_t *module, const char *id,
		    struct hw_device_t **device);
};
struct hw_module_t {
	uint32_t tag;
	uint16_t module_api_version;
	uint16_t hal_api_version;
	const char *id;
	const char *name;
	const char *author;
	struct hw_module_methods_t *methods;
	void *dso;
	uint64_t reserved[32 - 7];
};
struct hw_device_t {
	uint32_t tag;
	uint32_t version;
	struct hw_module_t *module;
	uint64_t reserved[12];
	int (*close)(struct hw_device_t *device);
};

struct sensor_t {
	const char *name;
	const char *vendor;
	int version;
	int handle;
	int type;
	float maxRange;
	float resolution;
	float power;
	int32_t minDelay;
	uint32_t fifoReservedEventCount;
	uint32_t fifoMaxEventCount;
	const char *stringType;
	const char *requiredPermission;
	int64_t maxDelay;
	uint64_t flags;
	void *reserved[2];
};

typedef struct {
	int32_t version;
	int32_t sensor;
	int32_t type;
	int32_t reserved0;
	int64_t timestamp;
	union {
		float data[16];
		struct {
			float x, y, z;
			int8_t status;
			uint8_t reserved[3];
		} vec;
		struct {
			int32_t what;
			int32_t sensor;
		} meta;
		uint64_t u64[8];
	};
	uint32_t flags;
	uint32_t reserved1[3];
} sensors_event_t;

struct sensors_poll_device_1 {
	struct hw_device_t common;
	int (*activate)(struct sensors_poll_device_1 *dev, int handle, int en);
	int (*setDelay)(struct sensors_poll_device_1 *dev, int handle,
			int64_t ns);
	int (*poll)(struct sensors_poll_device_1 *dev, sensors_event_t *data,
		    int count);
	int (*batch)(struct sensors_poll_device_1 *dev, int handle, int flags,
		     int64_t period_ns, int64_t timeout);
	int (*flush)(struct sensors_poll_device_1 *dev, int handle);
	int (*inject_sensor_data)(struct sensors_poll_device_1 *dev,
				  const sensors_event_t *data);
	void *register_direct_channel;
	void *config_direct_report;
	void (*reserved_procs[5])(void);
};

struct sensors_module_t {
	struct hw_module_t common;
	int (*get_sensors_list)(struct sensors_module_t *module,
				const struct sensor_t **list);
	int (*set_operation_mode)(unsigned int mode);
};

_Static_assert(sizeof(struct sensor_t) == 104, "sensor_t");
_Static_assert(sizeof(sensors_event_t) == 104, "sensors_event_t");
_Static_assert(offsetof(struct sensors_poll_device_1, flush) == 152, "flush");

/* ---- helpers ---- */
static size_t slen(const char *s)
{
	size_t n = 0;

	while (s[n])
		n++;
	return n;
}

static char *scat(char *d, const char *s)
{
	while (*s)
		*d++ = *s++;
	*d = 0;
	return d;
}

static void zero(void *p, size_t n)
{
	volatile unsigned char *d = p;

	while (n--)
		*d++ = 0;
}

static int sysfs_read(const char *path, char *buf, size_t n)
{
	int fd = open(path, O_RDONLY | O_CLOEXEC);
	long r;

	if (fd < 0)
		return -1;
	r = read(fd, buf, n - 1);
	close(fd);
	if (r < 0)
		return -1;
	buf[r] = 0;
	while (r > 0 && (buf[r - 1] == '\n' || buf[r - 1] == ' '))
		buf[--r] = 0;
	return 0;
}

static int sysfs_write(const char *path, const char *val)
{
	int fd = open(path, O_WRONLY | O_CLOEXEC);
	long r;

	if (fd < 0)
		return -1;
	r = write(fd, val, slen(val));
	close(fd);
	return r < 0 ? -1 : 0;
}

static float parse_float(const char *s)
{
	float v = 0, f = 0.1f;
	int neg = 0;

	if (*s == '-') {
		neg = 1;
		s++;
	}
	while (*s >= '0' && *s <= '9')
		v = v * 10 + (*s++ - '0');
	if (*s == '.') {
		s++;
		while (*s >= '0' && *s <= '9') {
			v += (*s++ - '0') * f;
			f *= 0.1f;
		}
	}
	return neg ? -v : v;
}

static void utoa(unsigned v, char *out)
{
	char tmp[12];
	int i = 0;

	do {
		tmp[i++] = '0' + v % 10;
		v /= 10;
	} while (v);
	while (i)
		*out++ = tmp[--i];
	*out = 0;
}

/* ---- state ---- */
enum { H_ACCEL = 1, H_GYRO = 2 };

static char dev_dir[64];	/* /sys/bus/iio/devices/iio:deviceN */
static char dev_node[32];	/* /dev/iio:deviceN */
static float accel_scale = 0.000598f, gyro_scale = 0.000133090f;
static int iio_fd = -1;
static int wake_pipe[2] = { -1, -1 };
static int active_mask;
static int flush_pending[3];
static int64_t period_ns[3] = { 0, 200000000, 200000000 };
static int ts_boottime;		/* driver stamps CLOCK_BOOTTIME, else stamp on read */
static int64_t mutex_storage[5];	/* bionic pthread_mutex_t is 40 bytes on LP64 */

static void path(char *out, const char *attr)
{
	scat(scat(scat(out, dev_dir), "/"), attr);
}

static int attr_write(const char *attr, const char *val)
{
	char p[160];

	path(p, attr);
	return sysfs_write(p, val);
}

static int find_device(void)
{
	char p[96], name[32], num[12];
	unsigned i;

	for (i = 0; i < 8; i++) {
		utoa(i, num);
		scat(scat(scat(p, "/sys/bus/iio/devices/iio:device"), num),
		     "/name");
		if (sysfs_read(p, name, sizeof(name)))
			continue;
		if (name[0] == 'i' && name[1] == 'c' && name[2] == 'm') {
			scat(scat(dev_dir, "/sys/bus/iio/devices/iio:device"),
			     num);
			scat(scat(dev_node, "/dev/iio:device"), num);
			return 0;
		}
	}
	return -1;
}

static void apply_rate(void)
{
	int64_t p = 0;
	unsigned hz;
	char v[12];

	if ((active_mask & H_ACCEL) && (!p || period_ns[H_ACCEL] < p))
		p = period_ns[H_ACCEL];
	if ((active_mask & H_GYRO) && (!p || period_ns[H_GYRO] < p))
		p = period_ns[H_GYRO];
	if (!p)
		return;
	hz = (unsigned)(1000000000LL / p);
	if (hz < 5)
		hz = 5;
	if (hz > 200)
		hz = 200;
	utoa(hz, v);
	attr_write("sampling_frequency", v);
}

static void set_buffer(int on)
{
	attr_write("buffer/enable", "0");
	if (on) {
		apply_rate();
		attr_write("buffer/enable", "1");
	}
}

static void wake(void)
{
	char c = 1;

	if (wake_pipe[1] >= 0)
		write(wake_pipe[1], &c, 1);
}

/* ---- device ops ---- */
static int dev_activate(struct sensors_poll_device_1 *dev, int handle, int en)
{
	int old;

	(void)dev;
	if (handle != H_ACCEL && handle != H_GYRO)
		return -22;
	pthread_mutex_lock(mutex_storage);
	old = active_mask;
	if (en)
		active_mask |= handle;
	else
		active_mask &= ~handle;
	if (active_mask != old)
		set_buffer(active_mask != 0);
	pthread_mutex_unlock(mutex_storage);
	wake();
	return 0;
}

static int dev_batch(struct sensors_poll_device_1 *dev, int handle, int flags,
		     int64_t ns, int64_t timeout)
{
	(void)dev;
	(void)flags;
	(void)timeout;
	if (handle != H_ACCEL && handle != H_GYRO)
		return -22;
	pthread_mutex_lock(mutex_storage);
	period_ns[handle] = ns > 0 ? ns : 5000000;
	if (active_mask) {
		attr_write("buffer/enable", "0");
		apply_rate();
		attr_write("buffer/enable", "1");
	}
	pthread_mutex_unlock(mutex_storage);
	return 0;
}

static int dev_set_delay(struct sensors_poll_device_1 *dev, int handle,
			 int64_t ns)
{
	return dev_batch(dev, handle, 0, ns, 0);
}

static int dev_flush(struct sensors_poll_device_1 *dev, int handle)
{
	(void)dev;
	if (handle != H_ACCEL && handle != H_GYRO)
		return -22;
	if (!(active_mask & handle))
		return -22;
	flush_pending[handle] = 1;
	wake();
	return 0;
}

static int64_t now_boottime(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_BOOTTIME, &ts);
	return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static int16_t be16(const unsigned char *p)
{
	return (int16_t)((p[0] << 8) | p[1]);
}

static void fill(sensors_event_t *e, int handle, int64_t ts, float x, float y,
		 float z)
{
	zero(e, sizeof(*e));
	e->version = sizeof(*e);
	e->sensor = handle;
	e->type = handle == H_ACCEL ? SENSOR_TYPE_ACCELEROMETER
				    : SENSOR_TYPE_GYROSCOPE;
	e->timestamp = ts;
	e->vec.x = x;
	e->vec.y = y;
	e->vec.z = z;
	e->vec.status = SENSOR_STATUS_ACCURACY_HIGH;
}

#define FRAME 24	/* 6 x be16, pad 4, s64 timestamp */

static int dev_poll(struct sensors_poll_device_1 *dev, sensors_event_t *data,
		    int count)
{
	unsigned char buf[FRAME * 16];
	struct pollfd pf[2];
	int n = 0, h, i, mask;
	long r;

	(void)dev;
	for (;;) {
		for (h = H_ACCEL; h <= H_GYRO && n < count; h++) {
			if (flush_pending[h]) {
				flush_pending[h] = 0;
				zero(&data[n], sizeof(data[n]));
				data[n].version = META_DATA_VERSION;
				data[n].type = SENSOR_TYPE_META_DATA;
				data[n].meta.what = META_DATA_FLUSH_COMPLETE;
				data[n].meta.sensor = h;
				n++;
			}
		}
		if (n)
			return n;

		pf[0].fd = wake_pipe[0];
		pf[0].events = POLLIN;
		pf[0].revents = 0;
		pf[1].fd = iio_fd;
		pf[1].events = POLLIN;
		pf[1].revents = 0;
		if (poll(pf, active_mask ? 2 : 1, -1) < 0)
			continue;
		if (pf[0].revents & POLLIN) {
			char c[16];

			read(wake_pipe[0], c, sizeof(c));
		}
		if (!(pf[1].revents & POLLIN) || !active_mask)
			continue;

		i = count / 2;
		if (i < 1)
			i = 1;
		if (i > 16)
			i = 16;
		r = read(iio_fd, buf, (size_t)i * FRAME);
		if (r < FRAME)
			continue;
		mask = active_mask;
		for (i = 0; i + FRAME <= r && n < count; i += FRAME) {
			const unsigned char *f = buf + i;
			int64_t ts = 0;
			int k;

			for (k = 0; k < 8; k++)
				ts |= (int64_t)f[16 + k] << (8 * k);
			if (!ts_boottime)
				ts = now_boottime();
			if ((mask & H_ACCEL) && n < count) {
				float s = accel_scale;

				fill(&data[n++], H_ACCEL, ts,
				     -be16(f + 2) * s, -be16(f + 0) * s,
				     -be16(f + 4) * s);
			}
			if ((mask & H_GYRO) && n < count) {
				float s = gyro_scale;

				fill(&data[n++], H_GYRO, ts,
				     -be16(f + 8) * s, -be16(f + 6) * s,
				     -be16(f + 10) * s);
			}
		}
		if (n)
			return n;
	}
}

static int dev_close(struct hw_device_t *d)
{
	(void)d;
	return 0;
}

/* ---- module ---- */
static struct sensor_t sensor_list[] = {
	{
		.name = "ICM-20608-D Accelerometer",
		.vendor = "InvenSense",
		.version = 1,
		.handle = H_ACCEL,
		.type = SENSOR_TYPE_ACCELEROMETER,
		.maxRange = 2.0f * GRAVITY_EARTH,
		.resolution = GRAVITY_EARTH / 16384.0f,
		.power = 0.4f,
		.minDelay = 5000,
		.stringType = "android.sensor.accelerometer",
		.maxDelay = 200000,
		.flags = SENSOR_FLAG_CONTINUOUS_MODE,
	},
	{
		.name = "ICM-20608-D Gyroscope",
		.vendor = "InvenSense",
		.version = 1,
		.handle = H_GYRO,
		.type = SENSOR_TYPE_GYROSCOPE,
		.maxRange = 34.9f,	/* 2000 dps */
		.resolution = 0.001065f,
		.power = 3.2f,
		.minDelay = 5000,
		.stringType = "android.sensor.gyroscope",
		.maxDelay = 200000,
		.flags = SENSOR_FLAG_CONTINUOUS_MODE,
	},
};

static int sensors_count;

static int setup(void)
{
	static const char *const en[] = {
		"scan_elements/in_accel_x_en", "scan_elements/in_accel_y_en",
		"scan_elements/in_accel_z_en", "scan_elements/in_anglvel_x_en",
		"scan_elements/in_anglvel_y_en", "scan_elements/in_anglvel_z_en",
		"scan_elements/in_timestamp_en",
	};
	char p[160], v[32];
	unsigned i;

	if (find_device()) {
		LOGE("no icm iio device");
		return -1;
	}
	path(p, "in_accel_scale");
	if (!sysfs_read(p, v, sizeof(v)))
		accel_scale = parse_float(v);
	path(p, "in_anglvel_scale");
	if (!sysfs_read(p, v, sizeof(v)))
		gyro_scale = parse_float(v);
	/* inv_mpu6050 scales are m/s^2 and rad/s per LSB, as Android wants */
	attr_write("buffer/enable", "0");
	for (i = 0; i < sizeof(en) / sizeof(en[0]); i++)
		attr_write(en[i], "1");
	attr_write("buffer/length", "480");
	if (!attr_write("current_timestamp_clock", "boottime"))
		ts_boottime = 1;
	iio_fd = open(dev_node, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (iio_fd < 0) {
		LOGE("open %s failed", dev_node);
		return -1;
	}
	if (pipe2(wake_pipe, O_NONBLOCK | O_CLOEXEC)) {
		close(iio_fd);
		iio_fd = -1;
		return -1;
	}
	LOGI("%s: accel scale %d ug, gyro scale %d urad, boottime %d", dev_dir,
	     (int)(accel_scale * 1e6f), (int)(gyro_scale * 1e6f), ts_boottime);
	return 0;
}

static int get_sensors_list(struct sensors_module_t *m,
			    const struct sensor_t **list)
{
	(void)m;
	if (!sensors_count && !setup())
		sensors_count = 2;
	*list = sensor_list;
	return sensors_count;
}

static int set_operation_mode(unsigned int mode)
{
	return mode ? -22 : 0;
}

static struct sensors_poll_device_1 poll_dev;

static int module_open(const struct hw_module_t *module, const char *id,
		       struct hw_device_t **device)
{
	(void)id;
	if (!sensors_count && !setup())
		sensors_count = 2;
	zero(&poll_dev, sizeof(poll_dev));
	poll_dev.common.tag = HARDWARE_DEVICE_TAG;
	poll_dev.common.version = SENSORS_DEVICE_API_VERSION_1_3;
	poll_dev.common.module = (struct hw_module_t *)module;
	poll_dev.common.close = dev_close;
	poll_dev.activate = dev_activate;
	poll_dev.setDelay = dev_set_delay;
	poll_dev.poll = dev_poll;
	poll_dev.batch = dev_batch;
	poll_dev.flush = dev_flush;
	*device = &poll_dev.common;
	return 0;
}

static struct hw_module_methods_t module_methods = {
	.open = module_open,
};

__attribute__((visibility("default")))
struct sensors_module_t HMI = {	/* HAL_MODULE_INFO_SYM */
	.common = {
		.tag = HARDWARE_MODULE_TAG,
		.module_api_version = 1,	/* SENSORS_MODULE_API_VERSION_0_1 */
		.hal_api_version = 0,
		.id = "sensors",
		.name = "M5 Note ICM-20608-D sensors",
		.author = "m3note port",
		.methods = &module_methods,
	},
	.get_sensors_list = get_sensors_list,
	.set_operation_mode = set_operation_mode,
};
