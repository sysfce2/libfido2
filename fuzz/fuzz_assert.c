/*
 * Copyright (c) 2019 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "mutator_aux.h"
#include "fido.h"
#include "fido/es256.h"
#include "fido/rs256.h"
#include "fido/eddsa.h"

#include "../openbsd-compat/openbsd-compat.h"

#define TAG_U2F		0x01
#define TAG_TYPE	0x02
#define TAG_CDH		0x03
#define TAG_RP_ID	0x04
#define TAG_EXT		0x05
#define TAG_SEED	0x06
#define TAG_UP		0x07
#define TAG_UV		0x08
#define TAG_WIRE_DATA	0x09
#define TAG_CRED_COUNT	0x0a
#define TAG_CRED	0x0b
#define TAG_ES256	0x0c
#define TAG_RS256	0x0d
#define TAG_PIN		0x0e
#define TAG_EDDSA	0x0f

/* Parameter set defining a FIDO2 get assertion operation. */
struct param {
	char		pin[MAXSTR];
	char		rp_id[MAXSTR];
	int		ext;
	int		seed;
	struct blob	cdh;
	struct blob	cred;
	struct blob	es256;
	struct blob	rs256;
	struct blob	eddsa;
	struct blob	wire_data;
	uint8_t		cred_count;
	uint8_t		type;
	uint8_t		u2f;
	uint8_t		up;
	uint8_t		uv;
};

/* Example parameters. */
static const char dummy_rp_id[] = "localhost";
static const char dummy_pin[] = "9}4gT:8d=A37Dh}U";

static const uint8_t dummy_cdh[] = {
	0xec, 0x8d, 0x8f, 0x78, 0x42, 0x4a, 0x2b, 0xb7,
	0x82, 0x34, 0xaa, 0xca, 0x07, 0xa1, 0xf6, 0x56,
	0x42, 0x1c, 0xb6, 0xf6, 0xb3, 0x00, 0x86, 0x52,
	0x35, 0x2d, 0xa2, 0x62, 0x4a, 0xbe, 0x89, 0x76,
};

static const uint8_t dummy_es256[] = {
	0xcc, 0x1b, 0x50, 0xac, 0xc4, 0x19, 0xf8, 0x3a,
	0xee, 0x0a, 0x77, 0xd6, 0xf3, 0x53, 0xdb, 0xef,
	0xf2, 0xb9, 0x5c, 0x2d, 0x8b, 0x1e, 0x52, 0x58,
	0x88, 0xf4, 0x0b, 0x85, 0x1f, 0x40, 0x6d, 0x18,
	0x15, 0xb3, 0xcc, 0x25, 0x7c, 0x38, 0x3d, 0xec,
	0xdf, 0xad, 0xbd, 0x46, 0x91, 0xc3, 0xac, 0x30,
	0x94, 0x2a, 0xf7, 0x78, 0x35, 0x70, 0x59, 0x6f,
	0x28, 0xcb, 0x8e, 0x07, 0x85, 0xb5, 0x91, 0x96,
};

