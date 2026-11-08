/*
 * LD_PRELOAD for android.hardware.gnss@1.0-service (64 bit).
 *
 * Navigation messages are broken end to end with the N GNSS blobs:
 *  - gps.mt6755.so reports them only through the legacy
 *    GpsNavigationMessageCallbacks.navigation_message_callback, which the
 *    AOSP gnss@1.0-impl leaves NULL -> SIGSEGV in mnl2hal_hdlr as soon as an
 *    app (GPS Test) registers a navigation message listener; the service
 *    restarts and GPS is restarted about once per second.
 *  - with that callback patched, mnld itself crashes in
 *    get_gnss_navigation_event (memcpy from NULL after
 *    "mtk_gnss_get_navigation_event GPS sv fail") once satellites are
 *    tracked, throwing away the acquisition.
 *
 * The shim wraps hw_get_module("gps") down to GpsInterface.get_extension and
 * reports no "gps_navigation_message" extension, so the framework never
 * enables navigation messages.  Everything else is passed through unchanged.
 *
 * The blob also has the GPS week rollover: fixes carry timestamps 1024 weeks
 * early (March 2007 in November 2026).  location_cb is wrapped and adds
 * 1024 weeks to any timestamp before 2020.
 *
 * Built freestanding (no libc), DT_NEEDED only on libdl for dlsym.
 */
#include <stddef.h>
#include <stdint.h>

#define RTLD_NEXT ((void *)-1L)
extern void *dlsym(void *handle, const char *symbol);

/* hardware/libhardware/include/hardware/hardware.h (LP64) */
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

/* hardware/libhardware/include/hardware/gps.h */
struct gps_device_t {
	struct hw_device_t common;
	const void *(*get_gps_interface)(struct gps_device_t *dev);
};
struct GpsInterface_head {
	size_t size;
	void *fn[8]; /* init .. set_position_mode */
	const void *(*get_extension)(const char *name);
};
static void copy(void *d, const void *s, size_t n)
{
	volatile unsigned char *dd = d;
	const unsigned char *ss = s;
	while (n--)
		*dd++ = *ss++;
}

static int streq(const char *a, const char *b)
{
	while (*a && *a == *b) {
		a++;
		b++;
	}
	return *a == *b;
}

/* GpsInterface */
/* GpsLocation (gps.h, LP64): timestamp (ms since epoch) at offset 56 */
#define GPSLOC_TIMESTAMP_OFF	56
#define GPSLOC_MAX		128
#define GPS_1024_WEEKS_MS	(1024LL * 7 * 24 * 3600 * 1000)
#define Y2020_MS		1577836800000LL

/* GpsCallbacks (gps.h): size, location_cb, status_cb, ... */
struct GpsCallbacks_head {
	size_t size;
	void (*location_cb)(void *location);
};

static void (*real_location_cb)(void *location);
static unsigned char cbs_buf[256] __attribute__((aligned(16)));

static void wrap_location_cb(void *location)
{
	unsigned char loc[GPSLOC_MAX] __attribute__((aligned(16)));
	size_t n = *(size_t *)location;
	int64_t *ts;

	if (n < GPSLOC_TIMESTAMP_OFF + sizeof(int64_t) || n > sizeof(loc)) {
		real_location_cb(location);
		return;
	}
	copy(loc, location, n);
	ts = (int64_t *)(loc + GPSLOC_TIMESTAMP_OFF);
	while (*ts > 0 && *ts < Y2020_MS)
		*ts += GPS_1024_WEEKS_MS;
	real_location_cb(loc);
}

static const struct GpsInterface_head *real_iface;

static int wrap_init(void *callbacks)
{
	const struct GpsCallbacks_head *c = callbacks;
	size_t n = c->size;

	if (n < sizeof(*c) || n > sizeof(cbs_buf) || !c->location_cb)
		return ((int (*)(void *))real_iface->fn[0])(callbacks);
	copy(cbs_buf, c, n);
	real_location_cb = c->location_cb;
	((struct GpsCallbacks_head *)cbs_buf)->location_cb = wrap_location_cb;
	return ((int (*)(void *))real_iface->fn[0])(cbs_buf);
}
static unsigned char iface_buf[512] __attribute__((aligned(16)));

static const void *wrap_get_extension(const char *name)
{
	if (streq(name, "gps_navigation_message"))
		return NULL;
	return real_iface->get_extension(name);
}

/* gps_device_t */
static struct gps_device_t *real_dev;
static struct gps_device_t dev_wrap;

static const void *wrap_get_gps_interface(struct gps_device_t *dev)
{
	const struct GpsInterface_head *i;
	size_t n;

	(void)dev;
	i = real_dev->get_gps_interface(real_dev);
	if (!i)
		return i;
	n = i->size;
	if (n < sizeof(*i) || n > sizeof(iface_buf))
		return i; /* unexpected layout: leave untouched */
	real_iface = i;
	copy(iface_buf, i, n);
	((struct GpsInterface_head *)iface_buf)->get_extension = wrap_get_extension;
	((struct GpsInterface_head *)iface_buf)->fn[0] = (void *)wrap_init;
	return iface_buf;
}

static int wrap_close(struct hw_device_t *d)
{
	(void)d;
	return real_dev->common.close(&real_dev->common);
}

/* hw_module_t */
static const struct hw_module_t *real_mod;
static struct hw_module_t mod_wrap;
static struct hw_module_methods_t methods_wrap;

static int wrap_open(const struct hw_module_t *module, const char *id,
		     struct hw_device_t **device)
{
	int r;

	(void)module;
	r = real_mod->methods->open(real_mod, id, device);
	if (r || !*device)
		return r;
	real_dev = (struct gps_device_t *)*device;
	copy(&dev_wrap, real_dev, sizeof(dev_wrap));
	dev_wrap.get_gps_interface = wrap_get_gps_interface;
	dev_wrap.common.close = wrap_close;
	*device = &dev_wrap.common;
	return 0;
}

int hw_get_module(const char *id, const struct hw_module_t **module)
{
	static int (*real)(const char *, const struct hw_module_t **);
	int r;

	if (!real)
		real = dlsym(RTLD_NEXT, "hw_get_module");
	if (!real)
		return -2;
	r = real(id, module);
	if (r || !streq(id, "gps") || !*module || !(*module)->methods)
		return r;
	real_mod = *module;
	copy(&mod_wrap, real_mod, sizeof(mod_wrap));
	methods_wrap.open = wrap_open;
	mod_wrap.methods = &methods_wrap;
	*module = &mod_wrap;
	return 0;
}
