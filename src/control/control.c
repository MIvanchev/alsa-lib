/**
 * \file control/control.c
 * \brief CTL interface - primitive controls
 * \author Abramo Bagnara <abramo@alsa-project.org>
 * \date 2000
 *
 * CTL interface is designed to access primitive controls.
 * See \ref control page for more details.
 */
/*
 *  Control Interface - main file
 *  Copyright (c) 2000 by Abramo Bagnara <abramo@alsa-project.org>
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/*! \page control Control interface

<P>Control interface is designed to access primitive controls. There is
also an interface for notifying about control and structure changes.


\section control_general_overview General overview

In Alsa, there are physical sound cards, such as USB headsets, and
virtual sound cards, such as "pulse", which represents the PulseAudio
Sound system. Each sound card offers a control interface, making its
settings (e.g. volume knobs) available. The complete list of available
control interfaces can be obtained using snd_device_name_hint(),
giving -1 as card index and "ctl" as interface type. Each returned
NAME hint identifies a control interface.

Sound cards have an ID (a string), an index (an int, sometimes called
the "card number"), a name, a longname, a mixername and a "components"
property. The file /proc/asound/cards lists most of these properties
for physical sound cards. Virtual sound cards are not listed in that
file. The format is:

\verbatim
index [ID     ] Driver - name
                longname
\endverbatim

Note that the mixername and components are not listed.


\subsection control_cards_id Identifying and Opening Control Interfaces

To work with a control interface, is must be opened first, using
snd_ctl_open(). This function takes the interface name.

For physical sound cards, the control interface can be identified
using the string "hw:<index>" (e.g. `hw:2`). The NAME hint - which is
"hw:CARD=<ID>" - can also be used. Further, its device file (something
like `/dev/snd/controlC0`) is also acceptable. Either of them can be
given to snd_ctl_open().

For virtual sound cards, the NAME hint is given to snd_ctl_open().

The functions snd_card_get_index(), snd_card_get_name() and
snd_card_get_longname() can be used to find an identifying property
when another one is already known.

\section control_elements Elements

In ALSA control feature, each sound card can have control elements. The elements
are managed according to below model.

 - Element set

   - A set of elements with the same attribute (i.e. name, get/put operations).
     Some element sets can be added to a sound card by drivers in kernel and
     userspace applications.

 - Element

   - A control element might be a master volume control, for example, or a
     read-only indicator, such as a sync status. An element has a type (e.g.
     SNDRV_CTL_ELEM_TYPE_INTEGER or SNDRV_CTL_ELEM_TYPE_BOOLEAN) and - depending
     on the type - min/max values, a step size, a set of possible values (for
     enums), etc.

 - Member

   - An element usually includes one or more member(s) to have a value. For
     example, a stereo volume control element has two members (for left/right),
     while a mono volume has only one member. The member count can be obtained
     using snd_ctl_elem_info_get_count(). Elements of type
     "SNDRV_CTL_ELEM_TYPE_BYTES" or "SNDRV_CTL_ELEM_TYPE_IEC958" have no members
     at all (and thus no member count), they have just a single value. The
     members share the same properties (e.g. both volume control members have
     the same min/max values). The value of each member can be changed by both
     of userspace applications and drivers in kernel.


\subsection identifying_elements Identifying Elements

Each element has the following identifying properties:

  - The numid (a numeric identifier, assigned when the sound card is
    detected, constant while the sound card is kept connected)
  - The interface type (e.g. MIXER, CARD or PCM)
  - The device
  - The subdevice
  - Its name
  - Its index

An element can be identified either by its short numid or by the full
set of fields (interface type, device, subdevice, name, index).
This set of fields is always the same (driver updates can change it,
but in practice this is rare). The numid can change on each boot.
In case of an USB sound card, the numid can also change when it
is reconnected. The short numid is used to reduce the lookup time.

\subsection element_lists Element Lists

An element list can be used to obtain a list of all elements of the
sound card. The list contains generic information (e.g. how many
elements the card has), and the identifying properties of the elements
(numid, card, name, ...). See #snd_ctl_elem_list_t to learn more about
element lists.


\subsection working_with_elements Working with Elements

It is possible to obtain information about an element using the
snd_ctl_elem_info_*() functions. For enums, the allowed values can be
obtained, for integers, the min/max values can be obtained, and so
on. In addition, these functions can report the identifying
properties. E.g. when the element is addressed using its numid, the
functions complements the name, index, etc.

To access the members (i.e. values) of a control, use the
snd_ctl_elem_value*() functions.  These allow to get and set the
actual values or settings. It is also possible to get and set the ID
values (such as the numid or the name).


\subsection element_sets Element Sets

The type of element set is one of integer, integer64, boolean, enumerators,
bytes and IEC958 structure. This indicates the type of value for each member in
elements included in the element set.


\section events Events

When the value of a member is changed, corresponding events are transferred to
userspace applications. The applications should subscribe any events in advance.

\section tlv_blob Supplemental data for elements in an element set

TLV feature is designed to transfer data in a shape of Type/Length/Value,
between a driver and any userspace applications. The main purpose is to attach
supplement information for elements to an element set; e.g. dB range.

At first, this feature was implemented to add pre-defined data readable to
userspace applications. Soon, it was extended to handle several operations;
read, write and command. The original implementation remains as the read
operation. The command operation allows drivers to have own implementations
against requests from userspace applications.

This feature was introduced to ALSA control feature in 2006, at commit
c7a0708a2362, corresponding to a series of work for Linux kernel (42750b04c5ba
and 8aa9b586e420).

There's no limitation about maximum size of the data, therefore it can be used
to deliver quite large arbitrary data from userspace to in-kernel drivers via
ALSA control character device. Focusing on this nature, as of 2016, some
in-kernel implementations utilize this feature for I/O operations. This is
against the original design.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <stdbool.h>
#include <limits.h>
#include "control_local.h"

/**
 * \brief get identifier of CTL handle
 * \param ctl CTL handle
 * \return ascii identifier of CTL handle
 *
 * Returns the ASCII identifier of given CTL handle. It's the same
 * identifier specified in snd_ctl_open().
 */
const char *snd_ctl_name(snd_ctl_t *ctl)
{
	assert(ctl);
	return ctl->name;
}

/**
 * \brief get type of CTL handle
 * \param ctl CTL handle
 * \return type of CTL handle
 *
 * Returns the type #snd_ctl_type_t of given CTL handle.
 */
snd_ctl_type_t snd_ctl_type(snd_ctl_t *ctl)
{
	assert(ctl);
	return ctl->type;
}

/**
 * \brief close CTL handle
 * \param ctl CTL handle
 * \return 0 on success otherwise a negative error code
 *
 * Closes the specified CTL handle and frees all associated
 * resources.
 */
int snd_ctl_close(snd_ctl_t *ctl)
{
	int err;
	while (!list_empty(&ctl->async_handlers)) {
		snd_async_handler_t *h = list_entry(&ctl->async_handlers.next, snd_async_handler_t, hlist);
		snd_async_del_handler(h);
	}
	err = ctl->ops->close(ctl);
	free(ctl->name);
	snd_dlobj_cache_put(ctl->open_func);
	free(ctl);
	return err;
}

/**
 * \brief set nonblock mode
 * \param ctl CTL handle
 * \param nonblock 0 = block, 1 = nonblock mode, 2 = abort
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_nonblock(snd_ctl_t *ctl, int nonblock)
{
	int err;
	assert(ctl);
	err = ctl->ops->nonblock(ctl, nonblock);
	if (err < 0)
		return err;
	ctl->nonblock = nonblock;
	return 0;
}

#ifndef DOC_HIDDEN
int snd_ctl_new(snd_ctl_t **ctlp, snd_ctl_type_t type, const char *name, int mode)
{
	snd_ctl_t *ctl;
	ctl = calloc(1, sizeof(*ctl));
	if (!ctl)
		return -ENOMEM;
	ctl->type = type;
	ctl->mode = mode;
	if (name)
		ctl->name = strdup(name);
	INIT_LIST_HEAD(&ctl->async_handlers);
	*ctlp = ctl;
	return 0;
}
	

/**
 * \brief set async mode
 * \param ctl CTL handle
 * \param sig Signal to raise: < 0 disable, 0 default (SIGIO)
 * \param pid Process ID to signal: 0 current
 * \return 0 on success otherwise a negative error code
 *
 * A signal is raised when a change happens.
 */
int snd_ctl_async(snd_ctl_t *ctl, int sig, pid_t pid)
{
	assert(ctl);
	if (sig == 0)
		sig = SIGIO;
	if (pid == 0)
		pid = getpid();
	return ctl->ops->async(ctl, sig, pid);
}
#endif

/**
 * \brief get count of poll descriptors for CTL handle
 * \param ctl CTL handle
 * \return count of poll descriptors
 */
int snd_ctl_poll_descriptors_count(snd_ctl_t *ctl)
{
	assert(ctl);
	if (ctl->ops->poll_descriptors_count)
		return ctl->ops->poll_descriptors_count(ctl);
	if (ctl->poll_fd < 0)
		return 0;
	return 1;
}

/**
 * \brief get poll descriptors
 * \param ctl CTL handle
 * \param pfds array of poll descriptors
 * \param space space in the poll descriptor array
 * \return count of filled descriptors
 */
int snd_ctl_poll_descriptors(snd_ctl_t *ctl, struct pollfd *pfds, unsigned int space)
{
	assert(ctl && pfds);
	if (ctl->ops->poll_descriptors)
		return ctl->ops->poll_descriptors(ctl, pfds, space);
	if (ctl->poll_fd < 0)
		return 0;
	if (space > 0) {
		pfds->fd = ctl->poll_fd;
		pfds->events = POLLIN|POLLERR|POLLNVAL;
		return 1;
	}
	return 0;
}

/**
 * \brief get returned events from poll descriptors
 * \param ctl CTL handle
 * \param pfds array of poll descriptors
 * \param nfds count of poll descriptors
 * \param revents returned events
 * \return zero if success, otherwise a negative error code
 */
int snd_ctl_poll_descriptors_revents(snd_ctl_t *ctl, struct pollfd *pfds, unsigned int nfds, unsigned short *revents)
{
	assert(ctl && pfds && revents);
	if (ctl->ops->poll_revents)
		return ctl->ops->poll_revents(ctl, pfds, nfds, revents);
	if (nfds == 1) {
		*revents = pfds->revents;
                return 0;
	}
	return -EINVAL;
}

/**
 * \brief Ask to be informed about events (poll, #snd_async_add_ctl_handler, #snd_ctl_read)
 * \param ctl CTL handle
 * \param subscribe 0 = unsubscribe, 1 = subscribe, -1 = check subscribe or not
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_subscribe_events(snd_ctl_t *ctl, int subscribe)
{
	assert(ctl);
	return ctl->ops->subscribe_events(ctl, subscribe);
}


/**
 * \brief Get information about the sound card.
 *
 * Obtain information about the sound card previously opened using
 * snd_ctl_open(). The object "info" must be allocated prior to calling this
 * function. See snd_ctl_card_info_t for details.
 *
 * \param ctl The CTL handle.
 * \param info The card information is stored here.
 * \return 0 on success, otherwise a negative error code.
 */
int snd_ctl_card_info(snd_ctl_t *ctl, snd_ctl_card_info_t *info)
{
	assert(ctl && info);
	return ctl->ops->card_info(ctl, info);
}

/**
 * \brief Get a list of element identifiers
 *
 * Before calling this function, memoru must be allocated using
 * snd_ctl_elem_list_malloc().
 *
 * This function obtains data from the sound card driver and puts it
 * into the list.
 *
 * If there was space allocated for the element identifiers (using
 * snd_ctl_elem_list_alloc_space()), information will be filled in. If
 * too little space was allocated, only a part of the elements will be
 * queried. If there was too much space allocated, some of it remains
 * unused. Use snd_ctl_elem_list_get_count() and
 * snd_ctl_elem_list_get_used() to obtain information about space
 * usage. See #snd_ctl_elem_list_t to learn more.
 *
 * \param ctl CTL handle
 * \param list CTL element identifiers list pointer
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_elem_list(snd_ctl_t *ctl, snd_ctl_elem_list_t *list)
{
	assert(ctl && list);
	assert(list->space == 0 || list->pids);
	return ctl->ops->element_list(ctl, list);
}

/**
 * \brief Get CTL element information
 * \param ctl CTL handle
 * \param info CTL element id/information pointer
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_elem_info(snd_ctl_t *ctl, snd_ctl_elem_info_t *info)
{
	assert(ctl && info && (info->id.name[0] || info->id.numid));
	return ctl->ops->element_info(ctl, info);
}

#if 0 /* deprecated */
static bool validate_element_member_dimension(snd_ctl_elem_info_t *info)
{
	unsigned int members;
	unsigned int i;

	if (info->dimen.d[0] == 0)
		return true;

	members = 1;
	for (i = 0; i < ARRAY_SIZE(info->dimen.d); ++i) {
		if (info->dimen.d[i] == 0)
			break;
		members *= info->dimen.d[i];

		if (members > info->count)
			return false;
	}

	for (++i; i < ARRAY_SIZE(info->dimen.d); ++i) {
		if (info->dimen.d[i] > 0)
			return false;
	}

	return members == info->count;
}
#else /* deprecated */
#define validate_element_member_dimension(info)		true
#endif /* deprecated */

#define USER_ACCESS_DEFAULT (\
	SNDRV_CTL_ELEM_ACCESS_READWRITE |\
	SNDRV_CTL_ELEM_ACCESS_TLV_READWRITE |\
	SNDRV_CTL_ELEM_ACCESS_USER)

#define USER_ACCESS_SETTABLE (\
	SNDRV_CTL_ELEM_ACCESS_READWRITE |\
	SNDRV_CTL_ELEM_ACCESS_VOLATILE |\
	SNDRV_CTL_ELEM_ACCESS_TLV_READWRITE |\
	SNDRV_CTL_ELEM_ACCESS_INACTIVE |\
	SNDRV_CTL_ELEM_ACCESS_USER)