static const uint8_t dummy_rs256[] = {
	0xd2, 0xa8, 0xc0, 0x11, 0x82, 0x9e, 0x57, 0x2e,
	0x60, 0xae, 0x8c, 0xb0, 0x09, 0xe1, 0x58, 0x2b,
	0x99, 0xec, 0xc3, 0x11, 0x1b, 0xef, 0x81, 0x49,
	0x34, 0x53, 0x6a, 0x01, 0x65, 0x2c, 0x24, 0x09,
	0x30, 0x87, 0x98, 0x51, 0x6e, 0x30, 0x4f, 0x60,
	0xbd, 0x54, 0xd2, 0x54, 0xbd, 0x94, 0x42, 0xdd,
	0x63, 0xe5, 0x2c, 0xc6, 0x04, 0x32, 0xc0, 0x8f,
	0x72, 0xd5, 0xb4, 0xf0, 0x4f, 0x42, 0xe5, 0xb0,
	0xa2, 0x95, 0x11, 0xfe, 0xd8, 0xb0, 0x65, 0x34,
	0xff, 0xfb, 0x44, 0x97, 0x52, 0xfc, 0x67, 0x23,
	0x0b, 0xad, 0xf3, 0x3a, 0x82, 0xd4, 0x96, 0x10,
	0x87, 0x6b, 0xfa, 0xd6, 0x51, 0x60, 0x3e, 0x1c,
	0xae, 0x19, 0xb8, 0xce, 0x08, 0xae, 0x9a, 0xee,
	0x78, 0x16, 0x22, 0xcc, 0x92, 0xcb, 0xa8, 0x95,
	0x34, 0xe5, 0xb9, 0x42, 0x6a, 0xf0, 0x2e, 0x82,
	0x1f, 0x4c, 0x7d, 0x84, 0x94, 0x68, 0x7b, 0x97,
	0x2b, 0xf7, 0x7d, 0x67, 0x83, 0xbb, 0xc7, 0x8a,
	0x31, 0x5a, 0xf3, 0x2a, 0x95, 0xdf, 0x63, 0xe7,
	0x4e, 0xee, 0x26, 0xda, 0x87, 0x00, 0xe2, 0x23,
	0x4a, 0x33, 0x9a, 0xa0, 0x1b, 0xce, 0x60, 0x1f,
	0x98, 0xa1, 0xb0, 0xdb, 0xbf, 0x20, 0x59, 0x27,
	0xf2, 0x06, 0xd9, 0xbe, 0x37, 0xa4, 0x03, 0x6b,
	0x6a, 0x4e, 0xaf, 0x22, 0x68, 0xf3, 0xff, 0x28,
	0x59, 0x05, 0xc9, 0xf1, 0x28, 0xf4, 0xbb, 0x35,
	0xe0, 0xc2, 0x68, 0xc2, 0xaa, 0x54, 0xac, 0x8c,
	0xc1, 0x69, 0x9e, 0x4b, 0x32, 0xfc, 0x53, 0x58,
	0x85, 0x7d, 0x3f, 0x51, 0xd1, 0xc9, 0x03, 0x02,
	0x13, 0x61, 0x62, 0xda, 0xf8, 0xfe, 0x3e, 0xc8,
	0x95, 0x12, 0xfb, 0x0c, 0xdf, 0x06, 0x65, 0x6f,
	0x23, 0xc7, 0x83, 0x7c, 0x50, 0x2d, 0x27, 0x25,
	0x4d, 0xbf, 0x94, 0xf0, 0x89, 0x04, 0xb9, 0x2d,
	0xc4, 0xa5, 0x32, 0xa9, 0x25, 0x0a, 0x99, 0x59,
	0x01, 0x00, 0x01,
};

static const uint8_t dummy_eddsa[] = {
	0xfe, 0x8b, 0x61, 0x50, 0x31, 0x7a, 0xe6, 0xdf,
	0xb1, 0x04, 0x9d, 0x4d, 0xb5, 0x7a, 0x5e, 0x96,
	0x4c, 0xb2, 0xf9, 0x5f, 0x72, 0x47, 0xb5, 0x18,
	0xe2, 0x39, 0xdf, 0x2f, 0x87, 0x19, 0xb3, 0x02,
};

/*
 * Collection of HID reports from an authenticator issued with a FIDO2
 * get assertion using the example parameters above.
 */
