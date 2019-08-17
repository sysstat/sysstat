/*
 * format.c: Output format definitions for sadf and sar
 * (C) 2011-2018 by Sebastien GODARD (sysstat <at> orange.fr)
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

#ifdef SOURCE_SAR
#include "sa.h"
#endif

/*
 ***************************************************************************
 * Definitions of output formats.
 * See sadf.h file for format structure definition.
 ***************************************************************************
 */

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