static inline int set_user_access(snd_ctl_elem_info_t *info)
{
	if (info->access == 0) {
		info->access = USER_ACCESS_DEFAULT;
	} else {
		if ((info->access & SNDRV_CTL_ELEM_ACCESS_READWRITE) == 0)
			return -1;
		if (info->access & ~USER_ACCESS_SETTABLE)
			return -1;
		info->access |= SNDRV_CTL_ELEM_ACCESS_USER;
	}
	return 0;
}

int __snd_ctl_add_elem_set(snd_ctl_t *ctl, snd_ctl_elem_info_t *info,
			   unsigned int element_count,
			   unsigned int member_count)
{
	if (ctl == NULL || info->id.name[0] == '\0')
		return -EINVAL;

	if (set_user_access(info))
		return -EINVAL;

	info->owner = element_count;
	info->count = member_count;

	if (!validate_element_member_dimension(info))
		return -EINVAL;

	return ctl->ops->element_add(ctl, info);
}

/**
 * \brief Create and add some user-defined control elements of integer type.
 * \param ctl A handle of backend module for control interface.
 * \param info Common information for a new element set, with ID of the first new
 *	       element.
 * \param element_count The number of elements added by this operation.
 * \param member_count The number of members which a element has to
 *			   represent its states.
 * \param min Minimum value for each member of the elements.
 * \param max Maximum value for each member of the elements.
 * \param step The step of value for each member in the elements.
 * \return Zero on success, otherwise a negative error code.
 *
 * This function creates some user elements with integer type. These elements
 * are not controlled by device drivers in kernel. They can be operated by the
 * same way as usual elements added by the device drivers.
 *
 * The name field of \a id must be set with unique value to identify new control
 * elements. After returning, all fields of \a id are filled. A element can be
 * identified by the combination of name and index, or by numid.
 *
 * All of members in the new elements are locked. The value of each member is
 * initialized with the minimum value.
 *
 * \par Errors:
 * <dl>
 * <dt>-EBUSY
 * <dd>A element with ID \a id already exists.
 * <dt>-EINVAL
 * <dd>Some arguments include invalid value; i.e. ID field in \a info has no
 *     name, or the number of members is not between 1 to 127.
 * <dt>-ENOMEM
 * <dd>Out of memory, or there are too many user elements.
 * <dt>-ENXIO
 * <dd>This backend module does not support user elements of integer type.
 * <dt>-ENODEV
 * <dd>Device unplugged.
 * </dl>
 *
 * \par Compatibility:
 * This function is added in version 1.1.2.
 */
int snd_ctl_add_integer_elem_set(snd_ctl_t *ctl, snd_ctl_elem_info_t *info,
				 unsigned int element_count,
				 unsigned int member_count,
				 long min, long max, long step)
{
	snd_ctl_elem_value_t data = {0};
	unsigned int i;
	unsigned int j;
	unsigned int numid;
	int err;

	if (info == NULL)
		return -EINVAL;

	info->type = SND_CTL_ELEM_TYPE_INTEGER;
	info->value.integer.min = min;
	info->value.integer.max = max;
	info->value.integer.step = step;

	err = __snd_ctl_add_elem_set(ctl, info, element_count, member_count);
	if (err < 0)
		return err;
	numid = snd_ctl_elem_id_get_numid(&info->id);

	/* Set initial value to all of members in all of added elements. */
	data.id = info->id;
	for (i = 0; i < element_count; i++) {
		snd_ctl_elem_id_set_numid(&data.id, numid + i);

		for (j = 0; j < member_count; j++)
			data.value.integer.value[j] = min;

		err = ctl->ops->element_write(ctl, &data);
		if (err < 0)
			return err;
	}

	return 0;
}

/**
 * \brief Create and add some user-defined control elements of integer64 type.
 * \param ctl A handle of backend module for control interface.
 * \param info Common information for a new element set, with ID of the first new
 *	       element.
 * \param element_count The number of elements added by this operation.
 * \param member_count The number of members which a element has to
 *	    	   represent its states.
 * \param min Minimum value for each member of the elements.
 * \param max Maximum value for each member of the elements.
 * \param step The step of value for each member in the elements.
 * \return Zero on success, otherwise a negative error code.
 *
 * This function creates some user elements with integer64 type. These elements
 * are not controlled by device drivers in kernel. They can be operated by the
 * same way as usual elements added by the device drivers.
 *
 * The name field of \a id must be set with unique value to identify new control
 * elements. After returning, all fields of \a id are filled. A element can be
 * identified by the combination of name and index, or by numid.
 *
 * All of members in the new elements are locked. The value of each member is
 * initialized with the minimum value.
 *
 * \par Errors:
 * <dl>
 * <dt>-EBUSY
 * <dd>A element with ID \a id already exists.
 * <dt>-EINVAL
 * <dd>Some arguments include invalid value; i.e. ID has no name, or the number
 *     of members is not between 1 to 127.
 * <dt>-ENOMEM
 * <dd>Out of memory, or there are too many user elements.
 * <dt>-ENXIO
 * <dd>This backend module does not support user elements of integer64 type.
 * <dt>-ENODEV
 * <dd>Device unplugged.
 * </dl>
 *
 * \par Compatibility:
 * This function is added in version 1.1.2.
 */
int snd_ctl_add_integer64_elem_set(snd_ctl_t *ctl, snd_ctl_elem_info_t *info,
				   unsigned int element_count,
				   unsigned int member_count,
				   long long min, long long max, long long step)
{
	snd_ctl_elem_value_t data = {0};
	unsigned int i;
	unsigned int j;
	unsigned int numid;
	int err;

	if (info == NULL)
		return -EINVAL;

	info->type = SND_CTL_ELEM_TYPE_INTEGER64;
	info->value.integer64.min = min;
	info->value.integer64.max = max;
	info->value.integer64.step = step;

	err = __snd_ctl_add_elem_set(ctl, info, element_count, member_count);
	if (err < 0)
		return err;
	numid = snd_ctl_elem_id_get_numid(&info->id);

	/* Set initial value to all of members in all of added elements. */
	data.id = info->id;
	for (i = 0; i < element_count; i++) {
		snd_ctl_elem_id_set_numid(&data.id, numid + i);

		for (j = 0; j < member_count; j++)
			data.value.integer64.value[j] = min;

		err = ctl->ops->element_write(ctl, &data);
		if (err < 0)
			return err;
	}

	return 0;
}

/**
 * \brief Create and add some user-defined control elements of boolean type.
 * \param ctl A handle of backend module for control interface.
 * \param info Common information for a new element set, with ID of the first new
 *	       element.
 * \param element_count The number of elements added by this operation.
 * \param member_count The number of members which a element has to
 *			   represent its states.
 *
 * This function creates some user elements with boolean type. These elements
 * are not controlled by device drivers in kernel. They can be operated by the
 * same way as usual elements added by the device drivers.
 *
 * The name field of \a id must be set with unique value to identify new control
 * elements. After returning, all fields of \a id are filled. A element can be
 * identified by the combination of name and index, or by numid.
 *
 * All of members in the new elements are locked. The value of each member is
 * initialized with false.
 *
 * \par Errors:
 * <dl>
 * <dt>-EBUSY
 * <dd>A element with ID \a id already exists.
 * <dt>-EINVAL
 * <dd>Some parameters include invalid value; i.e. ID has no name, or the number
 *      of members is not between 1 to 127.
 * <dt>-ENOMEM
 * <dd>Out of memory, or there are too many user elements.
 * <dt>-ENXIO
 * <dd>This backend module does not support user elements of boolean type.
 * <dt>-ENODEV
 * <dd>Device unplugged.
 * </dl>
 *
 * \par Compatibility:
 * This function is added in version 1.1.2.
 */
int snd_ctl_add_boolean_elem_set(snd_ctl_t *ctl, snd_ctl_elem_info_t *info,
				 unsigned int element_count,
				 unsigned int member_count)
{
	if (info == NULL)
		return -EINVAL;

	info->type = SND_CTL_ELEM_TYPE_BOOLEAN;
	info->value.integer.min = 0;
	info->value.integer.max = 1;

	return __snd_ctl_add_elem_set(ctl, info, element_count, member_count);
}

/**
 * \brief Create and add some user-defined control elements of enumerated type.
 * \param ctl A handle of backend module for control interface.
 * \param info Common information for a new element set, with ID of the first new
 *	       element.
 * \param element_count The number of elements added by this operation.
 * \param member_count The number of members which a element has to
 *	    	   represent its states.
 * \param items Range of possible values (0 ... \a items - 1).
 * \param labels An array containing \a items strings.
 * \return Zero on success, otherwise a negative error code.
 *
 * This function creates some user elements with enumerated type. These elements
 * are not controlled by device drivers in kernel. They can be operated by the
 * same way as usual elements added by the device drivers.
 *
 * The name field of \a id must be set with unique value to identify new control
 * elements. After returning, all fields of \a id are filled. A element can be
 * identified by the combination of name and index, or by numid.
 *
 * All of members in the new elements are locked. The value of each member is
 * initialized with the first entry of labels.
 *
 * \par Errors:
 * <dl>
 * <dt>-EBUSY
 * <dd>A control element with ID \a id already exists.
 * <dt>-EINVAL
 * <dd>Some arguments include invalid value; i.e. \a element_count is not
 *     between 1 to 127, or \a items is not at least one, or a string in \a
 *     labels is empty, or longer than 63 bytes, or total length of the labels
 *     requires more than 64 KiB storage.
 * <dt>-ENOMEM
 * <dd>Out of memory, or there are too many user control elements.
 * <dt>-ENXIO
 * <dd>This driver does not support (enumerated) user controls.
 * <dt>-ENODEV
 * <dd>Device unplugged.
 * </dl>
 *
 * \par Compatibility:
 * This function is added in version 1.1.2.
 */
int snd_ctl_add_enumerated_elem_set(snd_ctl_t *ctl, snd_ctl_elem_info_t *info,
				    unsigned int element_count,
				    unsigned int member_count,
				    unsigned int items,
				    const char *const labels[])
{
	unsigned int i, bytes;
	char *buf, *p;
	int err;

	if (info == NULL || labels == NULL)
		return -EINVAL;

	info->type = SND_CTL_ELEM_TYPE_ENUMERATED;
	info->owner = element_count;
	info->count = member_count;
	info->value.enumerated.items = items;

	bytes = 0;
	for (i = 0; i < items; ++i)
		bytes += strlen(labels[i]) + 1;
	if (bytes == 0)
		return -EINVAL;
	buf = malloc(bytes);
	if (buf == NULL)
		return -ENOMEM;
	info->value.enumerated.names_ptr = (uintptr_t)buf;
	info->value.enumerated.names_length = bytes;
	p = buf;
	for (i = 0; i < items; ++i) {
		strcpy(p, labels[i]);
		p += strlen(labels[i]) + 1;
	}

	err = __snd_ctl_add_elem_set(ctl, info, element_count, member_count);

	free(buf);

	return err;
}

/**
 * \brief Create and add some user-defined control elements of bytes type.
 * \param ctl A handle of backend module for control interface.
 * \param info Common information for a new element set, with ID of the first new
 *	       element.
 * \param element_count The number of elements added by this operation.
 * \param member_count The number of members which a element has to
 *			   represent its states.
 * \return Zero on success, otherwise a negative error code.
 *
 * This function creates some user elements with bytes type. These elements are
 * not controlled by device drivers in kernel. They can be operated by the same
 * way as usual elements added by the device drivers.
 *
 * The name field of \a id must be set with unique value to identify new control
 * elements. After returning, all fields of \a id are filled. A element can be
 * identified by the combination of name and index, or by numid.
 *
 * All of members in the new elements are locked. The value of each member is
 * initialized with the minimum value.
 *
 * \par Errors:
 * <dl>
 * <dt>-EBUSY
 * <dd>A element with ID \a id already exists.
 * <dt>-EINVAL
 * <dd>Some arguments include invalid value; i.e. ID has no name, or the number
 *     of members is not between 1 to 511.
 * <dt>-ENOMEM
 * <dd>Out of memory, or there are too many user elements.
 * <dt>-ENXIO
 * <dd>This backend module does not support user elements of bytes type.
 * <dt>-ENODEV
 * <dd>Device unplugged.
 * </dl>
 *
 * \par Compatibility:
 * This function is added in version 1.1.2.
 */
int snd_ctl_add_bytes_elem_set(snd_ctl_t *ctl, snd_ctl_elem_info_t *info,
			       unsigned int element_count,
			       unsigned int member_count)
{
	if (info == NULL)
		return -EINVAL;

	info->type = SND_CTL_ELEM_TYPE_BYTES;

	return __snd_ctl_add_elem_set(ctl, info, element_count, member_count);
}

/**
 * \brief Create and add an user-defined control element of integer type.
 *
 * This is a wrapper function to snd_ctl_add_integer_elem_set() for a control
 * element. This doesn't fill the id data with full information, thus it's
 * recommended to use snd_ctl_add_integer_elem_set(), instead.
 */
int snd_ctl_elem_add_integer(snd_ctl_t *ctl, const snd_ctl_elem_id_t *id,
			     unsigned int member_count,
			     long min, long max, long step)
{
	snd_ctl_elem_info_t info = {0};

	assert(ctl && id && id->name[0]);

	info.id = *id;

	return snd_ctl_add_integer_elem_set(ctl, &info, 1, member_count,
					    min, max, step);
}

/**
 * \brief Create and add an user-defined control element of integer64 type.
 *
 * This is a wrapper function to snd_ctl_add_integer64_elem_set() for a single
 * control element. This doesn't fill the id data with full information, thus
 * it's recommended to use snd_ctl_add_integer64_elem_set(), instead.
 */
int snd_ctl_elem_add_integer64(snd_ctl_t *ctl, const snd_ctl_elem_id_t *id,
			       unsigned int member_count,
			       long long min, long long max, long long step)
{
	snd_ctl_elem_info_t info = {0};

	assert(ctl && id && id->name[0]);

	info.id = *id;

	return snd_ctl_add_integer64_elem_set(ctl, &info, 1, member_count,
					      min, max, step);
}

