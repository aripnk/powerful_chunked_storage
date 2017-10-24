#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/http.h>
#include <event2/keyvalq_struct.h>
#include "../libs/leveldb_api/db.h"

//FOR METADATA CONTENT
struct meta_t
{
  uint32_t filesize;
  uint32_t nchunks;
  uint32_t filename_len;
	uint32_t type_len;
};
typedef struct meta_t meta_t;

struct chunk_req_state_t {
  struct event_base *base;
  struct evhttp_request *req;
  struct event *timer;
  int i;
  int total;
  char key[1024];
  db_t db;
};
typedef struct chunk_req_state_t chunk_req_state_t;

struct websvc_t{
  struct event_base *base;
  db_t db;
};
typedef struct websvc_t websvc_t;

static struct evhttp *
http_setup(const char *address, short port, /*struct event_base *base*/websvc_t* websvc);

static void
http_chunked_cb(struct evhttp_request *req, void *arg);

static void
http_chunked_trickle_cb(evutil_socket_t fd, short events, void *arg);

static void
schedule_trickle(chunk_req_state_t *state, int ms);

static void
add_custom_download_header(struct evhttp_request *req, char* content_type, char* filename, char* filesize);

static void
http_upload_cb (struct evhttp_request *req, void *arg);

static void
ignore_sigpipe(void);

static void
sigint_cb(evutil_socket_t sig, short events, void *ptr);

static void
meta_set_value(meta_t *meta, char* valuemeta,
               const char *filename, const char *type);

static void
add_default_upload_header(struct evhttp_request *req);

static void
meta_set_header(meta_t * meta, int filesize, int nchunks,
               int filename_len, int type_len);

bool
isthisfirst(const char* key);

long long current_timestamp();

static void
websvc_init(websvc_t *websvc, void* base);
