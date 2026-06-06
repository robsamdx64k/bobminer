#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <jansson.h>
#include <netdb.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define MAX_BRANCHES 32
#define MAX_HEX 4096
#define MAX_LINE 16384
#define MAX_THREADS 64

typedef struct {
    char job_id[128];
    char prevhash[129];
    char coinb1[MAX_HEX];
    char coinb2[MAX_HEX];
    char branches[MAX_BRANCHES][129];
    int branch_count;
    char version[16];
    char nbits[16];
    char ntime[16];
    int clean;
    uint32_t job_seq;
} job_t;

typedef struct {
    char host[256];
    char port[16];
    char user[512];
    char pass[256];
    char url[512];
    int threads;
} config_t;

static config_t g_cfg;
static int g_sock = -1;
static volatile sig_atomic_t g_stop = 0;
static pthread_mutex_t g_job_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_sock_lock = PTHREAD_MUTEX_INITIALIZER;
static job_t g_job;
static char g_extranonce1[128] = {0};
static int g_extranonce2_size = 4;
static double g_diff = 1.0;
static uint64_t g_hashes = 0;
static uint32_t g_target_prefix_zero_bits = 32; // Bitcoin-style diff1 starts around 32 leading zero bits

static void die(const char *msg) { perror(msg); exit(1); }
static void on_sig(int s) { (void)s; g_stop = 1; if (g_sock >= 0) close(g_sock); }

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static size_t hex2bin(const char *hex, uint8_t *out, size_t maxout) {
    size_t len = strlen(hex);
    if (len % 2) return 0;
    size_t n = len / 2;
    if (n > maxout) return 0;
    for (size_t i = 0; i < n; i++) {
        int a = hexval(hex[2*i]), b = hexval(hex[2*i+1]);
        if (a < 0 || b < 0) return 0;
        out[i] = (uint8_t)((a << 4) | b);
    }
    return n;
}

static void bin2hex(const uint8_t *bin, size_t len, char *out) {
    static const char *h = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) { out[2*i] = h[bin[i] >> 4]; out[2*i+1] = h[bin[i] & 15]; }
    out[2*len] = 0;
}

static void rev32(uint8_t *x) { for (int i=0;i<16;i++){ uint8_t t=x[i]; x[i]=x[31-i]; x[31-i]=t; } }
static void put_le32(uint8_t *p, uint32_t v) { p[0]=v; p[1]=v>>8; p[2]=v>>16; p[3]=v>>24; }

static uint32_t hex_u32(const char *s) { return (uint32_t)strtoul(s, NULL, 16); }

static void sha256d(const uint8_t *in, size_t len, uint8_t out[32]) {
    uint8_t tmp[32];
    SHA256(in, len, tmp);
    SHA256(tmp, 32, out);
}

static void sha3_256_once(const uint8_t *in, size_t len, uint8_t out[32]) {
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha3_256(), NULL);
    EVP_DigestUpdate(ctx, in, len);
    unsigned int outlen = 0;
    EVP_DigestFinal_ex(ctx, out, &outlen);
    EVP_MD_CTX_free(ctx);
}

static void sha3_256t(const uint8_t *in, size_t len, uint8_t out[32]) {
    uint8_t h1[32], h2[32];
    sha3_256_once(in, len, h1);
    sha3_256_once(h1, 32, h2);
    sha3_256_once(h2, 32, out);
}

static bool hash_meets_share(const uint8_t hash[32]) {
    // Conservative quick check. Pools validate real target. This submits only hashes with enough leading zero bits.
    uint32_t bits = g_target_prefix_zero_bits;
    for (uint32_t i = 0; i < bits / 8; i++) if (hash[i] != 0) return false;
    uint32_t rem = bits % 8;
    if (rem) {
        uint8_t mask = (uint8_t)(0xff << (8 - rem));
        if ((hash[bits/8] & mask) != 0) return false;
    }
    return true;
}

