
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include "lib/mongoose/mongoose.h"
#include "lib/libdocker/inc/docker.h"

// BEGIN extra C functions

bool startsWith(const char *pre, const char *str)
{
    size_t lenpre = strlen(pre),
           lenstr = strlen(str);
    return lenstr < lenpre ? false : memcmp(pre, str, lenpre) == 0;
}

// END extra C functions

static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
  if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;
    if (mg_http_match_uri(hm, "/")) { // with input
      // Expecting JSON array in the HTTP body, e.g. [ 123.38, -2.72 ]
      double num1, num2;
      if (mg_json_get_num(hm->body, "$[0]", &num1) &&
          mg_json_get_num(hm->body, "$[1]", &num2)) {
        // Success! create JSON response
        mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                      "{%m:%g}\n",
                      mg_print_esc, 0, "result", num1 + num2);
      } else { // with no input
        //mg_http_reply(c, 500, NULL, "Do docker standard stuff\n");
      
        DOCKER *docker = docker_init("v1.25");
        CURLcode responseCreate;

        if (docker) {

          fprintf(stderr, "Successfully initialized to docker\n");

          // CURLcode responseImageCreate = docker_post(docker, "http://v1.43/images/create", "{\"fromImage\": \"alpine\"}");
          responseCreate = docker_post(docker, "http://v1.25/containers/create", "{\"Image\": \"alpine:3.14\", \"Cmd\": [\"echo\", \"hello world\"]}");
          if ( responseCreate == CURLE_OK )
          {
            fprintf(stderr, "Try to create container, CURL response code: %d\n", (int) responseCreate);
            char *dbuf = docker_buffer(docker);
            fprintf(stderr, "dbuf: %s\n", dbuf);
            struct mg_str *dmessage_str = (struct mg_str *) mg_str(dbuf);
            char *dmessage;
            mg_json_get_str(dmessage_str, &dmessage);
            fprintf(stderr, "dmessage: %s\n", dmessage);

            if ( startsWith("No such image:", dbuf) == false ) { // image needs to be pulled
              mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                            "{%m:%s}\n",
                            mg_print_esc, 0, "result", dbuf);
              fprintf(stderr, "Image needs to be pulled, CURL response code: %d\n", (int) responseCreate);
              // responsePull = docker_post(docker, "http://v1.43/images/create?fromImage=alpine:3.14", "");
              // if (responsePull == CURLE_OK) {
              //   char *dbuf = docker_buffer(docker);
              //   mg_http_reply(c, 200, "Content-Type: application/json\r\n",
              //                 "{%m:%s}\n",
              //                 mg_print_esc, 0, "result", dbuf);
              //   fprintf(stderr, "No such image, CURL response code: %d\n", (int) responsePull);
              // } else {
              //   fprintf(stderr, "Unable to pull image, CURL response code: %d\n", (int) responsePull);
              // }
            } else { // image already on the server
              mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                            "{%m:%s}\n",
                            mg_print_esc, 0, "result", dbuf);
              fprintf(stderr, "Image was already there, CURL response code: %d\n", (int) responseCreate);
            }
          } else {
            mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                          "{%m:%g}\n",
                          mg_print_esc, 0, "Error in CURL", ( double ) responseCreate);
            fprintf(stderr, "responseCreate BAD, CURL error code: %d instead of: %d\n", ( int ) responseCreate, ( int ) CURLE_OK);
          }

          docker_destroy(docker);

        } else {

          mg_http_reply(c, 500, NULL, "ERROR: Failed to initialize to docker!\n");
          fprintf(stderr, "ERROR: Failed to initialize to docker!\n");

        }
      
      }
    } else {
      mg_http_reply(c, 500, NULL, "Emtpy response\n");
    }
  }
}

/*
static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
  if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;
    if (mg_http_match_uri(hm, "/api/sum")) {
      // Expecting JSON array in the HTTP body, e.g. [ 123.38, -2.72 ]
      double num1, num2;
      if (mg_json_get_num(hm->body, "$[0]", &num1) &&
          mg_json_get_num(hm->body, "$[1]", &num2)) {
        // Success! create JSON response
        mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                      "{%m:%g}\n",
                      mg_print_esc, 0, "result", num1 + num2);
      } else {
        mg_http_reply(c, 500, NULL, "Parameters missing\n");
      }
    } else {
      mg_http_reply(c, 500, NULL, "\n");
    }
  }
}

static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
  if (ev == MG_EV_HTTP_MSG) {
    mg_http_reply(c, 200, "Content-Type: text/plain\r\n", "Hello, %s\n", "world");
  }
}

int main(int argc, char *argv[]) {
  struct mg_mgr mgr;
  mg_mgr_init(&mgr);                                        // Init manager
  mg_http_listen(&mgr, "http://localhost:8000", fn, &mgr);  // Setup listener
  for (;;) mg_mgr_poll(&mgr, 1000);                         // Event loop
  mg_mgr_free(&mgr);                                        // Cleanup
  return 0;
}
*/

