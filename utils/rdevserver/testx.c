
#include "devserver.c"

int main(void) {
	devserver_session_t *s;

	s = devserver_new(-1);
	devserver_open(s);
}
