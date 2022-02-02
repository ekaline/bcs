/* - To build test_mcast_pitch: */
/*   On Linux: */
/*     gcc -o test_mcast_pitch test_mcast_pitch.c */

/*   On Windows: */
/*     cl test_mcast_pitch.c */

/* - Usage information: */
/*   On Linux: */
/*     ./test_mcast_pitch --help */
/*     ./test_mcast_pitch [--verbose] [--gapverbose] --ip <ip> --port <port> [--interface <interface>] [--binfile <filename>] [--maxblocks <blocks>] */

/*   On Windows: */
/*     test_mcast_pitch --help */
/*     test_mcast_pitch [--verbose] [--gapverbose] --ip <ip> --port <port> [--interface <interface>] [--binfile <filename>] [--maxblocks <blocks>] */

/*   where */
/*     --help       : Print usage information */
/*     --verbose    : Display program progress (this option disables pretty printing) */
/*     --gapverbose : Display sequence information when gaps are encountered */
/*     --binfile    : Output file to write mcast binary pitch data to (Optional) */
/*     --maxblocks  : Stop processing after maxblocks message blocks have been received */

/* - Output Columns: */
/*         Bytes := Total bytes received */
/*          Msgs := Total number of messages */
/*          Gaps := Total number of gaps */
/*        Missed := Total messages missed */
/*     BkWrdGaps := Total number of backward gaps */
/*    Duplicates := Total number of duplicates */
/*      IntBytes := Number of bytes in last progress interval */
/*       IntMsgs := Number of messages in last progress interval */
/*       IntGaps := Number of messages in last progress interval */
/*     IntMissed := Number of missed messages in last progress interval */
/*      IntBkWrd := Number of backward messages in last progress interval */
/*       IntDups := Number of duplicates in last progress interval */




#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#ifdef _MSC_VER

#pragma comment( lib, "ws2_32" )

#include <winsock2.h>
#include <ws2tcpip.h>

#define IS_INVALID_SOCKET(SOCKETVAR) SOCKETVAR == INVALID_SOCKET

typedef int socklen_t;
typedef long ssize_t;

int inet_aton(const char *strptr, struct in_addr *addrptr)
{
    if (!addrptr) return 0;

    addrptr->s_addr = inet_addr(strptr);
    return (addrptr->s_addr != INADDR_NONE) ? 1 : 0;
}

char const *socket_error_text() {
    static char message[1024];
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, WSAGetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), message, sizeof(message), NULL);
    return message;
}

#else

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define IS_INVALID_SOCKET(SOCKETVAR) SOCKETVAR < 0

char const *socket_error_text() {
    return strerror(errno);
}

#endif

// ----------------------------------------------------------------------------------------------------
#define MAX_BUFFER_SIZE 131072

int verbose = 0;
int gap_verbose = 0;

FILE *file = 0;
const char *bin_file = 0;

#ifdef _MSC_VER
SOCKET sock = INVALID_SOCKET;
#else
int sock = 0;
#endif

// ----------------------------------------------------------------------------------------------------
struct header_msg {
    unsigned short length;      // Length of entire block of messages (includes header)
    unsigned char  count;       // Number of messages to follow this header
    unsigned char  unit;        // Unit that apply to messages included in this header
    unsigned int   sequence;    // Sequence of first message to follow this header
};

// ----------------------------------------------------------------------------------------------------
struct unit {
    unsigned char id;
    unsigned int sequence;
};

struct unit *make_unit(unsigned char u, unsigned int s) {
    struct unit *un = (struct unit*)malloc(sizeof(struct unit));
    un->id = u;
    un->sequence = s;
    return un;
}

// ----------------------------------------------------------------------------------------------------
struct statistics {
    unsigned long bytes;              // Total bytes received, including header and heartbeat
    unsigned long messages;           // Total number of messages received, excluding header and heartbeat
    unsigned long gaps;               // Total number of gaps detected
    unsigned long missed;             // Total number of messages missed
    unsigned long backward_sequences; // Total number of messages with sequences behind current value
    unsigned long duplicates;         // Total number of messages with sequences equal to current value

    unsigned int int_bytes;              // Interval bytes received, including header and heartbeat
    unsigned int int_messages;           // Interval number of messages received, excluding header and heartbeat
    unsigned int int_gaps;               // Interval number of gaps detected
    unsigned int int_missed;             // Interval number of messages missed
    unsigned int int_backward_sequences; // Interval number of messages with sequences behind current value
    unsigned int int_duplicates;         // Interval number of messages with seuqneces equal to current value
};

