//
//  server.c
//  CS317A3hw
//
//  Created by Toshi . on 13-03-17.
//  Copyright (c) 2013 __MyCompanyName__. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>

#include <cv.h>
#include <highgui.h>

#include <sys/time.h>
#include <signal.h>
#include <time.h>

#include "server.h"


int want_print_console = 0;

/**
   MAIN - This is the server program
*/

int main(void)
{
	    
  int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr; // connector's address information
  socklen_t sin_size;
	    
  //note: sigaction sa from Beej removed
	    
  int yes=1;
  char s[INET6_ADDRSTRLEN];
  int rv;
	    

  /*--------------------------
    ATTEMP CONNECTION
    ---------------------------*/
	    
  //hints start clean, then fill it
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM; //means TCP
  hints.ai_flags = AI_PASSIVE; //use my IP
	    
  //do DNS lookup and get servinfo (a linked list of addrinfo) for later use
  if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1; //if fail quit main() and return 1
  }
	    
  //loop through above results and bind to the first we can
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype,
			 p->ai_protocol)) == -1) {
      perror("server: socket");
      continue;
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
	           sizeof(int)) == -1) {
      perror("setsockopt");
      exit(1);
    }  
    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("server: bind");
      continue;
    }
	        
    break;
  }
	    
  //if still can't bind after trying everything, give up
  if (p == NULL)  {
    fprintf(stderr, "server: failed to bind\n");
    return 2;
  }
	    
  //all done with this structure, free it  
  freeaddrinfo(servinfo); 
	   
  //now starts listening to potentially client who wants to connect)
  if (listen(sockfd, BACKLOG) == -1) {
    perror("listen");
    exit(1);
  }
	    
  //note: sigaction codes from Beej removed (for fork only)

	    
  printf("server: waiting for connections...\n");
	    
	    
  /*--------------------------------
    LOOP FOREVER AND SERVE VIDEO
    --------------------------------*/
	    
  while(1) {  // main accept() loop
    sin_size = sizeof their_addr;
    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
    if (new_fd == -1) {
      perror("accept");
      continue;
    }
	        
    inet_ntop(their_addr.ss_family,
	      get_in_addr((struct sockaddr *)&their_addr),
	      s, sizeof s);
    printf("server: got connection from %s\n", s);
	        
    //spin a thread for this client, and detach to let it run itself  
    pthread_t client_thread;
    pthread_create(&client_thread, NULL, serve_client, (void *) (intptr_t) new_fd);
    pthread_detach(client_thread);
	                
  }
  pthread_exit(NULL);
  return 0;
}

/**
   SERVE_CLIENT - This is the main method that each thread will run
*/
void *serve_client(void *ptr) {
  int new_fd = (int) (intptr_t) ptr;
	    
  Vstruct* vs = (Vstruct* )malloc(sizeof(Vstruct));
  Tstruct* ts = (Tstruct* )malloc(sizeof(Tstruct));

  //SAMMYADDED
  /*making the info struct to track users' state */
  //creating the info struct and putting the value in it
  Info* info = (Info* )malloc(sizeof(Info));
  memset( info, '\0', sizeof(info) );
  
  //on initial setup, state should be -1 to indicate session needs to be set
  info->state = -1;
  info->new_fd = new_fd;

  //fill in the relevant fields as they come in   

  if (!vs) {
    printf("Error creating Vstruct\n");
  }

  while(1) {
    char buf[MAX_REQUEST_MSG_LEN_CHAR] = {'\0'};
    int numbytes;
	    
    //Wait until receive a request from client
    if ((numbytes = (int)recv(new_fd, buf, 99, 0)) == -1) {
      printf("ERROR recv() ing");
    }
    //TODO hardy why 99 above, and why saw ERROR recv()
	 
    //Else if received, pass to handler
    if (numbytes > 0) {
      buf[numbytes] = '\0';
      printf("...client just gives me \n%s \n", buf);

      Request* req = create_req_struct(buf, new_fd);

	//SAMMYADDED param
      handle_req(req, vs, ts, info);
    }
	    
  }

  //to end this function, you can either call pthread_exit, or return NULL
  printf("now free() ing info, vs, and ts\n");
  free_info(info);
  free_vstruct(vs);
  free_tstruct(ts);  
  pthread_exit(NULL);
}

