#include "chunkedstorage.h"

int
main(int argc, char* argv[])
{
  ignore_sigpipe();

  struct event_base *base = event_base_new();
  websvc_t websvc;
  websvc_init(&websvc, base);

  struct event *sigint_event = evsignal_new(base, SIGINT, sigint_cb, base);
  if (!sigint_event || event_add(sigint_event, NULL) < 0) {
      fprintf(stderr, "Could not create or add the SIGINT signal event.\n");
      return -1;
  }

  struct evhttp *http = http_setup("0.0.0.0", atoi(argv[1]), &websvc);

  event_base_dispatch(base);

  event_free(sigint_event);
  evhttp_free(http);
  event_base_free(base);

  return 0;
}

static void
websvc_init(websvc_t *websvc, void* base)
{
  if (websvc == NULL) return;
  websvc->base = base;
  db_t db;
  websvc->db = db;
  db_init(&websvc->db);
}

long long current_timestamp()
{
    struct timeval te;
    gettimeofday(&te, NULL); // get current time
    long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // caculate milliseconds
    return milliseconds;
}

static struct evhttp *
http_setup(const char *address, short port, /*struct event_base *base*/websvc_t* websvc)
{
  struct evhttp *http = evhttp_new(websvc->base);

  evhttp_bind_socket(http, address, port);

  evhttp_set_cb(http, "/download", http_chunked_cb, websvc);
  evhttp_set_cb(http, "/upload", http_upload_cb, websvc);

  return http;
}

static void
sigint_cb(evutil_socket_t sig, short events, void *ptr)
{
  struct event_base *base = ptr;
  struct timeval delay = { 1, 0 };
  printf("Caught an interrupt signal; exiting cleanly in two seconds.\n");
  event_base_loopexit(base, &delay);
}

static void
ignore_sigpipe(void)
{
  struct sigaction sa;

  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_IGN;

  if (sigemptyset(&sa.sa_mask) < 0 || sigaction(SIGPIPE, &sa, 0) < 0) {
      perror("Could not ignore the SIGPIPE signal");
      exit(EXIT_FAILURE);
  }
}

bool
isthisfirst(const char* key)
{
  if (strncmp(key, "0", 1) == 0) return true;
  return false;
}

static void
meta_set_header(meta_t * meta, int filesize, int nchunks,
                int filename_len, int type_len)
{
  if (meta == NULL) return;
  meta->filesize = filesize;
  meta->nchunks = nchunks;
  meta->filename_len = filename_len;
  meta->type_len = type_len;
}

static void
add_default_upload_header(struct evhttp_request *req)
{
  evhttp_add_header (evhttp_request_get_output_headers (req), "Content-Type", "application/json;");
  evhttp_add_header (evhttp_request_get_output_headers (req), "Access-Control-Allow-Origin", "*");
}

static void
meta_set_value(meta_t *meta, char* valuemeta,
               const char *filename, const char *type)
{
  memcpy(valuemeta, meta, sizeof(meta_t));

  memcpy(valuemeta+sizeof(meta_t),
        filename,
        meta->filename_len);

  memcpy(valuemeta + sizeof(meta_t) + meta->filename_len,
        type,
        meta->type_len);
}


static void
add_custom_download_header(struct evhttp_request *req, char* content_type, char* filename, char* filesize)
{
  char name[512];
  memset(name,0,512);
  sprintf(name,"filename=%s", filename);
  evhttp_add_header (evhttp_request_get_output_headers (req), "Pragma", "public;");
  evhttp_add_header (evhttp_request_get_output_headers (req), "Cache-Control", "must-revalidate, post-check=0, pre-check=0;");
  evhttp_add_header (evhttp_request_get_output_headers (req), "Content-type", content_type);
  evhttp_add_header (evhttp_request_get_output_headers (req), "Content-Disposition", name);
  evhttp_add_header (evhttp_request_get_output_headers (req), "Content-Length", filesize);
  evhttp_add_header (evhttp_request_get_output_headers (req), "Content-Transfer-Encoding", "chunked;");
}

