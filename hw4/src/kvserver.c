#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include "kvconstants.h"
#include "kvstore.h"
#include "kvmessage.h"
#include "kvserver.h"
#include "index.h"
#include "tpclog.h"
#include "socket_server.h"

void do_log (kvserver_t *server);
char *parse_log(char *data, int len);

/* Initializes a kvserver. Will return 0 if successful, or a negative error
 * code if not. DIRNAME is the directory which should be used to store entries
 * for this server.  HOSTNAME and PORT indicate where SERVER will be
 * made available for requests. */
int kvserver_init(kvserver_t *server, char *dirname, unsigned int num_sets,
    unsigned int elem_per_set, unsigned int max_threads, const char *hostname,
    int port) {
  int ret;
  ret = kvstore_init(&server->store, dirname);
  if (ret < 0) return ret;
  ret = tpclog_init(&server->log, dirname);
  if (ret < 0) return ret;
  server->hostname = malloc(strlen(hostname) + 1);
  if (!server->hostname) fatal_malloc();
  strcpy(server->hostname, hostname);
  server->port = port;
  server->max_threads = max_threads;
  server->handle = kvserver_handle;

  server->state = TPC_INIT;

  /* Rebuild TPC state. */
  kvserver_rebuild_state(server);
  return 0;
}

/* Sends a message to register SERVER with a TPCLeader over a socket located at
 * SOCKFD which has previously been connected. Does not close the socket when
 * done. Returns -1 if an error was encountered.
 */
int kvserver_register_leader(kvserver_t *server, int sockfd) {
  kvrequest_t register_req;
  memset(&register_req, 0, sizeof(kvrequest_t));

  register_req.type = REGISTER;
  register_req.key = server->hostname;
  char port_string[6];
  sprintf(port_string, "%d", server->port);
  register_req.val = port_string;

  kvrequest_send(&register_req, sockfd);

  kvresponse_t *res = kvresponse_recieve(sockfd);
  int ret = (res && res->type == SUCCESS);
  kvresponse_free(res);
  return ret;
}

/* Attempts to get KEY from SERVER. Returns 0 if successful, else a negative
 * error code.  If successful, VALUE will point to a string which should later
 * be free()d.  */
int kvserver_get(kvserver_t *server, char *key, char **value) {
  int ret;
  if (strlen(key) > MAX_KEYLEN)
    return ERRKEYLEN;
  ret = kvstore_get(&server->store, key, value);
  return ret;
}

/* Checks if the given KEY, VALUE pair can be inserted into this server's
 * store. Returns 0 if it can, else a negative error code. */
int kvserver_put_check(kvserver_t *server, char *key, char *value) {
  int check;
  if (strlen(key) > MAX_KEYLEN || strlen(key) == 0)
    return ERRKEYLEN;
  if (strlen(value) > MAX_VALLEN)
    return ERRVALLEN;
  if ((check = kvstore_put_check(&server->store, key, value)) < 0)
    return check;
  return 0;
}

/* Inserts the given KEY, VALUE pair into this server's store
 * Returns 0 if successful, else a negative error code. */
int kvserver_put(kvserver_t *server, char *key, char *value) {
  int ret;
  if ((ret = kvserver_put_check(server, key, value)) < 0)
    return ret;
  ret = kvstore_put(&server->store, key, value);
  return ret;
}

/* Checks if the given KEY can be deleted from this server's store.
 * Returns 0 if it can, else a negative error code. */
int kvserver_del_check(kvserver_t *server, char *key) {
  int check;
  if (strlen(key) > MAX_KEYLEN || strlen(key) == 0)
    return ERRKEYLEN;
  if ((check = kvstore_del_check(&server->store, key)) < 0)
    return check;
  return 0;
}

/* Removes the given KEY from this server's store. Returns
 * 0 if successful, else a negative error code. */
int kvserver_del(kvserver_t *server, char *key) {
  int ret;
  if ((ret = kvserver_del_check(server, key)) < 0)
    return ret;
  ret = kvstore_del(&server->store, key);
  return ret;
}

/* Handles an incoming kvrequest REQ, and populates RES as a response.  REQ and
 * RES both must point to valid kvrequest_t and kvrespont_t structs,
 * respectively. Assumes that the request should be handled as a TPC
 * message. This should also log enough information in the server's TPC log to
 * be able to recreate the current state of the server upon recovering from
 * failure. See the spec for details on logic and error messages.
 */
 // i.e. read from req, and decide the proper behavior and revise
 // the res as the respond
 // only handle the PUTREQ, DELREQ, COMMIT, ABORT...