/**SAMMYADDED - changed params
   HANDLE_REQUEST - This is an umbrella catcher that later delegates again
*/
void handle_req(Request* req, Vstruct* vs, Tstruct* ts, Info* info) {
  printf("In handle_req \n");
  if (want_print_console) {
    printf("req->primary_cmd is %s\n", req->primary_cmd);
  }

  //HANDLE ERROR CASES HERE: FIRST CHECK IF BADLY FORMATTED STRING

  //set fields in setup
  if (strncasecmp(req->primary_cmd, "SETUP", 5) == 0) {
    //recieved setup request. should be in state 0.
    //time to initialize the fields
    if(initialize_fields(req, info)){
       handle_setup(req, vs, info);
    }else{
	if(want_print_console){
		printf("Looks like a setup request has been igonored");
	}
    }
  }
  else if (strncasecmp(req->primary_cmd, "PLAY", 4) == 0) {
 	if(check_consistent(req, info)){ //TODO sk to approve video_name null check
	    handle_play(req, vs, ts, info);
	}else{
		if(want_print_console){
			printf("Looks like a play request has been igonored");
		}
    	}	
  }
  else if (strncasecmp(req->primary_cmd, "PAUSE", 5) == 0) {
	if(check_consistent(req, info)){
	    handle_pause(req, vs, ts, info);
	}else{
		if(want_print_console){
			printf("Looks like a pause request has been igonored");
		}
    	}
  }
  else if (strncasecmp(req->primary_cmd, "TEARDOWN", 8) == 0) {
	if(check_consistent(req, info)){
	    handle_teardown(req, vs, ts, info);	
	}else{
		if(want_print_console){
			printf("Looks like a teardown request has been igonored\n");
		}
    	}
  }
  else {
	bad_format(BAD_PRIMARY);
  }

  return;
}

/**
   CHECK CONSISTENT - checks state maintained to see if next request is reasonable. Returns 0 if unreasonable, 1 if consistent
*/
int check_consistent(Request* req, Info* info){


  //FIRST CHECK IF THE MOVIE NAME IS THE SAME
  //TODO sammy pls approve: need to do  null check, else get segmentation fault
  if(info->video_name != NULL && strcmp(info->video_name, req->file_name) != 0){
	//send an error to close the socket
  	bad_format(NAME_MISMATCH);

	//now close it
	close(info->new_fd);
	exit(0);
  }

  //didnt make an else case cause its not required and looks messy
  if (strncasecmp(req->primary_cmd, "PLAY", 4) == 0) {

 	if((info->state == 0) || (info->state == -1)){
		//send 455 Method Not Allowed In This State
   		create_and_send_response("RTSP/1.0 455 Method Not Allowed in This State", req->c_seq, req->session, req->new_fd);
		printf("A play request has been recieved");
		printf(" without the a proper setup request. It will ");
		printf(" be ignored\n");
		return 0;	
	}else if(info->state == 1){
		if(want_print_console){
			printf("Good, play recieved in good state");	
		}
		return 1;
	}else if(info->state == 2){
    		//send 455 Method Not Allowed In This State
   		create_and_send_response("RTSP/1.0 455 Method Not Allowed in This State", req->c_seq, info->session, req->new_fd);
		printf("A play request has been recieved");
		printf(" during play. It will be ignored\n");
		return 0;			
	}else{
    		//send 455 Method Not Allowed In This State
   		create_and_send_response("RTSP/1.0 455 Method Not Allowed in This State", req->c_seq, info->session, req->new_fd);
		info->state = 0;
		printf("You should know your states out of wack.. not sure");
		return 1;	
	}
  }
  else if (strncasecmp(req->primary_cmd, "PAUSE", 5) == 0) {
	 if((info->state == 0) || (info->state == -1)){
    		//send 455 Method Not Allowed In This State
   		create_and_send_response("RTSP/1.0 455 Method Not Allowed in This State", req->c_seq, info->session, req->new_fd);
		printf("A pause request has been recieved");
		printf(" without the a proper setup request. It will ");
		printf(" be ignored\n");
		return 0;	
	}else if(info->state == 1){
    		//send 455 Method Not Allowed In This State
   		create_and_send_response("RTSP/1.0 455 Method Not Allowed in This State", req->c_seq, info->session, req->new_fd);
		printf("A pause request has been recieved");
		printf(" in ready state. It will be ignored\n");
		return 0;					
	}else if(info->state == 2){
		if(want_print_console){
			printf("Good, pause recieved in good state");	
		}
		return 1;		
	}else{
    		//send 455 Method Not Allowed In This State
   		create_and_send_response("RTSP/1.0 455 Method Not Allowed in This State", req->c_seq, info->session, req->new_fd);
		info->state = 1;
		printf("You should know your state out of wack");
		return 1;	
	}
  }
  else if (strncasecmp(req->primary_cmd, "TEARDOWN", 8) == 0) {
	if((info->state == 0) || (info->state == -1)){
    		//send 455 Method Not Allowed In This State
   		create_and_send_response("RTSP/1.0 455 Method Not Allowed in This State", req->c_seq, info->session, req->new_fd);
		printf("A teardow request has been recieved");
		printf(" without the a proper setup request. It will ");
		printf(" be ignored\n");
		return 0;	
	}else if((info->state == 1) || (info->state == 2)){ //TODO see note on handle_teardown re: seg fault (possible timer related) and info struct actions

		if(want_print_console){
			printf("Good, teardown recieved in good state");	
		}
		return 1;		
	}else{
    		//send 455 Method Not Allowed In This State
   		create_and_send_response("RTSP/1.0 455 Method Not Allowed in This State", req->c_seq, info->session, req->new_fd);
		info->state = 1;
		printf("You should know your states out of wack");
		return 1;	
	}
  }
  else {
   //bad format... return 0. inconsistent
    bad_format(BAD_PRIMARY);//Primary method BAD --> give error
    return 0;
  }

}

