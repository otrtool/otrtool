#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>

#include <unistd.h>
#include <termios.h>
#include <sys/stat.h>
#include <fcntl.h>
#if _POSIX_THREADS > 0 && _POSIX_SEMAPHORES > 0
  #include <pthread.h>
  #include <semaphore.h>
  #define MT 1
#else
  #define MT 0
#endif

#ifdef __linux__
  #include <sys/syscall.h> /* ioprio_set */
#endif

#include <curl/curl.h>
#include <mcrypt.h>
#include "md5.h"

#define ERROR(...) \
  ({fprintf(stderr, "\n"); \
    fprintf(stderr, __VA_ARGS__); \
    fprintf(stderr, "\n"); \
    exit(EXIT_FAILURE); })

#define PERROR(...) \
  ({fprintf(stderr, "\n"); \
    fprintf(stderr, __VA_ARGS__); \
    fprintf(stderr, ": "); \
    perror(NULL); \
    exit(EXIT_FAILURE); })

#define MIN(a,b) ((a)<(b)?(a):(b))

#ifndef VERSION
  #define VERSION "version unknown"
#endif

#define LINE_LENGTH 80
#define MAX_RESPONSE_LENGTH 1000
#define CREAT_MODE (S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH)

/* global options as supplied by the user via command-line etc. */
struct otrtool_options {
  enum { ACTION_INFO, ACTION_FETCHKEY, ACTION_DECRYPT, ACTION_VERIFY }
      action;
  enum { VERB_INFO, VERB_DEBUG }
      verbosity;
  int guimode; // do not output \r and stuff
  int unlinkmode;
  char *email;
  char *password;
  char *keyphrase;
  char *destdir;
  char *destfile;
  int n_threads;
};
static struct otrtool_options opts = {
  .action = ACTION_INFO,
  .verbosity = VERB_INFO,
  .guimode = 0,
  .unlinkmode = 0,
  .email = NULL,
  .password = NULL,
  .keyphrase = NULL,
  .destdir = NULL,
  .destfile = NULL,
  .n_threads = 0,
};

/* global variables */
static int interactive = 1; // ask questions instead of exiting
static int logfilemode = 0; // do not output progress bar

static char *email = NULL;
static char *password = NULL;
static char *keyphrase = NULL;
static char *filename = NULL;
static char *destfilename = NULL;

static FILE *file = NULL;
static FILE *keyfile = NULL;
static char *keyfilename = NULL;
static char *keyfileshortname = NULL; /* home dir is abbreviated to ~ */
static FILE *ttyfile = NULL;
static char *header = NULL;
static char *info = NULL;

// ######################## curl-stuff #######################

struct MemoryStruct {
  char *memory;
  size_t size;
};

static size_t WriteMemoryCallback(void *ptr, size_t size,
          size_t nmemb, void *data) {
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)data;
  char *newmem;
  
  // abort very long transfers
  if (mem->size + realsize > MAX_RESPONSE_LENGTH) {
    realsize = mem->size <= MAX_RESPONSE_LENGTH
        ? MAX_RESPONSE_LENGTH - mem->size
        : 0;
  }
  if (realsize < 1) return 0;
  
  // "If realloc() fails the original block is left untouched" (man 3 realloc)
  newmem = realloc(mem->memory, mem->size + realsize);
  if (newmem != NULL) {
    mem->memory = newmem;
    memcpy(&(mem->memory[mem->size]), ptr, realsize);
    mem->size += realsize;
  } else return 0;
  return realsize;
}

// ######################## generic functions ####################

void perrorf(const char *fmt, ...) {
  int errsv = errno;
  va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
  fputs(": ", stderr);
  errno = errsv; perror(NULL);
}

char * bin2hex(void *data_, int len) {
  unsigned char *data = data_;
  unsigned char *result = malloc(sizeof(char) * len * 2 + 1);
  result[len * 2] = 0;
  int foo;
  for (len-- ; len >= 0 ; len--) {
    foo = data[len] % 16;
    result[len*2 + 1] = foo > 9 ? 0x37 + foo : 0x30 + foo;
    foo = data[len] >> 4;
    result[len*2] = foo > 9 ? 0x37 + foo : 0x30 + foo;
  }
  return (char*)result;
}

void * hex2bin(char *data_) {
  int len = strlen(data_) / 2;
  unsigned char *data = (unsigned char*)data_;
  // never tested with lowercase letters!
  unsigned char *result = malloc(sizeof(char) * len + 1);
  int foo, bar;
  result[len] = 0;
  for (len-- ; len >= 0 ; len--) {
    foo = data[len*2];
    if (foo < 0x41) {
      // is a digit
      bar = foo - 0x30;
    } else if (foo < 0x61) {
      // is a uppercase letter
      bar = foo - 0x37;
    } else {
      // is a lowercase letter
      bar = foo - 0x57;
    }
    result[len] = bar << 4;
    
    foo = data[len*2 + 1];
    if (foo < 0x41) {
      // is a digit
      bar = foo - 0x30;
    } else if (foo < 0x61) {
      // is a uppercase letter
      bar = foo - 0x37;
    } else {
      // is a lowercase letter
      bar = foo - 0x57;
    }
    result[len] += bar;
  }
  return (void*)result;
}

