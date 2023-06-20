
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

char * str_p_to_char_ar(const char *str) {
  char *res = (char*)malloc((strlen(str)+1) * sizeof(char));
  strcpy(res, "");
  strcpy(res, str);
  return res;
}

char * str_glue(const char *str1, const char *str2) {
  char *res = (char*)malloc((strlen(str1)+strlen(str2)+1) * sizeof(char));
  strcpy(res, "");
  strcpy(res, str1);
  strcat(res, str2);
  return res;
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

typedef struct responseResult {
  bool success;
  char *response;
} responseResult;

const char *do_docker_pull(DOCKER *docker, const char *image) {
  // PULL v1.43/images/create?fromImage=alpine
  if( strchr(image, '/') == NULL ) {
    fprintf(stderr, "\"%s\" is a wrong image name, give a real image name before pulling\n", image);
    return "ERROR: Wrong image name";
  }
  // char *cmd_url_pull = (char*)malloc((1024) * sizeof(char));
  // const char *pull_str_begin = "http://v1.43/images/create?fromImage=";
  // cmd_url_pull = str_glue(cmd_url_pull, pull_str_begin);
  // cmd_url_pull = str_glue(cmd_url_pull, image);
  char cmd_url_pull[1024];
  const char *pull_str_begin = "http://v1.43/images/create?fromImage=";
  strcpy(cmd_url_pull, pull_str_begin);
  strcat(cmd_url_pull, image);
  fprintf(stderr, "cmd_url_pull: %s\n", cmd_url_pull);
  CURLcode responsePull;
  responsePull = docker_post(docker, cmd_url_pull, "");
  if ( responsePull == CURLE_OK ) {
    char *dbuf = docker_buffer(docker);
    if ( starts_with("{\"message\":\"pull access denied", dbuf) ) {
      return "ERROR: Pull access denied";
    } else if ( starts_with("{\"message\":\"manifest for", dbuf) ) {
      fprintf(stderr, "ERROR: during pull %s", dbuf);
      // return "ERROR: message during pull";
      char *me = (char*)malloc((strlen(image)+28+1) * sizeof(char));
      sprintf(me, "ERROR: during pull of image %s", image);
      return me;
    } else {
      fprintf(stderr, "PULL dbuf: %s\n", dbuf);
      fprintf(stderr, "SUCCESS: Image pulled, CURL response code: %d\n", (int) responsePull);
      return "SUCCESS: Image pulled";
    }
  } else {
    fprintf(stderr, "ERROR: during request to pull image, CURL response code: %d\n", (int) responsePull);
    return "ERROR: Unable to pull image";
  }
}

const char *do_docker_create(DOCKER *docker, const char *body) {
  // CREATE docker_post(docker, "http://v1.25/containers/create", "{\"Image\": \"rodezee/hello-world:0.0.1\", \"Cmd\": [\"echo\", \"hello world\"]}");
  fprintf(stderr, "do_docker_create, body: %s\n", body);
  struct mg_str json = mg_str(body);
  char image[1024] = "";
  strcpy(image, mg_json_get_str(json, "$.Image"));
  CURLcode responseCreate;
  // responseCreate = docker_post(docker, "http://v1.25/containers/create", "{\"Image\": \"rodezee/hello-world:0.0.1\"}");
  responseCreate = docker_post(docker, "http://v1.25/containers/create", (char *)body);
  if ( responseCreate == CURLE_OK ) {
    fprintf(stderr, "Try to create container, CURL response code: %d\n", (int) responseCreate);
    char *dbuf = docker_buffer(docker);
    fprintf(stderr, "dbuf: %s\n", dbuf);
    if ( starts_with("{\"message\":\"No such image: ", dbuf) ) { // image needs to be pulled
      fprintf(stderr, "Image needs to be pulled, dbuf: %s\n", dbuf);
      const char *dpull = do_docker_pull(docker, image);
      if( starts_with("SUCCESS:", dpull) ) {  
        // AGAIN CREATE after pulling
        responseCreate = 0;
        responseCreate = docker_post(docker, "http://v1.25/containers/create", (char *)body);
        if ( responseCreate == CURLE_OK ) {
          fprintf(stderr, "Try to create container after pulling image, CURL response code: %d\n", (int) responseCreate);
          char *dbufAfterPull = docker_buffer(docker);
          fprintf(stderr, "after pulling dbuf: %s\n", dbufAfterPull);
          if ( starts_with("{\"message\":", dbufAfterPull) ) { // for all errors of container creation
            fprintf(stderr, "ERROR: during creation of container after pulling dbuf: %s\n", dbuf);
            return "ERROR: message during creation of container after pulling";
          } else { // SUCCESS container creation after pulling
            return str_slice( dbufAfterPull, 7, (7+64) ); // RETURN the id of the new container after pulling
          }
        } else {
          fprintf(stderr, "ERROR: docker connection error: %d\n", (int) responseCreate);
          return "ERROR: docker connection";
        }
      } else {
        // fprintf(stderr, "ERROR: during pull of image %s\n", image);
        char *pe = (char*)malloc((strlen(image)+28+1) * sizeof(char));
        sprintf(pe, "ERROR: during pull of image %s", image);
        return pe;
      }
    } else if ( starts_with("{\"message\":", dbuf) ) { // for all errors of container creation
      fprintf(stderr, "ERROR: during creation of container dbuf: %s\n", dbuf);
      return "ERROR: message during creation of container";
    } else { // SUCCESS container creation
      return str_slice( dbuf, 7, (7+64) ); // RETURN the id of the new container
    }
  } else {
    fprintf(stderr, "ERROR: docker connection error: %d\n", (int) responseCreate);
    return "ERROR: docker connection";
  }
}

const char *do_docker_start(DOCKER *docker, const char *id) {
  // START v1.43/containers/1c6594faf5/start
  CURLcode responseStart;
  char cmd_url_start[1024];
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

const char *do_docker_wait(DOCKER *docker, const char *id) {
  // WAIT v1.43/containers/1c6594faf5/wait
  CURLcode responseWait;
  char cmd_url_wait[1024];
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
  char cmd_url_response[1024];
  const char *response_cp1 = "http://v1.43/containers/"; // http://v1.43/containers/
  const char *response_cp2 = "/logs?stdout=1"; // ?stdout=true&timestamps=true&tail=1
  strcpy(cmd_url_response, response_cp1);
  strcat(cmd_url_response, id);
  strcat(cmd_url_response, response_cp2);
  fprintf(stderr, "response cmd_url_response: %s\n", cmd_url_response);
  responseResponse = docker_get(docker, cmd_url_response);
  if (responseResponse == CURLE_OK) {
    // char *dbuf = docker_buffer(docker);
    char *dbuf = (char*)malloc((docker->buffer->size+1) * sizeof(char));
    strcpy(dbuf, "");
    // fprintf(stderr, "on start dbuf = %s\n", dbuf);
    char b = '\0';
    // fprintf(stderr, "Container Response Successfully, dbuf size: %lu\n", docker->buffer->size);
    for ( size_t i=8; i < docker->buffer->size; i++ ) {
      // fprintf(stderr, "docker->buffer->data[i] d: %d\n", (int)docker->buffer->data[i]);
      // fprintf(stderr, "docker->buffer->data[i] c: %c\n", (char)docker->buffer->data[i]);
      b = (char)docker->buffer->data[i];
      if ( b == '\n' )  i = i + 8;
      strncat(dbuf, &b, 1);
      // fprintf(stderr, "striped 8 - char %c ascii %i - dbuf = %s\n", b, b, dbuf);
    }
    // fprintf(stderr, "Container Response Successfully, dbuf: %s\n", dbuf);
    return (messageResult) { "SUCCESS: read result of container", dbuf };
  } else {
    fprintf(stderr, "Unable to get response from container, CURL response code: %d\n", (int) responseResponse);
    return (messageResult) { "ERROR: unable to get response from container", "" };
  }
}

// bool allowed_to_run(const char *image) {
//   char allowed[3][1024] = { "rodezee/",
//                             "library/hello-world",
//                             "quay.io/podman/hello:latest" };
//   for(long unsigned int i=0; i < ( sizeof(allowed)/sizeof(allowed[0]) ); i++) {
//     fprintf(stderr, "allowed check %s => starts with => %s\n", image, allowed[i]);
//     if ( starts_with(allowed[i], image) ) {
//       fprintf(stderr, "ALLOWED: %s\n", image);
//       return true;
//     }
//   }
//   return false;
// }

responseResult docker_run(const char *body) {
  // GET image from body
  fprintf(stderr, "docker run, body: %s\n", body);
  struct mg_str json = mg_str(body);
  char image[1024] = "";
  strcpy(image, mg_json_get_str(json, "$.Image"));

  // // ACCESS CONTROL
  // if ( !allowed_to_run(image) ) {
  //   fprintf(stderr, "ERROR: NOT ALLOWED TO RUN IMAGE %s\n", image);
  //   return (responseResult) { false, "NOT ALLOWED TO RUN IMAGE" };
  // } else {

  // INIT
  DOCKER *docker = docker_init("v1.43"); // v1.25
  if ( !docker ) {
    fprintf(stderr, "ERROR: Failed to initialize to docker!\n");
    return (responseResult) { false, "Docker Initialization error" };
  } else {

    fprintf(stderr, "SUCCESS: initialized docker\n");

    // CREATE
    const char *id = do_docker_create(docker, body);
    if ( starts_with("ERROR:", id) ) {
      fprintf(stderr, "ERROR: Container Creation error %s\n", id);
      return (responseResult) { false, "Container Creation error" };
    } else {
      
      fprintf(stderr, "SUCCESS: image found and container created id: %s\n", id);

      // START
      const char *dstart = do_docker_start(docker, id);
      if ( starts_with("ERROR:", dstart) ) {
        fprintf(stderr, "ERROR: Container Starting error %s\n", dstart);
        return (responseResult) { false, "Container Starting error" };
      } else {
        
        fprintf(stderr, "SUCCESS: started container with id: %s\n", id);

        // WAIT
        const char *dwait = do_docker_wait(docker, id);
        if ( starts_with("ERROR:", dwait) ) {
          fprintf(stderr, "ERROR: Container Waiting error %s\n", dwait);
          return (responseResult) { false, "Container Waiting error" };
        } else {

          fprintf(stderr, "SUCCESS: waited container with id: %s\n", id);

          // RESPONSE
          messageResult mr = get_docker_result(docker, id);
          // messageResult mr = (messageResult) { "SUCCESS: test", "1234567890" };
          if ( starts_with("ERROR:", mr.message) ) {
            fprintf(stderr, "%s\n", mr.message);
            // mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{\"message\":%m}", mg_print_esc, 0, mr.message);
            return (responseResult) { false, mr.message };
          } else {
            fprintf(stderr, "%s\n%s\n", mr.message, mr.result);
            // mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{\"result\":%m}", mg_print_esc, 0, mr.result);
            return (responseResult) { true, mr.result };
            // mg_http_reply(c, 200, "Content-Type: text/plain; charset=utf-8\r\n", "%m%s", mg_print_esc, 0, "", r);
          }
        }
      }
    }
    docker_destroy(docker);
  }

  // }
}

// END DOCKER

// CUSTOM MONGOOSE

static char *mg_dhtml(const char *path) {
  long lSize;
  char *buffer;
  FILE *fp = fopen(path, "rb");
  if (fp != NULL) {

    if( !fp ) perror(path),exit(1);

    fseek( fp , 0L , SEEK_END);
    lSize = ftell( fp );
    rewind( fp );

    /* allocate memory for entire content */
    buffer = calloc( 1, lSize+1 );
    if( !buffer ) fclose(fp),fputs("memory alloc fails",stderr),exit(1);

    /* copy the file into the buffer */
    if( 1!=fread( buffer , lSize, 1 , fp) )
      fclose(fp),free(buffer),fputs("entire read fails",stderr),exit(1);

    /* do your work here, buffer is a string contains the whole text */
    fclose(fp);
  }
  return (char *) buffer;
}

// END CUSTOM MONGOOSE

static int s_debug_level = MG_LL_INFO;
static const char *s_root_dir = "/www";
static const char *s_listening_address = "http://0.0.0.0:8000";
static const char *s_enable_hexdump = "no";
static const char *s_ssi_pattern = "#.shtml";

static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
  if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;
    if ( mg_http_match_uri(hm, "/httpdocker") ) { // index uri
      responseResult rr = (responseResult) { true, "{}" };
      double num1, num2; // Expecting JSON array in the HTTP body, e.g. [ 123.38, -2.72 ]
      char *image; // Expecting JSON with string body, e.g. {"Image": "rodezee/hello-world:0.0.1"}
      if ( mg_json_get_num(hm->body, "$[0]", &num1)
        && mg_json_get_num(hm->body, "$[1]", &num2) ) { // found two numbers
        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{%m:%g}\n", mg_print_esc, 0, "result", num1 + num2);
      } else if ( (image = mg_json_get_str(hm->body, "$.Image")) ) { // found string image
        fprintf(stderr, "fn, body %s\n", hm->body.ptr);
        fprintf(stderr, "SUCCESS: found image in body %s\n", image);
        rr = docker_run(hm->body.ptr);
        if ( !rr.success ) {
          fprintf(stderr, "ERROR: unable to run the image %s\n", image);
          mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{\"error\":%m}", mg_print_esc, 0, rr.response);
        } else {
          fprintf(stderr, "SUCCESS: did run the image %s\n", image);
          mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{\"result\":%m}", mg_print_esc, 0, rr.response);
          free(rr.response);
        }
        free(image);
      } else { // found no input, go with standard image
        const char *body = "{\"Image\": \"quay.io/podman/hello:latest\"}"; // rodezee/hello-universe:0.0.1 rodezee/hello-world:0.0.1 library/hello-world:latest
        rr = docker_run(body);
        if ( !rr.success ) {
          fprintf(stderr, "ERROR: unable to run the body %s\n", body);
          mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{\"error\":%m}", mg_print_esc, 0, rr.response);
        } else {
          fprintf(stderr, "SUCCESS: did run the body %s\n", body);
          mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{\"result\":%m}", mg_print_esc, 0, rr.response);
          free(rr.response);
        }
      }
    } else if ( mg_http_match_uri(hm, "#.htmld") ) {
      const char uristr[1024];
      strncpy_s(uristr, hm->uri.ptr, strcspn(hm->uri.ptr," "));
      fprintf(stderr, "Uri Str :: %s ::\n", uristr);
      char *filetmp = mg_dhtml("/www/contact/hello-world.htmld");
      responseResult rr = (responseResult) { true, "{}" };
      rr = docker_run(filetmp);
      if ( !rr.success ) {
        fprintf(stderr, "ERROR: unable to run the body %s\n", filetmp);
        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{\"error\":%m}", mg_print_esc, 0, rr.response);
      } else {
        fprintf(stderr, "SUCCESS: did run the body %s\n", filetmp);
        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{\"result\":%m}", mg_print_esc, 0, rr.response);
        free(rr.response);
      }
      // fprintf(stderr, "file content: %s", filetmp);
      // mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{\"file_content\":%m}", mg_print_esc, 0, filetmp);
      free(filetmp);

    } else { // on all other uri return files
      struct mg_http_message *hm = ev_data, tmp = {0};
      struct mg_str unknown = mg_str_n("?", 1), *cl;
      struct mg_http_serve_opts opts = {0};
      opts.root_dir = s_root_dir;
      opts.ssi_pattern = s_ssi_pattern; // read more mongoose.c L 1964
      // opts.htmld_pattern = s_htmld_pattern; // custom createDockerContainerFile
      mg_http_serve_dir(c, hm, &opts);
      mg_http_parse((char *) c->send.buf, c->send.len, &tmp);
      cl = mg_http_get_header(&tmp, "Content-Length");
      if (cl == NULL) cl = &unknown;
      MG_INFO(("%.*s %.*s %.*s %.*s", (int) hm->method.len, hm->method.ptr,
              (int) hm->uri.len, hm->uri.ptr, (int) tmp.uri.len, tmp.uri.ptr,
              (int) cl->len, cl->ptr));
      // MG_INFO(("-%s %s %s %s", hm->method.ptr, hm->uri.ptr, tmp.uri.ptr, cl->ptr));
      (void) fn_data; // workaround compiler for errors: https://stackoverflow.com/questions/7354786/what-does-void-variable-name-do-at-the-beginning-of-a-c-function
    }
  }
}

// Handle interrupts, like Ctrl-C
static int s_signo;
static void signal_handler(int signo) {
  s_signo = signo;
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
  MG_INFO(("Debug level      : [%d]", s_debug_level));
  while (s_signo == 0) mg_mgr_poll(&mgr, 1000); // was also good with: 1000000
  mg_mgr_free(&mgr);
  MG_INFO(("Exiting on signal %d", s_signo));
  return 0;
}

// test: curl -d '{"Image": "rodezee/print-env:0.0.1", "Env": ["FOO=1", "BAR=2"]}' http://192.168.0.28:8000/httpdocker