/**
 * \brief Create and add an user-defined control element of boolean type.
 *
 * This is a wrapper function to snd_ctl_add_boolean_elem_set() for a single
 * control element. This doesn't fill the id data with full information, thus
 * it's recommended to use snd_ctl_add_boolean_elem_set(), instead.
 */
int snd_ctl_elem_add_boolean(snd_ctl_t *ctl, const snd_ctl_elem_id_t *id,
			     unsigned int member_count)
{
	snd_ctl_elem_info_t info = {0};

	assert(ctl && id && id->name[0]);

	info.id = *id;

	return snd_ctl_add_boolean_elem_set(ctl, &info, 1, member_count);
}

/**
 * \brief Create and add a user-defined control element of enumerated type.
 *
 * This is a wrapper function to snd_ctl_add_enumerated_elem_set() for a single
 * control element. This doesn't fill the id data with full information, thus
 * it's recommended to use snd_ctl_add_enumerated_elem_set(), instead.
 *
 * This function is added in version 1.0.25.
 */
int snd_ctl_elem_add_enumerated(snd_ctl_t *ctl, const snd_ctl_elem_id_t *id,
				unsigned int member_count, unsigned int items,
				const char *const labels[])
{
	snd_ctl_elem_info_t info = {0};

	assert(ctl && id && id->name[0] && labels);

	info.id = *id;

	return snd_ctl_add_enumerated_elem_set(ctl, &info, 1, member_count,
					       items, labels);
}

/**
 * \brief Create and add a user-defined control element of IEC958 type.
 * \param[in] ctl A handle of backend module for control interface.
 * \param[in,out] id ID of the new control element.
 *
 * This function creates an user element with IEC958 type. This element is not
 * controlled by device drivers in kernel. It can be operated by the same way as
 * usual elements added by the device drivers.
 *
 * The name field of \a id must be set with unique value to identify a new
 * control element. After returning, all fields of \a id are filled. A element
 * can be identified by the combination of name and index, or by numid.
 *
 * A member in the new element is locked and filled with zero.
 *
 * \par Errors:
 * <dl>
 * <dt>-EBUSY
 * <dd>A control element with ID \a id already exists.
 * <dt>-EINVAL
 * <dd>ID has no name.
 * <dt>-ENOMEM
 * <dd>Out of memory, or there are too many user elements.
 * <dt>-ENXIO
 * <dd>This backend module does not support user elements of IEC958 type.
 * <dt>-ENODEV
 * <dd>Device unplugged.
 * </dl>
 */
int snd_ctl_elem_add_iec958(snd_ctl_t *ctl, const snd_ctl_elem_id_t *id)
{
	snd_ctl_elem_info_t info = {0};

	assert(ctl && id && id->name[0]);

	info.id = *id;
	info.type = SND_CTL_ELEM_TYPE_IEC958;
	info.owner = 1;
	info.count = 1;
	return ctl->ops->element_add(ctl, &info);
}

/**
 * \brief Remove an user CTL element
 * \param ctl CTL handle
 * \param id CTL element identification
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_elem_remove(snd_ctl_t *ctl, snd_ctl_elem_id_t *id)
{
	assert(ctl && id && (id->name[0] || id->numid));
	return ctl->ops->element_remove(ctl, id);
}

/**
 * \brief Get CTL element value.
 *
 * Read information from sound card. You must set the ID of the
 * element before calling this function.
 *
 * See snd_ctl_elem_value_t for details.
 *
 * \param ctl CTL handle.
 * \param data The element value. The ID must be set before calling
 *             the function, and the actual value will be returned
 *             here.
 *
 * \return 0 on success otherwise a negative error code.
 */
int snd_ctl_elem_read(snd_ctl_t *ctl, snd_ctl_elem_value_t *data)
{
	assert(ctl && data && (data->id.name[0] || data->id.numid));
	return ctl->ops->element_read(ctl, data);
}

/**
 * \brief Set CTL element value.
 *
 * Write new value(s) to the sound card. You must set the ID and the
 * value of the element before calling this function.
 *
 * See snd_ctl_elem_value_t for details.
 *
 * \param ctl CTL handle.
 * \param data The new value.
 *
 * \retval 0 on success
 * \retval >0 on success when value was changed
 * \retval <0 a negative error code
 */
int snd_ctl_elem_write(snd_ctl_t *ctl, snd_ctl_elem_value_t *data)
{
	assert(ctl && data && (data->id.name[0] || data->id.numid));
	return ctl->ops->element_write(ctl, data);
}

static int snd_ctl_tlv_do(snd_ctl_t *ctl, int op_flag,
			  const snd_ctl_elem_id_t *id,
		          unsigned int *tlv, unsigned int tlv_size)
{
	snd_ctl_elem_info_t *info = NULL;
	int err;

	if (id->numid == 0) {
		info = calloc(1, sizeof(*info));
		if (info == NULL)
			return -ENOMEM;
		info->id = *id;
		id = &info->id;
		err = snd_ctl_elem_info(ctl, info);
		if (err < 0)
			goto __err;
		if (id->numid == 0) {
			err = -ENOENT;
			goto __err;
		}
	}
	err = ctl->ops->element_tlv(ctl, op_flag, id->numid, tlv, tlv_size);
      __err:
      	if (info)
      		free(info);
	return err;
}

/**
 * \brief Read structured data from an element set to given buffer.
 * \param ctl A handle of backend module for control interface.
 * \param id ID of an element.
 * \param tlv An array with members of unsigned int type.
 * \param tlv_size The length of the array.
 * \return 0 on success otherwise a negative error code
 *
 * The format of an array of \a tlv argument is:
 *   tlv[0]:   Type. One of SND_CTL_TLVT_XXX.
 *   tlv[1]:   Length. The length of value in units of byte.
 *   tlv[2..]: Value. Depending on the type.
 *
 * Details are described in <sound/tlv.h>.
 */
int snd_ctl_elem_tlv_read(snd_ctl_t *ctl, const snd_ctl_elem_id_t *id,
			  unsigned int *tlv, unsigned int tlv_size)
{
	int err;
	assert(ctl && id && (id->name[0] || id->numid) && tlv);
	if (tlv_size < 2 * sizeof(int))
		return -EINVAL;
	/* 1.0.12 driver doesn't return the error even if the user TLV
	 * is empty.  So, initialize TLV here with an invalid type
	 * and compare the returned value after ioctl for checking
	 * the validity of TLV.
	 */
	tlv[SNDRV_CTL_TLVO_TYPE] = -1;
	tlv[SNDRV_CTL_TLVO_LEN] = 0;
	err = snd_ctl_tlv_do(ctl, 0, id, tlv, tlv_size);
	if (err >= 0 && tlv[SNDRV_CTL_TLVO_TYPE] == (unsigned int)-1)
		err = -ENXIO;
	return err;
}

/**
 * \brief Write structured data from given buffer to an element set.
 * \param ctl A handle of backend module for control interface.
 * \param id ID of an element.
 * \param tlv An array with members of unsigned int type. The second member
 *	      must represent total bytes of the rest of array.
 * \retval 0 on success
 * \retval >0 on success when value was changed
 * \retval <0 a negative error code
 *
 * The format of an array of \a tlv argument is:
 *   tlv[0]:   Type. One of SND_CTL_TLVT_XXX.
 *   tlv[1]:   Length. The length of value in units of byte.
 *   tlv[2..]: Value. Depending on the type.
 *
 * Details are described in <sound/tlv.h>.
 */
int snd_ctl_elem_tlv_write(snd_ctl_t *ctl, const snd_ctl_elem_id_t *id,
			   const unsigned int *tlv)
{
	assert(ctl && id && (id->name[0] || id->numid) && tlv);
	return snd_ctl_tlv_do(ctl, 1, id, (unsigned int *)tlv,
			tlv[SNDRV_CTL_TLVO_LEN] + 2 * sizeof(unsigned int));
}

/**
 * \brief Process structured data from given buffer for an element set.
 * \param ctl A handle of backend module for control interface.
 * \param id ID of an element.
 * \param tlv An array with members of unsigned int type. The second member
 *	      must represent total bytes of the rest of array.
 * \retval 0 on success
 * \retval >0 on success when value was changed
 * \retval <0 a negative error code
 *
 * The format of an array of \a tlv argument is:
 *   tlv[0]:   Type. One of SND_CTL_TLVT_XXX.
 *   tlv[1]:   Length. The length of value in units of byte.
 *   tlv[2..]: Value. Depending on the type.
 *
 * Details are described in <sound/tlv.h>.
 */
int snd_ctl_elem_tlv_command(snd_ctl_t *ctl, const snd_ctl_elem_id_t *id,
			     const unsigned int *tlv)
{
	assert(ctl && id && (id->name[0] || id->numid) && tlv);
	return snd_ctl_tlv_do(ctl, -1, id, (unsigned int *)tlv,
			tlv[SNDRV_CTL_TLVO_LEN] + 2 * sizeof(unsigned int));
}

/**
 * \brief Lock CTL element
 * \param ctl CTL handle
 * \param id CTL element id pointer
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_elem_lock(snd_ctl_t *ctl, snd_ctl_elem_id_t *id)
{
	assert(ctl && id);
	return ctl->ops->element_lock(ctl, id);
}

/**
 * \brief Unlock CTL element
 * \param ctl CTL handle
 * \param id CTL element id pointer
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_elem_unlock(snd_ctl_t *ctl, snd_ctl_elem_id_t *id)
{
	assert(ctl && id);
	return ctl->ops->element_unlock(ctl, id);
}

/**
 * \brief Get next hardware dependent device number
 * \param ctl CTL handle
 * \param device current device on entry and next device on return
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_hwdep_next_device(snd_ctl_t *ctl, int *device)
{
	assert(ctl && device);
	return ctl->ops->hwdep_next_device(ctl, device);
}

/**
 * \brief Get info about a hardware dependent device
 * \param ctl CTL handle
 * \param info Hardware dependent device id/info pointer
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_hwdep_info(snd_ctl_t *ctl, snd_hwdep_info_t * info)
{
	assert(ctl && info);
	return ctl->ops->hwdep_info(ctl, info);
}

/**
 * \brief Get next PCM device number
 * \param ctl CTL handle
 * \param device current device on entry and next device on return
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_pcm_next_device(snd_ctl_t *ctl, int * device)
{
	assert(ctl && device);
	return ctl->ops->pcm_next_device(ctl, device);
}

/**
 * \brief Get info about a PCM device
 * \param ctl CTL handle
 * \param info PCM device id/info pointer
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_pcm_info(snd_ctl_t *ctl, snd_pcm_info_t * info)
{
	assert(ctl && info);
	return ctl->ops->pcm_info(ctl, info);
}

/**
 * \brief Set preferred PCM subdevice number of successive PCM open
 * \param ctl CTL handle
 * \param subdev Preferred PCM subdevice number
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_pcm_prefer_subdevice(snd_ctl_t *ctl, int subdev)
{
	assert(ctl);
	return ctl->ops->pcm_prefer_subdevice(ctl, subdev);
}

/**
 * \brief Get next RawMidi device number
 * \param ctl CTL handle
 * \param device current device on entry and next device on return
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_rawmidi_next_device(snd_ctl_t *ctl, int * device)
{
	assert(ctl && device);
	return ctl->ops->rawmidi_next_device(ctl, device);
}

/**
 * \brief Get info about a RawMidi device
 * \param ctl CTL handle
 * \param info RawMidi device id/info pointer
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_rawmidi_info(snd_ctl_t *ctl, snd_rawmidi_info_t * info)
{
	assert(ctl && info);
	return ctl->ops->rawmidi_info(ctl, info);
}

/**
 * \brief Set preferred RawMidi subdevice number of successive RawMidi open
 * \param ctl CTL handle
 * \param subdev Preferred RawMidi subdevice number
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_rawmidi_prefer_subdevice(snd_ctl_t *ctl, int subdev)
{
	assert(ctl);
	return ctl->ops->rawmidi_prefer_subdevice(ctl, subdev);
}

/**
 * \brief Set Power State to given SND_CTL_POWER_* value and do the power management
 * \param ctl CTL handle
 * \param state Desired Power State
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_set_power_state(snd_ctl_t *ctl, unsigned int state)
{
	assert(ctl);
	if (ctl->ops->set_power_state)
		return ctl->ops->set_power_state(ctl, state);
	return -ENXIO;
}

/**
 * \brief Get actual Power State
 * \param ctl CTL handle
 * \param state Destination value
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_get_power_state(snd_ctl_t *ctl, unsigned int *state)
{
	assert(ctl);
	if (ctl->ops->get_power_state)
		return ctl->ops->get_power_state(ctl, state);
	return -ENXIO;
}

/**
 * \brief Read an event
 * \param ctl CTL handle
 * \param event Event pointer
 * \return number of events read otherwise a negative error code on failure
 */
int snd_ctl_read(snd_ctl_t *ctl, snd_ctl_event_t *event)
{
	assert(ctl && event);
	return (ctl->ops->read)(ctl, event);
}

/**
 * \brief Wait for a CTL to become ready (i.e. at least one event pending)
 * \param ctl CTL handle
 * \param timeout maximum time in milliseconds to wait
 * \return 0 otherwise a negative error code on failure
 */
int snd_ctl_wait(snd_ctl_t *ctl, int timeout)
{
	struct pollfd *pfd;
	unsigned short revents;
	int npfds, err, err_poll;

	npfds = snd_ctl_poll_descriptors_count(ctl);
	if (npfds <= 0 || npfds >= 16) {
		SNDERR("Invalid poll_fds %d\n", npfds);
		return -EIO;
	}
	pfd = alloca(sizeof(*pfd) * npfds);
	err = snd_ctl_poll_descriptors(ctl, pfd, npfds);
	if (err < 0)
		return err;
	if (err != npfds) {
		SNDMSG("invalid poll descriptors %d\n", err);
		return -EIO;
	}
	for (;;) {
		err_poll = poll(pfd, npfds, timeout);
		if (err_poll < 0)
			return -errno;
		if (! err_poll)
			return 0;
		err = snd_ctl_poll_descriptors_revents(ctl, pfd, npfds, &revents);
		if (err < 0)
			return err;
		if (revents & (POLLERR | POLLNVAL))
			return -EIO;
		if (revents & (POLLIN | POLLOUT))
			return 1;
	}
}