void initialize_stats(struct statistics *stats) {
    stats->bytes = 0;
    stats->messages = 0;
    stats->gaps = 0;
    stats->missed = 0;
    stats->backward_sequences = 0;
    stats->duplicates = 0;

    stats->int_bytes = 0;
    stats->int_messages = 0;
    stats->int_gaps = 0;
    stats->int_missed = 0;
    stats->int_backward_sequences = 0;
    stats->int_duplicates = 0;
}

void reset_interval(struct statistics *stats) {
    stats->int_bytes = 0;
    stats->int_messages = 0;
    stats->int_gaps = 0;
    stats->int_missed = 0;
    stats->int_backward_sequences = 0;
    stats->int_duplicates = 0;
}

void update_totals(struct statistics *stats) {
    stats->bytes += stats->int_bytes;
    stats->messages += stats->int_messages;
    stats->gaps += stats->int_gaps;
    stats->missed += stats->int_missed;
    stats->backward_sequences += stats->int_backward_sequences;
    stats->duplicates += stats->int_duplicates;
}

// This call also updates the unit sequence.
void update_stats(struct statistics *stats, struct header_msg *header, struct unit *un, time_t now) {
    unsigned int count = (unsigned int)header->count;
    unsigned int missed = 0;

    stats->int_bytes += header->length;
    stats->int_messages += count;

    if (count > 0) {
        if (header->sequence > (un->sequence + 1)) {
            ++stats->int_gaps;
            missed = header->sequence - un->sequence - 1;
            stats->int_missed += missed;
            if (gap_verbose) { printf("Found gap: expected=%u, found=%u, missed=%u\n", un->sequence + 1, header->sequence, missed); }
            un->sequence = header->sequence + count - 1;
        }
        else if (header->sequence < un->sequence) {
            if (gap_verbose) { printf("Found backward gap: expected=%u, found=%u\n", un->sequence + 1, header->sequence); }
            ++stats->int_backward_sequences;
            un->sequence = header->sequence + count - 1;
        }
        else if (header->sequence == un->sequence) {
            if (gap_verbose) { printf("Found duplicate: sequence=%u\n", header->sequence); }
            ++stats->int_duplicates;
            un->sequence = header->sequence + count - 1;
        }
        else {
            un->sequence += count;
        }
    }
}

void print_stats_header(time_t now) {
    char date_str[11] = { 0 };
    size_t n = strftime(date_str, 11, "%m/%d/%Y", localtime(&now));
    date_str[n] = '\0';
    printf("Date: %s\n", date_str);
    if (!verbose) {
        printf("    Time         Bytes        Msgs        Gaps      Missed   BkWrdGaps  Duplicates  IntBytes   IntMsgs   IntGaps IntMissed  IntBkWrd   IntDups\n");
    }
}

void print_stats(struct statistics *stats, time_t now) {
    static char time_str[10] = { 0 };
    size_t n = strftime(time_str, 10, "%H:%M:%S", localtime(&now));
    time_str[n] = '\0';
    if (verbose) {
        printf("%s: bytes(%lu) msgs(%lu) gaps(%lu) missed(%lu) backward_gaps(%lu) duplicates(%lu) int_bytes(%u) int_msgs(%u) int_gaps(%u) int_missed(%u) int_backward_gaps(%u) int_dups(%u)\n", 
               time_str,
               stats->bytes, stats->messages, stats->gaps, stats->missed, stats->backward_sequences, stats->duplicates,
               stats->int_bytes, stats->int_messages, stats->int_gaps, stats->int_missed, stats->int_backward_sequences, stats->int_duplicates);
    }
    else {
        printf("%s%14lu%12lu%12lu%12lu%12lu%12lu%10u%10u%10u%10u%10u%10u\n",
               time_str,
               stats->bytes, stats->messages, stats->gaps, stats->missed, stats->backward_sequences, stats->duplicates,
               stats->int_bytes, stats->int_messages, stats->int_gaps, stats->int_missed, stats->int_backward_sequences, stats->int_duplicates);
    }
}