// C does not support binary constants, but gcc >= 4.3 does.
// Because we can't really expect people to update their compilers in four
// years (4.3 is from march 2008), the following defines will substitute
// the three values used by base64Encode with their decimal equivalent.
#define B_11 3
#define B_1111 15
#define B_111111 63
char * base64Encode(void *data_, int len) {
  unsigned char *data = data_;
  static const char *b64 = "\
ABCDEFGHIJKLMNOPQRSTUVWXYZ\
abcdefghijklmnopqrstuvwxyz\
0123456789+/";
  int blocks = (len + 2) / 3;
  int newlen = blocks * 4 + 1;
  char *result = malloc(newlen);
  char *resptr = result;
  int i;
  
  for (i = len / 3 ; i > 0 ; i--) {
    resptr[0] = b64[  data[0]             >> 2 ];
    resptr[1] = b64[ (data[0] &     B_11) << 4
                    | data[1]             >> 4 ];
    resptr[2] = b64[ (data[1] &   B_1111) << 2
                    | data[2]             >> 6 ];
    resptr[3] = b64[  data[2] & B_111111       ];
    resptr += 4;
    data += 3;
  }
  
  if (len < blocks * 3 - 1) {
    resptr[0] = b64[  data[0]             >> 2 ];
    resptr[1] = b64[ (data[0] &     B_11) << 4 ];
    resptr[2] = '=';
    resptr[3] = '=';
    resptr += 4;
  } else if (len < blocks * 3) {
    resptr[0] = b64[  data[0]             >> 2 ];
    resptr[1] = b64[ (data[0] &     B_11) << 4
                    | data[1]             >> 4 ];
    resptr[2] = b64[ (data[1] &   B_1111) << 2 ];
    resptr[3] = '=';
    resptr += 4;
  }
  
  *resptr = 0;
  return result;
}

void * base64Decode(char *text, int *outlen) {
  static const unsigned char b64dec[] = {
   0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0, //00
   0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0, //10
   0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0, 62,  0,  0,  0, 63, //20
  52, 53, 54, 55, 56, 57, 58, 59,  60, 61,  0,  0,  0,  0,  0,  0, //30
   0,  0,  1,  2,  3,  4,  5,  6,   7,  8,  9, 10, 11, 12, 13, 14, //40
  15, 16, 17, 18, 19, 20, 21, 22,  23, 24, 25,  0,  0,  0,  0,  0, //50
   0, 26, 27, 28, 29, 30, 31, 32,  33, 34, 35, 36, 37, 38, 39, 40, //60
  41, 42, 43, 44, 45, 46, 47, 48,  49, 50, 51,  0,  0,  0,  0,  0, //70
   0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0, //80
   0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0, //90
   0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0, //a0
   0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0, //b0
   0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0, //c0
   0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0, //d0
   0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0, //e0
   0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0  //f0
  };
  // this functions treats invalid characters as 'A'. deal with it :-P
  int inlen = (strlen(text) >> 2) << 2;
  int blocks = inlen >> 2;
  *outlen = blocks * 3 - (text[inlen-2] == '='
      ? 2 : (text[inlen-1] == '=' ? 1 : 0));
  char *result = malloc(blocks * 3);
  char *resptr = result;
  uint8_t *text_ = (uint8_t*)text;
  int i;
  
  for (i = 0 ; i < blocks ; i++) {
    resptr[0] = b64dec[text_[0]] << 2 | b64dec[text_[1]] >> 4;
    resptr[1] = b64dec[text_[1]] << 4 | b64dec[text_[2]] >> 2;
    resptr[2] = b64dec[text_[2]] << 6 | b64dec[text_[3]];
    
    text_ += 4;
    resptr += 3;
  }
  
  return (void*)result;
}

int isBase64(char *text) {
  static const char *b64 = "\
ABCDEFGHIJKLMNOPQRSTUVWXYZ\
abcdefghijklmnopqrstuvwxyz\
0123456789+/=";
  return strlen(text) == strspn(text, b64);
}

char * queryGetParam(char *query, char *name) {
  char *begin = strchr(query, '&');
  char *end;
  int nameLen = strlen(name);
  
  while (begin != NULL) {
    begin++;
    if (strncmp(begin, name, nameLen) == 0 && begin[nameLen] == '=') {
      begin += nameLen + 1;
      end = strchr(begin, '&');
      if (end == NULL)
        end = begin + strlen(begin);
      char *result = malloc(end - begin + 1);
      strncpy(result, begin, end - begin);
      result[end - begin] = 0;
      return result;
    }
    begin = strchr(begin, '&');
  }
  return NULL;
}

void quote(char *message) {
  char line[LINE_LENGTH + 1];
  line[0] = '>';
  line[1] = ' ';
  int index = 2;
  
  while (*message != 0) {
    if (*message < 0x20 || *message > 0x7E) {
      line[index++] = ' ';
    } else {
      line[index++] = *message;
    }
    if (index == LINE_LENGTH) {
      line[index++] = '\n';
      fwrite(line, index, 1, stderr);
      line[0] = '>';
      line[1] = ' ';
      index = 2;
    }
    message++;
  }
  line[index++] = '\n';
  if (index != 3) fwrite(line, index, 1, stderr);
}

void dumpQuerystring(char *query) {
  char line[LINE_LENGTH + 1];
  int index = 0;

  if (*query == '&') {
    line[0] = '&';
    index++;
    query++;
  }

  for (; *query; query++) {
    if (*query == '&') {
      line[index] = '\n';
      fwrite(line, index + 1, 1, stderr);
      index = 0;
    }
    line[index] = *query;

    index++;
    if (index == LINE_LENGTH) {
      line[index] = '\n';
      fwrite(line, index + 1, 1, stderr);
      line[0] = ' ';
      index = 1;
    }
  }
  line[index] = '\n';
  if (index != LINE_LENGTH) fwrite(line, index + 1, 1, stderr);
}

void dumpHex(void *data_, int len) {
  unsigned char *data = data_;
  unsigned char *line = malloc(sizeof(char) * LINE_LENGTH + 1);
  char *hexrep_orig = bin2hex(data, len);
  char *hexrep = hexrep_orig;
  int i, pos;
  
  for (pos = 0 ; pos < len ; pos += 16) {
    for (i = 0 ; i < 8 ; i++) {
      line[i*3]   = pos+i < len ? hexrep[i*2] : ' ';
      line[i*3+1] = pos+i < len ? hexrep[i*2+1] : ' ';
      line[i*3+2] = ' ';
    }
    line[24] = ' ';
    for (i = 8 ; i < 16 ; i++) {
      line[i*3+1] = pos+i < len ? hexrep[i*2] : ' ';
      line[i*3+2] = pos+i < len ? hexrep[i*2+1] : ' ';
      line[i*3+3] = ' ';
    }
    line[49] = ' ';
    line[50] = '|';
    for (i = 0 ; i < 16 ; i++) {
      if (data[pos+i] >= 0x20 && data[pos+i] < 0x7f) {
        line[51+i] = pos+i < len ? data[pos+i] : ' ';
      } else {
        line[51+i] = pos+i < len ? '.' : ' ';
      }
    }
    line[67] = '|';
    
    line[68] = 0;
    fprintf(stderr, "%08x  %s\n", pos, line);
    hexrep += 32;
  }
  fprintf(stderr, "%08x\n", len);
  free(line);
  free(hexrep_orig);
}