/**
 * \brief Add an async handler for a CTL
 * \param handler Returned handler handle
 * \param ctl CTL handle
 * \param callback Callback function
 * \param private_data Callback private data
 * \return 0 otherwise a negative error code on failure
 */
int snd_async_add_ctl_handler(snd_async_handler_t **handler, snd_ctl_t *ctl, 
			      snd_async_callback_t callback, void *private_data)
{
	int err;
	int was_empty;
	snd_async_handler_t *h;
	err = snd_async_add_handler(&h, _snd_ctl_async_descriptor(ctl),
				    callback, private_data);
	if (err < 0)
		return err;
	h->type = SND_ASYNC_HANDLER_CTL;
	h->u.ctl = ctl;
	was_empty = list_empty(&ctl->async_handlers);
	list_add_tail(&h->hlist, &ctl->async_handlers);
	if (was_empty) {
		err = snd_ctl_async(ctl, snd_async_handler_get_signo(h), getpid());
		if (err < 0) {
			snd_async_del_handler(h);
			return err;
		}
	}
	*handler = h;
	return 0;
}

/**
 * \brief Return CTL handle related to an async handler
 * \param handler Async handler handle
 * \return CTL handle
 */
snd_ctl_t *snd_async_handler_get_ctl(snd_async_handler_t *handler)
{
	assert(handler->type == SND_ASYNC_HANDLER_CTL);
	return handler->u.ctl;
}

static const char *const build_in_ctls[] = {
	"hw", "empty", "remap", "shm", NULL
};

static int snd_ctl_open_conf(snd_ctl_t **ctlp, const char *name,
			     snd_config_t *ctl_root, snd_config_t *ctl_conf, int mode)
{
	const char *str;
	char *buf = NULL, *buf1 = NULL;
	int err;
	snd_config_t *conf, *type_conf = NULL;
	snd_config_iterator_t i, next;
	const char *lib = NULL, *open_name = NULL;
	const char *id;
	int (*open_func)(snd_ctl_t **, const char *, snd_config_t *, snd_config_t *, int) = NULL;
#ifndef PIC
	extern void *snd_control_open_symbols(void);
#endif
	if (snd_config_get_type(ctl_conf) != SND_CONFIG_TYPE_COMPOUND) {
		if (name)
			SNDERR("Invalid type for CTL %s definition", name);
		else
			SNDERR("Invalid type for CTL definition");
		return -EINVAL;
	}
	err = snd_config_search(ctl_conf, "type", &conf);
	if (err < 0) {
		SNDERR("type is not defined");
		return err;
	}
	err = snd_config_get_id(conf, &id);
	if (err < 0) {
		SNDERR("unable to get id");
		return err;
	}
	err = snd_config_get_string(conf, &str);
	if (err < 0) {
		SNDERR("Invalid type for %s", id);
		return err;
	}
	err = snd_config_search_definition(ctl_root, "ctl_type", str, &type_conf);
	if (err >= 0) {
		if (snd_config_get_type(type_conf) != SND_CONFIG_TYPE_COMPOUND) {
			SNDERR("Invalid type for CTL type %s definition", str);
			err = -EINVAL;
			goto _err;
		}
		snd_config_for_each(i, next, type_conf) {
			snd_config_t *n = snd_config_iterator_entry(i);
			const char *id;
			if (snd_config_get_id(n, &id) < 0)
				continue;
			if (strcmp(id, "comment") == 0)
				continue;
			if (strcmp(id, "lib") == 0) {
				err = snd_config_get_string(n, &lib);
				if (err < 0) {
					SNDERR("Invalid type for %s", id);
					goto _err;
				}
				continue;
			}
			if (strcmp(id, "open") == 0) {
				err = snd_config_get_string(n, &open_name);
				if (err < 0) {
					SNDERR("Invalid type for %s", id);
					goto _err;
				}
				continue;
			}
			SNDERR("Unknown field %s", id);
			err = -EINVAL;
			goto _err;
		}
	}
	if (!open_name) {
		buf = malloc(strlen(str) + 32);
		if (buf == NULL) {
			err = -ENOMEM;
			goto _err;
		}
		open_name = buf;
		sprintf(buf, "_snd_ctl_%s_open", str);
	}
	if (!lib) {
		const char *const *build_in = build_in_ctls;
		while (*build_in) {
			if (!strcmp(*build_in, str))
				break;
			build_in++;
		}
		if (*build_in == NULL) {
			buf1 = malloc(strlen(str) + 32);
			if (buf1 == NULL) {
				err = -ENOMEM;
				goto _err;
			}
			lib = buf1;
			sprintf(buf1, "libasound_module_ctl_%s.so", str);
		}
	}
#ifndef PIC
	snd_control_open_symbols();
#endif
	open_func = snd_dlobj_cache_get(lib, open_name,
			SND_DLSYM_VERSION(SND_CONTROL_DLSYM_VERSION), 1);
	if (open_func) {
		err = open_func(ctlp, name, ctl_root, ctl_conf, mode);
		if (err >= 0) {
			(*ctlp)->open_func = open_func;
			err = 0;
		} else {
			snd_dlobj_cache_put(open_func);
		}
	} else {
		err = -ENXIO;
	}
       _err:
	if (type_conf)
		snd_config_delete(type_conf);
	free(buf);
	free(buf1);
	return err;
}

static int snd_ctl_open_noupdate(snd_ctl_t **ctlp, snd_config_t *root,
				 const char *name, int mode, int hop)
{
	int err;
	snd_config_t *ctl_conf;
	const char *str;

	err = snd_config_search_definition(root, "ctl", name, &ctl_conf);
	if (err < 0) {
		SNDERR("Invalid CTL %s", name);
		return err;
	}
	if (snd_config_get_string(ctl_conf, &str) >= 0)
		err = snd_ctl_open_noupdate(ctlp, root, str, mode, hop + 1);
	else {
		snd_config_set_hop(ctl_conf, hop);
		err = snd_ctl_open_conf(ctlp, name, root, ctl_conf, mode);
	}
	snd_config_delete(ctl_conf);
	return err;
}

#ifndef DOC_HIDDEN
int _snd_ctl_open_named_child(snd_ctl_t **pctl, const char *name,
			      snd_config_t *root, snd_config_t *conf,
			      int mode, snd_config_t *parent_conf)
{
	const char *str;
	int hop;

	if ((hop = snd_config_check_hop(parent_conf)) < 0)
		return hop;
	if (snd_config_get_string(conf, &str) >= 0)
		return snd_ctl_open_noupdate(pctl, root, str, mode, hop + 1);
	return snd_ctl_open_conf(pctl, name, root, conf, mode);
}
#endif

/**
 * \brief Opens a sound card.
 *
 * \param ctlp Returned CTL handle.
 * \param name A string identifying the card (See \ref control_cards_id).
 * \param mode Open mode (see #SND_CTL_NONBLOCK, #SND_CTL_ASYNC).
 *
 * \return 0 on success otherwise a negative error code.
 */
int snd_ctl_open(snd_ctl_t **ctlp, const char *name, int mode)
{
	snd_config_t *top;
	int err;

	assert(ctlp && name);
	if (_snd_is_ucm_device(name)) {
		name = uc_mgr_alibcfg_by_device(&top, name);
		if (name == NULL)
			return -ENODEV;
	} else {
		err = snd_config_update_ref(&top);
		if (err < 0)
			return err;
	}
	err = snd_ctl_open_noupdate(ctlp, top, name, mode, 0);
	snd_config_unref(top);
	return err;
}

/**
 * \brief Opens a CTL using local configuration
 * \param ctlp Returned CTL handle
 * \param name ASCII identifier of the CTL handle
 * \param mode Open mode (see #SND_CTL_NONBLOCK, #SND_CTL_ASYNC)
 * \param lconf Local configuration
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_open_lconf(snd_ctl_t **ctlp, const char *name,
		       int mode, snd_config_t *lconf)
{
	assert(ctlp && name && lconf);
	return snd_ctl_open_noupdate(ctlp, lconf, name, mode, 0);
}

/**
 * \brief Opens a fallback CTL
 * \param ctlp Returned CTL handle
 * \param root Configuration root
 * \param name ASCII identifier of the CTL handle used as fallback
 * \param orig_name The original ASCII name
 * \param mode Open mode (see #SND_CTL_NONBLOCK, #SND_CTL_ASYNC)
 * \return 0 on success otherwise a negative error code
 */
int snd_ctl_open_fallback(snd_ctl_t **ctlp, snd_config_t *root,
			  const char *name, const char *orig_name, int mode)
{
	int err;
	assert(ctlp && name && root);
	err = snd_ctl_open_noupdate(ctlp, root, name, mode, 0);
	if (err >= 0) {
		free((*ctlp)->name);
		(*ctlp)->name = orig_name ? strdup(orig_name) : NULL;
	}
	return err;
}

#ifndef DOC_HIDDEN
#define TYPE(v) [SND_CTL_ELEM_TYPE_##v] = #v
#define IFACE(v) [SND_CTL_ELEM_IFACE_##v] = #v
#define IFACE1(v, n) [SND_CTL_ELEM_IFACE_##v] = #n
#define EVENT(v) [SND_CTL_EVENT_##v] = #v

static const char *const snd_ctl_elem_type_names[] = {
	TYPE(NONE),
	TYPE(BOOLEAN),
	TYPE(INTEGER),
	TYPE(ENUMERATED),
	TYPE(BYTES),
	TYPE(IEC958),
	TYPE(INTEGER64),
};

static const char *const snd_ctl_elem_iface_names[] = {
	IFACE(CARD),
	IFACE(HWDEP),
	IFACE(MIXER),
	IFACE(PCM),
	IFACE(RAWMIDI),
	IFACE(TIMER),
	IFACE(SEQUENCER),
};

static const char *const snd_ctl_event_type_names[] = {
	EVENT(ELEM),
};
#endif

/**
 * \brief get name of a CTL element type
 * \param type CTL element type
 * \return ascii name of CTL element type
 */
const char *snd_ctl_elem_type_name(snd_ctl_elem_type_t type)
{
	assert(type <= SND_CTL_ELEM_TYPE_LAST);
	return snd_ctl_elem_type_names[type];
}

/**
 * \brief get name of a CTL element related interface
 * \param iface CTL element related interface
 * \return ascii name of CTL element related interface
 */
const char *snd_ctl_elem_iface_name(snd_ctl_elem_iface_t iface)
{
	assert(iface <= SND_CTL_ELEM_IFACE_LAST);
	return snd_ctl_elem_iface_names[iface];
}

/**
 * \brief get name of a CTL event type
 * \param type CTL event type
 * \return ascii name of CTL event type
 */
const char *snd_ctl_event_type_name(snd_ctl_event_type_t type)
{
	assert(type <= SND_CTL_EVENT_LAST);
	return snd_ctl_event_type_names[type];
}

/**
 * \brief allocate space for CTL element identifiers list
 *
 * The space can be released with snd_ctl_elem_list_free_space().
 *
 * \param obj CTL element identifiers list.
 * \param entries How many entries to allocate. See
 *        #snd_ctl_elem_list_t to learn how to obtain
 *        this number in advance.
 * \return 0 on success otherwise a negative error code.
 */
int snd_ctl_elem_list_alloc_space(snd_ctl_elem_list_t *obj, unsigned int entries)
{
	free(obj->pids);
	obj->pids = calloc(entries, sizeof(*obj->pids));
	if (!obj->pids) {
		obj->space = 0;
		return -ENOMEM;
	}
	obj->space = entries;
	return 0;
}  

/**
 * \brief free previously allocated space for CTL element identifiers list
 *
 * Releases space previously allocated using
 * snd_ctl_elem_list_alloc_space().
 *
 * \param obj CTL element identifiers list
 */
void snd_ctl_elem_list_free_space(snd_ctl_elem_list_t *obj)
{
	free(obj->pids);
	obj->pids = NULL;
	obj->space = 0;
}

/**
 * \brief Get event mask for an element related event
 * \param obj CTL event
 * \return event mask for element related event
 */
unsigned int snd_ctl_event_elem_get_mask(const snd_ctl_event_t *obj)
{
	assert(obj);
	assert(obj->type == SND_CTL_EVENT_ELEM);
	return obj->data.elem.mask;
}

/**
 * \brief Get CTL element identifier for an element related event
 * \param obj CTL event
 * \param ptr Pointer to returned CTL element identifier
 */
void snd_ctl_event_elem_get_id(const snd_ctl_event_t *obj, snd_ctl_elem_id_t *ptr)
{
	assert(obj && ptr);
	assert(obj->type == SND_CTL_EVENT_ELEM);
	*ptr = obj->data.elem.id;
}

/**
 * \brief Get element numeric identifier for an element related event
 * \param obj CTL event
 * \return element numeric identifier
 */
unsigned int snd_ctl_event_elem_get_numid(const snd_ctl_event_t *obj)
{
	assert(obj);
	assert(obj->type == SND_CTL_EVENT_ELEM);
	return obj->data.elem.id.numid;
}

/**
 * \brief Get interface part of CTL element identifier for an element related event
 * \param obj CTL event
 * \return interface part of element identifier
 */
snd_ctl_elem_iface_t snd_ctl_event_elem_get_interface(const snd_ctl_event_t *obj)
{
	assert(obj);
	assert(obj->type == SND_CTL_EVENT_ELEM);
	return obj->data.elem.id.iface;
}

/**
 * \brief Get device part of CTL element identifier for an element related event
 * \param obj CTL event
 * \return device part of element identifier
 */
