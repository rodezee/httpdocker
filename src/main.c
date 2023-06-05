
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include "lib/mongoose/mongoose.h"
#include "lib/libdocker/inc/docker.h"

// BEGIN extra C functions

bool starts_with(const char *pre, const char *str)
{
    size_t lenpre = strlen(pre),
           lenstr = strlen(str);
    return lenstr < lenpre ? false : memcmp(pre, str, lenpre) == 0;
}

char * str_slice(char str[], int slice_from, int slice_to)
{
    // if a string is empty, returns nothing
    if (str[0] == '\0')
        return NULL;

    char *buffer;
    size_t str_len, buffer_len;

    // for negative indexes "slice_from" must be less "slice_to"
    if (slice_to < 0 && slice_from < slice_to) {
        str_len = strlen(str);

        // if "slice_to" goes beyond permissible limits
        if (abs(slice_to) > str_len - 1)
            return NULL;

        // if "slice_from" goes beyond permissible limits
        if (abs(slice_from) > str_len)
            slice_from = (-1) * str_len;

        buffer_len = slice_to - slice_from;
        str += (str_len + slice_from);

    // for positive indexes "slice_from" must be more "slice_to"
    } else if (slice_from >= 0 && slice_to > slice_from) {
        str_len = strlen(str);

        // if "slice_from" goes beyond permissible limits
        if ( (size_t) slice_from > str_len - 1)
            return NULL;

        buffer_len = slice_to - slice_from;
        str += slice_from;

    // otherwise, returns NULL
    } else
        return NULL;

    buffer = calloc(buffer_len, sizeof(char));
    strncpy(buffer, str, buffer_len);
    return buffer;
}

// END extra C functions

// BEGIN DOCKER

typedef struct messageResult {
  char *message;
  char *result;
} messageResult;

const char * do_docker_pull(DOCKER *docker, const char *image) {
  if( strchr(image, '/') == NULL ) {
    fprintf(stderr, "\"%s\" is a wrong image name, give a real image name before pulling", image);
    return "ERROR: Wrong image name";
  }
  char cmd_url_pull[255];
  const char *pull_str_begin = "http://v1.43/images/create?fromImage=";
  strcpy(cmd_url_pull, pull_str_begin);
  strcat(cmd_url_pull, image);
  fprintf(stderr, "cmd_url_pull: %s\n", cmd_url_pull);
  CURLcode responsePull;
  responsePull = docker_post(docker, cmd_url_pull, "");
  if (responsePull == CURLE_OK) {
    char *dbuf = docker_buffer(docker);
    fprintf(stderr, "PULL dbuf: %s\n", dbuf);
    fprintf(stderr, "Image pulled, refresh please, CURL response code: %d\n", (int) responsePull);
    return "SUCCESS: Image pulled, refresh please";
  } else {
    fprintf(stderr, "Unable to pull image, CURL response code: %d\n", (int) responsePull);
    return "ERROR: Unable to pull image";
  }
}

const char * do_docker_create(DOCKER *docker, const char *image) {
  // CREATE docker_post(docker, "http://v1.25/containers/create", "{\"Image\": \"rodezee/hello-world:0.0.1\", \"Cmd\": [\"echo\", \"hello world\"]}");
  char cmd_url_create[255];
  const char *create_str_begin = "{\"Image\": \"";
  const char *create_str_end = "\"}";
  strcpy(cmd_url_create, create_str_begin);
  strcat(cmd_url_create, image);
  strcat(cmd_url_create, create_str_end);
  fprintf(stderr, "cmd_url_create: %s\n", cmd_url_create);
  CURLcode responseCreate;
  // responseCreate = docker_post(docker, "http://v1.25/containers/create", "{\"Image\": \"rodezee/hello-world:0.0.1\"}");
  responseCreate = docker_post(docker, "http://v1.25/containers/create", cmd_url_create);
  if ( responseCreate == CURLE_OK ) {
    fprintf(stderr, "Try to create container, CURL response code: %d\n", (int) responseCreate);
    char *dbuf = docker_buffer(docker);
    fprintf(stderr, "dbuf: %s\n", dbuf);
    if ( starts_with("{\"message\":\"No such image: ", dbuf) ) { // image needs to be pulled
      fprintf(stderr, "Image needs to be pulled, dbuf: %s\n", dbuf);
      const char *dpull = do_docker_pull(docker, image);
      if( starts_with("SUCCESS:", dpull) ) {
        return do_docker_create(docker, image);
      } else {
        fprintf(stderr, "ERROR: during pull of image: %s", dpull);
        return "ERROR: during pull";
      }
    } else if ( starts_with("{\"message\":", dbuf) ) { // for all errors of container creation
      fprintf(stderr, "ERROR during creation of container dbuf: %s", dbuf);
      return "ERROR: message during creation of container";
    }
    return str_slice( dbuf, 7, (7+64) ); // RETURN the id of the new container
  } else {
    fprintf(stderr, "docker connection error: %d\n", (int) responseCreate);
    return "ERROR: docker connection";
  }
}

const char * do_docker_start(DOCKER *docker, const char *id) {
  // START v1.43/containers/1c6594faf5/start
  CURLcode responseStart;
  char cmd_url_start[255];
  const char *start_cp1 = "http://v1.43/containers/";
  const char *start_cp2 = "/start";
  strcpy(cmd_url_start, start_cp1);
  strcat(cmd_url_start, id);
  strcat(cmd_url_start, start_cp2);
  fprintf(stderr, "Start cmd_url_start: %s\n", cmd_url_start);
  responseStart = docker_post(docker, cmd_url_start, "");
  if (responseStart == CURLE_OK) {
    char *dbuf = docker_buffer(docker);
    fprintf(stderr, "Container Started dbuf: %s\n", dbuf);
    fprintf(stderr, "Container Started id: %s\n", id);
    fprintf(stderr, "CURL response code: %d\n", (int) responseStart);
    return "SUCCESS: started container";
  } else {
    return "ERROR: during starting request of container";
  }
}