/* special case length=0 means 'finished' */
void showProgress(long long position, long long length) {
  static long long oldpos = 0;
  static unsigned int blocknum = 0;
  const char progressbar[41] = "========================================";
  const char *rotatingFoo = "|/-\\";

  if (logfilemode)
    return;
  if (length > 0) {
    if (oldpos > position) {
      oldpos = 0;
      blocknum = 0;
    }
    if (position - oldpos >= 2097152 || position == 0) {
      if (opts.guimode == 0) {
        fprintf(stderr, "[%-40.*s] %3i%% %c\r", (int)(position*40/length),
            progressbar, (int)(position*100/length),
            rotatingFoo[blocknum++ % 4]);
      } else {
        fprintf(stdout, "gui> %7.3f\n", position*100.0/length);
        fflush(stdout);
      }
      fflush(stderr);
      oldpos = position;
    }
  } else {
    if (opts.guimode == 0) {
      fputs("[========================================] 100%    \n", stderr);
    } else {
      fputs("gui> Finished\n", stdout);
      fflush(stdout);
    }
    oldpos = 0;
    blocknum = 0;
  }
}

// ################# multi-threading functions #################
/* Just some wrapper functions that handle errors
   and allow to fall back to single-threading easily */

#if MT == 0 /* no multi-threading */
typedef void *thread_t;
typedef int semaphore_t;

thread_t createThread(void *(*s)(void *), void *a) { return s(a); }
void *joinThread(thread_t t) { return t; }
#define initSemaphore(x,y)
#define destroySemaphore(x)
#define waitSemaphore(x)
#define postSemaphore(x)

#elif MT == 1 /* POSIX threads and semaphores */
typedef pthread_t thread_t;
typedef sem_t semaphore_t;

thread_t createThread(void *(*start_routine)(void *), void *arg) {
  pthread_t tid;
  errno = pthread_create(&tid, NULL, start_routine, arg);
  if (errno != 0) PERROR("pthread_create");
  return tid;
}
void *joinThread(thread_t tid) {
  void *res;
  errno = pthread_join(tid, &res);
  if (errno != 0) PERROR("pthread_join");
  return res;
}
void initSemaphore(semaphore_t *psem, unsigned int value) {
  if (sem_init(psem, 0, value) < 0) PERROR("sem_init");
}
void destroySemaphore(semaphore_t *psem) {
  if (sem_destroy(psem) < 0) PERROR("sem_destroy");
}
void waitSemaphore(semaphore_t *psem) {
  int i;
  while ((i = sem_wait(psem)) == -1 && errno == EINTR);
  if (i < 0) PERROR("sem_wait");
}
void postSemaphore(semaphore_t *psem) {
  if (sem_post(psem) < 0) PERROR("sem_post");
}
#endif /* MT */

// ###################### special functions ####################

char * getHeader() {
  unsigned char *header = malloc(sizeof(char) * 513);
  if (fread(header, 512, 1, file) < 1 && !feof(file))
    PERROR("Error reading file");
  if (feof(file))
    ERROR("Error: unexpected end of file");
  MCRYPT blowfish;
  blowfish = mcrypt_module_open("blowfish-compat", NULL, "ecb", NULL);
  unsigned char hardKey[] = {
      0xEF, 0x3A, 0xB2, 0x9C, 0xD1, 0x9F, 0x0C, 0xAC,
      0x57, 0x59, 0xC7, 0xAB, 0xD1, 0x2C, 0xC9, 0x2B,
      0xA3, 0xFE, 0x0A, 0xFE, 0xBF, 0x96, 0x0D, 0x63,
      0xFE, 0xBD, 0x0F, 0x45};
  mcrypt_generic_init(blowfish, hardKey, 28, NULL);
  mdecrypt_generic(blowfish, header, 512);
  mcrypt_generic_deinit(blowfish);
  mcrypt_module_close(blowfish);
  header[512] = 0;
  
  char *padding = strstr((char*)header, "&PD=");
  if (padding == NULL)
    ERROR("Corrupted header: could not find padding");
  *padding = 0;
  
  if (opts.verbosity >= VERB_DEBUG) {
    fputs("\nDumping decrypted header:\n", stderr);
    dumpQuerystring((char*)header);
    fputs("\n", stderr);
  }
  return (char*)header;
}

void * generateBigkey(char *date) {
  char *mailhash = bin2hex(MD5(
      (unsigned char*)email, strlen(email), NULL), 16);
  char *passhash = bin2hex(MD5(
      (unsigned char*)password, strlen(password), NULL), 16);
  char *bigkey_hex = malloc(57 * sizeof(char));
  char *ptr = bigkey_hex;
  
  strncpy(ptr, mailhash, 13);
  ptr += 13;
  
  strncpy(ptr, date, 4);
  date += 4;
  ptr += 4;
  
  strncpy(ptr, passhash, 11);
  ptr += 11;
  
  strncpy(ptr, date, 2);
  date += 2;
  ptr += 2;
  
  strncpy(ptr, mailhash + 21, 11);
  ptr += 11;
  
  strncpy(ptr, date, 2);
  ptr += 2;
  
  strncpy(ptr, passhash + 19, 13);
  ptr += 13;
  
  *ptr = 0;
  
  if (opts.verbosity >= VERB_DEBUG) {
    fprintf(stderr, "\nGenerated BigKey: %s\n\n", bigkey_hex);
  }
  
  void *res = hex2bin(bigkey_hex);
  
  free(bigkey_hex);
  free(mailhash);
  free(passhash);
  return res;
}

