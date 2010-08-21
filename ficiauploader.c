/*
  MIT LICENSE
  copyright kazuhiro osawa
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <sys/stat.h>

#define DEBUG 0
#define VERSION_STR "0.01"

#define API_LOGINNAME "ficia"
#define API_ENDPOINT  "http://ficia.com/api/gr2/eyefi/main.php"
#define GR2_STATUS_LOGIN_OK "#__GR2PROTO__\nserver_version=2.11\nstatus=0\nstatus_text=OK\n"
#define GR2_STATUS_UPLOAD_OK "#__GR2PROTO__\nstatus=0\nstatus_text=OK\n"

typedef struct {
  int len;
  char *buf;
} gallery2_text;

static void usage(void)
{
  fputs("ficiauploader\n"
	"\n"
	"Usage: ficiauploader [options]\n"
	"\n"
	"Environment Variable:\n"
	"FICIA_PASSWORD     your ficia eye-fi password\n"
	"\n"
	"Options:\n"
	" -f <upload file>  upload file\n"
	" -h                displays this help message\n"
	" -v                displays version number\n"
	"\n"
	"Examples:\n"
	" $ export FICIA_PASSWORD=xxxxxxxx ficiauploader -f example.jpg\n",
	stdout);
  exit(0);
}

static void show_version(void)
{
  fputs("ficiauploader " VERSION_STR "\n"
	"\n"
	"Copyright (C) Kazuhiro Osawa\n",
	stdout);
  exit(0);
}

size_t writefunction_callback(void *ptr, size_t size ,size_t nmemb, gallery2_text *text)
{
  int ptrsize = size * nmemb;
  text->len += ptrsize;
  if ((text->buf = realloc(text->buf, text->len + 1)) == NULL) {
    fprintf(stderr, "memory allocation error\n");
    exit(-1);
  }
  memcpy(text->buf + text->len - ptrsize, ptr, ptrsize);
  text->buf[text->len] = '\0';
  return ptrsize;
}

int gallery2_response_success(gallery2_text *text) {
  if (!text->buf) return 0;

  /* login */
  if (strlen(text->buf) == strlen(GR2_STATUS_LOGIN_OK)) {
    if (strcmp(text->buf, GR2_STATUS_LOGIN_OK) == 0) return 1;
    return 2;
  }

  /* upload */
  text->buf[strlen(GR2_STATUS_UPLOAD_OK)] = '\0';
  if (strcmp(text->buf, GR2_STATUS_UPLOAD_OK) != 0) return 0;

  return 1;
}

int gallery2_login(CURL *curl, char *login_name, char *login_password) {
  CURLcode ret;
  struct curl_httppost *post = NULL;
  struct curl_httppost *last = NULL;
  gallery2_text response;

  bzero(&response, sizeof(gallery2_text));

  curl_formadd(&post, &last,
               CURLFORM_COPYNAME, "g2_controller",
               CURLFORM_COPYCONTENTS, "remote:GalleryRemote",
	       CURLFORM_END);
  curl_formadd(&post, &last,
               CURLFORM_COPYNAME, "g2_form[cmd]",
               CURLFORM_COPYCONTENTS, "login",
	       CURLFORM_END);
  curl_formadd(&post, &last,
               CURLFORM_COPYNAME, "g2_form[uname]",
               CURLFORM_COPYCONTENTS, login_name,
	       CURLFORM_END);
  curl_formadd(&post, &last,
               CURLFORM_COPYNAME, "g2_form[password]",
               CURLFORM_COPYCONTENTS, login_password,
	       CURLFORM_END);


  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunction_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
  curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
  ret = curl_easy_perform(curl);
  curl_formfree(post);
  if (ret != 0) {
    if (response.buf) free(response.buf);
    return 0;
  }
  if (gallery2_response_success(&response) == 0) {
    if (response.buf) free(response.buf);
    return 0;
  }
  if (response.buf) free(response.buf);


  return 1;
}

