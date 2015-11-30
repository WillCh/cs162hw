#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include "kvconstants.h"
#include "kvmessage.h"
#include "index.h"
#include "md5.h"
#include "socket_server.h"
#include "time.h"
#include "tpcleader.h"

#define TIMEOUT 5

/* Initializes a tpcleader. Will return 0 if successful, or a negative error
 * code if not. FOLLOWER_CAPACITY indicates the maximum number of followers that
 * the leader will support. REDUNDANCY is the number of replicas (followers) that
 * each key will be stored in. */
int tpcleader_init(tpcleader_t *leader, unsigned int follower_capacity,
    unsigned int redundancy) {
  int ret;
  ret = pthread_rwlock_init(&leader->follower_lock, NULL);
  if (ret < 0) return ret;

  leader->follower_count = 0;
  leader->follower_capacity = follower_capacity;
  if (redundancy > follower_capacity) {
    leader->redundancy = follower_capacity;
  } else {
    leader->redundancy = redundancy;
  }
  leader->followers_head = NULL;
  leader->handle = tpcleader_handle;
  return 0;
}

/* Handles an incoming kvrequest REQ, and populates RES as a response. REQ and
 * RES both must point to valid kvrequest_t and kvrespont_t structs,
 * respectively. Assigns an ID to the follower by hashing a string in the format
 * PORT:HOSTNAME, then tries to add its info to the LEADER's list of followers. If
 * the follower is already in the list, do nothing (success).  There can never be
 * more followers than the LEADER's follower_capacity.  RES will be a SUCCESS if
 * registration succeeds, or an error otherwise.
 */
void tpcleader_register(tpcleader_t *leader, kvrequest_t *req, kvresponse_t *res) {
  if (leader->follower_count == leader->follower_capacity) {
    res->type = ERROR;
    alloc_msg(res->body, ERRMSG_FOLLOWER_CAPACITY);
    return;
  }

  tpcfollower_t *new_follower = malloc(sizeof(tpcfollower_t));
  if (!new_follower) fatal_malloc();

  new_follower->host = malloc(strlen(req->key) + 1);
  if (!new_follower->host) fatal_malloc();
  strcpy(new_follower->host, req->key);
  new_follower->port = atoi(req->val);
  char address[strlen(new_follower->host) + strlen(req->val)];
  sprintf(address, "%s:%s", req->val, new_follower->host);
  new_follower->id = strhash64(address);
  new_follower->prev = new_follower;
  new_follower->next = new_follower;

  res->type = SUCCESS;
  pthread_rwlock_wrlock(&leader->follower_lock);
  if (!leader->followers_head) {
    leader->followers_head = new_follower;
    leader->follower_count++;
    goto end;
  }
  tpcfollower_t *first_follower = leader->followers_head;
  tpcfollower_t *curr_follower = first_follower;
  do {
    if (curr_follower->id > new_follower->id) {
      new_follower->next = curr_follower;
      new_follower->prev = curr_follower->prev;
      curr_follower->prev = new_follower;
      new_follower->prev->next = new_follower;
      if (curr_follower == first_follower)
        leader->followers_head = new_follower;
      leader->follower_count++;
      goto end;
    } else if (curr_follower->id == new_follower->id) {
      goto end;
    }
    curr_follower = curr_follower->next;
  } while (curr_follower != first_follower);
  /* New follower has highest ID. */
  new_follower->next = leader->followers_head;
  new_follower->prev = leader->followers_head->prev;
  leader->followers_head->prev = new_follower;
  new_follower->prev->next = new_follower;
  leader->follower_count++;

end:
  pthread_rwlock_unlock(&leader->follower_lock);
  return;
}

/* Hashes KEY and finds the first follower that should contain it.
 * It should return the first follower whose ID is greater than the
 * KEY's hash, and the one with lowest ID if none matches the
 * requirement. Returns NULL if we're not yet at our follower capacity.
 */
tpcfollower_t *tpcleader_get_primary(tpcleader_t *leader, char *key) {
  if (leader->follower_count < leader->follower_capacity)
    return NULL;
  uint64_t hash = strhash64(key);
  pthread_rwlock_wrlock(&leader->follower_lock);
  tpcfollower_t *curr_follower = leader->followers_head;
  do {
    if (curr_follower->id > hash) {
      pthread_rwlock_unlock(&leader->follower_lock);
      return curr_follower;
    }
    curr_follower = curr_follower->next;
  } while (curr_follower != leader->followers_head);
  curr_follower = leader->followers_head;
  pthread_rwlock_unlock(&leader->follower_lock);
  return curr_follower;
}

/* Returns the follower whose ID comes after PREDECESSOR's, sorted
 * in increasing order.
 */
tpcfollower_t *tpcleader_get_successor(tpcleader_t *leader,
    tpcfollower_t *predecessor) {
  pthread_rwlock_wrlock(&leader->follower_lock);
  tpcfollower_t *result = predecessor->next;
  pthread_rwlock_unlock(&leader->follower_lock);
  return result;
}

/* Handles an incoming GET request REQ, and populates response RES. REQ and
 * RES both must point to valid kvrequest_t and kvrespont_t structs,
 * respectively.
 */
