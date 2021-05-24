/*  -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*  Please make sure that the TAB width in your editor is set to 4 spaces  */

/*\
|*|
|*| nautilus-hide.c
|*|
|*| https://gitlab.gnome.org/madmurphy/nautilus-hide
|*|
|*| Copyright (C) 2021 <madmurphy333@gmail.com>
|*|
|*| **Nautilus Hide** is free software: you can redistribute it and/or modify
|*| it under the terms of the GNU General Public License as published by the
|*| Free Software Foundation, either version 3 of the License, or (at your
|*| option) any later version.
|*|
|*| **Nautilus Hide** is distributed in the hope that it will be useful, but
|*| WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
|*| or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
|*| more details.
|*|
|*| You should have received a copy of the GNU General Public License along
|*| with this program. If not, see <http://www.gnu.org/licenses/>.
|*|
\*/



#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <nautilus-extension.h>



/*\
|*|
|*| BUILD SETTINGS
|*|
\*/


#ifdef ENABLE_NLS
#include <libintl.h>
#include <glib/gi18n-lib.h>
#define I18N_INIT() \
	bindtextdomain(GETTEXT_PACKAGE, NAUTILUS_HIDE_LOCALEDIR)
#else
#define _(STRING) ((char *) (STRING))
#define g_dngettext(DOMAIN, STRING1, STRING2, NUM) \
	((NUM) > 1 ? (char *) (STRING2) : (char *) (STRING1))
#define I18N_INIT()
#endif



/*\
|*|
|*| GLOBAL TYPES AND VARIABLES
|*|
\*/


#ifdef G_LOG_DOMAIN
#undef G_LOG_DOMAIN
#endif
#define G_LOG_DOMAIN "Nautilus-Hide"

#define NH_R_OK 1
#define NH_W_OK 2

typedef struct {
	GObject parent_slot;
} NautilusHide;

typedef struct {
	GObjectClass parent_slot;
} NautilusHideClass;

typedef struct ssofwcp_T {
	char * directory;
	char * hdb_path;
	char ** hdb_entries;
	GList * selection;
	guint8 hdb_access;
} ssofwcp;

static GType provider_types[1];
static GType nautilus_hide_type;
static GObjectClass * parent_class;
static const char hidden_db_fname[] = ".hidden";



/*\
|*|
|*| FUNCTIONS
|*|
\*/


static void close_hdb_file (
	FILE * hdb_file,
	const char * const hdb_path
) {

	if (fclose(hdb_file) == EOF) {

		g_message(
			"%s (database: %s)",
			_("Could not close the database of hidden files"),
			hdb_path
		);

	}

}


