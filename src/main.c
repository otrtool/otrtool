#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/syscall.h>

#include <time.h>

#include <mcrypt.h>
#include <openssl/md5.h>

#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

#define ERROR(...) \
  ({fprintf(stderr, __VA_ARGS__); \
    fprintf(stderr, "\n"); \
    exit(EXIT_FAILURE); })

#ifndef VERSION
  #define VERSION "0.2.2"
#endif

#define LINE_LENGTH 80
#define CHUNK_SIZE  2097152    // 2M, must be multiple of 8
#define MAX_RESPONSE_LENGTH 1000

#define VERB_INFO  1
#define VERB_DEBUG 2
static int verbosity = VERB_INFO;

#define ACTION_INFO       1
#define ACTION_FETCHKEY   2
#define ACTION_DECRYPT    3
static int action = ACTION_INFO;

static char *email = NULL;
static char *password = NULL;
static char *keyphrase = NULL;
static char *filename = NULL;
static char *destfolder = NULL;
static char *destfilename = NULL;

static FILE *file = NULL;
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
  
  // abort very long transfers
  if (mem->size + realsize > MAX_RESPONSE_LENGTH) {
    realsize = mem->size <= MAX_RESPONSE_LENGTH
        ? MAX_RESPONSE_LENGTH - mem->size
        : 0;
  }
  
  mem->memory = realloc(mem->memory, mem->size + realsize + 1);
  if (mem->memory) {
    memcpy(&(mem->memory[mem->size]), ptr, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
  } else return 0;
  return realsize;
}

// ######################## generic functions ####################

void reverseWords(void *in_, int size) {
  unsigned char *in = in_;
  unsigned char *out = malloc(size);
  int i;
  for (i = 0; i < size; i++) {
    out[i + 3 - (i%4 * 2)] = in[i];
  }
  memcpy(in, out, size);
  free(out);
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
  int len = strlen(data_);
  unsigned char *data = (unsigned char*)data_;
  // never tested with lowercase letters!
  unsigned char *result = malloc(sizeof(char) * len + 1);
  int foo, bar;
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
  result[len] = 0;
  return (void*)result;
}

char * base64Encode(void *data_, int len) {
  unsigned char *data = data_;
  static const char b64[] = "\
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
    resptr[1] = b64[ (data[0] &     0b11) << 4 
                    | data[1]             >> 4 ];
    resptr[2] = b64[ (data[1] &   0b1111) << 2 
                    | data[2]             >> 6 ];
    resptr[3] = b64[  data[2] & 0b111111       ];
    resptr += 4;
    data += 3;
  }
  
  if (len < blocks * 3 - 1) {
    resptr[0] = b64[  data[0]             >> 2 ];
    resptr[1] = b64[ (data[0] &     0b11) << 4 ];
    resptr[2] = '=';
    resptr[3] = '=';
    resptr += 4;
  } else if (len < blocks * 3) {
    resptr[0] = b64[  data[0]             >> 2 ];
    resptr[1] = b64[ (data[0] &     0b11) << 4 
                    | data[1]             >> 4 ];
    resptr[2] = b64[ (data[1] &   0b1111) << 2 ];
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
  u_int8_t *text_ = (u_int8_t*)text;
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

char * queryGetParam(char *query, char *name) {
  char *begin = index(query, '&');
  char *end;
  int nameLen = strlen(name);
  
  while (begin != NULL) {
    begin++;
    if (strncmp(begin, name, nameLen) == 0 && begin[nameLen] == '=') {
      begin += nameLen + 1;
      end = index(begin, '&');
      char *result = malloc(end - begin + 1);
      strncpy(result, begin, end - begin);
      result[end - begin] = 0;
      return result;
    }
    begin = index(begin, '&');
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
      fwrite(line, index, 1, stdout);
      line[0] = '>';
      line[1] = ' ';
      index = 2;
    }
    message++;
  }
  line[index++] = '\n';
  if (index != 3) fwrite(line, index, 1, stdout);
}

