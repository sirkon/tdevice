#ifndef _BINDER_H_
#define _BINDER_H_

#include "confparser.h"
#include "maplib.h"


typedef struct {
    enum {
        WORKER_STATUS_SUCCESS,
        WORKER_STATUS_ERROR } status;
    char result[32];
} work_result_t;


typedef work_result_t *(*handler_t)(map_t *params);


typedef struct {
    map_t *params;
    char *name;
    types_t ret_type;
    handler_t handler;
} handler_data_t;


typedef struct {
    map_t *params;
    char *name;
} request_t;


typedef enum {
    RESULT_SUCCESS,
    RESULT_METHOD_NOT_ALLOWED,
    RESULT_WRONG_PARAMETER,
} result_status_t;


typedef struct {
    enum {
        CALL_STATUS_SUCCESS,
        CALL_STATUS_INTERNAL_ERROR,
        CALL_STATUS_ERROR } status;
    char value[32];
    char *message;
} call_handler_result_t;


#define RESULT_SUCCESS(result, var, type)                               \
    result->status = WORKER_STATUS_SUCCESS, memcpy((void*)result->result, (void*)&var, sizeof(type)), result


#define RESULT_ERROR(result)                            \
    result->status = WORKER_STATUS_ERROR, result


void handler_bind(map_t *handler_map, char *handler_name, handler_t handler, map_t *rules);
call_handler_result_t *handler_call(map_t *handler_map, request_t *request);

#endif