static ssofwcp * ssofwcp_new_with_dir (
	const char * const dpath
) {

	ssofwcp * new_ssofwcp = malloc(sizeof(ssofwcp));

	if (!new_ssofwcp) {

		g_message("%s (ENOMEM ID: k75zc4j)", _("Error allocating memory"));
		return NULL;

	}

	const gsize dp_len = strlen(dpath);

	new_ssofwcp->hdb_entries = NULL;
	new_ssofwcp->selection = NULL;
	new_ssofwcp->directory = malloc(dp_len + 1);

	if (!new_ssofwcp->directory) {

		g_message("%s (ENOMEM ID: nkyd8w8)", _("Error allocating memory"));
		free(new_ssofwcp);
		return NULL;

	}

	new_ssofwcp->hdb_path = malloc(dp_len + sizeof(hidden_db_fname) + 1);

	if (!new_ssofwcp->hdb_path) {

		g_message("%s (ENOMEM ID: bo9axpr)", _("Error allocating memory"));
		free(new_ssofwcp->directory);
		free(new_ssofwcp);
		return NULL;

	}

	memcpy(new_ssofwcp->directory, dpath, dp_len + 1);
	memcpy(new_ssofwcp->hdb_path, dpath, dp_len);

	memcpy(
		new_ssofwcp->hdb_path + dp_len + 1,
		hidden_db_fname,
		sizeof(hidden_db_fname)
	);

	new_ssofwcp->hdb_path[dp_len] = '/';

	new_ssofwcp->hdb_access = (
		!g_access(new_ssofwcp->hdb_path, R_OK) || (
			g_access(new_ssofwcp->hdb_path, F_OK) &&
			!g_access(new_ssofwcp->directory, X_OK)
		) ?
			NH_R_OK
		:
			0
	) | (
		(
			g_access(new_ssofwcp->hdb_path, F_OK) ?
				!g_access(new_ssofwcp->directory, W_OK)
			:
				!g_access(new_ssofwcp->hdb_path, W_OK)
		) ?
			NH_W_OK
		:
			0
	);

	if (g_access(new_ssofwcp->hdb_path, R_OK)) {

		return new_ssofwcp;

	}

	FILE * const hdb_file_r = fopen(new_ssofwcp->hdb_path, "rb");

	if (!hdb_file_r) {

		g_message(
			"%s (database: %s)",
			_("Error attempting to access the database of hidden files"),
			new_ssofwcp->hdb_path
		);

		return new_ssofwcp;

	}

	unsigned long int file_size;

	if (
		fseek(hdb_file_r, 0, SEEK_END) ||
		(file_size = ftell(hdb_file_r)) < 0
	) {

		g_message(
			"%s (database: %s)",
			_("Could not calculate the size of the database of hidden files"),
			new_ssofwcp->hdb_path
		);

		close_hdb_file(hdb_file_r, new_ssofwcp->hdb_path);
		return new_ssofwcp;

	}

	if ((uintmax_t) file_size > (uintmax_t) SIZE_MAX) {

		g_message(
			"%s (database: %s)",
			_("Database of hidden files is too big"),
			new_ssofwcp->hdb_path
		);

		close_hdb_file(hdb_file_r, new_ssofwcp->hdb_path);
		return new_ssofwcp;

	}

	char * const cache = (char *) malloc((size_t) file_size + 1);

	if (!cache) {

		g_message("%s (ENOMEM ID: kmh80ll)", _("Error allocating memory"));
		close_hdb_file(hdb_file_r, new_ssofwcp->hdb_path);
		return new_ssofwcp;

	}

	rewind(hdb_file_r);

	if (fread(cache, 1, (size_t) file_size, hdb_file_r) < file_size) {

		g_message(
			"%s (database: %s)",
			_("I/O error while accessing database of hidden files"),
			new_ssofwcp->hdb_path
		);

		free(cache);
		close_hdb_file(hdb_file_r, new_ssofwcp->hdb_path);
		return new_ssofwcp;

	}

	close_hdb_file(hdb_file_r, new_ssofwcp->hdb_path);
	cache[file_size] = '\0';

	gsize idx = 0, line_start = 0, fileno = 1;

	for (; idx < file_size; idx++) {

		if (cache[idx] == '\n') {

			if (line_start != idx) {

				fileno++;

			}

			line_start = idx + 1;
			cache[idx] = '\0';

		}

	}

	new_ssofwcp->hdb_entries = malloc(sizeof(char *) * fileno + file_size + 1);

	if (!new_ssofwcp->hdb_entries) {

		g_message("%s (ENOMEM ID: fk9c9sj)", _("Error allocating memory"));
		free(cache);
		return new_ssofwcp;

	}

	char * const list_buffer =
		((char *) new_ssofwcp->hdb_entries) + sizeof(char *) * fileno;

	memcpy(list_buffer, cache, file_size + 1);
	free(cache);
	new_ssofwcp->hdb_entries[fileno - 1] = NULL;

	for (fileno = 0, line_start = 0, idx = 0; idx < file_size; idx++) {

		if (!list_buffer[idx]) {

			if (line_start != idx) {

				new_ssofwcp->hdb_entries[fileno++] = list_buffer + line_start;

			}

			line_start = idx + 1;

		}

	}

	return new_ssofwcp;

}


static GList * ordered_file_selection_new (
	GList * const file_selection
) {

	char * dpath;
	GFile * location;
	GList * ordered_selection = NULL;
	ssofwcp * new_ssofwcp;

	for (
		GList * d_iter, * s_iter = file_selection;
			s_iter;
		s_iter = s_iter->next
	) {

		location = nautilus_file_info_get_parent_location(
			NAUTILUS_FILE_INFO(s_iter->data)
		);

		dpath = g_file_get_path(location);
		g_object_unref(location);

		if (!dpath) {

			/*  `s_iter->data` is not an actual file  */

			return NULL;

		}

		for (d_iter = ordered_selection; d_iter; d_iter = d_iter->next) {

			#define subselection ((ssofwcp *) d_iter->data)

			if (!strcmp(subselection->directory, dpath)) {

				/*  The parent directory of this file was already indicized  */

				subselection->selection = g_list_prepend(
					subselection->selection,
					s_iter->data
				);

				goto free_and_continue;

			}

			#undef subselection

		}

		/*  The parent directory of this file has not been indicized yet  */

		new_ssofwcp = ssofwcp_new_with_dir(dpath);

		if (!new_ssofwcp) {

			g_free(dpath);
			return NULL;

		}

		new_ssofwcp->selection = g_list_append(NULL, s_iter->data);
		ordered_selection = g_list_prepend(ordered_selection, new_ssofwcp);


		/* \                                /\
		\ */     free_and_continue:        /* \
		 \/     ______________________     \ */


		g_free(dpath);

	}

	return ordered_selection;

}


