/*
 * format.c: Output format definitions for sadf and sar
 * (C) 2011-2020 by Sebastien GODARD (sysstat <at> orange.fr)
 *
 ***************************************************************************
 * This program is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU General Public License as published  by  the *
 * Free Software Foundation; either version 2 of the License, or (at  your *
 * option) any later version.                                              *
 *                                                                         *
 * This program is distributed in the hope that it  will  be  useful,  but *
 * WITHOUT ANY WARRANTY; without the implied warranty  of  MERCHANTABILITY *
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License *
 * for more details.                                                       *
 *                                                                         *
 * You should have received a copy of the GNU General Public License along *
 * with this program; if not, write to the Free Software Foundation, Inc., *
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335 USA              *
 ***************************************************************************
 */

#ifdef SOURCE_SADF
#include "sadf.h"
#endif

#ifdef SOURCE_SAR
#include "sa.h"
#endif

/*
 ***************************************************************************
 * Definitions of output formats.
 * See sadf.h file for format structure definition.
 ***************************************************************************
 */

#ifdef SOURCE_SADF
/*
 * Display only datafile header.
 */
struct report_format hdr_fmt = {
	.id		= F_HEADER_OUTPUT,
	.options	= FO_HEADER_ONLY,
	.f_header	= print_hdr_header,
	.f_statistics	= NULL,
	.f_timestamp	= NULL,
	.f_restart	= NULL,
	.f_comment	= NULL,
	.f_display	= NULL
};

/*
 * Database friendly format.
 */
struct report_format db_fmt = {
	.id		= F_DB_OUTPUT,
	.options	= FO_LOCAL_TIME + FO_HORIZONTALLY +
			  FO_SEC_EPOCH + FO_FIELD_LIST,
	.f_header	= NULL,
	.f_statistics	= NULL,
	.f_timestamp	= print_db_timestamp,
	.f_restart	= print_db_restart,
	.f_comment	= print_db_comment,
	.f_display	= logic2_display_loop
};

/*
 * Format easily handled by pattern processing commands like awk.
 */
struct report_format ppc_fmt = {
	.id		= F_PPC_OUTPUT,
	.options	= FO_LOCAL_TIME + FO_SEC_EPOCH,
	.f_header	= NULL,
	.f_statistics	= NULL,
	.f_timestamp	= print_ppc_timestamp,
	.f_restart	= print_ppc_restart,
	.f_comment	= print_ppc_comment,
	.f_display	= logic2_display_loop
};

/*
 * XML output.
 */
struct report_format xml_fmt = {
	.id		= F_XML_OUTPUT,
	.options	= FO_HEADER_ONLY + FO_LOCAL_TIME + FO_TEST_MARKUP,
	.f_header	= print_xml_header,
	.f_statistics	= print_xml_statistics,
	.f_timestamp	= print_xml_timestamp,
	.f_restart	= print_xml_restart,
	.f_comment	= print_xml_comment,
	.f_display	= logic1_display_loop
};

/*
 * JSON output.
 */
struct report_format json_fmt = {
	.id		= F_JSON_OUTPUT,
	.options	= FO_HEADER_ONLY + FO_LOCAL_TIME + FO_TEST_MARKUP +
			  FO_LC_NUMERIC_C,
	.f_header	= print_json_header,
	.f_statistics	= print_json_statistics,
	.f_timestamp	= print_json_timestamp,
	.f_restart	= print_json_restart,
	.f_comment	= print_json_comment,
	.f_display	= logic1_display_loop
};

/*
 * Convert an old datafile to up-to-date format.
 */
struct report_format conv_fmt = {
	.id		= F_CONV_OUTPUT,
	.options	= 0,
	.f_header	= NULL,
	.f_statistics	= NULL,
	.f_timestamp	= NULL,
	.f_restart	= NULL,
	.f_comment	= NULL,
	.f_display	= NULL
};

/*
 * SVG output.
 */
struct report_format svg_fmt = {
	.id		= F_SVG_OUTPUT,
	.options	= FO_HEADER_ONLY + FO_LOCAL_TIME + FO_NO_TRUE_TIME +
			  FO_LC_NUMERIC_C,
	.f_header	= print_svg_header,
	.f_statistics	= NULL,
	.f_timestamp	= NULL,
	.f_restart	= NULL,
	.f_comment	= NULL,
	.f_display	= svg_display_loop
};

/*
 * Raw output.
 */
struct report_format raw_fmt = {
	.id		= F_RAW_OUTPUT,
	.options	= FO_LOCAL_TIME + FO_SEC_EPOCH,
	.f_header	= NULL,
	.f_statistics	= NULL,
	.f_timestamp	= print_raw_timestamp,
	.f_restart	= print_raw_restart,
	.f_comment	= print_raw_comment,
	.f_display	= logic2_display_loop
};

/*
 * PCP output.
 */
struct report_format pcp_fmt = {
	.id		= F_PCP_OUTPUT,
	.options	= FO_HEADER_ONLY + FO_LOCAL_TIME + FO_NO_TRUE_TIME +
			  FO_ITEM_LIST + FO_FULL_ORDER,
	.f_header	= print_pcp_header,
	.f_statistics	= print_pcp_statistics,
	.f_timestamp	= print_pcp_timestamp,
	.f_restart	= print_pcp_restart,
	.f_comment	= print_pcp_comment,
	.f_display	= logic1_display_loop
};

/*
 * Array of output formats.
 */
struct report_format *fmt[NR_FMT] = {
	&hdr_fmt,
	&db_fmt,
	&ppc_fmt,
	&xml_fmt,
	&json_fmt,
	&conv_fmt,
	&svg_fmt,
	&raw_fmt,
	&pcp_fmt
};
#endif

#ifdef SOURCE_SAR
/*
 * Special output format for sar.
 * Used only for functions to display special
 * (RESTART and COMMENT) records.
 */
struct report_format sar_fmt = {
	.id		= 0,
	.options	= 0,
	.f_header	= NULL,
	.f_statistics	= NULL,
	.f_timestamp	= NULL,
	.f_restart	= print_sar_restart,
	.f_comment	= print_sar_comment
};
#endif