char * generateRequest(void *bigkey, char *date) {
  char *headerFN = queryGetParam(header, "FN");
  char *thatohthing = queryGetParam(header, "OH");
  MCRYPT blowfish = mcrypt_module_open("blowfish-compat", NULL, "cbc", NULL);
  char *iv = malloc(mcrypt_enc_get_iv_size(blowfish));
  char *code = malloc(513);
  char *dump = malloc(513);
  char *result = malloc(1024); // base64-encoded code is 680 bytes
  
  memset(iv, 0x42, mcrypt_enc_get_iv_size(blowfish));
  memset(dump, 'd', 512);
  dump[512] = 0;
  
  snprintf(code, 513, "FOOOOBAR\
&OS=01677e4c0ae5468b9b8b823487f14524\
&M=01677e4c0ae5468b9b8b823487f14524\
&LN=DE\
&VN=1.4.1132\
&IR=TRUE\
&IK=aFzW1tL7nP9vXd8yUfB5kLoSyATQ\
&FN=%s\
&OH=%s\
&A=%s\
&P=%s\
&D=%s", headerFN, thatohthing, email, password, dump);
  
  if (opts.verbosity >= VERB_DEBUG) {
    fputs("\nGenerated request-'code':\n", stderr);
    dumpQuerystring(code);
    fputs("\n", stderr);
  }
  
  mcrypt_generic_init(blowfish, bigkey, 28, iv);
  mcrypt_generic(blowfish, code, 512);
  mcrypt_generic_deinit(blowfish);
  mcrypt_module_close(blowfish);
  
  if (opts.verbosity >= VERB_DEBUG) {
    fputs("\nEncrypted request-'code':\n", stderr);
    dumpHex(code, 512);
    fputs("\n", stderr);
  }
  
  snprintf(result, 1024, "http://185.195.80.111/quelle_neu1.php\
?code=%s\
&AA=%s\
&ZZ=%s", base64Encode(code, 512), email, date);
  
  if (opts.verbosity >= VERB_DEBUG) {
    fprintf(stderr, "\nRequest:\n%s\n\n", result);
  }
  
  free(code);
  free(dump);
  free(iv);
  free(headerFN);
  free(thatohthing);
  return result;
}

struct MemoryStruct * contactServer(char *request) {
  // http://curl.haxx.se/libcurl/c/getinmemory.html
  CURL *curl_handle;
  CURLcode res;
  char errbuf[CURL_ERROR_SIZE];
  
  struct MemoryStruct *chunk = malloc(sizeof(struct MemoryStruct));
  chunk->memory=NULL; /* we expect realloc(NULL, size) to work */ 
  chunk->size = 0;    /* no data at this point */ 
  
  curl_global_init(CURL_GLOBAL_ALL);
  
  /* init the curl session */ 
  curl_handle = curl_easy_init();
  
  /* specify URL to get */ 
  curl_easy_setopt(curl_handle, CURLOPT_URL, request);
  
  /* send all data to this function  */ 
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
  
  /* we pass our 'chunk' struct to the callback function */ 
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)chunk);
  
  /* imitate the original OTR client */ 
  curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "Linux-OTR-Decoder/0.4.592");
  curl_easy_setopt(curl_handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
  
  /* set verbosity and error message buffer */
  if (opts.verbosity >= VERB_DEBUG)
    curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1);
  curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER, errbuf);
  
  /* get it! */ 
  *errbuf = 0;
  if ((res = curl_easy_perform(curl_handle)) != CURLE_OK)
    ERROR("cURL error %d: %s", res, *errbuf ? errbuf : curl_easy_strerror(res));
  
  /* cleanup curl stuff */ 
  curl_easy_cleanup(curl_handle);
  
  /*
   * Now, our chunk.memory points to a memory block that is chunk.size
   * bytes big and contains the remote file.
   *
   * Do something nice with it!
   *
   * You should be aware of the fact that at this point we might have an
   * allocated data block, and nothing has yet deallocated that data. So when
   * you're done with it, you should free() it as a nice application.
   */ 
  
  /* we're done with libcurl, so clean it up */ 
  curl_global_cleanup();
  
  // null-terminate response
  chunk->memory = realloc(chunk->memory, chunk->size + 1);
  if (chunk->memory == NULL) PERROR("realloc");
  chunk->memory[chunk->size] = 0;
  return chunk;
}

char * decryptResponse(char *response, int length, void *bigkey) {
  MCRYPT blowfish = mcrypt_module_open("blowfish-compat", NULL, "cbc", NULL);
  
  if (length < mcrypt_enc_get_iv_size(blowfish) || length < 8)
    return NULL;
  length -= 8;
  
  char *result = malloc(length);
  memcpy(result, response+8, length);
  
  mcrypt_generic_init(blowfish, bigkey, 28, response);
  mdecrypt_generic(blowfish, result, length);
  mcrypt_generic_deinit(blowfish);
  mcrypt_module_close(blowfish);
  
  char *padding = strstr(result, "&D=");
  if (padding == NULL)
    ERROR("Corrupted response: could not find padding");
  *padding = 0;
  
  if (opts.verbosity >= VERB_DEBUG) {
    fputs("\nDecrypted response:\n", stderr);
    dumpQuerystring(result);
    fputs("\n", stderr);
  }
  
  return result;
}