void dumpQuerystring(char *query) {
  int length = strlen(query);
  char line[LINE_LENGTH + 1];
  int index = 0;
  
  if (*query == '&') {
    line[0] = '&';
    index++;
    query++;
  }
  
  for (; length > 0 ; length --) {
    if (*query == '&') {
      line[index] = '\n';
      fwrite(line, index + 1, 1, stdout);
      index = 0;
    }
    line[index] = *query;
    
    index++;
    if (index == LINE_LENGTH) {
      line[index] = '\n';
      fwrite(line, index + 1, 1, stdout);
      line[0] = ' ';
      index = 1;
    }
    query++;
  }
  line[index] = '\n';
  if (index != LINE_LENGTH) fwrite(line, index + 1, 1, stdout);
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
    printf("%08x  %s\n", pos, line);
    hexrep += 32;
  }
  printf("%08x\n", len);
  free(line);
  free(hexrep_orig);
}

// ###################### special functions ####################

char * getHeader() {
  unsigned char *header = malloc(sizeof(char) * 513);
  if (fread(header, 512, 1, file) < 1)
    ERROR("Error reading file");
  MCRYPT blowfish;
  blowfish = mcrypt_module_open("blowfish", NULL, "ecb", NULL);
  unsigned char hardKey[] = {
      0xEF, 0x3A, 0xB2, 0x9C, 0xD1, 0x9F, 0x0C, 0xAC,
      0x57, 0x59, 0xC7, 0xAB, 0xD1, 0x2C, 0xC9, 0x2B,
      0xA3, 0xFE, 0x0A, 0xFE, 0xBF, 0x96, 0x0D, 0x63,
      0xFE, 0xBD, 0x0F, 0x45};
  mcrypt_generic_init(blowfish, hardKey, 28, NULL);
  reverseWords(header, 512);
  mdecrypt_generic(blowfish, header, 512);
  reverseWords(header, 512);
  mcrypt_generic_deinit(blowfish);
  mcrypt_module_close(blowfish);
  
  char *padding = strstr((char*)header, "&PD=");
  if (padding == NULL)
    ERROR("Corrupted header: could not find padding");
  *padding = 0;
  
  if (verbosity >= VERB_DEBUG) {
    printf("\nDumping decrypted header:\n");
    dumpQuerystring((char*)header);
    printf("\n");
  }
  header[512] = 0;
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
  
  if (verbosity >= VERB_DEBUG) {
    printf("\nGenerated BigKey: %s\n\n", bigkey_hex);
  }
  
  void *res = hex2bin(bigkey_hex);
  
  free(bigkey_hex);
  free(mailhash);
  free(passhash);
  return res;
}

char * generateRequest(void *bigkey, char *date) {
  char *filename = queryGetParam(header, "FN");
  char *thatohthing = queryGetParam(header, "OH");
  MCRYPT blowfish = mcrypt_module_open("blowfish", NULL, "cbc", NULL);
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
&LN=EN\
&VN=0.4.1132\
&IR=TRUE\
&IK=aFzW1tL7nP9vXd8yUfB5kLoSyATQ\
&FN=%s\
&OH=%s\
&A=%s\
&P=%s\
&D=%s", filename, thatohthing, email, password, dump);
  
  if (verbosity >= VERB_DEBUG) {
    printf("\nGenerated request-'code':\n");
    dumpQuerystring(code);
    printf("\n");
  }
  
  mcrypt_generic_init(blowfish, bigkey, 28, iv);
  reverseWords(code, 512);
  mcrypt_generic(blowfish, code, 512);
  reverseWords(code, 512);
  mcrypt_generic_deinit(blowfish);
  mcrypt_module_close(blowfish);
  
  if (verbosity >= VERB_DEBUG) {
    printf("\nEncrypted request-'code':\n");
    dumpHex(code, 512);
    printf("\n");
  }
  
  snprintf(result, 1024, "http://87.236.198.182/quelle_neu1.php\
?code=%s\
&AA=%s\
&ZZ=%s", base64Encode(code, 512), email, date);
  
  if (verbosity >= VERB_DEBUG) {
    printf("\nRequest:\n%s\n\n", result);
  }
  
  free(code);
  free(dump);
  return result;
}

void * contactServer(char *request) {
  // http://curl.haxx.se/libcurl/c/getinmemory.html
  CURL *curl_handle;
  
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
  
  /* some servers don't like requests that are made without a user-agent
     field, so we provide one */ 
  curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
  
  /* get it! */ 
  curl_easy_perform(curl_handle);
  
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
  
  return (void *)chunk;
}