// ----------------------------------------------------------------------------------------------------
void print_usage(const char *prog) {
    printf("Usage:\n");
    printf("  %s --help\n", prog);
    printf("  %s [--verbose] [--gapverbose] --ip <ip> --port <port> [--interface <interface>] [--binfile <filename>] [--maxblocks <blocks>]\n", prog);
    printf("\nwhere\n");
    printf("  --help       : Print usage information\n");
    printf("  --verbose    : Display program progress (this option disables pretty printing)\n");
    printf("  --gapverbose : Display sequence information when gaps are encountered\n");
    printf("  --binfile    : Output file to write mcast binary pitch data to (Optional)\n");
    printf("  --maxblocks  : Stop processing after maxblocks message blocks have been received\n");
    printf("\nOutput Columns:\n");
    printf("       Bytes := Total bytes received\n");
    printf("        Msgs := Total number of messages\n");
    printf("        Gaps := Total number of gaps\n");
    printf("      Missed := Total messages missed\n");
    printf("   BkWrdGaps := Total number of backward gaps\n");
    printf("  Duplicates := Total number of duplicates\n");
    printf("    IntBytes := Number of bytes in last progress interval\n");
    printf("     IntMsgs := Number of messages in last progress interval\n");
    printf("     IntGaps := Number of messages in last progress interval\n");
    printf("   IntMissed := Number of missed messages in last progress interval\n");
    printf("    IntBkWrd := Number of backward messages in last progress interval\n");
    printf("     IntDups := Number of duplicates in last progress interval\n");
    printf("\n");
}

void initialize_socket_library() {
#ifdef _MSC_VER
    int ires;
    WSADATA wsa_data;
    if ((ires = WSAStartup(MAKEWORD(2,0), &wsa_data)) != 0) {
        printf("Error calling WSAStartup: %d\n", ires);
        exit(1);
    }
#endif
}

void close_binary_file(int flush) {
    if (bin_file && file) {
        if (flush) {
            if (fflush(file) != 0) { fprintf(stderr, "Error flushing: %s\n", socket_error_text()); }
        }

        fclose(file);
        file = 0;
    }
}

// ----------------------------------------------------------------------------------------------------
const char *get_args(int index, int size, char **args) {
    if (index < size) { return args[index]; }
    else { fprintf(stderr, "get_args: Index exceeds args size\n"); exit(1); }
}

int main(int argc, char **argv) {
    const char *ip = 0;
    int port = 0;
    const char *interface_address = 0;
    char buffer[MAX_BUFFER_SIZE];
    ssize_t bytes_read = 0;
    time_t now = 0;
    time_t last_print_at = time(0);
    int idx = 0;
    long max_blocks = 0;
    long blocks = 0;

    const int on=1;

    struct sockaddr_in address;
    struct sockaddr receive_address;
    socklen_t receive_length;
    struct in_addr addr;
    struct in_addr interface_addr;
    struct ip_mreq mcast_req;

    struct header_msg header;
    struct unit *un = 0;
    struct statistics stats;

    // Initialize structures
    initialize_stats(&stats);
    memset(&address, 0, sizeof(address));
    memset(&receive_address, 0, sizeof(receive_address));
    memset(&addr, 0, sizeof(addr));
    memset(&interface_addr, 0, sizeof(interface_addr));
    memset(&mcast_req, 0, sizeof(mcast_req));

    // Initialize socket library
    initialize_socket_library();

    // Read command line arguments
    for (idx = 1; idx < argc; ++idx) {
        const char *arg = argv[idx];
        if (strcmp(arg, "--ip") == 0) { ip = get_args(++idx, argc, argv); }
        else if (strcmp(arg, "--port") == 0) { port = atoi(get_args(++idx, argc, argv)); }
        else if (strcmp(arg, "--interface") == 0) { interface_address = get_args(++idx, argc, argv); }
        else if (strcmp(arg, "--binfile") == 0) { bin_file = get_args(++idx, argc, argv); }
        else if (strcmp(arg, "--maxblocks") == 0) { max_blocks = atol(get_args(++idx, argc, argv)); }
        else if (strcmp(arg, "--verbose") == 0) { verbose = 1; }
        else if (strcmp(arg, "--gapverbose") == 0) { gap_verbose = 1; }
        else { print_usage(argv[0]); return 1; }
    }
    if (ip == 0) { fprintf(stderr, "\n *** ip not specified\n\n"); print_usage(argv[0]); return 1; }
    if (port <= 0) { fprintf(stderr, "\n *** port is not < 0\n\n"); print_usage(argv[0]); return 1; }
    if (max_blocks < 0) { fprintf(stderr, "\n *** max_blocks is not >= 0\n\n"); print_usage(argv[0]); return 1; }

    printf("ip: %s\nport: %i\ninterface: %s\nbinfile: %s\nmaxblocks: %ld\n",
           ip, port, interface_address ? interface_address : "", bin_file ? bin_file : "", max_blocks);

    // Create socket
    if (verbose) { printf("Creating socket\n"); }
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (IS_INVALID_SOCKET(sock)) {
        fprintf(stderr, "Error creating socket: %s\n", socket_error_text());
        return 1;
    }

    if (verbose) { printf("Setting SO_REUSEADDR\n"); }
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on) ) < 0) {
        fprintf(stderr, "Error setting SO_REUSEADDR: %s\n", socket_error_text());
        return 1;
    }

    // Convert ip
    if (!inet_aton(ip, &addr)) {
        fprintf(stderr, "Error converting ip to binary data\n");
        return 1;
    }

    // Initialize address
    if (verbose) { printf("Initializing socket address\n"); }
    address.sin_family = AF_INET;