const char * do_docker_wait(DOCKER *docker, const char *id) {
  // WAIT v1.43/containers/1c6594faf5/wait
  CURLcode responseWait;
  char cmd_url_wait[255];
  const char *wait_cp1 = "http://v1.43/containers/";
  const char *wait_cp2 = "/wait";
  strcpy(cmd_url_wait, wait_cp1);
  strcat(cmd_url_wait, id);
  strcat(cmd_url_wait, wait_cp2);
  fprintf(stderr, "wait cmd_url_wait: %s\n", cmd_url_wait);
  responseWait = docker_post(docker, cmd_url_wait, "");
  if (responseWait == CURLE_OK) {
    char *dbuf = docker_buffer(docker);
    if ( strcmp(dbuf, "{\"StatusCode\":0}\n") == 0 ) {
      fprintf(stderr, "Container Waited Successfully, dbuf: %s\n", dbuf);
      return "SUCCESS: waited on container";
    } else {
      return "ERROR: wrong return status on waiting request";
    }
  } else {
    return "ERROR: during waiting request on container";
  }
}

messageResult get_docker_result(DOCKER *docker, const char *id) {
  CURLcode responseResponse;
  char cmd_url_response[255];
  const char *response_cp1 = "http://v1.43/containers/"; // http://v1.43/containers/
  const char *response_cp2 = "/logs?stdout=1"; // ?stdout=true&timestamps=true&tail=1
  strcpy(cmd_url_response, response_cp1);
  strcat(cmd_url_response, id);
  strcat(cmd_url_response, response_cp2);
  fprintf(stderr, "response cmd_url_response: %s\n", cmd_url_response);
  responseResponse = docker_get(docker, cmd_url_response);
  if (responseResponse == CURLE_OK) {
    // char *dbuf = docker_buffer(docker);
    char dbuf[] = "";
    // fprintf(stderr, "Container Response Successfully, dbuf size: %lu\n", docker->buffer->size);
    for ( size_t i=8; i < docker->buffer->size; i++ ) {
      fprintf(stderr, "docker->buffer->data[i] d: %d\n", (int)docker->buffer->data[i]);
      // fprintf(stderr, "docker->buffer->data[i] c: %c\n", (char)docker->buffer->data[i]);
      strncat(dbuf, &docker->buffer->data[i], 1);
    }
    fprintf(stderr, "Container Response Successfully, dbuf: %s\n", dbuf);
    return (messageResult) { "SUCCESS: read result of container", dbuf };
  } else {
    fprintf(stderr, "Unable to get response from container, CURL response code: %d\n", (int) responseResponse);
    return (messageResult) { "ERROR: unable to get response from container", "" };
  }
}

// END DOCKER

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
      
        char *image = "rodezee/hello-world:0.0.1";

        // INIT
        DOCKER *docker = docker_init("v1.43"); // v1.25
        if ( docker ) {

          fprintf(stderr, "SUCCESS: initialized docker\n");

          // CREATE
          const char * id = do_docker_create(docker, image);
          if ( starts_with("ERROR:", id) ) {
            fprintf(stderr, "%s\n", id);
            mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{%m:\"%s\"}", mg_print_esc, 0, "Container Creation error: ", id);
          } else {
            
            fprintf(stderr, "SUCCESS: image found and container created id: %s\n", id);

            // START
            const char *dstart = do_docker_start(docker, id);
            if ( starts_with("ERROR:", dstart) ) {
              fprintf(stderr, "%s\n", dstart);
              mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{%m:\"%s\"}", mg_print_esc, 0, "Container Starting error: ", id);
            } else {
              
              fprintf(stderr, "SUCCESS: started container with id: %s\n", id);

              // WAIT
              const char *dwait = do_docker_wait(docker, id);
              if ( starts_with("ERROR:", dwait) ) {
                fprintf(stderr, "%s\n", dwait);
                mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{%m:\"%s\"}", mg_print_esc, 0, "Container Waiting error: ", id);
              } else {

                fprintf(stderr, "SUCCESS: waited container with id: %s\n", id);

                // RESPONSE
                messageResult mr = get_docker_result(docker, id);
                if ( starts_with("ERROR:", mr.message) ) {
                  fprintf(stderr, "%s\n", mr.message);
                  mg_http_reply(c, 200, "Content-Type: text/plain; charset=utf-8\r\n", "%m", mg_print_esc, 0, mr.message);
                } else {
                  char* res = (char*)malloc((strlen(mr.result)+1) * sizeof(char));
                  strcpy(res, mr.result);
                  fprintf(stderr, "%s, %s\n", mr.message, res);
                  mg_http_reply(c, 200, "Content-Type: text/plain; charset=utf-8\r\n", "%m", mg_print_esc, 0, res);
                  // mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{\"result\":%m}", mg_print_esc, 0, mr.result);
                }
              }
            }
          }

          docker_destroy(docker);

        } else {

          fprintf(stderr, "ERROR: Failed to initialize to docker!\n");
          mg_http_reply(c, 500, NULL, "ERROR: Failed to initialize to docker!\n");

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


