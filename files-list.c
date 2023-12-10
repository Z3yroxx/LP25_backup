#include <files-list.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>

/*!
 * @brief clear_files_list clears a files list
 * @param list is a pointer to the list to be cleared
 * This function is provided, you don't need to implement nor modify it
 */
void clear_files_list(files_list_t *list) {
    while (list->head) {
        files_list_entry_t *tmp = list->head;
        list->head = tmp->next;
        free(tmp);
    }
}

/*!
 *  @brief add_file_entry adds a new file to the files list.
 *  It adds the file in an ordered manner (strcmp) and fills its properties
 *  by calling stat on the file.
 *  Il the file already exists, it does nothing and returns 0
 *  @param list the list to add the file entry into
 *  @param file_path the full path (from the root of the considered tree) of the file
 *  @return a pointer to the added element if success, NULL else (out of memory)
 */
files_list_entry_t *add_file_entry(files_list_t *list, char *file_path) {

        // Vérifier si le fichier existe déjà dans la liste
    files_list_entry_t *existing_entry = find_entry_by_name(list, file_path, 0, 0);
    if (existing_entry != NULL) {
        return NULL;  // Le fichier existe déjà, ne rien faire
    }

    // Créer une nouvelle entrée pour le fichier
    files_list_entry_t *new_entry = (files_list_entry_t *)malloc(sizeof(files_list_entry_t));
    if (new_entry == NULL) {
        return NULL;  // Échec d'allocation mémoire
    }

    // Remplir les propriétés de la nouvelle entrée en utilisant stat sur le fichier
    struct stat file_stat;
    if (stat(file_path, &file_stat) != 0) {
        free(new_entry);
        return NULL;  // Échec de l'obtention des informations sur le fichier
    }

    strncpy(new_entry->path_and_name, file_path, sizeof(new_entry->path_and_name));
    new_entry->path_and_name[sizeof(new_entry->path_and_name) - 1] = '\0';
    new_entry->mtime.tv_sec = file_stat.st_mtime;
    new_entry->mtime.tv_nsec = 0;
    new_entry->size = file_stat.st_size;
    new_entry->entry_type = S_ISDIR(file_stat.st_mode) ? DOSSIER : FICHIER;
    new_entry->mode = file_stat.st_mode;
    memset(new_entry->md5sum, 0, sizeof(new_entry->md5sum));  // Remplir le MD5 à votre discrétion

    // Ajouter l'entrée à la liste
    if (add_entry_to_tail(list, new_entry) != 0) {
        free(new_entry);
        return NULL;  // Échec d'ajout à la liste
    }

    return new_entry;    
}

/*!
 * @brief add_entry_to_tail adds an entry directly to the tail of the list
 * It supposes that the entries are provided already ordered, e.g. when a lister process sends its list's
 * elements to the main process.
 * @param list is a pointer to the list to which to add the element
 * @param entry is a pointer to the entry to add. The list becomes owner of the entry.
 * @return 0 in case of success, -1 else
 */
int add_entry_to_tail(files_list_t *list, files_list_entry_t *entry) {

        if (list == NULL || entry == NULL) {
        return -1;  // Paramètres invalides
    }

    // Ajouter l'entrée à la fin de la liste
    entry->next = NULL;
    entry->prev = list->tail;

    if (list->tail == NULL) {
        // La liste est vide, donc l'élément ajouté devient la tête et la queue
        list->head = entry;
        list->tail = entry;
    } else {
        // Ajouter à la fin de la liste
        list->tail->next = entry;
        list->tail = entry;
    }

    return 0;  // Succès
}

/*!
 *  @brief find_entry_by_name looks up for a file in a list
 *  The function uses the ordering of the entries to interrupt its search
 *  @param list the list to look into
 *  @param file_path the full path of the file to look for
 *  @param start_of_src the position of the name of the file in the source directory (removing the source path)
 *  @param start_of_dest the position of the name of the file in the destination dir (removing the dest path)
 *  @return a pointer to the element found, NULL if none were found.
 */
files_list_entry_t *find_entry_by_name(files_list_t *list, char *file_path, size_t start_of_src, size_t start_of_dest) {

        if (list == NULL || file_path == NULL) {
        return NULL;  // Paramètres invalides
    }

    // Parcourir la liste pour trouver l'entrée correspondante
    for (files_list_entry_t *cursor = list->head; cursor != NULL; cursor = cursor->next) {
        // Comparer les noms des fichiers en ignorant les parties de chemin spécifiées
        if (strcmp(cursor->path_and_name + start_of_src, file_path + start_of_dest) == 0) {
            return cursor;  // Entrée trouvée
        }
    }

    return NULL;  // Aucune entrée trouvée
}

/*!
 * @brief display_files_list displays a files list
 * @param list is the pointer to the list to be displayed
 * This function is already provided complete.
 */
void display_files_list(files_list_t *list) {
    if (!list)
        return;
    
    for (files_list_entry_t *cursor=list->head; cursor!=NULL; cursor=cursor->next) {
        printf("%s\n", cursor->path_and_name);
    }
}

/*!
 * @brief display_files_list_reversed displays a files list from the end to the beginning
 * @param list is the pointer to the list to be displayed
 * This function is already provided complete.
 */
void display_files_list_reversed(files_list_t *list) {
    if (!list)
        return;
    
    for (files_list_entry_t *cursor=list->tail; cursor!=NULL; cursor=cursor->prev) {
        printf("%s\n", cursor->path_and_name);
    }
}