void keycache_open() {
  char *home = NULL, *data_home = NULL, *pch;
  char *filenames[2] = { NULL, NULL };
  const int n_filenames = 2;
  int i;

  /* Build home and data home strings */
  if ((pch = getenv("HOME")) != NULL && strlen(pch) > 0)
    home = strdup(pch);
  if ((pch = getenv("XDG_DATA_HOME")) != NULL && strlen(pch) > 0)
    data_home = strdup(pch);
  if (data_home == NULL && home != NULL
      && (data_home = malloc(strlen(home) + 14)) != NULL) {
    strcpy(data_home, home);
    strcat(data_home, "/.local/share");
  }
  if (!home && !data_home)
    fputs("warning: neither HOME nor XDG_DATA_HOME is set\n", stderr);

  /* Make array of file name alternatives */
  if (home && (filenames[0] = malloc(strlen(home) + 15)) != NULL) {
    strcpy(filenames[0], home);
    strcat(filenames[0], "/.otrkey_cache");
  }
  if (data_home && (filenames[1] = malloc(strlen(data_home) + 22)) != NULL) {
    strcpy(filenames[1], data_home);
    strcat(filenames[1], "/otrtool/otrkey_cache");
  }

  /* Choose the first existing cache file alternative */
  keyfilename = NULL;
  for (i=0; i < n_filenames; ++i) {
    if (filenames[i] && access(filenames[i], F_OK) == 0) break;
  }
  if (i == n_filenames) i = 0; /* choose default */
  if (filenames[i]) keyfilename = strdup(filenames[i]);

  if (keyfilename) {
    /* Construct abbreviated key file name (replace HOME with ~) */
    if (home && strncmp(keyfilename, home, strlen(home)) == 0) {
      if ((pch = malloc(strlen(keyfilename)-strlen(home)+2)) == NULL)
        PERROR("malloc");
      strcpy(pch, "~");
      strcat(pch, keyfilename+strlen(home));
      keyfileshortname = pch;
    }
    else {
      if ((keyfileshortname = strdup(keyfilename)) == NULL) PERROR("strdup");
    }
    if(opts.verbosity >= VERB_DEBUG)
      fprintf(stderr, "key cache is at %s\n", keyfileshortname);

    /* Open key cache */
    keyfile = fopen(keyfilename, "a+");
    if (keyfile == NULL && opts.verbosity >= VERB_DEBUG)
      perrorf("cannot open key cache: %s", keyfilename);
  }

  for (i=0; i < n_filenames; ++i) free(filenames[i]);
  free(home);
  free(data_home);
}

char *keycache_get(const char *fh) {
  char *cachephrase, *cachefh;
  static char line[512];
  
  if (fh == NULL || keyfile == NULL) return NULL;
  rewind(keyfile);
  while (fgets(line, sizeof(line), keyfile) != NULL) {
    cachefh = strtok(line, " \t\r\n");
    cachephrase = strtok(NULL, " \t\r\n");
    if (cachephrase == NULL || cachefh == NULL) continue;
    if (strcmp(cachefh, fh) == 0) return cachephrase;
  }
  if (!feof(keyfile)) PERROR("fgets");
  return NULL;
}

void keycache_put(const char *fh, const char *keyphrase) {
  char *cachephrase, *fn;
  
  if (fh == NULL || keyfile == NULL) return;
  if ((cachephrase = keycache_get(fh)) != NULL) {
    if (strcmp(keyphrase, cachephrase) != 0)
      fputs("warning: differing keyphrase was found in cache file!\n", stderr);
    else
      fputs("info: keyphrase was already in cache\n", stderr);
    return;
  }
  fn = queryGetParam(header, "FN");
  if (fprintf(keyfile, "%s\t%s\t# %s\n", fh, keyphrase, fn) < 0)
    PERROR("fprintf");
  fflush(keyfile);
  fprintf(stderr, "info: saved keyphrase to %s\n", keyfileshortname);
}

void fetchKeyphrase() {
  struct termios ios0, ios1;
  time_t time_ = time(NULL);
  char *date = malloc(9);
  strftime(date, 9, "%Y%m%d", gmtime(&time_));
  
  if (info) {
    free(info);
    info = NULL;
  }
  
  if (opts.email == NULL) {
    if (!interactive) ERROR("Email address not specified");
    opts.email = malloc(51);
    fputs("Enter your eMail-address: ", stderr);
    if (fscanf(ttyfile, "%50s", opts.email) < 1)
      ERROR("Email invalid");
    while (fgetc(ttyfile) != '\n');
  }
  email = strdup(opts.email);

  if (opts.password == NULL) {
    if (!interactive) ERROR("Password not specified");
    opts.password = malloc(51);
    fputs("Enter your password: ", stderr);
    tcgetattr(fileno(ttyfile), &ios0);
    ios1 = ios0;
    ios1.c_lflag &= ~ECHO;
    tcsetattr(fileno(ttyfile), TCSAFLUSH, &ios1);
    if (fscanf(ttyfile, "%50s", opts.password) < 1) {
      tcsetattr(0, TCSAFLUSH, &ios0);
      ERROR("Password invalid");
    }
    tcsetattr(fileno(ttyfile), TCSAFLUSH, &ios0);
    while (fgetc(ttyfile) != '\n');
    fputc('\n', stderr);
  }
  password = strdup(opts.password);
  
  char *bigkey = generateBigkey(date);
  char *request = generateRequest(bigkey, date);
  free(email);
  free(password);
  
  fputs("Trying to contact server...\n", stderr);
  struct MemoryStruct *response = contactServer(request);

  if (response->size == 0 || response->memory == NULL) {
    ERROR("Server sent an empty response, exiting");
  }
  fputs("Server responded.\n", stderr);
  
  // skip initial whitespace
  char *message = response->memory;
  message += strspn(message, " \t\n");
  
  if (isBase64(message) == 0) {
    if (memcmp(message,"MessageToBePrintedInDecoder",27) ==0) {
      fputs("Server sent us this sweet message:\n", stderr);
      quote(message + 27);
    } else {
      fputs("Server sent us this ugly crap:\n", stderr);
      dumpHex(response->memory, response->size);
    }
    ERROR("Server response is unuseable, exiting");
  }
  
  int info_len;
  char *info_crypted = base64Decode(message, &info_len);
  
  if (info_len % 8 != 0) {
    fputs("Length of response must be a multiple of 8.", stderr);
    dumpHex(info_crypted, info_len);
    ERROR("Server response is unuseable, exiting");
  }
  
  info = decryptResponse(info_crypted, info_len, bigkey);
  
  keyphrase = queryGetParam(info, "HP");
  if (keyphrase == NULL)
    ERROR("Response lacks keyphrase");
  
  if (strlen(keyphrase) != 56)
    ERROR("Keyphrase has wrong length");
  
  fprintf(stderr, "Keyphrase: %s\n", keyphrase);
  keycache_put(queryGetParam(header, "FH"), keyphrase);
  
  free(date);
  free(bigkey);
  free(request);
  free(response->memory);
  free(response);
  free(info_crypted);
}

