/* SPDX-License-Identifier: Apache-2.0 */

/*
 * examples/c/luw-echo-request.c - Example of writing a WASM module for use
 *				   with Unit using libunit-wasm
 *
 * Copyright (C) Andrew Clayton
 * Copyright (C) F5, Inc.
 */

#define _XOPEN_SOURCE	500

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "unit/unit-wasm.h"

static u8 *request_buf;

__luw_export_name("luw_module_end_handler")
void luw_module_end_handler(void)
{
	free(request_buf);
}

__luw_export_name("luw_module_init_handler")
void luw_module_init_handler(void)
{
	request_buf = malloc(luw_mem_get_init_size());
}

static bool hdr_iter_func(luw_ctx_t *ctx, const char *name, const char *value,
			  void *user_data __luw_unused)
{
	luw_mem_writep(ctx, "%s = %s\n", name, value);

	return true;
}

__luw_export_name("luw_request_handler")
int luw_request_handler(u8 *addr)
{
	luw_ctx_t ctx;
	char clen[32];
	const char *method;

	luw_init_ctx(&ctx, addr, 4096 /* Response offset */);
	/* Take a copy of the request and use that */
	luw_set_req_buf(&ctx, &request_buf, LUW_SRB_NONE);

#define BUF_ADD(fmt, member) \
	luw_mem_writep(&ctx, fmt, luw_get_http_##member(&ctx));

	luw_mem_writep(&ctx,
		       " *** Welcome to WebAssembly on Unit! "
		       "[libunit-wasm (%d.%d.%d/%#0.8x)] ***\n\n",
		       LUW_VERSION_MAJOR, LUW_VERSION_MINOR, LUW_VERSION_PATCH,
		       LUW_VERSION_NUMBER);

	luw_mem_writep(&ctx, "[Request Info]\n");
	BUF_ADD("REQUEST_PATH = %s\n", path);
	BUF_ADD("METHOD       = %s\n", method);
	BUF_ADD("VERSION      = %s\n", version);
	BUF_ADD("QUERY        = %s\n", query);
	BUF_ADD("REMOTE       = %s\n", remote);
	BUF_ADD("LOCAL_ADDR   = %s\n", local_addr);
	BUF_ADD("LOCAL_PORT   = %s\n", local_port);
	BUF_ADD("SERVER_NAME  = %s\n", server_name);

	luw_mem_writep(&ctx, "\n[Request Headers]\n");

	luw_http_hdr_iter(&ctx, hdr_iter_func, NULL);

	method = luw_get_http_method(&ctx);
	if (memcmp(method, "POST", strlen(method)) == 0 ||
	    memcmp(method, "PUT", strlen(method)) == 0) {
		luw_mem_writep(&ctx, "\n[%s data]\n", method);
		luw_mem_writep_data(&ctx, luw_get_http_content(&ctx),
				    luw_get_http_content_len(&ctx));
		luw_mem_writep(&ctx, "\n");
	}

	luw_http_init_headers(&ctx, 2, 0);

	snprintf(clen, sizeof(clen), "%lu", luw_get_response_data_size(&ctx));
	luw_http_add_header(&ctx, "Content-Type", "text/plain");
	luw_http_add_header(&ctx, "Content-Length", clen);

	luw_http_send_headers(&ctx);

	luw_http_send_response(&ctx);
	/* Tell Unit no more data to send */
	luw_http_response_end();

	return 0;
}