static int s_debug_level = MG_LL_INFO;
static const char *s_root_dir = ".";
static const char *s_listening_address = "http://0.0.0.0:8000";
static const char *s_enable_hexdump = "no";
static const char *s_ssi_pattern = "#.html";

// Handle interrupts, like Ctrl-C
static int s_signo;
static void signal_handler(int signo) {
  s_signo = signo;
}

// Event handler for the listening connection.
// Simply serve static files from `s_root_dir`
static void cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
  if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = ev_data, tmp = {0};
    struct mg_str unknown = mg_str_n("?", 1), *cl;
    struct mg_http_serve_opts opts = {0};
    opts.root_dir = s_root_dir;
    opts.ssi_pattern = s_ssi_pattern;
    mg_http_serve_dir(c, hm, &opts);
    mg_http_parse((char *) c->send.buf, c->send.len, &tmp);
    cl = mg_http_get_header(&tmp, "Content-Length");
    if (cl == NULL) cl = &unknown;
    MG_INFO(("%.*s %.*s %.*s %.*s", (int) hm->method.len, hm->method.ptr,
             (int) hm->uri.len, hm->uri.ptr, (int) tmp.uri.len, tmp.uri.ptr,
             (int) cl->len, cl->ptr));
  }
  (void) fn_data;
}

static void usage(const char *prog) {
  fprintf(stderr,
          "Mongoose v.%s\n"
          "Usage: %s OPTIONS\n"
          "  -H yes|no - enable traffic hexdump, default: '%s'\n"
          "  -S PAT    - SSI filename pattern, default: '%s'\n"
          "  -d DIR    - directory to serve, default: '%s'\n"
          "  -l ADDR   - listening address, default: '%s'\n"
          "  -v LEVEL  - debug level, from 0 to 4, default: %d\n",
          MG_VERSION, prog, s_enable_hexdump, s_ssi_pattern, s_root_dir,
          s_listening_address, s_debug_level);
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
  char path[MG_PATH_MAX] = ".";
  struct mg_mgr mgr;
  struct mg_connection *c;
  int i;

  // Parse command-line flags
  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-d") == 0) {
      s_root_dir = argv[++i];
    } else if (strcmp(argv[i], "-H") == 0) {
      s_enable_hexdump = argv[++i];
    } else if (strcmp(argv[i], "-S") == 0) {
      s_ssi_pattern = argv[++i];
    } else if (strcmp(argv[i], "-l") == 0) {
      s_listening_address = argv[++i];
    } else if (strcmp(argv[i], "-v") == 0) {
      s_debug_level = atoi(argv[++i]);
    } else {
      usage(argv[0]);
    }
  }

  // Root directory must not contain double dots. Make it absolute
  // Do the conversion only if the root dir spec does not contain overrides
  if (strchr(s_root_dir, ',') == NULL) {
    realpath(s_root_dir, path);
    s_root_dir = path;
  }

  // Initialise stuff
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  mg_log_set(s_debug_level);
  mg_mgr_init(&mgr);
  if ((c = mg_http_listen(&mgr, s_listening_address, fn, &mgr)) == NULL) {
    MG_ERROR(("Cannot listen on %s. Use http://ADDR:PORT or :PORT",
              s_listening_address));
    exit(EXIT_FAILURE);
  }
  if (mg_casecmp(s_enable_hexdump, "yes") == 0) c->is_hexdumping = 1;

  // Start infinite event loop
  MG_INFO(("Mongoose version : v%s", MG_VERSION));
  MG_INFO(("Listening on     : %s", s_listening_address));
  MG_INFO(("Web root         : [%s]", s_root_dir));
  MG_INFO(("debug level      : [%d]", s_debug_level));
  while (s_signo == 0) mg_mgr_poll(&mgr, 1000000);
  mg_mgr_free(&mgr);
  MG_INFO(("Exiting on signal %d", s_signo));
  return 0;
}