static void update_prefix_from_diff(double diff) {
    if (diff <= 0) diff = 1.0;

    int bits = 32;

    if (diff >= 1.0) {
        while (diff >= 2.0 && bits < 64) {
            bits++;
            diff /= 2.0;
        }
    } else {
        while (diff < 1.0 && bits > 20) {
            bits--;
            diff *= 2.0;
        }
    }

    g_target_prefix_zero_bits = (uint32_t)bits;
}

static int parse_url(const char *url, char *host, size_t hsz, char *port, size_t psz) {
    const char *p = strstr(url, "://");
    p = p ? p + 3 : url;
    const char *colon = strrchr(p, ':');
    if (!colon) return -1;
    size_t hl = (size_t)(colon - p);
    if (hl >= hsz) return -1;
    memcpy(host, p, hl); host[hl] = 0;
    snprintf(port, psz, "%s", colon + 1);
    char *slash = strchr(port, '/'); if (slash) *slash = 0;
    return 0;
}

static int connect_pool(void) {
    struct addrinfo hints = {0}, *res = NULL, *rp;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    int e = getaddrinfo(g_cfg.host, g_cfg.port, &hints, &res);
    if (e) { fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(e)); return -1; }
    int s = -1;
    for (rp = res; rp; rp = rp->ai_next) {
        s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (s < 0) continue;
        if (connect(s, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(s); s = -1;
    }
    freeaddrinfo(res);
    return s;
}

static int send_json(const char *fmt, ...) {
    char buf[4096];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
    pthread_mutex_lock(&g_sock_lock);
    size_t len = strlen(buf);
    int ok = send(g_sock, buf, len, 0) == (ssize_t)len;
    pthread_mutex_unlock(&g_sock_lock);
    return ok ? 0 : -1;
}

static int recv_line(int s, char *buf, size_t max) {
    size_t n = 0;
    while (n + 1 < max) {
        char c;
        ssize_t r = recv(s, &c, 1, 0);
        if (r <= 0) return -1;
        if (c == '\n') break;
        if (c != '\r') buf[n++] = c;
    }
    buf[n] = 0;
    return (int)n;
}

static void handle_subscribe(json_t *root) {
    json_t *id = json_object_get(root, "id");
    if (!json_is_integer(id) || json_integer_value(id) != 1) return;
    json_t *result = json_object_get(root, "result");
    if (!json_is_array(result) || json_array_size(result) < 3) return;
    const char *ex1 = json_string_value(json_array_get(result, 1));
    json_t *ex2sz = json_array_get(result, 2);
    if (ex1) snprintf(g_extranonce1, sizeof(g_extranonce1), "%s", ex1);
    if (json_is_integer(ex2sz)) g_extranonce2_size = (int)json_integer_value(ex2sz);
    printf("Subscribed. extranonce1=%s extranonce2_size=%d\n", g_extranonce1, g_extranonce2_size);
}

static void handle_set_diff(json_t *params) {
    if (!json_is_array(params) || json_array_size(params) < 1) return;
    g_diff = json_number_value(json_array_get(params, 0));
    update_prefix_from_diff(g_diff);
    printf("Pool difficulty %.8f, local submit filter ~%u leading zero bits\n", g_diff, g_target_prefix_zero_bits);
}

static void handle_notify(json_t *params) {
    if (!json_is_array(params) || json_array_size(params) < 9) return;
    job_t j = {0};
    const char *s;
    s = json_string_value(json_array_get(params,0)); if (s) snprintf(j.job_id,sizeof(j.job_id),"%s",s);
    s = json_string_value(json_array_get(params,1)); if (s) snprintf(j.prevhash,sizeof(j.prevhash),"%s",s);
    s = json_string_value(json_array_get(params,2)); if (s) snprintf(j.coinb1,sizeof(j.coinb1),"%s",s);
    s = json_string_value(json_array_get(params,3)); if (s) snprintf(j.coinb2,sizeof(j.coinb2),"%s",s);
    json_t *br = json_array_get(params,4);
    if (json_is_array(br)) {
        size_t n = json_array_size(br); if (n > MAX_BRANCHES) n = MAX_BRANCHES;
        for (size_t i=0;i<n;i++) { s = json_string_value(json_array_get(br,i)); if (s) snprintf(j.branches[i], sizeof(j.branches[i]), "%s", s); }
        j.branch_count = (int)n;
    }
    s = json_string_value(json_array_get(params,5)); if (s) snprintf(j.version,sizeof(j.version),"%s",s);
    s = json_string_value(json_array_get(params,6)); if (s) snprintf(j.nbits,sizeof(j.nbits),"%s",s);
    s = json_string_value(json_array_get(params,7)); if (s) snprintf(j.ntime,sizeof(j.ntime),"%s",s);
    j.clean = json_is_true(json_array_get(params,8));
    pthread_mutex_lock(&g_job_lock);
    j.job_seq = g_job.job_seq + 1;
    g_job = j;
    pthread_mutex_unlock(&g_job_lock);
    printf("New job %s ntime=%s branches=%d clean=%d\n", j.job_id, j.ntime, j.branch_count, j.clean);
}

static bool make_merkle(const job_t *j, uint32_t ex2, uint8_t merkle[32], char ex2hex[32]) {
    snprintf(ex2hex, 32, "%0*x", g_extranonce2_size * 2, ex2);
    char *cbhex = NULL;
    if (asprintf(&cbhex, "%s%s%s%s", j->coinb1, g_extranonce1, ex2hex, j->coinb2) < 0) return false;
    size_t cblen = strlen(cbhex)/2;
    uint8_t *cb = malloc(cblen);
    if (!cb) { free(cbhex); return false; }
    if (!hex2bin(cbhex, cb, cblen)) { free(cbhex); free(cb); return false; }
    sha256d(cb, cblen, merkle);
    free(cbhex); free(cb);
    for (int i=0; i<j->branch_count; i++) {
        uint8_t branch[32], cat[64];
        if (!hex2bin(j->branches[i], branch, sizeof(branch))) return false;
        memcpy(cat, merkle, 32);
        memcpy(cat+32, branch, 32);
        sha256d(cat, 64, merkle);
    }
    return true;
}

static bool build_header(const job_t *j, const uint8_t merkle[32], uint32_t nonce, uint8_t header[80]) {
    uint8_t prev[32];
    if (!hex2bin(j->prevhash, prev, sizeof(prev))) return false;
    // Standard stratum usually supplies prevhash in internal byte order. If pool rejects all shares, try rev32(prev).
    put_le32(header+0, hex_u32(j->version));
    memcpy(header+4, prev, 32);
    memcpy(header+36, merkle, 32);
    put_le32(header+68, hex_u32(j->ntime));
    put_le32(header+72, hex_u32(j->nbits));
    put_le32(header+76, nonce);
    return true;
}

static void submit_share(const job_t *j, const char *ex2hex, uint32_t nonce) {
    char noncehex[16];
    snprintf(noncehex, sizeof(noncehex), "%08x", nonce);
    send_json("{\"id\":4,\"method\":\"mining.submit\",\"params\":[\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"]}\n",
              g_cfg.user, j->job_id, ex2hex, j->ntime, noncehex);
}

