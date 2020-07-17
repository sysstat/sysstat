/*
 * sadf: System activity data formatter
 * (C) 1999-2020 by Sebastien Godard (sysstat <at> orange.fr)
 */

#ifndef _SADF_H
#define _SADF_H

#include "sa.h"

/* DTD version for XML output */
#define XML_DTD_VERSION	"3.9"

/* Various constants */
#define DO_SAVE		0
#define DO_RESTORE	1

#define IGNORE_NOTHING		0
#define IGNORE_RESTART		1
#define DONT_READ_CPU_NR	2
#define IGNORE_COMMENT		4
#define SET_TIMESTAMPS		8

/*
 ***************************************************************************
 * Output format identification values.
 ***************************************************************************
 */

/* Number of output formats */
#define NR_FMT	9

/* Output formats */
#define F_DB_OUTPUT	1
#define F_HEADER_OUTPUT	2
#define F_PPC_OUTPUT	3
#define F_XML_OUTPUT	4
#define F_JSON_OUTPUT	5
#define F_CONV_OUTPUT	6
#define F_SVG_OUTPUT	7
#define F_RAW_OUTPUT	8
#define F_PCP_OUTPUT	9

/* Format options */

/*
 * Indicate that a decimal point should be used to make output
 * locale independent.
 */
#define FO_LC_NUMERIC_C		0x01

/*
 * Indicate that option -H may be used with corresponding format
 * so that only the header is displayed.
 */
#define FO_HEADER_ONLY		0x02

/* Unused: 0x04 */

/*
 * Indicate that timestamp can be displayed in local time instead of UTC
 * if option -T or -t has been used.
 */
#define FO_LOCAL_TIME		0x08

/*
 * Indicate that all activities will be displayed horizontally
 * if option -h is used.
 */
#define FO_HORIZONTALLY		0x10

/*
 * Indicate that the timestamp can be displayed in seconds since the epoch
 * if option -U has been used.
 */
#define FO_SEC_EPOCH		0x20

/*
 * Indicate that the list of fields should be displayed before the first
 * line of statistics.
 */
#define FO_FIELD_LIST		0x40

/*
 * Indicate that flag AO_CLOSE_MARKUP (set for activities that need it)
 * should be taken into account for this output format.
 */
#define FO_TEST_MARKUP		0x80

/*
 * Indicate that timestamp cannot be displayed in the original local time
 * of the data file creator.
 */
#define FO_NO_TRUE_TIME		0x100

/*
 * Indicate that the number of different items should be counted and
 * a list created (see @item_list and @item_list_sz in struct activity).
 */
#define FO_ITEM_LIST		0x200

/*
 * Indicate that all the records, including RESTART and COMMENT ones,
 * should be displayed in order of time.
 */
#define FO_FULL_ORDER		0x400

#define SET_LC_NUMERIC_C(m)		(((m) & FO_LC_NUMERIC_C)	== FO_LC_NUMERIC_C)
#define ACCEPT_HEADER_ONLY(m)		(((m) & FO_HEADER_ONLY)		== FO_HEADER_ONLY)
#define ACCEPT_LOCAL_TIME(m)		(((m) & FO_LOCAL_TIME)		== FO_LOCAL_TIME)
#define ACCEPT_HORIZONTALLY(m)		(((m) & FO_HORIZONTALLY)	== FO_HORIZONTALLY)
#define ACCEPT_SEC_EPOCH(m)		(((m) & FO_SEC_EPOCH)		== FO_SEC_EPOCH)
#define DISPLAY_FIELD_LIST(m)		(((m) & FO_FIELD_LIST)		== FO_FIELD_LIST)
#define TEST_MARKUP(m)			(((m) & FO_TEST_MARKUP)		== FO_TEST_MARKUP)
#define REJECT_TRUE_TIME(m)		(((m) & FO_NO_TRUE_TIME)	== FO_NO_TRUE_TIME)
#define CREATE_ITEM_LIST(m)		(((m) & FO_ITEM_LIST)		== FO_ITEM_LIST)
#define ORDER_ALL_RECORDS(m)		(((m) & FO_FULL_ORDER)		== FO_FULL_ORDER)


/*
 ***************************************************************************
 * Various function prototypes
 ***************************************************************************
 */

void convert_file
	(char [], struct activity *[]);

