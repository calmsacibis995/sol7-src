#pragma ident   "@(#)ns_parser.h 1.1     97/12/03 SMI"

typedef union
#ifdef __cplusplus
	YYSTYPE
#endif
 {
	char *			cp;
	int			s_int;
	long			num;
	u_long			ul_int;
	u_int16_t		us_int;
	struct in_addr		ip_addr;
	ip_match_element	ime;
	ip_match_list		iml;
	key_info		keyi;
	enum axfr_format	axfr_fmt;
} YYSTYPE;
extern YYSTYPE yylval;
# define L_EOS 257
# define L_IPADDR 258
# define L_NUMBER 259
# define L_STRING 260
# define L_QSTRING 261
# define L_END_INCLUDE 262
# define T_INCLUDE 263
# define T_OPTIONS 264
# define T_DIRECTORY 265
# define T_PIDFILE 266
# define T_NAMED_XFER 267
# define T_FAKE_IQUERY 268
# define T_RECURSION 269
# define T_FETCH_GLUE 270
# define T_QUERY_SOURCE 271
# define T_LISTEN_ON 272
# define T_PORT 273
# define T_ADDRESS 274
# define T_DATASIZE 275
# define T_STACKSIZE 276
# define T_CORESIZE 277
# define S_LSTNBLOG 278
# define S_POLLFDSZ 279
# define T_DEFAULT 280
# define T_UNLIMITED 281
# define T_FILES 282
# define T_TRANSFERS_IN 283
# define T_TRANSFERS_OUT 284
# define T_TRANSFERS_PER_NS 285
# define T_TRANSFER_FORMAT 286
# define T_MAX_TRANSFER_TIME_IN 287
# define T_ONE_ANSWER 288
# define T_MANY_ANSWERS 289
# define T_NOTIFY 290
# define T_AUTH_NXDOMAIN 291
# define T_MULTIPLE_CNAMES 292
# define T_CLEAN_INTERVAL 293
# define T_INTERFACE_INTERVAL 294
# define T_STATS_INTERVAL 295
# define T_LOGGING 296
# define T_CATEGORY 297
# define T_CHANNEL 298
# define T_SEVERITY 299
# define T_DYNAMIC 300
# define T_FILE 301
# define T_VERSIONS 302
# define T_SIZE 303
# define T_SYSLOG 304
# define T_DEBUG 305
# define T_NULL_OUTPUT 306
# define T_PRINT_TIME 307
# define T_PRINT_CATEGORY 308
# define T_PRINT_SEVERITY 309
# define T_TOPOLOGY 310
# define T_SERVER 311
# define T_LONG_AXFR 312
# define T_BOGUS 313
# define T_TRANSFERS 314
# define T_KEYS 315
# define T_ZONE 316
# define T_IN 317
# define T_CHAOS 318
# define T_HESIOD 319
# define T_TYPE 320
# define T_MASTER 321
# define T_SLAVE 322
# define T_STUB 323
# define T_RESPONSE 324
# define T_HINT 325
# define T_MASTERS 326
# define T_ALSO_NOTIFY 327
# define T_ACL 328
# define T_ALLOW_UPDATE 329
# define T_ALLOW_QUERY 330
# define T_ALLOW_TRANSFER 331
# define T_SEC_KEY 332
# define T_ALGID 333
# define T_SECRET 334
# define T_CHECK_NAMES 335
# define T_WARN 336
# define T_FAIL 337
# define T_IGNORE 338
# define T_FORWARD 339
# define T_FORWARDERS 340
# define T_ONLY 341
# define T_FIRST 342
# define T_IF_NO_ANSWER 343
# define T_IF_NO_DOMAIN 344
# define T_YES 345
# define T_TRUE 346
# define T_NO 347
# define T_FALSE 348