char * decryptResponse(char *response, void *bigkey) {
  MCRYPT blowfish = mcrypt_module_open("blowfish", NULL, "ecb", NULL);
  
  char *result = malloc(512);
  memcpy(result, response+8, 512);
  
  mcrypt_generic_init(blowfish, bigkey, 28, NULL);
  reverseWords(result, 512);
  mdecrypt_generic(blowfish, result, 512);
  reverseWords(result, 512);
  mcrypt_generic_deinit(blowfish);
  mcrypt_module_close(blowfish);
  
  int i;
  for (i = 0 ; i < 512 ; i++) {
    result[i] ^= response[i];
  }
  
  char *padding = strstr(result, "&D=");
  if (padding == NULL)
    ERROR("Corrupted response: could not find padding");
  *padding = 0;
  
  if (verbosity >= VERB_DEBUG) {
    printf("\nDecrypted response:\n");
    dumpQuerystring(result);
    printf("\n");
  }
  
  return result;
}

void fetchKeyphrase() {
  time_t time_ = time(NULL);
  char *date = malloc(9);
  strftime(date, 9, "%Y%m%d", gmtime(&time_));
  
  if (email == NULL)
    ERROR("Email unknown");
  if (password == NULL)
    ERROR("Password unknown");
  
  char *bigkey = generateBigkey(date);
  char *request = generateRequest(bigkey, date);
  
  printf("Trying to contact server...\n");
  struct MemoryStruct *response =
      (struct MemoryStruct *)contactServer(request);
  printf("Server responded.\n");
  
  response->memory[response->size] = 0;
  
  if (response->size != 696) {
    if (memcmp(response->memory,"MessageToBePrintedInDecoder",27) ==0) {
      printf("Server sent us this sweet message:\n");
      quote(response->memory + 27);
    } else {
      printf("Server sent us this ugly crap:\n");
      dumpHex(response->memory, response->size);
    }
    ERROR("Server response is unuseable, exiting");
  }
  
  int info_len;
  char *info_crypted = base64Decode(response->memory, &info_len);
  // check if len == 0x208
  if (info_len != 0x208)
    ERROR("Programmer was getting tired and added a bug");
  
  info = decryptResponse(info_crypted, bigkey);
  
  keyphrase = queryGetParam(info, "HP");
  if (keyphrase == NULL)
    ERROR("Response lacks keyphrase");
  
  if (strlen(keyphrase) != 56)
    ERROR("Keyphrase has wrong length");
  
  printf("Keyphrase: %s\n", keyphrase);
  
  free(bigkey);
  free(request);
  free(response);
  free(info_crypted);
}

void openFile() {
  if (strcmp("-", filename) == 0)
    file = stdin;
  else
    file = fopen(filename, "rb");
  
  if (file == NULL)
    ERROR("Error opening file");
  
  char magic[11];
  magic[10] = 0;
  if (fread(magic, 10, 1, file) < 1)
    ERROR("Error reading file");
  if (strcmp(magic, "OTRKEYFILE") != 0)
    ERROR("Wrong file format");
  
  header = getHeader();
}

void decryptFile() {
  int fd;
  
  if (destfolder != NULL)
    ERROR("Option -D is not implemented, yet");
  
  if (destfilename == NULL) {
    // this leaks memory. 10 valuable bytes
    destfilename = queryGetParam(header, "FN");
  }
  
  fd = open(destfilename, O_WRONLY|O_CREAT|O_EXCL,
    S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH);
  if (fd < 0 && errno == EEXIST) {
    // TODO: offer to overwrite
    ERROR("Destination file exists: %s", destfilename);
  }
  if (fd < 0)
    ERROR("Error opening destination file: %s", destfilename);
  
  // decrypt
  void *key = hex2bin(keyphrase);
  MCRYPT blowfish = mcrypt_module_open("blowfish", NULL, "ecb", NULL);
  mcrypt_generic_init(blowfish, key, 28, NULL);
  
  printf("Decrypting...\n");
  
  unsigned long long length = atol(queryGetParam(header, "SZ")) - 512;
  unsigned long long position = 0;
  int blocknum;
  char *progressbar = malloc(41);
  char rotatingFoo[4] = {'|', '/', '-', '\\'};
  int readsize;
  int writesize;
  char *buffer = malloc(CHUNK_SIZE);
  for (blocknum = 0 ; 1 ; blocknum++) {
    readsize = fread(buffer, 1, CHUNK_SIZE, file);
    if (readsize <= 0) break;
    
    reverseWords(buffer, CHUNK_SIZE);
    mdecrypt_generic(blowfish, buffer, CHUNK_SIZE);
    reverseWords(buffer, CHUNK_SIZE);
    
    writesize = write(fd, buffer, readsize);
    if (writesize != readsize)
      ERROR("Error writing to destination file");
    
    position += writesize;
    memset(progressbar, ' ', 40);
    memset(progressbar, '=', (position*40)/length);
    progressbar[40] = 0;
    printf("[%s] %3lli%% %c\r", progressbar, (position*100)/length,
      rotatingFoo[blocknum % 4]);
    fflush(stdout);
  }
  
  printf("[========================================] 100%%    \n");
  
  mcrypt_generic_deinit(blowfish);
  mcrypt_module_close(blowfish);
  
  if (close(fd) < 0)
    ERROR("Error closing destination file.");
  
  free(progressbar);
  free(key);
  free(buffer);
}