static void ssofwcp_destroy (gpointer data) {

	#define subselection ((ssofwcp *) data)

	free(subselection->directory);
	free(subselection->hdb_path);
	free(subselection->hdb_entries);
	g_list_free(subselection->selection);

	#undef subselection

}


static void nautilus_hide_push_files (
	NautilusMenuItem * const menu_item,
	gpointer user_data
) {

	GList * const file_selection = g_object_get_data(
		G_OBJECT(menu_item),
		"nautilus_hide_files"
	);

	if (!file_selection) {

		g_message("%s", _("No files have been selected to be hidden"));
		return;

	}

	GList * const ordered_selection =
		ordered_file_selection_new(file_selection);

	if (!ordered_selection) {

		g_message(
			"%s",
			_(
				"Could not hide the selected files, error attempting "
				"to group them according to their parent directories"
			)
		);

		return;

	}

	char * fname;
	gsize idx;
	FILE * hdb_file_w;

	for (
		GList * i_iter, * o_iter = ordered_selection;
			o_iter;
		o_iter = o_iter->next
	) {

		#define subselection ((ssofwcp *) o_iter->data)

	   	hdb_file_w = fopen(subselection->hdb_path, "wb");

		if (!hdb_file_w) {

			g_message(
				"%s (database: %s)",
				_(
					"Could not hide the selected files, "
					"error attempting to edit the database"
				),
				subselection->hdb_path
			);

			continue;

		}

		if (!subselection->hdb_entries || !*subselection->hdb_entries) {

			/*  Database file is empty or missing  */

			for (
				i_iter = subselection->selection;
					i_iter;
				i_iter = i_iter->next
			) {

				fname = nautilus_file_info_get_name(
					NAUTILUS_FILE_INFO(i_iter->data)
				);

				if (*fname != '.') {

					/*  This is not a dotfile  */

					fwrite(fname, 1, strlen(fname), hdb_file_w);
					fputc('\n', hdb_file_w);

				}

				g_free(fname);

			}

		   	goto fclose_and_continue;

		}


		/*  Database file is not empty  */

		for (idx = 0; subselection->hdb_entries[idx]; idx++) {

			if (*subselection->hdb_entries[idx] != '.') {

				/*  This is not a dotfile  */

				fwrite(
					subselection->hdb_entries[idx],
					1,
					strlen(subselection->hdb_entries[idx]),
					hdb_file_w
				);

				fputc('\n', hdb_file_w);

			}

		}

		for (i_iter = subselection->selection; i_iter; i_iter = i_iter->next) {

			fname = nautilus_file_info_get_name(
				NAUTILUS_FILE_INFO(i_iter->data)
			);

			if (*fname != '.') {

				/*  This is not a dotfile  */

				for (
					idx = 0;
						subselection->hdb_entries[idx] &&
						strcmp(fname, subselection->hdb_entries[idx]);
					idx++
				);

				if (!subselection->hdb_entries[idx]) {

					/*  This file has not been found in the database,
					    let's add it  */

					fwrite(fname, 1, strlen(fname), hdb_file_w);
					fputc('\n', hdb_file_w);

				}

			}

			g_free(fname);

		}


		/* \                                /\
		\ */     fclose_and_continue:      /* \
		 \/     ______________________     \ */


		close_hdb_file(hdb_file_w, subselection->hdb_path);

		#undef subselection

	}

	g_list_free_full(ordered_selection, ssofwcp_destroy);

}