static void *miner_thread(void *arg) {
    uintptr_t tid = (uintptr_t)arg;
    uint32_t ex2 = (uint32_t)tid;
    uint32_t nonce = (uint32_t)(0x10000000u * tid);
    uint32_t seen_seq = 0;
    job_t local = {0};
    uint8_t merkle[32]; char ex2hex[32] = {0};
    while (!g_stop) {
        pthread_mutex_lock(&g_job_lock);
        if (g_job.job_seq != seen_seq) { local = g_job; seen_seq = g_job.job_seq; ex2 += g_cfg.threads; make_merkle(&local, ex2, merkle, ex2hex); }
        pthread_mutex_unlock(&g_job_lock);
        if (!seen_seq) { usleep(100000); continue; }
        for (int i=0; i<10000 && !g_stop; i++) {
            uint8_t header[80], h[32];
            if (!build_header(&local, merkle, nonce, header)) break;
            sha3_256t(header, 80, h);
            __sync_fetch_and_add(&g_hashes, 1);
            if (hash_meets_share(h)) {
                char hx[65]; bin2hex(h, 32, hx);
                printf("Thread %zu found candidate nonce=%08x hash=%s\n", (size_t)tid, nonce, hx);
                submit_share(&local, ex2hex, nonce);
            }
            nonce += (uint32_t)g_cfg.threads;
            if (nonce < g_cfg.threads) { ex2 += g_cfg.threads; make_merkle(&local, ex2, merkle, ex2hex); }
        }
    }
    return NULL;
}