int gallery2_upload(CURL *curl, char *filename) {
  CURLcode ret;
  struct curl_httppost *post = NULL;
  struct curl_httppost *last = NULL;
  char *p, *basename, *buf;
  struct stat s;
  FILE *fp;
  gallery2_text response;

  bzero(&response, sizeof(gallery2_text));

  bzero(&s, sizeof(s));
  stat(filename, &s);

  if ((s.st_mode & S_IFMT) == 0) {
    fprintf(stderr, "No such file or directory: %s\n", filename);
    return 0;
  }

  /*
  buf = calloc(s.st_size, sizeof(char));
  if (buf == NULL) return 0;
  if ((fp = fopen(filename, "rb")) == NULL) return 0;
  if (fread(buf, 1, s.st_size, fp) != s.st_size) return 0;
  */

  p = filename + strlen(filename);
  while (p != filename) {
    if (*p == '/') {
      p++;
      break;
    }
    p--;
  }
  basename = p;

  if (strlen(basename) == 0) {
    fprintf(stderr, "cannot detected file name: %s\n", filename);
    return 0;
  }

  curl_formadd(&post, &last,
               CURLFORM_COPYNAME, "g2_controller",
               CURLFORM_COPYCONTENTS, "remote:GalleryRemote",
	       CURLFORM_END);
  curl_formadd(&post, &last,
               CURLFORM_COPYNAME, "g2_form[cmd]",
               CURLFORM_COPYCONTENTS, "add-item",
	       CURLFORM_END);
  curl_formadd(&post, &last,
               CURLFORM_COPYNAME, "g2_userfile_name",
               CURLFORM_COPYCONTENTS, basename,
	       CURLFORM_END);
  curl_formadd(&post, &last,
               CURLFORM_COPYNAME, "g2_userfile",
               CURLFORM_FILE, filename, CURLFORM_END);

  /*
  curl_formadd(&post, &last,
               CURLFORM_COPYNAME, "g2_userfile",
               CURLFORM_PTRCONTENTS, buf,
	       CURLFORM_CONTENTSLENGTH, s.st_size,
	       CURLFORM_CONTENTTYPE, "image/jpeg",
	       CURLFORM_FILECONTENT, "ageage.jpg",
	       CURLFORM_CONTENTHEADER, headers,
	       CURLFORM_END);
  */

  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunction_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
  curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
  ret = curl_easy_perform(curl);
  curl_formfree(post);
  if (ret != 0) {
    if (response.buf) free(response.buf);
    return 0;
  }
  if (gallery2_response_success(&response) == 0) {
    if (response.buf) free(response.buf);
    return 0;
  }
  if (response.buf) free(response.buf);
  return 1;
}

int main(int argc, char **argv)
{
  CURL *curl;
  CURLcode ret;
  char *login_name, *login_password;
  char *filename = NULL;

  /* parse args */
  argv++;
  while (*argv != NULL && argv[0][0] == '-') {
    char *arg = *argv++;
    if (strcmp(arg, "-f") == 0) {
      filename = *argv++;
    } else if (strcmp(arg, "-v") == 0) {
      show_version();
    } else if (strcmp(arg, "-h") == 0) {
      usage();
    } 
  }

  if (filename == NULL) usage();

  login_name     = API_LOGINNAME;
  login_password = getenv("FICIA_PASSWORD");

  curl = curl_easy_init();
  if (!curl) {
    fprintf(stderr, "couldn't init curl\n");
    return 0;
  }

  if (DEBUG) curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt(curl, CURLOPT_COOKIESESSION, 1);
  curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");
  curl_easy_setopt(curl, CURLOPT_URL, API_ENDPOINT);

  if (!gallery2_login(curl, login_name, login_password)) {
    fprintf(stderr, "couldn't login\n");
    return 0;
  }

  if (!gallery2_upload(curl, filename)) {
    fprintf(stderr, "couldn't file upload\n");
    return 0;
  }

  fprintf(stdout, "uploaded: %s\n", filename);

  curl_easy_cleanup(curl);

  return 0;
}
