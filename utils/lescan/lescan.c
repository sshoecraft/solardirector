#ifndef WINDOWS
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <sys/signal.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
 
int signal_received;
static void sigint_handler(int sig)
{
	printf("sig: %d\n", sig);
        signal_received = sig;
}

#define EIR_NAME_SHORT              0x08  /* shortened local name */
#define EIR_NAME_COMPLETE           0x09  /* complete local name */

static void eir_parse_name(uint8_t *eir, size_t eir_len, char *buf, size_t buf_len) {
        size_t offset;

        offset = 0;
        while (offset < eir_len) {
                uint8_t field_len = eir[0];
                size_t name_len;

                /* Check for the end of EIR */
                if (field_len == 0)
                        break;

		printf("offset: %d, field_len: %d, eir_len: %d\n", offset, field_len, eir_len);
                if (offset + field_len > eir_len)
                        goto failed;

                switch (eir[1]) {
                case EIR_NAME_SHORT:
                case EIR_NAME_COMPLETE:
                        name_len = field_len - 1;
                        if (name_len > buf_len)
                                goto failed;

                        memcpy(buf, &eir[2], name_len);
                        return;
                }

                offset += field_len + 1;
                eir += field_len + 1;
        }

failed:
        snprintf(buf, buf_len, "(unknown)");
}

int main(int argc, char **argv) {
    int dev_id, sock, len, err;
        uint8_t own_type = LE_PUBLIC_ADDRESS;
        uint8_t scan_type = 0x01;
//        uint8_t filter_type = 0;
        uint8_t filter_policy = 0x00;
        uint16_t interval = htobs(0x0010);
        uint16_t window = htobs(0x0010);
        uint8_t filter_dup = 0x01;
        unsigned char buf[HCI_MAX_EVENT_SIZE], *ptr;
        struct hci_filter nf, of;
        struct sigaction sa;
        socklen_t olen;



 
    dev_id = hci_get_route(NULL);
    sock = hci_open_dev( dev_id );
    if (dev_id < 0 || sock < 0) {
        perror("opening socket");
        exit(1);
    }
 
        err = hci_le_set_scan_parameters(sock, scan_type, interval, window, own_type, filter_policy, 0);
        if (err < 0) {
                perror("Set scan parameters failed");
		goto done;
        }

        err = hci_le_set_scan_enable(sock, 0x01, filter_dup, 10000);
        if (err < 0) {
                perror("Enable scan failed");
		goto done;
        }
 
        olen = sizeof(of);
        if (getsockopt(sock, SOL_HCI, HCI_FILTER, &of, &olen) < 0) {
                printf("Could not get socket options\n");
                return -1;
        }

        hci_filter_clear(&nf);
        hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
        hci_filter_set_event(EVT_LE_META_EVENT, &nf);

        if (setsockopt(sock, SOL_HCI, HCI_FILTER, &nf, sizeof(nf)) < 0) {
                printf("Could not set socket options\n");
                return -1;
        }

        memset(&sa, 0, sizeof(sa));
        sa.sa_flags = SA_NOCLDSTOP;
        sa.sa_handler = sigint_handler;
        sigaction(SIGINT, &sa, NULL);

        while (1) {
                evt_le_meta_event *meta;
                le_advertising_info *info;
                char addr[18];

                while ((len = read(sock, buf, sizeof(buf))) < 0) {
                        if (errno == EINTR && signal_received == SIGINT) {
                                len = 0;
                                goto done;
                        }

                        if (errno == EAGAIN || errno == EINTR)
                                continue;
                        goto done;
                }

                ptr = buf + (1 + HCI_EVENT_HDR_SIZE);
                len -= (1 + HCI_EVENT_HDR_SIZE);

                meta = (void *) ptr;

                if (meta->subevent != 0x02)
                        goto done;

                /* Ignoring multiple reports */
                info = (le_advertising_info *) (meta->data + 1);
		{
                        char name[30];

                        memset(name, 0, sizeof(name));

                        ba2str(&info->bdaddr, addr);
                        eir_parse_name(info->data, info->length, name, sizeof(name) - 1);

                        printf("%s %s\n", addr, name);
                }
        }

done:
	setsockopt(sock, SOL_HCI, HCI_FILTER, &of, sizeof(of));
        err = hci_le_set_scan_enable(sock, 0x00, filter_dup, 10000);
        if (err < 0) {
                perror("Disable scan failed");
                exit(1);
        }

        hci_close_dev(sock);
    return 0;
}
#else
int main(void) { return 0; }
#endif