static void usage(const char *p) {
    fprintf(stderr, "BobMiner ARM BC3 v0.1\nUsage: %s -o stratum+tcp://host:port -u wallet.worker -p x [-t threads]\n", p);
}

int main(int argc, char **argv) {
    memset(&g_cfg, 0, sizeof(g_cfg)); g_cfg.threads = 1; snprintf(g_cfg.pass,sizeof(g_cfg.pass),"x");
    int opt;
    while ((opt = getopt(argc, argv, "o:u:p:t:h")) != -1) {
        switch (opt) {
            case 'o': snprintf(g_cfg.url,sizeof(g_cfg.url),"%s",optarg); break;
            case 'u': snprintf(g_cfg.user,sizeof(g_cfg.user),"%s",optarg); break;
            case 'p': snprintf(g_cfg.pass,sizeof(g_cfg.pass),"%s",optarg); break;
            case 't': g_cfg.threads = atoi(optarg); if (g_cfg.threads<1) g_cfg.threads=1; if (g_cfg.threads>MAX_THREADS) g_cfg.threads=MAX_THREADS; break;
            default: usage(argv[0]); return 1;
        }
    }
    if (!g_cfg.url[0] || !g_cfg.user[0] || parse_url(g_cfg.url, g_cfg.host, sizeof(g_cfg.host), g_cfg.port, sizeof(g_cfg.port))) { usage(argv[0]); return 1; }
    signal(SIGINT, on_sig); signal(SIGTERM, on_sig);
    printf("BobMiner ARM BC3 v0.1 connecting to %s:%s with %d threads\n", g_cfg.host, g_cfg.port, g_cfg.threads);
    g_sock = connect_pool(); if (g_sock < 0) die("connect");
    send_json("{\"id\":1,\"method\":\"mining.subscribe\",\"params\":[\"BobMinerARM/0.1\"]}\n");
    send_json("{\"id\":2,\"method\":\"mining.authorize\",\"params\":[\"%s\",\"%s\"]}\n", g_cfg.user, g_cfg.pass);
    pthread_t th[MAX_THREADS];
    for (int i=0;i<g_cfg.threads;i++) pthread_create(&th[i], NULL, miner_thread, (void*)(uintptr_t)i);
    time_t last = time(NULL); uint64_t last_hashes = 0;
    char line[MAX_LINE];
    while (!g_stop && recv_line(g_sock, line, sizeof(line)) > 0) {
        json_error_t err; json_t *root = json_loads(line, 0, &err);
        if (!root) { fprintf(stderr, "JSON parse error: %s\n", err.text); continue; }
        const char *method = json_string_value(json_object_get(root, "method"));
        if (method && strcmp(method, "mining.set_difficulty") == 0) handle_set_diff(json_object_get(root, "params"));
        else if (method && strcmp(method, "mining.notify") == 0) handle_notify(json_object_get(root, "params"));
        else handle_subscribe(root);
        json_t *id = json_object_get(root, "id");
        if (json_is_integer(id) && json_integer_value(id) == 4) printf("Submit response: %s\n", line);
        json_decref(root);
        time_t now = time(NULL);
        if (now - last >= 10) {
            uint64_t h = g_hashes;
            printf("Speed: %.2f H/s total=%llu\n", (double)(h-last_hashes)/(double)(now-last), (unsigned long long)h);
            last = now; last_hashes = h;
        }
    }
    g_stop = 1;
    for (int i=0;i<g_cfg.threads;i++) pthread_join(th[i], NULL);
    return 0;
}