/**
   BAD FORMAT - the incoming request is not in a parsable form 

*/
void bad_format(char* error_message){
	printf("The incoming request was of an improper format, reason being:\n%s", error_message);

}

/**
   BAD FORMAT - the incoming request is not in a parsable form 

*/
void bad_format_2(char* error_message, int response_code, int is_fatal, Info* info){
	printf("The incoming request was of an improper format, reason being:\n%s", error_message);
    //should send out a response
    //DISCUSS SAMMYQ: really shouldn't be using their bad info to regurg back to them, but what about if the c_seq and session are uninitialized though??
    if(response_code == 400){
        create_and_send_response("RTSP/1.0 400 Bad Request", info->c_seq, info->session, info->new_fd);
    }else if(response_code == 455){
        create_and_send_response("RTSP/1.0 455 Method Not Valid in This State", info->c_seq, info->session, info->new_fd);
    }else if(response_code == 500){
        create_and_send_response("RTSP/1.0 500  Internal Server Error", info->c_seq, info->session, info->new_fd);
    }
    
    if(is_fatal){
        close(info->new_fd);
        exit(0);
        //shut down socket
    }
    
}

/**
  INITIALIZE FIELDS - Called on FIRST setup request TODOs: put this in header
*/
int initialize_fields(Request* req, Info* info){

	printf("info state is %d\n", info->state);

	// first check if its in the right state for a setup


//below commented out for SK to approve
// if((info->state != 0) || (info->state != -1)){ //TODO given this say, say I start wtih info = -1. I will stll be denied since the first clause evaluates to true; 


if (info->state != -1) {

	//in a weird state for setup
	printf("\n Presumably a SETUP request was just recieved in ");
	printf("the current session, therefore, this one will be ignored\n");

	//don't want to process setup request so return 0
	return 0;
 }else{
	//set fields	

	//NAME
	info->video_name = malloc((strlen(req->file_name)+1)*sizeof(char));
	strcpy(info->video_name, req->file_name); 

	//RTSP VERSION - but who really cares //TODO hardy likes this comment
	info->rtsp_ver = malloc((strlen(req->rtsp_ver) + 1)*sizeof(char));
	strcpy(info->rtsp_ver, req->rtsp_ver);

	//CSEQ
	info->c_seq = req->c_seq;

	//SESSION
	info->session = req->session;

	//..new FD ...? SAMMYQ
	info->new_fd = req->new_fd;

	//wont update state because haven't actually recieved request yet'
	//presumably successful enough to attempt setup
	return 1;
 }

}


