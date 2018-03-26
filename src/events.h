int connect_to_node(struct event_base *base, struct node_comm *comm, id_t peer_id);

void accept_conn_cb(struct evconnlistener *lev,
		evutil_socket_t sock, struct sockaddr *addr, int len, void *ptr);
void accept_error_cb(struct evconnlistener *lev, void *ptr);
void read_cb(struct bufferevent *bev, void *ctx);
void event_cb(struct bufferevent *bev, short events, void *ctx);