void kvserver_handle_tpc(kvserver_t *server, kvrequest_t *req, kvresponse_t *res) {
  /* TODO: Implement me! */
  printf("received at client(%s): %d, %s, %s\n", server->log.dirname, req->type, req->key, req->val);
  if (req->type == PUTREQ) {
    printf("the state is %d\n", server->state);
    if (server->state == TPC_INIT || server->state == TPC_WAIT) {
      int check_res = kvserver_put_check(server, req->key, req->val);
      if (check_res == 0) {
        // check the state
        // OK to put command into the log
        tpclog_log(&(server->log), req->type, req->key, req->val);
        res->type = VOTE;
        alloc_msg(res->body, MSG_COMMIT);
        server->state = TPC_READY;
      
      } else {
        res->type = VOTE;
        alloc_msg(res->body, GETMSG(check_res));
      }
    } else {
      res->type = ERROR;
      alloc_msg(res->body, ERRMSG_INVALID_REQUEST);
    }
  } else if (req->type == DELREQ) {
    if (server->state == TPC_INIT || server->state == TPC_WAIT) {
      int check_res = kvserver_del_check(server, req->key);
      res->type = VOTE;
      if (check_res == 0) {
        tpclog_log(&(server->log), req->type, req->key, NULL);
        alloc_msg(res->body, MSG_COMMIT);
        server->state = TPC_READY;
      } else {
        alloc_msg(res->body, GETMSG(check_res));
      }
    } else {
      res->type = ERROR;
      alloc_msg(res->body, ERRMSG_INVALID_REQUEST);
    }
  } else if (req->type == COMMIT) {
    if (server->state == TPC_READY) {
      tpclog_log(&(server->log), req->type, NULL, NULL);
      res->type = ACK;
      server->state = TPC_COMMIT;
      // we need to do the work from the log
      // do all the work
      do_log (server);
      tpclog_clear_log (&(server->log));
      server->state = TPC_WAIT;
    } else {
      res->type = ACK;
    }
    
  } else if (req->type == ABORT) {
    if (server->state == TPC_READY) {
      tpclog_log(&(server->log), req->type, NULL, NULL);
      res->type = ACK;
      server->state = TPC_ABORT;
      // we need to do the work from the log
      // drop all the log 
      tpclog_clear_log (&(server->log));
      server->state = TPC_WAIT;
    } else {
      res->type = ACK;
    }
    
  } else if (req->type == GETREQ) {
    // don't need to do log
    res->type = GETRESP;
    // the res->body's memory is allocated
    int get_res = kvserver_get(server, req->key, &(res->body));
    if (get_res != 0) {
      res->type = ERROR;
      alloc_msg(res->body, GETMSG(get_res));
    }
  }
  printf("send from client(%s): %d, %s\n", server->log.dirname, res->type, res->body);
  // res->type = ERROR;
  // alloc_msg(res->body, ERRMSG_NOT_IMPLEMENTED);
}

/* Generic entrypoint for this SERVER. Takes in a socket on SOCKFD, which
 * should already be connected to an incoming request. Processes the request
 * and sends back a response message.  This should call out to the appropriate
 * internal handler. */
void kvserver_handle(kvserver_t *server, int sockfd, void *extra) {
  (void)extra; // silence compiler
  kvrequest_t *req = kvrequest_recieve(sockfd);
  kvresponse_t *res = calloc(1, sizeof(kvresponse_t));
  if (!res) fatal_malloc();
  do {
    if (!req) {
      res->type = ERROR;
      alloc_msg(res->body, ERRMSG_INVALID_REQUEST);
    } else if (req->type == INDEX) {
      index_send(sockfd, 0);
      break;
    } else {
      kvserver_handle_tpc(server, req, res);
    }
    kvresponse_send(res, sockfd);
  } while (0);

  kvresponse_free(res);
  kvrequest_free(req);
}

/* Restore SERVER back to the state it should be in, according to the
 * associated LOG.  Must be called on an initialized  SERVER. Only restores the
 * state of the most recent TPC transaction, assuming that all previous actions
 * have been written to persistent storage. Should restore SERVER to its exact
 * state; e.g. if SERVER had written into its log that it received a PUTREQ but
 * no corresponding COMMIT/ABORT, after calling this function SERVER should
 * again be waiting for a COMMIT/ABORT.  This should also ensure that as soon
 * as a server logs a COMMIT, even if it crashes immediately after (before the
 * KVStore has a chance to write to disk), the COMMIT will be finished upon
 * rebuild.
 */
int kvserver_rebuild_state(kvserver_t *server) {
  /* TODO: Implement me! */
  // iterate though the logs to re-do the actions
  // we need to recover the state as well
  logentry_t *iter = NULL;
  tpclog_iterate_begin(&(server->log));
  while (tpclog_iterate_has_next(&(server->log))) {
    iter = tpclog_iterate_next(&(server->log));
    /**
    printf("Inside do log: %d, %s\n", iter->type, iter->data );
    if (iter->type == PUTREQ) {
      // do the put
      kvserver_put (server, iter->data, parse_log(iter->data, iter->length));

    } else if (iter->type == DELREQ) {
      kvserver_del (server, iter->data);
    } **/
  }
  if (iter != NULL) {
    if (iter->type == PUTREQ || iter->type == DELREQ) {
      server->state = TPC_READY;
    } else if (iter->type == COMMIT) {
      do_log(server);
      tpclog_clear_log (&(server->log));
      server->state = TPC_WAIT;
      // need to send the ack
    } else if (iter->type == ABORT) {
      tpclog_clear_log (&(server->log));
      server->state = TPC_WAIT;
      // need to send the ack
    }
  }
  return 0;
}

/* Deletes all current entries in SERVER's store and removes the store
 * directory.  Also cleans the associated log. Note that you will be required
 * to reinitialize SERVER following this action. */
int kvserver_clean(kvserver_t *server) {
  return kvstore_clean(&server->store);
}

// do all the command from the log

void do_log (kvserver_t *server) {
  logentry_t *iter = NULL;
  tpclog_iterate_begin(&(server->log));
  while (tpclog_iterate_has_next(&(server->log))) {
    iter = tpclog_iterate_next(&(server->log));
    printf("Inside do log: %d, %s\n", iter->type, iter->data );
    if (iter->type == PUTREQ) {
      // do the put
      kvserver_put (server, iter->data, parse_log(iter->data, iter->length));

    } else if (iter->type == DELREQ) {
      kvserver_del (server, iter->data);
    } 
  }
}

// parse the log data by \0;
// key \0 val \0
// return val start address
char *parse_log(char *data, int len) {
  int i = 0;
  for (; i < len; i++) {
    if (data[i] == 0) {
      return (data + i + 1);
    }
  }
  return NULL;
}