static void nautilus_hide_pop_files (
	NautilusMenuItem * const menu_item,
	gpointer user_data
) {

	GList * const file_selection = g_object_get_data(
		G_OBJECT(menu_item),
		"nautilus_hide_files"
	);

	if (!file_selection) {

		g_message("%s", _("No files have been selected to be unhidden"));
		return;

	}

	GList * const ordered_selection =
		ordered_file_selection_new(file_selection);

	if (!ordered_selection) {

		g_message(
			"%s",
			_(
				"Could not unhide the selected files, error attempting "
				"to group them according to their parent directories"
			)
		);

		return;

	}

	bool b_keep_hdb_file;
	gsize idx;
	char * fname;
	FILE * hdb_file_w;

	for (
		GList * i_iter, * o_iter = ordered_selection;
			o_iter;
		o_iter = o_iter->next
	) {

		#define subselection ((ssofwcp *) o_iter->data)

		if (!subselection->hdb_entries || !*subselection->hdb_entries) {

			/*  Database file is empty or missing  */

			goto remove_db;

		}


		/*  Database file is not empty  */

	   	hdb_file_w = fopen(subselection->hdb_path, "wb");

		if (!hdb_file_w) {

			g_message(
				"%s (database: %s)",
				_(
					"Could not unhide the selected files, "
					"error attempting to edit the database"
				),
				subselection->hdb_path
			);

			continue;

		}

		idx = 0;
		b_keep_hdb_file = false;


		/* \                                /\
		\ */     check_member:             /* \
		 \/     ______________________     \ */


		if (subselection->hdb_entries[idx]) {

			if (*subselection->hdb_entries[idx] == '.') {

				/*  This is already a dotfile  */

				idx++;
				goto check_member;

			}

			for (
				i_iter = subselection->selection;
					i_iter;
				i_iter = i_iter->next
			) {

				fname = nautilus_file_info_get_name(
					NAUTILUS_FILE_INFO(i_iter->data)
				);

				if (!strcmp(fname, subselection->hdb_entries[idx])) {

					/*  This database entry is marked for deletion  */

					idx++;
					g_free(fname);
					goto check_member;

				}

				g_free(fname);

			}


			/*  This database entry is not marked for deletion  */

			fwrite(
				subselection->hdb_entries[idx],
				1,
				strlen(subselection->hdb_entries[idx]), hdb_file_w
			);

			fputc('\n', hdb_file_w);
			b_keep_hdb_file = true;
			idx++;
			goto check_member;

		}

	   	close_hdb_file(hdb_file_w, subselection->hdb_path);

		if (b_keep_hdb_file) {

			continue;

		}


		/* \                                /\
		\ */     remove_db:                /* \
		 \/     ______________________     \ */


		/*  Database is empty, let's remove the file  */

		if (!g_access(subselection->directory, W_OK)) {

			g_remove(subselection->hdb_path);

		}

		#undef subselection

	}

	g_list_free_full(ordered_selection, ssofwcp_destroy);

}