void usageError() {
  printf("\n");
  printf("Usage: otrtool [-h] [-v] [-i|-f|-x] [-k <keyphrase>] [-e <email> -p <password>]\n");
  printf("               [-D <destfolder>] [-O <destfile>] <otrkey-file>\n");
  printf("\n");
  printf("MODES OF OPERATION\n");
  printf("  -i | Display information about file\n");
  printf("  -f | Fetch keyphrase for file\n");
  printf("  -x | Decrypt file\n");
  printf("\n");
  printf("OTHER ARGUMENTS\n");
  printf("  -h | Display help\n");
  printf("  -v | Be verbose\n");
  printf("  -k | Do not fetch keyphrase, use this one\n");
  printf("  -e | Use this eMail address\n");
  printf("  -p | Use this password\n");
  printf("  -D | Decrypt to this folder (but use default name)\n");
  printf("  -O | Decrypt to this file (overwrite default name)\n");
  printf("\n");
}

int main(int argc, char *argv[]) {
  int opt;
  
  while ( (opt = getopt(argc, argv, "hvifxk:e:p:D:O:")) != -1) {
    switch (opt) {
      case 'h':
        usageError();
        exit(EXIT_SUCCESS);
        break;
      case 'v':
        verbosity = VERB_DEBUG;
        break;
      case 'i':
        action = ACTION_INFO;
        break;
      case 'f':
        action = ACTION_FETCHKEY;
        break;
      case 'x':
        action = ACTION_DECRYPT;
        break;
      case 'k':
        keyphrase = optarg;
        break;
      case 'e':
        email = optarg;
        break;
      case 'p':
        password = optarg;
        break;
      case 'D':
        destfolder = optarg;
        break;
      case 'O':
        destfilename = optarg;
        break;
      default:
        usageError();
        exit(EXIT_FAILURE);
    }
  }
  
  if (verbosity >= VERB_DEBUG)
    printf("OTR-Tool %s\n", VERSION);
  
  if (optind >= argc) {
    fprintf(stderr, "Missing argument: otrkey-file\n");
    usageError();
    exit(EXIT_FAILURE);
  }
  
  filename = argv[optind];
  
  openFile();
  
  switch (action) {
    case ACTION_INFO:
      // TODO: output something nicer than just the querystring
      dumpQuerystring(header);
      break;
    case ACTION_FETCHKEY:
      fetchKeyphrase();
      break;
    case ACTION_DECRYPT:
      if (keyphrase == NULL)
        fetchKeyphrase();
      
      errno = 0;
      nice(10);
      if (errno == 0 && verbosity >= VERB_DEBUG)
        printf("NICE was set to 10\n");
      
      // I am not sure if this really catches all errors
      // If this causes problems, just delete the ionice-stuff
      #ifdef __NR_ioprio_set
        if (syscall(__NR_ioprio_set, 1, getpid(), 7 | 3 << 13) == 0
             && verbosity >= VERB_DEBUG)
          printf("IONICE class was set to Idle\n");
      #endif
      
      decryptFile();
      break;
  }
  
  free(header);
  
  if (fclose(file) != 0)
    ERROR("Error closing file. I don't care, I was going to exit anyway");
  
  exit(EXIT_SUCCESS);
}