static void
schedule_trickle(chunk_req_state_t *state, int ms)
{
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = ms * 1000;
  evtimer_add(state->timer, &tv);
}

static void
http_chunked_trickle_cb(evutil_socket_t fd, short events, void *arg)
{
  chunk_req_state_t *state = arg;
  struct evbuffer *evb = evbuffer_new();

  char key[1024];
  sprintf(key, "%s.%d",state->key, state->i);

  char* data;
  int ret = db_read(&state->db, key, strlen(key), &data);


  if (ret != DB_SUCCESS){
    evhttp_send_reply_end(state->req);
    event_free(state->timer);
    free(state);
		return;
  }

  evbuffer_add(evb, data, state->db.vallen);

  evhttp_send_reply_chunk(state->req, evb);
  evbuffer_free(evb);

  if (++state->i < state->total) {
    schedule_trickle(state, 100);
  } else {
    evhttp_send_reply_end(state->req);
    // XXX TODO why no evtimer_free?
    event_free(state->timer);
    free(state);
  }
}


//*CALLBACK FUNCTION*//

static void
http_upload_cb (struct evhttp_request *req, void *arg)
{
  websvc_t* websvc = (websvc_t*)arg;
  struct evbuffer *evb = NULL;
	const char *uri = evhttp_request_get_uri (req);
	struct evhttp_uri *decoded = NULL;

  char answer[256];
  char keymeta[256];
  char valuemeta[1024];

  /* Decode URL */
	decoded = evhttp_uri_parse (uri);
	if (! decoded){
		evhttp_send_error (req, HTTP_BADREQUEST, 0);
		return;
	}

  struct evkeyvalq kv;
	memset (&kv, 0, sizeof (kv));
	struct evbuffer *buf = evhttp_request_get_input_buffer (req);

	char* query = NULL;
	query = malloc(strlen(uri)+1);
	query[strlen(uri)] = 0;
	memcpy(query, uri+8, strlen(uri));

	if (0 != evhttp_parse_query_str (query, &kv)){
		evhttp_send_error (req, HTTP_BADREQUEST, 0);
    evbuffer_drain(buf, evbuffer_get_length(buf));
    free (query);
		return;
	}

	const char* key = evhttp_find_header (&kv, "key");
	const char* filename = evhttp_find_header (&kv, "filename");
  const char* filesize = evhttp_find_header (&kv, "size");
  const char* type = evhttp_find_header (&kv, "type");
  const char* nchunks = evhttp_find_header (&kv, "nchunks");

  if (key == NULL) {
    evhttp_send_error (req, HTTP_BADREQUEST, 0);
    if (decoded) evhttp_uri_free (decoded);
		if (evb) evbuffer_free (evb);
    evbuffer_drain(buf, evbuffer_get_length(buf));
		free(query);
		return;
	}

  // FIRST QUERY/DATA
  if (isthisfirst(key)){
    // FOR METADATA
    long long now = current_timestamp();
    sprintf(keymeta, "%lld.%s.meta", now, filename);
    meta_t meta;
    meta_set_header(&meta, atoi(filesize), atoi(nchunks), strlen(filename), strlen(type));
    meta_set_value(&meta, valuemeta, filename, type);
    int ret = db_write(&websvc->db, keymeta, strlen(keymeta), valuemeta, sizeof(valuemeta));

    // FOR FIRST CHUNK
    size_t len = evbuffer_get_length(buf);
    char* data = malloc(len + 1);
    data[len] = 0;
    char firstkey[256];
    sprintf(firstkey,"%lld.%s.0", now, filename);

    int keylen = strlen(firstkey);
    evbuffer_copyout(buf, data, len);

    ret = db_write(&websvc->db, firstkey, keylen, data, len);
    sprintf(answer, "{\"key\":\"%lld.%s\",\"status\":%d}", now, filename, ret);

    evhttp_clear_headers (&kv);
    evb = evbuffer_new ();
    add_default_upload_header(req);
  	evbuffer_add (evb, answer, strlen (answer));
  	evhttp_send_reply (req, 200, "OK", evb);

    // DONT FORGET TO FREE ALLOCATED MEM
  	if (decoded) evhttp_uri_free (decoded);
  	if (evb) evbuffer_free (evb);
    evbuffer_drain(buf, evbuffer_get_length(buf));
  	free(data);
  	free(query);
    return;
  }

  // ALL CHUNK
  size_t len = evbuffer_get_length(buf);
  char* data = malloc(len + 1);
  data[len] = 0;
  int keylen = strlen(key);
  evbuffer_copyout(buf, data, len);
  int ret = db_write(&websvc->db, key, keylen, data, len);

  sprintf(answer,"{\"key\":\"%s\",\"status\":%d}",key,ret);
  evhttp_clear_headers (&kv);
  evb = evbuffer_new ();
  add_default_upload_header(req);
  evbuffer_add (evb, answer, strlen (answer));
  evhttp_send_reply (req, 200, "OK", evb);

  // DONT FORGET TO FREE ALLOCATED MEM
  if (decoded) evhttp_uri_free (decoded);
  if (evb) evbuffer_free (evb);
  evbuffer_drain(buf, evbuffer_get_length(buf));
  free(data);
  free(query);
  return;
}