/*
 * Prototypes used to display restart messages
 */
__printf_funct_t print_db_restart
	(int *, int, char *, char *, int, struct file_header *, struct record_header *);
__printf_funct_t print_ppc_restart
	(int *, int, char *, char *, int, struct file_header *, struct record_header *);
__printf_funct_t print_xml_restart
	(int *, int, char *, char *, int, struct file_header *, struct record_header *);
__printf_funct_t print_json_restart
	(int *, int, char *, char *, int, struct file_header *, struct record_header *);
__printf_funct_t print_raw_restart
	(int *, int, char *, char *, int, struct file_header *, struct record_header *);
__printf_funct_t print_pcp_restart
	(int *, int, char *, char *, int, struct file_header *, struct record_header *);

/*
 * Prototypes used to display comments
 */
__printf_funct_t print_db_comment
	(int *, int, char *, char *, int, char *, struct file_header *, struct record_header *);
__printf_funct_t print_ppc_comment
	(int *, int, char *, char *, int, char *, struct file_header *, struct record_header *);
__printf_funct_t print_xml_comment
	(int *, int, char *, char *, int, char *, struct file_header *, struct record_header *);
__printf_funct_t print_json_comment
	(int *, int, char *, char *, int, char *, struct file_header *, struct record_header *);
__printf_funct_t print_sar_comment
	(int *, int, char *, char *, int, char *, struct file_header *, struct record_header *);
__printf_funct_t print_raw_comment
	(int *, int, char *, char *, int, char *, struct file_header *, struct record_header *);
__printf_funct_t print_pcp_comment
	(int *, int, char *, char *, int, char *, struct file_header *, struct record_header *);

/*
 * Prototypes used to display the statistics part of the report
 */
__printf_funct_t print_xml_statistics
	(int *, int, struct activity * [], unsigned int []);
__printf_funct_t print_json_statistics
	(int *, int, struct activity * [], unsigned int []);
__printf_funct_t print_pcp_statistics
	(int *, int, struct activity * [], unsigned int []);

/*
 * Prototypes used to display the timestamp part of the report
 */
__tm_funct_t print_db_timestamp
	(void *, int, char *, char *, unsigned long long,
	 struct record_header *, struct file_header *, unsigned int);
__tm_funct_t print_ppc_timestamp
	(void *, int, char *, char *, unsigned long long,
	 struct record_header *, struct file_header *, unsigned int);
__tm_funct_t print_xml_timestamp
	(void *, int, char *, char *, unsigned long long,
	 struct record_header *, struct file_header *, unsigned int);
__tm_funct_t print_json_timestamp
	(void *, int, char *, char *, unsigned long long,
	 struct record_header *, struct file_header *, unsigned int);
__tm_funct_t print_raw_timestamp
	(void *, int, char *, char *, unsigned long long,
	 struct record_header *, struct file_header *, unsigned int);
__tm_funct_t print_pcp_timestamp
	(void *, int, char *, char *, unsigned long long,
	 struct record_header *, struct file_header *, unsigned int);

/*
 * Prototypes used to display the report header
 */
__printf_funct_t print_xml_header
	(void *, int, char *, struct file_magic *, struct file_header *,
	 struct activity * [], unsigned int [], struct file_activity *);
__printf_funct_t print_json_header
	(void *, int, char *, struct file_magic *, struct file_header *,
	 struct activity * [], unsigned int [], struct file_activity *);
__printf_funct_t print_hdr_header
	(void *, int, char *, struct file_magic *, struct file_header *,
	 struct activity * [], unsigned int [], struct file_activity *);
__printf_funct_t print_svg_header
	(void *, int, char *, struct file_magic *, struct file_header *,
	 struct activity * [], unsigned int [], struct file_activity *);
__printf_funct_t print_pcp_header
	(void *, int, char *, struct file_magic *, struct file_header *,
	 struct activity * [], unsigned int [], struct file_activity *);

/*
 * Main display functions
 */
void logic1_display_loop
	(int, char *, struct file_activity *, struct file_magic *,
	 struct tm *, void *);
void logic2_display_loop
	(int, char *, struct file_activity *, struct file_magic *,
	 struct tm *, void *);
void svg_display_loop
	(int, char *, struct file_activity *, struct file_magic *,
	 struct tm *, void *);

#endif  /* _SADF_H */