void openFile() {
  if (strcmp("-", filename) == 0)
    file = stdin;
  else
    file = fopen(filename, "rb");
  
  if (file == NULL)
    PERROR("Error opening file");
  
  char magic[11] = { 0 };
  if (fread(magic, 10, 1, file) < 1 && !feof(file))
    PERROR("Error reading file");
  if (feof(file))
    ERROR("Error: unexpected end of file");
  if (strcmp(magic, "OTRKEYFILE") != 0)
    ERROR("Wrong file format");
  
  header = getHeader();
}

typedef struct verifyFile_ctx {
  MD5_CTX ctx;
  char hash1[16];
  int input;
} vfy_t;

void verifyFile_init(vfy_t *vfy, int input) {
  char *hash_hex, *hash;
  int i;
  
  memset(vfy, 0, sizeof(*vfy));
  vfy->input = input;
  
  /* get MD5 sum from 'OH' or 'FH' header field */
  hash_hex = queryGetParam(header, vfy->input?"OH":"FH");
  if (hash_hex == NULL || strlen(hash_hex) != 48)
    ERROR("Missing hash in file header / unexpected format");
  for (i=1; i<16; ++i) {
    hash_hex[2*i] = hash_hex[3*i];
    hash_hex[2*i+1] = hash_hex[3*i+1];
  }
  hash_hex[32] = 0;
  if (opts.verbosity >= VERB_DEBUG)
    fprintf(stderr, "Checking %s against MD5 sum: %s\n",
      vfy->input?"input":"output", hash_hex);
  hash = hex2bin(hash_hex);
  memcpy(vfy->hash1, hash, 16);
  
  /* calculate MD5 sum of file (without header) */
  memset(&vfy->ctx, 0, sizeof(vfy->ctx));
  MD5_Init(&vfy->ctx);
  
  free(hash_hex);
  free(hash);
}

void verifyFile_data(vfy_t *vfy, char *buffer, size_t len) {
  MD5_Update(&vfy->ctx, buffer, len);
}

void verifyFile_final(vfy_t *vfy) {
  unsigned char md5[16];
  
  MD5_Final(md5, &vfy->ctx);
  if (memcmp(vfy->hash1, md5, 16) != 0) {
    if (vfy->input)
      ERROR("Input file had errors. Output may or may not be usable.");
    else
      ERROR("Output verification failed. Wrong key?");
  }
}

void verifyOnly() {
  vfy_t vfy;
  size_t n;
  static char buffer[65536];
  unsigned long long length;
  unsigned long long position;

  length = atoll(queryGetParam(header, "SZ")) - 522;
  fputs("Verifying otrkey...\n", stderr);
  verifyFile_init(&vfy, 1);
  for (position = 0; position < length; position += n) {
    showProgress(position, length);
    n = MIN(length - position, sizeof(buffer));
    if (fread(buffer, 1, n, file) < n) break;
    verifyFile_data(&vfy, buffer, n);
  }
  if (position < length) {
    if (!feof(file)) PERROR("fread");
    if (!logfilemode) fputc('\n', stderr);
    fputs("file is too short\n", stderr);
  }
  else
    showProgress(1, 0);

  if (fread(buffer, 1, 1, file) > 0)
    fputs("file contains trailing garbage\n", stderr);
  else if (!feof(file))
    PERROR("fread");
  verifyFile_final(&vfy);
  fputs("file is OK\n", stderr);
}

struct worker_info {
  struct worker_info *next;
  semaphore_t read_sema, ivfy_sema, ovfy_sema, write_sema;
  void *key;
  unsigned long long length;
  unsigned long long *pposition;
  FILE *file, *destfile;
  vfy_t *vfy_inp, *vfy_outp;
};

void *decryptFileWorker(void *w_) {
  struct worker_info *w = w_;
  char buffer[65536];
  unsigned long long length = w->length;
  unsigned long long position;
  size_t readsize, writesize;
  MCRYPT blowfish;

  blowfish = mcrypt_module_open("blowfish-compat", NULL, "ecb", NULL);
  mcrypt_generic_init(blowfish, w->key, 28, NULL);

  waitSemaphore(&w->read_sema);
  while ((position = *w->pposition) < length) {
    showProgress(position, length);
    readsize = MIN(length - position, sizeof(buffer));
    if (fread(buffer, 1, readsize, w->file) < readsize) {
      if (feof(w->file))
        ERROR("Input file is too short");
      PERROR("Error reading input file");
    }
    *w->pposition = position + readsize;
    postSemaphore(&w->next->read_sema);

    waitSemaphore(&w->ivfy_sema);
    verifyFile_data(w->vfy_inp, buffer, readsize);
    postSemaphore(&w->next->ivfy_sema);

    /* If the payload length is not a multiple of eight,
     * the last few bytes are stored unencrypted */
    mdecrypt_generic(blowfish, buffer, readsize - readsize % 8);

    waitSemaphore(&w->ovfy_sema);
    verifyFile_data(w->vfy_outp, buffer, readsize);
    postSemaphore(&w->next->ovfy_sema);

    waitSemaphore(&w->write_sema);
    writesize = fwrite(buffer, 1, readsize, w->destfile);
    if (writesize != readsize)
      PERROR("Error writing to destination file");
    postSemaphore(&w->next->write_sema);

    waitSemaphore(&w->read_sema);
  }
  postSemaphore(&w->next->read_sema);

  mcrypt_generic_deinit(blowfish);
  mcrypt_module_close(blowfish);

  return NULL;
}