/**
   HANDLE_SETUP - Handles SETUP request and sends response
*/
void handle_setup(Request* req, Vstruct* vs, Info* info) {
	    
  printf("In handle_setup\n");
  if (want_print_console) {
    print_req(req);
  }
	    
  int isGood = 1; //TODO HW TO SAMMY: I trimmed the easy isGood cases (where no code in ELSE), but leave this one here for you..

  if (isGood) {
	    
    //SAMMY CHECK:
    //random number for session only need to do this if setup is -1?
    if(info->state == -1){
    	srand(time(NULL));
    	int random_num = rand();
	info->session = random_num;
    }
	      
    //open the video
    vs->video = cvCaptureFromFile(req->file_name);
    if(vs->video) {
      printf("SUCCESS OPEN VIDEO\n");
    }
    if(!vs->video) {
      printf("FAIL OPEN VIDEO\n");
    } 

    //save socket_fd into vs SAMMY: can take this out, b/c replaced with Info?
    vs->socket_fd = req->new_fd;

    //send 200 OK
    create_and_send_response("RTSP/1.0 200 OK", req->c_seq, info->session, req->new_fd);
	      
    if (want_print_console) {
      printf("HEY, req->new_fd is %d, vs->socket_fd is %d\n", req->new_fd, vs->socket_fd);
      printf("just send 200 OK in handle_setup\n");
      printf("state is now (should be 1)...%d\n", info->state);
    }

    //setup successful, therefore update state
    info->state = 1;

  }
  else {
    //Write code here if request is NOT good
	//in this case, the state should return to 0, because the 
	//movie didn't open
	info->state = 0; //TODO check with SK: 0 or -1??
  }
}

/**
   HANDLE_PLAY - Handles PLAY request, sends response, starts play timer
*/
void handle_play(Request* req, Vstruct* vs, Tstruct* ts, Info* info) {
	
  printf("In handle_play\n");
  if (want_print_console) {
    print_req(req);
  }   
     
  create_and_send_response("RTSP/1.0 200 OK", req->c_seq, req->session, req->new_fd);
	    
  info->state = 2;
  start_my_timer(ts, vs, req->scale);
     
}

/**
   HANDLE_PAUSE - Handles PAUSE request, sends response, halts play timer
*/
void handle_pause(Request* req, Vstruct* vs, Tstruct *ts, Info* info) {
	
  printf("In handle_pause\n");
  if (want_print_console) {
    print_req(req);
  }        
	    
  create_and_send_response("RTSP/1.0 200 OK", req->c_seq, req->session, req->new_fd);
	    
  stop_my_timer(ts);
  info->state = 1;     
}

/**
   HANDLE_TEARDOWN - Handles TEARDOWN request, sends response, perform teardown
*/
void handle_teardown(Request* req, Vstruct* vs, Tstruct *ts, Info* info) {
	

	//TODO if we are to allow SETUP -> then TEARDOWN immediately
	//this code should not call stop_my_timer(ts) since timer never started (seg fault)
	//also, not sure if we should reset info struct??, since... are we allowing user to then
	//press SETUP again with another video? Or insist to still use same video?
	//as a simple fix (for now), we do NOT allow SETUP -> TEARDOWN, to allow all these compexities
	//leave for SK to re-architect, if needed

	//If too much a hassle, may also just leave it for now, and flag this as limitation


  printf("In handle_teardown\n");
  if (want_print_console) {
    print_req(req);
  }
  
   //stop the timer and reset the structs
   stop_my_timer(ts);
   memset(ts, '\0', sizeof(ts) );
   memset(vs, '\0', sizeof(vs) );
   
   //NEED TO RETAIN SOME INFO
   int session_n = info->session;
   int old_fd = info->new_fd;
   memset(info, '\0', sizeof(info) );
   info->state = 0; //TODO apparently upon teardown user cannot open another movie?
   info->session = session_n;
   info->new_fd = old_fd;

   //only send the OK response if it was succesfully closed
   create_and_send_response("RTSP/1.0 200 OK", req->c_seq, req->session, req->new_fd);

	
}


