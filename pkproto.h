/* Data structure describing a frame */
struct pk_frame {
  int   length;                /* Length of data                    */
  char* data;                  /* Pointer to beginning of data      */
  int   raw_length;            /* Length of raw data                */
  char* raw_frame;             /* Raw data (including frame header  */
};

/* Data structure describing a parsed chunk */
struct pk_chunk {
  char*           sid;             /* SID:   Stream ID                       */
  char*           request_host;    /* Host:  Requested host/domain-name      */
  char*           request_proto;   /* Proto: Requested protocol              */
  char*           request_port;    /* Port:  Requested port number (string)  */
  char*           remote_ip;       /* RIP:   Remote IP address (string)      */
  char*           remote_port;     /* RPort: Remote port number (string)     */
  int             length;          /* Length of chunk data.                  */
  char*           data;            /* Pointer to chunk data.                 */
  struct pk_frame frame;           /* The raw data.                          */
};

/* Parser object. */
struct pk_parser {
  int              buffer_bytes_left;
  struct pk_chunk* chunk;
  void*            chunk_callback; /* FIXME: This should be a function type. */
};

struct pk_parser* parser_create (int, char*, void*);
int               parser_parse  (struct pk_parser*, int, char*);

int               pkproto_test  (void);
