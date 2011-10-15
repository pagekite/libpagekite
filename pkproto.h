/* Data structure describing a frame */
struct s_frame {
  int   length;                         /* Length of data                    */
  char* data;                           /* Pointer to beginning of data      */
  int   raw_length;                     /* Length of raw data                */
  char* raw_frame;                      /* Raw data (including frame header  */
};

/* Data structure describing a parsed chunk */
struct s_chunk {
  char*           sid;             /* SID:   Stream ID                       */
  char*           request_host;    /* Host:  Requested host/domain-name      */
  char*           request_proto;   /* Proto: Requested protocol              */
  char*           request_port;    /* Port:  Requested port number (string)  */
  char*           remote_ip;       /* RIP:   Remote IP address (string)      */
  char*           remote_port;     /* RPort: Remote port number (string)     */
  int             length;          /* Length of chunk data.                  */
  char*           data;            /* Pointer to chunk data.                 */
  struct s_frame  frame;           /* The raw data.                          */
};

/* Parser object. */
struct s_parser {
  int parser_size;
  int parser_size_max;
  struct s_chunk* current_chunk;
  void *chunk_callback; /* FIXME: This should be a function type. */
};

struct s_parser* parser_create(int, char*, void*);
int              parser_parse(struct s_parser*, int, char*);

