#ifndef _CLOUD_HELPER_H_
#define _CLOUD_HELPER_H_

struct cloud_server {
    char *server;
    int port;
};

const struct cloud_server *get_cloud_server(const char *video_name, int frame_number);

#endif
