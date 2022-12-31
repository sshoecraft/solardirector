
#include "config.c"
#include "devserver.c"

int main(void) {
	rdev_config_t *conf;
	devserver_session_t *s;

debug = 9;

        conf = calloc(1,sizeof(*conf));
        if (!conf) {
                log_write(LOG_SYSERR,"calloc config");
		return 1;
        }
	rdev_get_config(conf,"rdtest.conf");

	s = devserver_new(conf,-1);
	dprintf(4,"s: %p\n", s);
	strcpy((char *)s->data,"can0");
	devserver_open(s);
	return 0;
}