unsigned int snd_ctl_event_elem_get_device(const snd_ctl_event_t *obj)
{
	assert(obj);
	assert(obj->type == SND_CTL_EVENT_ELEM);
	return obj->data.elem.id.device;
}

/**
 * \brief Get subdevice part of CTL element identifier for an element related event
 * \param obj CTL event
 * \return subdevice part of element identifier
 */
unsigned int snd_ctl_event_elem_get_subdevice(const snd_ctl_event_t *obj)
{
	assert(obj);
	assert(obj->type == SND_CTL_EVENT_ELEM);
	return obj->data.elem.id.subdevice;
}

/**
 * \brief Get name part of CTL element identifier for an element related event
 * \param obj CTL event
 * \return name part of element identifier
 */
const char *snd_ctl_event_elem_get_name(const snd_ctl_event_t *obj)
{
	assert(obj);
	assert(obj->type == SND_CTL_EVENT_ELEM);
	return (const char *)obj->data.elem.id.name;
}

/**
 * \brief Get index part of CTL element identifier for an element related event
 * \param obj CTL event
 * \return index part of element identifier
 */
unsigned int snd_ctl_event_elem_get_index(const snd_ctl_event_t *obj)
{
	assert(obj);
	assert(obj->type == SND_CTL_EVENT_ELEM);
	return obj->data.elem.id.index;
}

#ifndef DOC_HIDDEN
int _snd_ctl_poll_descriptor(snd_ctl_t *ctl)
{
	assert(ctl);
	return ctl->poll_fd;
}
#endif

/**
 * \brief get size of #snd_ctl_elem_id_t
 * \return size in bytes
 */
size_t snd_ctl_elem_id_sizeof()
{
	return sizeof(snd_ctl_elem_id_t);
}

/**
 * \brief allocate an invalid #snd_ctl_elem_id_t using standard malloc
 * \param ptr returned pointer
 * \return 0 on success otherwise negative error code
 */