/**
   A minion function of handle_play
   START_TIMER - starts play timer which performs send_frame at regular interval
*/
void start_my_timer(Tstruct *ts, Vstruct *vs, int scale) {
	
	
  Send_frame_data *data = (Send_frame_data *)malloc(sizeof(Send_frame_data));
	
  if(!vs->video) {
    printf("In start_my_timer, VIDEO IS CORRUPT\n");
  }
	
  //set data's video to point to vs->video
  data->video = vs->video;
	
  //need to malloc for int, then set its value --- Took a while to get this fixed
  data->socket_fd = (int*)malloc(sizeof(int));
  *(data->socket_fd) = vs->socket_fd;
	
  //same for next_frame
  data->next_frame = (double *)malloc(sizeof(double));
  *(data->next_frame) = 0;
    
  //same for scale  
  data->scale = (int*)malloc(sizeof(int));
  *(data->scale) = scale;
	
  memset(&ts->play_event, 0, sizeof(ts->play_event));
  ts->play_event.sigev_notify = SIGEV_THREAD;
  ts->play_event.sigev_value.sival_ptr = data;  //Old approach in PDF is wrong
  ts->play_event.sigev_notify_function = send_frame;
	
	
  ts->play_interval.it_interval.tv_sec = 0;
  ts->play_interval.it_interval.tv_nsec = MS_PER_FRAME * 1000000;
	
  ts->play_interval.it_value.tv_sec = 0;
  ts->play_interval.it_value.tv_nsec = 1;
	
  timer_create(CLOCK_REALTIME, &ts->play_event, &ts->play_timer);
  timer_settime(ts->play_timer, 0, &ts->play_interval, NULL);
	
}

/**
   A minion function of handle_pause
   STOP_TIMER - stops play timer
*/
void stop_my_timer(Tstruct* ts) {
	
  ts->play_interval.it_interval.tv_sec = 0;
  ts->play_interval.it_interval.tv_nsec = 0;
	
  ts->play_interval.it_value.tv_sec = 0;
  ts->play_interval.it_value.tv_nsec = 0;
	
  timer_settime(ts->play_timer, 0, &ts->play_interval, NULL);
}



/**
   A minion function of start_timer
   SEND_FRAME - extract image from video, package into RTP packet, and send to client
*/
void send_frame(union sigval sv_data) {


  Send_frame_data *data = (Send_frame_data *) sv_data.sival_ptr;

  *(data->next_frame) = *(data->next_frame) + 1;

  if(data->video) {
    printf("VIDEO OK, socket fd: %d, next_frame: %f\n", *(data->socket_fd), *(data->next_frame));
  }


  IplImage *img = 0;
  int frame_skip;
  int desired_skip = *(data->scale);
  for (frame_skip = 0; frame_skip < desired_skip; frame_skip++) {
    img = cvQueryFrame(data->video);
  }

  if (want_print_console) {
    printf("IPL Image header fields: nSize: %d, width %d, imageSize %d, dataOrigPtr: %p\n"
	   ,img->nSize, img->width, img->imageSize, img->imageDataOrigin);
  }
	    
  //TODO revisit later
  //cvSetCaptureProperty(data->video, CV_CAP_PROP_POS_FRAMES, (double)*(data->next_frame));
	  
  //Convert the frame to a smaller size
  CvMat* thumb = cvCreateMat(img->height, img->width, CV_8UC3);
  cvResize(img, thumb, CV_INTER_AREA);

  //Encode in JPEG format with quality 30%
  const static int encodeParams[] = { CV_IMWRITE_JPEG_QUALITY, COMPRESSION_PERCENT };
  CvMat* encoded = cvEncodeImage(".jpeg", thumb, encodeParams);

  //After the call above, encoded data is in encoded->data.ptr and has length (in bytes) encoded->cols
	  
  int total_pkt_size = encoded->cols + PREFIX_NUM_BYTES + RTP_HEADER_NUM_BYTES;
  if (want_print_console) {
    printf("encoded->cols i.e. length in bytes is %d\n", encoded->cols);
    printf("total_pkt_size: %d bytes\n", total_pkt_size);
  }
	    
  //make RTP packet  
  unsigned char rtp_pkt[total_pkt_size];
  create_rtp_packet(rtp_pkt, encoded, data);  
	  
  //send the RTP packet to client
  send(*(data->socket_fd), rtp_pkt, total_pkt_size ,0);

	    
}

