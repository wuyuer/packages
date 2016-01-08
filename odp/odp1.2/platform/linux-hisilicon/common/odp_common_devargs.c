/*-
 *   BSD LICENSE
 *
 *   Copyright 2014 6WIND S.A.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of 6WIND S.A nor the names of its contributors
 *       may be used to endorse or promote products derived from this
 *       software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* This file manages the list of devices and their arguments, as given
 * by the user at startup */

#include <string.h>

/* #include <rte_log.h> */
#include <odp/config.h>
#include <odp_pci.h>
#include <odp_devargs.h>
#include "odp_private.h"

/** Global list of user devices */
struct odp_devargs_list devargs_list =
	TAILQ_HEAD_INITIALIZER(devargs_list);

int odp_parse_devargs_str(const char *devargs_str,
			   char **drvname, char **drvargs)
{
	char *sep;

	if (!devargs_str || !drvname || !drvargs)
		return -1;

	*drvname = strdup(devargs_str);
	if (!drvname) {
		PRINT("cannot allocate temp memory for driver name\n");
		return -1;
	}

	/* set the first ',' to '\0' to split name and arguments */
	sep = strchr(*drvname, ',');
	if (sep) {
		sep[0] = '\0';
		*drvargs = strdup(sep + 1);
	} else {
		*drvargs = strdup("");
	}

	if (!(*drvargs)) {
		PRINT("cannot allocate temp memory for driver arguments\n");
		free(*drvname);
		return -1;
	}

	return 0;
}

/* store a whitelist parameter for later parsing */
int odp_devargs_add(enum odp_devtype devtype, const char *devargs_str)
{
	struct odp_devargs *devargs = NULL;
	char *buf = NULL;
	int   ret;

	/* use malloc instead of odp_malloc as it's called early at init */
	devargs = malloc(sizeof(*devargs));
	if (!devargs) {
		PRINT("cannot allocate devargs\n");
		goto fail;
	}

	memset(devargs, 0, sizeof(*devargs));
	devargs->type = devtype;

	if (odp_parse_devargs_str(devargs_str, &buf, &devargs->args))
		goto fail;

	switch (devargs->type) {
	case ODP_DEVTYPE_WHITELISTED_PCI:
	case ODP_DEVTYPE_BLACKLISTED_PCI:

		/* try to parse pci identifier */
		if ((odp_parse_pci_BDF(buf, &devargs->pci.addr) != 0) &&
		    (odp_parse_pci_dombdf(buf, &devargs->pci.addr) != 0)) {
			PRINT("invalid PCI identifier <%s>\n", buf);
			goto fail;
		}

		break;
	case ODP_DEVTYPE_VIRTUAL:

		/* save driver name */
		ret = snprintf(devargs->virtual.drv_name,
			       sizeof(devargs->virtual.drv_name), "%s", buf);
		if ((ret < 0) || (ret >= (int)sizeof(devargs->virtual.drv_name))) {
			PRINT("driver name too large: <%s>\n", buf);
			goto fail;
		}

		break;
	}

	free(buf);
	TAILQ_INSERT_TAIL(&devargs_list, devargs, next);
	return 0;

fail:
	if (buf)
		free(buf);

	if (devargs) {
		free(devargs->args);
		free(devargs);
	}

	return -1;
}

/* count the number of devices of a specified type */
unsigned int odp_devargs_type_count(enum odp_devtype devtype)
{
	struct odp_devargs *devargs;
	unsigned int count = 0;

	TAILQ_FOREACH(devargs, &devargs_list, next)
	{
		if (devargs->type != devtype)
			continue;

		count++;
	}
	return count;
}

/* dump the user devices on the console */
void odp_devargs_dump(FILE *f)
{
	struct odp_devargs *devargs;

	fprintf(f, "User device white list:\n");
	TAILQ_FOREACH(devargs, &devargs_list, next)
	{
		if (devargs->type == ODP_DEVTYPE_WHITELISTED_PCI)
			fprintf(f, "  PCI whitelist " PCI_PRI_FMT " %s\n",
				devargs->pci.addr.domain,
				devargs->pci.addr.bus,
				devargs->pci.addr.devid,
				devargs->pci.addr.function,
				devargs->args);
		else if (devargs->type == ODP_DEVTYPE_BLACKLISTED_PCI)
			fprintf(f, "  PCI blacklist " PCI_PRI_FMT " %s\n",
				devargs->pci.addr.domain,
				devargs->pci.addr.bus,
				devargs->pci.addr.devid,
				devargs->pci.addr.function,
				devargs->args);
		else if (devargs->type == ODP_DEVTYPE_VIRTUAL)
			fprintf(f, "  VIRTUAL %s %s\n",
				devargs->virtual.drv_name,
				devargs->args);
		else
			fprintf(f, "  UNKNOWN %s\n", devargs->args);
	}
}