#ifdef _MSC_VER
    address.sin_addr.s_addr = htonl(INADDR_ANY);
#else
    address.sin_addr.s_addr = addr.s_addr;
#endif
    address.sin_port = htons(port);

    // Bind
    if (verbose) { printf("Binding\n"); }
    if (bind(sock, (struct sockaddr *)&address, sizeof(address)) < 0) {
        fprintf(stderr, "Error binding: %s\n", socket_error_text());
        return 1;
    }

    // Initialize mcast request
    if (verbose) { printf("Initializing mcast request\n"); }
    mcast_req.imr_multiaddr.s_addr = addr.s_addr;
    if (interface_address) {
        if (verbose) { printf("Initializing interface address\n"); }

        // Convert ip
        if (!inet_aton(interface_address, &interface_addr)) {
            fprintf(stderr, "Error converting interface to binary data\n");
            return 1;
        }
        mcast_req.imr_interface.s_addr = interface_addr.s_addr;
    }
    else {
        mcast_req.imr_interface.s_addr = htonl(INADDR_ANY);
    }

    // Join mcast group
    if (verbose) { printf("Joining mcast group\n"); }
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *)&mcast_req, sizeof(mcast_req)) < 0) {
        fprintf(stderr, "Error joining group: %s\n", socket_error_text());
        return 1;
    }

    // Open bin file if specified
    if (bin_file) {
        if (verbose) { printf("Opening binary file for writing\n"); }
        file = fopen(bin_file, "wb");
        if (file == 0) {
            fprintf(stderr, "Error opening file '%s': %s\n", bin_file, socket_error_text());
            return 1;
        }
    }

    // Test read/update/display loop
    if (verbose) { printf("Entering test loop\n"); }
    print_stats_header(last_print_at);
    for (;;) {
        receive_length = sizeof(receive_address);
        bytes_read = recvfrom(sock, buffer, MAX_BUFFER_SIZE, 0, &receive_address, &receive_length);
        if (bytes_read < 0) {
            fprintf(stderr, "Error reading from socket: %s\n", socket_error_text());
            close_binary_file(0);
            return 1;
        }
        else if ((uint64_t)bytes_read < sizeof(header)) {
            fprintf(stderr, "Invalid message read, size smaller than message header\n");
            continue;
        }

        ++blocks;
        if (max_blocks > 0 && blocks > max_blocks) { break; }

        // Write to binary file if file is open
        if (file) {
            if (fwrite((void *)buffer, sizeof(unsigned char), bytes_read, file) != (uint64_t)bytes_read) {
                fprintf(stderr, "Error writing to binary file: %s\n", socket_error_text());
                close_binary_file(0);
            }
        }

        memcpy((void *)&header, (const void *)&buffer, sizeof(header));
        if (verbose) {
            printf("Received: length=%u count=%u, unit=%u sequence=%u\n", 
                   header.length, header.count, header.unit, header.sequence);
        }
        if (bytes_read != (unsigned int)header.length) {
            printf("Block message read size(%ld) does not match header.length(%u)\n", bytes_read, header.length);
            close_binary_file(0);
            return 1;
        }

        if (!un) {
            if (verbose) { printf("Creating unit: id=%u\n", header.unit); }
            un = make_unit(header.unit, header.sequence - 1);
        }
        if (verbose) {
            printf("Unit: id=%u sequence=%u\n", un->id, un->sequence);
        }

        now = time(0);
        update_stats(&stats, &header, un, now);
        if (difftime(now, last_print_at) >= 1) {
            update_totals(&stats);
            print_stats(&stats, now);
            reset_interval(&stats);
            last_print_at = now;
        }
    }

    close_binary_file(1);

    return 0;
}