static void
http_chunked_cb(struct evhttp_request *req, void *arg)
{
  struct evbuffer *evb = NULL;
	const char *uri = evhttp_request_get_uri (req);
	struct evhttp_uri *decoded = NULL;
	char answer[256];

  /* Decode payload */
	struct evkeyvalq kv;
	memset (&kv, 0, sizeof (kv));

	struct evbuffer *buf = evhttp_request_get_input_buffer (req);
	evbuffer_add (buf, "", 1);    /* NUL-terminate the buffer */
	char *payload = (char *) evbuffer_pullup (buf, -1);
  if (0 != evhttp_parse_query_str (payload, &kv)) {
		printf ("Malformed payload. Sending BADREQUEST\n");
		evhttp_send_error (req, HTTP_BADREQUEST, 0);
		return;
	}

  const char* key;
  char* keyg;
  char datakey[256];

  if (evhttp_request_get_command (req) != EVHTTP_REQ_POST) {
		strtok_r((char*)uri, "?", &keyg);
    strcpy(datakey, keyg);
	}else{
    key = evhttp_find_header (&kv, "key");
    strcpy(datakey, key);
  }

  if (key == NULL && keyg == NULL) {
    evhttp_send_error (req, HTTP_BADREQUEST, 0);
		return;
  }

  chunk_req_state_t *state = malloc(sizeof(chunk_req_state_t));
  memset(state, 0, sizeof(chunk_req_state_t));
  websvc_t *websvc = (websvc_t*)arg;

  //GET METADATA TO SET THE TOTAL CHUNK
  char keymeta[256];
  sprintf(keymeta, "%s.meta", datakey);

  char content_type[256];
  char filename[256];
  char filesize[256];

  char* metaval;
  int ret = db_read(&websvc->db, keymeta, strlen(keymeta), &metaval);

  if (ret != DB_SUCCESS) {
    evhttp_send_error (req, HTTP_NOTFOUND, 0);
    if (decoded) evhttp_uri_free (decoded);
    if (evb) evbuffer_free (evb);
    return;
  }

  meta_t meta_header;
  memcpy(&meta_header, metaval, sizeof(meta_t));

  memset(filename,0,256);
  memset(content_type,0,256);

  sprintf(filesize, "%u", meta_header.filesize);

  memcpy(&filename, metaval + sizeof(meta_t), meta_header.filename_len);

  memcpy(&content_type, metaval + sizeof(meta_t) + meta_header.filename_len,
         meta_header.type_len);

  state->req = req;
  state->total = meta_header.nchunks;
  state->base = websvc->base;
  state->db = websvc->db;
  strcpy(state->key,datakey);
  state->timer = evtimer_new(state->base, http_chunked_trickle_cb, state);

  evhttp_clear_headers (&kv);
  add_custom_download_header(req, content_type, filename, filesize);
  evhttp_send_reply_start(req, HTTP_OK, "OK BRO!!");
  schedule_trickle(state, 0);

  if (decoded) evhttp_uri_free (decoded);
  if (evb) evbuffer_free (evb);
}