static GList * nautilus_hide_get_file_items (
	NautilusMenuProvider * const provider,
	GtkWidget * const window,
	GList * const file_selection
) {

	if (!file_selection) {

		return NULL;

	}

	GList * const ordered_selection =
		ordered_file_selection_new(file_selection);

	if (!ordered_selection) {

		return NULL;

	}

	const guint sellen = g_list_length(file_selection);
	bool b_show_hide = false;
	bool b_show_unhide = false;
	gsize idx;
	char * fname;
	NautilusMenuItem * menu_item;

	for (
		GList * i_iter, * o_iter = ordered_selection;
			o_iter;
		o_iter = o_iter->next
	) {

		#define subselection ((ssofwcp *) o_iter->data)

		if (
			(subselection->hdb_access & (NH_R_OK | NH_W_OK)) !=
			(NH_R_OK | NH_W_OK)
		) {

			/*  At least one database has not enough permissions  */

			return NULL;

		}

		if (subselection->hdb_entries && *subselection->hdb_entries) {

			/*  Database file is not empty  */

			for (
				i_iter = subselection->selection;
					i_iter;
				i_iter = i_iter->next
			) {

				fname = nautilus_file_info_get_name(
					NAUTILUS_FILE_INFO(i_iter->data)
				);

				if (*fname == '.') {

					/*  This is already a dotfile, let's skip it  */

					g_free(fname);
					continue;

				}

				for (
					idx = 0;
						subselection->hdb_entries[idx] &&
						strcmp(fname, subselection->hdb_entries[idx]);
					idx++
				);

				g_free(fname);

				if (subselection->hdb_entries[idx]) {

					/*  There are hidden files in the selection  */

					b_show_unhide = true;

					if (b_show_hide) {

						goto populate_menu;

					}

				} else {

					/*  There are non-hidden files in the selection  */

					b_show_hide = true;

					if (b_show_unhide) {

						goto populate_menu;

					}

				}

			}

		} else {

			/*  Database file is empty or missing  */

			for (
				i_iter = subselection->selection;
					i_iter;
				i_iter = i_iter->next
			) {

				fname = nautilus_file_info_get_name(
					NAUTILUS_FILE_INFO(i_iter->data)
				);

				if (*fname != '.') {

					b_show_hide = true;
					g_free(fname);
					goto populate_menu;

				}

				/*  This is already a dotfile, let's skip it  */

				g_free(fname);

			}

		}

		#undef subselection

	}


	/* \                                /\
	\ */     populate_menu:            /* \
	 \/     ______________________     \ */


	g_list_free_full(ordered_selection, ssofwcp_destroy);
	GList * menu_entries = NULL;

	if (b_show_hide) {

		/*  Show a `Hide` menu entry  */

		menu_item = nautilus_menu_item_new(
			"NautilusHide::hide",
			g_dngettext(
				GETTEXT_PACKAGE,
				"_Hide file",
				"_Hide files",
				sellen
			),
			g_dngettext(
				GETTEXT_PACKAGE,
				"Hide the selected file",
				"Hide the selected files",
				sellen
			),
			"view-conceal"
		);

		g_signal_connect(
			menu_item,
			"activate",
			G_CALLBACK(nautilus_hide_push_files),
			NULL
		);

		g_object_set_data_full(
			G_OBJECT(menu_item),
			"nautilus_hide_files",
			nautilus_file_info_list_copy(file_selection),
			(GDestroyNotify) nautilus_file_info_list_free
		);

		menu_entries = g_list_append(NULL, menu_item);

	}

	if (b_show_unhide) {

		/*  Show an `Unhide` menu entry  */

		menu_item = nautilus_menu_item_new(
			"NautilusHide::unhide",
			g_dngettext(
				GETTEXT_PACKAGE,
				"_Unhide file",
				"_Unhide files",
				sellen
			),
			g_dngettext(
				GETTEXT_PACKAGE,
				"Unhide the selected file",
				"Unhide the selected files",
				sellen
			),
			"view-reveal"
		);

		g_signal_connect(
			menu_item,
			"activate",
			G_CALLBACK(nautilus_hide_pop_files),
			NULL
		);

		g_object_set_data_full(
			G_OBJECT(menu_item),
			"nautilus_hide_files",
			nautilus_file_info_list_copy(file_selection),
			(GDestroyNotify) nautilus_file_info_list_free
		);

		menu_entries = g_list_append(menu_entries, menu_item);

	}

	return menu_entries;

}


static void nautilus_hide_menu_provider_iface_init (
	NautilusMenuProviderIface * const iface,
	gpointer const iface_data
) {

	iface->get_file_items = nautilus_hide_get_file_items;

}


static void nautilus_hide_class_init (
	NautilusHideClass * const nautilus_hide_class,
	gpointer const class_data
) {

	parent_class = g_type_class_peek_parent(nautilus_hide_class);

}


static void nautilus_hide_register_type (
	GTypeModule * const module
) {

	static const GTypeInfo info = {
		sizeof(NautilusHideClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) nautilus_hide_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,
		sizeof(NautilusHide),
		0,
		(GInstanceInitFunc) NULL,
		(GTypeValueTable *) NULL
	};

	static const GInterfaceInfo menu_provider_iface_info = {
		(GInterfaceInitFunc) nautilus_hide_menu_provider_iface_init,
		(GInterfaceFinalizeFunc) NULL,
		NULL
	};

	nautilus_hide_type = g_type_module_register_type(
		module,
		G_TYPE_OBJECT,
		"NautilusHide",
		&info,
		0
	);

	g_type_module_add_interface(
		module,
		nautilus_hide_type,
		NAUTILUS_TYPE_MENU_PROVIDER,
		&menu_provider_iface_info
	);

}


GType nautilus_hide_get_type (void) {

	return nautilus_hide_type;

}


void nautilus_module_shutdown (void) {

	/*  Any module-specific shutdown  */

}


void nautilus_module_list_types (
	const GType ** const types,
	int * const num_types
) {

	*types = provider_types;
	*num_types = G_N_ELEMENTS(provider_types);

}


void nautilus_module_initialize (
	GTypeModule * const module
) {

	I18N_INIT();
	nautilus_hide_register_type(module);
	*provider_types = nautilus_hide_get_type();

}


/*  NOTE: `ssofwcp` ==> Sub-Selection Of Files With Common Parent  */


/*  EOF  */

