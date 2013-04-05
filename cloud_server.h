


#define PORT "4001"  // the port users will be connecting to
#define BACKLOG 10   // how many pending connections queue will hold

#define MAX_REQUEST_MSG_LEN_CHAR 100
#define MAX_RESPONSE_MSG_LEN_CHAR 200
#define COMPRESSION_PERCENT 30

#define PREFIX_NUM_BYTES 4
#define RTP_HEADER_NUM_BYTES 12

#define DOLLAR_SIGN_CHAR '$'
#define RTP_HEADER_FIRST_4_BITS_AS_INT 8
#define CSRC_ID_DEFAULT_4_BITS_AS_INT 0
#define PAYLOAD_CODE_JPEG 26

#define MS_PER_FRAME 40

#define want_to_print_console 0

#define BAD_PRIMARY "\nThe primary command sent by client is none of SETUP, PLAY, PAUSE, or TEARDOWN. It will therefore be ignored.\n"

#define NAME_MISMATCH "\nThe movie name provided does not match from previous requests. Bad client! This will force a teardown of the socket.\n"

//DEFINE STRUCTS

typedef struct _request {
    char *primary_cmd;
    char *file_name;
    char *rtsp_ver;
    int c_seq;
    int session;
    char *transport;
    int interleaved;
    int scale;
    
    //the new_fd of the client
    int new_fd;
    
} Request;

typedef struct _response {
    char *status_line;
    int c_seq;
    int session;
} Response;

typedef struct _vstruct {
    int socket_fd;
    char *video_name; //TODO CHECK    

    CvCapture *video;
    IplImage *image;
    CvMat *thumb;
    CvMat *encoded;
} Vstruct;

typedef struct _tmstruct {
    struct sigevent play_event;
    timer_t play_timer;
    struct itimerspec play_interval;
} Tstruct;

typedef struct _info {
	int state;
	int new_fd;
	char *video_name;
	char* rtsp_ver;
	int c_seq;
	int session;
} Info;

typedef struct _send_frame_data {
    int *socket_fd;
    double *next_frame;
    int *scale;
    CvCapture* video;
    char *video_name; //TODO check
    
} Send_frame_data;




//FUNCTION PROTOTYPES

void sigchld_handler(int);
void *get_in_addr(struct sockaddr*);
void *serve_client(void *ptr);
Request* create_req_struct(char[], int);
void remove_newline_carriage(char* ori, char* res);
void print_req(Request* req);
void handle_req(Request* req, Vstruct* vs, Tstruct *ts);
void handle_setup(Request* req, Vstruct* vs);
void handle_play(Request* req, Vstruct* vs, Tstruct *ts);
void handle_pause(Request* req, Vstruct* vs, Tstruct *ts);
void handle_teardown(Request* req, Vstruct* vs, Tstruct *ts);

void stop_my_timer(Tstruct *ts);
void start_my_timer(Tstruct *ts, Vstruct *vs, int scale);
void send_frame(union sigval sv_data);
void request_and_send_frame(union sigval sv_data);
void create_and_send_response(char* status_line, int c_seq, int session, int new_fd);
void create_rtp_packet(unsigned char rtp_pkt[], CvMat* encoded, Send_frame_data *data);
int connect_to_cloud(char *cloud_server_name, char *cloud_port_number);
void create_rtp_packet_after_cloud(unsigned char rtp_pkt[], short payload_plus_header_size, Send_frame_data *data);

int initialize_fields(Request* req, Info* info);
int check_consistent(Request* req, Info* info);
void bad_format(char* error_message);

void free_info(Info *info);
void free_vstruct(Vstruct* vs);
void free_tstruct(Tstruct* ts); 
void free_data(Send_frame_data *data); 
void free_req(Request *req); 

// FOR REFERENCE - PREFIX AND RTP FORMAT

/* 
 
 --FIRST NEED RTSP PREFIX, 4 bytes
 
 BYTE-0
 - is dollar sign "$" ASCII 0x24
 BYTE-1
 - is always 0 for us
 BYTE-2-3
 - is length of RTP packet in Big Endian (i.e. the length of whatever comes after this prefix)

 --RTP HEADER IS 12 BYTES
 
 BYTE-0 [BYTE-4 overall]
 - bit 0-1: is 10
 - bit 2: is 0 (no padding)
 - bit 3: is 0 (no extension)
 -> in other words bits 0-3 is: 1,0,0,0 i.e. decimal 8
 - bit 4-7: is CSRC identifier //TODO what is this
 
 BYTE-1 [BYTE-5]
 - bit 0: is 0 (no marker)
 - bit 1-7: is payload type
 
 BYTE-2 and 3 [BYTE-6-7]
 - bit 0 to 15: seq number
 
 BYTES 4-7 [BYTE-8-11]
 - bit 0-31: is timestamp
 
 BYTES 8-11 [BYTE-12-15]
 - bit 0-32: ia SSRC identifier (random)

 */

// FOR REFERENCE

/*
 Timer - http://man7.org/linux/man-pages/man2/timer_gettime.2.html
 IplImage - http://opencv.willowgarage.com/documentation/basic_structures.html
 
 */