/**
   A minion function of send_frame
   CREATE_RTP_PACKET - make the RTP packet according to RFC specification
*/
void create_rtp_packet(unsigned char rtp_pkt[], CvMat* encoded, Send_frame_data *data) {
	    
  short payload_plus_header_size = encoded->cols + RTP_HEADER_NUM_BYTES;
	    
  /* First, do the 4-bytes prefix */
	    
  //BYTE-0 is '$'  
  rtp_pkt[0] = DOLLAR_SIGN_CHAR;
	    
  //BYTE-1 is 0  
  rtp_pkt[1] = (unsigned char)0;
	    
  //BYTES 2-3 is payload_plus_header_size in big endian
  short big_end_value = htons(payload_plus_header_size); //THANKS TO GENE
  memcpy(&rtp_pkt[2], &big_end_value, sizeof(short));
	    
	    
  /* Then build RTP header */
	    
  //BYTE-4 (first BYTE of RTP header)  
  int number_for_byte_4 = (RTP_HEADER_FIRST_4_BITS_AS_INT << 4) | CSRC_ID_DEFAULT_4_BITS_AS_INT;
  memcpy(&rtp_pkt[4], &number_for_byte_4, sizeof(unsigned char));
	    
	    
  //BYTE-5 (second BYTE of RTP header): 0, followed by 7-bit PT
  int number_26 = PAYLOAD_CODE_JPEG;
  memcpy(&rtp_pkt[5], &number_26, sizeof(unsigned char));
  //TODO hardy check: I think need htons
	    
	    
  //BYTE-6-7 is a short seq_num
  //We can use *(data->next_frame)
  short next_seq = *(data->next_frame);
  memcpy(&rtp_pkt[6], &next_seq, sizeof(short));
  //TODO hardy check: need htons
	    
  //BYTE-8-11 is timestamp
  //We can use next_frame * 40 ms
  int next_time_stamp = *(data->next_frame) * 40;
  memcpy(&rtp_pkt[8], &next_time_stamp, sizeof(int));
  //TODO hardy check: need htons
	    
  //BYTE-12-15 is SSRC identifier
  int my_num = 123;//TODO randomize
  memcpy(&rtp_pkt[12], &my_num, sizeof(int));
	    
	    
  /* Finally copy the data */  
	    
  memcpy(&rtp_pkt[16], encoded->data.ptr, encoded->cols * sizeof(unsigned char));
}