static const uint8_t dummy_wire_data_fido[] = {
	0xff, 0xff, 0xff, 0xff, 0x86, 0x00, 0x11, 0xf7,
	0x6f, 0xda, 0x52, 0xfd, 0xcb, 0xb6, 0x24, 0x00,
	0x92, 0x00, 0x0e, 0x02, 0x05, 0x00, 0x02, 0x05,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x92, 0x00, 0x0e, 0x90, 0x00, 0x51, 0x00,
	0xa1, 0x01, 0xa5, 0x01, 0x02, 0x03, 0x38, 0x18,
	0x20, 0x01, 0x21, 0x58, 0x20, 0xe9, 0x1d, 0x9b,
	0xac, 0x14, 0x25, 0x5f, 0xda, 0x1e, 0x11, 0xdb,
	0xae, 0xc2, 0x90, 0x22, 0xca, 0x32, 0xec, 0x32,
	0xe6, 0x05, 0x15, 0x44, 0xe5, 0xe8, 0xbc, 0x4f,
	0x0a, 0xb6, 0x1a, 0xeb, 0x11, 0x22, 0x58, 0x20,
	0xcc, 0x72, 0xf0, 0x22, 0xe8, 0x28, 0x82, 0xc5,
	0x00, 0x92, 0x00, 0x0e, 0x00, 0xa6, 0x65, 0x6e,
	0xff, 0x1e, 0xe3, 0x7f, 0x27, 0x44, 0x2d, 0xfb,
	0x8d, 0x41, 0xfa, 0x85, 0x0e, 0xcb, 0xda, 0x95,
	0x64, 0x64, 0x9b, 0x1f, 0x34, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x92, 0x00, 0x0e, 0x90, 0x00, 0x14, 0x00,
	0xa1, 0x02, 0x50, 0xee, 0x40, 0x4c, 0x85, 0xd7,
	0xa1, 0x2f, 0x56, 0xc4, 0x4e, 0xc5, 0x93, 0x41,
	0xd0, 0x3b, 0x23, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x92, 0x00, 0x0e, 0x90, 0x00, 0xcb, 0x00,
	0xa3, 0x01, 0xa2, 0x62, 0x69, 0x64, 0x58, 0x40,
	0x4a, 0x4c, 0x9e, 0xcc, 0x81, 0x7d, 0x42, 0x03,
	0x2b, 0x41, 0xd1, 0x38, 0xd3, 0x49, 0xb4, 0xfc,
	0xfb, 0xe4, 0x4e, 0xe4, 0xff, 0x76, 0x34, 0x16,
	0x68, 0x06, 0x9d, 0xa6, 0x01, 0x32, 0xb9, 0xff,
	0xc2, 0x35, 0x0d, 0x89, 0x43, 0x66, 0x12, 0xf8,
	0x8e, 0x5b, 0xde, 0xf4, 0xcc, 0xec, 0x9d, 0x03,
	0x00, 0x92, 0x00, 0x0e, 0x00, 0x85, 0xc2, 0xf5,
	0xe6, 0x8e, 0xeb, 0x3f, 0x3a, 0xec, 0xc3, 0x1d,
	0x04, 0x6e, 0xf3, 0x5b, 0x88, 0x64, 0x74, 0x79,
	0x70, 0x65, 0x6a, 0x70, 0x75, 0x62, 0x6c, 0x69,
	0x63, 0x2d, 0x6b, 0x65, 0x79, 0x02, 0x58, 0x25,
	0x49, 0x96, 0x0d, 0xe5, 0x88, 0x0e, 0x8c, 0x68,
	0x74, 0x34, 0x17, 0x0f, 0x64, 0x76, 0x60, 0x5b,
	0x8f, 0xe4, 0xae, 0xb9, 0xa2, 0x86, 0x32, 0xc7,
	0x00, 0x92, 0x00, 0x0e, 0x01, 0x99, 0x5c, 0xf3,
	0xba, 0x83, 0x1d, 0x97, 0x63, 0x04, 0x00, 0x00,
	0x00, 0x09, 0x03, 0x58, 0x47, 0x30, 0x45, 0x02,
	0x21, 0x00, 0xcf, 0x3f, 0x36, 0x0e, 0x1f, 0x6f,
	0xd6, 0xa0, 0x9d, 0x13, 0xcf, 0x55, 0xf7, 0x49,
	0x8f, 0xc8, 0xc9, 0x03, 0x12, 0x76, 0x41, 0x75,
	0x7b, 0xb5, 0x0a, 0x90, 0xa5, 0x82, 0x26, 0xf1,
	0x6b, 0x80, 0x02, 0x20, 0x34, 0x9b, 0x7a, 0x82,
	0x00, 0x92, 0x00, 0x0e, 0x02, 0xd3, 0xe1, 0x79,
	0x49, 0x55, 0x41, 0x9f, 0xa4, 0x06, 0x06, 0xbd,
	0xc8, 0xb9, 0x2b, 0x5f, 0xe1, 0xa7, 0x99, 0x1c,
	0xa1, 0xfc, 0x7e, 0x3e, 0xd5, 0x85, 0x2e, 0x11,
	0x75, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/*
 * Collection of HID reports from an authenticator issued with a U2F
 * authentication using the example parameters above.
 */
static const uint8_t dummy_wire_data_u2f[] = {
	0xff, 0xff, 0xff, 0xff, 0x86, 0x00, 0x11, 0x0f,
	0x26, 0x9c, 0xd3, 0x87, 0x0d, 0x7b, 0xf6, 0x00,
	0x00, 0x99, 0x01, 0x02, 0x01, 0x01, 0x00, 0x01,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x99, 0x01, 0x83, 0x00, 0x02, 0x69,
	0x85, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x99, 0x01, 0x83, 0x00, 0x02, 0x69,
	0x85, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x99, 0x01, 0x83, 0x00, 0x02, 0x69,
	0x85, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x99, 0x01, 0x83, 0x00, 0x02, 0x69,
	0x85, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x99, 0x01, 0x83, 0x00, 0x02, 0x69,
	0x85, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x99, 0x01, 0x83, 0x00, 0x02, 0x69,
	0x85, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x99, 0x01, 0x83, 0x00, 0x02, 0x69,
	0x85, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x99, 0x01, 0x83, 0x00, 0x02, 0x69,
	0x85, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x99, 0x01, 0x83, 0x00, 0x02, 0x69,
	0x85, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x99, 0x01, 0x83, 0x00, 0x4e, 0x01,
	0x00, 0x00, 0x00, 0x2c, 0x30, 0x45, 0x02, 0x20,
	0x1c, 0xf5, 0x7c, 0xf6, 0xde, 0xbe, 0xe9, 0x86,
	0xee, 0x97, 0xb7, 0x64, 0xa3, 0x4e, 0x7a, 0x70,
	0x85, 0xd0, 0x66, 0xf9, 0xf0, 0xcd, 0x04, 0x5d,
	0x97, 0xf2, 0x3c, 0x22, 0xe3, 0x0e, 0x61, 0xc8,
	0x02, 0x21, 0x00, 0x97, 0xef, 0xae, 0x36, 0xe6,
	0x17, 0x9f, 0x5e, 0x2d, 0xd7, 0x8c, 0x34, 0xa7,
	0x00, 0x00, 0x99, 0x01, 0x00, 0xa1, 0xe9, 0xfb,
	0x8f, 0x86, 0x8c, 0xe3, 0x1e, 0xde, 0x3f, 0x4e,
	0x1b, 0xe1, 0x2f, 0x8f, 0x2f, 0xca, 0x42, 0x26,
	0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

int    LLVMFuzzerTestOneInput(const uint8_t *, size_t);
size_t LLVMFuzzerCustomMutator(uint8_t *, size_t, size_t, unsigned int);

static int
unpack(const uint8_t *ptr, size_t len, struct param *p) NO_MSAN
{
	uint8_t **pp = (void *)&ptr;

	if (unpack_byte(TAG_UV, pp, &len, &p->uv) < 0 ||
	    unpack_byte(TAG_UP, pp, &len, &p->up) < 0 ||
	    unpack_byte(TAG_U2F, pp, &len, &p->u2f) < 0 ||
	    unpack_byte(TAG_TYPE, pp, &len, &p->type) < 0 ||
	    unpack_byte(TAG_CRED_COUNT, pp, &len, &p->cred_count) < 0 ||
	    unpack_int(TAG_EXT, pp, &len, &p->ext) < 0 ||
	    unpack_int(TAG_SEED, pp, &len, &p->seed) < 0 ||
	    unpack_string(TAG_RP_ID, pp, &len, p->rp_id) < 0 ||
	    unpack_string(TAG_PIN, pp, &len, p->pin) < 0 ||
	    unpack_blob(TAG_WIRE_DATA, pp, &len, &p->wire_data) < 0 ||
	    unpack_blob(TAG_RS256, pp, &len, &p->rs256) < 0 ||
	    unpack_blob(TAG_ES256, pp, &len, &p->es256) < 0 ||
	    unpack_blob(TAG_EDDSA, pp, &len, &p->eddsa) < 0 ||
	    unpack_blob(TAG_CRED, pp, &len, &p->cred) < 0 ||
	    unpack_blob(TAG_CDH, pp, &len, &p->cdh) < 0)
		return (-1);

	return (0);
}

static size_t
pack(uint8_t *ptr, size_t len, const struct param *p)
{
	const size_t max = len;

	if (pack_byte(TAG_UV, &ptr, &len, p->uv) < 0 ||
	    pack_byte(TAG_UP, &ptr, &len, p->up) < 0 ||
	    pack_byte(TAG_U2F, &ptr, &len, p->u2f) < 0 ||
	    pack_byte(TAG_TYPE, &ptr, &len, p->type) < 0 ||
	    pack_byte(TAG_CRED_COUNT, &ptr, &len, p->cred_count) < 0 ||
	    pack_int(TAG_EXT, &ptr, &len, p->ext) < 0 ||
	    pack_int(TAG_SEED, &ptr, &len, p->seed) < 0 ||
	    pack_string(TAG_RP_ID, &ptr, &len, p->rp_id) < 0 ||
	    pack_string(TAG_PIN, &ptr, &len, p->pin) < 0 ||
	    pack_blob(TAG_WIRE_DATA, &ptr, &len, &p->wire_data) < 0 ||
	    pack_blob(TAG_RS256, &ptr, &len, &p->rs256) < 0 ||
	    pack_blob(TAG_ES256, &ptr, &len, &p->es256) < 0 ||
	    pack_blob(TAG_EDDSA, &ptr, &len, &p->eddsa) < 0 ||
	    pack_blob(TAG_CRED, &ptr, &len, &p->cred) < 0 ||
	    pack_blob(TAG_CDH, &ptr, &len, &p->cdh) < 0)
		return (0);

	return (max - len);
}

static void
get_assert(fido_assert_t *assert, uint8_t u2f, const struct blob *cdh,
    const char *rp_id, int ext, uint8_t up, uint8_t uv, const char *pin,
    uint8_t cred_count, struct blob *cred)
{
	fido_dev_t	*dev;
	fido_dev_io_t	 io;

	io.open = dev_open;
	io.close = dev_close;
	io.read = dev_read;
	io.write = dev_write;

	if ((dev = fido_dev_new()) == NULL || fido_dev_set_io_functions(dev,
	    &io) != FIDO_OK || fido_dev_open(dev, "nodev") != FIDO_OK) {
		fido_dev_free(&dev);
		return;
	}

	if (u2f & 1)
		fido_dev_force_u2f(dev);

	for (uint8_t i = 0; i < cred_count; i++)
		fido_assert_allow_cred(assert, cred->body, cred->len);

	fido_assert_set_clientdata_hash(assert, cdh->body, cdh->len);
	fido_assert_set_rp(assert, rp_id);
	if (ext & 1)
		fido_assert_set_extensions(assert, FIDO_EXT_HMAC_SECRET);
	if (up & 1)
		fido_assert_set_up(assert, FIDO_OPT_TRUE);
	if (uv & 1)
		fido_assert_set_uv(assert, FIDO_OPT_TRUE);
	/* XXX reuse cred as hmac salt to keep struct param small */
	fido_assert_set_hmac_salt(assert, cred->body, cred->len);

	fido_dev_get_assert(dev, assert, u2f & 1 ? NULL : pin);

	fido_dev_cancel(dev);
	fido_dev_close(dev);
	fido_dev_free(&dev);
}

static void
verify_assert(int type, const unsigned char *cdh_ptr, size_t cdh_len,
    const char *rp_id, const unsigned char *authdata_ptr, size_t authdata_len,
    const unsigned char *sig_ptr, size_t sig_len, uint8_t up, uint8_t uv,
    int ext, void *pk)
{
	fido_assert_t	*assert = NULL;

	if ((assert = fido_assert_new()) == NULL)
		return;

	fido_assert_set_clientdata_hash(assert, cdh_ptr, cdh_len);
	fido_assert_set_rp(assert, rp_id);
	fido_assert_set_count(assert, 1);
	fido_assert_set_authdata(assert, 0, authdata_ptr, authdata_len);
	fido_assert_set_extensions(assert, ext);
	if (up & 1) fido_assert_set_up(assert, FIDO_OPT_TRUE);
	if (uv & 1) fido_assert_set_uv(assert, FIDO_OPT_TRUE);
	fido_assert_set_sig(assert, 0, sig_ptr, sig_len);
	fido_assert_verify(assert, 0, type, pk);

	fido_assert_free(&assert);
}

/*
 * Do a dummy conversion to exercise rs256_pk_from_RSA().
 */
static void
rs256_convert(const rs256_pk_t *k)
{
	EVP_PKEY *pkey = NULL;
	rs256_pk_t *pk = NULL;
	RSA *rsa = NULL;
	volatile int r;

	if ((pkey = rs256_pk_to_EVP_PKEY(k)) == NULL ||
	    (pk = rs256_pk_new()) == NULL ||
	    (rsa = EVP_PKEY_get0_RSA(pkey)) == NULL)
		goto out;

	r = rs256_pk_from_RSA(pk, rsa);
out:
	if (pk)
		rs256_pk_free(&pk);
	if (pkey)
		EVP_PKEY_free(pkey);
}

/*
 * Do a dummy conversion to exercise eddsa_pk_from_EVP_PKEY().
 */
static void
eddsa_convert(const eddsa_pk_t *k)
{
	EVP_PKEY *pkey = NULL;
	eddsa_pk_t *pk = NULL;
	volatile int r;

	if ((pkey = eddsa_pk_to_EVP_PKEY(k)) == NULL ||
	    (pk = eddsa_pk_new()) == NULL)
		goto out;

	r = eddsa_pk_from_EVP_PKEY(pk, pkey);
out:
	if (pk)
		eddsa_pk_free(&pk);
	if (pkey)
		EVP_PKEY_free(pkey);
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	struct param	 p;
	fido_assert_t	*assert = NULL;
	es256_pk_t	*es256_pk = NULL;
	rs256_pk_t	*rs256_pk = NULL;
	eddsa_pk_t	*eddsa_pk = NULL;
	uint8_t		 flags;
	int		 cose_alg = 0;
	void		*pk;

	memset(&p, 0, sizeof(p));

	if (unpack(data, size, &p) < 0)
		return (0);

	srandom((unsigned int)p.seed);

	fido_init(0);

	switch (p.type & 3) {
	case 0:
		cose_alg = COSE_ES256;

		if ((es256_pk = es256_pk_new()) == NULL)
			return (0);

		es256_pk_from_ptr(es256_pk, p.es256.body, p.es256.len);
		pk = es256_pk;

		break;
	case 1:
		cose_alg = COSE_RS256;

		if ((rs256_pk = rs256_pk_new()) == NULL)
			return (0);

		rs256_pk_from_ptr(rs256_pk, p.rs256.body, p.rs256.len);
		pk = rs256_pk;

		rs256_convert(pk);

		break;
	default:
		cose_alg = COSE_EDDSA;

		if ((eddsa_pk = eddsa_pk_new()) == NULL)
			return (0);

		eddsa_pk_from_ptr(eddsa_pk, p.eddsa.body, p.eddsa.len);
		pk = eddsa_pk;

		eddsa_convert(pk);

		break;
	}

	if ((assert = fido_assert_new()) == NULL)
		goto out;

	set_wire_data(p.wire_data.body, p.wire_data.len);

	get_assert(assert, p.u2f, &p.cdh, p.rp_id, p.ext, p.up, p.uv, p.pin,
	    p.cred_count, &p.cred);

	/* XXX +1 on purpose */
	for (size_t i = 0; i <= fido_assert_count(assert); i++) {
		verify_assert(cose_alg,
		    fido_assert_clientdata_hash_ptr(assert),
		    fido_assert_clientdata_hash_len(assert),
		    fido_assert_rp_id(assert),
		    fido_assert_authdata_ptr(assert, i),
		    fido_assert_authdata_len(assert, i),
		    fido_assert_sig_ptr(assert, i),
		    fido_assert_sig_len(assert, i), p.up, p.uv, p.ext, pk);
		consume(fido_assert_id_ptr(assert, i),
		    fido_assert_id_len(assert, i));
		consume(fido_assert_user_id_ptr(assert, i),
		    fido_assert_user_id_len(assert, i));
		consume(fido_assert_hmac_secret_ptr(assert, i),
		    fido_assert_hmac_secret_len(assert, i));
		consume(fido_assert_user_icon(assert, i),
		    xstrlen(fido_assert_user_icon(assert, i)));
		consume(fido_assert_user_name(assert, i),
		    xstrlen(fido_assert_user_name(assert, i)));
		consume(fido_assert_user_display_name(assert, i),
		    xstrlen(fido_assert_user_display_name(assert, i)));
		flags = fido_assert_flags(assert, i);
		consume(&flags, sizeof(flags));
	}

out:
	es256_pk_free(&es256_pk);
	rs256_pk_free(&rs256_pk);
	eddsa_pk_free(&eddsa_pk);

	fido_assert_free(&assert);

	return (0);
}

static size_t
pack_dummy(uint8_t *ptr, size_t len)
{
	struct param	dummy;
	uint8_t		blob[16384];
	size_t		blob_len;

	memset(&dummy, 0, sizeof(dummy));

	dummy.type = 1;
	dummy.ext = FIDO_EXT_HMAC_SECRET;

	strlcpy(dummy.pin, dummy_pin, sizeof(dummy.pin));
	strlcpy(dummy.rp_id, dummy_rp_id, sizeof(dummy.rp_id));

	dummy.cdh.len = sizeof(dummy_cdh);
	dummy.es256.len = sizeof(dummy_es256);
	dummy.rs256.len = sizeof(dummy_rs256);
	dummy.eddsa.len = sizeof(dummy_eddsa);
	dummy.wire_data.len = sizeof(dummy_wire_data_fido);

	memcpy(&dummy.cdh.body, &dummy_cdh, dummy.cdh.len);
	memcpy(&dummy.wire_data.body, &dummy_wire_data_fido,
	    dummy.wire_data.len);
	memcpy(&dummy.es256.body, &dummy_es256, dummy.es256.len);
	memcpy(&dummy.rs256.body, &dummy_rs256, dummy.rs256.len);
	memcpy(&dummy.eddsa.body, &dummy_eddsa, dummy.eddsa.len);

	blob_len = pack(blob, sizeof(blob), &dummy);
	assert(blob_len != 0);

	if (blob_len > len) {
		memcpy(ptr, blob, len);
		return (len);
	}

	memcpy(ptr, blob, blob_len);

	return (blob_len);
}

size_t
LLVMFuzzerCustomMutator(uint8_t *data, size_t size, size_t maxsize,
    unsigned int seed) NO_MSAN
{
	struct param	p;
	uint8_t		blob[16384];
	size_t		blob_len;

	(void)seed;

	memset(&p, 0, sizeof(p));

	if (unpack(data, size, &p) < 0)
		return (pack_dummy(data, maxsize));

	mutate_byte(&p.uv);
	mutate_byte(&p.up);
	mutate_byte(&p.u2f);
	mutate_byte(&p.type);
	mutate_byte(&p.cred_count);

	mutate_int(&p.ext);
	p.seed = (int)seed;

	if (p.u2f & 1) {
		p.wire_data.len = sizeof(dummy_wire_data_u2f);
		memcpy(&p.wire_data.body, &dummy_wire_data_u2f,
		    p.wire_data.len);
	} else {
		p.wire_data.len = sizeof(dummy_wire_data_fido);
		memcpy(&p.wire_data.body, &dummy_wire_data_fido,
		    p.wire_data.len);
	}

	mutate_blob(&p.wire_data);
	mutate_blob(&p.rs256);
	mutate_blob(&p.es256);
	mutate_blob(&p.eddsa);
	mutate_blob(&p.cred);
	mutate_blob(&p.cdh);

	mutate_string(p.rp_id);
	mutate_string(p.pin);

	blob_len = pack(blob, sizeof(blob), &p);

	if (blob_len == 0 || blob_len > maxsize)
		return (0);

	memcpy(data, blob, blob_len);

	return (blob_len);
}