void tpcleader_handle_get(tpcleader_t *leader, kvrequest_t *req, kvresponse_t *res) {
  /* TODO: Implement me! */
  // find the correct follower to send the res
  // it need multiple threads?
  printf("inside the handle get: %d, %s\n", req->type, req->key);
  tpcfollower_t *pri = tpcleader_get_primary(leader, req->key);
  bool succeed = false;
  int visited_node = 0;
  while (pri != NULL && visited_node < leader->redundancy) {
    int socketid = connect_to(pri->host, pri->port, TIMEOUT);
    kvrequest_send(req, socketid);
    kvresponse_t *restmp = kvresponse_recieve(socketid);
    printf("received: %d, %s\n", restmp->type, restmp->body);
    visited_node++;
    if (restmp != NULL && restmp->type == GETRESP) {
      // copy the received rest to the res
      res->type = GETRESP;
      alloc_msg(res->body, restmp->body);
      kvresponse_free(restmp);
      close(socketid);
      succeed = true;    
      break;
    } else {
      // communicate with second one
      pri = tpcleader_get_successor(leader, pri);
      if (restmp != NULL) {
        kvresponse_free(restmp);
      }
      close(socketid);
    }
  }
  if (!succeed) {
    res->type = ERROR;
    alloc_msg(res->body, ERRMSG_NO_KEY);
  } 
}

/* Handles an incoming TPC request REQ, and populates RES as a response.
 * REQ and RES both must point to valid kvrequest_t and kvrespont_t structs,
 * respectively.
 *
 * Implements the TPC algorithm, polling all the followers for a vote first and
 * sending a COMMIT or ABORT message in the second phase.  Must wait for an ACK
 * from every follower after sending the second phase messages.
 */
void tpcleader_handle_tpc(tpcleader_t *leader, kvrequest_t *req, kvresponse_t *res) {
  if (leader->follower_count != leader->follower_capacity) {
    res->type = ERROR;
    alloc_msg(res->body, ERRMSG_NOT_AT_CAPACITY);
    return;
  }
  /* TODO: Implement me! */
  if (req->type == PUTREQ || req->type == DELREQ) {
    printf("inside handle tpc\n");

    tpcfollower_t *pri = tpcleader_get_primary(leader, req->key);
    int visited_node = 0;
    bool is_commit = true;
    while (pri != NULL && visited_node < leader->redundancy) {
      int socktmpfd = -1;
      // while((socktmpfd = connect_to(pri->host, pri->port, TIMEOUT)) == -1);
      socktmpfd = connect_to(pri->host, pri->port, TIMEOUT);
      printf("the follower is %s, %d\n", pri->host, pri->port);
      if (socktmpfd == -1) {
        is_commit = false;
      } else {
        // send the req to it
        printf("send to %d\n", visited_node);
        kvrequest_send(req, socktmpfd);
        kvresponse_t *restmp = kvresponse_recieve(socktmpfd);
        if (!(restmp != NULL && !strcmp(restmp->body, "commit"))) {
          is_commit = false;
        }
        kvresponse_free(restmp);
        close(socktmpfd);
      }
      
      pri = tpcleader_get_successor(leader, pri);
      visited_node++;
    }
    
    kvrequest_t *res_client = calloc(1, sizeof(kvrequest_t));
    if (!is_commit) {
      // abort
      res_client->type = ABORT;
      res->type = ERROR;
      alloc_msg(res->body, "abort the command");
      printf("abort\n");
    } else {
      // commit
      res_client->type = COMMIT;
      res->type = SUCCESS;
      printf("succeed\n");
    }
    // send the message
    // printf("sleep for 10sec\n");
    // sleep (20);
    bool is_all_acked = true;
    do {
      printf("inside do while\n");
      is_all_acked = true;
      visited_node = 0;
      pri = tpcleader_get_primary(leader, req->key);
      while (pri != NULL && visited_node < leader->redundancy) {      
        int socktmpfd = -1;
        printf("inside innerr while: %d\n", visited_node);
        printf("the node is: %s, %d\n", pri->host, pri->port);
        // while((socktmpfd = connect_to(pri->host, pri->port, TIMEOUT)) == -1);
        socktmpfd = connect_to(pri->host, pri->port, TIMEOUT);
        printf("the socktmpfd is %d\n", socktmpfd);
        if (socktmpfd == -1) {
          is_all_acked = false;
        } else {
          kvrequest_send(res_client, socktmpfd);
          printf("iter: %d, %d\n", visited_node, pri->port);
          kvresponse_t *restmp = kvresponse_recieve(socktmpfd);
          if (!(restmp != NULL && restmp->type == ACK)) {
            is_all_acked = false;
            printf("unacked\n");
          }
          kvresponse_free(restmp);
          close(socktmpfd);
        }
        

        pri = tpcleader_get_successor(leader, pri);
        visited_node++;
      }
    } while (!is_all_acked);
    
    
    printf("finished ack\n");
    kvrequest_free(res_client);
  } else {
    res->type = ERROR;
    alloc_msg(res->body, "unknown message type");
  }
}


/* Generic entrypoint for this LEADER. Takes in a socket on SOCKFD, which
 * should already be connected to an incoming request. Processes the request
 * and sends back a response message.  This should call out to the appropriate
 * internal handler. */
void tpcleader_handle(tpcleader_t *leader, int sockfd) {
  kvresponse_t *res = calloc(1, sizeof(kvresponse_t));

  kvrequest_t *req;
  req = kvrequest_recieve(sockfd);
  //
  printf("get a message: %d, %s, %s\n", req->type, req->key, req->val);
  do {
    if (!req) {
      res->type = ERROR;
      alloc_msg(res->body, ERRMSG_INVALID_REQUEST);
    } else if (req->type == INDEX) {
      index_send(sockfd, 1);
      break;
    } else if (req->type == REGISTER) {
      tpcleader_register(leader, req, res);
    } else if (req->type == GETREQ) {
      tpcleader_handle_get(leader, req, res);
    } else {
      tpcleader_handle_tpc(leader, req, res);
    }
    kvresponse_send(res, sockfd);
  } while (0);

  kvrequest_free(req);
  kvresponse_free(res);
}