void decryptFile() {
  int fd;
  char *headerFN;
  struct stat st;
  FILE *destfile;

  if (opts.destfile == NULL) {
    headerFN = queryGetParam(header, "FN");
    if (opts.destdir != NULL) {
      destfilename = malloc(strlen(opts.destdir) + strlen(headerFN) + 2);
      strcpy(destfilename, opts.destdir);
      strcat(destfilename, "/");
      strcat(destfilename, headerFN);
      free(headerFN);
    }
    else {
      destfilename = headerFN;
    }
  }
  else {
    destfilename = strdup(opts.destfile);
  }
  
  if (strcmp(destfilename, "-") == 0) {
    if (isatty(1)) ERROR("error: cowardly refusing to output to a terminal");
    fd = 1;
  }
  else
    fd = open(destfilename, O_WRONLY|O_CREAT|O_EXCL, CREAT_MODE);
  if (fd < 0 && errno == EEXIST) {
    if (stat(destfilename, &st) != 0 || S_ISREG(st.st_mode)) {
      if (!interactive) ERROR("Destination file exists: %s", destfilename);
      fprintf(stderr, "Destination file exists: %s\nType y to overwrite: ",
        destfilename);
      if (fgetc(ttyfile) != 'y') exit(EXIT_FAILURE);
      while (fgetc(ttyfile) != '\n');
      fd = open(destfilename, O_WRONLY|O_TRUNC, 0);
    }
    else
      fd = open(destfilename, O_WRONLY, 0);
  }
  if (fd < 0)
    PERROR("Error opening destination file: %s", destfilename);
  if ((destfile = fdopen(fd, "wb")) == NULL)
    PERROR("fdopen");
  
  fputs("Decrypting and verifying...\n", stderr); // -----------------------
  
  void *key = hex2bin(keyphrase);
  unsigned long long length = atoll(queryGetParam(header, "SZ")) - 522;
  unsigned long long position = 0;
  vfy_t vfy_in, vfy_out;
  
  verifyFile_init(&vfy_in, 1);
  verifyFile_init(&vfy_out, 0);

  enum {
    N_WRK_MAX = MT ? 16 : 1,
    /* Maximum multithreading speedup should be around 6 to 7. In case the
       number of CPUs is unknown, rather have too many threads than too few. */
    N_WRK_DEFAULT = 8,
  };
  thread_t wrk_id[16];
  struct worker_info wrk_info[16];
  int n_wrk = N_WRK_DEFAULT;
  int i;

#ifdef _SC_NPROCESSORS_ONLN
  i = sysconf(_SC_NPROCESSORS_ONLN);
  if (opts.verbosity >= VERB_DEBUG)
    fprintf(stderr, "number of CPUs: %d\n", i);
  if (i > 0) n_wrk = MIN(i, N_WRK_DEFAULT);
#endif
  if (opts.n_threads > 0)
    n_wrk = MIN(opts.n_threads, N_WRK_MAX);
  if (opts.verbosity >= VERB_DEBUG) {
  #if MT
    fprintf(stderr, "number of worker threads: %d\n", n_wrk);
  #else
    fputs("multi-threading disabled at compile-time\n", stderr);
  #endif
  }

  /* Create worker threads */
  for (i=0; i < n_wrk; i++) {
    wrk_info[i].next = (i < n_wrk - 1) ? wrk_info + i + 1 : wrk_info;
    initSemaphore(&wrk_info[i].read_sema, 0);
    initSemaphore(&wrk_info[i].ivfy_sema, 0);
    initSemaphore(&wrk_info[i].ovfy_sema, 0);
    initSemaphore(&wrk_info[i].write_sema, 0);
    wrk_info[i].key = key;
    wrk_info[i].length = length;
    wrk_info[i].pposition = &position;
    wrk_info[i].file = file;
    wrk_info[i].destfile = destfile;
    wrk_info[i].vfy_inp = &vfy_in;
    wrk_info[i].vfy_outp = &vfy_out;
  }
  for (i=0; i < n_wrk; i++)
    wrk_id[i] = createThread(decryptFileWorker, &wrk_info[i]);

  /* Go! */
  postSemaphore(&wrk_info[0].read_sema);
  postSemaphore(&wrk_info[0].ivfy_sema);
  postSemaphore(&wrk_info[0].ovfy_sema);
  postSemaphore(&wrk_info[0].write_sema);

  /* Wait... */
  for (i=0; i < n_wrk; i++)
    joinThread(wrk_id[i]);

  for (i=0; i < n_wrk; i++) {
    destroySemaphore(&wrk_info[i].read_sema);
    destroySemaphore(&wrk_info[i].ivfy_sema);
    destroySemaphore(&wrk_info[i].ovfy_sema);
    destroySemaphore(&wrk_info[i].write_sema);
  }
  showProgress(1, 0);

  verifyFile_final(&vfy_in);
  verifyFile_final(&vfy_out);
  fputs("OK checksums from header match\n", stderr);
  
  if (fclose(destfile) != 0)
    PERROR("Error closing destination file.");

  if (opts.unlinkmode) {
    if (strcmp(filename, "-") != 0 &&
        stat(filename, &st) == 0 && S_ISREG(st.st_mode) &&
        strcmp(destfilename, "-") != 0 &&
        stat(destfilename, &st) == 0 && S_ISREG(st.st_mode)) {
      if (unlink(filename) != 0)
        PERROR("Cannot delete input file");
      else
        fputs("info: input file has been deleted\n", stderr);
    }
    else {
      fputs("Warning: Not deleting input file (input or "
          "output is not a regular file)\n", stderr);
    }
  }
  
  free(key);
  free(destfilename);
}