int snd_ctl_elem_id_malloc(snd_ctl_elem_id_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_ctl_elem_id_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

/**
 * \brief frees a previously allocated #snd_ctl_elem_id_t
 * \param obj pointer to object to free
 */
void snd_ctl_elem_id_free(snd_ctl_elem_id_t *obj)
{
	free(obj);
}

/**
 * \brief clear given #snd_ctl_elem_id_t object
 * \param obj pointer to object to clear
 */
void snd_ctl_elem_id_clear(snd_ctl_elem_id_t *obj)
{
	memset(obj, 0, sizeof(snd_ctl_elem_id_t));
}

/**
 * \brief copy one #snd_ctl_elem_id_t to another
 * \param dst pointer to destination
 * \param src pointer to source
 */
void snd_ctl_elem_id_copy(snd_ctl_elem_id_t *dst, const snd_ctl_elem_id_t *src)
{
	assert(dst && src);
	*dst = *src;
}

/**
 * \brief compare one #snd_ctl_elem_id_t to another using numid
 * \param id1 pointer to first id
 * \param id2 pointer to second id
 * \retval zero when values are identical, other value on a difference (like strcmp)
 *
 * This comparison ignores the set of fields part.
 *
 * The return value can be used for sorting like qsort(). It gives persistent
 * results.
 */
int snd_ctl_elem_id_compare_numid(const snd_ctl_elem_id_t *id1, const snd_ctl_elem_id_t *id2)
{
	int64_t d;

	assert(id1 && id2);
	d = (int64_t)id1->numid - (int64_t)id2->numid;
	if (d & ((int64_t)INT_MAX + 1)) {	/* fast path */
		if (d > INT_MAX)
			d = INT_MAX;
		else if (d < INT_MIN)
			d = INT_MIN;
	}
	return d;
}

/**
 * \brief compare one #snd_ctl_elem_id_t to another
 * \param id1 pointer to first id
 * \param id2 pointer to second id
 * \retval zero when values are identical, other value on a difference (like strcmp)
 *
 * This comparison ignores the numid part. The numid comparison can be easily
 * implemented using snd_ctl_elem_id_get_numid() calls.
 *
 * The identifier set fields are compared in this order: interface, device,
 * subdevice, name, index.
 *
 * The return value can be used for sorting like qsort(). It gives persistent
 * results.
 */
int snd_ctl_elem_id_compare_set(const snd_ctl_elem_id_t *id1, const snd_ctl_elem_id_t *id2)
{
	int d;

	assert(id1 && id2);
	/* although those values are unsigned integer, practically, */
	/* the useable limit is really much lower */
	assert((id1->iface | id1->device | id1->subdevice | id1->index) <= INT_MAX);
	assert((id2->iface | id2->device | id2->subdevice | id1->index) <= INT_MAX);
	d = id1->iface - id2->iface;
	if (d != 0)
		return d;
	d = id1->device - id2->device;
	if (d != 0)
		return d;
	d = id1->subdevice - id2->subdevice;
	if (d != 0)
		return d;
	d = strcmp((const char *)id1->name, (const char *)id2->name);
	if (d != 0)
		return d;
	return id1->index - id2->index;
}

/**
 * \brief Get numeric identifier from a CTL element identifier
 * \param obj CTL element identifier
 * \return CTL element numeric identifier
 */
unsigned int snd_ctl_elem_id_get_numid(const snd_ctl_elem_id_t *obj)
{
	assert(obj);
	return obj->numid;
}

/**
 * \brief Get interface part of a CTL element identifier
 * \param obj CTL element identifier
 * \return CTL element related interface
 */
snd_ctl_elem_iface_t snd_ctl_elem_id_get_interface(const snd_ctl_elem_id_t *obj)
{
	assert(obj);
	return obj->iface;
}

/**
 * \brief Get device part of a CTL element identifier
 * \param obj CTL element identifier
 * \return CTL element related device
 */
unsigned int snd_ctl_elem_id_get_device(const snd_ctl_elem_id_t *obj)
{
	assert(obj);
	return obj->device;
}

/**
 * \brief Get subdevice part of a CTL element identifier
 * \param obj CTL element identifier
 * \return CTL element related subdevice
 */
unsigned int snd_ctl_elem_id_get_subdevice(const snd_ctl_elem_id_t *obj)
{
	assert(obj);
	return obj->subdevice;
}

/**
 * \brief Get name part of a CTL element identifier
 * \param obj CTL element identifier
 * \return CTL element name
 */
const char *snd_ctl_elem_id_get_name(const snd_ctl_elem_id_t *obj)
{
	assert(obj);
	return (const char *)obj->name;
}

/**
 * \brief Get index part of a CTL element identifier
 * \param obj CTL element identifier
 * \return CTL element index
 */
unsigned int snd_ctl_elem_id_get_index(const snd_ctl_elem_id_t *obj)
{
	assert(obj);
	return obj->index;
}

/**
 * \brief Set numeric identifier for a CTL element identifier
 * \param obj CTL element identifier
 * \param val CTL element numeric identifier
 */
void snd_ctl_elem_id_set_numid(snd_ctl_elem_id_t *obj, unsigned int val)
{
	assert(obj);
	obj->numid = val;
}

/**
 * \brief Set interface part for a CTL element identifier
 * \param obj CTL element identifier
 * \param val CTL element related interface
 */
void snd_ctl_elem_id_set_interface(snd_ctl_elem_id_t *obj, snd_ctl_elem_iface_t val)
{
	assert(obj);
	obj->iface = val;
}

/**
 * \brief Set device part for a CTL element identifier
 * \param obj CTL element identifier
 * \param val CTL element related device
 */
void snd_ctl_elem_id_set_device(snd_ctl_elem_id_t *obj, unsigned int val)
{
	assert(obj);
	obj->device = val;
}

/**
 * \brief Set subdevice part for a CTL element identifier
 * \param obj CTL element identifier
 * \param val CTL element related subdevice
 */
void snd_ctl_elem_id_set_subdevice(snd_ctl_elem_id_t *obj, unsigned int val)
{
	assert(obj);
	obj->subdevice = val;
}

/**
 * \brief Set name part for a CTL element identifier
 * \param obj CTL element identifier
 * \param val CTL element name
 */
void snd_ctl_elem_id_set_name(snd_ctl_elem_id_t *obj, const char *val)
{
	assert(obj);
	snd_strlcpy((char *)obj->name, val, sizeof(obj->name));
}

/**
 * \brief Set index part for a CTL element identifier
 * \param obj CTL element identifier
 * \param val CTL element index
 */
void snd_ctl_elem_id_set_index(snd_ctl_elem_id_t *obj, unsigned int val)
{
	assert(obj);
	obj->index = val;
}

/**
 * \brief get size of #snd_ctl_card_info_t.
 * \return Size in bytes.
 */
size_t snd_ctl_card_info_sizeof()
{
	return sizeof(snd_ctl_card_info_t);
}

/**
 * \brief Allocate an invalid #snd_ctl_card_info_t on the heap.
 *
 * Allocate space for a card info object on the heap. The allocated memory
 * must be freed using snd_ctl_card_info_free().
 *
 * See snd_ctl_card_info_t for details.
 *
 * \param ptr Pointer to a snd_ctl_card_info_t pointer. The address
 *            of the allocated space will be returned here.
 * \return 0 on success, otherwise a negative error code.
 */
int snd_ctl_card_info_malloc(snd_ctl_card_info_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_ctl_card_info_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

/**
 * \brief Free an #snd_ctl_card_info_t previously allocated using
 *        snd_ctl_card_info_malloc().
 *
 * \param obj Pointer to the snd_ctl_card_info_t.
 */
void snd_ctl_card_info_free(snd_ctl_card_info_t *obj)
{
	free(obj);
}

/**
 * \brief Clear given card info object.
 *
 * See snd_ctl_elem_value_t for details.
 *
 * \param obj Card info object.
 */
void snd_ctl_card_info_clear(snd_ctl_card_info_t *obj)
{
	memset(obj, 0, sizeof(snd_ctl_card_info_t));
}

/**
 * \brief Bitwise copy of a #snd_ctl_card_info_t object.
 *
 * \param dst Pointer to destination.
 * \param src Pointer to source.
 */
void snd_ctl_card_info_copy(snd_ctl_card_info_t *dst, const snd_ctl_card_info_t *src)
{
	assert(dst && src);
	*dst = *src;
}

/**
 * \brief Get the sound card index from the given info object.
 *
 * See snd_ctl_card_info_t for more details.
 *
 * \param obj The card info object.
 * \return Sound card index.
 */
int snd_ctl_card_info_get_card(const snd_ctl_card_info_t *obj)
{
	assert(obj);
	return obj->card;
}

/**
 * \brief Get the sound card ID from the given info object.
 *
 * See snd_ctl_card_info_t for more details.
 *
 * \param obj The card info object.
 * \return Sound card ID.
 */
const char *snd_ctl_card_info_get_id(const snd_ctl_card_info_t *obj)
{
	assert(obj);
	return (const char *)obj->id;
}

/**
 * \brief Get the sound card driver from the given info object.
 *
 * See snd_ctl_card_info_t for more details.
 *
 * \param obj The card info object.
 * \return The sound card driver.
 */
const char *snd_ctl_card_info_get_driver(const snd_ctl_card_info_t *obj)
{
	assert(obj);
	return (const char *)obj->driver;
}

/**
 * \brief Get the sound card name from the given info object.
 *
 * See snd_ctl_card_info_t for more details.
 *
 * \param obj The card info object.
 * \return Sound card name.
 */
const char *snd_ctl_card_info_get_name(const snd_ctl_card_info_t *obj)
{
	assert(obj);
	return (const char *)obj->name;
}

/**
 * \brief Get the sound cards long name from the given info object.
 *
 * See snd_ctl_card_info_t for more details.
 *
 * \param obj The card info object.
 * \return Sound cards long name.
 */
const char *snd_ctl_card_info_get_longname(const snd_ctl_card_info_t *obj)
{
	assert(obj);
	return (const char *)obj->longname;
}

/**
 * \brief Get the sound card mixer name from the given info object.
 *
 * See snd_ctl_card_info_t for more details.
 *
 * \param obj The card info object.
 * \return Sound card mixer name.
 */
const char *snd_ctl_card_info_get_mixername(const snd_ctl_card_info_t *obj)
{
	assert(obj);
	return (const char *)obj->mixername;
}

/**
 * \brief Get the sound cards "components" property from the given info object.
 *
 * See snd_ctl_card_info_t for more details.
 *
 * \param obj The card info object.
 * \return Sound cards "components" property.
 */
const char *snd_ctl_card_info_get_components(const snd_ctl_card_info_t *obj)
{
	assert(obj);
	return (const char *)obj->components;
}

/**
 * \brief get size of #snd_ctl_event_t
 * \return size in bytes
 */
size_t snd_ctl_event_sizeof()
{
	return sizeof(snd_ctl_event_t);
}

/**
 * \brief allocate an invalid #snd_ctl_event_t using standard malloc
 * \param ptr returned pointer
 * \return 0 on success otherwise negative error code
 */
int snd_ctl_event_malloc(snd_ctl_event_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_ctl_event_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

/**
 * \brief frees a previously allocated #snd_ctl_event_t
 * \param obj pointer to object to free
 */
void snd_ctl_event_free(snd_ctl_event_t *obj)
{
	free(obj);
}

/**
 * \brief clear given #snd_ctl_event_t object
 * \param obj pointer to object to clear
 */
void snd_ctl_event_clear(snd_ctl_event_t *obj)
{
	memset(obj, 0, sizeof(snd_ctl_event_t));
}

/**
 * \brief copy one #snd_ctl_event_t to another
 * \param dst pointer to destination
 * \param src pointer to source
 */
void snd_ctl_event_copy(snd_ctl_event_t *dst, const snd_ctl_event_t *src)
{
	assert(dst && src);
	*dst = *src;
}

/**
 * \brief Get type of a CTL event
 * \param obj CTL event
 * \return CTL event type
 */
snd_ctl_event_type_t snd_ctl_event_get_type(const snd_ctl_event_t *obj)
{
	assert(obj);
	return obj->type;
}

/**
 * \brief get size of #snd_ctl_elem_list_t.
 * \return size in bytes
 */
size_t snd_ctl_elem_list_sizeof()
{
	return sizeof(snd_ctl_elem_list_t);
}

/**
 * \brief allocate a #snd_ctl_elem_list_t using standard malloc.
 *
 * The memory can be released using snd_ctl_elem_list_free().
 *
 * \param ptr returned pointer
 * \return 0 on success otherwise negative error code
 */
int snd_ctl_elem_list_malloc(snd_ctl_elem_list_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_ctl_elem_list_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

/**
 * \brief frees a previously allocated #snd_ctl_elem_list_t.
 *
 * Release memory previously allocated using
 * snd_ctl_elem_list_malloc().
 *
 * If you used snd_ctl_elem_list_alloc_space() on the list, you must
 * use snd_ctl_elem_list_free_space() \em before calling this
 * function.
 *
 * \param obj pointer to object to free
 */
void snd_ctl_elem_list_free(snd_ctl_elem_list_t *obj)
{
	free(obj);
}

/**
 * \brief Clear given #snd_ctl_elem_list_t object.
 *
 * This will make the stored identifiers inaccessible without freeing
 * their space.
 *
 * \warning The element identifier space cannot be freed after calling
 *          this function. Therefore, snd_ctl_elem_list_free_space()
 *          must be called in advance.
 *
 * \param obj pointer to object to clear
 */
void snd_ctl_elem_list_clear(snd_ctl_elem_list_t *obj)
{
	memset(obj, 0, sizeof(snd_ctl_elem_list_t));
}

/**
 * \brief copy one #snd_ctl_elem_list_t to another.
 *
 * This performs a shallow copy. That means the both lists will share
 * the same space for the elements.  The elements will not be copied.
 *
 * \param dst pointer to destination
 * \param src pointer to source
 */
void snd_ctl_elem_list_copy(snd_ctl_elem_list_t *dst, const snd_ctl_elem_list_t *src)
{
	assert(dst && src);
	*dst = *src;
}

/**
 * \brief Set index of first wanted CTL element identifier in a CTL element identifiers list
 * \param obj CTL element identifiers list
 * \param val index of CTL element to put at position 0 of list
 */
void snd_ctl_elem_list_set_offset(snd_ctl_elem_list_t *obj, unsigned int val)
{
	assert(obj);
	obj->offset = val;
}

/**
 * \brief Get number of used entries in CTL element identifiers list
 *
 * This function returns how many entries are actually filled with
 * useful information.
 *
 * See also snd_ctl_elem_list_get_count().
 *
 * \param obj CTL element identifier list
 * \return number of used entries
 */
unsigned int snd_ctl_elem_list_get_used(const snd_ctl_elem_list_t *obj)
{
	assert(obj);
	return obj->used;
}

/**
 * \brief Get total count of elements present in CTL device
 *
 * This function returns how many entries were allocated using
 * snd_ctl_elem_list_alloc_space(). This information is present after
 * snd_ctl_elem_list() was called.
 *
 * See also snd_ctl_elem_list_get_used().
 *
 * \param obj CTL element identifier list
 * \return total number of elements
 */
unsigned int snd_ctl_elem_list_get_count(const snd_ctl_elem_list_t *obj)
{
	assert(obj);
	return obj->count;
}

/**
 * \brief Get CTL element identifier for an entry of a CTL element identifiers list
 * \param obj CTL element identifier list
 * \param idx Index of entry
 * \param ptr Pointer to returned CTL element identifier
 */
void snd_ctl_elem_list_get_id(const snd_ctl_elem_list_t *obj, unsigned int idx, snd_ctl_elem_id_t *ptr)
{
	assert(obj && ptr);
	assert(idx < obj->used);
	*ptr = obj->pids[idx];
}

/**
 * \brief Get CTL element numeric identifier for an entry of a CTL element identifiers list
 * \param obj CTL element identifier list
 * \param idx Index of entry
 * \return CTL element numeric identifier
 */
unsigned int snd_ctl_elem_list_get_numid(const snd_ctl_elem_list_t *obj, unsigned int idx)
{
	assert(obj);
	assert(idx < obj->used);
	return obj->pids[idx].numid;
}

/**
 * \brief Get interface part of CTL element identifier for an entry of a CTL element identifiers list
 * \param obj CTL element identifier list
 * \param idx Index of entry
 * \return CTL element related interface
 */
snd_ctl_elem_iface_t snd_ctl_elem_list_get_interface(const snd_ctl_elem_list_t *obj, unsigned int idx)
{
	assert(obj);
	assert(idx < obj->used);
	return obj->pids[idx].iface;
}

/**
 * \brief Get the device part of CTL element identifier for an entry of a CTL element identifiers list
 * \param obj CTL element identifier list
 * \param idx Index of entry
 * \return CTL element related device
 */
unsigned int snd_ctl_elem_list_get_device(const snd_ctl_elem_list_t *obj, unsigned int idx)
{
	assert(obj);
	assert(idx < obj->used);
	return obj->pids[idx].device;
}

/**
 * \brief Get subdevice part of CTL element identifier for an entry of a CTL element identifiers list
 * \param obj CTL element identifier list
 * \param idx Index of entry
 * \return CTL element related subdevice
 */
unsigned int snd_ctl_elem_list_get_subdevice(const snd_ctl_elem_list_t *obj, unsigned int idx)
{
	assert(obj);
	assert(idx < obj->used);
	return obj->pids[idx].subdevice;
}

/**
 * \brief Get name part of CTL element identifier for an entry of a CTL element identifiers list
 * \param obj CTL element identifier list
 * \param idx Index of entry
 * \return CTL element name
 */
const char *snd_ctl_elem_list_get_name(const snd_ctl_elem_list_t *obj, unsigned int idx)
{
	assert(obj);
	assert(idx < obj->used);
	return (const char *)obj->pids[idx].name;
}

/**
 * \brief Get index part of CTL element identifier for an entry of a CTL element identifiers list
 * \param obj CTL element identifier list
 * \param idx Index of entry
 * \return CTL element index
 */
unsigned int snd_ctl_elem_list_get_index(const snd_ctl_elem_list_t *obj, unsigned int idx)
{
	assert(obj);
	assert(idx < obj->used);
	return obj->pids[idx].index;
}

/**
 * \brief get size of #snd_ctl_elem_info_t
 * \return size in bytes
 */
size_t snd_ctl_elem_info_sizeof()
{
	return sizeof(snd_ctl_elem_info_t);
}

/**
 * \brief allocate an invalid #snd_ctl_elem_info_t using standard malloc
 * \param ptr returned pointer
 * \return 0 on success otherwise negative error code
 */
int snd_ctl_elem_info_malloc(snd_ctl_elem_info_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_ctl_elem_info_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

/**
 * \brief frees a previously allocated #snd_ctl_elem_info_t
 * \param obj pointer to object to free
 */
void snd_ctl_elem_info_free(snd_ctl_elem_info_t *obj)
{
	free(obj);
}

/**
 * \brief clear given #snd_ctl_elem_info_t object
 * \param obj pointer to object to clear
 */
void snd_ctl_elem_info_clear(snd_ctl_elem_info_t *obj)
{
	memset(obj, 0, sizeof(snd_ctl_elem_info_t));
}

/**
 * \brief copy one #snd_ctl_elem_info_t to another
 * \param dst pointer to destination
 * \param src pointer to source
 */
void snd_ctl_elem_info_copy(snd_ctl_elem_info_t *dst, const snd_ctl_elem_info_t *src)
{
	assert(dst && src);
	*dst = *src;
}

/**
 * \brief Get type from a CTL element id/info
 * \param obj CTL element id/info
 * \return CTL element content type
 */
snd_ctl_elem_type_t snd_ctl_elem_info_get_type(const snd_ctl_elem_info_t *obj)
{
	assert(obj);
	return obj->type;
}

/**
 * \brief Get info about readability from a CTL element id/info
 * \param obj CTL element id/info
 * \return 0 if element is not readable, 1 if element is readable
 */
int snd_ctl_elem_info_is_readable(const snd_ctl_elem_info_t *obj)
{
	assert(obj);
	return !!(obj->access & SNDRV_CTL_ELEM_ACCESS_READ);
}

/**
 * \brief Get info about writability from a CTL element id/info
 * \param obj CTL element id/info
 * \return 0 if element is not writable, 1 if element is not writable
 */
int snd_ctl_elem_info_is_writable(const snd_ctl_elem_info_t *obj)
{
	assert(obj);
	return !!(obj->access & SNDRV_CTL_ELEM_ACCESS_WRITE);
}

/**
 * \brief Get info about notification feasibility from a CTL element id/info
 * \param obj CTL element id/info
 * \return 0 if all element value changes are notified to subscribed applications, 1 otherwise
 */
int snd_ctl_elem_info_is_volatile(const snd_ctl_elem_info_t *obj)
{
	assert(obj);
	return !!(obj->access & SNDRV_CTL_ELEM_ACCESS_VOLATILE);
}

/**
 * \brief Get info about status from a CTL element id/info
 * \param obj CTL element id/info
 * \return 0 if element value is not active, 1 if is active
 */
int snd_ctl_elem_info_is_inactive(const snd_ctl_elem_info_t *obj)
{
	assert(obj);
	return !!(obj->access & SNDRV_CTL_ELEM_ACCESS_INACTIVE);
}

/**
 * \brief Get info whether an element is locked
 * \param obj CTL element id/info
 * \return 0 if element value is currently changeable, 1 if it's locked by another application
 */
int snd_ctl_elem_info_is_locked(const snd_ctl_elem_info_t *obj)
{
	assert(obj);
	return !!(obj->access & SNDRV_CTL_ELEM_ACCESS_LOCK);
}

/**
 * \brief Get info if I own an element
 * \param obj CTL element id/info
 * \return 0 if element value is currently changeable, 1 if it's locked by another application
 */
int snd_ctl_elem_info_is_owner(const snd_ctl_elem_info_t *obj)
{
	assert(obj);
	return !!(obj->access & SNDRV_CTL_ELEM_ACCESS_OWNER);
}

/**
 * \brief Get info if it's a user element
 * \param obj CTL element id/info
 * \return 0 if element value is a system element, 1 if it's a user-created element
 */
int snd_ctl_elem_info_is_user(const snd_ctl_elem_info_t *obj)
{
	assert(obj);
	return !!(obj->access & SNDRV_CTL_ELEM_ACCESS_USER);
}

/**
 * \brief Get info about TLV readability from a CTL element id/info
 * \param obj CTL element id/info
 * \return 0 if element's TLV is not readable, 1 if element's TLV is readable
 */
int snd_ctl_elem_info_is_tlv_readable(const snd_ctl_elem_info_t *obj)
{
	assert(obj);
	return !!(obj->access & SNDRV_CTL_ELEM_ACCESS_TLV_READ);
}

/**
 * \brief Get info about TLV writeability from a CTL element id/info
 * \param obj CTL element id/info
 * \return 0 if element's TLV is not writable, 1 if element's TLV is writable
 */
int snd_ctl_elem_info_is_tlv_writable(const snd_ctl_elem_info_t *obj)
{
	assert(obj);
	return !!(obj->access & SNDRV_CTL_ELEM_ACCESS_TLV_WRITE);
}

/**
 * \brief Get info about TLV command possibility from a CTL element id/info
 * \param obj CTL element id/info
 * \return 0 if element's TLV command is not possible, 1 if element's TLV command is supported
 */
int snd_ctl_elem_info_is_tlv_commandable(const snd_ctl_elem_info_t *obj)
{
	assert(obj);
	return !!(obj->access & SNDRV_CTL_ELEM_ACCESS_TLV_COMMAND);
}

/**
 * \brief (DEPRECATED) Get info about values passing policy from a CTL element value
 * \param obj CTL element id/info
 * \return 0 if element value need to be passed by contents, 1 if need to be passed with a pointer
 */
int snd_ctl_elem_info_is_indirect(const snd_ctl_elem_info_t *obj)
{
	assert(obj);
	return 0;
}
link_warning(snd_ctl_elem_info_is_indirect, "Warning: snd_ctl_elem_info_is_indirect is deprecated, do not use it");

/**
 * \brief Get owner of a locked element
 * \param obj CTL element id/info
 * \return value entries count
 */
pid_t snd_ctl_elem_info_get_owner(const snd_ctl_elem_info_t *obj)
{
	assert(obj);
	return obj->owner;
}

/**
 * \brief Get number of value entries from a CTL element id/info
 * \param obj CTL element id/info
 * \return value entries count
 */
unsigned int snd_ctl_elem_info_get_count(const snd_ctl_elem_info_t *obj)
{
	assert(obj);
	return obj->count;
}

/**
 * \brief Get minimum value from a #SND_CTL_ELEM_TYPE_INTEGER CTL element id/info
 * \param obj CTL element id/info
 * \return Minimum value
 */
long snd_ctl_elem_info_get_min(const snd_ctl_elem_info_t *obj)
{
	assert(obj);
	assert(obj->type == SND_CTL_ELEM_TYPE_INTEGER);
	return obj->value.integer.min;
}

/**
 * \brief Get maximum value from a #SND_CTL_ELEM_TYPE_INTEGER CTL element id/info
 * \param obj CTL element id/info
 * \return Maximum value
 */
long snd_ctl_elem_info_get_max(const snd_ctl_elem_info_t *obj)
{
	assert(obj);
	assert(obj->type == SND_CTL_ELEM_TYPE_INTEGER);
	return obj->value.integer.max;
}

/**
 * \brief Get value step from a #SND_CTL_ELEM_TYPE_INTEGER CTL element id/info
 * \param obj CTL element id/info
 * \return Step
 */
long snd_ctl_elem_info_get_step(const snd_ctl_elem_info_t *obj)
{
	assert(obj);
	assert(obj->type == SND_CTL_ELEM_TYPE_INTEGER);
	return obj->value.integer.step;
}

/**
 * \brief Get minimum value from a #SND_CTL_ELEM_TYPE_INTEGER64 CTL element id/info
 * \param obj CTL element id/info
 * \return Minimum value
 */
long long snd_ctl_elem_info_get_min64(const snd_ctl_elem_info_t *obj)
{
	assert(obj);
	assert(obj->type == SND_CTL_ELEM_TYPE_INTEGER64);
	return obj->value.integer64.min;
}

/**
 * \brief Get maximum value from a #SND_CTL_ELEM_TYPE_INTEGER64 CTL element id/info
 * \param obj CTL element id/info
 * \return Maximum value
 */
long long snd_ctl_elem_info_get_max64(const snd_ctl_elem_info_t *obj)
{
	assert(obj);
	assert(obj->type == SND_CTL_ELEM_TYPE_INTEGER64);
	return obj->value.integer64.max;
}

/**
 * \brief Get value step from a #SND_CTL_ELEM_TYPE_INTEGER64 CTL element id/info
 * \param obj CTL element id/info
 * \return Step
 */
long long snd_ctl_elem_info_get_step64(const snd_ctl_elem_info_t *obj)
{
	assert(obj);
	assert(obj->type == SND_CTL_ELEM_TYPE_INTEGER64);
	return obj->value.integer64.step;
}

/**
 * \brief Get number of items available from a #SND_CTL_ELEM_TYPE_ENUMERATED CTL element id/info
 * \param obj CTL element id/info
 * \return items count
 */
unsigned int snd_ctl_elem_info_get_items(const snd_ctl_elem_info_t *obj)
{
	assert(obj);
	assert(obj->type == SND_CTL_ELEM_TYPE_ENUMERATED);
	return obj->value.enumerated.items;
}

/**
 * \brief Select item in a #SND_CTL_ELEM_TYPE_ENUMERATED CTL element id/info
 * \param obj CTL element id/info
 * \param val item number
 */
void snd_ctl_elem_info_set_item(snd_ctl_elem_info_t *obj, unsigned int val)
{
	assert(obj);
	obj->value.enumerated.item = val;
}

/**
 * \brief Get name for selected item in a #SND_CTL_ELEM_TYPE_ENUMERATED CTL element id/info
 * \param obj CTL element id/info
 * \return name of chosen item
 */
const char *snd_ctl_elem_info_get_item_name(const snd_ctl_elem_info_t *obj)
{
	assert(obj);
	assert(obj->type == SND_CTL_ELEM_TYPE_ENUMERATED);
	return obj->value.enumerated.name;
}

/**
 * \brief Get count of dimensions for given element
 * \param obj CTL element id/info
 * \return zero value if no dimensions are defined, otherwise positive value with count of dimensions
 *
 * \deprecated	Since 1.1.5
 * #snd_ctl_elem_info_get_dimensions is deprecated without any replacement.
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_ctl_elem_info_get_dimensions)(const snd_ctl_elem_info_t *obj ATTRIBUTE_UNUSED)
#else
int snd_ctl_elem_info_get_dimensions(const snd_ctl_elem_info_t *obj)
#endif
{
#if 0 /* deprecated */
	int i;

	assert(obj);
	for (i = 3; i >= 0; i--)
		if (obj->dimen.d[i])
			break;
	return i + 1;
#else
	return -EINVAL;
#endif
}
use_default_symbol_version(__snd_ctl_elem_info_get_dimensions, snd_ctl_elem_info_get_dimensions, ALSA_0.9.3);

/**
 * \brief Get specified of dimension width for given element
 * \param obj CTL element id/info
 * \param idx The dimension index
 * \return zero value if no dimension width is defined, otherwise positive value with with of specified dimension
 *
 * \deprecated	Since 1.1.5
 * #snd_ctl_elem_info_get_dimension is deprecated without any replacement.
 */
#ifndef DOXYGEN
EXPORT_SYMBOL int INTERNAL(snd_ctl_elem_info_get_dimension)(const snd_ctl_elem_info_t *obj ATTRIBUTE_UNUSED, unsigned int idx ATTRIBUTE_UNUSED)
#else
int snd_ctl_elem_info_get_dimension(const snd_ctl_elem_info_t *obj, unsigned int idx)
#endif
{
#if 0 /* deprecated */
	assert(obj);
	if (idx > 3)
		return 0;
	return obj->dimen.d[idx];
#else /* deprecated */
	return -EINVAL;
#endif /* deprecated */
}
use_default_symbol_version(__snd_ctl_elem_info_get_dimension, snd_ctl_elem_info_get_dimension, ALSA_0.9.3);

/**
 * \brief Set width to a specified dimension level of given element information.
 * \param info Information of an element.
 * \param dimension Dimension width for each level by member unit.
 * \return Zero on success, otherwise a negative error code.
 *
 * \par Errors:
 * <dl>
 * <dt>-EINVAL
 * <dd>Invalid arguments are given as parameters.
 * </dl>
 *
 * \par Compatibility:
 * This function is added in version 1.1.2.
 *
 * \deprecated Since 1.1.5
 * #snd_ctl_elem_info_set_dimension is deprecated without any replacement.
 */
int snd_ctl_elem_info_set_dimension(snd_ctl_elem_info_t *info ATTRIBUTE_UNUSED,
				    const int dimension[4] ATTRIBUTE_UNUSED)
{
#if 0 /* deprecated */
	unsigned int i;

	if (info == NULL)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(info->dimen.d); i++) {
		if (dimension[i] < 0)
			return -EINVAL;

		info->dimen.d[i] = dimension[i];
	}

	return 0;
#else /* deprecated */
	return -EINVAL;
#endif /* deprecated */
}

/**
 * \brief Get CTL element identifier of a CTL element id/info
 * \param obj CTL element id/info
 * \param ptr Pointer to returned CTL element identifier
 */
void snd_ctl_elem_info_get_id(const snd_ctl_elem_info_t *obj, snd_ctl_elem_id_t *ptr)
{
	assert(obj && ptr);
	*ptr = obj->id;
}

/**
 * \brief Get element numeric identifier of a CTL element id/info
 * \param obj CTL element id/info
 * \return element numeric identifier
 */
unsigned int snd_ctl_elem_info_get_numid(const snd_ctl_elem_info_t *obj)
{
	assert(obj);
	return obj->id.numid;
}

/**
 * \brief Get interface part of CTL element identifier of a CTL element id/info
 * \param obj CTL element id/info
 * \return interface part of element identifier
 */
snd_ctl_elem_iface_t snd_ctl_elem_info_get_interface(const snd_ctl_elem_info_t *obj)
{
	assert(obj);
	return obj->id.iface;
}

/**
 * \brief Get device part of CTL element identifier of a CTL element id/info
 * \param obj CTL element id/info
 * \return device part of element identifier
 */
unsigned int snd_ctl_elem_info_get_device(const snd_ctl_elem_info_t *obj)
{
	assert(obj);
	return obj->id.device;
}

/**
 * \brief Get subdevice part of CTL element identifier of a CTL element id/info
 * \param obj CTL element id/info
 * \return subdevice part of element identifier
 */
unsigned int snd_ctl_elem_info_get_subdevice(const snd_ctl_elem_info_t *obj)
{
	assert(obj);
	return obj->id.subdevice;
}

/**
 * \brief Get name part of CTL element identifier of a CTL element id/info
 * \param obj CTL element id/info
 * \return name part of element identifier
 */
const char *snd_ctl_elem_info_get_name(const snd_ctl_elem_info_t *obj)
{
	assert(obj);
	return (const char *)obj->id.name;
}

/**
 * \brief Get index part of CTL element identifier of a CTL element id/info
 * \param obj CTL element id/info
 * \return index part of element identifier
 */
unsigned int snd_ctl_elem_info_get_index(const snd_ctl_elem_info_t *obj)
{
	assert(obj);
	return obj->id.index;
}

/**
 * \brief Set CTL element identifier of a CTL element id/info
 * \param obj CTL element id/info
 * \param ptr CTL element identifier
 */
void snd_ctl_elem_info_set_id(snd_ctl_elem_info_t *obj, const snd_ctl_elem_id_t *ptr)
{
	assert(obj && ptr);
	obj->id = *ptr;
}

/**
 * \brief Set element numeric identifier of a CTL element id/info
 * \param obj CTL element id/info
 * \param val element numeric identifier
 */
void snd_ctl_elem_info_set_numid(snd_ctl_elem_info_t *obj, unsigned int val)
{
	assert(obj);
	obj->id.numid = val;
}

/**
 * \brief Set interface part of CTL element identifier of a CTL element id/info
 * \param obj CTL element id/info
 * \param val interface part of element identifier
 */
void snd_ctl_elem_info_set_interface(snd_ctl_elem_info_t *obj, snd_ctl_elem_iface_t val)
{
	assert(obj);
	obj->id.iface = val;
}

/**
 * \brief Set device part of CTL element identifier of a CTL element id/info
 * \param obj CTL element id/info
 * \param val device part of element identifier
 */
void snd_ctl_elem_info_set_device(snd_ctl_elem_info_t *obj, unsigned int val)
{
	assert(obj);
	obj->id.device = val;
}

/**
 * \brief Set subdevice part of CTL element identifier of a CTL element id/info
 * \param obj CTL element id/info
 * \param val subdevice part of element identifier
 */
void snd_ctl_elem_info_set_subdevice(snd_ctl_elem_info_t *obj, unsigned int val)
{
	assert(obj);
	obj->id.subdevice = val;
}

/**
 * \brief Set name part of CTL element identifier of a CTL element id/info
 * \param obj CTL element id/info
 * \param val name part of element identifier
 */
void snd_ctl_elem_info_set_name(snd_ctl_elem_info_t *obj, const char *val)
{
	assert(obj);
	snd_strlcpy((char *)obj->id.name, val, sizeof(obj->id.name));
}

/**
 * \brief Set index part of CTL element identifier of a CTL element id/info
 * \param obj CTL element id/info
 * \param val index part of element identifier
 */
void snd_ctl_elem_info_set_index(snd_ctl_elem_info_t *obj, unsigned int val)
{
	assert(obj);
	obj->id.index = val;
}

/**
 * \brief Set readability/writeability parameter of a CTL element id/info
 * \param obj CTL element id/info
 * \param rval readability part of element identifier
 * \param wval writeability part of element identifier
 */
void snd_ctl_elem_info_set_read_write(snd_ctl_elem_info_t *obj, int rval, int wval)
{
	assert(obj);
	obj->access = (obj->access & ~SNDRV_CTL_ELEM_ACCESS_READWRITE) |
				(rval ? SNDRV_CTL_ELEM_ACCESS_READ : 0) |
				(wval ? SNDRV_CTL_ELEM_ACCESS_WRITE : 0);
}

/**
 * \brief Set TLV readability/writeability parameter of a CTL element id/info
 * \param obj CTL element id/info
 * \param rval TLV readability part of element identifier
 * \param wval TLV writeability part of element identifier
 */
void snd_ctl_elem_info_set_tlv_read_write(snd_ctl_elem_info_t *obj, int rval, int wval)
{
	assert(obj);
	obj->access = (obj->access & ~SNDRV_CTL_ELEM_ACCESS_TLV_READWRITE) |
				(rval ? SNDRV_CTL_ELEM_ACCESS_TLV_READ : 0) |
				(wval ? SNDRV_CTL_ELEM_ACCESS_TLV_WRITE : 0);
}

/**
 * \brief Set inactive parameter of a CTL element id/info
 * \param obj CTL element id/info
 * \param val inactive part of element identifier
 */
void snd_ctl_elem_info_set_inactive(snd_ctl_elem_info_t *obj, int val)
{
	assert(obj);
	obj->access = (obj->access & ~SNDRV_CTL_ELEM_ACCESS_INACTIVE) |
				(val ? SNDRV_CTL_ELEM_ACCESS_INACTIVE : 0);
}

/**
 * \brief Get size of data structure for an element.
 * \return Size in bytes.
 */
size_t snd_ctl_elem_value_sizeof()
{
	return sizeof(snd_ctl_elem_value_t);
}

/**
 * \brief Allocate an invalid #snd_ctl_elem_value_t on the heap.
 *
 * Allocate space for a value object on the heap. The allocated memory
 * must be freed using snd_ctl_elem_value_free().
 *
 * See snd_ctl_elem_value_t for details.
 *
 * \param ptr Pointer to a snd_ctl_elem_value_t pointer. The address
 *            of the allocated space will be returned here.
 * \return 0 on success, otherwise a negative error code.
 */
int snd_ctl_elem_value_malloc(snd_ctl_elem_value_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_ctl_elem_value_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

/**
 * \brief Free an #snd_ctl_elem_value_t previously allocated using
 *        snd_ctl_elem_value_malloc().
 *
 * \param obj Pointer to the snd_ctl_elem_value_t.
 */
void snd_ctl_elem_value_free(snd_ctl_elem_value_t *obj)
{
	free(obj);
}

/**
 * \brief Clear given data of an element.
 *
 * See snd_ctl_elem_value_t for details.
 *
 * \param obj Data of an element.
 */
void snd_ctl_elem_value_clear(snd_ctl_elem_value_t *obj)
{
	memset(obj, 0, sizeof(snd_ctl_elem_value_t));
}

/**
 * \brief Bitwise copy of a snd_ctl_elem_value_t value.
 * \param dst Pointer to destination.
 * \param src Pointer to source.
 */
void snd_ctl_elem_value_copy(snd_ctl_elem_value_t *dst,
			     const snd_ctl_elem_value_t *src)
{
	assert(dst && src);
	*dst = *src;
}

/**
 * \brief Compare two snd_ctl_elem_value_t values, bytewise.
 *
 * \param left First value.
 * \param right Second value.
 * \return 0 on match, less than or greater than otherwise, see memcmp(3).
 */
int snd_ctl_elem_value_compare(snd_ctl_elem_value_t *left,
			       const snd_ctl_elem_value_t *right)
{
	assert(left && right);
	return memcmp(left, right, sizeof(*left));
}

/**
 * \brief Get the element identifier from the given element value.
 *
 * See snd_ctl_elem_value_t for more details.
 *
 * \param obj The element value.
 * \param ptr Pointer to an identifier object. The identifier is
 *            stored there.
 */
void snd_ctl_elem_value_get_id(const snd_ctl_elem_value_t *obj, snd_ctl_elem_id_t *ptr)
{
	assert(obj && ptr);
	*ptr = obj->id;
}

/**
 * \brief Get the identifiers 'numid' part from the given element value.
 *
 * See snd_ctl_elem_value_t for more details.
 *
 * \param obj The element value.
 * \return The numid.
 */
unsigned int snd_ctl_elem_value_get_numid(const snd_ctl_elem_value_t *obj)
{
	assert(obj);
	return obj->id.numid;
}

/**
 * \brief Get the identifiers 'interface' part from the given element value.
 *
 * See snd_ctl_elem_value_t for more details.
 *
 * \param obj The element value.
 * \return The interface part of element identifier.
 */
snd_ctl_elem_iface_t snd_ctl_elem_value_get_interface(const snd_ctl_elem_value_t *obj)
{
	assert(obj);
	return obj->id.iface;
}

/**
 * \brief Get the identifiers 'device' part from the given element value.
 *
 * See snd_ctl_elem_value_t for more details.
 *
 * \param obj The element value.
 * \return The device part of element identifier.
 */
unsigned int snd_ctl_elem_value_get_device(const snd_ctl_elem_value_t *obj)
{
	assert(obj);
	return obj->id.device;
}

/**
 * \brief Get the identifiers 'subdevice' part from the given element value.
 *
 * See snd_ctl_elem_value_t for more details.
 *
 * \param obj The element value.
 * \return The subdevice part of element identifier.
 */
unsigned int snd_ctl_elem_value_get_subdevice(const snd_ctl_elem_value_t *obj)
{
	assert(obj);
	return obj->id.subdevice;
}

/**
 * \brief Get the identifiers 'name' part from the given element value.
 *
 * See snd_ctl_elem_value_t for more details.
 *
 * \param obj The element value.
 * \return The "name" part of element identifier.
 */
const char *snd_ctl_elem_value_get_name(const snd_ctl_elem_value_t *obj)
{
	assert(obj);
	return (const char *)obj->id.name;
}

/**
 * \brief Get the identifiers 'index' part from the given element value.
 *
 * See snd_ctl_elem_value_t for more details.
 *
 * \param obj The element value.
 * \return The index part of element identifier.
 */
unsigned int snd_ctl_elem_value_get_index(const snd_ctl_elem_value_t *obj)
{
	assert(obj);
	return obj->id.index;
}


/**
 * \brief Set the element identifier within the given element value.
 *
 * See snd_ctl_elem_value_t for more details.
 *
 * \param obj The element value.
 * \param ptr The new identifier.
 */
void snd_ctl_elem_value_set_id(snd_ctl_elem_value_t *obj, const snd_ctl_elem_id_t *ptr)
{
	assert(obj && ptr);
	obj->id = *ptr;
}

/**
 * \brief Set the identifiers 'numid' part within the given element value.
 *
 * See snd_ctl_elem_value_t for more details.
 *
 * \param obj The element value.
 * \param val The new numid.
 */
void snd_ctl_elem_value_set_numid(snd_ctl_elem_value_t *obj, unsigned int val)
{
	assert(obj);
	obj->id.numid = val;
}

/**
 * \brief Set the identifiers 'interface' part within the given element value.
 *
 * See snd_ctl_elem_value_t for more details.
 *
 * \param obj The element value.
 * \param val The new interface.
 */
void snd_ctl_elem_value_set_interface(snd_ctl_elem_value_t *obj, snd_ctl_elem_iface_t val)
{
	assert(obj);
	obj->id.iface = val;
}

/**
 * \brief Set the identifiers 'device' part within the given element value.
 *
 * See snd_ctl_elem_value_t for more details.
 *
 * \param obj The element value.
 * \param val The new device.
 */
void snd_ctl_elem_value_set_device(snd_ctl_elem_value_t *obj, unsigned int val)
{
	assert(obj);
	obj->id.device = val;
}

/**
 * \brief Set the identifiers 'subdevice' part within the given element value.
 *
 * See snd_ctl_elem_value_t for more details.
 *
 * \param obj The element value.
 * \param val The new subdevice.
 */
void snd_ctl_elem_value_set_subdevice(snd_ctl_elem_value_t *obj, unsigned int val)
{
	assert(obj);
	obj->id.subdevice = val;
}

/**
 * \brief Set the identifiers 'name' part within the given element value.
 *
 * See snd_ctl_elem_value_t for more details.
 *
 * \param obj The element value.
 * \param val The new name.
 */
void snd_ctl_elem_value_set_name(snd_ctl_elem_value_t *obj, const char *val)
{
	assert(obj);
	snd_strlcpy((char *)obj->id.name, val, sizeof(obj->id.name));
}

/**
 * \brief Set the identifiers 'index' part within the given element value.
 *
 * See snd_ctl_elem_value_t for more details.
 *
 * \param obj The element value.
 * \param val The new index.
 */
void snd_ctl_elem_value_set_index(snd_ctl_elem_value_t *obj, unsigned int val)
{
	assert(obj);
	obj->id.index = val;
}

/**
 * \brief Get an element members value.
 *
 * Use this function if the element is of type SNDRV_CTL_ELEM_TYPE_BOOLEAN. It
 * returns the value of one member. See \ref snd_ctl_elem_value_t and \ref
 * control for more details.
 *
 * \param obj The element value object
 * \param idx The index of the member.
 * \return The members value.
 */
int snd_ctl_elem_value_get_boolean(const snd_ctl_elem_value_t *obj, unsigned int idx)
{
	assert(obj);
	assert(idx < ARRAY_SIZE(obj->value.integer.value));
	return obj->value.integer.value[idx];
}

/**
 * \brief Get an element members value.
 *
 * Use this function if the element is of type SNDRV_CTL_ELEM_TYPE_INTEGER. It
 * returns the value of one member. See \ref snd_ctl_elem_value_t and \ref
 * control for more details.
 *
 * \param obj The element value object.
 * \param idx The index of the member.
 * \return The members value.
 */
long snd_ctl_elem_value_get_integer(const snd_ctl_elem_value_t *obj, unsigned int idx)
{
	assert(obj);
	assert(idx < ARRAY_SIZE(obj->value.integer.value));
	return obj->value.integer.value[idx];
}

/**
 * \brief Get an element members value.
 *
 * Use this function if the element is of type SNDRV_CTL_ELEM_TYPE_INTEGER64. It
 * returns the value of one member. See \ref snd_ctl_elem_value_t and \ref
 * control for more details.
 *
 * \param obj The element value object.
 * \param idx The index of the member.
 * \return The members value.
 */
long long snd_ctl_elem_value_get_integer64(const snd_ctl_elem_value_t *obj, unsigned int idx)
{
	assert(obj);
	assert(idx < ARRAY_SIZE(obj->value.integer64.value));
	return obj->value.integer64.value[idx];
}

/**
 * \brief Get an element members value.
 *
 * Use this function if the element is of type
 * SNDRV_CTL_ELEM_TYPE_ENUMERATED. It returns the index of the active item. See
 * \ref snd_ctl_elem_value_t and \ref control for more details.
 *
 * \param obj The element value object.
 * \param idx The index of the requested member.
 * \return The index of the active item.
 */
unsigned int snd_ctl_elem_value_get_enumerated(const snd_ctl_elem_value_t *obj, unsigned int idx)
{
	assert(obj);
	assert(idx < ARRAY_SIZE(obj->value.enumerated.item));
	return obj->value.enumerated.item[idx];
}

/**
 * \brief Get an element members value.
 *
 * Use this function if the element is of type SNDRV_CTL_ELEM_TYPE_BYTE. It
 * returns the value of one member. See \ref snd_ctl_elem_value_t and \ref
 * control for more details.
 *
 * \param obj The element value object.
 * \param idx The index of the member.
 * \return The members value.
 */
unsigned char snd_ctl_elem_value_get_byte(const snd_ctl_elem_value_t *obj, unsigned int idx)
{
	assert(obj);
	assert(idx < ARRAY_SIZE(obj->value.bytes.data));
	return obj->value.bytes.data[idx];
}

/**
 * \brief Set an element members value.
 *
 * Use this function if the element is of type SNDRV_CTL_ELEM_TYPE_BOOLEAN. It
 * sets the value of one member. See \ref snd_ctl_elem_value_t and \ref control
 * for more details.
 *
 * \param obj The element value object.
 * \param idx The index of the member.
 * \param val The new value.
 */
void snd_ctl_elem_value_set_boolean(snd_ctl_elem_value_t *obj, unsigned int idx, long val)
{
	assert(obj);
	assert(idx < ARRAY_SIZE(obj->value.integer.value));
	obj->value.integer.value[idx] = val;
}

/**
 * \brief Set an element members value.
 *
 * Use this function if the element is of type SNDRV_CTL_ELEM_TYPE_INTEGER. It
 * sets the value of one member. See \ref snd_ctl_elem_value_t and \ref control
 * for more details.
 *
 * \param obj The element value object.
 * \param idx The index of the member.
 * \param val The new value.
 */
void snd_ctl_elem_value_set_integer(snd_ctl_elem_value_t *obj, unsigned int idx, long val)
{
	assert(obj);
	assert(idx < ARRAY_SIZE(obj->value.integer.value));
	obj->value.integer.value[idx] = val;
}

/**
 * \brief Set an element members value.
 *
 * Use this function if the element is of type SNDRV_CTL_ELEM_TYPE_INTEGER64. It
 * sets the value of one member. See \ref snd_ctl_elem_value_t and \ref control
 * for more details.
 *
 * \param obj The element value object.
 * \param idx The index of the member.
 * \param val The new value.
 */
void snd_ctl_elem_value_set_integer64(snd_ctl_elem_value_t *obj, unsigned int idx, long long val)
{
	assert(obj);
	assert(idx < ARRAY_SIZE(obj->value.integer64.value));
	obj->value.integer64.value[idx] = val;
}

/**
 * \brief Set an element members value.
 *
 * Use this function if the element is of type
 * SNDRV_CTL_ELEM_TYPE_ENUMERATED. It activates the specified item. See \ref
 * snd_ctl_elem_value_t and \ref control for more details.
 *
 * \param obj The element value object.
 * \param idx The index of the requested member.
 * \param val The new index of the item to be activated.
 */
void snd_ctl_elem_value_set_enumerated(snd_ctl_elem_value_t *obj, unsigned int idx, unsigned int val)
{
	assert(obj);
	assert(idx < ARRAY_SIZE(obj->value.enumerated.item));
	obj->value.enumerated.item[idx] = val;
}

/**
 * \brief Set an element members value.
 *
 * Use this function if the element is of type SNDRV_CTL_ELEM_TYPE_BYTE. It
 * sets the value of one member. See \ref snd_ctl_elem_value_t and \ref control
 * for more details.
 *
 * \param obj The element value object.
 * \param idx The index of the member.
 * \param val The new value.
 */
void snd_ctl_elem_value_set_byte(snd_ctl_elem_value_t *obj, unsigned int idx, unsigned char val)
{
	assert(obj);
	assert(idx < ARRAY_SIZE(obj->value.bytes.data));
	obj->value.bytes.data[idx] = val;
}

/**
 * \brief Replace the data stored within the element.
 *
 * Use this function if the element is of type SNDRV_CTL_ELEM_TYPE_BYTES. It
 * replaces the data stored in the element. Note that "bytes" elements don't
 * have members. They have only one single block of data.
 *
 * See \ref snd_ctl_elem_value_t and \ref control for more details.
 *
 * \param obj The element value object.
 * \param data Pointer to the new data.
 * \param size The size of the new data, in bytes.
 */
void snd_ctl_elem_set_bytes(snd_ctl_elem_value_t *obj, void *data, size_t size)
{
	assert(obj);
	assert(size <= ARRAY_SIZE(obj->value.bytes.data));
	memcpy(obj->value.bytes.data, data, size);
}

/**
 * \brief Get the data stored within the element.
 *
 * Use this function if the element is of type SNDRV_CTL_ELEM_TYPE_BYTES. It
 * returns the data stored in the element. Note that "bytes" elements don't have
 * members. They have only one single block of data.
 *
 * See \ref snd_ctl_elem_value_t and \ref control for more details.
 *
 * \param obj The element value object.
 * \return Pointer to the elements data.
 */
const void * snd_ctl_elem_value_get_bytes(const snd_ctl_elem_value_t *obj)
{
	assert(obj);
	return obj->value.bytes.data;
}

/**
 * \brief Get an elements IEC958 data.
 *
 * Use this function if the element is of type SNDRV_CTL_ELEM_TYPE_IEC958.  Note that
 * "IEC958" elements don't have members. They have only one single
 * IEC958 information block.
 *
 * See \ref snd_ctl_elem_value_t and \ref control for more details.
 *
 * \param obj The element value object.
 * \param ptr Pointer to an IEC958 structure. The data is stored there.
 */
void snd_ctl_elem_value_get_iec958(const snd_ctl_elem_value_t *obj, snd_aes_iec958_t *ptr)
{
	assert(obj && ptr);
	memcpy(ptr, &obj->value.iec958, sizeof(*ptr));
}

/**
 * \brief Set an elements IEC958 data.
 *
 * Use this function if the element is of type SNDRV_CTL_ELEM_TYPE_IEC958.  Note
 * that "IEC958" elements don't have members. They have only one single IEC958
 * information block.
 *
 * See \ref snd_ctl_elem_value_t and \ref control for more details.
 *
 * \param obj The element value object.
 * \param ptr Pointer to the new IEC958 data.
 */
void snd_ctl_elem_value_set_iec958(snd_ctl_elem_value_t *obj, const snd_aes_iec958_t *ptr)
{
	assert(obj && ptr);
	memcpy(&obj->value.iec958, ptr, sizeof(obj->value.iec958));
}