/**
   CREATE_REQUEST_STRUCT - Given RTSP message, makes a struct and returns struct pointer
*/
Request* create_req_struct(char buf[], int new_fd) {
	    
  char trimmed_buf[MAX_REQUEST_MSG_LEN_CHAR] = {'\0'};
  remove_newline_carriage(buf, trimmed_buf);
  char *pch = strtok(trimmed_buf, ": =\r\n");

  if (want_print_console) {
    printf("size of Request is %ld\n", sizeof(Request));
  }
	  
  Request * req = malloc(sizeof(*req));//HARDY REMOVED
  req->new_fd = new_fd;
  int counter = 0;
	    
  while (pch != NULL) {
    //    printf("%d, %s\n", counter, pch);
    if (counter == 0) {
      req->primary_cmd = malloc((strlen(pch) + 1 )* sizeof(char));//need to explicitly malloc it
      // (req->primary_cmd) = pch; //doing this still gives gibberish
      strcpy(req->primary_cmd, pch); //doing this works
    } 
    else if (counter == 1) {
      req->file_name = malloc((strlen(pch)+1)*sizeof(char));
      strcpy(req->file_name, pch);
    } 
    else if (counter == 2) {
      req->rtsp_ver = malloc((strlen(pch)+1)*sizeof(char));
      strcpy(req->rtsp_ver, pch);
    } 
    else {
	        
      //IMPORTANT! The \r\n in Client's msg is messing up the strtoken, hence we trimmed beforehand
	            
      if (strncasecmp(pch, "CSeq", 4) == 0) {
	pch = strtok(NULL, ": =");
	req->c_seq = atoi(pch);
      }
      if (strncasecmp(pch, "Transport", 9) == 0) {
	pch = strtok(NULL, ": =");

	req->transport = malloc((strlen(pch)+1)*sizeof(char));
	strcpy(req->transport, pch);
      }
      if (strncasecmp(pch, "interleaved", 11) == 0) {
	pch = strtok(NULL, ": =");
	req->interleaved = atoi(pch);
      }
      if (strncasecmp(pch, "Session", 7) == 0) {
	pch = strtok(NULL, ": =");
	req->session = atoi(pch);
      }
      if (strncasecmp(pch, "Scale", 5) == 0) {
	pch = strtok(NULL, ": =");
	req->scale = atoi(pch);
      }
    }
	        
    counter++;
    pch = strtok(NULL, ": =");
  }
  if (want_print_console) {
    printf("Now ready to return the created request struct..\n");
    printf("... printing req structure at end of create_struct method\n");
    print_req(req);
    printf("req_struct pointer addr is %p\n", req);
  }
  return req;

}

/**
   CREATE AND SEND RESPONSE - Creates Response struct and sends to client's socket fd
*/
void create_and_send_response(char* status_line, int c_seq, int session, int new_fd) {
	
  char response[MAX_RESPONSE_MSG_LEN_CHAR];
  sprintf(response,
	  "%s\r\n"
	  "CSeq: %d\r\n"
	  "Session: %d\r\n"
	  "\r\n",
	  status_line, c_seq, session);
	
  //TODO terminate with null char
  send(new_fd, response, strlen(response) ,0);
}


/**
   HELPER - Gets sockaddr, IPv4 or IPv6
*/
void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }
	    
  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/**
   HELPER - Because strtok in create_req_struct needs cr-lf to be removed
*/
void remove_newline_carriage(char* ori, char* res) {
  size_t len = strlen(ori);
  int ori_index = 0;
  int res_index = 0;
	    
  while (ori_index < len - 1) {
    if (ori[ori_index] == '\r') {
      ori_index++;
      res[res_index++] = ' ';
    }
    else if (ori[ori_index] == '\n') {
      ori_index++;
      res[res_index++] = ' ';
    }
    else {
      res[res_index++] = ori[ori_index++];
    }
  }
  res[res_index] = '\0';
	    
}

/**
   HELPER - Convenient printer to see Request message
*/
void print_req(Request* req) {

  printf("--Struct is:\n");
  if(req->primary_cmd) { printf("%s\n",req->primary_cmd); }
  if(req->file_name) { printf("%s\n",req->file_name); }
  if(req->rtsp_ver) { printf("%s\n",req->rtsp_ver); }
  if(req->c_seq) { printf("%d\n",req->c_seq); }
  if(req->session) { printf("%d\n",req->session); }
  printf("...reaches end of print_req\n");

  return;
}


/**
   ALL THESE FUNCTIONS ARE RECURSIVE FREE() FUNCTIONS
*/

void free_info(Info *info) {

  free(info->video_name);
  free(info->rtsp_ver);
  free(info);
}

void free_vstruct(Vstruct* vs) {

  //is there anything else to free?
  free(vs);
}

void free_tstruct(Tstruct* ts) {
  
  //is there anything else to free?
  free(ts);
}

void free_data(Send_frame_data *data) {

  free(data->socket_fd);
  free(data->next_frame);
  free(data->scale);
  free(data);
}

void free_req(Request *req) {

  free(req->primary_cmd);
  free(req->file_name);
  free(req->rtsp_ver);
  free(req->transport);
}