void processFile() {
  int storeKeyphrase;
  switch (opts.action) {
    case ACTION_INFO:
      // TODO: output something nicer than just the querystring
      dumpQuerystring(header);
      break;
    case ACTION_FETCHKEY:
      fetchKeyphrase();
      break;
    case ACTION_DECRYPT:
      storeKeyphrase = 1;
      if (opts.keyphrase == NULL) {
        storeKeyphrase = 0;
        keyphrase = keycache_get(queryGetParam(header, "FH"));
        if (keyphrase)
          fprintf(stderr, "Keyphrase from cache: %s\n", keyphrase);
        else
          fetchKeyphrase();
      }
      else {
        keyphrase = strdup(opts.keyphrase);
      }
      decryptFile();
      if (storeKeyphrase)
        keycache_put(queryGetParam(header, "FH"), keyphrase);
      break;
    case ACTION_VERIFY:
      verifyOnly();
      break;
  }
}

void usageError() {
  fputs("\n"
    "Usage: otrtool [-h] [-v] [-i|-f|-x|-y] [-u]\n"
    "               [-k <keyphrase>] [-e <email>] [-p <password>]\n"
    "               [-D <destfolder>] [-O <destfile>]\n"
    "               <otrkey-file1> [<otrkey-file2> ... [<otrkey-fileN>]]\n"
    "\n"
    "MODES OF OPERATION\n"
    "  -i | Display information about file (default action)\n"
    "  -f | Fetch keyphrase for file\n"
    "  -x | Decrypt file\n"
    "  -y | Verify only\n"
    "\n"
    "FREQUENTLY USED OPTIONS\n"
    "  -k | Do not fetch keyphrase, use this one\n"
    "  -D | Output folder\n"
    "  -O | Output file (overrides -D)\n"
    "  -u | Delete otrkey-files after successful decryption\n"
    "\n"
    "See otrtool(1) for further information\n", stderr);
}

int main(int argc, char *argv[]) {
  fputs("OTR-Tool, " VERSION "\n", stderr);

  int i;
  int opt;
  while ( (opt = getopt(argc, argv, "hvgifxyk:e:p:D:O:uT:")) != -1) {
    switch (opt) {
      case 'h':
        usageError();
        exit(EXIT_SUCCESS);
        break;
      case 'v':
        opts.verbosity = VERB_DEBUG;
        break;
      case 'g':
        opts.guimode = 1;
        interactive = 0;
        break;
      case 'i':
        opts.action = ACTION_INFO;
        break;
      case 'f':
        opts.action = ACTION_FETCHKEY;
        break;
      case 'x':
        opts.action = ACTION_DECRYPT;
        break;
      case 'y':
        opts.action = ACTION_VERIFY;
        break;
      case 'k':
        opts.keyphrase = optarg;
        break;
      case 'e':
        opts.email = strdup(optarg);
        memset(optarg, 'x', strlen(optarg));
        break;
      case 'p':
        opts.password = strdup(optarg);
        memset(optarg, 'x', strlen(optarg));
        break;
      case 'D':
        opts.destdir = optarg;
        break;
      case 'O':
        opts.destfile = optarg;
        break;
      case 'u':
        opts.unlinkmode = 1;
        break;
      case 'T':
        opts.n_threads = atoi(optarg);
        break;
      default:
        usageError();
        exit(EXIT_FAILURE);
    }
  }
  if (opts.verbosity >= VERB_DEBUG) {
    fputs("command line: ", stderr);
    for (i = 0; i < argc; ++i) {
      fputs(argv[i], stderr);
      fputc((i == argc - 1) ? '\n' : ' ', stderr);
    }
  }
  
  if (optind >= argc) {
    fprintf(stderr, "Missing argument: otrkey-file\n");
    usageError();
    exit(EXIT_FAILURE);
  }
  if (argc > optind + 1) {
    if (opts.destfile != NULL && strcmp(opts.destfile, "-") == 0) {
      i = 0;
    }
    else for (i = optind; i < argc; i++) {
      if (strcmp(argv[i], "-") == 0)
        break;
    }
    if (i < argc)
      ERROR("Usage error: piping is not possible with multiple input files");
  }
  if (opts.guimode && opts.destfile != NULL && strcmp(opts.destfile, "-") == 0)
    ERROR("Usage error: options -g and -O - are incompatible");

  if (!isatty(2) && opts.guimode == 0) {
    logfilemode = 1;
    interactive = 0;
  }
  if (interactive) {
    if (!isatty(0)) {
      const char *ttyname = ctermid(NULL); /* usually /dev/tty */
      ttyfile = fopen(ttyname, "r");
      if (ttyfile == NULL) {
        if (opts.verbosity >= VERB_DEBUG)
          perrorf("cannot open TTY: %s", ttyname);
        interactive = 0;
      }
    }
    else ttyfile = stdin;
  }

  if ((opts.action == ACTION_DECRYPT || opts.action == ACTION_VERIFY)
      && opts.n_threads == 0) {
    errno = 0;
    nice(10);
    if (errno == 0 && opts.verbosity >= VERB_DEBUG)
      fputs("NICE was set to 10\n", stderr);

    // Set I/O scheduling class using the Linux-specific ioprio_set syscall
    // If this causes problems, just delete the ionice-stuff
    #if defined(__linux__) && defined(__NR_ioprio_set)
      if (syscall(__NR_ioprio_set, 1, getpid(), 7 | 3 << 13) == 0
           && opts.verbosity >= VERB_DEBUG)
        fputs("IONICE class was set to Idle\n", stderr);
    #endif
  }
  if (opts.action == ACTION_FETCHKEY || opts.action == ACTION_DECRYPT) {
    keycache_open();
  }

  for (i = optind; i < argc; i++) {
    filename = argv[i];
    if (argc > optind + 1)
      fprintf(stderr, "\n==> %s <==\n", filename);
    openFile();
    processFile();
    if (fclose(file) != 0)
      PERROR("Error closing file");
    free(header);
  }
  
  exit(EXIT_SUCCESS);
}
